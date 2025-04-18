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
/*****************************************************************************
 * name:        files.c
 *
 * desc:        handle based filesystem for Quake III Arena
 *
 *****************************************************************************/

#include "q_shared_client.h"
#include "qcommon_client.h"
//#include <emscripten/html5.h>

/*
=============================================================================
QUAKE3 NEW FILESYSTEM

Everything is access by path and retrieved either from
the server or from the browser's cache
=============================================================================
*/

typedef void (*em_idb_onload_func)(void*, void*, int);
void emscripten_idb_async_load(const char *db_name __attribute__((nonnull)), const char *file_id __attribute__((nonnull)), void* arg, em_idb_onload_func onload, em_arg_callback_func onerror);
void emscripten_idb_async_store(const char *db_name __attribute__((nonnull)), const char *file_id __attribute__((nonnull)), void* ptr, int num, void* arg, em_arg_callback_func onstore, em_arg_callback_func onerror);
void emscripten_idb_async_delete(const char *db_name __attribute__((nonnull)), const char *file_id __attribute__((nonnull)), void* arg, em_arg_callback_func ondelete, em_arg_callback_func onerror);
typedef void (*em_idb_exists_func)(void*, int);
void emscripten_idb_async_exists(const char *db_name __attribute__((nonnull)), const char *file_id __attribute__((nonnull)), void* arg, em_idb_exists_func oncheck, em_arg_callback_func onerror);
void emscripten_idb_async_clear(const char *db_name __attribute__((nonnull)), void* arg, em_arg_callback_func onclear, em_arg_callback_func onerror);

typedef struct {
    const char *filename;
    char *reason;
    int  reason_code;
} ignore_file_reason;

#define MAX_FILES_TO_LOG 4096
static byte                *file_list = NULL;
static int                 file_list_length = 0; // DOES NOT includes terminating null
static qboolean            fs_initialized = qfalse;
static ignore_file_reason  files_to_ignore[MAX_FILES_TO_LOG]       = { 0 };
static char *              file_load_success[MAX_FILES_TO_LOG * 4] = { 0 };
static qboolean            fs_preload_file_read = qfalse;
static int                 num_preloaded = 0;

typedef struct preload_s {
    char *hash;    // 32 char md5hash of file. name used to load
    byte *data;    // preload the RAW DATA for consumption by loadshaders/loadimage
    char *name;    // name of preloaded file
    int length;    // length of data
    qboolean read; // has the file been consumed
} preload_t;

#define MAX_MISSES 512
char *preload_misses[MAX_MISSES] = { 0 };

#define NUM_EXTS 3

#define MAX_DRAWIMAGES 4096
static preload_t preloads[MAX_DRAWIMAGES];
static int num_preloads       = 0;
static int preload_total_size = 0;

static int fileLoadsInFlight = 0;
static int numFileLoads      = 0;
static int numFileLoadErrors = 0;

extern char commandLine[ MAX_STRING_CHARS ];
extern char map_name[ MAX_STRING_CHARS ];

#define APPROOT "approot"

#define IDBFSROOT "q3fs"

// All file load errors are now FATAL ( except md3 until I unroll load loops );

void load_progress(char *, int);
static void FS_PreLoadFiles( void );

typedef void(*IDBcallback)(const char *filename, const void *data, const int length) ;
typedef struct IDBcallbackdata{
    const char *filename;
    IDBcallback callback;
};

static void FS_IDBLoadErrorCallback ( void *ptr ) {
    Com_Printf("failed to load file\n");
}

static void FS_IDBStoreErrorCallback ( void *ptr ) {
    Com_Printf("failed to store file\n");
}

qboolean FS_SaveFile( const char *filename, void *data, const int length ) {
    emscripten_idb_async_store(IDBFSROOT, filename, data, length, NULL, NULL, FS_IDBStoreErrorCallback);
    return qtrue;
}

static void FS_GetIDBFileAsyncCallback ( void *arg, void *data, int length ) {

    struct IDBcallbackdata *cbd = arg;
    IDBcallback callback = cbd->callback;
    callback( cbd->filename, data, length );
    //Com_Printf("Loading file '%s' from IDB callback\n", cbd->filename);

    free(cbd);
}

