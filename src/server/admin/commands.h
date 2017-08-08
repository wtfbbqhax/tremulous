#include "../admin.h"

#include "admintest.h"
#include "listplayers.h"

namespace Admin
{
    command_t admin_commands[] =
    {
        admintest,
        listplayers
    };

    constexpr size_t numCmds = ARRAY_LEN(admin_commands);
}
