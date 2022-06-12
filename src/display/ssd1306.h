#pragma once

#include <stdint.h>
#include "sprite/sprite.h"

class SSD1306 : public Sprite {
public:
    virtual void clear() = 0;
    virtual bool display() = 0;
protected:
    SSD1306(uint16_t w, uint16_t h, uint8_t* b);
};

class SSD1306_128x64 : public SSD1306 {
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

class SSD1306_128x32 : public SSD1306 {
public:
    static const int WIDTH = 128;
    static const int HEIGHT = 32;
    static const uint8_t ADDR = 0x3C;
    SSD1306_128x32();

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

class SSD1306_72x40 : public SSD1306 {
public:
    static const int WIDTH = 72;
    static const int HEIGHT = 40;
    static const uint8_t ADDR = 0x3C;
    SSD1306_72x40();

    void clear();
    bool display();

    bool init(int reset, int scl, int sda, int inst=0, bool _init=true);

private:
    uint8_t buffer[WIDTH*((HEIGHT+7)/8)];
    int instance;
    bool initok;
    bool sendCmd(uint8_t c);
    bool sendList(uint8_t header, const uint8_t *c, int len);
};
