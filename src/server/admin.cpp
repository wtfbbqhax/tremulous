#include "admin.h"
#include "server.h"

#include "admin/utils/admin_higher.h"
#include "admin/utils/SanitiseString.h"
#include "admin/utils/ClientCleanName.h"
#include "admin/utils/ClientFromString.h"
#include "admin/utils/admin_name_check.h"
#include "admin/commands.h"

namespace Admin
{
std::unordered_map<std::string, Admin> Admin::guid_admin_map;

void Admin::Add(client_t * cl, unsigned level)
{
    Admin admin;
    ::memcpy(admin.guid, cl->guid, sizeof(admin.guid));
    ::memcpy(admin.name, cl->name, sizeof(admin.name));
    admin.flags = 0;
    admin.denied = 0;
    admin.level = level;
    guid_admin_map.emplace(std::make_pair(cl->guid, admin));
    cl->admin = &guid_admin_map.find(cl->guid)->second;
}

Admin* Admin::Find(Guid const guid)
{
    auto it = guid_admin_map.find(guid);
    if (it != guid_admin_map.end())
        return &it->second;
    return nullptr;
}

void Admin::Store()
{
}

void Admin::Load()
{
}

void ConsoleCommand()
{
    Command(nullptr);
}

void Init()
{
    for (size_t i = 0; i < numCmds; ++i)
        Cmd_AddCommand(admin_commands[i].keyword, ConsoleCommand);

    qsort(admin_commands, numCmds, sizeof(admin_commands[0]), command_t::sort);
}

bool Command(client_t* cl)
{
    command_t* cmd = (command_t*)bsearch(Cmd_Argv(0), admin_commands, numCmds, sizeof(admin_commands[0]), command_t::cmp);
    if (!cmd) return false;

    if (cl && !cl->has_permission(cmd->flag))
    {
        ADMP("Sorry, you do not have permission.\n");
        return false;
    }

    cmd->handler(cl);
    return true;
}
};
