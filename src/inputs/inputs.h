#pragma once

#include <stdint.h>

class Inputs {
public:
    using KeyBitmap = uint8_t;
    static const KeyBitmap OK = (1<<0);
    static const KeyBitmap LEFT = (1<<1);
    static const KeyBitmap RIGHT = (1<<2);
    static const KeyBitmap UP = (1<<3);
    static const KeyBitmap DOWN = (1<<4);

    Inputs() : pressedKeys(0), releasedKeys(0), currentKeys(0) {}

    void init();

    void process();

    inline KeyBitmap pressed(){ return pressedKeys; }
    inline KeyBitmap released(){ return releasedKeys; }
    inline KeyBitmap status(){ return currentKeys; }

    inline bool wasOkPressed(){
        return (pressed()&OK)!=0;
    }
    inline bool wasLeftPressed(){
        return (pressed()&LEFT)!=0;
    }
    inline bool wasRightPressed(){
        return (pressed()&RIGHT)!=0;
    }
    inline bool wasUpPressed(){
        return (pressed()&UP)!=0;
    }
    inline bool wasDownPressed(){
        return (pressed()&DOWN)!=0;
    }

    inline bool wasOkReleased(){
        return (released()&OK)!=0;
    }
    inline bool wasLeftReleased(){
        return (released()&LEFT)!=0;
    }
    inline bool wasRightReleased(){
        return (released()&RIGHT)!=0;
    }
    inline bool wasUpReleased(){
        return (released()&UP)!=0;
    }
    inline bool wasDownReleased(){
        return (released()&DOWN)!=0;
    }

    inline bool isOkActive(){
        return (status()&OK)!=0;
    }
    inline bool isLeftActive(){
        return (status()&LEFT)!=0;
    }
    inline bool isRightActive(){
        return (status()&RIGHT)!=0;
    }
    inline bool isUpActive(){
        return (status()&UP)!=0;
    }
    inline bool isDownActive(){
        return (status()&DOWN)!=0;
    }

private:
    KeyBitmap pressedKeys, releasedKeys, currentKeys;
};