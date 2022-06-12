#include "ssd1306.h"
#include <stdlib.h>
#include <string.h>
#include "hardware/i2c.h"
#include "pico/stdlib.h"

#define I2C (instance?&i2c1_inst:&i2c0_inst)

#define SSD1306_MEMORYMODE          0x20 ///< See datasheet
#define SSD1306_COLUMNADDR          0x21 ///< See datasheet
#define SSD1306_PAGEADDR            0x22 ///< See datasheet
#define SSD1306_SETCONTRAST         0x81 ///< See datasheet
#define SSD1306_CHARGEPUMP          0x8D ///< See datasheet
#define SSD1306_SEGREMAP            0xA0 ///< See datasheet
#define SSD1306_DISPLAYALLON_RESUME 0xA4 ///< See datasheet
#define SSD1306_DISPLAYALLON        0xA5 ///< Not currently used
#define SSD1306_NORMALDISPLAY       0xA6 ///< See datasheet
#define SSD1306_INVERTDISPLAY       0xA7 ///< See datasheet
#define SSD1306_SETMULTIPLEX        0xA8 ///< See datasheet
#define SSD1306_DISPLAYOFF          0xAE ///< See datasheet
#define SSD1306_DISPLAYON           0xAF ///< See datasheet
#define SSD1306_COMSCANINC          0xC0 ///< Not currently used
#define SSD1306_COMSCANDEC          0xC8 ///< See datasheet
#define SSD1306_SETDISPLAYOFFSET    0xD3 ///< See datasheet
#define SSD1306_SETDISPLAYCLOCKDIV  0xD5 ///< See datasheet
#define SSD1306_SETPRECHARGE        0xD9 ///< See datasheet
#define SSD1306_SETCOMPINS          0xDA ///< See datasheet
#define SSD1306_SETVCOMDETECT       0xDB ///< See datasheet

#define SSD1306_SETLOWCOLUMN        0x00 ///< Not currently used
#define SSD1306_SETHIGHCOLUMN       0x10 ///< Not currently used
#define SSD1306_SETSTARTLINE        0x40 ///< See datasheet

#define SSD1306_EXTERNALVCC         0x01 ///< External display voltage source
#define SSD1306_SWITCHCAPVCC        0x02 ///< Gen. display voltage from 3.3V

#define SSD1306_RIGHT_HORIZONTAL_SCROLL              0x26 ///< Init rt scroll
#define SSD1306_LEFT_HORIZONTAL_SCROLL               0x27 ///< Init left scroll
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29 ///< Init diag scroll
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL  0x2A ///< Init diag scroll
#define SSD1306_DEACTIVATE_SCROLL                    0x2E ///< Stop scroll
#define SSD1306_ACTIVATE_SCROLL                      0x2F ///< Start scroll
#define SSD1306_SET_VERTICAL_SCROLL_AREA             0xA3 ///< Set scroll range

static const uint8_t SSD1306_init1[] = {
    SSD1306_DISPLAYOFF,                   // 0xAE
    SSD1306_SETDISPLAYCLOCKDIV,           // 0xD5
    0x80,                                 // the suggested ratio 0x80
    SSD1306_SETMULTIPLEX };               // 0xA8
static const uint8_t SSD1306_init2[] = {
    SSD1306_SETDISPLAYOFFSET,             // 0xD3
    0x0,                                  // no offset
    SSD1306_SETSTARTLINE | 0x0,           // line #0
    SSD1306_CHARGEPUMP };                 // 0x8D
static const uint8_t SSD1306_init3[] = {
    SSD1306_MEMORYMODE,                   // 0x20
    0x01,                                 // 0x0 Vertical adressing
    SSD1306_SEGREMAP | 0x1,
    0xC0}; //SSD1306_COMSCANDEC };    
static const uint8_t SSD1306_init4a_64[] = {
    SSD1306_SETCOMPINS,                 // 0xDA
    SSD1306_SWITCHCAPVCC|0x10,
    SSD1306_SETCONTRAST,                // 0x81
    0xCF };
static const uint8_t SSD1306_init4a_32[] = {
    SSD1306_SETCOMPINS,                 // 0xDA
    0x02, //SSD1306_SWITCHCAPVCC|0x10,
    SSD1306_SETCONTRAST,                // 0x81
    0x8F}; //0xCF };
