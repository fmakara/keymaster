#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <list>
#include <string>
#include <vector>
#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "pico/time.h"
#include "pico/bootrom.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/regs/resets.h"
#include "hardware/resets.h"

#include "bsp/board.h"
#include "tusb.h"
#include "usb/usb_descriptors.h"
#include "pio_pwm.pio.h"

#include "display/ssd1306.h"
#include "sprite/dict8.h"
#include "sprite/menu.h"
#include "pers/persistency.h"
#include "xceiver/xceiver.h"

#define EVT_NUMLOCK 1
#define EVT_CAPSLOCK 2
#define EVT_SCROLLLOCK 3

#define PIN_XCEIVER_RX    (12)
#define PIN_XCEIVER_TX    (13)
#define PIN_SELECT_IR     (20)
#define PIN_SELECT_LED    (19)
#define PIN_DISPLAY_SDA   (4)
#define PIN_DISPLAY_SCL   (5)
#define PIN_DISPLAY_RESET (7)

bool lastCaps = false, lastNum = false, lastScroll = false, lastKeysInited = false;
std::list<char> hidCharList;
SSD1306_72x40 display;
Dict8 writer(&display);
Xceiver xceiver(PIN_XCEIVER_RX, PIN_XCEIVER_TX, 1, 0, 1, 1, 115200);
volatile static uint8_t pauseUsbHandling;

queue_t usbHIDKeyQueue, usbHIDEvtQueue, usbCDCinputQueue;
const uint8_t ascii2keycode[128][2] = {HID_ASCII_TO_KEYCODE};

void sendHID(const std::string &s){
    for (int i = 0; i < s.size(); i++)
        queue_add_blocking(&usbHIDKeyQueue, &s[i]);
}

inline static uint64_t micros(){
    return to_us_since_boot(get_absolute_time());
}

#define IR_CNT (3)
const uint8_t ledPins[IR_CNT] = {23, 22, 21};
const uint8_t adcPins[IR_CNT] = {28, 27, 26};
const uint8_t adcNums[IR_CNT] = {2,  1,  0};

void readRawAnalogInputs(uint16_t *dark, uint16_t *bright){
    static const uint8_t binFilter = 2;
    static const uint8_t multFilter = (1 << binFilter) - 1;
    static uint32_t lastRawRead[2 * IR_CNT] = {0};
    static bool startedUp = false;
    uint32_t currentRead[2 * IR_CNT];
    gpio_put(PIN_SELECT_LED, true); // inverted logic... LED OFF
    for (int i = 0; i < IR_CNT; i++) gpio_put(ledPins[i], false);
    gpio_put(PIN_SELECT_IR, false); // inverted logic... IR ON
    sleep_us(100);
    for (int i = 0; i < IR_CNT; i++){
        adc_select_input(adcNums[i]);
        currentRead[0 + i] = adc_read();
        gpio_put(ledPins[i], true);
        sleep_us(100);
        currentRead[IR_CNT + i] = adc_read();
        gpio_put(ledPins[i], false);
        sleep_us(100);
    }

    if (!startedUp){
        startedUp = true;
        for (int i = 0; i < 2 * IR_CNT; i++)
            lastRawRead[i] = currentRead[i];
    }
    for (int i = 0; i < 2 * IR_CNT; i++)
        lastRawRead[i] = (lastRawRead[i] * multFilter + currentRead[i]) >> binFilter;
    for (int i = 0; i < IR_CNT; i++){
        dark[i] = lastRawRead[i + 0];
        bright[i] = lastRawRead[i + IR_CNT];
    }
}

uint8_t interpretRawIR(uint16_t *dark, uint16_t *bright){
    static const uint16_t th = 100;
    uint8_t ret = 0;
    for (int i = 0; i < IR_CNT; i++){
        if (dark[i] < bright[i] + 300) ret |= (1 << i);
    }
    return ret;
}

void turnOffPWM(){
    gpio_put(PIN_SELECT_LED, true); // inverted logic... LED OFF
    for (int i = 0; i < IR_CNT; i++){
        pio_sm_set_enabled(pio0, i, false);
        gpio_init(ledPins[i]);
        gpio_set_dir(ledPins[i], true);
        gpio_put(ledPins[i], false);
    }
    gpio_put(PIN_SELECT_IR, false); // inverted logic... IR ON
}

uint8_t filterInputsAndPWM(uint8_t inputs){
    static const uint8_t CNTS[IR_CNT] = {20, 15, 15};
    static const uint8_t RETS[IR_CNT] = {21, 5, 5};
    static uint8_t ledCounters[IR_CNT] = {0};
    uint8_t outputs = 0;
    gpio_put(PIN_SELECT_IR, true); // inverted logic... IR OFF
    for (int i = 0; i < IR_CNT; i++){
        if (((inputs >> i) & 1) == 0){
            ledCounters[i] = 0;
        }else if (ledCounters[i] < CNTS[i]){
            ledCounters[i]++;
        }
        uint32_t pwmLev = (64 * (uint16_t)ledCounters[i]) / CNTS[i];
        if (pwmLev > 64) pwmLev = 64;
        if (ledCounters[i] == CNTS[i]){
            outputs |= 1 << i;
            ledCounters[i] = RETS[i];
        }
        // setup PWM
        pio_gpio_init(pio0, ledPins[i]);
        pio_sm_set_enabled(pio0, i, true);
        pio_sm_put_blocking(pio0, i, pwmLev);
    }
    gpio_put(PIN_SELECT_LED, false); // inverted logic... LED ON
    return outputs;
}

