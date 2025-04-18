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

#include "tr_common.h"
#include "../qcommon/qcommon_client.h"

typedef struct
{
    char id[2];
    unsigned fileSize;
    unsigned reserved0;
    unsigned bitmapDataOffset;
    unsigned bitmapHeaderSize;
    unsigned width;
    unsigned height;
    unsigned short planes;
    unsigned short bitsPerPixel;
    unsigned compression;
    unsigned bitmapDataSize;
    unsigned hRes;
    unsigned vRes;
    unsigned colors;
    unsigned importantColors;
    unsigned char palette[256][4];
} BMPHeader_t;

void R_LoadBMPCallback(void * ptr);
void R_LoadBMPErrorCallback(void * ptr);

extern int numImageLoadsInFlight;

qboolean R_LoadBMP( const char *name, void *si, imgType_t type, imgFlags_t flags )
{
    // printf("%s type:%d flags:%d\n", __func__, type, flags);

    qboolean loading = FS_GetFileAsync( ( char * ) name, &(emscripten_fetch_callback_t){
        .additionalData = ( type & 0xff ) << 8 | ( flags & 0xff ),
        .errFatal         = qfalse,
        .errMessage       = "failed to load BMP",
        .error_callback   = R_LoadBMPErrorCallback,
        .success_callback = R_LoadBMPCallback,
        .userData         = si,
        .nocache          = qfalse,
    });

    if ( loading ) {
        numImageLoadsInFlight++;
    }

    return loading;
}

void R_LoadBMPErrorCallback(void *ptr) {
    numImageLoadsInFlight--;

    // have to call R_FindImageFileErrorCallback ( from all R_LoadXXX )
    void R_FindImageFileErrorCallback ( void *ptr );
    R_FindImageFileErrorCallback ( ptr );
}