qboolean FS_GetIDBFileAsync ( const char *filename, IDBcallback callback ) {
    assert(filename && filename[0]);
    //Com_Printf("Loading '%s' file from IDB\n", filename );

    struct IDBcallbackdata *cbd = malloc(sizeof(struct IDBcallbackdata));;
    cbd->callback = callback;
    cbd->filename = filename;

    emscripten_idb_async_load(IDBFSROOT, filename, cbd, FS_GetIDBFileAsyncCallback, FS_IDBLoadErrorCallback);
}

static const char *FS_MakePathFromPathRoot ( const char *filename ) {
    static char buffer[MAX_QPATH]; // TODO ????
    memset(buffer, 0, sizeof(buffer));
    Com_sprintf(buffer, MAX_QPATH - 1, "%s/%s", APPROOT, filename);

    return buffer;
}

int FS_GetFileLoadsInFlight(){
    return fileLoadsInFlight;
}

static int fails = 0;
static int goods = 0;
static void FS_FileAddToResults(emscripten_fetch_t *fetch) {

    emscripten_fetch_callback_t* fetch_callback = fetch->userData;

    char *filename = fetch->url;
    char *hash     = (char *)fetch_callback->userData;

    if ( hash ) {
        char *tmp = filename;
        filename = strdup(hash);
        hash = tmp;
    }

    // Com_Printf("%s filename:%s hash:%s\n", __func__, filename, hash );

    if ( fetch->status == 200 ) {
        for ( int i = 0 ; i < MAX_FILES_TO_LOG ; i++ ) {
            if ( ! file_load_success[i] ) {
                //Com_Printf("adding %s to load success list\n", filename);
                file_load_success[i] = filename;
                goods++;
                break;
            }
        }
    } else {
        for ( int i = 0 ; i < MAX_FILES_TO_LOG ; i++ ) {
            ignore_file_reason *fti = &files_to_ignore[i];

            if ( ! fti->filename ) {
                Com_Printf("adding %s to failure list\n", fetch->url);
                fails++;
                fti->filename    = filename;
                fti->reason      = strdup(fetch->statusText);
                fti->reason_code = fetch->status;
                break;
            }
        }
    }
}

void FS_ShowFileLoadResults(void) {

    Com_Printf("File Load Failures ( %d )\n", fails);
    int i = 0;
    for ( i = 0 ; i < MAX_FILES_TO_LOG ; i++ ) {
        ignore_file_reason *fti = &files_to_ignore[i];

        if ( ! fti->filename ) {
            break;
        }

        Com_Printf("%s %s %d\n", fti->filename, fti->reason, fti->reason_code);
    }

    if ( ! i ) {
        Com_Printf("No load failures ..\n");
    }

    Com_Printf("\n");

    Com_Printf("files not read\n");
    int nrcnt = 0;
    for ( i = 0 ; i < num_preloaded ; i++ ) {
        preload_t *p = &preloads[i];

        if ( p->name && ! p->read ) {
            Com_Printf("%d not read %s\n", ++nrcnt, p->name);
        }
    }

    Com_Printf("\n");

    Com_Printf("Successful file loads ( %d ) \n", goods);
    for ( i = 0 ; i < MAX_FILES_TO_LOG ; i++ ) {
        char *name = file_load_success[i];
        if ( ! name ) {
            break;
        }

        Com_Printf("%d: %s\n", i, name);
    }

    Com_Printf("\n");
    Com_Printf("Preload Misses\n");

    qboolean misses = qfalse;
    for ( i = 0 ; i < MAX_MISSES ; i++ ) {
        char *miss = preload_misses[i];

        if ( ! miss ) {
            break;
        }

        misses = qtrue;

        Com_Printf("%d '%s'\n", i, miss);
    }

    if ( ! misses ) {
        Com_Printf("No preload misses\n");
    }

    Com_Printf("\n");
}

