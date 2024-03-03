#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <stdint.h>
#include <math.h>
#include <list>
#include <string>
#include <vector>
#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "pico/bootrom.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/regs/resets.h"
#include "hardware/resets.h"

#include "bsp/board.h"
#include "tusb.h"
#include "usb/usb_descriptors.h"
#include "pio_pwm.pio.h"

#include "generic_macros.h"
#include "display/ssd1306.h"
#include "sprite/dict8.h"
#include "sprite/menu.h"
#include "pers/persistency.h"
#include "xceiver/xceiver.h"
#include "inputs/inputs.h"
#include "keyboard_hid/keyboard_hid_handler.h"

#define PIN_XCEIVER_RX    (12)
#define PIN_XCEIVER_TX    (13)
#define PIN_DISPLAY_SDA   (4)
#define PIN_DISPLAY_SCL   (5)
#define PIN_DISPLAY_RESET (7)

SSD1306_72x40 display;
Dict8 writer(&display);
Inputs hardInputs;
Xceiver xceiver(PIN_XCEIVER_RX, PIN_XCEIVER_TX, 1, 0, 1, 1, 115200);

void core1_entry();
volatile static uint8_t pauseUsbHandling;
KeyboardHIDHandler keyboard;

void mainLoop();
uint32_t commonConfirm();

void rebootAndFirmwareUpdate(){
    // somehow, including pico/bootrom.h in menus.cpp breaks the code extension, as if stdint is removed...
    // reset_usb_boot(1 << ledPins[2], 0); // TODO: REMOVE BEFORE FLIGHT
    reset_usb_boot(0, 0);
}


#define OPERATION_GET_COUNT (1)
#define OPERATION_GET_NAME  (2)
#define OPERATION_GET_UN    (3)
#define OPERATION_GET_PW    (4)
#define OPERATION_ERASE     (5)
#define OPERATION_SET_NAME  (6)
#define OPERATION_SET_UN    (7)
#define OPERATION_SET_PW    (8)
#define OPERATION_APPEND    (9)
#define OPERATION_END       (10)

void passiveMode(){
    std::string tmpName, tmpUn, tmpPw;
    int cogCounter = 0;
    bool running = true;
    pauseUsbHandling = 1;
    xceiver.enable();
    // turnOffPWM(); // TODO: REMOVE BEFORE FLIGHT

    while(running){
        xceiver.idle();
        if(xceiver.available()){
            std::vector<uint8_t> data;
            xceiver.rx(data);
            if(data.size()==1) {
                if(data[0]==OPERATION_GET_COUNT) {
                    xceiver.tx({(uint8_t)Pers::get().count()});

                } else if(data[0]==OPERATION_APPEND) {
                    if(tmpName.size()>0 && tmpUn.size()>0 && tmpPw.size()>0){
                        Pers::PassEntry entry;
                        strcpy(entry.name, tmpName.c_str());
                        strcpy(entry.username, tmpUn.c_str());
                        strcpy(entry.pass, tmpPw.c_str());
                        if(Pers::get().appendEntry(entry)){
                            xceiver.tx({(uint8_t)Pers::get().count()});
                        } else {
                            xceiver.tx({});
                        }
                    } else {
                        xceiver.tx({});
                    }
                    tmpName = "";
                    tmpUn = "";
                    tmpPw = "";
                }
                if(data[0]==OPERATION_END) {
                    xceiver.tx({1});
                    running = false;
                    for(int i=0; i<10; i++){
                        xceiver.idle();
                        sleep_ms(10);
                    }
                } else {
                    xceiver.tx({});
                }
            } else if(data.size()==2) {
                Pers::PassEntry entry;
                if(data[0]==OPERATION_GET_NAME) {
                    if(Pers::get().readEntry(data[1], entry)){
                        xceiver.tx(std::vector<uint8_t>(entry.name, entry.name+strlen(entry.name)+1));
                    } else {
                        xceiver.tx({});
                    }

                } else if(data[0]==OPERATION_GET_UN) {
                    if(Pers::get().readEntry(data[1], entry)){
                        xceiver.tx(std::vector<uint8_t>(entry.username, entry.username+strlen(entry.username)+1));
                    } else {
                        xceiver.tx({});
                    }

                } else if(data[0]==OPERATION_GET_PW) {
                    if(Pers::get().readEntry(data[1], entry)){
                        xceiver.tx(std::vector<uint8_t>(entry.pass, entry.pass+strlen(entry.pass)+1));
                    } else {
                        xceiver.tx({});
                    }

                } else if(data[0]==OPERATION_ERASE) {
                    if(Pers::get().eraseEntry(data[1])){
                        xceiver.tx({(uint8_t)Pers::get().count()});
                    } else {
                        xceiver.tx({});
                    }
                } else {
                    xceiver.tx({});
                }
            } else {
                if(data[0]==OPERATION_SET_NAME) {
                    if(data.size()<=33){
                        char tmp[33];
                        memcpy(tmp, data.data()+1, data.size()-1);
                        tmp[32] = 0;
                        tmpName = tmp;
                    } else {
                        tmpName = "";
                        xceiver.tx({});
                    }

                } else if(data[0]==OPERATION_SET_UN) {
                    if(data.size()<=129){
                        char tmp[129];
                        memcpy(tmp, data.data()+1, data.size()-1);
                        tmp[128] = 0;
                        tmpUn = tmp;
                    } else {
                        tmpUn = "";
                        xceiver.tx({});
                    }

                } else if(data[0]==OPERATION_SET_PW) {
                    if(data.size()<=65){
                        char tmp[65];
                        memcpy(tmp, data.data()+1, data.size()-1);
                        tmp[64] = 0;
                        tmpPw = tmp;
                    } else {
                        tmpPw = "";
                        xceiver.tx({});
                    }

                } else {
                    xceiver.tx({});
                }
            }
        } else {
            display.clear();
            xceiver.idle();
            writer.putch(30+sin((cogCounter+0)*2*3.14/16)*12,12+cos((cogCounter+0)*2*3.14/16)*12,'o');
            xceiver.idle();
            writer.putch(30+sin((cogCounter+4)*2*3.14/16)*12,12+cos((cogCounter+4)*2*3.14/16)*12,'o');
            xceiver.idle();
            writer.putch(30+sin((cogCounter+8)*2*3.14/16)*12,12+cos((cogCounter+8)*2*3.14/16)*12,'o');
            xceiver.idle();
            writer.putch(30+sin((cogCounter+12)*2*3.14/16)*12,12+cos((cogCounter+12)*2*3.14/16)*12,'o');
            cogCounter = (cogCounter+1)%16;
            xceiver.idle();
            display.display();
            xceiver.idle();
        }
    }
    xceiver.disable();
    pauseUsbHandling = 0;
}

