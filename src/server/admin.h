#include "server.h"

#include <bitset>
#include <string>
#include <unordered_map>

#include <msgpack.hpp>
#include <fstream>
#include <iostream>
#include <sstream>

#ifndef ADMIN_H
#define ADMIN_H

struct client_t;

namespace Admin {
#define ADMF_IMMUTABLE   0x01
#define ADMF_RCON        0x02
#define ADMF_DEVMAP      0x04
#define ADMF_RENAME      0x08

constexpr int MAX_ADMIN_FLAGS = 8;
using Flags = std::bitset<MAX_ADMIN_FLAGS>;
constexpr int MAX_ADMIN_LEVELS = 10;

class Admin
{
public:
    Guid guid;
    Name name;
    Flags flags;
    Flags denied;
    unsigned level;

    static void Add(client_t * const cl, unsigned level);
    static Admin * Find(Guid const guid);
    static void Store();
    static void Load();

private:
    static std::unordered_map<std::string, Admin> guid_admin_map;
};

void Init();
bool Command(client_t*);
};

#endif
