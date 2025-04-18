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

#include "../qcommon/q_shared_server.h"
#include "../qcommon/qcommon_server.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/time.h>
#include <libgen.h>
#include <fcntl.h>
#include <fenv.h>
#include <sys/socket.h>
#include <sys/un.h>

void Sys_SigHandler( int signal ) __attribute__ ((noreturn));

// Used to determine where to store user-specific files
static char homePath[ MAX_OSPATH ] = { 0 };

/*
==================
Sys_DefaultHomePath
==================
*/
char *Sys_DefaultHomePath(void)
{
    char *p;

    if( !*homePath && com_homepath != NULL )
    {
        if( ( p = getenv( "HOME" ) ) != NULL )
        {
            Com_sprintf(homePath, sizeof(homePath), "%s%c", p, PATH_SEP);
            if(com_homepath->string[0])
                Q_strcat(homePath, sizeof(homePath), com_homepath->string);
            else
                Q_strcat(homePath, sizeof(homePath), HOMEPATH_NAME_UNIX);
        }
    }

    return homePath;
}

/*
    base time in seconds, that's our origin
    timeval:tv_sec is an int:
    assuming this wraps every 0x7fffffff - ~68 years since the Epoch (1970) - we're safe till 2038

    current time in ms, using sys_timeBase as origin
    NOTE: sys_timeBase*1000 + curtime -> ms since the Epoch
    0x7fffffff ms - ~24 days
    although timeval:tv_usec is an int, I'm not sure wether it is actually used as an unsigned int
    (which would affect the wrap period)
*/

/*
   ================
   Sys_Milliseconds
   ================
*/

static unsigned long sys_timeBase = 0;
static int curtime;

int Sys_Milliseconds (void)
{
    struct timeval tp;

    gettimeofday(&tp, NULL);

    if (!sys_timeBase)
    {
        sys_timeBase = tp.tv_sec;
        return tp.tv_usec/1000;
    }

    curtime = (tp.tv_sec - sys_timeBase)*1000 + tp.tv_usec/1000;

    return curtime;
}

/*
==================
Sys_RandomBytes
==================
*/
qboolean Sys_RandomBytes( byte *string, size_t len )
{
    FILE *fp;

    fp = fopen( "/dev/urandom", "r" );
    if( !fp )
        return qfalse;

    setvbuf( fp, NULL, _IONBF, 0 ); // don't buffer reads from /dev/urandom

    if( fread( string, sizeof( byte ), len, fp ) != len )
    {
        fclose( fp );
        return qfalse;
    }

    fclose( fp );
    return qtrue;
}

/*
==================
Sys_GetCurrentUser
==================
*/
char *Sys_GetCurrentUser( void )
{
    return (char *)"server_user";
}

#define MEM_THRESHOLD 96*1024*1024

/*
==================
Sys_Basename
==================
*/
const char *Sys_Basename( char *path )
{
    return basename( path );
}

/*
==================
Sys_Dirname
==================
*/
const char *Sys_Dirname( char *path )
{
    return dirname( path );
}

/*
==============
Sys_FOpen
==============
*/
FILE *Sys_FOpen( const char *ospath, const char *mode ) {
    struct stat buf;

    // check if path exists and is a directory
    if ( !stat( ospath, &buf ) && S_ISDIR( buf.st_mode ) )
        return NULL;

    return fopen( ospath, mode );
}

/*
==================
Sys_Mkdir
==================
*/
qboolean Sys_Mkdir( const char *path )
{
    int result = mkdir( path, 0750 );

    if( result != 0 )
        return errno == EEXIST;

    return qtrue;
}

/*
==================
Sys_Mkfifo
==================
*/
FILE *Sys_Mkfifo( const char *ospath )
{
    FILE    *fifo;
    int result;
    int fn;
    struct  stat buf;

    // if file already exists AND is a pipefile, remove it
    if( !stat( ospath, &buf ) && S_ISFIFO( buf.st_mode ) )
        FS_Remove( ospath );

    result = mkfifo( ospath, 0600 );
    if( result != 0 )
        return NULL;

    fifo = fopen( ospath, "w+" );
    if( fifo )
    {
        fn = fileno( fifo );
        fcntl( fn, F_SETFL, O_NONBLOCK );
    }

    return fifo;
}

/*
==================
Sys_RemoveAFSock
==================
*/

static char *sockpath = NULL;

