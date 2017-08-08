#include "../server.h"
#include "../admin.h"

namespace Admin
{
    command_t listplayers 
    {
        "listplayers",
        [](client_t*cl)
        {
            int cnt = 0;
            client_t *client = svs.clients;

            if ( !client )
            {
                ADMP( "^3listplayers: ^7%d players connected:\n", cnt );
                return;
            }

            for ( int i = 0; i < sv_maxclients->integer; ++i, ++client )
            {
                if ( client->state != CS_ACTIVE ) 
                    continue;
                cnt++;
                ADMP( "%s\n", client->name );
            }
            ADMP( "^3listplayers: ^7%d players connected:\n", cnt );
        },
        0
    };
}