uint8_t readIR(){
    uint16_t dark[IR_CNT], bright[IR_CNT];
    readRawAnalogInputs(dark, bright);
    // a bit more spice for rand operations
    uint16_t randAcc = 0xCAFE;
    for (int i = 0; i < IR_CNT; i++)
        randAcc ^= dark[i] ^ bright[i];
    randAcc ^= randAcc >> 8;
    randAcc ^= randAcc >> 4;
    randAcc &= 0xF;
    for (int i = 0; i < randAcc; i++) rand();
    return filterInputsAndPWM(interpretRawIR(dark, bright));
}

void setLeds(uint8_t leds){
    gpio_put(PIN_SELECT_IR, true); // inverted logic... IR OFF
    gpio_put(PIN_SELECT_LED, false); // inverted logic... LED ON
    for (int i = 0; i < IR_CNT; i++){
        gpio_put(ledPins[i], (leds & (1 << i)) != 0);
    }
}

uint8_t filterReadIR(){
    turnOffPWM();
    uint8_t act = readIR();
    return act;
}

uint32_t commonKeyboardRead(){
    uint32_t ret = 0;
    char evt;
    if (queue_try_remove(&usbHIDEvtQueue, &evt)){
        if (evt == EVT_NUMLOCK){
            ret |= Menu::BTN_LEFT;
        }else if (evt == EVT_CAPSLOCK){
            ret |= Menu::BTN_OK;
        }else if (evt == EVT_SCROLLLOCK){
            ret |= Menu::BTN_RIGHT;
        }
    }
    char cdc;
    if (queue_try_remove(&usbCDCinputQueue, &cdc)){
        if (cdc >= ' ' && cdc <= '~'){
            ret |= Menu::BTN_CHR | cdc;
        }else if (cdc == 0x7F){
            ret |= Menu::BTN_CHR | Menu::CHR_BACKSPACE;
        }else if (cdc == '\n' || cdc == '\r'){
            ret |= Menu::BTN_OK;
        }else if (cdc == 0x1b){ // escape sequence (or escape)
            char cdc2, cdc3;
            if (queue_try_remove(&usbCDCinputQueue, &cdc2) && queue_try_remove(&usbCDCinputQueue, &cdc3)){
                if (cdc2 == 0x5b){
                    if (cdc3 == 'A'){
                        ret |= Menu::BTN_UP;
                    }else if (cdc3 == 'B'){
                        ret |= Menu::BTN_DOWN;
                    }else if (cdc3 == 'C'){
                        ret |= Menu::BTN_RIGHT;
                    }else if (cdc3 == 'D'){
                        ret |= Menu::BTN_LEFT;
                    }
                }
            }
        }
    }
    uint8_t hwIr = filterReadIR();
    if (hwIr & (1 << 2)){
        ret |= Menu::BTN_LEFT;
    }
    if (hwIr & (1 << 1)){
        ret |= Menu::BTN_RIGHT;
    }
    if (hwIr & (1 << 0)){
        ret |= Menu::BTN_OK;
    }
    return ret;
}

uint32_t commonMenuRead(){
    uint32_t ret = 0;
    char evt;
    if (queue_try_remove(&usbHIDEvtQueue, &evt)){
        if (evt == EVT_NUMLOCK){
            ret |= Menu::BTN_DOWN;
        }else if (evt == EVT_CAPSLOCK){
            ret |= Menu::BTN_OK;
        }else if (evt == EVT_SCROLLLOCK){
            ret |= Menu::BTN_UP;
        }
    }
    char cdc;
    if (queue_try_remove(&usbCDCinputQueue, &cdc)){
        if (cdc == '\n' || cdc == '\r'){
            ret |= Menu::BTN_OK;
        }else if (cdc == 0x1b){ // escape sequence (or escape)
            char cdc2, cdc3;
            if (queue_try_remove(&usbCDCinputQueue, &cdc2) && queue_try_remove(&usbCDCinputQueue, &cdc3)){
                if (cdc2 == 0x5b){
                    if (cdc3 == 'A'){
                        ret |= Menu::BTN_UP;
                    }else if (cdc3 == 'B'){
                        ret |= Menu::BTN_DOWN;
                    }else if (cdc3 == 'C'){
                        ret |= Menu::BTN_RIGHT;
                    }else if (cdc3 == 'D'){
                        ret |= Menu::BTN_LEFT;
                    }
                }
            }
        }
    }
    uint8_t hwIr = filterReadIR();
    if (hwIr & (1 << 2)){
        ret |= Menu::BTN_UP;
    }
    if (hwIr & (1 << 1)){
        ret |= Menu::BTN_DOWN;
    }
    if (hwIr & (1 << 0)){
        ret |= Menu::BTN_OK;
    }
    return ret;
}

