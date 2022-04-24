#pragma once

#include <stdint.h>
#include <vector>

class Pers {
public: // Singleton behaviour
    static Pers& get(){
        static Pers instance;
        return instance;
    }
    Pers(Pers const&) = delete;
    void operator=(Pers const&) = delete;
    ~Pers();
private:
    Pers();

public: // Rest of behaviour
    struct PassEntry{
        char name[33];
        char username[129];
        char pass[65];
    };

    uint16_t count();
    bool readEntry(uint16_t id, PassEntry& entry);
    bool appendEntry(const PassEntry& entry);
    bool eraseEntry(uint16_t id);

    uint32_t getCombinedId();
    uint32_t getLowId();
    uint32_t getHighId();

private:
    struct PassEntryInner {
        uint32_t magic;
        uint32_t checksum;
        PassEntry entry;
        char pad[256-(sizeof(PassEntry)+8)];
    };
    std::vector<uint32_t> offsets;
    uint32_t uid[2];
};