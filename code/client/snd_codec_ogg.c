/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)
Copyright (C) 2005-2006 Joerg Dietrich <dietrich_joerg@gmx.de>

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

// https://xiph.org/vorbis/doc/vorbisfile/callbacks.html


// OGG support is enabled by this define
#include <limits.h>
#include <stdio.h>
#ifdef USE_CODEC_VORBIS

// includes for the Q3 sound system
#include "client.h"
#include "snd_codec.h"

#include <errno.h>
// includes for the OGG codec
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>

// The OGG codec can return the samples in a number of different formats,
// we use the standard signed short format.
#define OGG_SAMPLEWIDTH 2

// Q3 OGG codec
snd_codec_t ogg_codec =
{
    "ogg",
    S_OGG_CodecLoad,
    NULL
};

void dump(const char *p, int cnt);

const char *OggErrorToString(int error) {

    switch ( error ) {
        case OV_FALSE:      return "OV_FALSE";
        case OV_HOLE:       return "hole in data";
        case OV_EBADHEADER: return "Invalid Vorbis bitstream header.";
        case OV_EBADLINK:   return "bad link";
        case OV_EBADPACKET: return "bad packet";
        case OV_EFAULT:     return "Internal logic fault; indicates a bug or heap/stack corruption.";
        case OV_EIMPL:      return "No implementation";
        case OV_EINVAL:     return "invalid value";
        case OV_ENOSEEK:    return "no seek";
        case OV_ENOTAUDIO:  return "not audio";
        case OV_ENOTVORBIS: return "Bitstream does not contain any Vorbis data.";
        case OV_EREAD:      return "A read from media returned an error.";
        case OV_EVERSION:   return "Vorbis version mismatch.";
    }

    return "unknown";
}

// callbacks for vobisfile
int    S_OGG_Callback_close(void *datasource);
int    S_OGG_Callback_seek (void *datasource, ogg_int64_t offset, int whence);
long   S_OGG_Callback_tell (void *datasource);
size_t S_OGG_Callback_read (void *ptr, size_t size, size_t nmemb, void *datasource);

// fread() replacement
size_t S_OGG_Callback_read(void *ptr, const size_t size, const size_t nmemb, void *datasource)
{
    // check if input is valid
    if ( ! ptr ) {
        errno = EFAULT;
        Com_Error(ERR_FATAL, "no ptr");
    }

    // It's not an error, caller just wants zero bytes!
    if ( size == 0 || nmemb == 0 ) {
        errno = 0;
        Com_Printf("wants 0 bytes\n");
        return 0;
    }

    if ( ! datasource ) {
        errno = EBADF;
        Com_Error(ERR_FATAL, "no datasource");
    }

    snd_stream_t *stream = (snd_stream_t *) datasource;

    int read = nmemb * size;

    if ( read == 0 ) {
        return 0;
    }

    if ( stream->pos + read > stream->length ) {
        read = stream->length - stream->pos;
    }

    memcpy(ptr, &stream->datasource[stream->pos], read); //ptr = &stream->datasource[stream->pos];
    stream->pos += read;

    size_t nMembRead = read / size;

    if ( read % size ) {
        nMembRead++;
    }

    return nMembRead;
}

// the callback structure
const ov_callbacks S_OGG_Callbacks =
{
    &S_OGG_Callback_read,
    &S_OGG_Callback_seek,
    &S_OGG_Callback_close,
    &S_OGG_Callback_tell
};

int S_OGG_Callback_close(void *datasource)
{
    // we do nothing here and close all things manually in S_OGG_CodecCloseStream()
    return 0;
}

long S_OGG_Callback_tell(void *datasource)
{
    // check if input is valid
    if ( ! datasource ) {
        errno = EBADF;
        return -1;
    }

    snd_stream_t *stream = (snd_stream_t *) datasource;

    //Com_Printf("%s %d\n", __func__, stream->pos);

    return stream->pos;
}

