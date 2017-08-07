#include "server.h"

static inline size_t _safe_vsnprintf(char *str, size_t size, const char *format, va_list args)
{
    if (!size)
        return 0;

    return (size_t)vsnprintf(str, size, format, args);
}

static char sbuf[32768];
static size_t sbufsiz = 0;

void _flush_server_print( client_t *cl )
{
    size_t chunksize = 1022 - strlen("print \"\n\"");
    chunksize = sbufsiz > chunksize ? chunksize : sbufsiz;

    for (int i = 0; sbufsiz; i += chunksize)
    {
        char sav = sbuf[i + chunksize - 1];
        sbuf[i + chunksize - 1] = '\0';
        SV_SendServerCommand( cl, "print \"%s\n\"", sbuf + i);
        sbuf[i + chunksize - 1] = sav;

        chunksize = sbufsiz > chunksize ? chunksize : sbufsiz;
        sbufsiz -= chunksize;
    }
    sbufsiz = 0;
}

void SV_admin_bprintf(client_t * cl, const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    sbufsiz += _safe_vsnprintf(sbuf+sbufsiz, sizeof(sbuf)-sbufsiz, fmt, args);
    va_end(args);

    //Flush
    if ( sbufsiz >= (sizeof(sbuf) * 0.8f) )
        _flush_server_print( cl );
}

void SV_admin_printf(client_t * cl, const char * fmt, ...)
{
    char *buf;
    va_list args;
    va_start(args, fmt);

    if (vasprintf(&buf, fmt, args) < 0)
    {
        // FIXIT-L Maybe we should Com_Error?
        Com_Error(ERR_FATAL, "%s: Unknown error occured\n", __func__);
        return;
    }
    va_end(args);

    if ( !cl )
    {
        Com_Printf("%s", buf);
        return;
    }

    SV_SendServerCommand( cl, "print \"%s\"", buf);
}

void SV_admin_cp_printf(client_t * cl, const char * fmt, ...)
{
    char *buf;
    va_list args;
    va_start(args, fmt);
    if (vasprintf(&buf, fmt, args) < 0) {
        // FIXIT-L Maybe we should Com_Error?
        Com_Error(ERR_FATAL, "%s: Unknown error occured\n", __func__);
        return;
    }
    va_end(args);

    SV_SendServerCommand( cl, "cp \"%s\"", buf);
}
