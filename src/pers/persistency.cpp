#include "persistency.h"

#include <stdlib.h>
#include <string.h>
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/regs/addressmap.h"
#include "pico/multicore.h"


#define FLASH_TARGET_OFFSET (1024 * 1024)

static uint8_t* kFlashBase = (uint8_t*) (XIP_BASE + FLASH_TARGET_OFFSET);
// total bytes: 2.097.152 - 262.144 = 1.835.008 (or 786.432 if using  half, 768 sectors)
// flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE); // 4096
// flash_range_program(FLASH_TARGET_OFFSET, random_data, FLASH_PAGE_SIZE); // 256

static const uint8_t kMainMagic[] = {73, 165, 59, 42, 14, 251, 182, 83, 20, 27, 67, 133, 186, 218, 129, 252};
static const uint32_t kEntryMagic = 78789659;

Pers::Pers(){
    {
        bool bad1 = memcmp(kFlashBase+0*FLASH_PAGE_SIZE, kMainMagic, sizeof(kMainMagic));
        bool bad2 = memcmp(kFlashBase+1*FLASH_PAGE_SIZE, kMainMagic, sizeof(kMainMagic));
        bool bad3 = memcmp(kFlashBase+2*FLASH_PAGE_SIZE, kMainMagic, sizeof(kMainMagic));
        bool bad4 = memcmp(kFlashBase+3*FLASH_PAGE_SIZE, kMainMagic, sizeof(kMainMagic));
        if( bad1 && bad2 && bad1 && bad4 ){
            multicore_lockout_start_blocking();
            uint32_t ints = save_and_disable_interrupts();
            flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE*256);
            {
                uint8_t base[FLASH_PAGE_SIZE];
                memset(base, 0xFF, FLASH_PAGE_SIZE);
                memcpy(base, kMainMagic, sizeof(kMainMagic));
                flash_range_program(FLASH_TARGET_OFFSET+0*FLASH_PAGE_SIZE, base, FLASH_PAGE_SIZE);
                flash_range_program(FLASH_TARGET_OFFSET+1*FLASH_PAGE_SIZE, base, FLASH_PAGE_SIZE);
                flash_range_program(FLASH_TARGET_OFFSET+2*FLASH_PAGE_SIZE, base, FLASH_PAGE_SIZE);
                flash_range_program(FLASH_TARGET_OFFSET+3*FLASH_PAGE_SIZE, base, FLASH_PAGE_SIZE);
            }
            {
                MainDataInner data;
                uint32_t *base = (uint32_t*)&data;
                memset(&data, 0, sizeof(MainDataInner));
                data.magic = kEntryMagic;
                data.checksum = 0;
                uint32_t crc = 0;
                for(int i=0; i<sizeof(MainDataInner)/4; i++) crc ^= base[i];
                data.checksum = crc;
                flash_range_program(FLASH_TARGET_OFFSET+0*FLASH_PAGE_SIZE+FLASH_SECTOR_SIZE, (uint8_t*)base, FLASH_PAGE_SIZE);
                flash_range_program(FLASH_TARGET_OFFSET+1*FLASH_PAGE_SIZE+FLASH_SECTOR_SIZE, (uint8_t*)base, FLASH_PAGE_SIZE);
                flash_range_program(FLASH_TARGET_OFFSET+2*FLASH_PAGE_SIZE+FLASH_SECTOR_SIZE, (uint8_t*)base, FLASH_PAGE_SIZE);
                flash_range_program(FLASH_TARGET_OFFSET+3*FLASH_PAGE_SIZE+FLASH_SECTOR_SIZE, (uint8_t*)base, FLASH_PAGE_SIZE);
            }
            restore_interrupts (ints);
            multicore_lockout_end_blocking();
        }
    }
    mainOffset = 0;
    for(int j=0; j<4; j++){
        if(memcmp(kFlashBase+FLASH_SECTOR_SIZE+j*FLASH_PAGE_SIZE, &kEntryMagic, sizeof(kEntryMagic))) continue;
        uint32_t crc = 0;
        uint32_t *base = (uint32_t*)(kFlashBase+FLASH_SECTOR_SIZE+j*FLASH_PAGE_SIZE);
        for(int k=0; k<FLASH_PAGE_SIZE/4; k++) crc ^= base[k];
        if(crc!=0) continue;
        mainOffset = FLASH_SECTOR_SIZE+j*FLASH_PAGE_SIZE;
        break;
    }
    for(int i=2; i<256; i++){
        for(int j=0; j<4; j++){
            if(memcmp(kFlashBase+i*FLASH_SECTOR_SIZE+j*FLASH_PAGE_SIZE, &kEntryMagic, sizeof(kEntryMagic))) continue;
            uint32_t crc = 0;
            uint32_t *base = (uint32_t*)(kFlashBase+i*FLASH_SECTOR_SIZE+j*FLASH_PAGE_SIZE);
            for(int k=0; k<FLASH_PAGE_SIZE/4; k++) crc ^= base[k];
            if(crc!=0) continue;
            offsets.push_back(i*FLASH_SECTOR_SIZE+j*FLASH_PAGE_SIZE);
            break;
        }
    }
    {
        multicore_lockout_start_blocking();
        uint32_t ints = save_and_disable_interrupts();
        flash_get_unique_id((uint8_t*)uid);
        restore_interrupts (ints);
        multicore_lockout_end_blocking();
    }
}

