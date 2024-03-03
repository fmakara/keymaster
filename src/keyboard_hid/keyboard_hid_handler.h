#pragma once

#include <stdint.h>
#include <string>
#include "tusb.h"
#include "pico/util/queue.h"

class KeyboardHIDHandler {
public:
    static const uint8_t NUMLOCK = 1;
    static const uint8_t CAPSLOCK = 2;
    static const uint8_t SCROLLLOCK = 3;

    static const uint32_t DIRECTION_UP = 0x1b5b41;
    static const uint32_t DIRECTION_DOWN = 0x1b5b42;
    static const uint32_t DIRECTION_RIGHT = 0x1b5b43;
    static const uint32_t DIRECTION_LEFT = 0x1b5b44;

    KeyboardHIDHandler();
    void init();

    void sendHID(const std::string &s);
    uint8_t getKeyboardEvent();
    uint32_t getSerialKeypress();
    void ignoreSerialKeypress();

    void mainEventLoop();
    void onReportCb(uint8_t instance, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize);

private:
    queue_t usbHIDKeyQueue, usbHIDEvtQueue, usbCDCinputQueue;
    bool lastCaps, lastNum, lastScroll, lastKeysInited;
    uint64_t hid_next_us;
};