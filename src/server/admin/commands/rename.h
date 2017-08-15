namespace Admin
{
    command_t rename 
    {
        ".rename", 
        []( client_t *cl )
        {
            if ( Cmd_Argc() < 3 )
            {
                ADMP( S_COLOR_YELLOW "rename: " S_COLOR_WHITE "usage: [name|slot#] [new name]\n" );
                return;
            }

            if ( !svs.initialized )
            {
                ADMP( S_COLOR_YELLOW "rename: " S_COLOR_WHITE "sever is not initialized\n" );
                return;
            }

            Err err;
            client_t * vic = ClientFromString(Cmd_Argv(1), err, sizeof(err));
            if ( !vic )
            {
                ADMP( S_COLOR_YELLOW "rename: " S_COLOR_WHITE "%s\n", err );
                return;
            }

            if ( !admin_higher(cl, vic) )
            {
                ADMP( S_COLOR_YELLOW "rename: " S_COLOR_WHITE 
                        "sorry, but your intended victim has a higher admin level than you\n" );
                return;
            }

            if ( vic->state < CS_CONNECTED )
            {
                ADMP( S_COLOR_YELLOW "rename: " S_COLOR_WHITE 
                        "sorry but your intended victim is still connecting\n" );
                return;
            }

            const char * const newname = Cmd_Argv(2);
            Name cleanname;

            ClientCleanName(newname, cleanname, sizeof(cleanname));
            if ( !admin_name_check(vic, cleanname, err, sizeof(err)) )
            {
                ADMP( S_COLOR_YELLOW "rename: " S_COLOR_WHITE "%s\n", err );
                return;
            }

            // FIXIT-H prevent rename to registered names? 
            // FIXIT-H auth log

            AP( S_COLOR_YELLOW "rename: " S_COLOR_WHITE "%s "
                S_COLOR_WHITE "has been renamed to %s " S_COLOR_WHITE "by %s\n",
                vic->name, cleanname, cl ? cl->name : "console" );

            Info_SetValueForKey(vic->userinfo, "name", cleanname);
            SV_UserinfoChanged(vic);
            VM_Call(gvm, GAME_CLIENT_USERINFO_CHANGED, vic - svs.clients);
        },
        ADMF_RENAME 
    };
}
