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

// tr_init.c -- functions that are not called every frame

#include "tr_local.h"
#include "tr_dsa.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
#include <SDL2/SDL_opengles2_gl2ext.h>
#include <webgl/webgl2.h>

extern int glMajorVersion, glMinorVersion;
#if ! defined(GL_VERSION_ATLEAST)
# define GL_VERSION_ATLEAST(major,minor) (glMajorVersion > major || ( glMajorVersion == major && glMinorVersion >= minor ))
#endif

glconfig_t  glConfig;
glRefConfig_t glRefConfig;

qboolean    textureFilterAnisotropic = qfalse;
int         maxAnisotropy = 0;

glstate_t   glState;

static void GfxInfo_f( void );
static void GfxMemInfo_f( void );

cvar_t  *r_flareSize;
cvar_t  *r_flareFade;
cvar_t  *r_flareCoeff;

cvar_t  *r_railWidth;
cvar_t  *r_railCoreWidth;
cvar_t  *r_railSegmentLength;

cvar_t  *r_verbose;
cvar_t  *r_ignore;

cvar_t  *r_displayRefresh;

cvar_t  *r_detailTextures;

cvar_t  *r_znear;
cvar_t  *r_zproj;
cvar_t  *r_stereoSeparation;

cvar_t  *r_skipBackEnd;

cvar_t  *r_stereoEnabled;
cvar_t  *r_anaglyphMode;

cvar_t  *r_greyscale;

cvar_t  *r_ignorehwgamma;
cvar_t  *r_measureOverdraw;

cvar_t  *r_fastsky;
cvar_t  *r_drawSun;
cvar_t  *r_dynamiclight;
cvar_t  *r_dlightBacks;

cvar_t  *r_lodbias;
cvar_t  *r_lodscale;

cvar_t  *r_norefresh;
cvar_t  *r_drawentities;
cvar_t  *r_drawworld;
cvar_t  *r_speeds;
cvar_t  *r_fullbright;
cvar_t  *r_novis;
cvar_t  *r_nocull;
cvar_t  *r_facePlaneCull;
cvar_t  *r_showcluster;
cvar_t  *r_nocurves;

cvar_t  *r_ext_multitexture;
cvar_t  *r_ext_compiled_vertex_array;
cvar_t  *r_ext_texture_env_add;
cvar_t  *r_ext_texture_filter_anisotropic;
cvar_t  *r_ext_max_anisotropy;

cvar_t  *r_ext_framebuffer_object;
cvar_t  *r_ext_texture_float;
cvar_t  *r_ext_framebuffer_multisample;
cvar_t  *r_arb_seamless_cube_map;
cvar_t  *r_arb_vertex_array_object;
cvar_t  *r_ext_direct_state_access;

cvar_t  *r_cameraExposure;

cvar_t  *r_externalGLSL;

cvar_t  *r_hdr;
cvar_t  *r_floatLightmap;
cvar_t  *r_postProcess;

cvar_t  *r_toneMap;
cvar_t  *r_forceToneMap;
cvar_t  *r_forceToneMapMin;
cvar_t  *r_forceToneMapAvg;
cvar_t  *r_forceToneMapMax;

cvar_t  *r_autoExposure;
cvar_t  *r_forceAutoExposure;
cvar_t  *r_forceAutoExposureMin;
cvar_t  *r_forceAutoExposureMax;

cvar_t  *r_depthPrepass;
cvar_t  *r_ssao;

cvar_t  *r_normalMapping;
cvar_t  *r_specularMapping;
cvar_t  *r_deluxeMapping;
cvar_t  *r_parallaxMapping;
cvar_t  *r_parallaxMapOffset;
cvar_t  *r_parallaxMapShadows;
cvar_t  *r_cubeMapping;
cvar_t  *r_cubemapSize;
cvar_t  *r_deluxeSpecular;
cvar_t  *r_pbr;
cvar_t  *r_baseNormalX;
cvar_t  *r_baseNormalY;
cvar_t  *r_baseParallax;
cvar_t  *r_baseSpecular;
cvar_t  *r_baseGloss;
cvar_t  *r_glossType;
cvar_t  *r_mergeLightmaps;
cvar_t  *r_dlightMode;
cvar_t  *r_pshadowDist;
cvar_t  *r_imageUpsample;
cvar_t  *r_imageUpsampleMaxSize;
cvar_t  *r_imageUpsampleType;
cvar_t  *r_genNormalMaps;
cvar_t  *r_forceSun;
cvar_t  *r_forceSunLightScale;
cvar_t  *r_forceSunAmbientScale;
cvar_t  *r_sunlightMode;
cvar_t  *r_drawSunRays;
cvar_t  *r_sunShadows;
cvar_t  *r_shadowFilter;
cvar_t  *r_shadowBlur;
cvar_t  *r_shadowMapSize;
cvar_t  *r_shadowCascadeZNear;
cvar_t  *r_shadowCascadeZFar;
cvar_t  *r_shadowCascadeZBias;
cvar_t  *r_ignoreDstAlpha;

cvar_t  *r_ignoreGLErrors;
cvar_t  *r_logFile;

cvar_t  *r_ext_multisample;

cvar_t  *r_drawBuffer;
cvar_t  *r_lightmap;
cvar_t  *r_vertexLight;
cvar_t  *r_uiFullScreen;
cvar_t  *r_shadows;
cvar_t  *r_flares;
cvar_t  *r_mode;
cvar_t  *r_nobind;
cvar_t  *r_singleShader;
cvar_t  *r_roundImagesDown;
cvar_t  *r_colorMipLevels;
cvar_t  *r_picmip;
cvar_t  *r_showtris;
cvar_t  *r_showsky;
cvar_t  *r_shownormals;
cvar_t  *r_finish;
cvar_t  *r_clear;
cvar_t  *r_swapInterval;
cvar_t  *r_textureMode;
cvar_t  *r_offsetFactor;
cvar_t  *r_offsetUnits;
cvar_t  *r_gamma;
cvar_t  *r_intensity;
cvar_t  *r_lockpvs;
cvar_t  *r_noportals;
cvar_t  *r_portalOnly;

cvar_t  *r_subdivisions;
cvar_t  *r_lodCurveError;

cvar_t  *r_fullscreen;
cvar_t  *r_noborder;

cvar_t  *r_customwidth;
cvar_t  *r_customheight;
cvar_t  *r_customPixelAspect;

cvar_t  *r_overBrightBits;
cvar_t  *r_mapOverBrightBits;

cvar_t  *r_debugSurface;
cvar_t  *r_simpleMipMaps;

cvar_t  *r_showImages;
cvar_t  *r_showDefaultImage;
cvar_t  *r_showAllShaderImages;

cvar_t  *r_ambientScale;
cvar_t  *r_directedScale;
cvar_t  *r_debugLight;
cvar_t  *r_debugSort;
cvar_t  *r_printShaders;
cvar_t  *r_saveFontData;

cvar_t  *r_marksOnTriangleMeshes;

cvar_t  *r_screenshotJpegQuality;

cvar_t  *r_maxpolys;
int     max_polys;
cvar_t  *r_maxpolyverts;
int     max_polyverts;

/*
** InitOpenGL
**
** This function is responsible for initializing a valid OpenGL subsystem.  This
** is done by calling GLimp_Init (which gives us a working OGL subsystem) then
** setting variables, checking GL constants, and reporting the gfx system config
** to the user.
*/

static void GL_SetDefaultState( void );

