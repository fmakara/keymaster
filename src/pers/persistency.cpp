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

static void eraseSector(uint32_t sector);
static void eraseSectors(uint32_t sector, uint32_t count);
static void writePage(uint32_t sector, uint32_t page, uint8_t* data);
static uint8_t* pagePointer(uint32_t sector, uint32_t page);
static void readFlashUID(uint8_t* uid);

Pers::Pers(){
    {
        bool bad1 = memcmp(pagePointer(0, 0), kMainMagic, sizeof(kMainMagic));
        bool bad2 = memcmp(pagePointer(0, 1), kMainMagic, sizeof(kMainMagic));
        bool bad3 = memcmp(pagePointer(0, 2), kMainMagic, sizeof(kMainMagic));
        bool bad4 = memcmp(pagePointer(0, 3), kMainMagic, sizeof(kMainMagic));
        if( bad1 && bad2 && bad1 && bad4 ){
            eraseSectors(0, 256);
            {
                uint8_t base[FLASH_PAGE_SIZE];
                memset(base, 0xFF, FLASH_PAGE_SIZE);
                memcpy(base, kMainMagic, sizeof(kMainMagic));
                writePage(0, 0, base);
                writePage(0, 1, base);
                writePage(0, 2, base);
                writePage(0, 3, base);
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

                writePage(1, 0, (uint8_t*)base);
                writePage(1, 1, (uint8_t*)base);
                writePage(1, 2, (uint8_t*)base);
                writePage(1, 3, (uint8_t*)base);
            }
        }
    }
    mainOffset = 0;
    for(int j=0; j<4; j++){
        if(memcmp(pagePointer(1, j), &kEntryMagic, sizeof(kEntryMagic))) continue;
        uint32_t crc = 0;
        uint32_t *base = (uint32_t*)(pagePointer(1, j));
        for(int k=0; k<FLASH_PAGE_SIZE/4; k++) crc ^= base[k];
        if(crc!=0) continue;
        mainOffset = FLASH_SECTOR_SIZE+j*FLASH_PAGE_SIZE;
        break;
    }
    for(int i=2; i<256; i++){
        for(int j=0; j<4; j++){
            if(memcmp(pagePointer(i, j), &kEntryMagic, sizeof(kEntryMagic))) continue;
            uint32_t crc = 0;
            uint32_t *base = (uint32_t*)(pagePointer(i, j));
            for(int k=0; k<FLASH_PAGE_SIZE/4; k++) crc ^= base[k];
            if(crc!=0) continue;
            offsets.push_back(i*FLASH_SECTOR_SIZE+j*FLASH_PAGE_SIZE);
            break;
        }
    }
    readFlashUID((uint8_t*)uid);
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
    eraseSector(1);
    for(int j=0; j<4; j++){
        writePage(1, j, (uint8_t*)&data);
    }
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

    for(int i=2; i<=256; i++){
        bool hasGood = false;
        for(int j=0; j<4 && !hasGood; j++){
            if(!memcmp(pagePointer(i, j), &kEntryMagic, sizeof(kEntryMagic))){
                hasGood = true;
            }
        }
        if(hasGood) continue;
        eraseSector(i);
        for(int j=0; j<4; j++){
            writePage(i, j, (uint8_t*)&data);
        }
        offsets.push_back(i*FLASH_SECTOR_SIZE);
        return true;
    }

    return false;
}

bool Pers::eraseEntry(uint16_t id){
    if(id>=count()) return false;
    uint32_t sector = offsets[id]/FLASH_SECTOR_SIZE;
    eraseSector(sector);
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

static uint32_t begin_critical_flash_section(void) {
    if (multicore_lockout_victim_is_initialized(1 - get_core_num())) {
        multicore_lockout_start_blocking();
    }
    return save_and_disable_interrupts();
}

static void end_critical_flash_section(uint32_t state) {
    restore_interrupts(state);
    if (multicore_lockout_victim_is_initialized(1 - get_core_num())) {
        multicore_lockout_end_blocking();
    }
}

static void eraseSector(uint32_t sector){
    uint32_t byteaddress = FLASH_TARGET_OFFSET + sector*FLASH_SECTOR_SIZE;
    uint32_t atomic_state = begin_critical_flash_section();
    flash_range_erase(byteaddress, FLASH_SECTOR_SIZE);
    end_critical_flash_section(atomic_state);
}

static void eraseSectors(uint32_t sector, uint32_t count){
    uint32_t byteaddress = FLASH_TARGET_OFFSET + sector*FLASH_SECTOR_SIZE;
    uint32_t atomic_state = begin_critical_flash_section();
    flash_range_erase(byteaddress, count*FLASH_SECTOR_SIZE);
    end_critical_flash_section(atomic_state);
}

static void writePage(uint32_t sector, uint32_t page, uint8_t* data){
    uint32_t byteaddress = FLASH_TARGET_OFFSET + sector*FLASH_SECTOR_SIZE + page*FLASH_PAGE_SIZE;
    uint32_t atomic_state = begin_critical_flash_section();
    flash_range_program(byteaddress, data, FLASH_PAGE_SIZE);
    end_critical_flash_section(atomic_state);
}

static uint8_t* pagePointer(uint32_t sector, uint32_t page){
    return kFlashBase + sector*FLASH_SECTOR_SIZE + page*FLASH_PAGE_SIZE;
}

static void readFlashUID(uint8_t* uid){
    uint32_t atomic_state = begin_critical_flash_section();
    flash_get_unique_id(uid);
    end_critical_flash_section(atomic_state);
}