
#include "system.h"

#include "sys_local.h"
#include "sys_loadlib.h"

#include <iostream>

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#include "../lua-5.3.3/include/lua.hpp"
#include "../sol/sol.hpp"

#include "../script/cvar.h"
//#include "../script/http_client.h"
#include "../script/client.h"
#include "../script/rapidjson.h"
#include "../script/nettle.h"

#ifndef DEDICATED
 #ifndef USE_LOCAL_HEADERS
  #include <SDL.h>
  #include <SDL_cpuinfo.h>
 #else
  #include "SDL.h"
  #include "SDL_cpuinfo.h"
 #endif
#endif

extern char** environ;

void Sys::ExecuteInstaller(const char *path)
{
    std::string cmd{ Sys::DefaultInstallPath() };
    cmd += PATH_SEP;
    cmd += "tremulous-installer";

    std::array<const char*,256> argv { };

    argv[0] = cmd.c_str();
    if ( path && path[0] )
    {
        argv[1] = path;
    }

    Com_Printf( S_COLOR_YELLOW "Executing %s\n", cmd.c_str());

    auto pid = fork();
    if ( pid == -1 )
    {
        Com_Printf(S_COLOR_RED "Failed to launch the installer");
        return;
    }

    if ( pid == 0 )
    {
        execve(cmd.c_str(),
            const_cast<char **>(argv.data()),
            environ);
        Com_Printf(S_COLOR_RED "Failed to launch the installer");
    }
    else
    {
        Engine_Exit("");
    }
}

void Sys::SetBinaryPath(const char *path)
{
    Q_strncpyz(binaryPath, path, sizeof(binaryPath));
}

char *Sys::BinaryPath(void)
{
    return binaryPath;
}

void Sys::SetDefaultInstallPath(const char *path)
{
    Q_strncpyz(installPath, path, sizeof(installPath));
}

char *Sys::DefaultInstallPath(void)
{
    return installPath;
}

char *Sys::DefaultAppPath(void)
{
    return Sys::BinaryPath();
}

void Sys::In_Restart_f()
{
    IN_Restart();
}

char *Sys::ConsoleInput(void)
{
    return CON_Input();
}

char *Sys::GetClipboardData(void)
{
    char *data = nullptr;
#ifndef DEDICATED
    char *cliptext;

    if ( ( cliptext = SDL_GetClipboardText() ) != nullptr ) {
        if ( cliptext[0] != '\0' ) {
            size_t bufsize = strlen( cliptext ) + 1;

            data = (char*)Z_Malloc( bufsize );
            Q_strncpyz( data, cliptext, bufsize );

            // find first listed char and set to '\0'
            strtok( data, "\n\r\b" );
        }
        SDL_free( cliptext );
    }
#endif
    return data;
}

#ifdef DEDICATED
 #define PID_FILENAME "tremded.pid"
#else
 #define PID_FILENAME "tremulous.pid"
#endif

static std::string Sys::PIDFileName()
{
    const char *homePath = Cvar_VariableString( "fs_homepath" );
    std::string pidfile;

    if( *homePath != '\0' )
    {
        pidfile += homePath;
        pidfile += "/";
        pidfile += PID_FILENAME;
    }

    return pidfile;
}

qboolean Sys::WritePIDFile()
{
    const char *pidFile = Sys::PIDFileName( ).c_str();
    FILE *f;
    qboolean  stale = qfalse;

    if( pidFile == nullptr )
        return qfalse;

    // First, check if the pid file is already there
    if( ( f = fopen( pidFile, "r" ) ) != nullptr )
    {
        char  pidBuffer[ 64 ] = { 0 };
        int   pid;

        pid = fread( pidBuffer, sizeof( char ), sizeof( pidBuffer ) - 1, f );
        fclose( f );

        if(pid > 0)
        {
            pid = atoi( pidBuffer );
            if( !Sys::PIDIsRunning( pid ) )
                stale = qtrue;
        }
        else
            stale = qtrue;
    }

    if( ( f = fopen( pidFile, "w" ) ) != nullptr )
    {
        fprintf( f, "%d", Sys::PID( ) );
        fclose( f );
    }
    else
        Com_Printf( S_COLOR_YELLOW "Couldn't write %s.\n", pidFile );

    return stale;
}

static __attribute__ ((noreturn))
void Sys::Exit( int exitCode )
{
    CON_Shutdown( );

#ifndef DEDICATED
    SDL_Quit( );
#endif

    if( exitCode < 2 )
    {
        // Normal exit
        const char *pidFile = Sys::PIDFileName( ).c_str();
        if( pidFile != nullptr )
            remove( pidFile );
    }

    NET_Shutdown( );

    Sys::PlatformExit( );

    exit( exitCode );
}

void Sys::Quit()
{
    Sys::Exit( 0 );
}

cpuFeatures_t Sys::GetProcessorFeatures()
{
    cpuFeatures_t features = CF_NONE;

#ifndef DEDICATED
    if( SDL_HasRDTSC( ) )      features |= CF_RDTSC;
    if( SDL_Has3DNow( ) )      features |= CF_3DNOW;
    if( SDL_HasMMX( ) )        features |= CF_MMX;
    if( SDL_HasSSE( ) )        features |= CF_SSE;
    if( SDL_HasSSE2( ) )       features |= CF_SSE2;
    if( SDL_HasAltiVec( ) )    features |= CF_ALTIVEC;
#endif

    return features;
}