extern int glesMajorVersion;
static void InitOpenGL( void )
{
ONCE;
    // TODO is memset here correct?
    Com_Memset( &glConfig,    0, sizeof( glConfig ) );
    Com_Memset( &glRefConfig, 0, sizeof( glRefConfig ) );
    Com_Memset( &glState,     0, sizeof( glState ) );

    GLimp_Init( );
    GLimp_InitExtraExtensions();

    glConfig.textureEnvAddAvailable = qtrue; // TODO

    GLint temp;

    // OpenGL driver constants
    glGetIntegerv( GL_MAX_TEXTURE_SIZE, &temp );
    glConfig.maxTextureSize = temp;

    // stubbed or broken drivers may have reported 0...
    if ( glConfig.maxTextureSize <= 0 )
    {
        Com_Error(ERR_FATAL,"glConfig.maxTextureSize <= 0");
        glConfig.maxTextureSize = 0;
    }

    glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &temp );
    glConfig.numTextureUnits = temp;

    glGetIntegerv( GL_MAX_VERTEX_ATTRIBS, &temp );
    glRefConfig.maxVertexAttribs = temp;

    // reserve 160 components for other uniforms
    if ( glesMajorVersion ) {
        glGetIntegerv( GL_MAX_VERTEX_UNIFORM_VECTORS, &temp );
        temp *= 4;
    } else {
        assert(0);
        glGetIntegerv( GL_MAX_VERTEX_UNIFORM_COMPONENTS, &temp );
    }

    glRefConfig.glslMaxAnimatedBones = Com_Clamp( 0, IQM_MAX_JOINTS, ( temp - 160 ) / 16 );
    if ( glRefConfig.glslMaxAnimatedBones < 12 ) {
        glRefConfig.glslMaxAnimatedBones = 0;
    }

    // check for GLSL function textureCubeLod()
    if ( r_cubeMapping->integer && !GL_VERSION_ATLEAST( 3, 0 ) ) {
        Com_Printf( "WARNING: Disabled r_cubeMapping because it requires OpenGL 3.0\n" );
        Cvar_Set( "r_cubeMapping", "0" );
    }

    // set default state
    GL_SetDefaultState();
}

/*
==================
GL_CheckErrors
==================
*/
extern trGlobals_t tr;

void GL_CheckErrs( char *file, int line ) {
    GLenum err;

    if ( ! tr.registered ) {
        return;
    }

    if ( ( err = glGetError() ) == GL_NO_ERROR ) {
        return;
    }

    if ( r_ignoreGLErrors->integer ) {
        static qboolean shown = qfalse;
        if ( ! shown ) {
            Com_Printf("\n\nOpenGL error occured but r_ignoreGLErrors is 1\n\n");
            shown = qtrue;
        }
        return;
    }

    char *s = "Unknown GL error";
    switch( err ) {
        case GL_INVALID_ENUM:
            s = "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            s = "GL_INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            s = "GL_INVALID_OPERATION";
            break;
        case GL_STACK_OVERFLOW:
            s = "GL_STACK_OVERFLOW";
            break;
        case GL_STACK_UNDERFLOW:
            s = "GL_STACK_UNDERFLOW";
            break;
        case GL_OUT_OF_MEMORY:
            s = "GL_OUT_OF_MEMORY";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            s = "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;
        default:
            break;
    }

    Com_Printf( "%s: %s in %s at line %d\n", __func__,  s, file, line );
    //assert(0);
}

/*
==============================================================================
                        SCREEN SHOTS
NOTE TTimo
some thoughts about the screenshots system:
screenshots get written in fs_homepath + fs_gamedir
vanilla q3 .. baseq3/screenshots/ *.tga
team arena .. missionpack/screenshots/ *.tga

two commands: "screenshot" and "screenshotJPEG"
we use statics to store a count and start writing the first screenshot/screenshot????.tga (.jpg) available
(with FS_FileExists / FS_FOpenFileWrite calls)
FIXME: the statics don't get a reinit between fs_game changes
==============================================================================
*/

/*
==================
RB_ReadPixels

Reads an image but takes care of alignment issues for reading RGB images.

Reads a minimum offset for where the RGB data starts in the image from
integer stored at pointer offset. When the function has returned the actual
offset was written back to address offset. This address will always have an
alignment of packAlign to ensure efficient copying.

Stores the length of padding after a line of pixels to address padlen

Return value must be freed with ri.Hunk_FreeTempMemory()
==================
*/

static byte *RB_ReadPixels(int x, int y, int width, int height, size_t *offset, int *padlen)
{
    byte *buffer, *bufstart;
    int padwidth, linelen, bytesPerPixel;
    int yin, xin, xout;
    GLint packAlign, format;

    // OpenGL ES is only required to support reading GL_RGBA
    if (glesMajorVersion >= 1) {
        format = GL_RGBA;
        bytesPerPixel = 4;
    } else {
        format = GL_RGB;
        bytesPerPixel = 3;
    }

    glGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);

    linelen = width * bytesPerPixel;
    padwidth = PAD(linelen, packAlign);

    // Allocate a few more bytes so that we can choose an alignment we like
    buffer = Hunk_AllocateTempMemory(padwidth * height + *offset + packAlign - 1);

    bufstart = PADP((intptr_t) buffer + *offset, packAlign);
    glReadPixels(x, y, width, height, format, GL_UNSIGNED_BYTE, bufstart);

    linelen = width * 3;

    // Convert RGBA to RGB, in place, line by line
    if (format == GL_RGBA) {
        for (yin = 0; yin < height; yin++) {
            for (xin = 0, xout = 0; xout < linelen; xin += 4, xout += 3) {
                bufstart[yin*padwidth + xout + 0] = bufstart[yin*padwidth + xin + 0];
                bufstart[yin*padwidth + xout + 1] = bufstart[yin*padwidth + xin + 1];
                bufstart[yin*padwidth + xout + 2] = bufstart[yin*padwidth + xin + 2];
            }
        }
    }

    *offset = bufstart - buffer;
    *padlen = padwidth - linelen;

    return buffer;
}

/*
==================
RB_TakeScreenshot
==================
*/
void RB_TakeScreenshot(int x, int y, int width, int height, char *fileName)
{
    assert(0);
    byte *allbuf, *buffer;
    byte *srcptr, *destptr;
    byte *endline, *endmem;
    byte temp;

    int linelen, padlen;
    size_t offset = 18; //, memcount;

    allbuf = RB_ReadPixels(x, y, width, height, &offset, &padlen);
    buffer = allbuf + offset - 18;

    Com_Memset (buffer, 0, 18);
    buffer[2] = 2;      // uncompressed type
    buffer[12] = width & 255;
    buffer[13] = width >> 8;
    buffer[14] = height & 255;
    buffer[15] = height >> 8;
    buffer[16] = 24;    // pixel size

    // swap rgb to bgr and remove padding from line endings
    linelen = width * 3;

    srcptr = destptr = allbuf + offset;
    endmem = srcptr + (linelen + padlen) * height;

    while(srcptr < endmem)
    {
        endline = srcptr + linelen;

        while(srcptr < endline)
        {
            temp = srcptr[0];
            *destptr++ = srcptr[2];
            *destptr++ = srcptr[1];
            *destptr++ = temp;

            srcptr += 3;
        }

        // Skip the pad
        srcptr += padlen;
    }

    //memcount = linelen * height;

    //ri.FS_WriteFile(fileName, buffer, memcount + 18);

    Hunk_FreeTempMemory(allbuf);
}

