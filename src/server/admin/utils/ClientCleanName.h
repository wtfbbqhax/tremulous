namespace Admin 
{
    // FIXIT-H Find alternative or put somewhere nice 
    #define DECOLOR_OFF '\16'
    #define DECOLOR_ON  '\17'
    static void DecolorString( char const * in, char * out, int len )
    {
        bool decolor = true;

        len--;

        while( *in && len > 0 )
        {
            if( *in == DECOLOR_OFF || *in == DECOLOR_ON )
            {
                decolor = ( *in == DECOLOR_ON );
                in++;
                continue;
            }
            if( Q_IsColorString( in ) && decolor ) {
                in += 2;
                continue;
            }
            *out++ = *in++;
            len--;
        }
        *out = '\0';
    }

    void ClientCleanName( char const * in, Name out, int outSize )
    {
        int len, colorlessLen;
        char *p;
        int spaces;
        bool invalid = false;

        //save room for trailing null byte
        outSize--;

        len = 0;
        colorlessLen = 0;
        p = out;
        *p = 0;
        spaces = 0;

        for( ; *in; in++ )
        {
            // don't allow leading spaces
            if( colorlessLen == 0 && *in == ' ' )
                continue;

            // don't allow nonprinting characters or (dead) console keys
            if( *in < ' ' || *in > '}' || *in == '`' )
                continue;

            // check colors
            if( Q_IsColorString( in ) )
            {
                in++;

                // make sure room in dest for both chars
                if( len > outSize - 2 )
                    break;

                *out++ = Q_COLOR_ESCAPE;

                *out++ = *in;

                len += 2;
                continue;
            }

            // don't allow too many consecutive spaces
            if( *in == ' ' )
            {
                spaces++;
                if( spaces > 3 )
                    continue;
            }
            else
                spaces = 0;

            if( len > outSize - 1 )
                break;

            *out++ = *in;
            colorlessLen++;
            len++;
        }

        *out = 0;

        // don't allow names beginning with "[skipnotify]" because it messes up /ignore-related code
        if( !Q_stricmpn( p, "[skipnotify]", 12 ) )
            invalid = true;

        // don't allow comment-beginning strings because it messes up various parsers
        if( strstr( p, "//" ) || strstr( p, "/*" ) )
            invalid = true;

        // don't allow empty names
        if( *p == 0 || colorlessLen == 0 )
            invalid = true;

        // if something made the name bad, put them back to UnnamedPlayer
        if( invalid )
            Q_strncpyz( p, "UnnamedPlayer", outSize );
    }
}