void activeMode() {
    // just doing dumb copy for now, as I need to redesign the board ASAP
    while(Pers::get().count()>0)Pers::get().eraseEntry(0);

    // std::string tmpName, tmpUn, tmpPw;
    // int cogCounter = 0;
    // bool running = true;
    pauseUsbHandling = 1;
    xceiver.enable();
    // turnOffPWM(); // TODO: REMOVE BEFORE FLIGHT

    std::string failReason;
    uint64_t end;
    int entryCount = 0;
    std::vector<uint8_t> rawResponse;
    xceiver.tx({OPERATION_GET_COUNT});
    end = micros()+1000000;
    while(xceiver.available()==0){
        if(micros()>end) {
            failReason = "Sem comunicacao";
            goto exitActiveMode;
        }
        xceiver.idle();
    }
    xceiver.rx(rawResponse);
    entryCount = rawResponse[0];

    display.clear();
    writer.print(0, 10, "entradas: %d", entryCount);
    display.display();

    for(uint8_t i=0; i<entryCount; i++){
        Pers::PassEntry entry;
        memset(&entry, 0, sizeof(entry));

        writer.print(0, 20, "   %d", i);
        display.display();

        xceiver.tx({OPERATION_GET_NAME, i});
        end = micros()+1000000;
        while(xceiver.available()==0){
            if(micros()>end) {
                failReason = "falha nome";
                goto exitActiveMode;
            }
            xceiver.idle();
        }
        xceiver.rx(rawResponse);
        memcpy(entry.name, &rawResponse[0], rawResponse.size());

        writer.print(0, 20, "N");
        display.display();

        xceiver.tx({OPERATION_GET_UN, i});
        end = micros()+1000000;
        while(xceiver.available()==0){
            if(micros()>end) {
                failReason = "falha un";
                goto exitActiveMode;
            }
            xceiver.idle();
        }
        xceiver.rx(rawResponse);
        memcpy(entry.username, &rawResponse[0], rawResponse.size());

        writer.print(0, 20, "U");
        display.display();

        xceiver.tx({OPERATION_GET_PW, i});
        end = micros()+1000000;
        while(xceiver.available()==0){
            if(micros()>end) {
                failReason = "falha pw";
                goto exitActiveMode;
            }
            xceiver.idle();
        }
        xceiver.rx(rawResponse);
        memcpy(entry.pass, &rawResponse[0], rawResponse.size());

        writer.print(0, 20, "P");
        display.display();

        if(!Pers::get().appendEntry(entry)){
            failReason = "falha adicionar";
            goto exitActiveMode;
        }
    }

    display.clear();
    writer.print(0, 10, "Importado %d", entryCount);
    writer.print(0, 20, "  entradas");
    display.display();
    sleep_ms(1000);
    xceiver.disable();
    pauseUsbHandling = 0;
    return;

exitActiveMode:
    display.clear();
    writer.print(0, 10, failReason.c_str());
    display.display();
    sleep_ms(1000);
    xceiver.disable();
    pauseUsbHandling = 0;
}