static const uint8_t SSD1306_init5[] = {
    SSD1306_SETVCOMDETECT,               // 0xDB
    0x40,
    SSD1306_DISPLAYALLON_RESUME,         // 0xA4
    SSD1306_NORMALDISPLAY,               // 0xA6
    SSD1306_DEACTIVATE_SCROLL,
    SSD1306_DISPLAYON };                 // Main screen turn on
static const uint8_t SSD1306_dlist_64[] = {
    SSD1306_PAGEADDR,
    0,                         // Page start address
    7,                         // end
    SSD1306_COLUMNADDR,        // Col start and end
    0,
    127};
static const uint8_t SSD1306_dlist_32[] = {
    SSD1306_PAGEADDR,
    0,                         // Page start address
    3,                         // end
    SSD1306_COLUMNADDR,        // Col start and end
    0,                               
    127}; 

bool SSD1306_128x64::sendCmd(uint8_t cmd){
    return sendList(0, &cmd, 1);
}

bool SSD1306_128x64::sendList(uint8_t header, const uint8_t *c, int len){
    uint8_t list[520];
    int lensent = 0;
    while(lensent<len){
        const int IIC_WRITE_BUFFER_MAX = 64;
        int currSize = (lensent+IIC_WRITE_BUFFER_MAX<len+1) ? IIC_WRITE_BUFFER_MAX : (len+1-lensent);
        memcpy(list+1, c+lensent, currSize-1);
        list[0] = header;
        //int retcode = i2c_write_timeout_per_char_us(I2C, ADDR, list, len-lensent+1, false, 50000);
        int retcode = i2c_write_blocking(I2C, ADDR, list, currSize, false);
        if(retcode<0)return false;
        lensent += retcode-1;
    }
    return lensent==len;
}

SSD1306_128x64::SSD1306_128x64()
    : SSD1306(WIDTH, HEIGHT, (uint8_t*)buffer)
    , initok(false)
{
    clear();
}

void SSD1306_128x64::clear(){
    memset(buffer, 0x00, sizeof(buffer));
}

bool SSD1306_128x64::display(){
    if(!initok) return false;
    sendList(0,SSD1306_dlist_64, sizeof(SSD1306_dlist_64));
    sendList(0x40, (uint8_t*)buffer, sizeof(buffer));
    sendCmd(SSD1306_COMSCANDEC);
    sendCmd(SSD1306_SEGREMAP | 0x1);
    return true;
}

bool SSD1306_128x64::init(int scl, int sda, int inst, bool _init){
    instance = inst;
    if(_init){
        i2c_init(I2C, 400 * 1000);
        gpio_set_function(sda, GPIO_FUNC_I2C);
        gpio_set_function(scl, GPIO_FUNC_I2C);
        gpio_pull_up(sda);
        gpio_pull_up(scl);
    }
    
    if(!sendList(0, SSD1306_init1, sizeof(SSD1306_init1))) return false;
    if(!sendCmd(HEIGHT - 1)) return false;
    if(!sendList(0, SSD1306_init2, sizeof(SSD1306_init2))) return false;
    if(!sendCmd(SSD1306_SETHIGHCOLUMN | 4)) return false; // internal VCC
    if(!sendList(0, SSD1306_init3, sizeof(SSD1306_init3))) return false;
    if(!sendList(0, SSD1306_init4a_64, sizeof(SSD1306_init4a_64))) return false;
    if(!sendCmd(SSD1306_SETPRECHARGE)) return false;
    if(!sendCmd(0xF1)) return false;
    if(!sendList(0, SSD1306_init5, sizeof(SSD1306_init5))) return false;
    initok = true;
    return true;
}

bool SSD1306_128x32::sendCmd(uint8_t cmd){
    return sendList(0, &cmd, 1);
}