uint32_t commonConfirm(){
    uint32_t ret = 0;
    char evt;
    if (queue_try_remove(&usbHIDEvtQueue, &evt)){
        if (evt == EVT_NUMLOCK){
            ret |= Menu::BTN_RIGHT; // TODO: toggle direction if unavailable
        }else if (evt == EVT_CAPSLOCK){
            ret |= Menu::BTN_OK;
        }else if (evt == EVT_SCROLLLOCK){
            ret |= Menu::BTN_LEFT;
        }
    }
    char cdc;
    if (queue_try_remove(&usbCDCinputQueue, &cdc)){
        if (cdc == '\n' || cdc == '\r'){
            ret |= Menu::BTN_OK;
        }else if (cdc == 0x1b){ // escape sequence (or escape)
            char cdc2, cdc3;
            if (queue_try_remove(&usbCDCinputQueue, &cdc2) && queue_try_remove(&usbCDCinputQueue, &cdc3)){
                if (cdc2 == 0x5b){
                    if (cdc3 == 'A'){
                        ret |= Menu::BTN_UP;
                    }else if (cdc3 == 'B'){
                        ret |= Menu::BTN_DOWN;
                    }else if (cdc3 == 'C'){
                        ret |= Menu::BTN_RIGHT;
                    }else if (cdc3 == 'D'){
                        ret |= Menu::BTN_LEFT;
                    }
                }
            }
        }
    }
    uint8_t hwIr = filterReadIR();
    if (hwIr & (1 << 2)){
        ret |= Menu::BTN_LEFT;
    }
    if (hwIr & (1 << 1)){
        ret |= Menu::BTN_RIGHT;
    }
    if (hwIr & (1 << 0)){
        ret |= Menu::BTN_OK;
    }
    return ret;
}

uint32_t phyConfirm(){
    static uint8_t keepCheck = 0;
    uint32_t ret = 0;
    char evt;
    if (queue_try_remove(&usbHIDEvtQueue, &evt)){
        if (evt == EVT_NUMLOCK){
            ret |= Menu::BTN_RIGHT;
        }else if (evt == EVT_SCROLLLOCK){
            ret |= Menu::BTN_LEFT;
        }
    }
    char cdc;
    while (queue_try_remove(&usbCDCinputQueue, &cdc));

    uint8_t hwIr = filterReadIR();
    if (hwIr & (1 << 2)){
        ret |= Menu::BTN_RIGHT;
    }
    if (hwIr & (1 << 1)){
        ret |= Menu::BTN_LEFT;
    }
    if (hwIr & (1 << 0)){
        ret |= Menu::BTN_OK;
    }
    return ret;
}

void subMenuPass(int id){
    Pers::PassEntry entry;
    if (!Pers::get().readEntry(id, entry)) return;
    std::vector<std::string> menuList = {
        "Nome:" + std::string(entry.name),
        "UN:" + std::string(entry.username),
        "UserName+TAB+Senha+Enter",
        "Username",
        "Senha+Enter",
        "Senha",
        "Sair",
        "Apagar"};
    int idx = 2;
    while (true){
        idx = Menu::genericList(display, commonMenuRead, menuList, idx);
        if (idx == 0 || idx == 1){
            // no action
        }else if (idx == 2){
            if (Menu::confirm(display, phyConfirm, {"! Confirmar digitar", "UserName+TAB+", "+Senha+Enter"})){
                sendHID(entry.username);
                sendHID("\t");
                sendHID(entry.pass);
                sendHID("\n");
            }
        }else if (idx == 3){
            if (Menu::confirm(display, phyConfirm, {"! Confirmar digitar", "UserName"})){
                sendHID(entry.username);
            }
        }else if (idx == 4){
            if (Menu::confirm(display, phyConfirm, {"! Confirmar digitar", "Senha+ENTER"})){
                sendHID(entry.pass);
                sendHID("\n");
            }
        }else if (idx == 5){
            if (Menu::confirm(display, phyConfirm, {"! Confirmar digitar", "Senha"})){
                sendHID(entry.pass);
            }
        }else if (idx == 7){
            if (Menu::confirm(display, phyConfirm, {"! Apagar mesmo?", entry.name})){
                if (Menu::confirm(display, phyConfirm, {"! Mesmo mesmo?", entry.name})){
                    if (Pers::get().eraseEntry(id)){
                        return;
                    }
                    else{
                        display.clear();
                        writer.print(32, 10, "Algo deu errado");
                        display.display();
                        sleep_ms(1000); // TODO!
                    }
                }
            }
        }else{
            return;
        }
    }
}

