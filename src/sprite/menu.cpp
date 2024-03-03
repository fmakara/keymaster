#include "menu.h"

#include <vector>
#include <string>
#include <cstring>
#include "dict8.h"
#include "sprite.h"
#include "pico/time.h"

#define MENUSTR_VERTCH    0x01
#define MENUSTR_ESC       0x02
#define MENUSTR_OK        0x03
#define MENUSTR_BACK      0x04
#define MENUSTR_SHOW      0x05

bool Menu::getString(SSD1306& screen, uint32_t (*input)(void), char* str, unsigned maxsize){
    static const std::vector<std::string> menulists = {
        "\x01" "abcdefghijkl" "\x01" "mnopqrstuvwxyz",
        "\x01" "ABCDEFGHIJKL" "\x01" "MNOPQRSTUVWXYZ",
        "\x01" "0123456789 ",
        "\x01" "@.,-_+=*~^:;'\"!?" "\x01" "#$%¨&/\\|()<>{}[]",
        "\x01\x04\x03\x02\x05",
    };
    static const Sprite8xnV specials[6] = {
        Sprite8xnV({0xFF}),
        Sprite8xnV({0x3C,0x42,0x81,0x81,0x89,0x4A,0x0C,0x0F}),
        Sprite8xnV({
            0x3C,0x42,0x81,0x81,0x42,0x00, // C
            0x64,0x94,0x94,0x78,0x00, // a
            0xFC,0x08,0x04,0xF8,0x00, // n
            0x78,0x84,0x84,0x48,0x00, // c
            0x78,0x94,0x94,0x58,0x00, // e
            0x7F,0x80,0x00, // l
            0x64,0x94,0x94,0x78,0x00, // a
            0xFC,0x08,0x04,0x08 // r
        }),
        Sprite8xnV({
            0x3C,0x42,0x81,0x81,0x81,0x42,0x3C,0x00, // O
            0xFF,0x08,0x18,0x24,0x42,0x81 // K
        }),
        Sprite8xnV({
            0xFF,0x89,0x89,0x76,0x00, // B
            0x64,0x94,0x94,0x78,0x00, // a
            0x78,0x84,0x84,0x48,0x00, // c
            0xFC,0x20,0x50,0x88,0x00, // k
            0x46,0x89,0x89,0x72,0x00, // S
            0xFC,0x24,0x24,0x18,0x00, // p
            0x64,0x94,0x94,0x78,0x00, // a
            0x78,0x84,0x84,0x48,0x00, // c
            0x78,0x94,0x94,0x58,0x00, // e
        }),
        Sprite8xnV({
            0x46,0x89,0x89,0x72,0x00, // S
            0xFC,0x10,0x10,0xE0,0x00, // h
            0x78,0x84,0x84,0x78,0x00, // o
            0x70,0x80,0x60,0x80,0x70, // w
        }),
    };
    static const Sprite8xnV cursor[2] = {
        Sprite8xnV({0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}),  
        Sprite8xnV({0xFF,0x81,0x81,0x81,0x81,0xFF}),  
    };

    unsigned line=0, col=0, len = strlen(str);
    Dict8 writer(&screen);
    while(true){
        rand();
        uint32_t read = (*input)();
        if(read&BTN_OK){
            if(menulists[line][col]==MENUSTR_OK){
                str[len] = 0;
                return true;
            }
            if(menulists[line][col]==MENUSTR_ESC){
                return false;
            }
            if(menulists[line][col]==MENUSTR_VERTCH){
                col = 0;
                line = (line+1)%menulists.size();
            } else if(menulists[line][col]==MENUSTR_BACK) {
                if(len>0) len--;
            } else if (menulists[line][col]==MENUSTR_SHOW) {
                int initialDelta = screen.width()-25;
                read = (*input)();
                while(!read){
                    read = (*input)();
                    screen.clear();
                    int xpos = initialDelta-8;
                    for(int i=0; i<len && xpos>-6; i++){
                        writer.putch(xpos, 1, str[len-i-1]);
                        xpos -= 7;
                    }
                    screen.display();
                    if(xpos>0) break;
                    if(read) break;
                    initialDelta += 1;
                }
                sleep_ms(500);
                while(!read){
                    read = (*input)();
                    screen.clear();
                    int xpos = initialDelta-8;
                    for(int i=0; i<len && xpos>-6; i++){
                        writer.putch(xpos, 1, str[len-i-1]);
                        xpos -= 7;
                    }
                    screen.display();
                    if(initialDelta<100) break;
                    initialDelta -= 1;
                }
            } else {
                if(len<maxsize){
                    str[len] = menulists[line][col];
                    len++;
                }
            }
        }
        if(read&BTN_UP){
            col = 0;
            line = (line+1)%menulists.size();
        }
        if(read&BTN_DOWN){
            col = 0;
            line = (line+menulists.size()-1)%menulists.size();
        }
        if(read&BTN_CHR){
            char chr = read&0xFF;
            if(chr==CHR_BACKSPACE){
                if(len>0) len--;
            } else {
                if(len<maxsize){
                    str[len] = chr;
                    len++;
                }
            }
        }
        if(read&BTN_CANCEL){
            return false;
        }
        int totaldelta = 0;
        if(read&BTN_LEFT)totaldelta = -1;
        if(read&BTN_RIGHT)totaldelta = 1;
        col = (col + menulists[line].size() - totaldelta)%menulists[line].size();

        screen.clear();
        uint8_t blink = (to_ms_since_boot(get_absolute_time())/500)&1;
        screen.copy(cursor[blink], 100, 1);
        int xpos = screen.width()-35; // 100-8;
        for(int i=0; i<len && xpos>-6; i++){
            writer.putch(xpos, 1, str[len-i-1]);
            xpos -= 7;
        }

        uint8_t centeroff, centerwidth, centericon = menulists[line][col];
        if(centericon>=' '){
            auto& sprite = Dict8::charlist[centericon-' '];
            centerwidth = sprite.width();
            centeroff = screen.width()/2-(centerwidth/2);
            screen.copy(sprite, centeroff, 16);
        } else {
            auto& sprite = specials[centericon];
            centerwidth = sprite.width();
            centeroff = screen.width()/2-(centerwidth/2);
            screen.copy(sprite, centeroff, 16);
        }

        writer.putch(centeroff+centerwidth+2, 16, '>');
        writer.putch(centeroff-(2+7), 16, '<');

        xpos = centeroff+centerwidth+16;
        int colinc = col;
        while(xpos<screen.width()){
            colinc = (colinc+1)%menulists[line].size();
            uint8_t width, icon = menulists[line][colinc];
            if(icon>=' '){
                auto& sprite = Dict8::charlist[icon-' '];
                width = sprite.width();
                screen.copy(sprite, xpos, 16);
            } else {
                auto& sprite = specials[icon];
                width = sprite.width();
                screen.copy(sprite, xpos, 16);
            }
            xpos += width+8;
        }

        xpos = centeroff-16;
        colinc = col;
        while(xpos>-8){
            colinc = (colinc+menulists[line].size()-1)%menulists[line].size();
            uint8_t width, icon = menulists[line][colinc];
            if(icon>=' '){
                auto& sprite = Dict8::charlist[icon-' '];
                width = sprite.width();
                screen.copy(sprite, xpos-width, 16);
            } else {
                auto& sprite = specials[icon];
                width = sprite.width();
                screen.copy(sprite, xpos-width, 16);
            }
            xpos -= width+8;
        }
        screen.display();
    }
}

