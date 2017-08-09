#include "server.h"

#include <bitset>
#include <string>
#include <unordered_map>

#ifndef ADMIN_H
#define ADMIN_H

struct client_t;

namespace Admin {
#define ADMF_IMMUTABLE   0x01
#define ADMIN_FLG_RCON   0x02
#define ADMIN_FLG_DEVMAP 0x04

constexpr int MAX_ADMIN_FLAGS = 8;
using Flags = std::bitset<MAX_ADMIN_FLAGS>;

constexpr int MAX_ADMIN_LEVELS = 10;

struct Admin {
    Guid guid;
    Name name;
    Flags flags;
    Flags denied;
    unsigned level;

    static Admin* Find(Guid const guid)
    {
        auto it = guid_admin_map.find(guid);
        if (it != guid_admin_map.end()) return &it->second;
        return nullptr;
    }

    static void Add(client_t *cl, unsigned level);

    static void Store() { }
    static void Load() { }

private:
    static std::unordered_map<std::string, Admin> guid_admin_map;
};

void Init();
bool Command(client_t*);
};

#endif