// fseek() replacement
int S_OGG_Callback_seek(void *datasource, ogg_int64_t offset, int whence)
{
    // check if input is valid
    if ( ! datasource ) {
        errno = EBADF;
        return -1;
    }

    // snd_stream_t in the generic pointer
    snd_stream_t *stream;
    stream = (snd_stream_t *) datasource;
    char *n[3] = {
        "SEEK_SET", "SEEK_CUR", "SEEK_END"
    };

    switch(whence)
    {
        case SEEK_SET :
        {
            // set the file position in the actual file with the Q3 function
            stream->pos = (int) offset;
            break;
        }

        case SEEK_CUR :
        {
            // set the file position in the actual file with the Q3 function
            stream->pos += (int) offset;
            break;
        }

        case SEEK_END :
        {
            // set the file position in the actual file with the Q3 function
            stream->pos = stream->length + (int) offset;
            break;
        }

        default : {
            errno = EINVAL;
            assert(0);
        }
    }

    // stream->pos shouldn't be smaller than zero or bigger than the filesize
    stream->pos = (stream->pos < 0) ? 0 : stream->pos;
    stream->pos = (stream->pos > stream->length) ? stream->length : stream->pos;

    return 0;
}
/*
=================
S_OGG_CodecOpenStream
=================
*/
static snd_stream_t *S_OGG_CodecOpenStream(const char *name, void *data, int length)
{
    snd_stream_t *stream = malloc ( sizeof(snd_stream_t) );

    if ( ! stream ) {
        Com_Error(ERR_FATAL, "malloc");
        return NULL;
    }

    memset(stream, 0, sizeof(snd_stream_t));

    // Open the stream
    stream->pos        = 0;
    stream->ptr        = NULL;
    stream->length     = length;
    stream->datasource = malloc(length);

    memcpy ( stream->datasource, data, length );

    //Com_Printf("%s %s length:%d\n", __func__, name, length);

    // OGG codec control structure
    // alloctate the OggVorbis_File
    OggVorbis_File *vf = malloc(sizeof(OggVorbis_File));
    if ( ! vf ) {
        Com_Error(ERR_FATAL, "malloc");
        return NULL;
    }

    // open the codec with our callbacks and stream as the generic pointer
    int result = ov_open_callbacks(stream, vf, stream->datasource, 0, S_OGG_Callbacks);

    if ( result != 0 ) {
        Com_Printf("ogg error %s for file %s\n", OggErrorToString(result), name);
        free(vf);
        S_CodecUtilClose(&stream);
        assert(0);
        return NULL;
    }

    // the stream must be seekable...
    long seekable = ov_seekable(vf);
    if ( ! seekable ) {
        //Com_Printf("stream seekable %ld\n", seekable);
        assert(0);
        // ov_clear(vf);
        // free(vf);
        // S_CodecUtilClose(&stream);
        // return NULL;
    }

    // we only support OGGs with one substream
    long numStreams = ov_streams(vf);
    if ( numStreams != 1 ) {
        ov_clear(vf);
        free(vf);
        S_CodecUtilClose(&stream);
        return NULL;
    }

    // get the info about channels and rate
    vorbis_info *ogg_info = ov_info(vf, 0);
    if ( ! ogg_info ) {
        ov_clear(vf);
        free(vf);
        S_CodecUtilClose(&stream);
        return NULL;
    }

    // get the number of sample-frames in the OGG
    ogg_int64_t numSamples;
    numSamples = ov_pcm_total(vf, 0);

    // fill in the info-structure in the stream
    stream->info.rate     = ogg_info->rate;     // 22050
    stream->info.width    = OGG_SAMPLEWIDTH;    //     2
    stream->info.channels = ogg_info->channels; //     1
    stream->info.samples  = numSamples;         //  -131 <<<<
    stream->info.size     = stream->info.samples * ogg_info->channels * stream->info.width;
    stream->info.dataofs  = 0;

    Com_Printf("rate:%d width:%d channels:%d samples:%d size:%d dataofs:%d\n", stream->info.rate, stream->info.width, stream->info.channels, stream->info.samples, stream->info.size, stream->info.dataofs );
    // We use stream->pos for the file pointer in the compressed ogg file
    stream->pos = 0;

    // We use the generic pointer in stream for the OGG codec control structure
    stream->ptr = vf;

    return stream;
}