void Sys::Installer_f()
{
    Sys::ExecuteInstaller(Cmd_Args());
}

void Sys::Script_f()
{
    std::string args = Cmd_Args();
    lua.script(args);
}

void Sys::ScriptFile_f()
{
    std::string args = Cmd_Args();
    lua.script_file(args);
}

void Sys::Init(void)
{
    Cvar_Set( "arch", OS_STRING " " ARCH_STRING );

    Cmd_AddCommand("in_restart", Sys::In_Restart_f);

    Cmd_AddCommand("script", Sys::Script_f);
    Cmd_AddCommand("script_file", Sys::ScriptFile_f);
    Cmd_AddCommand("installer", Sys::Installer_f);
}

// FIXME -bbq This could be more extensible
void Sys::AnsiColorPrint( const char *msg )
{
    static char buffer[ MAXPRINTMSG ];
    int         length = 0;
    static int  q3ToAnsi[ 8 ] =
    {
        7, // COLOR_BLACK
        31, // COLOR_RED
        32, // COLOR_GREEN
        33, // COLOR_YELLOW
        34, // COLOR_BLUE
        36, // COLOR_CYAN
        35, // COLOR_MAGENTA
        0   // COLOR_WHITE
    };

    while( *msg )
    {
        if( Q_IsColorString( msg ) || *msg == '\n' )
        {
            // First empty the buffer
            if( length > 0 )
            {
                buffer[ length ] = '\0';
                fputs( buffer, stderr );
                length = 0;
            }

            if( *msg == '\n' )
            {
                // Issue a reset and then the newline
                fputs( "\033[0m\n", stderr );
                msg++;
            }
            else
            {
                // Print the color code (reset first to clear potential inverse (black))
                Com_sprintf( buffer, sizeof(buffer),
                        "\033[0m\033[%dm", q3ToAnsi[ColorIndex(msg[1])]);
                fputs( buffer, stderr );
                msg += 2;
            }
        }
        else
        {
            if( length >= MAXPRINTMSG - 1 )
                break;

            buffer[ length ] = *msg;
            length++;
            msg++;
        }
    }

    // Empty anything still left in the buffer
    if( length > 0 )
    {
        buffer[ length ] = '\0';
        fputs( buffer, stderr );
    }
}

void Sys::Print( const char *msg )
{
    CON_LogWrite( msg );
    CON_Print( msg );
}

void Sys::Error( const char *error, ... )
{
    va_list argptr;
    char    string[1024];

    va_start (argptr,error);
    Q_vsnprintf (string, sizeof(string), error, argptr);
    va_end (argptr);

    Sys_ErrorDialog( string );

    Sys::Exit( 3 );
}

int Sys::FileTime( char *path )
{
    struct stat buf;

    if (stat(path,&buf) == -1)
        return -1;

    return buf.st_mtime;
}

void Sys::UnloadDll( void *dllhandle )
{
    if( !dllhandle )
        return;

    Sys_UnloadLibrary(dllhandle);
}

void* Sys::LoadDll(const char *name, qboolean useSystemLib)
{
    void* dllhandle = nullptr;

    if( useSystemLib )
        dllhandle = Sys_LoadLibrary(name);

    if ( dllhandle )
        return dllhandle;

    if ( useSystemLib )
    {
        Com_Printf(S_COLOR_RED "Failed to load system library \"%s\"", name);
        return nullptr;
    }

    char libPath[MAX_OSPATH];
    char const* topDir = Sys::BinaryPath();

    // search for library in our folder
    if ( !topDir[0] )
        topDir = ".";

    Com_sprintf(libPath, sizeof(libPath),
            "%s%c%s", topDir, PATH_SEP, name);

    dllhandle = Sys_LoadLibrary(libPath);
    if( dllhandle )
    {
        return dllhandle;
    }

    // search the basepath
    char const* basePath = Cvar_VariableString("fs_basepath");
    if( !basePath )
    {
        Com_Printf(S_COLOR_RED "Failed to load library \"%s\"", name);
        return nullptr;
    }

    Com_sprintf(libPath, sizeof(libPath), "%s%c%s",
            basePath, PATH_SEP, name);

    dllhandle = Sys_LoadLibrary(libPath);
    if( !dllhandle )
    {
        Com_Printf(S_COLOR_RED "Failed to load library \"%s\"", name);
        return nullptr;
    }

    return dllhandle;
}