Pers::~Pers() {}

bool Pers::readMain(MainData& entry){
    if(mainOffset==0) return false;
    memcpy(&entry, mainOffset+kFlashBase+8, sizeof(MainData));
    return true;
}

bool Pers::replaceMain(const MainData& entry){
    MainDataInner data;
    data.entry = entry;
    data.magic = kEntryMagic;
    data.checksum = 0;
    {
        uint32_t crc = 0;
        uint32_t *base = (uint32_t*)&data;
        for(int i=0; i<sizeof(MainDataInner)/4; i++) crc ^= base[i];
        data.checksum = crc;
    }
    multicore_lockout_start_blocking();
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET+FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
    for(int j=0; j<4; j++){
        flash_range_program(FLASH_TARGET_OFFSET+FLASH_SECTOR_SIZE+j*FLASH_PAGE_SIZE, (uint8_t*)&data, FLASH_PAGE_SIZE);
    }
    restore_interrupts (ints);
    multicore_lockout_end_blocking();
    mainOffset = FLASH_SECTOR_SIZE;
    return true;
}

uint16_t Pers::count() {
    return offsets.size();
}

bool Pers::readEntry(uint16_t id, PassEntry& entry){
    if(id>=count()) return false;
    memcpy(&entry, offsets[id]+kFlashBase+8, sizeof(PassEntry));
    return true;
}

bool Pers::appendEntry(const PassEntry& entry){
    if(strlen(entry.name)>32 || strlen(entry.username)>128 || strlen(entry.pass)>64)return false;
    if(strlen(entry.name)==0 || strlen(entry.pass)==0)return false;

    PassEntryInner data;
    data.entry = entry;
    data.magic = kEntryMagic;
    data.checksum = 0;
    {
        uint32_t crc = 0;
        uint32_t *base = (uint32_t*)&data;
        for(int i=0; i<sizeof(PassEntryInner)/4; i++) crc ^= base[i];
        data.checksum = crc;
    }

    for(int i=1; i<=256; i++){
        bool hasGood = false;
        for(int j=0; j<4 && !hasGood; j++){
            if(!memcmp(kFlashBase+i*FLASH_SECTOR_SIZE+j*FLASH_PAGE_SIZE, &kEntryMagic, sizeof(kEntryMagic))){
                hasGood = true;
            }
        }
        if(hasGood) continue;

        multicore_lockout_start_blocking();
        uint32_t ints = save_and_disable_interrupts();
        flash_range_erase(FLASH_TARGET_OFFSET+i*FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
        for(int j=0; j<4; j++){
            flash_range_program(FLASH_TARGET_OFFSET+i*FLASH_SECTOR_SIZE+j*FLASH_PAGE_SIZE, (uint8_t*)&data, FLASH_PAGE_SIZE);
        }
        restore_interrupts (ints);
        multicore_lockout_end_blocking();
        offsets.push_back(i*FLASH_SECTOR_SIZE);
        return true;
    }

    return false;
}

bool Pers::eraseEntry(uint16_t id){
    if(id>=count()) return false;
    uint32_t sector = offsets[id]&~(FLASH_SECTOR_SIZE-1);
    multicore_lockout_start_blocking();
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET+sector, FLASH_SECTOR_SIZE);
    restore_interrupts (ints);
    multicore_lockout_end_blocking();
    offsets.erase(offsets.begin()+id);
    return true;
}

uint32_t Pers::getCombinedId(){
    return uid[0]+uid[1];
}

uint32_t Pers::getLowId(){
    return uid[0];
}

uint32_t Pers::getHighId(){
    return uid[1];
}