bool submenuGerarSenha(char *senha){
    int idx = 0;
    int passlen = 16, minLowercase = 1, minUppercase = 1, minNumeric = 1, minSymbol = 1;
    bool useLowercase = true, useUppercase = true, useNumeric = true, useSymbol = true;
    char tempStr[32];
    const std::string lowercaseList = "abcdefghijklmnopqrstuvwxyz";
    const std::string uppercaseList = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::string numberList = "0123456789";
    const std::string symbolList = "@.,-_+=*:;'\"!?#$%¨&/\\|()<>{}[]";

    std::vector<std::string> menuList = {
        "Comprimento: 16",
        "Usar Min.: sim", "Min. Min.: 1",
        "Usar Mai.: sim", "Min. Mai.: 1",
        "Usar Num.: sim", "Min. Num.: 1",
        "Usar Simb.: sim", "Min. Simb.: 1",
        "Cancelar", "Gerar"};
    while (true){
        idx = Menu::genericList(display, commonMenuRead, menuList, idx);
        if (idx == 0){
            passlen = Menu::intRead(display, commonMenuRead, passlen, 4, 64, "Comprimento");
            sprintf(tempStr, "Comprimento: %d", passlen);
            menuList[0] = tempStr;
        }else if (idx == 1){
            useLowercase = Menu::confirm(display, commonConfirm, {"", "Usar Minusculas?"});
            sprintf(tempStr, "Usar Min.: %s", useLowercase ? "sim" : "nao");
            menuList[1] = tempStr;
        }else if (idx == 2){
            minLowercase = Menu::intRead(display, commonMenuRead, minLowercase, 0, 64, "Min. Minuscula:");
            sprintf(tempStr, "Min. Min.: %d", minLowercase);
            menuList[2] = tempStr;
        }else if (idx == 3){
            useUppercase = Menu::confirm(display, commonConfirm, {"", "Usar Maiuscula?"});
            sprintf(tempStr, "Usar Mai.: %s", useUppercase ? "sim" : "nao");
            menuList[3] = tempStr;
        }else if (idx == 4){
            minUppercase = Menu::intRead(display, commonMenuRead, minUppercase, 0, 64, "Min. Maiuscula:");
            sprintf(tempStr, "Min. Mai.: %d", minUppercase);
            menuList[4] = tempStr;
        }else if (idx == 5){
            useNumeric = Menu::confirm(display, commonConfirm, {"", "Usar Numeros?"});
            sprintf(tempStr, "Usar Num.: %s", useNumeric ? "sim" : "nao");
            menuList[5] = tempStr;
        }else if (idx == 6){
            minNumeric = Menu::intRead(display, commonMenuRead, minNumeric, 0, 64, "Min. Numeros:");
            sprintf(tempStr, "Min. Num.: %d", minNumeric);
            menuList[6] = tempStr;
        }else if (idx == 7){
            useSymbol = Menu::confirm(display, commonConfirm, {"", "Usar Simbolos?"});
            sprintf(tempStr, "Usar Simb.: %s", useSymbol ? "sim" : "nao");
            menuList[7] = tempStr;
        }else if (idx == 8){
            minSymbol = Menu::intRead(display, commonMenuRead, minSymbol, 0, 64, "Min. Simbolos:");
            sprintf(tempStr, "Min. Simb.: %d", minSymbol);
            menuList[8] = tempStr;
        }else if (idx == 10){
            int mincount = 0;
            if (useLowercase) mincount += minLowercase;
            if (useUppercase) mincount += minUppercase;
            if (useNumeric) mincount += minNumeric;
            if (useSymbol) mincount += minSymbol;
            if (mincount > passlen){
                display.clear();
                writer.print(0, 0, "Erro: contagem");
                writer.print(0, 10, " minima alta");
                writer.print(0, 20, "   demais");
                display.display();
                sleep_ms(1000); // TODO
            }
            for (int i = 0; i <= passlen; i++) senha[i] = 0;
            int emptyChars = passlen;
            std::string fullList = "";
            if (useLowercase){
                for (int i = 0; i < minLowercase; i++, emptyChars--){
                    char v = lowercaseList[rand() % lowercaseList.size()];
                    int pr = rand() % emptyChars, pv = 0;
                    while (pr > 0 && senha[pv]){
                        if (senha[pv] == 0)
                            pr--;
                        pv++;
                    }
                    senha[pv] = v;
                }
                fullList += lowercaseList;
            }
            if (useUppercase){
                for (int i = 0; i < minUppercase; i++, emptyChars--){
                    char v = uppercaseList[rand() % uppercaseList.size()];
                    int pr = rand() % emptyChars, pv = 0;
                    while (pr > 0 && senha[pv]){
                        if (senha[pv] == 0)
                            pr--;
                        pv++;
                    }
                    senha[pv] = v;
                }
                fullList += uppercaseList;
            }
            if (useNumeric){
                for (int i = 0; i < minNumeric; i++, emptyChars--){
                    char v = numberList[rand() % numberList.size()];
                    int pr = rand() % emptyChars, pv = 0;
                    while (pr > 0 && senha[pv]){
                        if (senha[pv] == 0)
                            pr--;
                        pv++;
                    }
                    senha[pv] = v;
                }
                fullList += numberList;
            }
            if (useSymbol){
                for (int i = 0; i < minSymbol; i++, emptyChars--){
                    char v = symbolList[rand() % symbolList.size()];
                    int pr = rand() % emptyChars, pv = 0;
                    while (pr > 0 && senha[pv]){
                        if (senha[pv] == 0)
                            pr--;
                        pv++;
                    }
                    senha[pv] = v;
                }
                fullList += symbolList;
            }
            for (int i = 0; i < passlen; i++){
                if (senha[i]) continue;
                senha[i] = fullList[rand() % fullList.size()];
            }
            return true;
        }else{
            return false;
        }
    }
}