static void Sys_RemoveAFSock(void) {
    struct stat buf;

    if ( ! sockpath ) {
        //Com_Printf("Sys_RemoveAFSock: sockpath not defined\n" );
        return;
    }

    //Com_Printf ( "Sys_RemoveAFSock '%s'\n", sockpath );

    // if file already exists AND is a AF_UNIX socket, remove it
    if( ! stat( sockpath, &buf ) ) {
        //Com_Printf ( "Sys_RemoveAFSock: file exists: '%s'\n", sockpath );
        if (  S_ISSOCK( buf.st_mode ) ) {
            //Com_Printf ( "%s is a socket\n", sockpath );
        } else {
            //Com_Printf ( "unknown file %s\n", sockpath );
        }

        //Com_Printf ( "attempting to remove '%s'\n", sockpath );
        FS_Remove( sockpath );
    }
}

/*
==================
Sys_MkAFSock
==================
*/
int Sys_MkAFSock( const char *ospath )
{
    static int count = 0;
    Com_Logf ( "Sys_MkAFSock: '%s'\n", ospath );

    // TODO do a file exists before attempt to clean up log
    // remove this as this should never happen as socket file will be a unique <uuid>.socket
    // but keep this code to clean up other files on exit
    if ( -1 == remove ( ospath ) ) { // Sys_RemoveAFSock ( ); // TODO
        Com_Logf ( "failed to remove '%s' '%s'", ospath, strerror(errno) );
    } else {
        // Com_Printf ( "successfully remove stale socket file '%s'\n", ospath );
    }

    errno = 0;
    struct sockaddr_un my_addr;
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);

    if ( -1 == sock ) {
        int err = errno;

        Com_Logf (            "failed to create socket %s\n", strerror(err) );
        Com_Error( ERR_FATAL, "failed to create socket %s\n", strerror(err) );
        return sock;
    }

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sun_family = AF_UNIX;
    strncpy(my_addr.sun_path, ospath, sizeof(my_addr.sun_path) - 1);

    if ( -1 == bind ( sock, (struct sockaddr *)&my_addr, sizeof(my_addr)) ) {
        Com_Logf  (            "bind call failed %s\n", strerror(errno) );
        Com_Error ( ERR_FATAL, "bind call failed %s\n", strerror(errno) );
        return 0;
    }

    sockpath = strdup ( (char *)ospath );

    if ( -1 == listen(sock, 1024) ) { // /proc/sys/net/ipv4/tcp_max_syn_backlog
        Com_Error( ERR_FATAL, "listen call failed %s\n", strerror(errno) );
        return 0;
    }

    int flags = fcntl(sock, F_GETFL, NULL);
    if ( -1 == flags ) {
        Com_Error( ERR_FATAL, "fcntl F_GETFL failed.%s", strerror(errno));
        return 0;
    } else {
        Com_Logf ( "flags returned from fcntl: %d\n", flags );
        flags |= O_NONBLOCK;
        if (fcntl(sock, F_SETFL, flags) < 0) {
            Com_Error( ERR_FATAL, "fcntl F_SETFL failed.%s", strerror(errno));
        }
    }

    if ( ++count > 1 ) { // TODO should always be less than 2
        Com_Error ( ERR_FATAL, "com_sockfile '%s' created successfully count more than once:%d\n", ospath, count );
    }

    return sock;
}

/*
==================
Sys_Cwd
==================
*/
char *Sys_Cwd( void )
{
    static char cwd[MAX_OSPATH];

    char *result = getcwd( cwd, sizeof( cwd ) - 1 );
    if( result != cwd )
        return NULL;

    cwd[MAX_OSPATH-1] = 0;

    return cwd;
}

/*
==============================================================

DIRECTORY SCANNING

==============================================================
*/

#define MAX_FOUND_FILES 0x1000

/*
==================
Sys_ListFilteredFiles
==================
*/
static void Sys_ListFilteredFiles( const char *basedir, char *subdirs, char *filter, char **list, int *numfiles )
{
    char          search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
    char          filename[MAX_OSPATH];
    DIR           *fdir;
    struct dirent *d;
    struct stat   st;

    if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
        return;
    }

    if (strlen(subdirs)) {
        Com_sprintf( search, sizeof(search), "%s/%s", basedir, subdirs );
    }
    else {
        Com_sprintf( search, sizeof(search), "%s", basedir );
    }

    if ((fdir = opendir(search)) == NULL) {
        return;
    }

    while ((d = readdir(fdir)) != NULL) {
        Com_sprintf(filename, sizeof(filename), "%s/%s", search, d->d_name);
        if (stat(filename, &st) == -1)
            continue;

        if (st.st_mode & S_IFDIR) {
            if (Q_stricmp(d->d_name, ".") && Q_stricmp(d->d_name, "..")) {
                if (strlen(subdirs)) {
                    Com_sprintf( newsubdirs, sizeof(newsubdirs), "%s/%s", subdirs, d->d_name);
                }
                else {
                    Com_sprintf( newsubdirs, sizeof(newsubdirs), "%s", d->d_name);
                }
                Sys_ListFilteredFiles( basedir, newsubdirs, filter, list, numfiles );
            }
        }
        if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
            break;
        }
        Com_sprintf( filename, sizeof(filename), "%s/%s", subdirs, d->d_name );
        if (!Com_FilterPath( filter, filename, qfalse ))
            continue;
        list[ *numfiles ] = CopyString( filename );
        (*numfiles)++;
    }

    closedir(fdir);
}