/*
==================
RB_TakeScreenshotJPEG
==================
*/

void RB_TakeScreenshotJPEG(int x, int y, int width, int height, char *fileName)
{
    assert(0);
#if 0
    byte *buffer;
    size_t offset = 0, memcount;
    int padlen;

    buffer = RB_ReadPixels(x, y, width, height, &offset, &padlen);
    memcount = (width * 3 + padlen) * height;

    //RE_SaveJPG(fileName, r_screenshotJpegQuality->integer, width, height, buffer + offset, padlen);
    Hunk_FreeTempMemory(buffer);
#endif
}

/*
==================
RB_TakeScreenshotCmd
==================
*/
const void *RB_TakeScreenshotCmd( const void *data ) {
    const screenshotCommand_t   *cmd;

    cmd = (const screenshotCommand_t *)data;

    // finish any 2D drawing if needed
    if(tess.numIndexes) {
        RB_EndSurface();
    }

    if (cmd->jpeg) {
        RB_TakeScreenshotJPEG( cmd->x, cmd->y, cmd->width, cmd->height, cmd->fileName);
    } else {
        RB_TakeScreenshot( cmd->x, cmd->y, cmd->width, cmd->height, cmd->fileName);
    }

    return (const void *)(cmd + 1);
}

/*
==================
R_TakeScreenshot
==================
*/
void R_TakeScreenshot( int x, int y, int width, int height, char *name, qboolean jpeg ) {
    static char fileName[MAX_OSPATH]; // bad things if two screenshots per frame?
    screenshotCommand_t *cmd;

    cmd = R_GetCommandBuffer( sizeof( *cmd ) );

    if ( !cmd ) {
        return;
    }

    cmd->commandId = RC_SCREENSHOT;

    cmd->x = x;
    cmd->y = y;
    cmd->width = width;
    cmd->height = height;
    Q_strncpyz( fileName, name, sizeof(fileName) );
    cmd->fileName = fileName;
    cmd->jpeg = jpeg;
}

/*
==================
R_ScreenshotFilename
==================
*/
void R_ScreenshotFilename( int lastNumber, char *fileName ) {
    int     a,b,c,d;

    if ( lastNumber < 0 || lastNumber > 9999 ) {
        Com_sprintf( fileName, MAX_OSPATH, "screenshots/shot9999.tga" );
        return;
    }

    a = lastNumber / 1000;
    lastNumber -= a*1000;
    b = lastNumber / 100;
    lastNumber -= b*100;
    c = lastNumber / 10;
    lastNumber -= c*10;
    d = lastNumber;

    Com_sprintf( fileName, MAX_OSPATH, "screenshots/shot%i%i%i%i.tga" , a, b, c, d );
}

/*
==================
R_ScreenshotFilename
==================
*/
void R_ScreenshotFilenameJPEG( int lastNumber, char *fileName ) {
    int     a,b,c,d;

    if ( lastNumber < 0 || lastNumber > 9999 ) {
        Com_sprintf( fileName, MAX_OSPATH, "screenshots/shot9999.jpg" );
        return;
    }

    a = lastNumber / 1000;
    lastNumber -= a*1000;
    b = lastNumber / 100;
    lastNumber -= b*100;
    c = lastNumber / 10;
    lastNumber -= c*10;
    d = lastNumber;

    Com_sprintf( fileName, MAX_OSPATH, "screenshots/shot%i%i%i%i.jpg" , a, b, c, d );
}

/*
====================
R_LevelShot

levelshots are specialized 128*128 thumbnails for
the menu system, sampled down from full screen distorted images
====================
*/
void R_LevelShot( void ) {
    char        checkname[MAX_OSPATH];
    byte        *buffer;
    byte        *source, *allsource;
    byte        *src, *dst;
    size_t          offset = 0;
    int         padlen;
    int         x, y;
    int         r, g, b;
    float       xScale, yScale;
    int         xx, yy;

    Com_sprintf(checkname, sizeof(checkname), "levelshots/%s.tga", tr.world->baseName);

    allsource = RB_ReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight, &offset, &padlen);
    source = allsource + offset;

    buffer = Hunk_AllocateTempMemory(128 * 128*3 + 18);
    Com_Memset (buffer, 0, 18);
    buffer[2] = 2;      // uncompressed type
    buffer[12] = 128;
    buffer[14] = 128;
    buffer[16] = 24;    // pixel size

    // resample from source
    xScale = glConfig.vidWidth / 512.0f;
    yScale = glConfig.vidHeight / 384.0f;
    for ( y = 0 ; y < 128 ; y++ ) {
        for ( x = 0 ; x < 128 ; x++ ) {
            r = g = b = 0;
            for ( yy = 0 ; yy < 3 ; yy++ ) {
                for ( xx = 0 ; xx < 4 ; xx++ ) {
                    src = source + (3 * glConfig.vidWidth + padlen) * (int)((y*3 + yy) * yScale) +
                        3 * (int) ((x*4 + xx) * xScale);
                    r += src[0];
                    g += src[1];
                    b += src[2];
                }
            }
            dst = buffer + 18 + 3 * ( y * 128 + x );
            dst[0] = b / 12;
            dst[1] = g / 12;
            dst[2] = r / 12;
        }
    }

    //ri.FS_WriteFile( checkname, buffer, 128 * 128*3 + 18 ); // TODO

    Hunk_FreeTempMemory(buffer);
    Hunk_FreeTempMemory(allsource);

    Com_Printf( "Wrote %s\n", checkname );
}

/*
==================
R_ScreenShot_f

screenshot
screenshot [silent]
screenshot [levelshot]
screenshot [filename]

Doesn't print the pacifier message if there is a second arg
==================
*/
void R_ScreenShot_f (void) {

    char    checkname[MAX_OSPATH];
    static  int lastNumber = -1;
    qboolean    silent;

    if ( !strcmp( Cmd_Argv(1), "levelshot" ) ) {
        R_LevelShot();
        return;
    }

    if ( !strcmp( Cmd_Argv(1), "silent" ) ) {
        silent = qtrue;
    } else {
        silent = qfalse;
    }

    if ( Cmd_Argc() == 2 && !silent ) {
        // explicit filename
        Com_sprintf( checkname, MAX_OSPATH, "screenshots/%s.tga", Cmd_Argv( 1 ) );
    } else {
        // scan for a free filename

        // if we have saved a previous screenshot, don't scan
        // again, because recording demo avis can involve
        // thousands of shots
        if ( lastNumber == -1 ) {
            lastNumber = 0;
        }
        // scan for a free number
        for ( ; lastNumber <= 9999 ; lastNumber++ ) {
            R_ScreenshotFilename( lastNumber, checkname );
        }

        if ( lastNumber >= 9999 ) {
            Com_Printf ("ScreenShot: Couldn't create a file\n");
            return;
        }

        lastNumber++;
    }

    R_TakeScreenshot( 0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname, qfalse );

    if ( !silent ) {
        Com_Printf ("Wrote %s\n", checkname);
    }
}

