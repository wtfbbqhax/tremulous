namespace Admin
{
    // FIXIT-H Find alternative or put somewhere nice 
    void SanitiseString( const char *in, char *out, int len )
    {
        len--;

        while( *in && len > 0 )
        {
            if( Q_IsColorString( in ) )
            {
                in += 2;    // skip color code
                continue;
            }

            if( isalnum( *in ) )
            {
                *out++ = tolower( *in );
                len--;
            }
            in++;
        }
        *out = 0;
    }
}