/*
==================
Sys_ListFiles
==================
*/
char **Sys_ListFiles( const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs )
{
    struct dirent *d;
    DIR           *fdir;
    qboolean      dironly = wantsubs;
    char          search[MAX_OSPATH];
    int           nfiles;
    char          **listCopy;
    char          *list[MAX_FOUND_FILES];
    int           i;
    struct stat   st;

    size_t           extLen;

    if (filter) {

        nfiles = 0;
        Sys_ListFilteredFiles( directory, "", filter, list, &nfiles );

        list[ nfiles ] = NULL;
        *numfiles = nfiles;

        if (!nfiles)
            return NULL;

        listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
        for ( i = 0 ; i < nfiles ; i++ ) {
            listCopy[i] = list[i];
        }
        listCopy[i] = NULL;

        return listCopy;
    }

    if ( !extension)
        extension = "";

    if ( extension[0] == '/' && extension[1] == 0 ) {
        extension = "";
        dironly = qtrue;
    }

    extLen = strlen( extension );

    // search
    nfiles = 0;

    if ((fdir = opendir(directory)) == NULL) {
        *numfiles = 0;
        return NULL;
    }

    while ((d = readdir(fdir)) != NULL) {
        Com_sprintf(search, sizeof(search), "%s/%s", directory, d->d_name);
        if (stat(search, &st) == -1)
            continue;
        if ((dironly && !(st.st_mode & S_IFDIR)) ||
            (!dironly && (st.st_mode & S_IFDIR)))
            continue;

        if (*extension) {
            if ( strlen( d->d_name ) < extLen ||
                Q_stricmp(
                    d->d_name + strlen( d->d_name ) - extLen,
                    extension ) ) {
                continue; // didn't match
            }
        }

        if ( nfiles == MAX_FOUND_FILES - 1 )
            break;
        list[ nfiles ] = CopyString( d->d_name );
        nfiles++;
    }

    list[ nfiles ] = NULL;

    closedir(fdir);

    // return a copy of the list
    *numfiles = nfiles;

    if ( !nfiles ) {
        return NULL;
    }

    listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
    for ( i = 0 ; i < nfiles ; i++ ) {
        listCopy[i] = list[i];
    }
    listCopy[i] = NULL;

    return listCopy;
}

/*
==================
Sys_FreeFileList
==================
*/
void Sys_FreeFileList( char **list )
{
    int i;

    if ( !list ) {
        return;
    }

    for ( i = 0 ; list[i] ; i++ ) {
        Z_Free( list[i] );
    }

    Z_Free( list );
}

/*
==================
Sys_Sleep

Block execution for msec or until input is received.
==================
*/
void Sys_Sleep( int msec )
{
    if( msec == 0 )
        return;

    // With nothing to select() on, we can't wait indefinitely
    if( msec < 0 )
        msec = 10; // TODO make this cvar

    usleep( msec * 1000 );

    return;
}

static void Sys_SetFloatEnv(void)
{
    // rounding toward nearest
    fesetround(FE_TONEAREST);
}

/*
==============
Sys_PlatformInit

Unix specific initialisation
==============
*/
void Sys_PlatformInit( void )
{
    signal( SIGHUP,  Sys_SigHandler );
    signal( SIGQUIT, Sys_SigHandler );
    signal( SIGTRAP, Sys_SigHandler );
    signal( SIGABRT, Sys_SigHandler );
    signal( SIGBUS,  Sys_SigHandler );
    signal( SIGPIPE, SIG_IGN        );

    Sys_SetFloatEnv();
}

/*
==============
Sys_PlatformExit

Unix specific deinitialisation
==============
*/
void Sys_PlatformExit( void )
{
    //Com_Printf ( "Sys_PlatformExit: sockpath:'%s'\n", sockpath );
    Sys_RemoveAFSock ();
    //Com_Printf ( "sys_unix:Sys_PlatformExit\n" );
}

/*
==============
Sys_SetEnv

set/unset environment variables (empty value removes it)
==============
*/

void Sys_SetEnv(const char *name, const char *value)
{
    if(value && *value)
        setenv(name, value, 1);
    else
        unsetenv(name);
}

/*
==============
Sys_PID
==============
*/
int Sys_PID( void )
{
    return getpid( );
}

/*
==============
Sys_PIDIsRunning
==============
*/
qboolean Sys_PIDIsRunning( int pid )
{
    return kill( pid, 0 ) == 0;
}

