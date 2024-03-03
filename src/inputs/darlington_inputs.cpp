#include "inputs.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "pico/util/queue.h"

#define PIN_LEFT  (27)
#define PIN_RIGHT (28)
#define PIN_X     (26)

static repeating_timer_t readTimer;
static queue_t inputEventQueue;

#define STATE_LEFT        (1<<0)
#define STATE_RIGHT       (1<<1)
#define STATE_X           (1<<2)
#define EVT_PRESSED_LEFT  (1<<4)
#define EVT_PRESSED_RIGHT (1<<5)
#define EVT_PRESSED_X     (1<<6)
#define EVT_RELEASE_LEFT  (1<<8)
#define EVT_RELEASE_RIGHT (1<<9)
#define EVT_RELEASE_X     (1<<10)
#define EVT_MASK          (0xFF0)
#define EVT_MASK_PRESS    (0x0F0)
#define EVT_MASK_RELEASE  (0xF00)
#define STATE_MASK        (0x00F)

#define PARAM_TIMEOUT_MS (70)

static const uint16_t relations[3][4] = {
    {PIN_LEFT, STATE_LEFT, EVT_PRESSED_LEFT, EVT_RELEASE_LEFT},
    {PIN_RIGHT, STATE_RIGHT, EVT_PRESSED_RIGHT, EVT_RELEASE_RIGHT},
    {PIN_X, STATE_X, EVT_PRESSED_X, EVT_RELEASE_X},
};

static bool timerHandler(repeating_timer_t *rt){
    static uint32_t now = 1; // 0 is invalid in this context
    static uint32_t lastPressed[3] = {0,0,0};
    static uint8_t ignoreRelease = 0;
    uint16_t state = 0;
    uint8_t buttonsPressed = 0;

    for(uint8_t i=0; i<3; i++){
        if(gpio_get(relations[i][0])){
            // button read as released
            if(lastPressed[i]>0){
                // current internal state is "pressed"
                if(lastPressed[i]+PARAM_TIMEOUT_MS<now){
                    // timeout reached, pin was released, generating event
                    lastPressed[i] = 0;
                    if(!ignoreRelease) state |= relations[i][3];
                }
            }

        } else {
            // button is currently pressed
            if(lastPressed[i]==0){
                // was not pressed, generating press event
                state |= relations[i][2];
            }
            lastPressed[i] = now;
        }
        // filling raw internal state
        if(lastPressed[i]>0) {
            state |= relations[i][1];
            buttonsPressed++;
        }
    }
    // handle "ignore command": when user touches a second button while
    // another has been touched, ignore any button release event
    if(buttonsPressed==0) {
        ignoreRelease = 0;
    } else if(buttonsPressed>1) {
        ignoreRelease = 1;
    }

    if( ((state&EVT_MASK) !=0)  || now%1000==1 ){
        // only forwarding events or once a second updates
        queue_try_add(&inputEventQueue, &state);
    }

    if( (state&STATE_MASK)==0 ){
        // resetting timebase to not overflow as easily
        if(now>1000){
            now = 1;
        } else {
            now++;
        }
    } else {
        now++;
    }
    return true;
}

void Inputs::init(){
    for(uint8_t i=0; i<3; i++){
        gpio_deinit(relations[i][0]);
        gpio_init(relations[i][0]);
        gpio_set_dir(relations[i][0], GPIO_IN);
        gpio_set_pulls(relations[i][0], true, false);
    }
    queue_init(&inputEventQueue, 2, 128);
    add_repeating_timer_ms(1, timerHandler, NULL, &readTimer);
}

void Inputs::process(){
    // User already has processed any event, clearing it
    pressedKeys = 0;
    releasedKeys = 0;

    uint16_t evt;
    if (queue_try_remove(&inputEventQueue, &evt)){
        currentKeys = 0;
        pressedKeys |= DOWN;
        do {
            if(evt&STATE_LEFT) currentKeys |= LEFT;
            if(evt&STATE_RIGHT) currentKeys |= RIGHT;
            if(evt&STATE_X) currentKeys |= OK;

            if(evt&EVT_PRESSED_LEFT) pressedKeys |= LEFT;
            if(evt&EVT_PRESSED_RIGHT) pressedKeys |= RIGHT;
            if(evt&EVT_PRESSED_X) pressedKeys |= OK;

            if(evt&EVT_RELEASE_LEFT) releasedKeys |= LEFT;
            if(evt&EVT_RELEASE_RIGHT) releasedKeys |= RIGHT;
            if(evt&EVT_RELEASE_X) releasedKeys |= OK;
            // maybe there have been multiple events since last processing,
            // so checking while there are events
            // All events should be OR'ed if there are any
        } while (queue_try_remove(&inputEventQueue, &evt));
    }
}
