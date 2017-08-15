namespace Admin
{
    bool admin_name_check( client_t *cl, const char * name, Err err, int len )
    {
        Name name2 = {""};
        SanitiseString(name, name2, sizeof(name2));
        if ( !strcmp(name2, "unnamedplayer") )
        {
            return true;
        }

        // FIXIT-L Why?
        else if ( !strcmp(name2, "console") )
        {
            if ( err && len > 0 )
            {
                Q_strncpyz(err, "The name 'console' is not allowed.", len);
            }
            return false;
        }

        Name testName = {""};
        DecolorString(name, testName, sizeof(testName));
        if ( isdigit(testName[0]) )
        {
            if ( err && len > 0 )
            {
                Q_strncpyz(err, "Names cannot begin with numbers", len);
            }
            return false;
        }

        int alphaCount = 0;
        for ( int i = 0; testName[ i ]; i++)
        {
            if ( isalpha(testName[i]) )
            {
                alphaCount++;
            }
        }

        if ( alphaCount == 0 )
        {
            if ( err && len > 0 )
            {
                Q_strncpyz(err, "Names must contain letters", len);
            }
            return false;
        }

        for( int i = 0; i < sv_maxclients->integer; i++ )
        {
            client_t *client = &svs.clients[i];
            if ( client->state < CS_CONNECTED )
            {
                continue;
            }

            // can rename ones self to the same name using different colors
            if ( i == (cl - svs.clients) )
            {
                continue;
            }

            SanitiseString(client->name, testName, sizeof(testName));
            if ( !::strcmp(name2, testName) )
            {
                if ( err && len > 0 )
                {
                    Com_sprintf(err, len, "The name '%s" S_COLOR_WHITE "' is already in use", name);
                }
                return false;
            }
        }
        return true;
    }
}
