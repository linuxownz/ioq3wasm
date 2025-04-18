/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)

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

#include "client.h"
#include "snd_codec.h"

static snd_codec_t *codecs;

/*
=================
S_CodecLoad

Opens/loads a sound.
=================
*/
void *S_CodecLoad(const char *filename, snd_info_t *info)
{
    char buffer[MAX_QPATH] = { 0 };
    strncpy(buffer, filename, MAX_QPATH - 1);
    char *ext = COM_GetExtension(buffer);

    if ( ! *ext ) {
        Com_Printf("%s filename does not have extension '%s'\n", __func__, filename);
        assert(0);
        return NULL;
    }
    //Com_Printf("ext: %s\n", ext);

    qboolean swapped = qfalse;
#if defined USE_OGG
    if ( strcmp ( ext, "wav" ) == 0 ) {
        //Com_Printf("swapping wav for ogg\n");
        ext = "ogg";
        swapped = qtrue;
    }
#else
    if ( strcmp ( ext, "ogg" ) == 0 ) {
        //Com_Printf("swapping ogg for wav\n");
        ext = "wav";
        swapped = qtrue;
    }
#endif

    if ( swapped ) {
        //Com_Printf("swapped bad extension %s\n", ext);
        char *dot = strrchr(buffer, '.');
        if ( dot ) {
            *dot = '.';
            *(dot+1) = '\0';
            strcat(dot + 1, ext);
        }
    }

    if ( strcmp ( ext, "wav" ) == 0 ) {
        return S_WAV_CodecLoad(buffer, info);
    }

#if defined USE_OGG
    if ( strcmp ( ext, "ogg" ) == 0 ) {
        //Com_Printf("Loading ogg file %s\n", buffer);
        return S_OGG_CodecLoad(buffer, info);
    }
#endif

    else {
        Com_Printf("%s filename is not an .wav/.ogg file '%s'\n", __func__, filename);
        assert(0);
    }

    return NULL;
}

/*
=================
S_CodecInit
=================
*/
void S_CodecInit()
{
    codecs = NULL;

    // TODO convert all sound files to ogg? YES  ISSUE with OV_HOLE, re-enabling wav
    // prefer opus to vorbis
    // Opus is the successor for Vorbis,
    // opus is currently 20230806 not available as a emcc port emcc --show-ports
#ifdef USE_CODEC_OPUS
#error DONT USE CODEC OPUS ( YET )
//    S_CodecRegister(&opus_codec);  // not used, no support
#endif

#ifdef USE_CODEC_VORBIS
    S_CodecRegister(&ogg_codec);   // disabled for async conversion
#endif

// Register wav codec last so that it is always tried first when a file extension was not found
    S_CodecRegister(&wav_codec);
}

/*
=================
S_CodecShutdown
=================
*/
void S_CodecShutdown()
{
    codecs = NULL;
}

/*
=================
S_CodecRegister
=================
*/
void S_CodecRegister(snd_codec_t *codec)
{
    codec->next = codecs;
    codecs = codec;
}

//=======================================================================
// Util functions (used by codecs)

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
=================
S_CodecUtilOpen
=================
*/
snd_stream_t *S_CodecUtilOpen(const char *filename, snd_codec_t *codec)
{
    Com_Printf("%s (%s, *codec)\n", __func__, filename );
    assert(0);

    snd_stream_t *stream = malloc(sizeof(snd_stream_t)); // TODO Z_Malloc or malloc?
    if ( ! stream ) {
        Com_Error(ERR_FATAL, "%s malloc\n", __func__);
        return NULL;
    }

    void **p   = NULL;
    int length = 0; //FS_GetFile( filename, p );

    if ( ! p ) {
        Com_DPrintf("Can't read sound file %s\n", filename);
        return NULL;
    }

    //stream->codec  = codec;
    stream->ptr    = *p;
    stream->length = length;

    // HexDumpData ( stream->ptr, length );
    return stream;
}

/*
=================
S_CodecUtilClose
=================
*/
void S_CodecUtilClose(snd_stream_t **stream)
{
    //FS_FileClose((*stream)->file);
    free(*stream);
    *stream = NULL;
}

