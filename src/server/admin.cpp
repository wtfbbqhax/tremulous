#include "admin.h"
#include "server.h"

namespace Admin
{
    static void _admin_adduser(client_t *cl)
    {
        AP(S_COLOR_YELLOW "adduser: " S_COLOR_WHITE "\n");
        
    }

    static void _admin_deluser(client_t *cl)
    {
        AP(S_COLOR_YELLOW "deluser: " S_COLOR_WHITE "\n");
    }

    struct command_t 
    {
        const char* keyword;
        void (*handler)(client_t*);
        int flag;
        static int sort( const void *a, const void *b ) { return strcasecmp(((command_t*)a)->keyword, ((command_t*)b)->keyword); }
        static int cmp( const void *a, const void *b ) { return strcasecmp((const char*)a, ((command_t*)b)->keyword); }
    };

    command_t admin_commands[] =
    {
        { "adduser", _admin_adduser, ADMIN_FLG_RCON },
        { "deluser", _admin_deluser, ADMIN_FLG_RCON },
    };

    constexpr size_t numCmds = ARRAY_LEN(admin_commands);

    void ConsoleCommand()
    { Command(nullptr); }

    void Init()
    {
        for ( size_t i = 0; i < numCmds; ++i )
            Cmd_AddCommand(admin_commands[i].keyword, ConsoleCommand);
        qsort(admin_commands, numCmds, sizeof(admin_commands[0]), command_t::sort);
    }

    bool Command(client_t* client)
    {
        command_t* cmd = (command_t*)bsearch(Cmd_Argv(0), admin_commands, numCmds, sizeof(admin_commands[0]), command_t::cmp);
        if ( !cmd )
            return false;

        cmd->handler(client);
        return true;
    }
};


