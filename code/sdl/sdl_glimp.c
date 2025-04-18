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

#include <stdio.h>
#include <stdlib.h>

#define GL_GLEXT_PROTOTYPES 1

#include "../qcommon/q_shared_client.h"
#include "../qcommon/qcommon_client.h"
#include "../renderercommon/tr_common.h"
#include "../renderergl2/tr_local.h"

#define   GL_VERSION_ATLEAST(major,minor)( glMajorVersion               > major || ( glMajorVersion               == major && glMinorVersion               >= minor ) )
#define GLES_VERSION_ATLEAST(major,minor)( glesMajorVersion             > major || ( glesMajorVersion             == major && glesMinorVersion             >= minor ) )
#define GLSL_VERSION_ATLEAST(major,minor)( glRefConfig.glslMajorVersion > major || ( glRefConfig.glslMajorVersion == major && glRefConfig.glslMinorVersion >= minor ) )

          SDL_Window   *SDL_window = NULL;
static SDL_GLContext SDL_glContext = NULL;

int glMajorVersion,   glMinorVersion;
int glesMajorVersion, glesMinorVersion;

extern glRefConfig_t glRefConfig;

/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown( void )
{
    IN_Shutdown();
    SDL_QuitSubSystem( SDL_INIT_VIDEO );
}

/*
===============
GLimp_Minimize
Minimize the game so that user is back at the desktop
===============
*/
void GLimp_Minimize( void ) // TODO remove me
{
    SDL_MinimizeWindow( SDL_window );
}

/*
===============
GLimp_LogComment
===============
*/
void GLimp_LogComment( char *comment )
{
    // printf("%s\n", comment); // TODO could help in debugging..
}

/*
===============
GLimp_GetOpenGLVersion
Get versions for OpenGL functions.
===============
*/
static void GLimp_GetOpenGLVersion( void ) {

    const char *version = (const char *)glGetString( GL_VERSION );

    if ( ! version ) {
        Com_Error( ERR_FATAL, "GL_VERSION is NULL" );
    }

    //Com_Printf("GL_VERSION:'%s'\n", version);

    if ( ! strstr ( version, "WebGL 2.0" ) ) {
        Com_Error ( ERR_FATAL, "Failed to get WebGL 2.0 context");
    }

    if ( Q_stricmpn( "OpenGL ES", version, 9 ) == 0 ) {
        char profile[6]; // ES, ES-CM, or ES-CL
        sscanf( version, "OpenGL %5s %d.%d", profile, &glesMajorVersion, &glesMinorVersion );
        glMajorVersion = glesMajorVersion; // TODO
        glMinorVersion = glesMinorVersion; // TODO
        // common lite profile (no floating point) is not supported
        if ( Q_stricmp( profile, "ES-CL" ) == 0 ) {
            glesMajorVersion = 0;
            glesMinorVersion = 0;
        }
    } else {
        sscanf( version, "%d.%d", &glMajorVersion, &glMinorVersion );
    }

    if ( GL_VERSION_ATLEAST( 3, 0 ) ) {
        //Com_Printf("OpenGL is at least 3.0\n");
    } else {
        Com_Error(ERR_FATAL, "OpenGL version not supported");
    }

    if ( GLES_VERSION_ATLEAST( 3, 0 ) ) {
        //Com_Printf("OpenGL ES is at least 3.0\n");
    } else {
        Com_Error( ERR_FATAL, "Unsupported OpenGL ES Version (%s), OpenGL ES 3.0 is required", version );
    }

    version = (const char *)glGetString( GL_SHADING_LANGUAGE_VERSION );

    if ( Q_stricmpn( "OpenGL ES GLSL ES", version, 17 ) == 0 ) {
        //OpenGL ES GLSL ES 3.00 (WebGL GLSL ES 3.00)

        sscanf ( version, "OpenGL ES GLSL ES %d.%d", &glRefConfig.glslMajorVersion, &glRefConfig.glslMinorVersion );

        if ( GLSL_VERSION_ATLEAST ( 3, 0 ) ) {
            //Com_Printf("GLSL version is at least 3.0\n");
        } else {
            Com_Error ( ERR_FATAL, "GLSL version is not 3.0, %s\n", version );
        }
    } else {
        Com_Error ( ERR_FATAL, "version not OpenGL ES GLSL ES '%s'", version );
    }

    assert(glMajorVersion   >= 3);
    assert(glesMajorVersion >= 3);
    assert(glRefConfig.glslMajorVersion >= 3);

    Com_Printf("%s   glMajorVersion: %d   glMinorVersion: %d\n", __func__, glMajorVersion,     glMinorVersion);
    Com_Printf("%s glesMajorVersion: %d glesMinorVersion: %d\n", __func__, glesMajorVersion, glesMinorVersion);
    Com_Printf("%s glslMajorVersion: %d glslMinorVersion: %d\n", __func__, glRefConfig.glslMajorVersion, glRefConfig.glslMinorVersion);
}

/*
===============
GLimp_SetMode
===============
*/