void *Sys::LoadGameDll(const char *name, EntryPoint* entryPoint, SysCalls systemcalls)
{
    void *libHandle = Sys_LoadLibrary(name);
    if ( !libHandle )
    {
        Com_Printf(S_COLOR_RED "Sys::LoadGameDll(%s) failed! \"%s\"",
                name,
                Sys_LibraryError());

        return nullptr;
    }

    Entry entry = (Entry)Sys_LoadFunction(libHandle, "dllEntry");
    if ( !entry )
    {
        Com_Printf(S_COLOR_RED "Sys::LoadGameDll(%s) failed! \"%s\"",
                name,
                "Failed to load `dllEntry`");
        Sys_UnloadLibrary(libHandle);
        return nullptr;
    }

    *entryPoint = (EntryPoint)Sys_LoadFunction(libHandle, "vmMain");
    if ( !*entryPoint )
    {
        Com_Printf(S_COLOR_RED "(Sys::LoadGameDll(%s) failed! \"%s\"",
                name, "Failed to load `vmMain`");
        Sys_UnloadLibrary(libHandle);
        return nullptr;
    }

    entry(systemcalls);
    return libHandle;
}

void Sys::LoadLua()
{
    using namespace script;

    lua.open_libraries(
     sol::lib::base,
     sol::lib::package,
     sol::lib::string,
     sol::lib::table,
     sol::lib::math,
     sol::lib::bit32,
     sol::lib::io,
     sol::lib::os,
     sol::lib::debug,
     sol::lib::utf8);

      rapidjson::init(std::move(lua));
         nettle::init(std::move(lua));
//    http_client::init(std::move(lua));
         client::init(std::move(lua));
           cvar::init(std::move(lua));
}

void Sys::ParseArgs( int argc, char **argv )
{
#ifdef __APPLE__
    if ( argc >= 2 )
    {
        // This is passed if we are launched by double-clicking
        if ( !strcmp(argv[1], "-psn") )
            argc = 1;
    }
#endif

    if ( argc == 2 )
    {
        if ( !strcmp( argv[1], "--version" )
          || !strcmp( argv[1], "-v" ) )
        {
            const char* date = __DATE__;
#ifdef DEDICATED
            fprintf( stdout, Q3_VERSION " dedicated server (%s)\n", date );
#else
            fprintf( stdout, Q3_VERSION " client (%s)\n", date );
#endif
            Sys::Exit( 0 );
        }
    }
}

void Sys::SigHandler( int signal )
{
    static qboolean signalcaught = qfalse;
    if( signalcaught )
    {
        std::cerr << "DOUBLE SIGNAL FAULT: Received signal "
            << signal << std::endl;
        abort();
    }
    else
    {
        char const* msg = va("Received signal %d", signal);

        signalcaught = qtrue;
        VM_Forced_Unload_Start();
#ifndef DEDICATED
        CL_Shutdown(va("Received signal %d", signal), qtrue, qtrue);
#endif
        SV_Shutdown(msg);
        VM_Forced_Unload_Done();
    }

    if( signal == SIGTERM || signal == SIGINT )
        Sys::Exit(1);
    Sys::Exit(2);
}

#ifdef __APPLE__
/*
=================
Sys_StripAppBundle

Discovers if passed dir is suffixed with the directory structure of a Mac OS X
.app bundle. If it is, the .app directory structure is stripped off the end and
the result is returned. If not, dir is returned untouched.
=================
*/
const char *Sys::StripAppBundle( const char *dir )
{
	static char cwd[MAX_OSPATH];

	Q_strncpyz(cwd, dir, sizeof(cwd));
	if(strcmp(Sys_Basename(cwd), "MacOS"))
		return dir;
	Q_strncpyz(cwd, Sys_Dirname(cwd), sizeof(cwd));
	if(strcmp(Sys_Basename(cwd), "Contents"))
		return dir;
	Q_strncpyz(cwd, Sys_Dirname(cwd), sizeof(cwd));
	if(!strstr(Sys_Basename(cwd), ".app"))
		return dir;
	Q_strncpyz(cwd, Sys_Dirname(cwd), sizeof(cwd));
	return cwd;
}
#endif

#ifndef DEFAULT_BASEDIR
# ifdef __APPLE__
#  define DEFAULT_BASEDIR Sys::StripAppBundle(Sys::BinaryPath())
# else
#  define DEFAULT_BASEDIR Sys::BinaryPath()
# endif
#endif

int main( int argc, char **argv )
{
    Sys_PlatformInit( );
    Sys_Milliseconds( );
    Sys::ParseArgs( argc, argv );
    Sys::SetBinaryPath( Sys_Dirname( argv[ 0 ] ) );
    Sys::SetDefaultInstallPath( DEFAULT_BASEDIR );

    char args[MAX_STRING_CHARS];
    args[0] = '\0';

    for( int i = 1; i < argc; i++ )
    {
        auto ws = strchr(argv[i], ' ') ? true : false;
        if (ws) Q_strcat(args, sizeof(args), "\"");
        Q_strcat(args, sizeof(args), argv[i]);
        if (ws) Q_strcat(args, sizeof(args), "\"");
        Q_strcat(args, sizeof(args), " " );
    }

    Com_Init( args );
    NET_Init( );
    CON_Init( );
    Sys::LoadLua( );

    for ( ;; )
    {
        try { 
            IN_Frame( );
            Com_Frame( );
        } 
        catch (std::exception& e) {
            Com_Printf(S_COLOR_YELLOW "%s\n", e.what());
        }
    }

    return 0;
}