void subMenuAdicionar(){
    int idx = 0;
    Pers::PassEntry entry;
    char tempStr[150];
    std::vector<std::string> menuList = {
        "Nome: ", "UN: ", "Senha: ", "Gerar Senha", "Testar UN", "Testar Senha", "Cancelar", "Adicionar"};
    entry.name[0] = 0;
    entry.username[0] = 0;
    entry.pass[0] = 0;
    while (true){
        idx = Menu::genericList(display, commonMenuRead, menuList, idx);
        if (idx == 0){
            strcpy(tempStr, entry.name);
            if (Menu::getString(display, commonKeyboardRead, tempStr, 32)){
                strcpy(entry.name, tempStr);
                sprintf(tempStr, "Nome: %s", entry.name);
                menuList[0] = tempStr;
            }
        }else if (idx == 1){
            strcpy(tempStr, entry.username);
            if (Menu::getString(display, commonKeyboardRead, tempStr, 128)){
                strcpy(entry.username, tempStr);
                sprintf(tempStr, "UN: %s", entry.username);
                menuList[1] = tempStr;
            }
        }else if (idx == 2){
            strcpy(tempStr, entry.pass);
            if (Menu::getString(display, commonKeyboardRead, tempStr, 64)){
                strcpy(entry.pass, tempStr);
                sprintf(tempStr, "Senha: %s", entry.pass);
                menuList[2] = tempStr;
            }
        }else if (idx == 3){
            if (submenuGerarSenha(tempStr)){
                strcpy(entry.pass, tempStr);
                sprintf(tempStr, "Senha: %s", entry.pass);
                menuList[2] = tempStr;
            }
        }else if (idx == 4){
            if (Menu::confirm(display, phyConfirm, {"! Confirmar digitar", "UserName"})){
                sendHID(entry.username);
            }
        }else if (idx == 5){
            if (Menu::confirm(display, phyConfirm, {"! Confirmar digitar", "Senha"})){
                sendHID(entry.pass);
            }
        }else if (idx == 7){
            if (Menu::confirm(display, phyConfirm, {"! Adicionar mesmo?", entry.name})){
                if (!Pers::get().appendEntry(entry)){
                    display.clear();
                    writer.print(32, 10, "Erro adicionando");
                    display.display();
                    sleep_ms(1000);
                }
                return;
            }
        }else{
            return;
        }
    }
}

#define OPERATION_GET_COUNT (1)
#define OPERATION_GET_NAME  (2)
#define OPERATION_GET_UN    (3)
#define OPERATION_GET_PW    (4)
#define OPERATION_ERASE     (5)
#define OPERATION_SET_NAME  (6)
#define OPERATION_SET_UN    (7)
#define OPERATION_SET_PW    (8)
#define OPERATION_APPEND    (9)
#define OPERATION_END       (10)

void passiveMode(){
    std::string tmpName, tmpUn, tmpPw;
    int cogCounter = 0;
    bool running = true;
    pauseUsbHandling = 1;
    xceiver.enable();
    turnOffPWM();

    while(running){
        xceiver.idle();
        if(xceiver.available()){
            std::vector<uint8_t> data;
            xceiver.rx(data);
            if(data.size()==1) {
                if(data[0]==OPERATION_GET_COUNT) {
                    xceiver.tx({(uint8_t)Pers::get().count()});

                } else if(data[0]==OPERATION_APPEND) {
                    if(tmpName.size()>0 && tmpUn.size()>0 && tmpPw.size()>0){
                        Pers::PassEntry entry;
                        strcpy(entry.name, tmpName.c_str());
                        strcpy(entry.username, tmpUn.c_str());
                        strcpy(entry.pass, tmpPw.c_str());
                        if(Pers::get().appendEntry(entry)){
                            xceiver.tx({(uint8_t)Pers::get().count()});
                        } else {
                            xceiver.tx({});
                        }
                    } else {
                        xceiver.tx({});
                    }
                    tmpName = "";
                    tmpUn = "";
                    tmpPw = "";
                }
                if(data[0]==OPERATION_END) {
                    xceiver.tx({1});
                    running = false;
                    for(int i=0; i<10; i++){
                        xceiver.idle();
                        sleep_ms(10);
                    }
                } else {
                    xceiver.tx({});
                }
            } else if(data.size()==2) {
                Pers::PassEntry entry;
                if(data[0]==OPERATION_GET_NAME) {
                    if(Pers::get().readEntry(data[1], entry)){
                        xceiver.tx(std::vector<uint8_t>(entry.name, entry.name+strlen(entry.name)+1));
                    } else {
                        xceiver.tx({});
                    }

                } else if(data[0]==OPERATION_GET_UN) {
                    if(Pers::get().readEntry(data[1], entry)){
                        xceiver.tx(std::vector<uint8_t>(entry.username, entry.username+strlen(entry.username)+1));
                    } else {
                        xceiver.tx({});
                    }

                } else if(data[0]==OPERATION_GET_PW) {
                    if(Pers::get().readEntry(data[1], entry)){
                        xceiver.tx(std::vector<uint8_t>(entry.pass, entry.pass+strlen(entry.pass)+1));
                    } else {
                        xceiver.tx({});
                    }

                } else if(data[0]==OPERATION_ERASE) {
                    if(Pers::get().eraseEntry(data[1])){
                        xceiver.tx({(uint8_t)Pers::get().count()});
                    } else {
                        xceiver.tx({});
                    }
                } else {
                    xceiver.tx({});
                }
            } else {
                if(data[0]==OPERATION_SET_NAME) {
                    if(data.size()<=33){
                        char tmp[33];
                        memcpy(tmp, data.data()+1, data.size()-1);
                        tmp[32] = 0;
                        tmpName = tmp;
                    } else {
                        tmpName = "";
                        xceiver.tx({});
                    }

                } else if(data[0]==OPERATION_SET_UN) {
                    if(data.size()<=129){
                        char tmp[129];
                        memcpy(tmp, data.data()+1, data.size()-1);
                        tmp[128] = 0;
                        tmpUn = tmp;
                    } else {
                        tmpUn = "";
                        xceiver.tx({});
                    }

                } else if(data[0]==OPERATION_SET_PW) {
                    if(data.size()<=65){
                        char tmp[65];
                        memcpy(tmp, data.data()+1, data.size()-1);
                        tmp[64] = 0;
                        tmpPw = tmp;
                    } else {
                        tmpPw = "";
                        xceiver.tx({});
                    }

                } else {
                    xceiver.tx({});
                }
            }
        } else {
            display.clear();
            xceiver.idle();
            writer.putch(30+sin((cogCounter+0)*2*3.14/16)*12,12+cos((cogCounter+0)*2*3.14/16)*12,'o');
            xceiver.idle();
            writer.putch(30+sin((cogCounter+4)*2*3.14/16)*12,12+cos((cogCounter+4)*2*3.14/16)*12,'o');
            xceiver.idle();
            writer.putch(30+sin((cogCounter+8)*2*3.14/16)*12,12+cos((cogCounter+8)*2*3.14/16)*12,'o');
            xceiver.idle();
            writer.putch(30+sin((cogCounter+12)*2*3.14/16)*12,12+cos((cogCounter+12)*2*3.14/16)*12,'o');
            cogCounter = (cogCounter+1)%16;
            xceiver.idle();
            display.display();
            xceiver.idle();
        }
    }
    xceiver.disable();
    pauseUsbHandling = 0;
}

