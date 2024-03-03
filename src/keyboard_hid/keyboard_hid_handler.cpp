#include "keyboard_hid_handler.h"
#include "generic_macros.h"
#include "usb/usb_descriptors.h"

#define HID_INTERVAL_US (10000)

static const uint8_t ascii2keycode[128][2] = {HID_ASCII_TO_KEYCODE};

KeyboardHIDHandler::KeyboardHIDHandler()
: lastCaps(false)
, lastNum(false)
, lastScroll(false)
, lastKeysInited(false)
, hid_next_us(0)
{}

void KeyboardHIDHandler::init(){
    queue_init(&usbHIDKeyQueue, 1, 512);
    queue_init(&usbHIDEvtQueue, 1, 16);
    queue_init(&usbCDCinputQueue, 1, 64);
}

void KeyboardHIDHandler::sendHID(const std::string &s){
    for (int i = 0; i < s.size(); i++)
        queue_add_blocking(&usbHIDKeyQueue, &s[i]);
}

uint8_t KeyboardHIDHandler::getKeyboardEvent(){
    uint8_t ret = 0;
    if(queue_try_remove(&usbHIDEvtQueue, &ret)) return ret;
    return 0;
}

uint32_t KeyboardHIDHandler::getSerialKeypress(){
    uint8_t cdc;
    if (queue_try_remove(&usbCDCinputQueue, &cdc)){
        if(cdc==0x1b){
            char cdc2, cdc3;
            if (queue_try_remove(&usbCDCinputQueue, &cdc2) && queue_try_remove(&usbCDCinputQueue, &cdc3)){
                return 0x1b0000 | (cdc2<<8) | (cdc3);
            }
        } else {
            return cdc;
        }
    }
    return 0;
}

void KeyboardHIDHandler::ignoreSerialKeypress(){
    uint8_t cdc;
    while (queue_try_remove(&usbCDCinputQueue, &cdc));
}

void KeyboardHIDHandler::mainEventLoop(){
    tud_task();

    if (tud_hid_ready()){
        if (micros() < hid_next_us)return;
        hid_next_us = micros() + HID_INTERVAL_US;

        // HID handling ----------------------------------------
        char chr;
        if (queue_try_remove(&usbHIDKeyQueue, &chr)){
            uint8_t keycode[6] = {0};
            uint8_t modifier = 0;
            if (ascii2keycode[chr][0]) modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            keycode[0] = ascii2keycode[chr][1];
            static char lastchr = 0;
            if(chr==lastchr){
                while(!tud_hid_keyboard_report(0, modifier, NULL)) tud_task();
            }
            while(!tud_hid_keyboard_report(0, modifier, keycode)) tud_task();
            lastchr = chr;
        }else{
            tud_hid_keyboard_report(0, 0, NULL);
        }
    }
    // CDC handling -----------------------------------------
    if (tud_cdc_available()){
        // respond all input data
        char buf[64];
        int count = tud_cdc_read(buf, sizeof(buf));
        for (int i = 0; i < count; i++){
            if (!queue_try_add(&usbCDCinputQueue, buf + i)) break;
        }
    }
}

void KeyboardHIDHandler::onReportCb(uint8_t instance, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize){
    if (instance == ITF_NUM_KEYBOARD){
        if (report_type == HID_REPORT_TYPE_OUTPUT){
            if (bufsize < 1) return;
            bool caps = (buffer[0] & KEYBOARD_LED_CAPSLOCK);
            bool num = (buffer[0] & KEYBOARD_LED_NUMLOCK);
            bool scroll = (buffer[0] & KEYBOARD_LED_SCROLLLOCK);
            if (!lastKeysInited){
                lastKeysInited = true;
                lastCaps = caps;
                lastNum = num;
                lastScroll = scroll;
            }
            if (num != lastNum){
                char evt = NUMLOCK;
                queue_try_add(&usbHIDEvtQueue, &evt);
            }
            if (caps != lastCaps){
                char evt = CAPSLOCK;
                queue_try_add(&usbHIDEvtQueue, &evt);
            }
            if (scroll != lastScroll){
                char evt = SCROLLLOCK;
                queue_try_add(&usbHIDEvtQueue, &evt);
            }
            lastCaps = caps;
            lastNum = num;
            lastScroll = scroll;
        }
    }
}