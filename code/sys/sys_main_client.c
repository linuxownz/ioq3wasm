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

#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <fenv.h>

#include <emscripten.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_cpuinfo.h>

#include "../qcommon/q_shared_client.h"
#include "../qcommon/qcommon_client.h"
#include "../qcommon/net_ws_client.h"

/*
=================
Sys_In_Restart_f

Restart the input subsystem
=================
*/
static void Sys_In_Restart_f( void )
{
    if( !SDL_WasInit( SDL_INIT_VIDEO ) )
    {
        Com_Printf( "in_restart: Cannot restart input while video is shutdown\n" );
        return;
    }

    IN_Restart( );
}

/*
=================
Sys_ConsoleInput

Handle new console input
=================
*/
char *Sys_ConsoleInput(void) // TODO remove
{
    return NULL;
}

/*
=================
Sys_Exit

Single exit point (regular exit or in case of error)
=================
*/
static __attribute__ ((noreturn)) void Sys_Exit( int exitCode )
{
    emscripten_cancel_main_loop();

    Com_Printf("Sys_Exit %d\n", exitCode);
    SDL_Quit( );

    NET_Shutdown( );

    //emscripten_force_exit(exitCode);
    exit( exitCode ); // TODO replace with emscripten_exit()
}

/*
=================
Sys_Quit
=================
*/
void Sys_Quit( void )
{
    void FS_ShowFileLoadResults(void);
    //FS_ShowFileLoadResults();

    Com_Printf("Sys_Quit\n");
    Sys_Exit( 0 );
}

/*
=================
Sys_GetProcessorFeatures
=================
*/
cpuFeatures_t Sys_GetProcessorFeatures( void ) // TODO nothing calls this but good to have
{
    cpuFeatures_t features = 0;

    if( SDL_HasRDTSC( ) )      features |= CF_RDTSC;
    if( SDL_Has3DNow( ) )      features |= CF_3DNOW;
    if( SDL_HasMMX( ) )        features |= CF_MMX;
    if( SDL_HasSSE( ) )        features |= CF_SSE;
    if( SDL_HasSSE2( ) )       features |= CF_SSE2;
    if( SDL_HasAltiVec( ) )    features |= CF_ALTIVEC;

    return features;
}

/*
=================
Sys_Init
=================
*/
void Sys_Init(void)
{
    Cmd_AddCommand( "in_restart", Sys_In_Restart_f );
    Cvar_Set( "arch", OS_STRING " " ARCH_STRING );
}

/*
=================
Sys_Print
=================
*/
void Sys_Print( const char *msg )
{
    Com_Printf( "%s", msg );
}

/*
=================
Sys_Error
=================
*/
static qboolean exit_error = qfalse;
void Sys_Error( const char *error, ... )
{
    va_list argptr;
    char    string[1024];

    exit_error = qtrue;

    va_start (argptr,error);
    Q_vsnprintf (string, sizeof(string), error, argptr);
    va_end (argptr);

    Com_Printf("Sys_Error: %s\n", string);

    emscripten_cancel_main_loop();
    Sys_Exit( 3 );
}

void AtExitFunc(void) {
//#if DEBUG
//    printf("NOT RUNNING ATEXIT\n");
//#else
    printf("%s\n", __func__ );
    if ( ! exit_error ) {
        emscripten_run_script("Module.atExitNow();");
    } else {
        printf("not running atexit on error\n");
    }
//#endif
}

/*
=================
main
=================
*/

char commandLine[ MAX_STRING_CHARS ];
char map_name [ MAX_STRING_CHARS ];
int main( int argc, char **argv )
{
    Com_Printf( "\n------------------------\n" );
    Com_Printf( "  Quake 3 %s\n", VERSION      );
#if DEBUG
    Com_Printf( "  DEBUG BUILD\n"              );
#endif
    Com_Printf( "   date : %s\n", __DATE__     );
    Com_Printf( "   time : %s\n", __TIME__     );
    Com_Printf( " commit : %s\n", GIT_REV      );
    Com_Printf( "   args :" );

    memset(commandLine, 0, sizeof(commandLine));

    for ( int i = 1 ; i < argc ; i++ ) {
        strcat ( commandLine, argv[i] );
        if ( i < argc - 1 ) {
            strcat ( commandLine, " " );
        }
    }

    Com_Printf( "'%s'\n", commandLine         );
    Com_Printf( "------------------------\n"  );

    if ( argc < 2 ) {
        Com_Printf("command line not set, exiting ...\n");
        return 1;
    }

    fesetround(FE_TONEAREST);
    Sys_Milliseconds( ); // Set the initial time base

    atexit( AtExitFunc );
    Com_Init(commandLine);
}

