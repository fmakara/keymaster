#pragma once

#include <stdint.h>
#include <array>
#include <vector>

// a 64 pixel height optimized sprite handler
class Sprite {
public:
    Sprite(uint8_t w, uint8_t h);
    Sprite(uint8_t w, uint8_t h, uint8_t s, uint8_t* buf, bool f=false);
    Sprite(Sprite& src, uint8_t x, uint8_t y, uint8_t w, uint8_t h);
    ~Sprite();

    uint8_t* rawBuffer() const;
    uint8_t width() const;
    uint8_t height() const;
    uint8_t stride() const;

    enum Color {
        BLACK=0,
        WHITE,
        INVERT
    };
    void setPixel(uint8_t x, uint8_t y, Color c);
    Color getPixel(uint8_t x, uint8_t y);

    void vertLine(uint8_t x, uint8_t y0, uint8_t y1, Color c);
    void horzLine(uint8_t x0, uint8_t x1, uint8_t y, Color c);
    void rect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, Color c);

    void copy(const Sprite& src, int16_t x, int16_t y);

protected:
    uint8_t _w, _h, _x, _y, _s;
    uint8_t *buffer;
    bool _f;
};
/*
template <int N>
class Sprite8xn : public Sprite {
public:
    Char8xn(const std::array<uint8_t,N>& c) : fixBuf(c), Sprite(N,8,(N+7)/8,fixBuf.data(),false) {}
protected:
    std::array<uint8_t,N> fixBuf;
};*/

class Sprite8xnV : public Sprite {
public:
    Sprite8xnV(const std::vector<uint8_t>& c) 
        : fixBuf(c)
        , Sprite(c.size(),8,0,NULL,false)
    {
        buffer = fixBuf.data();
    }
protected:
    std::vector<uint8_t> fixBuf;
};