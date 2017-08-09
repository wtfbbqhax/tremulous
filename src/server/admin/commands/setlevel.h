namespace Admin
{
    command_t setlevel 
    {

        "setlevel", 
        []( client_t *cl )
        {
            if ( Cmd_Argc() < 3 )
            {
                ADMP( S_COLOR_YELLOW "setlevel: " S_COLOR_WHITE "usage: setlevel [name|slot#] [level]\n" );
                return;
            }

            if ( !svs.initialized )
            {
                ADMP( S_COLOR_YELLOW "setlevel: " S_COLOR_WHITE "sever is not initialized\n" );
                return;
            }

            Err err;
            client_t *vic = ClientFromString(Cmd_Argv(1), err, sizeof(err));
            if ( !vic )
            {
                ADMP( S_COLOR_YELLOW "setlevel: " S_COLOR_WHITE "%s\n", err );
                return;
            }

            int max_level = MAX_ADMIN_LEVELS;
            if ( cl )
            {
                max_level = Admin::Find(cl->guid)->level;
                max_level = max_level >= MAX_ADMIN_LEVELS
                                       ? MAX_ADMIN_LEVELS - 1
                                       : max_level;
            }

            int l = atoi(Cmd_Argv(2));
            if ( l < 0 || l >= max_level )
            {
                ADMP( S_COLOR_YELLOW "setlevel: " S_COLOR_WHITE 
                        "sorry, but you may not use setlevel to set a level higher than your own\n" );
                return;
            }

            if ( !admin_higher(cl, vic) )
            {
                ADMP( S_COLOR_YELLOW "setlevel: " S_COLOR_WHITE
                        "sorry, but your intended victim has a higher admin level than you\n" );
                return;
            }

            Admin *s = Admin::Find( vic->guid );
            if ( !s )
            {
                Admin::Add(vic, l);
            }
            else
            {
                vic->admin->level = l;
            }

            AP( S_COLOR_YELLOW "setlevel: " S_COLOR_WHITE "%s "
                S_COLOR_WHITE "was given level %d admin rights by %s\n",
                vic->name, l, cl ? cl->name : "console" );

            // FIXIT-H auth log
            // FIXIT-H update cmdlist on the victim
            Admin::Store();
            return;
        },
        0
    };
}
