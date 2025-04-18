/*
===========================================================================
Copyright (C) 2011 James Canete (use.less01@gmail.com)

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
// tr_extensions.c - extensions needed by the renderer not in sdl_glimp.c

#include <SDL2/SDL.h>

#include "tr_local.h"
// #include "tr_dsa.h"

extern int glMajorVersion, glMinorVersion;
extern int glesMajorVersion, glesMinorVersion;
#define GL_VERSION_ATLEAST( major, minor )   ( glMajorVersion > major || ( glMajorVersion == major && glMinorVersion >= minor ) )

void GLimp_InitExtraExtensions(void)
{
    Com_Printf("Initializing OpenGL extra extensions\n");

    char *extension = NULL;
    const char* result[3] = {
        "... IGNORING %s\n",
        "... USING %s\n",
        "...%s NOT found\n"
    };

    Com_Printf("supported GL extensions:\n%s\n", glGetString(GL_EXTENSIONS));

    // EXT_color_buffer_float
    // EXT_float_blend
    // EXT_texture_compression_bptc
    // EXT_texture_compression_rgtc
    // EXT_texture_filter_anisotropic
    // GL_EXT_color_buffer_float
    // GL_EXT_float_blend
    // GL_EXT_texture_compression_bptc
    // GL_EXT_texture_compression_rgtc
    // GL_EXT_texture_filter_anisotropic
    // GL_OES_texture_float_linear
    // GL_WEBGL_compressed_texture_etc
    // GL_WEBGL_compressed_texture_s3tc
    // GL_WEBGL_compressed_texture_s3tc_srgb
    // GL_WEBGL_debug_renderer_info
    // GL_WEBGL_debug_shaders
    // GL_WEBGL_lose_context // TODO
    // OES_texture_float_linear
    // WEBGL_compressed_texture_etc
    // WEBGL_compressed_texture_s3tc
    // WEBGL_compressed_texture_s3tc_srgb
    // WEBGL_debug_renderer_info
    // WEBGL_debug_shaders
    // WEBGL_lose_context

    glRefConfig.depthClamp             = qfalse;// gemini said not exposed to webgl2

    glRefConfig.framebufferBlit        = qtrue;
    glRefConfig.framebufferMultisample = qtrue;
    glRefConfig.framebufferObject      = qtrue;
    glRefConfig.occlusionQuery         = qtrue;
    glRefConfig.textureFloat           = qtrue;
    glRefConfig.vertexArrayObject      = qtrue;
    glRefConfig.readStencil            = qtrue;
    glRefConfig.seamlessCubeMap        = qtrue;
    glRefConfig.shadowSamplers         = qtrue;
    glRefConfig.standardDerivatives    = qtrue;
    glRefConfig.swizzleNormalmap       = qtrue;
    // glRefConfig.readDepth              = qfalse; // nowhere in code

    glRefConfig.memInfo            = MI_NONE;

	if (glesMajorVersion) {
		glRefConfig.vaoCacheGlIndexType = GL_UNSIGNED_SHORT;
		glRefConfig.vaoCacheGlIndexSize = sizeof(unsigned short);
	} else {
		glRefConfig.vaoCacheGlIndexType = GL_UNSIGNED_INT;
		glRefConfig.vaoCacheGlIndexSize = sizeof(unsigned int);
	}

    if ( glesMajorVersion ) {
        extension = "GL_EXT_occlusion_query_boolean";

        if ( glesMajorVersion >= 3 || SDL_GL_ExtensionSupported ( extension ) ) {
            glRefConfig.occlusionQuery = qtrue;
            glRefConfig.occlusionQueryTarget = GL_ANY_SAMPLES_PASSED;
        }

        extension = "GL_NV_read_depth";
        if ( SDL_GL_ExtensionSupported(extension) ) {
            glRefConfig.readDepth = qtrue;
        }

        extension = "GL_NV_read_stencil";
        if ( SDL_GL_ExtensionSupported(extension)) {
            glRefConfig.readStencil = qtrue;
        }

        extension = "GL_EXT_shadow_samplers";
        if ( glesMajorVersion >= 3 || SDL_GL_ExtensionSupported(extension)) {
            glRefConfig.shadowSamplers = qtrue;
        }

        extension = "GL_OES_standard_derivatives";
        if ( glesMajorVersion >= 3 || SDL_GL_ExtensionSupported(extension)) {
            glRefConfig.standardDerivatives = qtrue;
        }

        extension = "GL_OES_element_index_unit";
        if ( glesMajorVersion >= 3 || SDL_GL_ExtensionSupported(extension)) {
            glRefConfig.vaoCacheGlIndexType = GL_UNSIGNED_INT;
            glRefConfig.vaoCacheGlIndexSize = sizeof(unsigned int);
        }
    }

    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &glRefConfig.maxRenderbufferSize);
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &glRefConfig.maxColorAttachments);

    // OpenGL 3.2 - GL_ARB_seamless_cube_map
    extension = "GL_ARB_seamless_cube_map";
    if ( SDL_GL_ExtensionSupported(extension) ) {
        glRefConfig.seamlessCubeMap = !!r_arb_seamless_cube_map->integer;
        Com_Printf(result[glRefConfig.seamlessCubeMap], extension);
    } else {
        Com_Printf(result[2], extension);
    }

    if ( 1 ) {
        Com_Printf("glRefConfig:\n");
        Com_Printf("%s:%d\n", "glslMajorVersion",       glRefConfig.glslMajorVersion );
        Com_Printf("%s:%d\n", "glslMinorVersion",       glRefConfig.glslMinorVersion );
        Com_Printf("%s:%d\n", "framebufferObject",      glRefConfig.framebufferObject );
        Com_Printf("%s:%d\n", "framebufferBlit",        glRefConfig.framebufferBlit );
        Com_Printf("%s:%d\n", "framebufferMultisample", glRefConfig.framebufferMultisample );
        Com_Printf("%s:%d\n", "maxRenderbufferSize",    glRefConfig.maxRenderbufferSize );
        Com_Printf("%s:%d\n", "maxColorAttachments",    glRefConfig.maxColorAttachments );
        Com_Printf("%s:%d\n", "vertexArrayObject",      glRefConfig.vertexArrayObject );
        Com_Printf("%s:%d\n", "occlusionQuery",         glRefConfig.occlusionQuery );
        Com_Printf("%s:%d\n", "depthClamp",             glRefConfig.depthClamp );
        Com_Printf("%s:%d\n", "readDepth",              glRefConfig.readDepth );
        Com_Printf("%s:%d\n", "readStencil",            glRefConfig.readStencil );
        Com_Printf("%s:%d\n", "shadowSamplers",         glRefConfig.shadowSamplers );
        Com_Printf("%s:%d\n", "standardDerivatives",    glRefConfig.standardDerivatives );
    }

    Com_Printf("Initializing OpenGL extra extensions complete.\n");
}

