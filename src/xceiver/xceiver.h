#pragma once

#include <stdint.h>
#include <vector>
#include <deque>

class Xceiver {
public:
    Xceiver(uint8_t rx, uint8_t tx, uint8_t pio_rx, uint8_t sm_rx, uint8_t pio_tx, uint8_t sm_tx, uint32_t baud);
    ~Xceiver();

    void init();
    void enable();
    void disable();
    void idle();
    void tx(const std::vector<uint8_t>& data);
    bool available();
    bool rx(std::vector<uint8_t>& data);

protected:
    uint8_t mRxPin, mRxPio, mRxSm;
    uint8_t mTxPin, mTxPio, mTxSm;
    uint32_t mRxOffset, mTxOffset;
    uint32_t mBaud;
    bool mEnabled;
    std::deque<uint8_t> mTx;
    std::deque<uint8_t> mRxBuffer;
    std::vector<uint8_t> mRxReady;
};
