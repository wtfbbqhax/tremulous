#include "server.h"

#include <string>
#include <bitset>
#include <unordered_map>

namespace Admin
{
    #define ADMIN_FLG_RCON 0x01
    #define MAX_ADMIN_FLAGS   8 
    using Flags = std::bitset<MAX_ADMIN_FLAGS>;

    struct Admin 
    {
        Guid guid;
        Name name;
        Flags flags;
    };

    static std::unordered_map<std::string,Admin> guid_admin_map;

    void Init();
    bool Command(client_t*);
};