#if 0
static const char *Emscripten_result_to_string ( int result ) {

    switch ( result ) {
        case EMSCRIPTEN_RESULT_SUCCESS:             return "EMSCRIPTEN_RESULT_SUCCESS";
        case EMSCRIPTEN_RESULT_DEFERRED:            return "EMSCRIPTEN_RESULT_DEFERRED";
        case EMSCRIPTEN_RESULT_NOT_SUPPORTED:       return "EMSCRIPTEN_RESULT_NOT_SUPPORTED";
        case EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED: return "EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED";
        case EMSCRIPTEN_RESULT_INVALID_TARGET:      return "EMSCRIPTEN_RESULT_INVALID_TARGET";
        case EMSCRIPTEN_RESULT_UNKNOWN_TARGET:      return "EMSCRIPTEN_RESULT_UNKNOWN_TARGET";
        case EMSCRIPTEN_RESULT_INVALID_PARAM:       return "EMSCRIPTEN_RESULT_INVALID_PARAM";
        case EMSCRIPTEN_RESULT_FAILED:              return "EMSCRIPTEN_RESULT_FAILED";
        case EMSCRIPTEN_RESULT_NO_DATA:             return "EMSCRIPTEN_RESULT_NO_DATA";
        case EMSCRIPTEN_RESULT_TIMED_OUT:           return "EMSCRIPTEN_RESULT_TIMED_OUT";
    }
    return "??";
}
#endif


qboolean FS_GetHashFromFileList ( const char *filename, char **hash ) {
}

/*
==================
FS_FileInFileList
Check if a filename is in the
filelist.<mapname> file
==================
*/
static qboolean FS_FileInFileList ( const char *filename ) {
    if ( NULL == filename ) {
        Com_Printf("WARNING: %s filename is null\n", __func__);
        return qfalse;
    }

    if ( ! fs_initialized ) {
        return qtrue;
    }

    if ( ! file_list ) {
        assert(0);
    }

    if ( strstr ( filename, "filelist." ) ) {
        return qtrue;
    }

    if ( strstr ( filename, "shader.list." ) ) {
        return qtrue;
    }

    if ( strstr ( filename, "q3config.cfg" ) ) {
        return qtrue;
    }

    const char * name = FS_MakePathFromPathRoot(filename); // TODO this is wrong here ?,!
    if ( strstr ( (const char *)file_list, filename ) ) {
        return qtrue;
    }

    Com_Printf("%s not found in filelist\n", filename);

    return qfalse;
}

/*
================
FS_FileExistsAsyncCallback
================
*/
#if 0
void FS_FileExistsAsyncCallback( emscripten_fetch_t* fetch ) {
    emscripten_fetch_exists_callback_t *fetch_exists_callback = fetch->userData;
    assert(fetch_exists_callback);

    qboolean exists = fetch->status == 200;

    if ( fetch_exists_callback->_exists ) {
        *fetch_exists_callback->_exists = exists;
    }

    fetch_exists_callback->filesize = fetch->numBytes;

    if ( fetch_exists_callback->_filesize ) {
        *fetch_exists_callback->_filesize = fetch->numBytes;
    }

    if ( exists ) {
        void(*exists_callback )(void *) = fetch_exists_callback->exists_callback;
        if ( exists_callback ) {
            //printf("Calling exists callback handler for file %s\n", fetch->url);
            exists_callback( fetch );
        }
    }

    emscripten_fetch_close(fetch);
}

/*
================
FS_FileExistsAsync
================
*/

// TODO dont think I need this
qboolean FS_FileExistsAsync(const char *filename __attribute__ ((nonnull)), emscripten_fetch_exists_callback_t * callback __attribute__ ((nonnull)))
{
assert(0);

    if ( FS_IgnoreFile ( filename ) ) {
        Com_Printf("%s ignoring file %s\n", __func__, filename); // TODO
        assert(0); // untested

        if ( callback->_exists ) {
            *callback->_exists = qfalse;
        }

        if ( callback->_filesize ) {
            *callback->_filesize = 0;
        }

        return qfalse;
    }

    emscripten_fetch_exists_callback_t *fetch_exists_callback = malloc ( sizeof(emscripten_fetch_exists_callback_t) ); // TODO calloc

    if ( fetch_exists_callback == NULL ) {
        Com_Error( ERR_FATAL, "malloc" );
    }

    fetch_exists_callback->exists           = qfalse;
    fetch_exists_callback->exists_callback  = callback->exists_callback;
    fetch_exists_callback->_exists          = callback->_exists;
    fetch_exists_callback->filesize         = callback->filesize;
    fetch_exists_callback->_filesize        = callback->_filesize;
    fetch_exists_callback->filename         = (char *)filename;
    fetch_exists_callback->userData         = callback->userData;

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);

    strcpy(attr.requestMethod, "HEAD");

    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_PERSIST_FILE | EMSCRIPTEN_FETCH_APPEND; // | EMSCRIPTEN_FETCH_REPLACE; // TODO
    attr.userData   = fetch_exists_callback;
    attr.onerror    = FS_FileExistsAsyncCallback;
    attr.onsuccess  = FS_FileExistsAsyncCallback;
    attr.onprogress = NULL;

    emscripten_fetch_t *fetch = emscripten_fetch(&attr, filename);

    if ( fetch == NULL ) {
        Com_Error ( ERR_FATAL, "FS_GetFile failed to create fetch obj\n" );
        exit(1);
    }

    return qtrue;
}
#endif

