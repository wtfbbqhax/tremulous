#include "../admin.h"

#include "admintest.h"

namespace Admin
{
    command_t admin_commands[] =
    {
        admintest,
    };

    constexpr size_t numCmds = ARRAY_LEN(admin_commands);
}
