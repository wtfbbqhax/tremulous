#include "admin.h"
#include "server.h"
#include "admin/commands.h"

namespace Admin
{
    using Err = char[MAX_STRING_CHARS];

       void ConsoleCommand()
    {
        Command(nullptr);
    }

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


