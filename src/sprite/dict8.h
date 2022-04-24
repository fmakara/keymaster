#pragma once

#include <stdint.h>
#include <stdlib.h>
#include "sprite.h"

class Char8x6 : public Sprite {
public:
    Char8x6(uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5);
    ~Char8x6();
protected:
    uint8_t fixBuf[7];
};

class Dict8 {
public:
    Dict8(Sprite* b);
    ~Dict8();
    int print(int x, int y, const char* str, ...);
    int printInto(Sprite* s, int x, int y, const char* str, ...);
    int printString(Sprite* s, int x, int y, const char* str);
    int putch(int x, int y, char c);
    static const Char8x6 charlist[96];
private:
    Sprite* base;
};