void activeMode() {
    // just doing dumb copy for now, as I need to redesign the board ASAP
    while(Pers::get().count()>0)Pers::get().eraseEntry(0);

    // std::string tmpName, tmpUn, tmpPw;
    // int cogCounter = 0;
    // bool running = true;
    pauseUsbHandling = 1;
    xceiver.enable();
    turnOffPWM();

    std::string failReason;
    uint64_t end;
    int entryCount = 0;
    std::vector<uint8_t> rawResponse;
    xceiver.tx({OPERATION_GET_COUNT});
    end = micros()+1000000;
    while(xceiver.available()==0){
        if(micros()>end) {
            failReason = "Sem comunicacao";
            goto exitActiveMode;
        }
        xceiver.idle();
    }
    xceiver.rx(rawResponse);
    entryCount = rawResponse[0];

    display.clear();
    writer.print(0, 10, "entradas: %d", entryCount);
    display.display();

    for(uint8_t i=0; i<entryCount; i++){
        Pers::PassEntry entry;
        memset(&entry, 0, sizeof(entry));

        writer.print(0, 20, "   %d", i);
        display.display();

        xceiver.tx({OPERATION_GET_NAME, i});
        end = micros()+1000000;
        while(xceiver.available()==0){
            if(micros()>end) {
                failReason = "falha nome";
                goto exitActiveMode;
            }
            xceiver.idle();
        }
        xceiver.rx(rawResponse);
        memcpy(entry.name, &rawResponse[0], rawResponse.size());

        writer.print(0, 20, "N");
        display.display();

        xceiver.tx({OPERATION_GET_UN, i});
        end = micros()+1000000;
        while(xceiver.available()==0){
            if(micros()>end) {
                failReason = "falha un";
                goto exitActiveMode;
            }
            xceiver.idle();
        }
        xceiver.rx(rawResponse);
        memcpy(entry.username, &rawResponse[0], rawResponse.size());

        writer.print(0, 20, "U");
        display.display();

        xceiver.tx({OPERATION_GET_PW, i});
        end = micros()+1000000;
        while(xceiver.available()==0){
            if(micros()>end) {
                failReason = "falha pw";
                goto exitActiveMode;
            }
            xceiver.idle();
        }
        xceiver.rx(rawResponse);
        memcpy(entry.pass, &rawResponse[0], rawResponse.size());

        writer.print(0, 20, "P");
        display.display();

        if(!Pers::get().appendEntry(entry)){
            failReason = "falha adicionar";
            goto exitActiveMode;
        }
    }

    display.clear();
    writer.print(0, 10, "Importado %d", entryCount);
    writer.print(0, 20, "  entradas");
    display.display();
    sleep_ms(1000);
    xceiver.disable();
    pauseUsbHandling = 0;
    return;

exitActiveMode:
    display.clear();
    writer.print(0, 10, failReason.c_str());
    display.display();
    sleep_ms(1000);
    xceiver.disable();
    pauseUsbHandling = 0;
}

