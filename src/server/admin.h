#include "server.h"

#include <string>
#include <bitset>
#include <unordered_map>

#ifndef ADMIN_H
#define ADMIN_H


struct client_t;

namespace Admin
{
    #define ADMIN_FLG_RCON      0x01
    #define ADMIN_FLG_DEVMAP    0x02

    constexpr int MAX_ADMIN_FLAGS = 8;
    using Flags = std::bitset<MAX_ADMIN_FLAGS>;


    constexpr int MAX_ADMIN_LEVELS = 10;

    struct Admin 
    {
        static Admin * Find(Guid const guid)
        {
            auto it = guid_admin_map.find(guid);
            if ( it != guid_admin_map.end() )
                return &it->second;
            return nullptr;
        }

        Guid guid;
        Name name;
        Flags flags;

    private:
        static std::unordered_map<std::string,Admin> guid_admin_map;
    };

    void Init();
    bool Command(client_t*);
};

#endif
