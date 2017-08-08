#include "../server.h"
#include "../admin.h"

namespace Admin
{
    client_t * ClientFromString(const char * s, char * err, int len)
    {
        client_t *vic, *match;
        int i;
        bool found = false;
        char s2[MAX_NAME_LENGTH];
        char *p = err;
        int l, l2 = len;

        // numeric values are slot numbers 
        i = 0;
        while( s[i] && isdigit(s[i++]) );
        if ( !s[i] )
        {
            i = atoi(s);
            if ( i < 0 || i >= sv_maxclients->integer )
                return nullptr;

            vic = &svs.clients[i];
            if ( vic->state < CS_CONNECTED )
            {
                if ( p )
                    Q_strncpyz(p, "no player connected in that slot #\n", len);
                return nullptr;
            }

            return vic;
        }

        SanitiseString(s, s2, sizeof(s2));
        if ( !s2[0] )
        {
            if ( err )
                Q_strncpyz(err, "no player name provided\n", len);
            return nullptr;
        }

        if( p )
        {
            Q_strncpyz( p, "more than one player name matches. be more specific or use the slot #:\n", l2 );
            l = strlen( p );
            p += l;
            l2 -= l;
        }

        // check for a name match
        for ( i = 0; i < sv_maxclients->integer; ++i )
        {
            char n2[MAX_NAME_LENGTH];
            vic = &svs.clients[i];

            if( vic->state < CS_CONNECTED )
                continue;

            SanitiseString( vic->name, n2, sizeof( n2 ) );

            if( !strcmp( n2, s2 ) )
                return vic;

            if( strstr( n2, s2 ) )
            {
                if( p )
                {
                    l = Q_snprintf( p, l2, "%-2d - %s^7\n", i, vic->name );
                    p += l;
                    l2 -= l;
                }

                found = true;
                match = vic;
            }
        }

        if( found == 1 )
            return match;

        if( found == 0 && err )
            Q_strncpyz( err, "no connected player by that name or slot #\n", len );

        return nullptr;
    }

};
