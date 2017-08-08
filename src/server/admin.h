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

    void SanitiseString(char const * in, char * out, int len);
    client_t * ClientFromString(char const * s, char * err, int len);
    void ClientCleanName(char const * in, Name out, int outSize); // FIXIT=L: make member of client_t
};