void shuffleSeed(uint32_t *seed){
    adc_set_temp_sensor_enabled(true);
    for(int i=0; i<4; i++){
        adc_gpio_init(26+i);
    }
    for(int i=0; i<5; i++){
        adc_select_input(i);
        *seed += adc_read()<<(i*4);
    }
}

int main(void){
    uint32_t seed = 0xA0A0A0A0;
    board_init();
    adc_init();

    shuffleSeed(&seed);
    xceiver.init();
    shuffleSeed(&seed);

    pauseUsbHandling = 0;
    keyboard.init();
    multicore_launch_core1(core1_entry);

    shuffleSeed(&seed);
    if (!display.init(PIN_DISPLAY_RESET, PIN_DISPLAY_SCL, PIN_DISPLAY_SDA, 0)){
        for (int i = 0; i < 15; i++){
            // TODO: have a last-resort status led
        }
    };
    shuffleSeed(&seed);
    display.clear();
    writer.print(0, 0, "LCD ok");
    writer.print(0, 10, "Inicializando");
    writer.print(0, 20, "  Memoria... ");
    if (!display.display()){
        for (int i = 0; i < 15; i++){
            // TODO: have a last-resort status led
        }
    }
    shuffleSeed(&seed);
    Pers::get();
    seed += Pers::get().getCombinedId();

    shuffleSeed(&seed);
    srand(seed);

    hardInputs.init();

    Pers::MainData data;
    if(!Pers::get().readMain(data)){
        display.clear();
        writer.print(0, 0, "ERROR");
        writer.print(0, 10, "reading");
        writer.print(0, 20, "pin data");
        display.display();
        memset(&data, 0, sizeof(data));
        Pers::get().replaceMain(data);
        display.clear();
        writer.print(0, 0, "Data");
        writer.print(0, 10, "sobrescrita");
        display.display();
    }
    uint8_t zeros[4] = {0,0,0,0};
    if(memcmp(zeros, data.pin, 4)){
        uint8_t code[4];
        do{
            for(int i=0; i<4; i++) code[i] = rand()%10;
            Menu::pinReader(display, commonConfirm, code, 4);
            for(int i=0; i<10; i++){
                display.clear();
                writer.print(10+i*5, 30, "*");
                display.display();
                sleep_ms(30);
            }
        }while(memcmp(code, data.pin, 4));
    }

#ifdef OLD_LED_INIT_AND_CHECK
    display.clear();
    writer.print(0, 0, "LCD ok, MEM ok");
    writer.print(0, 10, "Acione todas as");
    writer.print(0, 20, "entradas");
    display.display();{
        uint32_t seed = 0xA0A0A0A0 + Pers::get().getCombinedId();
        uint8_t darkCheck = 0, brightCheck = 0, fullCheck = (1 << IR_CNT) - 1;
        while (1){
            uint16_t dark[IR_CNT], bright[IR_CNT];
            readRawAnalogInputs(dark, bright);
            uint8_t current = interpretRawIR(dark, bright);
            setLeds(current);

            for (int i = 0; i < IR_CNT; i++){
                seed += dark[i] << (i * 8);
                seed += bright[i] << (i * 8 + IR_CNT);
            }

            darkCheck |= (~current) & fullCheck;
            brightCheck |= (current)&fullCheck;
            if (darkCheck == fullCheck && brightCheck == fullCheck && current == 0) break;
            sleep_ms(10);
        }

        srand(seed);
    }
#endif

    while (1){
        mainLoop();
        rand();
    }
}

// Doing everything USB-related in second core
void core1_entry(){
    multicore_lockout_victim_init();
    tusb_init();


    while (1){
        if(pauseUsbHandling){
            reset_block(RESETS_RESET_USBCTRL_BITS);
            while(pauseUsbHandling) sleep_ms(100);
            tusb_init();
        }
        keyboard.mainEventLoop();
    }
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize){
    keyboard.onReportCb(instance, report_type, buffer, bufsize);
}
