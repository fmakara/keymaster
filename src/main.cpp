#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <list>
#include <string>
#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "pico/bootrom.h"

#include "bsp/board.h"
#include "tusb.h"
#include "usb/usb_descriptors.h"

#include "display/ssd1306.h"
#include "sprite/dict8.h"
#include "pers/persistency.h"

#define EVT_NUMLOCK 1
#define EVT_CAPSLOCK 2
#define EVT_SCROLLLOCK 3

bool lastCaps=false, lastNum=false, lastScroll=false, lastKeysInited=false;
std::list<char> hidCharList;
SSD1306_128x32 display;
Dict8 writer(&display);

queue_t usbHIDKeyQueue, usbHIDEvtQueue;
const uint8_t ascii2keycode[128][2] = { HID_ASCII_TO_KEYCODE };

inline void sendHID(const std::string& s){
  for(int i=0; i<s.size(); i++) queue_add_blocking(&usbHIDKeyQueue, &s[i]);
}

void core1_entry();

int main(void) {
  //board_init();
  queue_init(&usbHIDKeyQueue, 1, 512);
  queue_init(&usbHIDEvtQueue, 1, 16);
  multicore_launch_core1(core1_entry);

  for(int i=8; i<=15; i++) gpio_set_pulls(i, true, false);
  if(!display.init(9, 12, 0)){
    while(1){
      // TODO blink leds
    }
  };

  display.clear();
  writer.print(32,0,"LCD ok");
  writer.print(32,10,"Inicializando");
  writer.print(32,20,"  Memoria... ");
  if (!display.display()) {
      while(1){
        // TODO blink leds
      }
  }

  Pers::get();

  display.clear();
  writer.print(32,0,"LCD ok");
  writer.print(32,10,"MEM ok");
  writer.print(32,20,"Aperte e solte");
  display.display();

  while (1) {
    char evt;
    if(queue_try_remove(&usbHIDEvtQueue, &evt)){
      display.clear();
      writer.print(32,0,"evt %d", evt);
      display.display();
      if(evt==EVT_NUMLOCK){
        sendHID("num");
      } else if(evt==EVT_CAPSLOCK){
        sendHID("caps");
      } else if(evt==EVT_SCROLLLOCK){
        sendHID("scroll");
      }
    }
  }
}

// Doing everything USB-related in second core
void core1_entry(){
  multicore_lockout_victim_init();
  tusb_init();

  const uint32_t hid_interval_ms = 10;
  uint32_t hid_next_ms = 0;

  while (1) {
    tud_task();

    if ( tud_hid_ready() ){
      if ( board_millis() < hid_next_ms) continue;
      hid_next_ms = board_millis()+hid_interval_ms;

      // HID handling ----------------------------------------
      char chr;
      if(queue_try_remove(&usbHIDKeyQueue, &chr)){
        uint8_t keycode[6] = { 0 };
        uint8_t modifier = 0;
        if ( ascii2keycode[chr][0] ) modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
        keycode[0] = ascii2keycode[chr][1];
        tud_hid_keyboard_report(0, modifier, keycode);
      } else {
        tud_hid_keyboard_report(0, 0, NULL);
      }
    }
    // CDC handling -----------------------------------------
    if ( tud_cdc_available() ) {
      // respond all input data
      char buf[64];
      uint32_t count = tud_cdc_read(buf, sizeof(buf));
      tud_cdc_write(buf, count);
      tud_cdc_write_flush();
    }
  }
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  (void) report_id;
  if (instance == ITF_NUM_KEYBOARD) {
    if (report_type == HID_REPORT_TYPE_OUTPUT) {
      if ( bufsize < 1 ) return;
      bool caps = (buffer[0]&KEYBOARD_LED_CAPSLOCK);
      bool num = (buffer[0]&KEYBOARD_LED_NUMLOCK);
      bool scroll = (buffer[0]&KEYBOARD_LED_SCROLLLOCK);
      if(!lastKeysInited){
        lastKeysInited = true;
        lastCaps = caps;
        lastNum = num;
        lastScroll = scroll;
      }
      if(num!=lastNum){
        char evt = EVT_NUMLOCK;
        queue_try_add(&usbHIDEvtQueue, &evt);
      }
      if(caps!=lastCaps){
        char evt = EVT_CAPSLOCK;
        queue_try_add(&usbHIDEvtQueue, &evt);
      }
      if(scroll!=lastScroll){
        char evt = EVT_SCROLLLOCK;
        queue_try_add(&usbHIDEvtQueue, &evt);
      }
      lastCaps = caps;
      lastNum = num;
      lastScroll = scroll;
    }
  }
}