void R_ScreenShotJPEG_f (void) {
    char        checkname[MAX_OSPATH];
    static  int lastNumber = -1;
    qboolean    silent;

    if ( !strcmp( Cmd_Argv(1), "levelshot" ) ) {
        R_LevelShot();
        return;
    }

    if ( !strcmp( Cmd_Argv(1), "silent" ) ) {
        silent = qtrue;
    } else {
        silent = qfalse;
    }

    if ( Cmd_Argc() == 2 && !silent ) {
        // explicit filename
        Com_sprintf( checkname, MAX_OSPATH, "screenshots/%s.jpg", Cmd_Argv( 1 ) );
    } else {
        // scan for a free filename

        // if we have saved a previous screenshot, don't scan
        // again, because recording demo avis can involve
        // thousands of shots
        if ( lastNumber == -1 ) {
            lastNumber = 0;
        }
        // scan for a free number
        for ( ; lastNumber <= 9999 ; lastNumber++ ) {
            R_ScreenshotFilenameJPEG( lastNumber, checkname );
        }

        if ( lastNumber == 10000 ) {
            Com_Printf ("ScreenShot: Couldn't create a file\n");
            return;
        }

        lastNumber++;
    }

    R_TakeScreenshot( 0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname, qtrue );

    if ( !silent ) {
        Com_Printf ("Wrote %s\n", checkname);
    }
}

/*
==================
R_ExportCubemaps
==================
*/
void R_ExportCubemaps(void)
{
    exportCubemapsCommand_t *cmd;

    cmd = R_GetCommandBuffer(sizeof(*cmd));
    if (!cmd) {
        return;
    }
    cmd->commandId = RC_EXPORT_CUBEMAPS;
}


/*
==================
R_ExportCubemaps_f
==================
*/
void R_ExportCubemaps_f(void)
{
    R_ExportCubemaps();
}

//============================================================================

/*
==================
RB_TakeVideoFrameCmd
==================
*/
const void *RB_TakeVideoFrameCmd( const void *data )
{
    assert(0);
    return NULL;
#if 0
    const videoFrameCommand_t   *cmd;
    byte                *cBuf;
    size_t              memcount, bytesPerPixel, linelen, avilinelen;
    int             padwidth, avipadwidth, padlen, avipadlen;
    int             yin, xin, xout;
    GLint packAlign, format;

    // finish any 2D drawing if needed
    if(tess.numIndexes)
        RB_EndSurface();

    cmd = (const videoFrameCommand_t *)data;

    // OpenGL ES is only required to support reading GL_RGBA
    if (glesMajorVersion >= 1) {
        format = GL_RGBA;
        bytesPerPixel = 4;
    } else {
        format = GL_RGB;
        bytesPerPixel = 3;
    }

    glGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);

    linelen = cmd->width * bytesPerPixel;

    // Alignment stuff for glReadPixels
    padwidth = PAD(linelen, packAlign);
    padlen = padwidth - linelen;
    avilinelen = cmd->width * 3;
    // AVI line padding
    avipadwidth = PAD(avilinelen, AVI_LINE_PADDING);
    avipadlen = avipadwidth - avilinelen;

    cBuf = PADP(cmd->captureBuffer, packAlign);

    glReadPixels(0, 0, cmd->width, cmd->height, format, GL_UNSIGNED_BYTE, cBuf);

    memcount = padwidth * cmd->height;

    if(cmd->motionJpeg)
    {
        // Convert RGBA to RGB, in place, line by line
        if (format == GL_RGBA) {
            linelen = cmd->width * 3;
            padlen = padwidth - linelen;

            for (yin = 0; yin < cmd->height; yin++) {
                for (xin = 0, xout = 0; xout < linelen; xin += 4, xout += 3) {
                    cBuf[yin*padwidth + xout + 0] = cBuf[yin*padwidth + xin + 0];
                    cBuf[yin*padwidth + xout + 1] = cBuf[yin*padwidth + xin + 1];
                    cBuf[yin*padwidth + xout + 2] = cBuf[yin*padwidth + xin + 2];
                }
            }
        }

        memcount = RE_SaveJPGToBuffer(cmd->encodeBuffer, avilinelen * cmd->height,
            r_aviMotionJpegQuality->integer,
            cmd->width, cmd->height, cBuf, padlen);
        ri.CL_WriteAVIVideoFrame(cmd->encodeBuffer, memcount);
    }
    else
    {
        byte *lineend, *memend;
        byte *srcptr, *destptr;

        srcptr = cBuf;
        destptr = cmd->encodeBuffer;
        memend = srcptr + memcount;

        // swap R and B and remove line paddings
        while(srcptr < memend)
        {
            lineend = srcptr + linelen;
            while(srcptr < lineend)
            {
                *destptr++ = srcptr[2];
                *destptr++ = srcptr[1];
                *destptr++ = srcptr[0];
                srcptr += bytesPerPixel;
            }

            Com_Memset(destptr, '\0', avipadlen);
            destptr += avipadlen;

            srcptr += padlen;
        }

        ri.CL_WriteAVIVideoFrame(cmd->encodeBuffer, avipadwidth * cmd->height);
    }

    return (const void *)(cmd + 1);
#endif
}

//============================================================================

/*
** GL_SetDefaultState
*/
static void GL_SetDefaultState( void )
{
    glClearDepthf( 1.0f );

    glCullFace(GL_FRONT);

    GL_BindNullTextures();

    if (glRefConfig.framebufferObject) {
        GL_BindNullFramebuffers();
    }

    GL_TextureMode( r_textureMode->string );

    //glShadeModel( GL_SMOOTH );
    glDepthFunc( GL_LEQUAL );

    //
    // make sure our GL state vector is set correctly
    //
    glState.glStateBits   = GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE;
    glState.storedGlState = 0;
    glState.faceCulling   = CT_TWO_SIDED;
    glState.faceCullFront = qtrue;

    GL_BindNullProgram();

    // OpenGL ES2.0 doesnt support vertex arrays   ... IT MAYBE DOES ....  GL_OES_vertex_array_object as an extension TODO TODO
    if (glRefConfig.vertexArrayObject) {
        glBindVertexArray(0);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glState.currentVao = NULL;
    glState.vertexAttribsEnabled = 0;

#if 0
    // TODO glPolygonMode doesn't exist not in OpenGL ES
    glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
#endif

    glDepthMask( GL_TRUE );
    glDisable( GL_DEPTH_TEST );
    glEnable( GL_SCISSOR_TEST );
    glDisable( GL_CULL_FACE );
    glDisable( GL_BLEND );

    if (glRefConfig.seamlessCubeMap) { // r_arb_seamless_cube_map  set to "0" by default
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    }

    // GL_POLYGON_OFFSET_FILL will be glEnable()d when this is used
    glPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );

    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f ); // FIXME: get color of sky
}

/*
================
R_PrintLongString
Workaround for ri.Printf's 1024 characters buffer limit.
================
*/
void R_PrintLongString(const char *string) {
    char buffer[1024];
    const char *p;
    int size = strlen(string);

    p = string;
    while(size > 0)
    {
        Q_strncpyz(buffer, p, sizeof (buffer) );
        Com_Printf( "%s", buffer );
        p += 1023;
        size -= 1023;
    }
}

