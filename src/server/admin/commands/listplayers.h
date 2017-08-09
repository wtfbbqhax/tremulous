namespace Admin
{
    command_t listplayers 
    {
        "listplayers",
        [](client_t*cl)
        {
            int cnt = 0;
            client_t *client = svs.clients;

            if ( client )
            {
                for ( int i = 0; i < sv_maxclients->integer; ++i, ++client )
                {
                    if ( client->state != CS_ACTIVE ) 
                        continue;
                    cnt++;
                    ADMP( "%s\n", client->name );
                }
            }
            
            ADMP(S_COLOR_YELLOW  "listplayers: " S_COLOR_WHITE "%d players connected\n", cnt);
        },
        0
    };
}