void subMenuExtras(){
    std::vector<std::string> menuList = {
        "Voltar",
        "Mudar PIN",
        "FW Update",
        "Despejar dados",
        "Modo Passivo",
        "Modo Ativo",
    };
    while (true){
        int idx = Menu::genericList(display, commonMenuRead, menuList, 0);
        if (idx == 1){
            Pers::MainData data;
            Pers::get().readMain(data);
            Menu::pinReader(display, commonConfirm, data.pin, 4);
            std::string npinstr(4, '0');
            for(int i=0; i<4; i++)npinstr[i] = '0'+data.pin[i];
            if (Menu::confirm(display, phyConfirm, {"! Salvar", "PIN novo?", npinstr})){
                display.clear();
                writer.print(0, 10, "Gravando");
                display.display();
                Pers::get().replaceMain(data);
                display.clear();
                writer.print(16, 10, "Feito!");
                display.display();
                sleep_ms(500);
            }
        }else if (idx == 2){
            //if (Menu::confirm(display, phyConfirm, {"! Trocar mesmo", "o firmware?"})){
            //    if (!Menu::confirm(display, phyConfirm, {"! Nao tem como", " convencer", "contrario?"})){
                    display.clear();
                    writer.print(16, 0, "Update de");
                    writer.print(16, 10, "Firmware");
                    writer.print(16, 20, "Ativo");
                    display.display();
                    reset_usb_boot(1 << ledPins[2], 0);
            //    }
            //}
        }else if (idx == 3){
            if (Menu::confirm(display, phyConfirm, {"! Despejar mesmo", "todos os dados?"})){
                if (!Menu::confirm(display, phyConfirm, {"! Nao tem como", "convencer", "contrario?"})){
                    Pers::PassEntry entry;
                    display.clear();
                    writer.print(32, 0, "Enviando");
                    writer.print(32, 10, "Dados...");
                    display.display();
                    for (int i = 0; i < Pers::get().count(); i++){
                        if (Pers::get().readEntry(i, entry)){
                            sendHID(entry.name);
                            sendHID("\n");
                            sendHID(entry.username);
                            sendHID("\n");
                            sendHID(entry.pass);
                            sendHID("\n\n");
                            sleep_ms(500);
                        }
                    }
                }
            }
        }else if (idx == 4){
            //if (Menu::confirm(display, phyConfirm, {"! Entrar modo", "  passivo?"})){
                passiveMode();
            //}
        }else if (idx == 5){
            //if (Menu::confirm(display, phyConfirm, {"! Entrar modo", "  ativo?"})){
                activeMode();
            //}
        }else{
            return;
        }
    }
}
void mainLoop(){
    static int idx = 1;{
        std::vector<std::string> menuList;
        menuList.push_back("Func. Extras");
        menuList.push_back("Adicionar novo");
        for (int i = 0; i < Pers::get().count(); i++){
            Pers::PassEntry entry;
            Pers::get().readEntry(i, entry);
            menuList.push_back(entry.name);
        }
        idx = Menu::genericList(display, commonMenuRead, menuList);
    }
    if (idx == 0){
        if (Menu::confirm(display, phyConfirm, {"! Entrar mesmo", "em extras?"})){
            subMenuExtras();
        }else{
            idx = 1;
        }
    }
    else if (idx == 1){
        subMenuAdicionar();
    }
    else{
        subMenuPass(idx - 2);
    }
}

// Doing everything USB-related in second core
void core1_entry(){
    multicore_lockout_victim_init();
    tusb_init();

    const uint32_t hid_interval_ms = 10;
    uint32_t hid_next_ms = 0;

    while (1){
        if(pauseUsbHandling){
            reset_block(RESETS_RESET_USBCTRL_BITS);
            while(pauseUsbHandling) sleep_ms(100);
            tusb_init();
        }
        tud_task();

        if (tud_hid_ready()){
            if (board_millis() < hid_next_ms)continue;
            hid_next_ms = board_millis() + hid_interval_ms;

            // HID handling ----------------------------------------
            char chr;
            if (queue_try_remove(&usbHIDKeyQueue, &chr)){
                uint8_t keycode[6] = {0};
                uint8_t modifier = 0;
                if (ascii2keycode[chr][0]) modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
                keycode[0] = ascii2keycode[chr][1];
                static char lastchr = 0;
                if(chr==lastchr){
                    while(!tud_hid_keyboard_report(0, modifier, NULL)) tud_task();
                }
                while(!tud_hid_keyboard_report(0, modifier, keycode)) tud_task();
                lastchr = chr;
            }else{
                tud_hid_keyboard_report(0, 0, NULL);
            }
        }
        // CDC handling -----------------------------------------
        if (tud_cdc_available()){
            // respond all input data
            char buf[64];
            int count = tud_cdc_read(buf, sizeof(buf));
            for (int i = 0; i < count; i++){
                if (!queue_try_add(&usbCDCinputQueue, buf + i)) break;
            }
        }
    }
}

void shuffleSeed(uint32_t *seed){
    uint16_t dark[IR_CNT], bright[IR_CNT];
    readRawAnalogInputs(dark, bright);
    for (int i = 0; i < IR_CNT; i++){
        *seed += dark[i] << (i * 8);
        *seed += bright[i] << (i * 8 + IR_CNT);
    }
}

