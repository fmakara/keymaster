#include "dict8.h"

#include <stdio.h>
#include <stdarg.h>

Char8x6::Char8x6(uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5) : fixBuf{c0, c1, c2, c3, c4, c5, 0}, Sprite(7,8,1,fixBuf,false) {}
Char8x6::~Char8x6() {}

Dict8::Dict8(Sprite* b) : base(b) {};
Dict8::~Dict8() {};

#define MAX_STR_SIZE 128
int Dict8::print(int x, int y, const char* str, ...){
    char tmp[MAX_STR_SIZE];
    va_list ap;
    va_start(ap, str);
    vsnprintf(tmp, MAX_STR_SIZE-1, str, ap);
    va_end(ap);
    return printString(base, x, y, tmp);
}

int Dict8::printInto(Sprite* s, int x, int y, const char* str, ...){
    char tmp[MAX_STR_SIZE];
    va_list ap;
    va_start(ap, str);
    vsnprintf(tmp, MAX_STR_SIZE-1, str, ap);
    va_end(ap);
    return printString(s, x, y, tmp);
}

int Dict8::printString(Sprite* s, int x, int y, const char* str){
    for(int i=0; str[i]; i++){
        if(str[i]>=' ' && ((str[i]-' ')<96)){
            base->copy(charlist[str[i]-' '], x, y);
            x+=7;
        }
    }
    return x;
}

int Dict8::putch(int x, int y, char c){
    if(c>=' '){
        base->copy(charlist[c-' '], x, y);
        x+=7;
    }
    return x;
}