bool SSD1306_128x32::sendList(uint8_t header, const uint8_t *c, int len){
    uint8_t list[520];
    int lensent = 0;
    while(lensent<len){
        const int IIC_WRITE_BUFFER_MAX = 64;
        int currSize = (lensent+IIC_WRITE_BUFFER_MAX<len+1) ? IIC_WRITE_BUFFER_MAX : (len+1-lensent);
        memcpy(list+1, c+lensent, currSize-1);
        list[0] = header;
        //int retcode = i2c_write_timeout_per_char_us(I2C, ADDR, list, len-lensent+1, false, 50000);
        int retcode = i2c_write_blocking(I2C, ADDR, list, currSize, false);
        if(retcode<0)return false;
        lensent += retcode-1;
    }
    return lensent==len;
}

SSD1306_128x32::SSD1306_128x32()
    : SSD1306(WIDTH, HEIGHT, (uint8_t*)buffer)
    , initok(false)
{
    clear();
}

void SSD1306_128x32::clear(){
    memset(buffer, 0x00, sizeof(buffer));
}

bool SSD1306_128x32::display(){
    if(!initok) return false;
    sendList(0,SSD1306_dlist_32, sizeof(SSD1306_dlist_32));
    sendList(0x40, (uint8_t*)buffer, sizeof(buffer));
    sendCmd(SSD1306_COMSCANDEC);
    sendCmd(SSD1306_SEGREMAP | 0x1);
    return true;
}

bool SSD1306_128x32::init(int scl, int sda, int inst, bool _init){
    instance = inst;
    if(_init){
        i2c_init(I2C, 400 * 1000);
        gpio_set_function(sda, GPIO_FUNC_I2C);
        gpio_set_function(scl, GPIO_FUNC_I2C);
        gpio_pull_up(sda);
        gpio_pull_up(scl);
    }

    if(!sendList(0, SSD1306_init1, sizeof(SSD1306_init1))) return false;
    if(!sendCmd(HEIGHT - 1)) return false;
    if(!sendList(0, SSD1306_init2, sizeof(SSD1306_init2))) return false;
    if(!sendCmd(SSD1306_SETHIGHCOLUMN | 4)) return false; // internal VCC
    if(!sendList(0, SSD1306_init3, sizeof(SSD1306_init3))) return false;
    if(!sendList(0, SSD1306_init4a_32, sizeof(SSD1306_init4a_32))) return false;
    if(!sendCmd(SSD1306_SETPRECHARGE)) return false;
    if(!sendCmd(0xF1)) return false;
    if(!sendList(0, SSD1306_init5, sizeof(SSD1306_init5))) return false;
    initok = true;
    return true;
}

SSD1306_72x40::SSD1306_72x40()
    : SSD1306(WIDTH, HEIGHT, (uint8_t*)buffer)
    , initok(false)
{
    clear();
}

void SSD1306_72x40::clear(){
    memset(buffer, 0x00, sizeof(buffer));
}

bool SSD1306_72x40::display(){
    if(!initok) return false;
    for(int page=0; page<HEIGHT/8; page++){
        sendCmd(0xB0+page);
        sendCmd(0x0C);
        sendCmd(0x11);
        /*
        uint8_t data[WIDTH];
        for(int i=0; i<WIDTH; i++){
            data[i] = buffer[page+i*(HEIGHT/8)];
        }
        sendList(0x40, data, WIDTH);
        */
        for(int i=0; i<WIDTH; i++){
            sendList(0x40, buffer+page+i*(HEIGHT/8), 1);
        }
    }
    return true;
}