#if 0
ifdef MISSIONPACK
size_t FS_FileGetSize ( const char *filename) { // async
assert(0);

    fileHandle_t f = FS_FileAlreadyOpenFileHandle ( filename );

    if ( -1 != f ) {
        return files[f].length;
    }

    size_t retval = 0;
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "HEAD");
    // EMSCRIPTEN_FETCH_REPLACE or EMSCRIPTEN_FETCH_APPEND append doesn't work???
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS | EMSCRIPTEN_FETCH_PERSIST_FILE | EMSCRIPTEN_FETCH_REPLACE;

    emscripten_fetch_t *fetch = emscripten_fetch(&attr, filename); // TODO this was hard coded "file.dat"  did it fix anything?

    if (fetch->status == 200) {
        retval = fetch->numBytes;
    } else {
        Com_Printf("fetching %s failed, failure code: %d.\n", fetch->url, fetch->status);
    }

    emscripten_fetch_close(fetch);

    return retval;
}
#endif

int num_files_to_load = 0;
int preload_start;
void FS_FilesystemInitComplete ( void *ptr ) {
    emscripten_fetch_t * fetch = ptr;

    file_list_length = fetch->numBytes;
    file_list        = malloc ( file_list_length + 1 ); // text file

    if ( ! file_list ) {
        Com_Error(ERR_FATAL, "malloc");
    }

    strncpy( file_list, fetch->data, file_list_length );
    file_list[file_list_length] = '\0';

    Com_Printf("file list size:%d\n", file_list_length);

    // count number of \n
    char *p = strstr(file_list, "\n" );
    while ( p ) {
        num_files_to_load++;
        p = strstr(p + 1, "\n" );
    }

    Com_Printf("Preloading %d Files..\n", num_files_to_load);
    emscripten_set_main_loop(FS_PreLoadFiles, 5, 0); // TOO FAST FOR CHROME

    preload_start = Sys_Milliseconds();

    //commandLine[0] = 0;
}

void FS_GetFileAsyncProgress ( emscripten_fetch_t *fetch ) {
    if ( 0 ) {
        if (fetch->totalBytes) {
            printf("Downloading %s.. %.2f%% complete.\n", fetch->url, fetch->dataOffset * 100.0 / fetch->totalBytes);
        } else {
            printf("Downloading %s.. %lld bytes complete.\n", fetch->url, fetch->dataOffset + fetch->numBytes);
        }
    }
}

void FS_GetFileAsyncError ( emscripten_fetch_t* fetch ) {
    fileLoadsInFlight--;
    numFileLoadErrors++;

    num_preloads--;

    emscripten_fetch_callback_t *fetch_callback = fetch->userData;
    qboolean errFatal = fetch_callback->errFatal;

    FS_FileAddToResults(fetch);
    if ( errFatal ) {
        Com_Error(ERR_FATAL, "%s: %s %s\n", __func__, fetch->statusText, fetch_callback->errMessage ? fetch_callback->errMessage : "unknown error" );
    } else {
        Com_Printf("%s: failed to fetch '%s' %s %s\n", __func__, fetch->url, fetch->statusText, fetch_callback->errMessage ? fetch_callback->errMessage : "unknown error" );
    }

    void(*error_callback )(void *) = fetch_callback->error_callback;

    if ( error_callback ) {
        error_callback( fetch );
    }

    emscripten_fetch_close(fetch);
}