bool Menu::confirm(SSD1306& screen, uint32_t (*input)(void), const std::vector<std::string>& strs){
    Dict8 writer(&screen);
    int opt = 0;
    while(true){
        rand();
        uint32_t read = (*input)();
        if(read&BTN_OK){
            if(opt>0)return true;
            if(opt<0)return false;
        }
        if(read&BTN_CANCEL) return false;
        if(read&BTN_LEFT) opt = 1;
        if(read&BTN_RIGHT) opt = -1;

        screen.clear();
        int offy = 0;
        for(int i=0; i<strs.size(); i++){
            int off = (screen.width()-strs[i].size()*7)/2;
            if(off<0) off=0;
            writer.print(off,offy,strs[i].c_str());
            offy += 10;
        }
        if(screen.width()>83){
            if(opt==0){
                writer.print((screen.width()-83)/2,offy," Sim <> Nao ");
            } else if(opt>0){
                writer.print((screen.width()-83)/2,offy,"<Sim>   Nao ");
            } else {
                writer.print((screen.width()-83)/2,offy," Sim   <Nao>");
            }
        } else {
            if(opt==0){
                writer.print((screen.width()-70)/2,offy," Sim<>Nao ");
            } else if(opt>0){
                writer.print((screen.width()-70)/2,offy,"<Sim> Nao ");
            } else {
                writer.print((screen.width()-70)/2,offy," Sim <Nao>");
            }
        }
        screen.display();
    }
}

static int pow10(int val){
    if(val<0) val=-val;
    if(val<10) return 1;
    if(val<100) return 2;
    if(val<1000) return 3;
    if(val<10000) return 4;
    if(val<100000) return 5;
    if(val<1000000) return 6;
    if(val<10000000) return 7;
    if(val<100000000) return 8;
    if(val<1000000000) return 9;
    return 10;
}
int Menu::intRead(SSD1306& screen, uint32_t (*input)(void), int val, int min, int max, const char* descr){
    Dict8 writer(&screen);
    while(true){
        rand();
        uint32_t read = (*input)();
        if(read&BTN_OK){
            return val;
        }
        if(read&BTN_CANCEL){
            return min-1;
        }
        if(read&BTN_DOWN) val--;
        if(read&BTN_UP) val++;
        if (val>max) val = max;
        if (val<min) val = min;

        screen.clear();
        if(descr){
            int len = strlen(descr);
            int off = (screen.width()-(len*7))/2;
            if(off<0) off=0;
            writer.print(off,0,descr);
        }
        int off = (screen.width()-((pow10(val)+4*7)))/2;
        if(val==min){
            writer.print(off, 16,"  %d >", val);
        } else if(val==max){
            writer.print(off, 16,"< %d  ", val);
        } else {
            writer.print(off, 16,"< %d >", val);
        }
        screen.display();
    }
}