bool SSD1306_72x40::init(int reset, int scl, int sda, int inst, bool _init){
    instance = inst;
    if(_init){
        i2c_init(I2C, 400 * 1000);
        gpio_set_function(sda, GPIO_FUNC_I2C);
        gpio_set_function(scl, GPIO_FUNC_I2C);
        gpio_pull_up(sda);
        gpio_pull_up(scl);
    }
    gpio_init(reset);
    gpio_set_dir(reset, true);
    gpio_put(reset, false);
    sleep_ms(100);
    gpio_put(reset, true);
    sleep_ms(100);

    static const uint8_t SSD1306_72x40_init[] = {
        SSD1306_DISPLAYOFF,         // 0xAE
        SSD1306_SETDISPLAYCLOCKDIV, // 0xD5
        0x80,                       // the suggested ratio 0xF0
        SSD1306_SETMULTIPLEX,       // 0xA8
        0x27,                       // recommended ratio 0x27 -> height-1
        SSD1306_SETDISPLAYOFFSET,   // 0xD3
        0x00,                       // recommended 0x00
        0xAD,                       // Internal IREF
        0x30,                       // settingj
        SSD1306_CHARGEPUMP,         // 0x8D
        0x14,                       // recommended 0x14 for internal DC-DC
        SSD1306_SETSTARTLINE,       // 0x40
        SSD1306_NORMALDISPLAY,      // 0xA6
        SSD1306_DISPLAYALLON_RESUME,// 0xA4
        SSD1306_SEGREMAP | 0x1,     // 0xA1 // remap 128 to 0
        SSD1306_COMSCANDEC,         // 0xC8
        SSD1306_SETCOMPINS,         // 0xDA
        0x12,                       // Recommended 0x12
        SSD1306_SETCONTRAST,        // 0x81
        0xAF,                       // recommended 0x2F
        SSD1306_SETPRECHARGE,       // 0xD9
        0x22,                       // recommended 0x22
        SSD1306_SETVCOMDETECT,      // 0xDB
        0x20,                       // recommended 0x20
    };

    if(!sendList(0, SSD1306_72x40_init, sizeof(SSD1306_72x40_init))) return false;
    // for(int i=0; i<sizeof(SSD1306_72x40_init); i++)sendCmd(SSD1306_72x40_init[i]);
    sleep_ms(100);
    if(!sendCmd(SSD1306_DISPLAYON)) return false; // 0xAF
    sleep_ms(100);
    initok = true;
    return true;
}

bool SSD1306_72x40::sendList(uint8_t header, const uint8_t *c, int len){
    uint8_t list[520];
    int lensent = 0;
    while(lensent<len){
        const int IIC_WRITE_BUFFER_MAX = 64;
        int currSize = (lensent+IIC_WRITE_BUFFER_MAX<len+1) ? IIC_WRITE_BUFFER_MAX : (len+1-lensent);
        memcpy(list+1, c+lensent, currSize-1);
        list[0] = header;
        //int retcode = i2c_write_timeout_per_char_us(I2C, ADDR, list, len-lensent+1, false, 50000);
        int retcode = i2c_write_blocking(I2C, ADDR, list, currSize, false);
        if(retcode<0)return false;
        lensent += retcode-1;
    }
    return lensent==len;
}

bool SSD1306_72x40::sendCmd(uint8_t cmd){
    return sendList(0, &cmd, 1);
}

SSD1306::SSD1306(uint16_t w, uint16_t h, uint8_t* b) : Sprite(w, h, (h+7)/8, b, false){}


/*
static const uint8_t SSD1306_72x40_init[] = {
        SSD1306_DISPLAYOFF,         // 0xAE
        SSD1306_SETDISPLAYCLOCKDIV, // 0xD5
        0xF0,                       // the suggested ratio 0xF0
        SSD1306_SETMULTIPLEX,       // 0xA8
        0x27,                       // recommended ratio 0x27 -> height-1
        SSD1306_SETDISPLAYOFFSET,   // 0xD3
        0x00,                       // recommended 0x00
        SSD1306_SETSTARTLINE,       // 0x40
        SSD1306_CHARGEPUMP,         // 0x8D
        0x14,                       // recommended 0x14 for internal DC-DC
        SSD1306_MEMORYMODE,         // 0x20
        0x01,                       // 0x0 Vertical adressing
        SSD1306_SEGREMAP | 0x1,     // 0xA1
        0xC0,                       // SSD1306_COMSCANDEC?
        // 0xA1,                       // Segment remap 0xA1
        // SSD1306_COMSCANDEC,         // 0xC8
        SSD1306_SETCOMPINS,         // 0xDA
        0x12,                       // Recommended 0x12
        SSD1306_SETCONTRAST,        // 0x81
        0x2F,                       // recommended 0x2F
        SSD1306_SETPRECHARGE,       // 0xD9
        0xF1,                       // recommended 0x22
        SSD1306_SETVCOMDETECT,      // 0xDB
        0x20,                       // recommended 0x20
        SSD1306_DISPLAYALLON_RESUME,// 0xA4
        SSD1306_NORMALDISPLAY,      // 0x
        SSD1306_DEACTIVATE_SCROLL,  // 0x
    };
    */