void FS_GetFileAsyncSuccess ( emscripten_fetch_t* fetch ) {
    fileLoadsInFlight--;

    // const char *url = fetch->url;
    // int          id = fetch->id;
    // uint64_t offset = fetch->dataOffset;
    // long   filesize = fetch->numBytes;
    // int  totalBytes = fetch->totalBytes;
    // int  readyState = fetch->readyState;
    // int      status = fetch->status;

    //Com_Printf("%s %s finished retrieving %lu bytes from %s.\n", __func__, fetch->statusText, filesize, fetch->url); // TODO logging

    if ( fs_preload_file_read && num_preloads == num_preloaded ) {
        FS_FileAddToResults(fetch);
    }

    if ( fetch->numBytes == 0 ) {
        Com_Printf("WARNING: file '%s' loaded with 0 bytes\n", fetch->url);
    }

    emscripten_fetch_callback_t *fetch_callback = fetch->userData;
    assert(fetch_callback);

    void(*success_callback )(void *) = fetch_callback->success_callback;

    if ( success_callback ) {
        success_callback( fetch );
    }

    emscripten_fetch_close(fetch);
}

static void FS_PreLoadFilesErrorCallback(void *ptr) {
    fileLoadsInFlight--;
    FS_FileAddToResults(ptr);
}

static void FS_PreLoadFilesCallback(void *ptr ) {
    fileLoadsInFlight--;
    numFileLoads--;

    num_preloaded++;

    emscripten_fetch_t                    *fetch = ptr;
    emscripten_fetch_callback_t * fetch_callback = fetch->userData;

    FS_FileAddToResults(fetch);

    size_t datasize = fetch->numBytes;
    int save_pos    = fetch_callback->additionalData;

    char *hash = strdup(fetch->url);
    char *ar = strstr ( hash, "approot/" );
    hash = ar + 8;

    for ( int i = 0 ; i < 32 ; i++ ) {
        char c = hash[i];
        assert(c >= 'a' && c <= 'f' || c >= 'A' && c <='F' || c >= '0' && c <= '9');
    }

    preload_t *preload = &preloads[save_pos];

    preload->hash   = hash;
    preload->name   = strdup(fetch_callback->userData);
    preload->data   = malloc(datasize);
    preload->length = datasize;
    preload->read   = qfalse;

    memcpy(preload->data, fetch->data, datasize);

    preload_total_size += datasize; // no readers yet TODO

    //Com_Printf("preloaded: %s hash:%s length:%d\n", preload->name, preload->hash, preload->length);
}

static int num_preloading = 0;
static void FS_PreLoadFiles( void ) {

    if ( ! file_list ) {
        Com_Printf("%s unable to preload as file list is not set\n", __func__);
        assert(0);
        return;
    }

    static char *file_list2 = NULL;
    static char *line       = NULL;
    static int line_num     = 0;

    if ( ! file_list2 ) {
        file_list2 = malloc( file_list_length + 1 ); // text file

        if ( ! file_list2 ) {
            Com_Error(ERR_FATAL, "malloc");
        }

        file_list2[file_list_length] = '\0';
        strncpy(file_list2, file_list, file_list_length) ;

        line  = strtok((char *)file_list2, "\n");
    }

    int count = 100;
    while ( line && count-- > 0 ) {
        line_num++;

        int progress = ( (float)line_num / num_files_to_load ) * 100. - 20;
        //Com_Printf("line_num: %d: / num_files_to_load: %d = progress '%d'\n", line_num, num_files_to_load, progress);
        load_progress("Loading Files", progress );
        //Com_Printf("%d '%s'\n", line_num, line);

        char *spc = strchr ( line, ' ' );

        if ( ! spc ) {
            line  = strtok(NULL, "\n");
            Com_Printf(" >>> preload line with no space '%s' <<<\n", line );
            return;
            //Com_Printf("filelist %s\n", file_list);
            //assert(0);
        }

        *spc = '\0';

        char *hash = strdup ( line );
        char *file = strdup ( spc + 1 );

        qboolean loading =
        FS_GetFileAsync( hash, &(emscripten_fetch_callback_t) {
            .additionalData   = num_preloading,
            .error_callback   = FS_PreLoadFilesErrorCallback,
            .errFatal         = qfalse, // TODO for now
            .errMessage       = "failed to preload file",
            .success_callback = FS_PreLoadFilesCallback,
            .userData         = file,
        });

        if ( loading ) {
            num_preloading++;
            fileLoadsInFlight++;
            // Com_Printf("preloading #%d %s\n", line_num, line);
        } else {
            Com_Printf("failed to load %s\n", hash);
            assert(0);
        }

        line  = strtok(NULL, "\n");
    }

    if ( ! line ) {
        emscripten_cancel_main_loop();

        fs_preload_file_read = qtrue;
        Com_Printf("Preload Complete %dms\n", Sys_Milliseconds() - preload_start);

        num_preloads = num_preloading;

        free(file_list2);
        file_list2 = NULL;

        Cmd_AddCommand("filelist", FS_ShowFileLoadResults);

        void Com_Init2(void);
        emscripten_set_main_loop(Com_Init2, 1, 0);
    }
}