const Char8x6 Dict8::charlist[96] = {
    Char8x6(0x00,0x00,0x00,0x00,0x00,0x00),// space
    Char8x6(0x00,0x00,0xbf,0x00,0x00,0x00),// !
    Char8x6(0x00,0x07,0x00,0x07,0x00,0x00),// "
    Char8x6(0x20,0xf4,0x2f,0xf4,0x2f,0x04),// #
    Char8x6(0x00,0xa4,0x4a,0x52,0x25,0x00),// $
    Char8x6(0x06,0xc9,0x3e,0x64,0x93,0x60),// %
    Char8x6(0x60,0x96,0x99,0xa6,0x40,0xb0),// &
    Char8x6(0x00,0x00,0x03,0x00,0x00,0x00),// '
    Char8x6(0x00,0x00,0x7e,0x81,0x00,0x00),// ( 
    Char8x6(0x00,0x00,0x81,0x7e,0x00,0x00),// )
    Char8x6(0x0a,0x04,0x1f,0x04,0x0a,0x00),// *
    Char8x6(0x10,0x10,0x7c,0x10,0x10,0x00),// +
    Char8x6(0x00,0x00,0x80,0x60,0x00,0x00),// ,
    Char8x6(0x00,0x10,0x10,0x10,0x00,0x00),// -
    Char8x6(0x00,0x00,0x80,0x00,0x00,0x00),// .
    Char8x6(0x00,0xc0,0x38,0x06,0x01,0x00),// /
    Char8x6(0x7e,0x81,0x81,0x81,0x7e,0x00),// 0
    Char8x6(0x82,0x81,0xff,0x80,0x80,0x00),// 1
    Char8x6(0x82,0xc1,0xa1,0x91,0x8e,0x00),// 2
    Char8x6(0x42,0x89,0x89,0x89,0x76,0x00),// 3
    Char8x6(0x30,0x2c,0x23,0xff,0x20,0x00),// 4
    Char8x6(0x4f,0x89,0x89,0x89,0x71,0x00),// 5
    Char8x6(0x7c,0x8a,0x89,0x89,0x71,0x00),// 6
    Char8x6(0x01,0xc1,0x31,0x0d,0x03,0x00),// 7
    Char8x6(0x76,0x89,0x89,0x89,0x76,0x00),// 8
    Char8x6(0x8e,0x91,0x91,0x51,0x3e,0x00),// 9
    Char8x6(0x00,0x00,0x24,0x00,0x00,0x00),// :
    Char8x6(0x00,0x00,0x80,0x64,0x00,0x00),// ;
    Char8x6(0x10,0x10,0x28,0x28,0x44,0x00),// <
    Char8x6(0x24,0x24,0x24,0x24,0x24,0x00),// =
    Char8x6(0x44,0x28,0x28,0x10,0x10,0x00),// >
    Char8x6(0x00,0x02,0xb1,0x09,0x06,0x00),// ?
    Char8x6(0x7e,0xb9,0x45,0x3d,0x41,0x3e),// @
    Char8x6(0xf0,0x2c,0x23,0x2c,0xf0,0x00),// A
    Char8x6(0xff,0x89,0x89,0x8e,0x70,0x00),// B
    Char8x6(0x3c,0x42,0x81,0x81,0x81,0x42),// C
    Char8x6(0xff,0x81,0x81,0x81,0x42,0x3c),// D
    Char8x6(0x00,0xff,0x89,0x89,0x89,0x00),// E
    Char8x6(0xff,0x09,0x09,0x09,0x00,0x00),// F
    Char8x6(0x3c,0x42,0x81,0x91,0x91,0x72),// G
    Char8x6(0xff,0x08,0x08,0x08,0x08,0xff),// H
    Char8x6(0x00,0x00,0xff,0x00,0x00,0x00),// I
    Char8x6(0x00,0x00,0x80,0x80,0x7f,0x00),// J
    Char8x6(0xff,0x08,0x14,0x62,0x81,0x00),// K
    Char8x6(0x00,0xff,0x80,0x80,0x80,0x00),// L
    Char8x6(0xff,0x0c,0x30,0x0c,0xff,0x00),// M
    Char8x6(0xff,0x03,0x0c,0x30,0xc0,0xff),// N
    Char8x6(0x3c,0x42,0x81,0x81,0x42,0x3c),// O
    Char8x6(0xff,0x11,0x11,0x11,0x0e,0x00),// P
    Char8x6(0x3c,0x42,0x81,0xa1,0x42,0xbc),// Q
    Char8x6(0xff,0x11,0x11,0x31,0xce,0x00),// R
    Char8x6(0x00,0x46,0x89,0x91,0x62,0x00),// S
    Char8x6(0x01,0x01,0xff,0x01,0x01,0x00),// T
    Char8x6(0x7f,0x80,0x80,0x80,0x80,0x7f),// U
    Char8x6(0x0f,0x30,0xc0,0x30,0x0f,0x00),// V
    Char8x6(0x1f,0xe0,0x18,0xe0,0x1f,0x00),// W
    Char8x6(0xc3,0x24,0x18,0x24,0xc3,0x00),// X
    Char8x6(0x03,0x0c,0xf0,0x0c,0x03,0x00),// Y
    Char8x6(0xc1,0xa1,0x91,0x89,0x85,0x83),// Z
    Char8x6(0x00,0x00,0xff,0x81,0x00,0x00),// [
    Char8x6(0x00,0x01,0x06,0x38,0xc0,0x00),// backslash
    Char8x6(0x00,0x00,0x81,0xff,0x00,0x00),// ]
    Char8x6(0x00,0x02,0x01,0x02,0x00,0x00),// ^
    Char8x6(0x80,0x80,0x80,0x80,0x80,0x80),// _
    Char8x6(0x48,0xa8,0xa8,0xaa,0xf1,0x00),// aacute  -> '`'
    Char8x6(0x64,0x94,0x94,0x94,0xf8,0x00),// a
    Char8x6(0xff,0x88,0x88,0x88,0x70,0x00),// b
    Char8x6(0x00,0x78,0x84,0x84,0x84,0x00),// c
    Char8x6(0x70,0x88,0x88,0x88,0xff,0x00),// d
    Char8x6(0x00,0x78,0x94,0x94,0x98,0x00),// e
    Char8x6(0x00,0x08,0xfe,0x09,0x09,0x00),// f
    Char8x6(0xd6,0xa9,0xa9,0xa7,0x41,0x00),// g
    Char8x6(0xff,0x10,0x08,0x08,0xf0,0x00),// h
    Char8x6(0x00,0x00,0xfd,0x00,0x00,0x00),// i
    Char8x6(0x00,0x80,0x80,0x7d,0x00,0x00),// j
    Char8x6(0x00,0xff,0x10,0x68,0x84,0x00),// k
    Char8x6(0x00,0x00,0xff,0x00,0x00,0x00),// l
    Char8x6(0xfc,0x08,0x04,0xf8,0x04,0xf8),// m
    Char8x6(0xfc,0x08,0x04,0x04,0xf8,0x00),// n
    Char8x6(0x78,0x84,0x84,0x84,0x78,0x00),// o
    Char8x6(0xfc,0x24,0x24,0x24,0x18,0x00),// p
    Char8x6(0x18,0x24,0x24,0x24,0xfc,0x00),// q
    Char8x6(0x00,0xfc,0x08,0x04,0x04,0x00),// r
    Char8x6(0x00,0x98,0x94,0xa4,0x64,0x00),// s
    Char8x6(0x00,0x04,0x7f,0x84,0x84,0x00),// t
    Char8x6(0x7c,0x80,0x80,0x40,0xfc,0x00),// u
    Char8x6(0x0c,0x30,0xc0,0x30,0x0c,0x00),// v
    Char8x6(0x3c,0xc0,0x30,0x30,0xc0,0x3c),// w
    Char8x6(0x84,0x48,0x30,0x48,0x84,0x00),// x
    Char8x6(0x04,0xc8,0x30,0x08,0x04,0x00),// y
    Char8x6(0x00,0xc4,0xb4,0x8c,0x00,0x00),// z
    Char8x6(0x00,0x1c,0xa2,0x62,0x22,0x00),// ccedil -> '{'
    Char8x6(0x00,0x00,0xff,0x00,0x00,0x00),// |
    Char8x6(0x00,0x81,0x6e,0x10,0x00,0x00),// }
    Char8x6(0x02,0x49,0xa9,0xaa,0xaa,0xf1),// atilde -> '~'
    Char8x6(0x02,0x05,0x7a,0x84,0x84,0x84),// degc
};