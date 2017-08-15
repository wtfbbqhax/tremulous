namespace Admin
{
    static void CompleteMapName(char*, int argc)
    {
        if ( argc == 2 )
        {
            Field_CompleteFilename("maps", "bsp", true, false);
        }
    }

    static void _mapcmd(client_t * cl, bool cheats)
    { 
        char const * map = Cmd_Argv(1);
        if ( !map )
            return;

        QPath expanded;
        Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", map);

        // FIXIT-L: FS_FileExistsInPak?
        if ( FS_ReadFile(expanded, nullptr) == -1 )
        {
            ADMP("Can't find map %s\n", expanded);
            return;
        }

        // save the map name here cause on a map restart we reload the autogen.cfg
        // and thus nuke the arguments of the map command
        QPath mapname;
        Q_strncpyz(mapname, map, sizeof(mapname));

        // start up the map
        SV_SpawnServer(mapname);

        Cvar_Set("sv_cheats", cheats ? "1" : "0");

        // This forces the local master server IP address cache
        // to be updated on sending the next heartbeat
        for( int a = 0; a < 3; ++a )
        {
            for( int i = 0; i < MAX_MASTER_SERVERS; i++ )
                sv_masters[a][i]->modified  = true;
        }
    }

    command_t map
    {
        "map", [](client_t * cl) { _mapcmd(cl, false); }, ADMF_DEVMAP, CompleteMapName
    };

    command_t devmap
    {
        "devmap", [](client_t * cl) { _mapcmd(cl, true); }, ADMF_DEVMAP, CompleteMapName
    };
}
