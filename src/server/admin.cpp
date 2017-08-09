#include "admin.h"
#include "server.h"

#include "admin/utils/SanitiseString.h"
#include "admin/utils/ClientFromString.h"
#include "admin/utils/ClientCleanName.h"
#include "admin/commands.h"

namespace Admin
{
    std::unordered_map<std::string,Admin> Admin::guid_admin_map;

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

    bool Command(client_t* cl)
    {
        command_t* cmd = (command_t*)bsearch(Cmd_Argv(0), admin_commands, numCmds, sizeof(admin_commands[0]), command_t::cmp);
        if ( !cmd )
            return false;

        if ( cl && !cl->has_permission(cmd->flag) )
        {
            ADMP("Sorry, you do not have permission.\n");
            return false;
        }

        cmd->handler(cl);
        return true;
    }
};


