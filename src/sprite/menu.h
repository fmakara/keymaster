#pragma once

#include <string>
#include "display/ssd1306.h"

class Menu {
public:
    static const char CHR_BACKSPACE=0x7F;
    static const uint32_t BTN_CHR=(1<<8);
    static const uint32_t BTN_UP=(1<<9);
    static const uint32_t BTN_DOWN=(1<<10);
    static const uint32_t BTN_LEFT=(1<<11);
    static const uint32_t BTN_RIGHT=(1<<12);
    static const uint32_t BTN_CANCEL=(1<<13);
    static const uint32_t BTN_OK=(1<<14);
    
    // Can use every input (CHR, UP, DOWN, LEFT, RIGHT, OK, CANCEL)
    static bool getString(SSD1306_128x32& screen, uint32_t (*input)(void), char* str, unsigned maxsize);
    // Uses LEFT, RIGHT, OK and CANCEL (false on cancel)
    static bool confirm(SSD1306_128x32& screen, uint32_t (*input)(void), const char* str0, const char* str1);
    // Uses UP, DOWN, OK and CANCEL (min-1 on cancel)
    static int intRead(SSD1306_128x32& screen, uint32_t (*input)(void), int val, int min, int max, const char* descr);
    static int genericList(SSD1306_128x32& screen, uint32_t (*input)(void), const std::vector<std::string>& items, int startingItem=0);
};