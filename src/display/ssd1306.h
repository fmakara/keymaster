#pragma once

#include <stdint.h>
#include "sprite/sprite.h"

class SSD1306_128x64 : public Sprite {
public:
    static const int WIDTH = 128;
    static const int HEIGHT = 64;
    static const uint8_t ADDR = 0x3C;
    SSD1306_128x64();

    void clear();
    bool display();

    bool init(int scl, int sda, int inst=0, bool _init=true);

private:
    uint32_t buffer[WIDTH*HEIGHT/32]; 
    int instance;
    bool initok;
    bool sendCmd(uint8_t c);
    bool sendList(uint8_t header, const uint8_t *c, int len);
};