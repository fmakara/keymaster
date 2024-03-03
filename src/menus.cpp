#include <string>
#include <vector>
#include <stdint.h>

#include "generic_macros.h"
#include "display/ssd1306.h"
#include "sprite/dict8.h"
#include "sprite/menu.h"
#include "pers/persistency.h"
#include "inputs/inputs.h"
#include "keyboard_hid/keyboard_hid_handler.h"

void rebootAndFirmwareUpdate();
extern SSD1306_72x40 display;
extern Dict8 writer;
extern Inputs hardInputs;
extern KeyboardHIDHandler keyboard;

uint32_t commonKeyboardRead(){
    uint32_t ret = 0;
    switch(keyboard.getKeyboardEvent()){
        case KeyboardHIDHandler::NUMLOCK: ret |= Menu::BTN_LEFT; break;
        case KeyboardHIDHandler::CAPSLOCK: ret |= Menu::BTN_OK; break;
        case KeyboardHIDHandler::SCROLLLOCK: ret |= Menu::BTN_RIGHT; break;
    }
    uint32_t cdc = keyboard.getSerialKeypress();
    if(cdc){
        if(cdc >= ' ' && cdc <= '~'){
            ret |= Menu::BTN_CHR | cdc;
        } else {
            switch(cdc){
                case 0x7F: ret |= Menu::BTN_CHR | Menu::CHR_BACKSPACE; break;
                case '\n':
                case '\r': ret |= Menu::BTN_OK; break;
                case KeyboardHIDHandler::DIRECTION_UP: ret |= Menu::BTN_UP; break;
                case KeyboardHIDHandler::DIRECTION_DOWN: ret |= Menu::BTN_DOWN; break;
                case KeyboardHIDHandler::DIRECTION_RIGHT: ret |= Menu::BTN_RIGHT; break;
                case KeyboardHIDHandler::DIRECTION_LEFT: ret |= Menu::BTN_LEFT; break;
            }
        }
    }
    hardInputs.process();
    if(hardInputs.wasLeftReleased()) ret |= Menu::BTN_LEFT;
    if(hardInputs.wasRightReleased()) ret |= Menu::BTN_RIGHT;
    if(hardInputs.wasOkReleased()) ret |= Menu::BTN_OK;
    return ret;
}

uint32_t commonMenuRead(){
    uint32_t ret = 0;
    switch(keyboard.getKeyboardEvent()){
        case KeyboardHIDHandler::NUMLOCK: ret |= Menu::BTN_DOWN; break;
        case KeyboardHIDHandler::CAPSLOCK: ret |= Menu::BTN_OK; break;
        case KeyboardHIDHandler::SCROLLLOCK: ret |= Menu::BTN_UP; break;
    }
    uint32_t cdc = keyboard.getSerialKeypress();
    if(cdc){
        switch(cdc){
            case '\n':
            case '\r': ret |= Menu::BTN_OK; break;
            case KeyboardHIDHandler::DIRECTION_UP: ret |= Menu::BTN_UP; break;
            case KeyboardHIDHandler::DIRECTION_DOWN: ret |= Menu::BTN_DOWN; break;
            case KeyboardHIDHandler::DIRECTION_RIGHT: ret |= Menu::BTN_RIGHT; break;
            case KeyboardHIDHandler::DIRECTION_LEFT: ret |= Menu::BTN_LEFT; break;
        }
    }
    hardInputs.process();
    if(hardInputs.wasLeftReleased()) ret |= Menu::BTN_UP;
    if(hardInputs.wasRightReleased()) ret |= Menu::BTN_DOWN;
    if(hardInputs.wasOkReleased()) ret |= Menu::BTN_OK;
    return ret;
}

uint32_t commonConfirm(){
    uint32_t ret = 0;
    switch(keyboard.getKeyboardEvent()){
        case KeyboardHIDHandler::NUMLOCK: ret |= Menu::BTN_RIGHT; break;
        case KeyboardHIDHandler::CAPSLOCK: ret |= Menu::BTN_OK; break;
        case KeyboardHIDHandler::SCROLLLOCK: ret |= Menu::BTN_LEFT; break;
    }
    uint32_t cdc = keyboard.getSerialKeypress();
    if(cdc){
        switch(cdc){
            case '\n':
            case '\r': ret |= Menu::BTN_OK; break;
            case KeyboardHIDHandler::DIRECTION_UP: ret |= Menu::BTN_UP; break;
            case KeyboardHIDHandler::DIRECTION_DOWN: ret |= Menu::BTN_DOWN; break;
            case KeyboardHIDHandler::DIRECTION_RIGHT: ret |= Menu::BTN_RIGHT; break;
            case KeyboardHIDHandler::DIRECTION_LEFT: ret |= Menu::BTN_LEFT; break;
        }
    }
    hardInputs.process();
    if(hardInputs.wasLeftReleased()) ret |= Menu::BTN_LEFT;
    if(hardInputs.wasRightReleased()) ret |= Menu::BTN_RIGHT;
    if(hardInputs.wasOkReleased()) ret |= Menu::BTN_OK;
    return ret;
}

uint32_t phyConfirm(){
    uint32_t ret = 0;
    switch(keyboard.getKeyboardEvent()){
        case KeyboardHIDHandler::NUMLOCK: ret |= Menu::BTN_RIGHT; break;
        case KeyboardHIDHandler::SCROLLLOCK: ret |= Menu::BTN_LEFT; break;
    }
    keyboard.ignoreSerialKeypress();
    hardInputs.process();
    if(hardInputs.wasLeftReleased()) ret |= Menu::BTN_LEFT;
    if(hardInputs.wasRightReleased()) ret |= Menu::BTN_RIGHT;
    if(hardInputs.wasOkReleased()) ret |= Menu::BTN_OK;
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
                keyboard.sendHID(entry.username);
                keyboard.sendHID("\t");
                keyboard.sendHID(entry.pass);
                keyboard.sendHID("\n");
            }
        }else if (idx == 3){
            if (Menu::confirm(display, phyConfirm, {"! Confirmar digitar", "UserName"})){
                keyboard.sendHID(entry.username);
            }
        }else if (idx == 4){
            if (Menu::confirm(display, phyConfirm, {"! Confirmar digitar", "Senha+ENTER"})){
                keyboard.sendHID(entry.pass);
                keyboard.sendHID("\n");
            }
        }else if (idx == 5){
            if (Menu::confirm(display, phyConfirm, {"! Confirmar digitar", "Senha"})){
                keyboard.sendHID(entry.pass);
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
                keyboard.sendHID(entry.username);
            }
        }else if (idx == 5){
            if (Menu::confirm(display, phyConfirm, {"! Confirmar digitar", "Senha"})){
                keyboard.sendHID(entry.pass);
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
                    rebootAndFirmwareUpdate();
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
                            keyboard.sendHID(entry.name);
                            keyboard.sendHID("\n");
                            keyboard.sendHID(entry.username);
                            keyboard.sendHID("\n");
                            keyboard.sendHID(entry.pass);
                            keyboard.sendHID("\n\n");
                            sleep_ms(500);
                        }
                    }
                }
            }
        }else if (idx == 4){
            //if (Menu::confirm(display, phyConfirm, {"! Entrar modo", "  passivo?"})){
                // passiveMode();
            //}
        }else if (idx == 5){
            //if (Menu::confirm(display, phyConfirm, {"! Entrar modo", "  ativo?"})){
                // activeMode();
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