static int GLimp_SetMode(int mode, qboolean fullscreen)
{
    // Com_Printf( "Initializing OpenGL display\n");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    // https://wiki.libsdl.org/SDL2/SDL_GLattr

    SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "1" ); // didnt seem to do anything or maybe 1 is the default

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); // working
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 2); // working

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); // working

    SDL_GL_SetAttribute(SDL_GL_STEREO, 0); // working? NOT WORKING but could be disabled in code
    // SDL_GL_STEREO

    // these are ignored
    // SDL_GL_SetAttribute(SDL_GL_ACCUM_RED_SIZE,    4 ); // how many color bits
    // SDL_GL_SetAttribute(SDL_GL_ACCUM_GREEN_SIZE,  4 );
    // SDL_GL_SetAttribute(SDL_GL_ACCUM_BLUE_SIZE,   4 );
    // SDL_GL_SetAttribute(SDL_GL_ACCUM_ALPHA_SIZE,  4 );

    extern cvar_t *r_customwidth;
    extern cvar_t *r_customheight;

    int customwidth  = r_customwidth->integer  >= 320 ? r_customwidth->integer  : 1600;
    int customheight = r_customheight->integer >= 200 ? r_customheight->integer : 1200;

    Com_Printf("r_customwidth:  %d\n", customwidth);
    Com_Printf("r_customheight: %d\n", customheight);

    // 320x240 works.  renders at that resolution then stretches it to the size of the canvas :)
    glConfig.vidWidth      = customwidth;
    glConfig.vidHeight     = customheight;
    glConfig.windowAspect  = (float)glConfig.vidWidth / (float)glConfig.vidHeight;
    glConfig.isFullscreen  = qfalse; // not hooked up
    glConfig.stereoEnabled = qfalse; // does seem to work, but seperation isn't high enough... doesn't appear to do anything in x64 client

    int x = 0; int y = 0;
    if( ( SDL_window = SDL_CreateWindow( CLIENT_WINDOW_TITLE, x, y, glConfig.vidWidth, glConfig.vidHeight, SDL_WINDOW_OPENGL ) ) == NULL )
    {
        Com_Error( ERR_FATAL, "SDL_CreateWindow failed: %s\n", SDL_GetError( ) );
    }

    //Com_Printf("Trying to get an OpenGL ES 2.0 context\n");
    if ((SDL_glContext = SDL_GL_CreateContext(SDL_window)) == NULL) {
        Com_Error(ERR_FATAL, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
    }

    glClearColor( 0, 0, 0, 1 );
    glClear( GL_COLOR_BUFFER_BIT );

    glEnable(GL_MULTISAMPLE);

    SDL_GL_SwapWindow( SDL_window );

    int realColorBits[3];
    SDL_GL_GetAttribute( SDL_GL_RED_SIZE,     &realColorBits[0] );
    SDL_GL_GetAttribute( SDL_GL_GREEN_SIZE,   &realColorBits[1] );
    SDL_GL_GetAttribute( SDL_GL_BLUE_SIZE,    &realColorBits[2] );
    SDL_GL_GetAttribute( SDL_GL_DEPTH_SIZE,   &glConfig.depthBits );
    SDL_GL_GetAttribute( SDL_GL_STENCIL_SIZE, &glConfig.stencilBits );

    glConfig.colorBits = realColorBits[0] + realColorBits[1] + realColorBits[2];
    Com_Printf( "Using %d color bits, %d depth, %d stencil display.\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits );


    int accumColorSize[4];
    SDL_GL_GetAttribute( SDL_GL_ACCUM_RED_SIZE,    &accumColorSize[0] );
    SDL_GL_GetAttribute( SDL_GL_ACCUM_GREEN_SIZE,  &accumColorSize[1] );
    SDL_GL_GetAttribute( SDL_GL_ACCUM_BLUE_SIZE,   &accumColorSize[2] );
    SDL_GL_GetAttribute( SDL_GL_ACCUM_ALPHA_SIZE,  &accumColorSize[3] );
    //Com_Printf("accumulator size red:%d, green:%d, blue:%d\n", accumColorSize[0], accumColorSize[1], accumColorSize[2], accumColorSize[3] );
    assert(accumColorSize[0] + accumColorSize[1] + accumColorSize[2] + accumColorSize[3] == 0);// notify if ever changes

    return qtrue;
}

/*
===============
GLimp_StartDriverAndSetMode
===============
*/
static qboolean GLimp_StartDriverAndSetMode(int mode, qboolean fullscreen )
{
    if ( ! ( SDL_WasInit(SDL_INIT_VIDEO ) & SDL_INIT_VIDEO ) ) {
        //Com_Printf("SDL_WasInit(SDL_INIT_VIDEO) false\n");

        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            //Com_Printf( "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n", SDL_GetError());
            return qfalse;
        }

        Com_Printf( "SDL_Init succeeded. SDL using driver \"%s\"\n", SDL_GetCurrentVideoDriver( ) );
    } else {
        //Com_Printf("SDL already init\n");
    }

    GLimp_SetMode(mode, fullscreen);

    GLimp_GetOpenGLVersion();

    return qtrue;
}

