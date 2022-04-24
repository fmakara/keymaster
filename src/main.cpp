#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <list>
#include <string>

#include "bsp/board.h"
#include "tusb.h"
#include "usb_descriptors.h"

bool lastCaps=false, lastNum=false, lastScroll=false, lastKeysInited=false;
bool capsChanged=false, numChanged=false, scrollChanged=false;
bool terminalConnected=false;
std::list<char> hidCharList;

inline void sendHID(const std::string& s){
  hidCharList.insert(hidCharList.end(), s.begin(), s.end());
}

void hid_task();
void main_task();

int main(void) {
  board_init();
  tusb_init();

  while (1) {
    tud_task();
    hid_task();
    main_task();
  }
}

void main_task(){
  if(scrollChanged){
    sendHID("scroll");
  }
  if(numChanged){
    sendHID("num");
  }
  if(capsChanged){
    sendHID("caps");
  }

  if(terminalConnected){
    tud_cdc_write("Bem vindo!\n", 11);
  }
  if ( tud_cdc_available() ) {
    // respond all input data
    char buf[64];
    uint32_t count = tud_cdc_read(buf, sizeof(buf));
    tud_cdc_write(buf, count);
    tud_cdc_write_flush();
  }

  capsChanged = false;
  numChanged = false;
  scrollChanged = false;
  terminalConnected = false;
}

void hid_task(){
  static const uint8_t ascii2keycode[128][2] = { HID_ASCII_TO_KEYCODE };
  uint8_t const report_id = 0;
  static const uint32_t interval_ms = 10;
  static uint32_t next_ms = 0;
  if ( !tud_hid_ready() ) return;
  if ( board_millis() < next_ms) return;
  next_ms = board_millis()+interval_ms;

  if(hidCharList.empty()){
    tud_hid_keyboard_report(report_id, 0, NULL);
    return;
  }

  char chr = hidCharList.front();
  uint8_t keycode[6] = { 0 };
  uint8_t modifier = 0;
  if ( ascii2keycode[chr][0] ) modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
  keycode[0] = ascii2keycode[chr][1];
  tud_hid_keyboard_report(report_id, modifier, keycode);

  hidCharList.pop_front();
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
      capsChanged = (caps!=lastCaps);
      numChanged = (num!=lastNum);
      scrollChanged = (scroll!=lastScroll);
      lastCaps = caps;
      lastNum = num;
      lastScroll = scroll;
    }
  }
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  (void) itf;
  (void) rts;
  if ( dtr ) {
    terminalConnected = true;
  }
}