/*
=================
S_OGG_CodecCloseStream
=================
*/
static void S_OGG_CodecCloseStream(snd_stream_t *stream)
{
    // check if input is valid
    if(!stream)
    {
        return;
    }

    // let the OGG codec cleanup its stuff
    ov_clear((OggVorbis_File *) stream->ptr);

    // free the OGG codec control struct
    free(stream->ptr);

    // close the stream
    S_CodecUtilClose(&stream);
}

/*
=================
S_OGG_CodecReadStream
=================
*/
static int S_OGG_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer)
{
    // buffer handling
    int bytesRead, bytesLeft, c;
    char *bufPtr;

    // Bitstream for the decoder
    int BS = 0;

    // big endian machines want their samples in big endian order
    int IsBigEndian = 0;

    // check if input is valid
    if ( ! ( stream && buffer ) ) {
        return 0;
    }

    if ( bytes <= 0 ) {
        return 0;
    }

    bytesRead = 0;
    bytesLeft = bytes;
    bufPtr    = buffer;

    // cycle until we have the requested or all available bytes read
    while ( qtrue ) {
        // read some bytes from the OGG codec
        c = ov_read((OggVorbis_File *) stream->ptr, bufPtr, bytesLeft, IsBigEndian, OGG_SAMPLEWIDTH, 1, &BS);

        if ( c == OV_HOLE ) {
            Com_Printf("OV_HOLE @ %d\n", bytesRead);
            continue ;
        }

        // no more bytes are left
        if ( c <= 0 ) {
            Com_Printf("%s @ %d\n", OggErrorToString(c), bytesRead );
            break;
        }

        bytesRead += c;
        bytesLeft -= c;
        bufPtr    += c;

        // we have enough bytes
        if ( bytesLeft <= 0 ) {
            Com_Printf( "bytes left <= 0 %d\n", bytesLeft );
            break;
        }
    }

    Com_Printf("bytes read: %d\n", bytesRead);
    return bytesRead;
}

/*
=====================================================================
S_OGG_CodecLoad

We handle S_OGG_CodecLoad as a special case of the streaming functions
where we read the whole stream at once.
======================================================================
*/

void *S_OGG_CodecLoad(const char *name, snd_info_t *info) {

    void *data = NULL;
    int length = 0;

    assert(name);
    assert(info);

    qboolean FS_GetPreLoadedFile( const char *name, void **data, int *length);
    if ( ! FS_GetPreLoadedFile ( name, &data, &length ) ) {
        Com_Printf("%s Failed to get preloaded file %s\n", __func__, name);
        return NULL;
    }

    if ( ! data ) {
        Com_Printf ( "get preload no data\n" );
        assert(0);
    }
    // open the file as a stream
    snd_stream_t *stream = S_OGG_CodecOpenStream(name, data, length);
    if( ! stream ) {
        assert(0);
        return NULL;
    }

    // copy over the info
    info->rate     = stream->info.rate;
    info->width    = stream->info.width;
    info->channels = stream->info.channels;
    info->samples  = stream->info.samples;
    info->size     = stream->info.size;
    info->dataofs  = stream->info.dataofs;

    // allocate a buffer
    // this buffer must be free-ed by the caller of this function
    assert(info->size > 0);
    byte *buffer = calloc(info->size, 1);
    if( ! buffer ) {
        assert(0);
        return NULL;
    }

    // fill the buffer
    int bytesRead = S_OGG_CodecReadStream(stream, info->size, buffer); // TODO does this load the entire file? test

    // we don't even have read a single byte
    if ( bytesRead <= 0 ) {
        assert(0);
        free(buffer);
        //S_OGG_CodecCloseStream(stream);
        return NULL;
    }

    if ( info->size != bytesRead ) {
        Com_Printf(" >>>>> %s info.size:%d bytesRead:%d\n", name, info->size, bytesRead);  // I think this is the issue causing the distortion, the whole file not being read .... TODO
        //buffer = realloc ( buffer, bytesRead );
        //info->size = bytesRead;
        //assert(info->size == bytesRead);
    }

    return buffer;
}

void dump(const char *p, int cnt) {

    for ( int i = 0 ; i < cnt ; i++ ) {
        if ( isprint(p[i]) ) {
            putchar(p[i]);
        } else {
            putchar('?');
        }
    }

    putchar('\n');
}

#endif // USE_CODEC_VORBIS

