#include "../server.h"
#include "../admin.h"

namespace Admin
{
    static void exec(client_t* cl)
    {
        ADMP(S_COLOR_YELLOW "admintest: " S_COLOR_WHITE "Hello world\n");
    }


    constexpr command_t admintest
    {
        "admintest", exec, 0 
    };
}