// TODO get file from session storage, not preloads
qboolean FS_GetPreLoadedFile(const char *filename, void **data, int *length) {

    *data = NULL;
    *length = 0;

    //const char *name = FS_MakePathFromPathRoot(filename);
    //Com_Printf("%s name:%s filename:%s\n", __func__, name, filename );

    for ( int i = 0 ; i < num_preloads ; i++ ) {
        preload_t *p = &preloads[i];
        if ( ! p->name || ! p->name[0] ) {
            continue;
        }

        if ( strcasecmp(p->name, filename) == 0 ) { // TODO changed to *** strCASEcmp ***
            //Com_Printf("found preload file: %s hash: %s length:%d\n", p->name, p->hash, p->length);
            *data   = p->data;
            *length = p->length;
            p->read = qtrue;
            return qtrue;
        }
    }

    // Com_Printf(">>>>>>>>>failed to find preload file %s\n", filename);

    for ( int i = 0 ; i < MAX_MISSES ; i++ ) {
        char *miss = preload_misses[i];

        if ( miss ) {
            if ( strcasecmp(miss, filename) == 0 ) {
                break;
            }
        } else {
            preload_misses[i] = strdup(filename);
            //Com_Printf("Adding %s to preload misses\n", filename);
            break;
        }
    }

    return qfalse;
}

qboolean FS_GetFileAsync ( const char *filename __attribute__ ((nonnull)), const emscripten_fetch_callback_t * callback __attribute__ ((nonnull)) ) {

    if ( strlen(filename) > 60 ) {
        Com_Printf("%s << WARNING >> long filename '%s' %ld\n", __func__, filename, strlen(filename));
        //return qfalse;
    }

    if ( ! FS_FileInFileList ( filename ) ) {
        return qfalse;
    }

    char *data = NULL;
    int   size = 0;

#if 0
    if ( qfalse && FS_GetPreLoadedFile ( filename, (void **)&data, &size ) ) {
        emscripten_fetch_t *fetch = malloc(sizeof(emscripten_fetch_t));
        memset(fetch, 0, sizeof(emscripten_fetch_t));

        fetch->data     = data;
        fetch->url      = filename;
        fetch->numBytes = size;

        emscripten_fetch_callback_t *fetch_callback = malloc (sizeof(emscripten_fetch_callback_t));
        memset(fetch_callback, 0, sizeof(emscripten_fetch_callback_t));

        fetch_callback->additionalData = callback->additionalData;
        fetch_callback->userData       = callback->userData;

        fetch->userData = fetch_callback;

        if ( callback->success_callback ) {
            callback->success_callback(fetch);
        }

        free(fetch->userData);
        free(fetch);

        return qtrue;
    }
#endif

    //Com_Printf(">>>>>>>>>>> failed to get preloaded file for %s\n", filename );

    // TODO need hash unless q3config.cfg, filelist.<mapname> or shader.list.<mapname>
    emscripten_fetch_callback_t *fetch_callback = malloc ( sizeof(emscripten_fetch_callback_t) );

    if ( fetch_callback == NULL ) {
        Com_Error( ERR_FATAL, "malloc" );
        return qfalse;
    }

    fetch_callback->additionalData   = callback->additionalData;
    fetch_callback->error_callback   = callback->error_callback;
    fetch_callback->errFatal         = callback->errFatal;
    fetch_callback->errMessage       = callback->errMessage;
    fetch_callback->success_callback = callback->success_callback;
    fetch_callback->userData         = callback->userData;

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);

    strcpy(attr.requestMethod, "GET");

    uint32_t attributes;

    attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_REPLACE;

    if ( ! callback->nocache ) {
        attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_PERSIST_FILE | EMSCRIPTEN_FETCH_APPEND; // | EMSCRIPTEN_FETCH_REPLACE; // TODO
    }

    //attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_APPEND; // TODO ignore PERSIST for now.  issues with caching.  still may be a problem

    attr.attributes = attributes;
    attr.userData   = fetch_callback;
    attr.onerror    = FS_GetFileAsyncError;
    attr.onsuccess  = FS_GetFileAsyncSuccess;
    attr.onprogress = FS_GetFileAsyncProgress;

    const char *name = FS_MakePathFromPathRoot(filename);
    emscripten_fetch_t *fetch = emscripten_fetch(&attr, name);

    if ( fetch == NULL ) {
        Com_Error ( ERR_FATAL, "fetch failed to init\n" );
        exit(1);
    }

    fileLoadsInFlight++;
    numFileLoads++;

    return qtrue;
}