/*
================
GfxInfo_f
================
*/
void GfxInfo_f( void )
{
    const char *enablestrings[] = {
        "disabled",
        "enabled"
    };

    const char *fsstrings[] = {
        "windowed",
        "fullscreen"
    };

    Com_Printf( "\nGL_VENDOR: %s\n", glConfig.vendor_string );
    Com_Printf( "GL_RENDERER: %s\n", glConfig.renderer_string );
    Com_Printf( "GL_VERSION: %s\n",  glConfig.version_string );

    // glConfig.extensions_string is a limited length so get the full list directly
    // glGetStringi not support in ES 2.0
#if 0
    if ( glGetStringi )
    {
        GLint numExtensions;
        int i;

        glGetIntegerv( GL_NUM_EXTENSIONS, &numExtensions );
        for ( i = 0; i < numExtensions; i++ )
        {
            ri.Printf( PRINT_ALL, "%s ", glGetStringi( GL_EXTENSIONS, i ) );
        }
    }
    else
    {
#endif
        R_PrintLongString( (char *) glGetString( GL_EXTENSIONS ) );
#if 0
    }
#endif

    Com_Printf( "GL_EXTENSIONS:\n" );
    Com_Printf( "GL_MAX_TEXTURE_SIZE: %d\n",        glConfig.maxTextureSize );
    Com_Printf( "GL_MAX_TEXTURE_IMAGE_UNITS: %d\n", glConfig.numTextureUnits );
    Com_Printf( "\nPIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits );
    Com_Printf( "MODE: %d, %d x %d %s hz:", r_mode->integer, glConfig.vidWidth, glConfig.vidHeight, fsstrings[r_fullscreen->integer == 1] );

    if ( glConfig.displayFrequency ) {
        Com_Printf( "%d\n", glConfig.displayFrequency );
    } else {
        Com_Printf( "N/A\n" );
    }

    Com_Printf( "texturemode: %s\n", r_textureMode->string );
    Com_Printf( "picmip: %d\n", r_picmip->integer );

#if 0
    Com_Printf( "compiled vertex arrays: %s\n", enablestrings[qglLockArraysEXT != 0 ] ); // TODO
#endif

    Com_Printf( "texenv add: %s\n",          enablestrings[glConfig.textureEnvAddAvailable != 0      ] );
    // Com_Printf( "compressed textures: %s\n", enablestrings[glConfig.textureCompression     != TC_NONE] );

    if ( r_vertexLight->integer ) {
        Com_Printf( "HACK: using vertex lightmap approximation\n" );
    }

    if ( r_finish->integer ) {
        Com_Printf( "Forcing glFinish\n" );
    }
}

/*
================
GfxMemInfo_f
================
*/
void GfxMemInfo_f( void )
{
    switch (glRefConfig.memInfo)
    {
        case MI_NONE:
        {
            Com_Printf("No extension found for GPU memory info.\n");
        }
        break;
        case MI_NVX:
        {
            int value;

            glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &value);
            Com_Printf("GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX: %ikb\n", value);

            glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &value);
            Com_Printf("GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX: %ikb\n", value);

            glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &value);
            Com_Printf("GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX: %ikb\n", value);

            glGetIntegerv(GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX, &value);
            Com_Printf("GPU_MEMORY_INFO_EVICTION_COUNT_NVX: %i\n", value);

            glGetIntegerv(GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX, &value);
            Com_Printf("GPU_MEMORY_INFO_EVICTED_MEMORY_NVX: %ikb\n", value);
        }
        break;
        case MI_ATI:
        {
            // GL_ATI_meminfo
            int value[4];

            glGetIntegerv(GL_VBO_FREE_MEMORY_ATI, &value[0]);
            Com_Printf("VBO_FREE_MEMORY_ATI: %ikb total %ikb largest aux: %ikb total %ikb largest\n", value[0], value[1], value[2], value[3]);

            glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, &value[0]);
            Com_Printf("TEXTURE_FREE_MEMORY_ATI: %ikb total %ikb largest aux: %ikb total %ikb largest\n", value[0], value[1], value[2], value[3]);

            glGetIntegerv(GL_RENDERBUFFER_FREE_MEMORY_ATI, &value[0]);
            Com_Printf("RENDERBUFFER_FREE_MEMORY_ATI: %ikb total %ikb largest aux: %ikb total %ikb largest\n", value[0], value[1], value[2], value[3]);
        }
        break;
    }
}

