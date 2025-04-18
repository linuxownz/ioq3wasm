/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// TODO merge this and sys_unix into a single file

#include <signal.h>
#include <stdio.h>

#include "../qcommon/q_shared_server.h"
#include "../qcommon/qcommon_server.h"

static char  binaryPath[ MAX_OSPATH ] = { 0 };
static char installPath[ MAX_OSPATH ] = { 0 };

void Sys_PlatformInit( void );
void Sys_PlatformExit( void );
int Sys_PID( void );
qboolean Sys_PIDIsRunning( int pid );

/*
=================
Sys_SetBinaryPath
=================
*/
static void Sys_SetBinaryPath(const char *path)
{
    printf ( "sys/sys_main::Sys_SetBinaryPath %s\n", path ); // TODO
    Q_strncpyz(binaryPath, path, sizeof(binaryPath));
}

/*
=================
Sys_BinaryPath
=================
*/
static char *Sys_BinaryPath(void)
{
    printf ( "sys/sys_main::Sys_(Get)BinaryPath %s\n", binaryPath );
    return binaryPath;
}

/*
=================
Sys_SetDefaultInstallPath
=================
*/
void Sys_SetDefaultInstallPath(const char *path)
{
    printf ( "sys/sys_main::Sys_SetDefaultInstallPath %s\n", path ); // TODO
    Q_strncpyz(installPath, path, sizeof(installPath));
    printf ( "sys/sys_main::Sys_SetDefaultInstallPath-> %s\n", installPath ); // TODO
}

/*
=================
Sys_DefaultInstallPath
=================
*/
char *Sys_DefaultInstallPath(void)
{
    if (*installPath) {
        printf ( "sys/sys_main::Sys_(Get)DefaultInstallPath %s\n", installPath ); // TODO
        return installPath;
    } else {
        char *p = Sys_Cwd();
        printf ( "sys/sys_main::Sys_(Get)DefaultInstallPath cwd() %s\n", p ); // TODO
        return p;
    }
}

/*
=================
Sys_DefaultAppPath
=================
*/
#if 0
// unused
static char *Sys_DefaultAppPath(void)
{
    char *p = Sys_BinaryPath();
    printf ( "sys/sys_main::Sys_(Get)DefaultAppPath %s\n", p ); // TODO
    return p;
}
#endif

#define PID_FILENAME PRODUCT_NAME "_server.pid"

/*
=================
Sys_PIDFileName
=================
*/
static char *Sys_PIDFileName( const char *gamedir )
{
    const char *homePath = Cvar_VariableString( "fs_homepath" );

    if( *homePath != '\0' ) {
        cvar_t *instance_id = Cvar_Get( "com_instance_id", "", CVAR_ARCHIVE|CVAR_LATCH );
        return va( (char *)"%s/%s/%s.pid", homePath, gamedir, instance_id->string );
    }

    return NULL;
}

/*
=================
Sys_RemovePIDFile
=================
*/
void Sys_RemovePIDFile( const char *gamedir )
{
    char *pidFile = Sys_PIDFileName( gamedir );

    if( pidFile != NULL )
        remove( pidFile );
}

/*
=================
Sys_WritePIDFile

Return qtrue if there is an existing stale PID file
=================
*/
static qboolean Sys_WritePIDFile( const char *gamedir )
{
    char      *pidFile = Sys_PIDFileName( gamedir );
    FILE      *f;
    qboolean  stale = qfalse;

    if( pidFile == NULL )
        return qfalse;

    // First, check if the pid file is already there
    if( ( f = fopen( pidFile, "r" ) ) != NULL )
    {
        char  pidBuffer[ 64 ] = { 0 };
        int   pid;

        pid = fread( pidBuffer, sizeof( char ), sizeof( pidBuffer ) - 1, f );
        fclose( f );

        if(pid > 0)
        {
            pid = atoi( pidBuffer );
            if( !Sys_PIDIsRunning( pid ) )
                stale = qtrue;
        }
        else
            stale = qtrue;
    }

    if( FS_CreatePath( pidFile ) ) {
        return 0;
    }

    if( ( f = fopen( pidFile, "w" ) ) != NULL )
    {
        fprintf( f, "%d", Sys_PID( ) );
        fclose( f );
    }
    else
        Com_Printf( S_COLOR_YELLOW "Couldn't write %s.\n", pidFile );

    return stale;
}

/*
=================
Sys_InitPIDFile
=================
*/
void Sys_InitPIDFile( const char *gamedir ) {
    if( Sys_WritePIDFile( gamedir ) ) {
        Com_Printf ( "Sys_InitPIDFile:Sys_WritePIDFile returned true ( pid file is stale? )\n" );
    }
}

/*
=================
Sys_Exit

Single exit point (regular exit or in case of error)
=================
*/
static __attribute__ ((noreturn)) void Sys_Exit( int exitCode )
{
    //CON_Shutdown( );

    if( exitCode < 2 && com_fullyInitialized )
    {
        // Normal exit
        Sys_RemovePIDFile( FS_GetCurrentGameDir() );
    }

    NET_Shutdown( );

    // close and remove com_sockfile // TODO

    Sys_PlatformExit( );

    exit( exitCode );
}