int Menu::genericList(SSD1306& screen, uint32_t (*input)(void), const std::vector<std::string>& items, int startingItem){
    Dict8 writer(&screen);
    static const uint8_t speedMul = 2;
    static const uint8_t speedSm = 2;
    int idx = startingItem;
    int sm = 0;
    if(items.size()==0) return 0;
    if(items.size()<=(screen.height()/8)) {
        while(true){
            rand();
            uint32_t read = (*input)();
            if(read&BTN_OK){
                return idx;
            }
            if(read&BTN_CANCEL){
                return -1;
            }
            if(read&BTN_UP) idx = (idx+items.size()-1)%items.size();
            if(read&BTN_DOWN) idx = (idx+1)%items.size();
            if(read) sm = 0;
            sm++;

            screen.clear();
            for(int i=0; i<items.size(); i++){
                if(i==idx){
                    if(sm<100) {
                        int endpos = writer.print(8,i*8,"%s",items[i].c_str());
                        if(endpos<screen.width()) sm = 0;
                    } else {
                        int endpos = writer.print(108-sm,i*8,"%s",items[i].c_str());
                        if(endpos<5) sm = 0;
                    }
                    writer.print(0,i*8,">");
                } else {
                    writer.print(3,i*8,"%s",items[i].c_str());
                }
            }
            screen.display();
        }
    }
    while(true){
        rand();
        uint32_t read = (*input)();
        if(read&BTN_OK){
            return idx;
        }
        if(read&BTN_CANCEL){
            return -1;
        }
        if(read&BTN_UP) {
            for(int k=0; k<8; k+=speedMul){
                screen.clear();
                writer.print(0,9,">");
                for(int i=2; i<(screen.height()/8); i++){
                    int j = (idx+i-1+items.size())%items.size();
                    writer.print(0,i*9+k,"%s",items[j].c_str());
                }
                int j = (idx-1+items.size())%items.size();
                writer.print(k,k,"%s",items[j].c_str());
                j = (idx+items.size())%items.size();
                writer.print(8-k,9+k,"%s",items[j].c_str());
                screen.display();
                (*input)();
            }
            idx = (idx+items.size()-1)%items.size();
        }
        if(read&BTN_DOWN) {
            for(int k=0; k<8; k+=speedMul){
                screen.clear();
                writer.print(0,9,">");
                for(int i=3; i<(screen.height()/8); i++){
                    int j = (idx+i-1+items.size())%items.size();
                    writer.print(0,i*9-k,"%s",items[j].c_str());
                }
                int j = (idx-1+items.size())%items.size();
                writer.print(0,-k,"%s",items[j].c_str());
                j = (idx+items.size())%items.size();
                writer.print(8-k,9-k,"%s",items[j].c_str());
                j = (idx+1+items.size())%items.size();
                writer.print(k,18-k,"%s",items[j].c_str());
                screen.display();
                (*input)();
            }
            idx = (idx+1)%items.size();
        }
        if(read) sm = 0;
        sm+=speedSm;

        screen.clear();
        for(int i=0; i<(screen.height()/8); i++){
            if(i==1) continue;
            int j = (idx+i-1+items.size())%items.size();
            writer.print(0,i*9,"%s",items[j].c_str());
        }
        if(sm<100) {
            int endpos = writer.print(8,9,"%s",items[idx].c_str());
            if(endpos<screen.width()) sm = 0;
        } else {
            int endpos = writer.print(108-sm,9,"%s",items[idx].c_str());
            if(endpos<5) sm = 0;
        }
        writer.print(0,9,">");
        screen.display();
    }
}

void Menu::pinReader(SSD1306& screen,  uint32_t (*input)(void), uint8_t *pin, uint8_t len){
    Dict8 writer(&screen);
    uint8_t off=0;
    uint8_t ctrl = 0;
    static const uint8_t FINISH=0;
    static const uint8_t BACK=1;

    while(true){
        rand();
        uint32_t read = (*input)();
        if(read&BTN_OK){
            if(off<len){
                off++;
            } else {
                if(ctrl == FINISH) return;
                if(ctrl == BACK) off=0;
            }
        }

        if(read&BTN_UP || read&BTN_RIGHT) {
            if(off<len){
                pin[off] = (pin[off]+1)%10;
            } else {
                ctrl = (ctrl+1)%2;
            }
        }
        if(read&BTN_DOWN || read&BTN_LEFT) {
            if(off<len){
                pin[off] = (pin[off]+9)%10;
            } else {
                ctrl = (ctrl+1)%2;
            }
        }

        screen.clear();
        for(int i=0; i<len; i++){
            char str[] = {(char)('0'+pin[i]), 0};
            writer.print(i*8, 10, str);
        }
        writer.putch(off*8, 0, '^');
        writer.putch(off*8, 20, 'v');
        if(ctrl == FINISH) writer.print(len*8, 10, "OK");
        if(ctrl == BACK) writer.print(len*8, 10, "RE");
        screen.display();
    }
}