#if 0
static void HexDumpData ( const byte *p, int filesize ) {

    printf("-----------------\nhexdump\n");

    for ( int i = 0 ; i < filesize ; i++ ) {
        char c = (char)p[i];

        if ( isprint(c) ) {
            putchar ( (char)p[i] );
        } else {
            printf("<%d>", c);
        }

        if ( i % 40 == 0 ) {
            puts(".\n");
        }
    }

    printf("-----------------\n");
}
#endif

/*
================
FS_InitFilesystem

Called only at initial startup, not when the filesystem
is resetting due to a game change
================
*/

void FS_InitFilesystem( void ) {
    if ( fs_initialized ) {
        Com_Error(ERR_FATAL, "%s fs_initialized is true\n", __func__);
    }

    fs_initialized = qfalse;

    if ( ! commandLine[0] ) {
        assert(0);
    }

    char *mapname = strstr(commandLine, "map=");

    if ( mapname ) {
        char filelist[MAX_QPATH];

        mapname = mapname + 4;
        char *end = strchr ( mapname, ' ' );
        char e;

        if ( end ) {
            e = *end;
            *end = '\0';
        }

        Q_strncpyz ( map_name, mapname, MAX_STRING_CHARS - 1 );

        Com_sprintf(filelist, MAX_QPATH - 1, "filelist.%s", mapname);
        Com_Printf("loading file list %s\n", filelist);

        if ( end ) {
            *end = e;
        }

        qboolean loading =
        // get filelist so only files in the list are allowed to load
        FS_GetFileAsync( filelist, &(emscripten_fetch_callback_t) {
            .additionalData   = 0,
            .error_callback   = NULL,
            .errFatal         = qtrue,
            .errMessage       = "Couldn't load filelist",
            .success_callback = FS_FilesystemInitComplete,
            .userData         = NULL,
        });

        if ( ! loading ) {
            Com_Printf(">> filelist fail %s <<\n", mapname );
            assert(0);
        }

    } else {
        assert(0);
    }

    memset( preload_misses, 0, sizeof(preload_misses));
}

/*
================
FS_Restart
================
*/
void FS_Restart( int checksumFeed ) { // async
    UNUSED(checksumFeed);

    Com_Printf(" >>>>>>>>>>>>>> %s <<<<<<<<<<<< \n", __func__);

    assert(0);

    // TODO get mapname

    // if we can't find default.cfg, assume that the paths are
    // busted and error out now, rather than getting an unreadable
    // graphics screen when the font fails to load
    //    DISABLED for now
    // if ( ! FS_FileExists( "default.cfg" ) ) {
    //     // this might happen when connecting to a pure server not using BASEGAME/pak0.pk3
    //     // (for instance a TA demo server)

    //     Com_Error( ERR_FATAL, "Couldn't load default.cfg" );
    // }
}

/*
================
FS_Initialized
================
*/
qboolean FS_Initialized ( void ) {
    Com_Printf("return fs_preload_file_read %d && num_preloads %d == num_preloaded %d\n", fs_preload_file_read, num_preloads, num_preloaded);
    Com_Printf("Total load size: %d\n", preload_total_size);

    fs_initialized = fs_preload_file_read && num_preloads == num_preloaded;
    return fs_initialized;
}

