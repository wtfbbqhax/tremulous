#include "admin.h"
#include "server.h"

namespace Admin
{
    //
    // Command Handlers
    //
    static void _admin_admintest(client_t *cl)
    {
        if ( !cl )
        {
            ADMP(S_COLOR_YELLOW "admincheck: " S_COLOR_WHITE "you are on the console\n");
            return;
        }

        AP( S_COLOR_YELLOW "admincheck: " S_COLOR_WHITE "%s " S_COLOR_WHITE "\n", cl->name );
    }

    //
    // Command Definitions
    //
    struct command_t 
    {
        const char* keyword;
        void (*handler)(client_t*);
        bool silent;
        int flag;
        const char *function;
        const char *syntax;
    }
    admin_commands[] =
    {
        { "admincheck", _admin_admintest, false, ADMIN_FLG_RCON, "display your current admin level", "" },
    };

    constexpr size_t numCmds = ARRAY_LEN(admin_commands);

    static int _sort_cmp( const void *a, const void *b ) { return strcasecmp(((command_t*)a)->keyword, ((command_t*)b)->keyword); }
    static int _cmdcmp( const void *a, const void *b ) { return strcasecmp((const char*)a, ((command_t*)b)->keyword); }

    void ConsoleCommand()
    { Command(nullptr); }

    //
    // Command Dispatch
    //
    void Init()
    {
        static bool init = false;
        if ( init ) return;

        for ( size_t i = 0; i < numCmds; ++i )
            Cmd_AddCommand(admin_commands[i].keyword, ConsoleCommand);
        
        qsort(admin_commands, numCmds, sizeof(admin_commands[0]), _sort_cmp);
        init = true;
    }

    bool Command(client_t* client)
    {
        Init();

        command_t *cmd = (command_t*)bsearch(Cmd_Argv(0), admin_commands, numCmds, sizeof(admin_commands[0]), _cmdcmp);

        if ( !cmd )
            return false;

        cmd->handler( client );
        return true;
    }
};