/*
=================
Sys_Quit
=================
*/
void Sys_Quit( void )
{
    Sys_Exit( 0 );
}

/*
=================
Sys_Init
=================
*/
void Sys_Init(void)
{
    Cvar_Set( "arch", OS_STRING " " ARCH_STRING );
    Cvar_Set( "username", Sys_GetCurrentUser( ) );
}

/*
=================
Sys_AnsiColorPrint

Transform Q3 colour codes to ANSI escape sequences
=================
*/
void Sys_AnsiColorPrint( const char *msg )
{
    static char buffer[ MAXPRINTMSG ];
    int         length = 0;
    static int  q3ToAnsi[ 8 ] =
    {
        30, // COLOR_BLACK
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
                // Print the color code
                Com_sprintf( buffer, sizeof( buffer ), "\033[%dm",
                        q3ToAnsi[ ColorIndex( *( msg + 1 ) ) ] );
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

/*
=================
Sys_Print
=================
*/
void Sys_Print( const char *msg )
{
    printf("%s", msg);
}

/*
=================
Sys_Error
=================
*/
void Sys_Error( const char *error, ... )
{
    va_list argptr;
    char    string[1024];

    va_start (argptr,error);
    Q_vsnprintf (string, sizeof(string), error, argptr);
    va_end (argptr);

    Sys_Print( string );

    Sys_Exit( 3 );
}

/*
============
Sys_FileTime

returns -1 if not present
============
*/
#if 0
//unused
static int Sys_FileTime( char *path )
{
    struct stat buf;

    if (stat (path,&buf) == -1)
        return -1;

    return buf.st_mtime;
}
#endif

/*
=================
Sys_ParseArgs
=================
*/
static void Sys_ParseArgs( int argc, char **argv )
{
    if( argc == 2 )
    {
        if( !strcmp( argv[1], "--version" ) ||
                !strcmp( argv[1], "-v" ) )
        {
            const char* date = PRODUCT_DATE;
            fprintf( stdout, Q3_VERSION " dedicated server (%s)\n", date );
            Sys_Exit( 0 );
        }
    }
}

#ifndef DEFAULT_BASEDIR
#   define DEFAULT_BASEDIR Sys_BinaryPath()
#else
# error DEFAULT_BASEDIR not defined
#endif

static const char * signal_to_name ( int signal ) {
    switch ( signal ) {
        case SIGINT:  return "SIGINT";
        case SIGBUS:  return "SIGBUS";
        case SIGFPE:  return "SIGFPE";
        case SIGHUP:  return "SIGHUP";
        case SIGILL:  return "SIGILL";
        case SIGABRT: return "SIGABRT";
        case SIGPIPE: return "SIGPIPE";
    }

    return "?";
}

/*
=================
Sys_SigHandler
=================
*/
void Sys_SigHandler( int signal )
{
    static qboolean signalcaught = qfalse;

    Com_Printf( "Received signal %s %d, exiting...\n", signal_to_name(signal), signal );

    if( signalcaught ) {
        fprintf( stderr, "DOUBLE SIGNAL FAULT: Received signal %d, exiting...\n", signal );
    } else {
        signalcaught = qtrue;
        SV_Shutdown(va("Received signal %d", signal) );
    }

    // TODO need to close and REMOVE com_sockfile
    if( signal == SIGTERM || signal == SIGINT ) {
        Sys_Exit( 1 );
    } else {
        Sys_Exit( 2 );
    }
}

/*
=================
main
=================
*/
int main( int argc, char **argv )
{
    int   i;
    char  commandLine[ MAX_STRING_CHARS ] = { 0 };

    printf("compile date: %s:%s\n", __DATE__, __TIME__);

    Sys_PlatformInit( );

    // Set the initial time base
    Sys_Milliseconds( );
    Sys_ParseArgs( argc, argv );
    Sys_SetBinaryPath( Sys_Dirname( argv[ 0 ] ) );

    // TODO make this configurable? command line option?
    Sys_SetDefaultInstallPath( DEFAULT_BASEDIR );

    // Concatenate the command line for passing to Com_Init
    for( i = 1; i < argc; i++ ) {
        const qboolean containsSpaces = strchr(argv[i], ' ') != NULL;
        if (containsSpaces)
            Q_strcat( commandLine, sizeof( commandLine ), "\"" );

        Q_strcat( commandLine, sizeof( commandLine ), argv[ i ] );

        if (containsSpaces)
            Q_strcat( commandLine, sizeof( commandLine ), "\"" );

        Q_strcat( commandLine, sizeof( commandLine ), " " );
    }

    printf ( "cmdline: %s\n", commandLine );

    Com_Init( commandLine );
    NET_Init( );

    // going to try and support both UDP and websockets sure why not ??? TODO :)
    while( 1 ) {
        Com_Frame( );
    }

    return 0;
}