void R_LoadBMPCallback(void * ptr) {
    numImageLoadsInFlight--;

LINE;
assert(0); // havent tested yet ?@!?!?!?!?!?! TODO

    emscripten_fetch_t *fetch = ptr;
    assert(fetch);

    emscripten_fetch_callback_t *fetch_callback = fetch->userData;
    assert(fetch_callback);

    if ( fetch->numBytes == 0 ) {
        Com_Error(ERR_FATAL,"FS_GetFile %s failed", fetch->url);
        return;
    }

    const char *filename = fetch->url;
    int length           = fetch->numBytes;
    imgType_t type       = fetch_callback->additionalData >> 8;
    imgFlags_t flags     = fetch_callback->additionalData & 0xFF;

    printf("%s type:%d flags:%d\n", __func__, type, flags);

    union {
        byte *b;
        void *v;
    } buffer;

    buffer.v = (void *)fetch->data;

    BMPHeader_t bmpHeader;

    if( length < 54 ) {
        Com_Error( ERR_DROP, "LoadBMP: header too short (%s)", filename );
    }

    byte *buf_p = buffer.b;
    byte *end   = buffer.b + length;

    // TODO possible alignment fault

    bmpHeader.id[0] = *buf_p++;
    bmpHeader.id[1] = *buf_p++;

    bmpHeader.fileSize = LittleLong( * ( int * ) buf_p );
    buf_p += 4;

    bmpHeader.reserved0 = LittleLong( * ( int * ) buf_p );
    buf_p += 4;

    bmpHeader.bitmapDataOffset = LittleLong( * ( int * ) buf_p );
    buf_p += 4;

    bmpHeader.bitmapHeaderSize = LittleLong( * ( int * ) buf_p );
    buf_p += 4;

    bmpHeader.width = LittleLong( * ( int * ) buf_p );
    buf_p += 4;

    bmpHeader.height = LittleLong( * ( int * ) buf_p );
    buf_p += 4;

    bmpHeader.planes = LittleShort( * ( short * ) buf_p );
    buf_p += 2;

    bmpHeader.bitsPerPixel = LittleShort( * ( short * ) buf_p );
    buf_p += 2;

    bmpHeader.compression = LittleLong( * ( int * ) buf_p );
    buf_p += 4;

    bmpHeader.bitmapDataSize = LittleLong( * ( int * ) buf_p );
    buf_p += 4;

    bmpHeader.hRes = LittleLong( * ( int * ) buf_p );
    buf_p += 4;

    bmpHeader.vRes = LittleLong( * ( int * ) buf_p );
    buf_p += 4;

    bmpHeader.colors = LittleLong( * ( int * ) buf_p );
    buf_p += 4;

    bmpHeader.importantColors = LittleLong( * ( int * ) buf_p );
    buf_p += 4;

    // TODO possible alignment fault
    if ( bmpHeader.bitsPerPixel == 8 ) {
        if (buf_p + sizeof(bmpHeader.palette) > end) {
            Com_Error( ERR_DROP, "LoadBMP: header too short (%s)", filename );
        }

        Com_Memcpy( bmpHeader.palette, buf_p, sizeof( bmpHeader.palette ) );
    }

    if (buffer.b + bmpHeader.bitmapDataOffset > end) {
        Com_Error( ERR_DROP, "LoadBMP: invalid offset value in header (%s)", filename );
    }

    buf_p = buffer.b + bmpHeader.bitmapDataOffset;

    if ( bmpHeader.id[0] != 'B' && bmpHeader.id[1] != 'M' ) {
        Com_Error( ERR_DROP, "LoadBMP: only Windows-style BMP files supported (%s)", filename );
    }

    if ( bmpHeader.fileSize != length ) {
        Com_Error( ERR_DROP, "LoadBMP: header size does not match file size (%u vs. %u) (%s)", bmpHeader.fileSize, length, filename );
    }

    if ( bmpHeader.compression != 0 ) {
        Com_Error( ERR_DROP, "LoadBMP: only uncompressed BMP files supported (%s)", filename );
    }

    if ( bmpHeader.bitsPerPixel < 8 ) {
        Com_Error( ERR_DROP, "LoadBMP: monochrome and 4-bit BMP files not supported (%s)", filename );
    }

    switch ( bmpHeader.bitsPerPixel )
    {
        case 8:
        case 16:
        case 24:
        case 32:
            break;
        default:
            Com_Error( ERR_DROP, "LoadBMP: illegal pixel_size '%hu' in file '%s'", bmpHeader.bitsPerPixel, filename );
            break;
    }

    int columns = bmpHeader.width;
    int rows    = bmpHeader.height;

    if ( rows < 0 ) {
        rows = -rows;
    }

    unsigned numPixels = columns * rows;

    if( columns <= 0 || !rows || numPixels > 0x1FFFFFFF || ((numPixels * 4) / columns) / 4 != rows)// 4*1FFFFFFF == 0x7FFFFFFC < 0x7FFFFFFF
    {
        Com_Error (ERR_DROP, "LoadBMP: %s has an invalid image size", filename);
    }

    if(buf_p + numPixels*bmpHeader.bitsPerPixel/8 > end) {
        Com_Error (ERR_DROP, "LoadBMP: file truncated (%s)", filename);
    }

    byte *bmpRGBA = malloc( numPixels * 4 );

    for ( int row = rows-1; row >= 0; row-- )
    {
        byte *pixbuf = bmpRGBA + row*columns*4;

        for ( int column = 0; column < columns; column++ )
        {
            unsigned char red, green, blue, alpha;
            int palIndex;
            unsigned short shortPixel;

            switch ( bmpHeader.bitsPerPixel )
            {
                case 8:
                    palIndex = *buf_p++;
                    *pixbuf++ = bmpHeader.palette[palIndex][2];
                    *pixbuf++ = bmpHeader.palette[palIndex][1];
                    *pixbuf++ = bmpHeader.palette[palIndex][0];
                    *pixbuf++ = 0xff;
                    break;
                case 16:
                    shortPixel = * ( unsigned short * ) pixbuf; // TODO possible alignment fault
                    pixbuf += 2;
                    *pixbuf++ = ( shortPixel & ( 31 << 10 ) ) >> 7;
                    *pixbuf++ = ( shortPixel & ( 31 << 5 ) ) >> 2;
                    *pixbuf++ = ( shortPixel & ( 31 ) ) << 3;
                    *pixbuf++ = 0xff;
                    break;

                case 24:
                    blue = *buf_p++;
                    green = *buf_p++;
                    red = *buf_p++;
                    *pixbuf++ = red;
                    *pixbuf++ = green;
                    *pixbuf++ = blue;
                    *pixbuf++ = 255;
                    break;
                case 32:
                    blue = *buf_p++;
                    green = *buf_p++;
                    red = *buf_p++;
                    alpha = *buf_p++;
                    *pixbuf++ = red;
                    *pixbuf++ = green;
                    *pixbuf++ = blue;
                    *pixbuf++ = alpha;
                    break;
            }
        }
    }

    // have to call R_FindImageFileCallback ( from all R_LoadXXX )
    void R_FindImageFileCallback ( void *ptr, byte *pic, int width, int height, imgType_t type, imgFlags_t flags );
    R_FindImageFileCallback ( ptr, bmpRGBA, columns, rows, type, flags );
    // have to call R_FindImageFileCallback ( from all R_LoadXXX )

    free(bmpRGBA);
}