/*
===============
R_Register
===============
*/
void R_Register( void )
{
    //
    // latched and archived variables
    //

    r_ext_multitexture            = Cvar_Get( "r_ext_multitexture",            "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_compiled_vertex_array   = Cvar_Get( "r_ext_compiled_vertex_array",   "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_texture_env_add         = Cvar_Get( "r_ext_texture_env_add",         "1", CVAR_ARCHIVE | CVAR_LATCH );

    r_ext_framebuffer_object      = Cvar_Get( "r_ext_framebuffer_object",      "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_texture_float           = Cvar_Get( "r_ext_texture_float",           "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_framebuffer_multisample = Cvar_Get( "r_ext_framebuffer_multisample", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_arb_seamless_cube_map       = Cvar_Get( "r_arb_seamless_cube_map",       "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_arb_vertex_array_object     = Cvar_Get( "r_arb_vertex_array_object",     "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_direct_state_access     = Cvar_Get( "r_ext_direct_state_access",     "1", CVAR_ARCHIVE | CVAR_LATCH );

    r_ext_texture_filter_anisotropic = Cvar_Get( "r_ext_texture_filter_anisotropic", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_max_anisotropy             = Cvar_Get( "r_ext_max_anisotropy",             "2", CVAR_ARCHIVE | CVAR_LATCH );

    r_picmip           = Cvar_Get ( "r_picmip",          "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_roundImagesDown  = Cvar_Get ( "r_roundImagesDown", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_colorMipLevels   = Cvar_Get ( "r_colorMipLevels",  "0", CVAR_LATCH );

    Cvar_CheckRange( r_picmip, 0, 16, qtrue );

    r_detailTextures  = Cvar_Get( "r_detailtextures",  "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_multisample = Cvar_Get( "r_ext_multisample", "0", CVAR_ARCHIVE | CVAR_LATCH );

    Cvar_CheckRange( r_ext_multisample, 0, 4, qtrue );

    r_overBrightBits    = Cvar_Get( "r_overBrightBits",    "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_ignorehwgamma     = Cvar_Get( "r_ignorehwgamma",     "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_mode              = Cvar_Get( "r_mode",             "-2", CVAR_ARCHIVE | CVAR_LATCH );
    r_fullscreen        = Cvar_Get( "r_fullscreen",        "1", CVAR_ARCHIVE );
    r_noborder          = Cvar_Get( "r_noborder",          "0", CVAR_ARCHIVE | CVAR_LATCH);
    r_customwidth       = Cvar_Get( "r_customwidth",    "1600", CVAR_ARCHIVE ); // | CVAR_LATCH );
    r_customheight      = Cvar_Get( "r_customheight",   "1200", CVAR_ARCHIVE ); // | CVAR_LATCH );
    r_customPixelAspect = Cvar_Get( "r_customPixelAspect", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_simpleMipMaps     = Cvar_Get( "r_simpleMipMaps",     "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_vertexLight       = Cvar_Get( "r_vertexLight",       "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_uiFullScreen      = Cvar_Get( "r_uifullscreen",      "0", 0);
    r_subdivisions      = Cvar_Get( "r_subdivisions",      "4", CVAR_ARCHIVE | CVAR_LATCH );
    r_stereoEnabled     = Cvar_Get( "r_stereoEnabled",     "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_greyscale         = Cvar_Get( "r_greyscale",         "0", CVAR_ARCHIVE | CVAR_LATCH );

    Cvar_CheckRange(r_greyscale, 0, 1, qfalse);

    r_externalGLSL = Cvar_Get( "r_externalGLSL", "0", CVAR_LATCH ); // TODO

    r_hdr               = Cvar_Get( "r_hdr",           "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_floatLightmap     = Cvar_Get( "r_floatLightmap", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_postProcess       = Cvar_Get( "r_postProcess",   "1", CVAR_ARCHIVE );

    r_toneMap           = Cvar_Get( "r_toneMap",            "1", CVAR_ARCHIVE );
    r_forceToneMap      = Cvar_Get( "r_forceToneMap",       "0", CVAR_CHEAT );
    r_forceToneMapMin   = Cvar_Get( "r_forceToneMapMin", "-8.0", CVAR_CHEAT );
    r_forceToneMapAvg   = Cvar_Get( "r_forceToneMapAvg", "-2.0", CVAR_CHEAT );
    r_forceToneMapMax   = Cvar_Get( "r_forceToneMapMax",  "0.0", CVAR_CHEAT );

    r_autoExposure         = Cvar_Get( "r_autoExposure",            "1", CVAR_ARCHIVE );
    r_forceAutoExposure    = Cvar_Get( "r_forceAutoExposure",       "0", CVAR_CHEAT );
    r_forceAutoExposureMin = Cvar_Get( "r_forceAutoExposureMin", "-2.0", CVAR_CHEAT );
    r_forceAutoExposureMax = Cvar_Get( "r_forceAutoExposureMax",  "2.0", CVAR_CHEAT );

    r_cameraExposure = Cvar_Get( "r_cameraExposure", "1", CVAR_CHEAT );

    r_depthPrepass = Cvar_Get( "r_depthPrepass", "1", CVAR_ARCHIVE );
    r_ssao         = Cvar_Get( "r_ssao", "0", CVAR_LATCH | CVAR_ARCHIVE );

    r_normalMapping        = Cvar_Get( "r_normalMapping",           "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_specularMapping      = Cvar_Get( "r_specularMapping",         "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_deluxeMapping        = Cvar_Get( "r_deluxeMapping",           "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_parallaxMapping      = Cvar_Get( "r_parallaxMapping",         "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_parallaxMapOffset    = Cvar_Get( "r_parallaxMapOffset",       "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_parallaxMapShadows   = Cvar_Get( "r_parallaxMapShadows",      "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_cubeMapping          = Cvar_Get( "r_cubeMapping",             "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_cubemapSize          = Cvar_Get( "r_cubemapSize",           "128", CVAR_ARCHIVE | CVAR_LATCH );
    r_deluxeSpecular       = Cvar_Get( "r_deluxeSpecular",        "0.3", CVAR_ARCHIVE | CVAR_LATCH );
    r_pbr                  = Cvar_Get( "r_pbr",                     "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_baseNormalX          = Cvar_Get( "r_baseNormalX",           "1.0", CVAR_ARCHIVE | CVAR_LATCH );
    r_baseNormalY          = Cvar_Get( "r_baseNormalY",           "1.0", CVAR_ARCHIVE | CVAR_LATCH );
    r_baseParallax         = Cvar_Get( "r_baseParallax",         "0.05", CVAR_ARCHIVE | CVAR_LATCH );
    r_baseSpecular         = Cvar_Get( "r_baseSpecular",         "0.04", CVAR_ARCHIVE | CVAR_LATCH );
    r_baseGloss            = Cvar_Get( "r_baseGloss",             "0.3", CVAR_ARCHIVE | CVAR_LATCH );
    r_glossType            = Cvar_Get( "r_glossType",               "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_dlightMode           = Cvar_Get( "r_dlightMode",              "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_pshadowDist          = Cvar_Get( "r_pshadowDist",           "128", CVAR_ARCHIVE );
    r_mergeLightmaps       = Cvar_Get( "r_mergeLightmaps",          "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_imageUpsample        = Cvar_Get( "r_imageUpsample",           "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_imageUpsampleMaxSize = Cvar_Get( "r_imageUpsampleMaxSize", "1024", CVAR_ARCHIVE | CVAR_LATCH );
    r_imageUpsampleType    = Cvar_Get( "r_imageUpsampleType",       "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_genNormalMaps        = Cvar_Get( "r_genNormalMaps",           "0", CVAR_ARCHIVE | CVAR_LATCH );

    r_forceSun             = Cvar_Get( "r_forceSun",               "0", CVAR_CHEAT );
    r_forceSunLightScale   = Cvar_Get( "r_forceSunLightScale",   "1.0", CVAR_CHEAT );
    r_forceSunAmbientScale = Cvar_Get( "r_forceSunAmbientScale", "0.5", CVAR_CHEAT );
    r_drawSunRays          = Cvar_Get( "r_drawSunRays",            "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_sunlightMode         = Cvar_Get( "r_sunlightMode",           "1", CVAR_ARCHIVE | CVAR_LATCH );

    r_sunShadows           = Cvar_Get( "r_sunShadows",            "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_shadowFilter         = Cvar_Get( "r_shadowFilter",          "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_shadowBlur           = Cvar_Get( "r_shadowBlur",            "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_shadowMapSize        = Cvar_Get( "r_shadowMapSize",      "1024", CVAR_ARCHIVE | CVAR_LATCH );
    r_shadowCascadeZNear   = Cvar_Get( "r_shadowCascadeZNear",    "8", CVAR_ARCHIVE | CVAR_LATCH );
    r_shadowCascadeZFar    = Cvar_Get( "r_shadowCascadeZFar",  "1024", CVAR_ARCHIVE | CVAR_LATCH );
    r_shadowCascadeZBias   = Cvar_Get( "r_shadowCascadeZBias",    "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_ignoreDstAlpha       = Cvar_Get( "r_ignoreDstAlpha",        "1", CVAR_ARCHIVE | CVAR_LATCH );

    //
    // temporary latched variables that can only change over a restart
    //
    r_displayRefresh = Cvar_Get( "r_displayRefresh", "0", CVAR_LATCH );

    Cvar_CheckRange( r_displayRefresh, 0, 200, qtrue ); // TODO

    r_fullbright        = Cvar_Get ( "r_fullbright",        "0", CVAR_LATCH|CVAR_CHEAT );
    r_mapOverBrightBits = Cvar_Get ( "r_mapOverBrightBits", "2", CVAR_LATCH );
    r_intensity         = Cvar_Get ( "r_intensity",         "1", CVAR_LATCH );
    r_singleShader      = Cvar_Get ( "r_singleShader",      "0", CVAR_CHEAT | CVAR_LATCH );

    //
    // archived variables that can change at any time
    //
    r_lodCurveError = Cvar_Get( "r_lodCurveError", "250", CVAR_ARCHIVE|CVAR_CHEAT );
    r_lodbias       = Cvar_Get( "r_lodbias",         "0", CVAR_ARCHIVE );
    r_flares        = Cvar_Get( "r_flares",          "0", CVAR_ARCHIVE );
    r_znear         = Cvar_Get( "r_znear",           "4", CVAR_CHEAT );

    Cvar_CheckRange( r_znear, 0.001f, 200, qfalse );

    r_zproj             = Cvar_Get( "r_zproj",            "64", CVAR_ARCHIVE );
    r_stereoSeparation  = Cvar_Get( "r_stereoSeparation", "64", CVAR_ARCHIVE );
    r_ignoreGLErrors    = Cvar_Get( "r_ignoreGLErrors",    "0", CVAR_ARCHIVE ); // TODO was "1"
    r_fastsky           = Cvar_Get( "r_fastsky",           "0", CVAR_ARCHIVE );
    //r_inGameVideo     = Cvar_Get( "r_inGameVideo",       "1", CVAR_ARCHIVE );
    r_drawSun           = Cvar_Get( "r_drawSun",           "0", CVAR_ARCHIVE );
    r_dynamiclight      = Cvar_Get( "r_dynamiclight",      "1", CVAR_ARCHIVE );
    r_dlightBacks       = Cvar_Get( "r_dlightBacks",       "1", CVAR_ARCHIVE );
    r_finish            = Cvar_Get( "r_finish",            "0", CVAR_ARCHIVE );
    r_swapInterval      = Cvar_Get( "r_swapInterval",      "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_gamma             = Cvar_Get( "r_gamma",             "1", CVAR_ARCHIVE );
    r_facePlaneCull     = Cvar_Get( "r_facePlaneCull",     "1", CVAR_ARCHIVE );

    r_textureMode       = Cvar_Get( "r_textureMode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE );

    r_railWidth         = Cvar_Get( "r_railWidth",         "16", CVAR_ARCHIVE );
    r_railCoreWidth     = Cvar_Get( "r_railCoreWidth",      "6", CVAR_ARCHIVE );
    r_railSegmentLength = Cvar_Get( "r_railSegmentLength", "32", CVAR_ARCHIVE );
    r_ambientScale      = Cvar_Get( "r_ambientScale",     "0.6", CVAR_CHEAT );
    r_directedScale     = Cvar_Get( "r_directedScale",      "1", CVAR_CHEAT );
    r_anaglyphMode      = Cvar_Get( "r_anaglyphMode",       "0", CVAR_ARCHIVE );

    //
    // temporary variables that can change at any time
    //
    r_showImages          = Cvar_Get( "r_showImages",          "0", CVAR_TEMP );
    r_showDefaultImage    = Cvar_Get( "r_showDefaultImage",    "0", CVAR_TEMP );
    r_showAllShaderImages = Cvar_Get( "r_showAllShaderImages", "0", CVAR_TEMP );

    r_debugLight   = Cvar_Get( "r_debuglight",    "0", CVAR_TEMP );
    r_debugSort    = Cvar_Get( "r_debugSort",     "0", CVAR_CHEAT );
    r_printShaders = Cvar_Get( "r_printShaders",  "0", 0 );
  //r_saveFontData = Cvar_Get( "r_saveFontData",  "0", 0 ); // TODO benefit to save?  save in IDB?
    r_nocurves     = Cvar_Get( "r_nocurves",      "0", CVAR_CHEAT );
    r_drawworld    = Cvar_Get( "r_drawworld",     "1", CVAR_CHEAT );
    r_lightmap     = Cvar_Get( "r_lightmap",      "0", 0 );
    r_portalOnly   = Cvar_Get( "r_portalOnly",    "0", CVAR_CHEAT );
    r_flareSize    = Cvar_Get( "r_flareSize",    "40", CVAR_CHEAT );
    r_flareFade    = Cvar_Get( "r_flareFade",     "7", CVAR_CHEAT );
    r_flareCoeff   = Cvar_Get( "r_flareCoeff", FLARE_STDCOEFF, CVAR_CHEAT);
    r_skipBackEnd  = Cvar_Get( "r_skipBackEnd",   "0", CVAR_CHEAT );
    r_measureOverdraw = Cvar_Get( "r_measureOverdraw", "0", CVAR_CHEAT );
    r_lodscale     = Cvar_Get( "r_lodscale",      "5", CVAR_CHEAT );
    r_norefresh    = Cvar_Get( "r_norefresh",     "0", CVAR_CHEAT );
    r_drawentities = Cvar_Get( "r_drawentities",  "1", CVAR_CHEAT );
    r_ignore       = Cvar_Get( "r_ignore",        "1", CVAR_CHEAT );
    r_nocull       = Cvar_Get( "r_nocull",        "0", CVAR_CHEAT );
    r_novis        = Cvar_Get( "r_novis",         "0", CVAR_CHEAT );
    r_showcluster  = Cvar_Get( "r_showcluster",   "0", CVAR_CHEAT );
    r_speeds       = Cvar_Get( "r_speeds",        "0", CVAR_CHEAT );
    r_verbose      = Cvar_Get( "r_verbose",       "0", CVAR_CHEAT );
    r_logFile      = Cvar_Get( "r_logFile",       "0", CVAR_CHEAT );
    r_debugSurface = Cvar_Get( "r_debugSurface",  "0", CVAR_CHEAT );
    r_nobind       = Cvar_Get( "r_nobind",        "0", CVAR_CHEAT );
    r_showtris     = Cvar_Get( "r_showtris",      "0", CVAR_CHEAT );
    r_showsky      = Cvar_Get( "r_showsky",       "0", CVAR_CHEAT );
    r_shownormals  = Cvar_Get( "r_shownormals",   "0", CVAR_CHEAT );
    r_clear        = Cvar_Get( "r_clear",         "0", CVAR_CHEAT );
    r_offsetFactor = Cvar_Get( "r_offsetfactor", "-1", CVAR_CHEAT );
    r_offsetUnits  = Cvar_Get( "r_offsetunits",  "-2", CVAR_CHEAT );
    r_drawBuffer   = Cvar_Get( "r_drawBuffer", "GL_BACK", CVAR_CHEAT );
    r_lockpvs      = Cvar_Get( "r_lockpvs",       "0", CVAR_CHEAT );
    r_noportals    = Cvar_Get( "r_noportals",     "0", CVAR_CHEAT );
    r_shadows      = Cvar_Get( "cg_shadows",      "1", 0 );

    r_marksOnTriangleMeshes = Cvar_Get("r_marksOnTriangleMeshes",  "0", CVAR_ARCHIVE );
    r_screenshotJpegQuality = Cvar_Get("r_screenshotJpegQuality", "90", CVAR_ARCHIVE );

    r_maxpolys     = Cvar_Get( "r_maxpolys",     va("%d", MAX_POLYS),     0); // TODO 600
    r_maxpolyverts = Cvar_Get( "r_maxpolyverts", va("%d", MAX_POLYVERTS), 0); // TODO 3000

    // make sure all the commands added here are also
    // removed in R_Shutdown
    Cmd_AddCommand( "imagelist", R_ImageList_f );
    Cmd_AddCommand( "shaderlist", R_ShaderList_f );
    Cmd_AddCommand( "skinlist", R_SkinList_f );
    Cmd_AddCommand( "modellist", R_Modellist_f );
    Cmd_AddCommand( "screenshot", R_ScreenShot_f );
    Cmd_AddCommand( "screenshotJPEG", R_ScreenShotJPEG_f );
    Cmd_AddCommand( "gfxinfo", GfxInfo_f );
    Cmd_AddCommand( "minimize", GLimp_Minimize );
    Cmd_AddCommand( "gfxmeminfo", GfxMemInfo_f );
    Cmd_AddCommand( "exportCubemaps", R_ExportCubemaps_f );

    void ShowImages(void); void HideImages(void);
    Cmd_AddCommand( "+showimages", ShowImages );
    Cmd_AddCommand( "-showimages", HideImages );

    void ShowAllShaderImages(void); void HideAllShaderImages(void);
    Cmd_AddCommand( "+showAllShaderImages", ShowAllShaderImages );
    Cmd_AddCommand( "-showAllShaderImages", HideAllShaderImages );


    void ShowDefaultImage(void); void HideDefaultImage(void);
    Cmd_AddCommand( "+showdefaultimage", ShowDefaultImage );
    Cmd_AddCommand( "-showdefaultimage", HideDefaultImage );

    void IncDefaultImage(void); void DecDefaultImage(void);
    Cmd_AddCommand( "incDefaultImage", IncDefaultImage );
    Cmd_AddCommand( "decDefaultImage", DecDefaultImage );

}

void ShowAllShaderImages(void) {
    Cvar_Set("r_showAllShaderImages", "1");
}

void HideAllShaderImages(void) {
    Cvar_Set("r_showAllShaderImages", "0");
}

void IncDefaultImage(void) {
    cvar_t *cvar = Cvar_Get("defaultImageNumber", "0", 0);

    int value = cvar->integer;
    value++;

    if ( value > tr.numImages ) {
        value = 0;
    }

    Com_Printf("setting defaultImageNumber to %d\n", value);

    Cvar_Set("defaultImageNumber", va("%d", value));
}

void DecDefaultImage(void) {
    cvar_t *cvar = Cvar_Get("defaultImageNumber", "0", 0);

    int value = cvar->integer;
    value--;

    if ( value < 0 ) {
        value = tr.numImages - 1;
    }

    Com_Printf("setting defaultImageNumber to %d\n", value);

    Cvar_Set("defaultImageNumber", va("%d", value));

}

void ShowImages(void) {
    Cvar_Set("r_showImages", "1");
}

void HideImages(void) {
    Cvar_Set("r_showImages", "0");
}

void ShowDefaultImage(void) {
    Cvar_Set("r_showDefaultImage", "1");
}

void HideDefaultImage(void) {
    Cvar_Set("r_showDefaultImage", "0");
}

void R_InitQueries(void)
{
    if ( ! glRefConfig.occlusionQuery ) {
        return;
    }

    if (r_drawSunRays->integer) {
        glGenQueries(ARRAY_LEN(tr.sunFlareQuery), tr.sunFlareQuery);
    }
}

void R_ShutDownQueries(void)
{
    if ( ! glRefConfig.occlusionQuery ) {
        return;
    }

    if (r_drawSunRays->integer) {
        glDeleteQueries(ARRAY_LEN(tr.sunFlareQuery), tr.sunFlareQuery);
    }
}

/*
===============
R_Init
===============
*/
void R_Init( void ) {

    Com_Printf( "\n----- R_Init -----\n" );

    // clear all our internal state
    Com_Memset( &tr,      0, sizeof( tr ) );     // tr.worldMapLoaded = 0
    Com_Memset( &backEnd, 0, sizeof( backEnd ) );
    Com_Memset( &tess,    0, sizeof( tess ) );

    if( sizeof(glconfig_t) != 11312 ) {
        Com_Error( ERR_FATAL, "Mod ABI incompatible: sizeof(glconfig_t) == %u != 11312", (unsigned int) sizeof(glconfig_t));
    }

    if ( (intptr_t)tess.xyz & 15 ) {
        Com_Error( ERR_FATAL, "tess.xyz not 16 byte aligned\n" );
    }

    //
    // init function tables
    //
    for ( int i = 0; i < FUNCTABLE_SIZE; i++ )
    {
        tr.sinTable[i]             = sin( DEG2RAD( i * 360.0f / ( ( float ) ( FUNCTABLE_SIZE - 1 ) ) ) );
        tr.squareTable[i]          = ( i < FUNCTABLE_SIZE/2 ) ? 1.0f : -1.0f;
        tr.sawToothTable[i]        = (float)i / FUNCTABLE_SIZE;
        tr.inverseSawToothTable[i] = 1.0f - tr.sawToothTable[i];

        if ( i < FUNCTABLE_SIZE / 2 )
        {
            if ( i < FUNCTABLE_SIZE / 4 )
            {
                tr.triangleTable[i] = ( float ) i / ( FUNCTABLE_SIZE / 4 );
            }
            else
            {
                tr.triangleTable[i] = 1.0f - tr.triangleTable[i-FUNCTABLE_SIZE / 4];
            }
        }
        else
        {
            tr.triangleTable[i] = -tr.triangleTable[i-FUNCTABLE_SIZE/2];
        }
    }

    R_InitFogTable();

    R_NoiseInit();

    R_Register();

    max_polys = r_maxpolys->integer;
    if (max_polys < MAX_POLYS) {
        max_polys = MAX_POLYS;
    }

    max_polyverts = r_maxpolyverts->integer;
    if (max_polyverts < MAX_POLYVERTS) {
        max_polyverts = MAX_POLYVERTS;
    }

    byte *ptr = Hunk_Alloc( sizeof( *backEndData ) + sizeof(srfPoly_t) * max_polys + sizeof(polyVert_t) * max_polyverts, h_low);

    backEndData            = (backEndData_t *) ptr;                                                                  // TODO possible alignment fault
    backEndData->polys     = (srfPoly_t *)  ((char *) ptr + sizeof( *backEndData ));                                 // TODO possible alignment fault
    backEndData->polyVerts = (polyVert_t *) ((char *) ptr + sizeof( *backEndData ) + sizeof(srfPoly_t) * max_polys); // TODO possible alignment fault

    R_InitNextFrame();

    InitOpenGL();

    R_InitImages();

    if (glRefConfig.framebufferObject) {
        FBO_Init();
    }

    GLSL_InitGPUShaders();

    R_InitVaos();

    R_InitSkins();

    R_ModelInit();

    R_InitQueries();

    R_InitShaders();
}

void R_Init2(void) {
    void R_InitShaders2(void);
    R_InitShaders2();

    GfxInfo_f();

    Com_Printf( "----- finished R_Init -----\n" );

}

/*
===============
RE_Shutdown
===============
*/
void RE_Shutdown( qboolean destroyWindow ) {

    Com_Printf( "RE_Shutdown( %i )\n", destroyWindow );

    Cmd_RemoveCommand( "imagelist" );
    Cmd_RemoveCommand( "shaderlist" );
    Cmd_RemoveCommand( "skinlist" );
    Cmd_RemoveCommand( "modellist" );
    Cmd_RemoveCommand( "screenshot" );
    Cmd_RemoveCommand( "screenshotJPEG" );
    Cmd_RemoveCommand( "gfxinfo" );
    Cmd_RemoveCommand( "minimize" );
    Cmd_RemoveCommand( "gfxmeminfo" );
    Cmd_RemoveCommand( "exportCubemaps" );

    if ( tr.registered ) {
        R_IssuePendingRenderCommands();
        R_ShutDownQueries();
#if 1
        if (glRefConfig.framebufferObject) {
            FBO_Shutdown();
        }
#endif
        R_DeleteTextures();
        R_ShutdownVaos();
        GLSL_ShutdownGPUShaders();
    }

    // shut down platform specific OpenGL stuff
    if ( destroyWindow ) {
        GLimp_Shutdown();

        Com_Memset(    &glConfig, 0, sizeof(    glConfig ) ); // TODO
        Com_Memset( &glRefConfig, 0, sizeof( glRefConfig ) );

        textureFilterAnisotropic = qfalse;
        maxAnisotropy = 0;

        Com_Memset( &glState, 0, sizeof( glState ) );
    }

    tr.registered = qfalse;
    tr.worldMapLoaded = qfalse; // TODO test
}

/*
=============
RE_EndRegistration
Touch all images to make sure they are resident
=============
*/
void RE_EndRegistration( void ) {
    R_IssuePendingRenderCommands();

    if ( ! Sys_LowPhysicalMemory() ) {
        RB_ShowImages();
    }

    // TODO check validity of loaded shaders/images
}

