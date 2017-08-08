#include "../server.h"
#include "../admin.h"

namespace Admin
{
    command_t admintest
    {
        "admintest", 
        [](client_t*cl)
        { 
            ADMP(S_COLOR_YELLOW "admintest: " S_COLOR_WHITE "Hello world\n");
        },
        0 
    };
}
