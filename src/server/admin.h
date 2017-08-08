#include "server.h"

#include <string>
#include <bitset>
#include <unordered_map>

#ifndef ADMIN_H
#define ADMIN_H


struct client_t;

namespace Admin
{
    #define ADMIN_FLG_RCON 0x01

    constexpr int MAX_ADMIN_FLAGS = 8;
    using Flags = std::bitset<MAX_ADMIN_FLAGS>;


    constexpr int MAX_ADMIN_LEVELS = 10;

    struct Admin 
    {
        Guid guid;
        Name name;
        Flags flags;
    };

    static std::unordered_map<std::string,Admin> guid_admin_map;

    struct command_t 
    {
        const char* keyword;
        void (*handler)(client_t*);
        int flag;

        static int sort( const void *a, const void *b )
        { return strcasecmp(((command_t*)a)->keyword, ((command_t*)b)->keyword); }
        static int cmp( const void *a, const void *b )
        { return strcasecmp((const char*)a, ((command_t*)b)->keyword); }
    };


    void Init();
    bool Command(client_t*);

    void SanitiseString(char const * in, char * out, int len);
    client_t * ClientFromString(char const * s, char * err, int len);
    void ClientCleanName(char const * in, Name out, int outSize); // FIXIT=L: make member of client_t
};

#endif
