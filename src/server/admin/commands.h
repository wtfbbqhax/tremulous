namespace Admin
{
    struct command_t 
    {
        const char* keyword;
        void (*handler)(client_t*);
        int flag;
        completionFunc_t complete;

        static int sort( const void *a, const void *b )
        { return strcasecmp(((command_t*)a)->keyword, ((command_t*)b)->keyword); }
        static int cmp( const void *a, const void *b )
        { return strcasecmp((const char*)a, ((command_t*)b)->keyword); }
    };
}

#include "commands/admintest.h"
#include "commands/listplayers.h"
#include "commands/map.h"

namespace Admin
{
    command_t admin_commands[] =
    {
        admintest,
        listplayers,
        devmap,
        map
    };

    constexpr size_t numCmds = ARRAY_LEN(admin_commands);
}