/*
===============
GLimp_InitExtensions

extensions available ..

EXT_color_buffer_float
EXT_float_blend
EXT_texture_compression_bptc
EXT_texture_compression_rgtc
EXT_texture_filter_anisotropic
GL_EXT_color_buffer_float
GL_EXT_float_blend
GL_EXT_texture_compression_bptc
GL_EXT_texture_compression_rgtc
GL_EXT_texture_filter_anisotropic
GL_OES_texture_float_linear
GL_WEBGL_compressed_texture_etc
GL_WEBGL_compressed_texture_s3tc
GL_WEBGL_compressed_texture_s3tc_srgb
GL_WEBGL_debug_renderer_info
GL_WEBGL_debug_shaders
GL_WEBGL_lose_context
OES_texture_float_linear
WEBGL_compressed_texture_etc
WEBGL_compressed_texture_s3tc
WEBGL_compressed_texture_s3tc_srgb
WEBGL_debug_renderer_info
WEBGL_debug_shaders
WEBGL_lose_context

===============
*/
static void GLimp_InitExtensions( )
{
    Com_Printf( "Initializing OpenGL extensions\n" );

    glConfig.textureEnvAddAvailable = qfalse;
    textureFilterAnisotropic        = qfalse;

    if ( SDL_GL_ExtensionSupported( "GL_EXT_texture_filter_anisotropic" ) )
    {
        if ( r_ext_texture_filter_anisotropic->integer ) {
            glGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint *)&maxAnisotropy );
            if ( maxAnisotropy <= 0 ) {
                Com_Printf( "...GL_EXT_texture_filter_anisotropic not properly supported!\n" );
                maxAnisotropy = 0;
            }
            else
            {
                Com_Printf( "...using GL_EXT_texture_filter_anisotropic (max: %i)\n", maxAnisotropy );
                textureFilterAnisotropic = qtrue;
            }
        }
        else
        {
            Com_Printf( "...ignoring GL_EXT_texture_filter_anisotropic\n" );
        }
    }
    else
    {
        Com_Printf( "...GL_EXT_texture_filter_anisotropic not found\n" );
    }

}

/*
===============
GLimp_Init
This routine is responsible for initializing the OS specific portions of OpenGL
===============
*/
void GLimp_Init( )
{
    Com_Printf( "Glimp_Init( )\n" );

    // Create the window and set up the context
    if( ! GLimp_StartDriverAndSetMode(r_mode->integer, r_fullscreen->integer)) {
        Com_Error( ERR_FATAL, "GLimp_Init() - could not create context not falling back" );
    }

    Com_Printf("OpenGL version %d.%d\n", glMajorVersion, glMinorVersion);

    // get our config strings
    Q_strncpyz( glConfig.vendor_string,   (char *) glGetString (GL_VENDOR),   sizeof( glConfig.vendor_string ) );
    Q_strncpyz( glConfig.renderer_string, (char *) glGetString (GL_RENDERER), sizeof( glConfig.renderer_string ) );

    if (*glConfig.renderer_string && glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] == '\n') {
        glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] = 0;
    }

    Q_strncpyz( glConfig.version_string,    (char *) glGetString (GL_VERSION),    sizeof( glConfig.version_string ) );
    Q_strncpyz( glConfig.extensions_string, (char *) glGetString (GL_EXTENSIONS), sizeof( glConfig.extensions_string ) );

    // initialize extensions
    GLimp_InitExtensions( );

    // This depends on SDL_INIT_VIDEO, hence having it here
    IN_Init( SDL_window );

    if ( ! GL_VERSION_ATLEAST( 3, 0 ) ) {
        Com_Error(ERR_FATAL, "opengl version not 3.0 or better");
    }
}

/*
===============
GLimp_EndFrame
Responsible for doing a swapbuffers
===============
*/
void GLimp_EndFrame( void )
{
    // don't flip if drawing to front buffer
    if ( qtrue || Q_stricmp( r_drawBuffer->string, "GL_FRONT" ) != 0 ) {
        SDL_GL_SwapWindow( SDL_window );
    }

    if( qfalse && r_fullscreen->modified )
    {
        int         fullscreen;
        qboolean    needToToggle;
        qboolean    sdlToggled = qfalse;

        // Find out the current state
        fullscreen = !!( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_FULLSCREEN );

        // Is the state we want different from the current state?
        needToToggle = !!r_fullscreen->integer != fullscreen;

        if( needToToggle )
        {
            sdlToggled = SDL_SetWindowFullscreen( SDL_window, r_fullscreen->integer ) >= 0;

            // SDL_WM_ToggleFullScreen didn't work, so do it the slow way
            if( !sdlToggled ) {
                Com_Error(ERR_FATAL,"Do I need to do this?"); // vid restart????
                Cbuf_ExecuteText(EXEC_APPEND, "vid_restart\n"); // TODO changed from cmd_executetext
            }

            IN_Restart( );
        }

        r_fullscreen->modified = qfalse;
    }
}