int main(void){
    uint32_t seed = 0xA0A0A0A0;
    board_init();
    adc_init();
    gpio_init(PIN_XCEIVER_RX);
    gpio_set_dir(PIN_XCEIVER_RX, GPIO_IN);
    gpio_set_pulls(PIN_XCEIVER_RX, false, false);

    gpio_init(PIN_XCEIVER_TX);
    gpio_set_dir(PIN_XCEIVER_TX, GPIO_IN);
    gpio_set_pulls(PIN_XCEIVER_TX, false, false);

    gpio_init(PIN_SELECT_IR);
    gpio_set_dir(PIN_SELECT_IR, GPIO_OUT);
    gpio_put(PIN_SELECT_IR, true);
    gpio_set_pulls(PIN_SELECT_IR, false, false);

    gpio_init(PIN_SELECT_LED);
    gpio_set_dir(PIN_SELECT_LED, GPIO_OUT);
    gpio_put(PIN_SELECT_LED, false);
    gpio_set_pulls(PIN_SELECT_LED, false, false);

    shuffleSeed(&seed);

    for (int i = 0; i < IR_CNT; i++){
        gpio_init(ledPins[i]);
        gpio_set_dir(ledPins[i], GPIO_OUT);
        gpio_put(ledPins[i], false);
        gpio_set_pulls(ledPins[i], false, false);
        adc_gpio_init(adcPins[i]);
    }

    gpio_put(ledPins[0], true);

    xceiver.init();
    pauseUsbHandling = 0;

    queue_init(&usbHIDKeyQueue, 1, 512);
    queue_init(&usbHIDEvtQueue, 1, 16);
    queue_init(&usbCDCinputQueue, 1, 64);
    multicore_launch_core1(core1_entry);

    shuffleSeed(&seed);

    gpio_put(ledPins[1], true);

    if (!display.init(PIN_DISPLAY_RESET, PIN_DISPLAY_SCL, PIN_DISPLAY_SDA, 0)){
        for (int i = 0; i < 15; i++){
            gpio_put(ledPins[2], true);
            sleep_ms(300);
            gpio_put(ledPins[2], false);
            sleep_ms(300);
        }
    };

    shuffleSeed(&seed);
    gpio_put(ledPins[2], true);

    display.clear();
    writer.print(0, 0, "LCD ok");
    writer.print(0, 10, "Inicializando");
    writer.print(0, 20, "  Memoria... ");
    if (!display.display()){
        for (int i = 0; i < 15; i++){
            gpio_put(ledPins[2], true);
            sleep_ms(100);
            gpio_put(ledPins[2], false);
            sleep_ms(100);
        }
    }
    shuffleSeed(&seed);
    Pers::get();
    seed += Pers::get().getCombinedId();

    shuffleSeed(&seed);
    uint offset = pio_add_program(pio0, &pwm_program);
    for (int i = 0; i < IR_CNT; i++){
        pwm_program_init(pio0, i, offset, ledPins[i]);
        pio_pwm_set_period(pio0, i, 64);
    }

    shuffleSeed(&seed);
    srand(seed);

    Pers::MainData data;
    if(!Pers::get().readMain(data)){
        display.clear();
        writer.print(0, 0, "ERROR");
        writer.print(0, 10, "reading");
        writer.print(0, 20, "pin data");
        display.display();
        memset(&data, 0, sizeof(data));
        Pers::get().replaceMain(data);
        display.clear();
        writer.print(0, 0, "Data");
        writer.print(0, 10, "sobrescrita");
        display.display();
    }
    uint8_t zeros[4] = {0,0,0,0};
    if(memcmp(zeros, data.pin, 4)){
        uint8_t code[4];
        do{
            for(int i=0; i<4; i++) code[i] = rand()%10;
            Menu::pinReader(display, commonConfirm, code, 4);
            for(int i=0; i<3; i++){
                display.clear();
                writer.print(10+i*20, 30, "*");
                display.display();
                sleep_ms(300);
            }
        }while(memcmp(code, data.pin, 4));
    }

#ifdef OLD_LED_INIT_AND_CHECK
    display.clear();
    writer.print(0, 0, "LCD ok, MEM ok");
    writer.print(0, 10, "Acione todas as");
    writer.print(0, 20, "entradas");
    display.display();{
        uint32_t seed = 0xA0A0A0A0 + Pers::get().getCombinedId();
        uint8_t darkCheck = 0, brightCheck = 0, fullCheck = (1 << IR_CNT) - 1;
        while (1){
            uint16_t dark[IR_CNT], bright[IR_CNT];
            readRawAnalogInputs(dark, bright);
            uint8_t current = interpretRawIR(dark, bright);
            setLeds(current);

            for (int i = 0; i < IR_CNT; i++){
                seed += dark[i] << (i * 8);
                seed += bright[i] << (i * 8 + IR_CNT);
            }

            darkCheck |= (~current) & fullCheck;
            brightCheck |= (current)&fullCheck;
            if (darkCheck == fullCheck && brightCheck == fullCheck && current == 0) break;
            sleep_ms(10);
        }

        srand(seed);
    }
#endif

    while (1){
        mainLoop();
        rand();
    }
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize){
    (void)report_id;
    if (instance == ITF_NUM_KEYBOARD){
        if (report_type == HID_REPORT_TYPE_OUTPUT){
            if (bufsize < 1) return;
            bool caps = (buffer[0] & KEYBOARD_LED_CAPSLOCK);
            bool num = (buffer[0] & KEYBOARD_LED_NUMLOCK);
            bool scroll = (buffer[0] & KEYBOARD_LED_SCROLLLOCK);
            if (!lastKeysInited){
                lastKeysInited = true;
                lastCaps = caps;
                lastNum = num;
                lastScroll = scroll;
            }
            if (num != lastNum){
                char evt = EVT_NUMLOCK;
                queue_try_add(&usbHIDEvtQueue, &evt);
            }
            if (caps != lastCaps){
                char evt = EVT_CAPSLOCK;
                queue_try_add(&usbHIDEvtQueue, &evt);
            }
            if (scroll != lastScroll){
                char evt = EVT_SCROLLLOCK;
                queue_try_add(&usbHIDEvtQueue, &evt);
            }
            lastCaps = caps;
            lastNum = num;
            lastScroll = scroll;
        }
    }
}