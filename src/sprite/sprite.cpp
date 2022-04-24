#include "sprite.h"
#include <stdlib.h>
#include <string.h>

Sprite::Sprite(uint8_t w, uint8_t h){
    _w = w;
    _h = h;
    _x = 0;
    _y = 0;
    _s = (h+7)/8;
    buffer = (uint8_t*)malloc(_s*_x);
    _f = true;
}

Sprite::Sprite(uint8_t w, uint8_t h, uint8_t s, uint8_t* buf, bool f){
    _w = w;
    _h = h;
    _x = 0;
    _y = 0;
    uint8_t ps = (h+7)/8;
    _s = (s<ps) ? ps : s;
    buffer = buf;
    _f = f;
}

Sprite::Sprite(Sprite& src, uint8_t x, uint8_t y, uint8_t w, uint8_t h){
    if(x>src._w || y>src._h || w>src._w || h>src._y){
        _w = 0;
        _h = 0;
        _x = 0;
        _y = 0;
        _s = 0;
        buffer = NULL;
        _f = false;
    } else {
        _w = (w+x<src._w) ? w : src._w-x;
        _h = (h+y<src._h) ? h : src._h-y;
        _x = x+src._x;
        _y = y+src._y;
        _s = src._s;
        buffer = src.buffer;
        _f = false;
    }
}

Sprite::~Sprite(){
    if(_f && buffer)free(buffer);
}

uint8_t* Sprite::rawBuffer() const { return buffer; }
uint8_t Sprite::width() const { return _w; }
uint8_t Sprite::height() const { return _h; }
uint8_t Sprite::stride() const { return _s; }

void Sprite::setPixel(uint8_t x, uint8_t y, Color c){
    if(x>=_w || y>=_h)return;
    uint8_t ny = y+_y;
    uint8_t bit = 1UL<<(ny&7);
    uint8_t *col = buffer + _s*(x+_x) + (ny>>3);
    if(c==BLACK) *col &= ~bit;
    else if(c==WHITE) *col |= bit;
    else *col ^= bit;
}

Sprite::Color Sprite::getPixel(uint8_t x, uint8_t y){
    if(x>=_w || y>=_h)return BLACK;
    uint8_t ny = y+_y;
    uint8_t bit = 1UL<<(ny&7);
    uint8_t *col = buffer + _s*(x+_x) + (ny>>3);
    if(*col & bit)return WHITE;
    return BLACK;
}

void Sprite::vertLine(uint8_t x, uint8_t y0, uint8_t y1, Color c){
    if(x>=_w || y1<y0 || y0>=_h || y1>=_h)return;
    uint64_t bit = ((1ULL<<(y1+_y+1))-1) & ~((1ULL<<(y0+_y))-1);
    uint64_t col;
    uint8_t *ptr = buffer + _s*(x+_x);
    memcpy(&col, ptr, _s);
    if(c==BLACK) col &= ~bit;
    else if(c==WHITE) col |= bit;
    else col ^= bit;
    memcpy(ptr, &col, _s);
}

void Sprite::horzLine(uint8_t x0, uint8_t x1, uint8_t y, Color c){
    if(x0>=_w || x1>=_w || x1<x0 || y>=_h)return;
    uint64_t bit = 1ULL<<(y+_y);
    for(int i=x0; i<x1; i++){
        uint8_t *ptr = buffer + _s*(i+_x);
        uint64_t col;
        memcpy(&col, ptr, _s);
        if(c==BLACK) col &= ~bit;
        else if(c==WHITE) col |= bit;
        else col ^= bit;
        memcpy(ptr, &col, _s);
    }
}

void Sprite::rect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, Color c){
    if(x0>=_w || x1>=_w || x1<x0 || y1<y0 || y0>=_h || y1>=_h)return;
    uint64_t bit = ((1<<(y1+_y))-1) & ~((1<<(y0+_y))-1);
    for(int i=x0; i<x1; i++){
        uint8_t *ptr = buffer + _s*(i+_x);
        uint64_t col;
        memcpy(&col, ptr, _s);
        if(c==BLACK) col &= ~bit;
        else if(c==WHITE) col |= bit;
        else col ^= bit;
        memcpy(ptr, &col, _s);
    }
}

void Sprite::copy(const Sprite& src, int16_t x, int16_t y){
    if(x>=_w || y>=_h || (-x)>src._w || (-y)>src._h)return;
    uint64_t destMask = ~(((1<<(y+_y+src._h))-1) & ~((1<<(y+_y))-1));
    uint64_t srcMask = ((1<<(src._h))-1);
    for(int i=0; i<src._w; i++){
        int16_t dx = i+_x+x;
        if(dx>=0 && dx<_w){
            uint64_t colSrc, colDest;
            uint8_t *ptrDest = buffer + _s*dx;
            memcpy(&colSrc, src.buffer + src._s*(i+src._x), src._s);
            memcpy(&colDest, ptrDest, _s);
            colSrc = ((colSrc>>src._y)&srcMask)<<(y+_y);
            colDest = (colDest&destMask) | colSrc;
            memcpy(ptrDest, &colDest, _s);
        }
    }
}
