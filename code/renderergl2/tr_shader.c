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
#include "tr_local.h"
#include "../renderercommon/tr_common.h"
#include "tr_shader_info.h"

// tr_shader.c -- this file deals with the parsing and definition of shaders
//
// api:
//
//  R_InitShaders
//  R_InitShaders2
//
//  RE_RegisterShader
//  RE_RegisterShaderNoMip
//  R_FindShader
//  R_GetShaderByHandle
//  R_RemapShader
//
//  everything else is static/private

qboolean FS_GetPreLoadedFile(const char *filename, void **data, int *length);
extern char map_name[MAX_STRING_CHARS];
static char *s_shaderText = NULL;

extern int numImageLoadsInFlight;

void DumpShader(shader_t *shader);
// the shader is parsed into these global variables, then copied into
// dynamically allocated memory if it is valid.

// static  shaderStage_t   stages[MAX_SHADER_STAGES];// moved into shader_t as _stages
// static  shader_t        shader;
// static  texModInfo_t    texMods[MAX_SHADER_STAGES][TR_MAX_TEXMODS];
// static  int             shader_realLightmapIndex; // moved into shader_t

#define FILE_HASH_SIZE      1024
static  shader_t*       hashTable[FILE_HASH_SIZE];

#define MAX_SHADERTEXT_HASH     2048
static char **shaderTextHashTable[MAX_SHADERTEXT_HASH];

#define SHADER_NOT_ACTIVE do{ if(!shader->stages[0]->active){Com_Printf("%s shader %s stages[0] not active\n", __func__, shader->name);assert(0);}}while(0)
/*
================
return a hash value for the filename
================
*/

static long generateHashValue( const char *fname, const int size ) {
    long hash = 0;
    int i = 0;
    while (fname[i] != '\0') {
        char letter = tolower(fname[i]);
        if (letter =='.') break;                // don't include extension
        if (letter =='\\') letter = '/';        // damn path names
        if (letter == PATH_SEP) letter = '/';   // damn path names
        hash+=(long)(letter)*(i+119);
        i++;
    }
    hash = (hash ^ (hash >> 10) ^ (hash >> 20));
    hash &= (size-1);
    return hash;
}

static shader_t *R_FindShaderByName( const char *name );
static qhandle_t RE_RegisterShaderLightMap( const char *name, int lightmapIndex );

/*
  R_RemapShader
  called from cgame/cg_servercmds.c
    CG_ServerCommand
    CG_ShaderStateChanged
  called from renderergl2/tr_bsp.c
    R_LoadIdentities
*/
void R_RemapShader(const char *shaderName, const char *newShaderName, const char *timeOffset) {
    char        strippedName[MAX_QPATH];
    int         hash;
    shader_t    *sh, *sh2;
    qhandle_t   h;

    sh = R_FindShaderByName( shaderName );
    if (sh == NULL || sh == tr.defaultShader) {
        h = RE_RegisterShaderLightMap(shaderName, 0);
        sh = R_GetShaderByHandle(h);
    }

    if (sh == NULL || sh == tr.defaultShader) {
        Com_Printf( "WARNING: R_RemapShader: shader %s not found\n", shaderName );
        return;
    }

    sh2 = R_FindShaderByName( newShaderName );
    if (sh2 == NULL || sh2 == tr.defaultShader) {
        h = RE_RegisterShaderLightMap(newShaderName, 0);
        sh2 = R_GetShaderByHandle(h);
    }

    if (sh2 == NULL || sh2 == tr.defaultShader) {
        Com_Printf( "WARNING: R_RemapShader: new shader %s not found\n", newShaderName );
        return;
    }

    // remap all the shaders with the given name
    // even tho they might have different lightmaps
    COM_StripExtension(shaderName, strippedName, sizeof(strippedName));
    hash = generateHashValue(strippedName, FILE_HASH_SIZE);
    for (sh = hashTable[hash]; sh; sh = sh->next) {
        if (Q_stricmp(sh->name, strippedName) == 0) {
            if (sh != sh2) {
                sh->remappedShader = sh2;
            } else {
                sh->remappedShader = NULL;
            }
        }
    }

    if (timeOffset) {
        sh2->timeOffset = atof(timeOffset);
    }
}

/*
===============
ParseVector

no loading
===============
*/
static qboolean ParseVector( const char *shader_name, char **text, int count, float *v ) {
    // FIXME: spaces are currently required after parens, should change parseext...
    char *token = COM_ParseExt( text, qfalse );

    if ( strcmp( token, "(" ) ) {
        Com_Printf( "WARNING: missing parenthesis in shader '%s'\n", shader_name );
        return qfalse;
    }

    for ( int i = 0 ; i < count ; i++ ) {
        token = COM_ParseExt( text, qfalse );
        if ( !token[0] ) {
            Com_Printf( "WARNING: missing vector element in shader '%s'\n", shader_name );
            return qfalse;
        }
        v[i] = atof( token );
    }

    token = COM_ParseExt( text, qfalse );
    if ( strcmp( token, ")" ) ) {
        Com_Printf( "WARNING: missing parenthesis in shader '%s'\n", shader_name );
        return qfalse;
    }

    return qtrue;
}

/*
===============
NameToAFunc
===============
*/
static unsigned NameToAFunc( const char *shader_name, const char *funcname )
{
    if ( !Q_stricmp( funcname, "GT0" ) )
    {
        return GLS_ATEST_GT_0;
    }
    else if ( !Q_stricmp( funcname, "LT128" ) )
    {
        return GLS_ATEST_LT_80;
    }
    else if ( !Q_stricmp( funcname, "GE128" ) )
    {
        return GLS_ATEST_GE_80;
    }

    Com_Printf( "WARNING: invalid alphaFunc name '%s' in shader '%s'\n", funcname, shader_name );
    return 0;
}

/*
===============
NameToSrcBlendMode
===============
*/
static int NameToSrcBlendMode( const char *shader_name, const char *name )
{
    if ( !Q_stricmp( name, "GL_ONE" ) )
    {
        return GLS_SRCBLEND_ONE;
    }
    else if ( !Q_stricmp( name, "GL_ZERO" ) )
    {
        return GLS_SRCBLEND_ZERO;
    }
    else if ( !Q_stricmp( name, "GL_DST_COLOR" ) )
    {
        return GLS_SRCBLEND_DST_COLOR;
    }
    else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_COLOR" ) )
    {
        return GLS_SRCBLEND_ONE_MINUS_DST_COLOR;
    }
    else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
    {
        return GLS_SRCBLEND_SRC_ALPHA;
    }
    else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
    {
        return GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA;
    }
    else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
    {
        if (r_ignoreDstAlpha->integer) {
            return GLS_SRCBLEND_ONE;
        }

        return GLS_SRCBLEND_DST_ALPHA;
    }
    else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
    {
        if (r_ignoreDstAlpha->integer) {
            return GLS_SRCBLEND_ZERO;
        }

        return GLS_SRCBLEND_ONE_MINUS_DST_ALPHA;
    }
    else if ( !Q_stricmp( name, "GL_SRC_ALPHA_SATURATE" ) )
    {
        return GLS_SRCBLEND_ALPHA_SATURATE;
    }

    Com_Printf( "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader_name );
    return GLS_SRCBLEND_ONE;
}

/*
===============
NameToDstBlendMode
===============
*/
static int NameToDstBlendMode( const char *shader_name, const char *name )
{
    if ( !Q_stricmp( name, "GL_ONE" ) )
    {
        return GLS_DSTBLEND_ONE;
    }
    else if ( !Q_stricmp( name, "GL_ZERO" ) )
    {
        return GLS_DSTBLEND_ZERO;
    }
    else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
    {
        return GLS_DSTBLEND_SRC_ALPHA;
    }
    else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
    {
        return GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
    }
    else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
    {
        if (r_ignoreDstAlpha->integer) {
            return GLS_DSTBLEND_ONE;
        }

        return GLS_DSTBLEND_DST_ALPHA;
    }
    else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
    {
        if (r_ignoreDstAlpha->integer) {
            return GLS_DSTBLEND_ZERO;
        }

        return GLS_DSTBLEND_ONE_MINUS_DST_ALPHA;
    }
    else if ( !Q_stricmp( name, "GL_SRC_COLOR" ) )
    {
        return GLS_DSTBLEND_SRC_COLOR;
    }
    else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_COLOR" ) )
    {
        return GLS_DSTBLEND_ONE_MINUS_SRC_COLOR;
    }

    Com_Printf( "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader_name );
    return GLS_DSTBLEND_ONE;
}

/*
===============
NameToGenFunc
===============
*/
static genFunc_t NameToGenFunc( const char *shader_name, const char *funcname )
{
    if ( !Q_stricmp( funcname, "sin" ) )
    {
        return GF_SIN;
    }
    else if ( !Q_stricmp( funcname, "square" ) )
    {
        return GF_SQUARE;
    }
    else if ( !Q_stricmp( funcname, "triangle" ) )
    {
        return GF_TRIANGLE;
    }
    else if ( !Q_stricmp( funcname, "sawtooth" ) )
    {
        return GF_SAWTOOTH;
    }
    else if ( !Q_stricmp( funcname, "inversesawtooth" ) )
    {
        return GF_INVERSE_SAWTOOTH;
    }
    else if ( !Q_stricmp( funcname, "noise" ) )
    {
        return GF_NOISE;
    }

    Com_Printf( "WARNING: invalid genfunc name '%s' in shader '%s'\n", funcname, shader_name );
    return GF_SIN;
}

/*
===================
ParseWaveForm
no loading
===================
*/
static void ParseWaveForm( const char *shader_name, char **text, waveForm_t *wave )
{
    char *token = COM_ParseExt( text, qfalse );

    if ( token[0] == 0 )
    {
        Com_Printf( "WARNING: missing waveform parm in shader '%s'\n", shader_name );
        return;
    }

    wave->func = NameToGenFunc( shader_name, token );

    // BASE, AMP, PHASE, FREQ
    token = COM_ParseExt( text, qfalse );
    if ( token[0] == 0 )
    {
        Com_Printf( "WARNING: missing waveform parm in shader '%s'\n", shader_name );
        return;
    }

    wave->base = atof( token );
    token = COM_ParseExt( text, qfalse );

    if ( token[0] == 0 )
    {
        Com_Printf( "WARNING: missing waveform parm in shader '%s'\n", shader_name );
        return;
    }

    wave->amplitude = atof( token );
    token = COM_ParseExt( text, qfalse );

    if ( token[0] == 0 )
    {
        Com_Printf( "WARNING: missing waveform parm in shader '%s'\n", shader_name );
        return;
    }

    wave->phase = atof( token );
    token = COM_ParseExt( text, qfalse );

    if ( token[0] == 0 )
    {
        Com_Printf( "WARNING: missing waveform parm in shader '%s'\n", shader_name );
        return;
    }

    wave->frequency = atof( token );
}

/*
===================
ParseTexMod
no loading
===================
*/
static void ParseTexMod( const char *shader_name, char *_text, shaderStage_t *stage )
{
    const char *token;
    char **text = &_text;
    texModInfo_t *tmi;

    if ( stage->bundle[0].numTexMods == TR_MAX_TEXMODS ) {
        Com_Error( ERR_DROP, "ERROR: too many tcMod stages in shader '%s'", shader_name );
        return;
    }

    tmi = &stage->bundle[0].texMods[stage->bundle[0].numTexMods];
    stage->bundle[0].numTexMods++;

    token = COM_ParseExt( text, qfalse );

    //
    // turb
    //
    if ( !Q_stricmp( token, "turb" ) )
    {
        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing tcMod turb parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->wave.base = atof( token );
        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing tcMod turb in shader '%s'\n", shader_name );
            return;
        }
        tmi->wave.amplitude = atof( token );
        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing tcMod turb in shader '%s'\n", shader_name );
            return;
        }
        tmi->wave.phase = atof( token );
        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing tcMod turb in shader '%s'\n", shader_name );
            return;
        }
        tmi->wave.frequency = atof( token );

        tmi->type = TMOD_TURBULENT;
    }
    //
    // scale
    //
    else if ( !Q_stricmp( token, "scale" ) )
    {
        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing scale parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->scale[0] = atof( token );

        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing scale parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->scale[1] = atof( token );
        tmi->type = TMOD_SCALE;
    }
    //
    // scroll
    //
    else if ( !Q_stricmp( token, "scroll" ) )
    {
        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing scale scroll parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->scroll[0] = atof( token );
        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing scale scroll parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->scroll[1] = atof( token );
        tmi->type = TMOD_SCROLL;
    }
    //
    // stretch
    //
    else if ( !Q_stricmp( token, "stretch" ) )
    {
        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing stretch parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->wave.func = NameToGenFunc( shader_name, token );

        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing stretch parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->wave.base = atof( token );

        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing stretch parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->wave.amplitude = atof( token );

        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing stretch parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->wave.phase = atof( token );

        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing stretch parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->wave.frequency = atof( token );

        tmi->type = TMOD_STRETCH;
    }
    //
    // transform
    //
    else if ( !Q_stricmp( token, "transform" ) )
    {
        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing transform parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->matrix[0][0] = atof( token );

        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing transform parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->matrix[0][1] = atof( token );

        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing transform parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->matrix[1][0] = atof( token );

        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing transform parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->matrix[1][1] = atof( token );

        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing transform parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->translate[0] = atof( token );

        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing transform parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->translate[1] = atof( token );

        tmi->type = TMOD_TRANSFORM;
    }
    //
    // rotate
    //
    else if ( !Q_stricmp( token, "rotate" ) )
    {
        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing tcMod rotate parms in shader '%s'\n", shader_name );
            return;
        }
        tmi->rotateSpeed = atof( token );
        tmi->type = TMOD_ROTATE;
    }
    //
    // entityTranslate
    //
    else if ( !Q_stricmp( token, "entityTranslate" ) )
    {
        tmi->type = TMOD_ENTITY_TRANSLATE;
    }
    else
    {
        Com_Printf( "WARNING: unknown tcMod '%s' in shader '%s'\n", token, shader_name );
    }
}

/*
===================
ParseStage
3 R_FindImageFile calls
===================
*/
static qboolean ParseStage( shader_t* shader, shaderStage_t *stage, int stage_num, char **text )
{
    char *token;
    int depthMaskBits = GLS_DEPTHMASK_TRUE, blendSrcBits = 0, blendDstBits = 0, atestBits = 0, depthFuncBits = 0;
    qboolean depthMaskExplicit = qfalse;

    stage->active = qtrue;

    while ( 1 )
    {
        token = COM_ParseExt( text, qtrue );
        if ( !token[0] )
        {
            Com_Printf( "WARNING: no matching '}' found\n" );
            return qfalse;
        }

        if ( token[0] == '}' )
        {
            break;
        }
        //
        // map <name>
        //
        else if ( !Q_stricmp( token, "map" ) )
        {
            token = COM_ParseExt( text, qfalse );
            if ( !token[0] )
            {
                Com_Printf( "WARNING: missing parameter for 'map' keyword in shader '%s'\n", shader->name );
                return qfalse;
            }

            if ( !Q_stricmp( token, "$whiteimage" ) )
            {
                stage->bundle[0].image[0] = tr.whiteImage;
                continue;
            }
            else if ( !Q_stricmp( token, "$lightmap" ) )
            {
                stage->bundle[0].isLightmap = qtrue;
                if ( shader->lightmapIndex < 0 || !tr.lightmaps ) {
                    stage->bundle[0].image[0] = tr.whiteImage;
                } else {
                    stage->bundle[0].image[0] = tr.lightmaps[shader->lightmapIndex];
                }
                continue;
            }
            else if ( !Q_stricmp( token, "$deluxemap" ) )
            {
                if (!tr.worldDeluxeMapping)
                {
                    Com_Printf( "WARNING: shader '%s' wants a deluxe map in a map compiled without them\n", shader->name );
                    return qfalse;
                }

                stage->bundle[0].isLightmap = qtrue;
                if ( shader->lightmapIndex < 0 ) {
                    stage->bundle[0].image[0] = tr.whiteImage;
                } else {
                    stage->bundle[0].image[0] = tr.deluxemaps[shader->lightmapIndex];
                }
                continue;
            }
            else
            {
                imgType_t   type = IMGTYPE_COLORALPHA;
                imgFlags_t flags = IMGFLAG_NONE;

                if (!shader->noMipMaps) {
                    flags |= IMGFLAG_MIPMAP;
                }

                if (!shader->noPicMip) {
                    flags |= IMGFLAG_PICMIP;
                }

                if (stage->type == ST_NORMALMAP || stage->type == ST_NORMALPARALLAXMAP)
                {
                    type = IMGTYPE_NORMAL;
                    flags |= IMGFLAG_NOLIGHTSCALE;

                    if (stage->type == ST_NORMALPARALLAXMAP) {
                        type = IMGTYPE_NORMALHEIGHT;
                    }
                }
                else
                {
                    if (r_genNormalMaps->integer) {
                        flags |= IMGFLAG_GENNORMALMAP;
                    }
                }

                stage->bundle[0].image[0] = R_FindImageFile( token, type, flags ); // callback NOT needed

            }
        }
        //
        // clampmap <name>
        //
        else if ( !Q_stricmp( token, "clampmap" ) )
        {
            imgType_t   type = IMGTYPE_COLORALPHA;
            imgFlags_t flags = IMGFLAG_CLAMPTOEDGE;

            token = COM_ParseExt( text, qfalse );
            if ( !token[0] )
            {
                Com_Printf( "WARNING: missing parameter for 'clampmap' keyword in shader '%s'\n", shader->name );
                return qfalse;
            }

            if (!shader->noMipMaps) {
                flags |= IMGFLAG_MIPMAP;
            }

            if (!shader->noPicMip) {
                flags |= IMGFLAG_PICMIP;
            }

            if (stage->type == ST_NORMALMAP || stage->type == ST_NORMALPARALLAXMAP)
            {
                 type  = IMGTYPE_NORMAL;
                flags |= IMGFLAG_NOLIGHTSCALE;

                if (stage->type == ST_NORMALPARALLAXMAP) {
                    type = IMGTYPE_NORMALHEIGHT;
                }
            }
            else
            {
                if (r_genNormalMaps->integer) {
                    flags |= IMGFLAG_GENNORMALMAP;
                }
            }

            stage->bundle[0].image[0] = R_FindImageFile( token, type, flags ); // callback NOT needed

        }
        //
        // animMap <frequency> <image1> .... <imageN>
        //
        else if ( !Q_stricmp( token, "animMap" ) )
        {
            int totalImages = 0;

            token = COM_ParseExt( text, qfalse );
            if ( !token[0] )
            {
                Com_Printf( "WARNING: missing parameter for 'animMap' keyword in shader '%s'\n", shader->name );
                return qfalse;
            }
            stage->bundle[0].imageAnimationSpeed = atof( token );

            // parse up to MAX_IMAGE_ANIMATIONS animations
            while ( 1 ) {
                int     num;

                token = COM_ParseExt( text, qfalse );
                if ( !token[0] ) {
                    break;
                }

                num = stage->bundle[0].numImageAnimations;

                if ( num < MAX_IMAGE_ANIMATIONS ) {
                    imgFlags_t flags = IMGFLAG_NONE;

                    if (!shader->noMipMaps) {
                        flags |= IMGFLAG_MIPMAP;
                    }

                    if (!shader->noPicMip) {
                        flags |= IMGFLAG_PICMIP;
                    }

                    stage->bundle[0].image[num] = R_FindImageFile( token, IMGTYPE_COLORALPHA, flags );
                    if ( stage->bundle[0].image[num] )  {
                        stage->bundle[0].numImageAnimations++;
                    }
                }
                totalImages++;
            }

            if ( totalImages > MAX_IMAGE_ANIMATIONS ) {
                assert(0);
                Com_Printf( "WARNING: ignoring excess images for 'animMap' (found %d, max is %d) in shader '%s'\n", totalImages, MAX_IMAGE_ANIMATIONS, shader->name );
            }
        }
        else if ( qfalse && qfalse && !Q_stricmp( token, "videoMap" ) ) // TODO disabled for CIN_PlayCinematic
        {
            token = COM_ParseExt( text, qfalse );
            if ( !token[0] )
            {
                Com_Printf( "WARNING: missing parameter for 'videoMap' keyword in shader '%s'\n", shader->name );
                return qfalse;
            }
            stage->bundle[0].videoMapHandle = 0; //ri.CIN_PlayCinematic( token, 0, 0, 256, 256, (CIN_loop | CIN_silent | CIN_shader)); // TODO
            if (stage->bundle[0].videoMapHandle != -1) {
                stage->bundle[0].isVideoMap = qtrue;
                stage->bundle[0].image[0] = tr.scratchImage[stage->bundle[0].videoMapHandle];
            } else {
                Com_Printf( "WARNING: could not load '%s' for 'videoMap' keyword in shader '%s'\n", token, shader->name );
            }
        }
        //
        // alphafunc <func>
        //
        else if ( !Q_stricmp( token, "alphaFunc" ) )
        {
            token = COM_ParseExt( text, qfalse );
            if ( !token[0] )
            {
                Com_Printf( "WARNING: missing parameter for 'alphaFunc' keyword in shader '%s'\n", shader->name );
                return qfalse;
            }

            atestBits = NameToAFunc( shader->name, token );
        }
        //
        // depthFunc <func>
        //
        else if ( !Q_stricmp( token, "depthfunc" ) )
        {
            token = COM_ParseExt( text, qfalse );

            if ( !token[0] )
            {
                Com_Printf( "WARNING: missing parameter for 'depthfunc' keyword in shader '%s'\n", shader->name );
                return qfalse;
            }

            if ( !Q_stricmp( token, "lequal" ) )
            {
                depthFuncBits = 0;
            }
            else if ( !Q_stricmp( token, "equal" ) )
            {
                depthFuncBits = GLS_DEPTHFUNC_EQUAL;
            }
            else
            {
                Com_Printf( "WARNING: unknown depthfunc '%s' in shader '%s'\n", token, shader->name );
                continue;
            }
        }
        //
        // detail
        //
        else if ( !Q_stricmp( token, "detail" ) )
        {
            stage->isDetail = qtrue;
        }
        //
        // blendfunc <srcFactor> <dstFactor>
        // or blendfunc <add|filter|blend>
        //
        else if ( !Q_stricmp( token, "blendfunc" ) )
        {
            token = COM_ParseExt( text, qfalse );
            if ( token[0] == 0 )
            {
                Com_Printf( "WARNING: missing parm for blendFunc in shader '%s'\n", shader->name );
                continue;
            }
            // check for "simple" blends first
            if ( !Q_stricmp( token, "add" ) ) {
                blendSrcBits = GLS_SRCBLEND_ONE;
                blendDstBits = GLS_DSTBLEND_ONE;
            } else if ( !Q_stricmp( token, "filter" ) ) {
                blendSrcBits = GLS_SRCBLEND_DST_COLOR;
                blendDstBits = GLS_DSTBLEND_ZERO;
            } else if ( !Q_stricmp( token, "blend" ) ) {
                blendSrcBits = GLS_SRCBLEND_SRC_ALPHA;
                blendDstBits = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
            } else {
                // complex double blends
                blendSrcBits = NameToSrcBlendMode( shader->name, token );

                token = COM_ParseExt( text, qfalse );
                if ( token[0] == 0 )
                {
                    Com_Printf( "WARNING: missing parm for blendFunc in shader '%s'\n", shader->name );
                    continue;
                }
                blendDstBits = NameToDstBlendMode( shader->name, token );
            }

            // clear depth mask for blended surfaces
            if ( !depthMaskExplicit )
            {
                depthMaskBits = 0;
            }
        }
        //
        // stage <type>
        //
        else if(!Q_stricmp(token, "stage"))
        {
            token = COM_ParseExt(text, qfalse);
            if(token[0] == 0)
            {
                Com_Printf("WARNING: missing parameters for stage in shader '%s'\n", shader->name);
                continue;
            }

            if(!Q_stricmp(token, "diffuseMap"))
            {
                stage->type = ST_DIFFUSEMAP;
            }
            else if(!Q_stricmp(token, "normalMap") || !Q_stricmp(token, "bumpMap"))
            {
                stage->type = ST_NORMALMAP;
                VectorSet4(stage->normalScale, r_baseNormalX->value, r_baseNormalY->value, 1.0f, r_baseParallax->value);
            }
            else if(!Q_stricmp(token, "normalParallaxMap") || !Q_stricmp(token, "bumpParallaxMap"))
            {
                if (r_parallaxMapping->integer) {
                    stage->type = ST_NORMALPARALLAXMAP;
                } else {
                    stage->type = ST_NORMALMAP;
                }
                VectorSet4(stage->normalScale, r_baseNormalX->value, r_baseNormalY->value, 1.0f, r_baseParallax->value);
            }
            else if(!Q_stricmp(token, "specularMap"))
            {
                stage->type = ST_SPECULARMAP;
                VectorSet4(stage->specularScale, 1.0f, 1.0f, 1.0f, 1.0f);
            }
            else
            {
                Com_Printf("WARNING: unknown stage parameter '%s' in shader '%s'\n", token, shader->name);
                continue;
            }
        }
        //
        // specularReflectance <value>
        //
        else if (!Q_stricmp(token, "specularreflectance"))
        {
            token = COM_ParseExt(text, qfalse);
            if ( token[0] == 0 )
            {
                Com_Printf( "WARNING: missing parameter for specular reflectance in shader '%s'\n", shader->name );
                continue;
            }

            if (r_pbr->integer)
            {
                // interpret specularReflectance < 0.5 as nonmetal
                stage->specularScale[1] = (atof(token) < 0.5f) ? 0.0f : 1.0f;
            }
            else
            {
                stage->specularScale[0] =
                    stage->specularScale[1] =
                    stage->specularScale[2] = atof( token );
            }
        }
        //
        // specularExponent <value>
        //
        else if (!Q_stricmp(token, "specularexponent"))
        {
            float exponent;

            token = COM_ParseExt(text, qfalse);
            if ( token[0] == 0 )
            {
                Com_Printf( "WARNING: missing parameter for specular exponent in shader '%s'\n", shader->name );
                continue;
            }
            exponent = atof( token );

            if (r_pbr->integer) {
                stage->specularScale[0] = 1.0f - powf(2.0f / (exponent + 2.0), 0.25);
            }
            else
            {
                // Change shininess to gloss
                // Assumes max exponent of 8190 and min of 0, must change here if altered in lightall_fp.glsl
                exponent = CLAMP(exponent, 0.0f, 8190.0f);
                stage->specularScale[3] = (log2f(exponent + 2.0f) - 1.0f) / 12.0f;
            }
        }
        //
        // gloss <value>
        //
        else if (!Q_stricmp(token, "gloss"))
        {
            float gloss;

            token = COM_ParseExt(text, qfalse);
            if ( token[0] == 0 )
            {
                Com_Printf( "WARNING: missing parameter for gloss in shader '%s'\n", shader->name );
                continue;
            }

            gloss = atof(token);

            if (r_pbr->integer) {
                stage->specularScale[0] = 1.0f - exp2f(-3.0f * gloss);
            } else {
                stage->specularScale[3] = gloss;
            }
        }
        //
        // roughness <value>
        //
        else if (!Q_stricmp(token, "roughness"))
        {
            float roughness;

            token = COM_ParseExt(text, qfalse);
            if (token[0] == 0)
            {
                Com_Printf("WARNING: missing parameter for roughness in shader '%s'\n", shader->name);
                continue;
            }

            roughness = atof(token);

            if (r_pbr->integer) {
                stage->specularScale[0] = 1.0 - roughness;
            } else {
                if (roughness >= 0.125) {
                    stage->specularScale[3] = log2f(1.0f / roughness) / 3.0f;
                } else {
                    stage->specularScale[3] = 1.0f;
                }
            }
        }
        //
        // parallaxDepth <value>
        //
        else if (!Q_stricmp(token, "parallaxdepth"))
        {
            token = COM_ParseExt(text, qfalse);
            if ( token[0] == 0 )
            {
                Com_Printf( "WARNING: missing parameter for parallaxDepth in shader '%s'\n", shader->name );
                continue;
            }

            stage->normalScale[3] = atof( token );
        }
        //
        // normalScale <xy>
        // or normalScale <x> <y>
        // or normalScale <x> <y> <height>
        //
        else if (!Q_stricmp(token, "normalscale"))
        {
            token = COM_ParseExt(text, qfalse);
            if ( token[0] == 0 )
            {
                Com_Printf( "WARNING: missing parameter for normalScale in shader '%s'\n", shader->name );
                continue;
            }

            stage->normalScale[0] = atof( token );

            token = COM_ParseExt(text, qfalse);
            if ( token[0] == 0 )
            {
                // one value, applies to X/Y
                stage->normalScale[1] = stage->normalScale[0];
                continue;
            }

            stage->normalScale[1] = atof( token );

            token = COM_ParseExt(text, qfalse);
            if ( token[0] == 0 )
            {
                // two values, no height
                continue;
            }

            stage->normalScale[3] = atof( token );
        }
        //
        // specularScale <rgb> <gloss>
        // or specularScale <metallic> <smoothness> with r_pbr 1
        // or specularScale <r> <g> <b>
        // or specularScale <r> <g> <b> <gloss>
        //
        else if (!Q_stricmp(token, "specularscale"))
        {
            token = COM_ParseExt(text, qfalse);
            if ( token[0] == 0 )
            {
                Com_Printf( "WARNING: missing parameter for specularScale in shader '%s'\n", shader->name );
                continue;
            }

            stage->specularScale[0] = atof( token );

            token = COM_ParseExt(text, qfalse);
            if ( token[0] == 0 )
            {
                Com_Printf( "WARNING: missing parameter for specularScale in shader '%s'\n", shader->name );
                continue;
            }

            stage->specularScale[1] = atof( token );

            token = COM_ParseExt(text, qfalse);
            if ( token[0] == 0 )
            {
                if (r_pbr->integer)
                {
                    // two values, metallic then smoothness
                    float smoothness = stage->specularScale[1];
                    stage->specularScale[1] = (stage->specularScale[0] < 0.5f) ? 0.0f : 1.0f;
                    stage->specularScale[0] = smoothness;
                }
                else
                {
                    // two values, rgb then gloss
                    stage->specularScale[3] = stage->specularScale[1];
                    stage->specularScale[1] =
                        stage->specularScale[2] = stage->specularScale[0];
                }
                continue;
            }

            stage->specularScale[2] = atof( token );

            token = COM_ParseExt(text, qfalse);
            if ( token[0] == 0 )
            {
                // three values, rgb
                continue;
            }

            stage->specularScale[3] = atof( token );

        }
        //
        // rgbGen
        //
        else if ( !Q_stricmp( token, "rgbGen" ) )
        {
            token = COM_ParseExt( text, qfalse );
            if ( token[0] == 0 )
            {
                Com_Printf( "WARNING: missing parameters for rgbGen in shader '%s'\n", shader->name );
                continue;
            }

            if ( !Q_stricmp( token, "wave" ) )
            {
                ParseWaveForm( shader->name, text, &stage->rgbWave );
                stage->rgbGen = CGEN_WAVEFORM;
            }
            else if ( !Q_stricmp( token, "const" ) )
            {
                vec3_t  color;

                VectorClear( color );

                ParseVector( shader->name, text, 3, color );
                stage->constantColor[0] = 255 * color[0];
                stage->constantColor[1] = 255 * color[1];
                stage->constantColor[2] = 255 * color[2];

                stage->rgbGen = CGEN_CONST;
            }
            else if ( !Q_stricmp( token, "identity" ) )
            {
                stage->rgbGen = CGEN_IDENTITY;
            }
            else if ( !Q_stricmp( token, "identityLighting" ) )
            {
                stage->rgbGen = CGEN_IDENTITY_LIGHTING;
            }
            else if ( !Q_stricmp( token, "entity" ) )
            {
                stage->rgbGen = CGEN_ENTITY;
            }
            else if ( !Q_stricmp( token, "oneMinusEntity" ) )
            {
                stage->rgbGen = CGEN_ONE_MINUS_ENTITY;
            }
            else if ( !Q_stricmp( token, "vertex" ) )
            {
                stage->rgbGen = CGEN_VERTEX;
                if ( stage->alphaGen == 0 ) {
                    stage->alphaGen = AGEN_VERTEX;
                }
            }
            else if ( !Q_stricmp( token, "exactVertex" ) )
            {
                stage->rgbGen = CGEN_EXACT_VERTEX;
            }
            else if ( !Q_stricmp( token, "vertexLit" ) )
            {
                stage->rgbGen = CGEN_VERTEX_LIT;
                if ( stage->alphaGen == 0 ) {
                    stage->alphaGen = AGEN_VERTEX;
                }
            }
            else if ( !Q_stricmp( token, "exactVertexLit" ) )
            {
                stage->rgbGen = CGEN_EXACT_VERTEX_LIT;
            }
            else if ( !Q_stricmp( token, "lightingDiffuse" ) )
            {
                stage->rgbGen = CGEN_LIGHTING_DIFFUSE;
            }
            else if ( !Q_stricmp( token, "oneMinusVertex" ) )
            {
                stage->rgbGen = CGEN_ONE_MINUS_VERTEX;
            }
            else
            {
                Com_Printf( "WARNING: unknown rgbGen parameter '%s' in shader '%s'\n", token, shader->name );
                continue;
            }
        }
        //
        // alphaGen
        //
        else if ( !Q_stricmp( token, "alphaGen" ) )
        {
            token = COM_ParseExt( text, qfalse );
            if ( token[0] == 0 )
            {
                Com_Printf( "WARNING: missing parameters for alphaGen in shader '%s'\n", shader->name );
                continue;
            }

            if ( !Q_stricmp( token, "wave" ) )
            {
                ParseWaveForm( shader->name, text, &stage->alphaWave );
                stage->alphaGen = AGEN_WAVEFORM;
            }
            else if ( !Q_stricmp( token, "const" ) )
            {
                token = COM_ParseExt( text, qfalse );
                stage->constantColor[3] = 255 * atof( token );
                stage->alphaGen = AGEN_CONST;
            }
            else if ( !Q_stricmp( token, "identity" ) )
            {
                stage->alphaGen = AGEN_IDENTITY;
            }
            else if ( !Q_stricmp( token, "entity" ) )
            {
                stage->alphaGen = AGEN_ENTITY;
            }
            else if ( !Q_stricmp( token, "oneMinusEntity" ) )
            {
                stage->alphaGen = AGEN_ONE_MINUS_ENTITY;
            }
            else if ( !Q_stricmp( token, "vertex" ) )
            {
                stage->alphaGen = AGEN_VERTEX;
            }
            else if ( !Q_stricmp( token, "lightingSpecular" ) )
            {
                stage->alphaGen = AGEN_LIGHTING_SPECULAR;
            }
            else if ( !Q_stricmp( token, "oneMinusVertex" ) )
            {
                stage->alphaGen = AGEN_ONE_MINUS_VERTEX;
            }
            else if ( !Q_stricmp( token, "portal" ) )
            {
                stage->alphaGen = AGEN_PORTAL;
                token = COM_ParseExt( text, qfalse );
                if ( token[0] == 0 )
                {
                    shader->portalRange = 256;
                    Com_Printf( "WARNING: missing range parameter for alphaGen portal in shader '%s', defaulting to 256\n", shader->name );
                }
                else
                {
                    shader->portalRange = atof( token );
                }
            }
            else
            {
                Com_Printf( "WARNING: unknown alphaGen parameter '%s' in shader '%s'\n", token, shader->name );
                continue;
            }
        }
        //
        // tcGen <function>
        //
        else if ( !Q_stricmp(token, "texgen") || !Q_stricmp( token, "tcGen" ) )
        {
            token = COM_ParseExt( text, qfalse );
            if ( token[0] == 0 )
            {
                Com_Printf( "WARNING: missing texgen parm in shader '%s'\n", shader->name );
                continue;
            }

            if ( !Q_stricmp( token, "environment" ) )
            {
                stage->bundle[0].tcGen = TCGEN_ENVIRONMENT_MAPPED;
            }
            else if ( !Q_stricmp( token, "lightmap" ) )
            {
                stage->bundle[0].tcGen = TCGEN_LIGHTMAP;
            }
            else if ( !Q_stricmp( token, "texture" ) || !Q_stricmp( token, "base" ) )
            {
                stage->bundle[0].tcGen = TCGEN_TEXTURE;
            }
            else if ( !Q_stricmp( token, "vector" ) )
            {
                ParseVector( shader->name, text, 3, stage->bundle[0].tcGenVectors[0] );
                ParseVector( shader->name, text, 3, stage->bundle[0].tcGenVectors[1] );

                stage->bundle[0].tcGen = TCGEN_VECTOR;
            }
            else
            {
                Com_Printf( "WARNING: unknown texgen parm in shader '%s'\n", shader->name );
            }
        }
        //
        // tcMod <type> <...>
        //
        else if ( !Q_stricmp( token, "tcMod" ) )
        {
            char buffer[1024] = "";

            while ( 1 )
            {
                token = COM_ParseExt( text, qfalse );
                if ( token[0] == 0 )
                    break;
                Q_strcat( buffer, sizeof (buffer), token );
                Q_strcat( buffer, sizeof (buffer), " " );
            }

            ParseTexMod( shader->name, buffer, stage ); // no loading

            continue;
        }
        //
        // depthmask
        //
        else if ( !Q_stricmp( token, "depthwrite" ) )
        {
            depthMaskBits = GLS_DEPTHMASK_TRUE;
            depthMaskExplicit = qtrue;

            continue;
        }
        else
        {
            Com_Printf( "WARNING: unknown parameter '%s' in shader '%s'\n", token, shader->name );
            return qfalse;
        }
    }

    //
    // if cgen isn't explicitly specified, use either identity or identitylighting
    //
    if ( stage->rgbGen == CGEN_BAD ) {
        if ( blendSrcBits == 0 || blendSrcBits == GLS_SRCBLEND_ONE || blendSrcBits == GLS_SRCBLEND_SRC_ALPHA ) {
            stage->rgbGen = CGEN_IDENTITY_LIGHTING;
        } else {
            stage->rgbGen = CGEN_IDENTITY;
        }
    }

    //
    // implicitly assume that a GL_ONE GL_ZERO blend mask disables blending
    //
    if ( ( blendSrcBits == GLS_SRCBLEND_ONE ) && ( blendDstBits == GLS_DSTBLEND_ZERO ) )
    {
        blendDstBits = blendSrcBits = 0;
        depthMaskBits = GLS_DEPTHMASK_TRUE;
    }

    // decide which agens we can skip
    if ( stage->alphaGen == AGEN_IDENTITY ) {
        if ( stage->rgbGen == CGEN_IDENTITY || stage->rgbGen == CGEN_LIGHTING_DIFFUSE ) {
            stage->alphaGen = AGEN_SKIP;
        }
    }

    //
    // compute state bits
    //
    stage->stateBits = depthMaskBits |
        blendSrcBits | blendDstBits |
        atestBits |
        depthFuncBits;

    return qtrue;
}

/*
===============
ParseDeform

deformVertexes wave <spread> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes normal <frequency> <amplitude>
deformVertexes move <vector> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes bulge <bulgeWidth> <bulgeHeight> <bulgeSpeed>
deformVertexes projectionShadow
deformVertexes autoSprite
deformVertexes autoSprite2
deformVertexes text[0-7]

no loading

===============
*/
static void ParseDeform( shader_t *shader, char **text ) {
    char    *token;
    deformStage_t   *ds;

    token = COM_ParseExt( text, qfalse );
    if ( token[0] == 0 )
    {
        Com_Printf( "WARNING: missing deform parm in shader '%s'\n", shader->name );
        return;
    }

    if ( shader->numDeforms == MAX_SHADER_DEFORMS ) {
        Com_Printf( "WARNING: MAX_SHADER_DEFORMS in '%s'\n", shader->name );
        return;
    }

    ds = &shader->deforms[ shader->numDeforms ];
    shader->numDeforms++;

    if ( !Q_stricmp( token, "projectionShadow" ) ) {
        ds->deformation = DEFORM_PROJECTION_SHADOW;
        return;
    }

    if ( !Q_stricmp( token, "autosprite" ) ) {
        ds->deformation = DEFORM_AUTOSPRITE;
        return;
    }

    if ( !Q_stricmp( token, "autosprite2" ) ) {
        ds->deformation = DEFORM_AUTOSPRITE2;
        return;
    }

    if ( !Q_stricmpn( token, "text", 4 ) ) {
        int     n;

        n = token[4] - '0';
        if ( n < 0 || n > 7 ) {
            n = 0;
        }
        ds->deformation = DEFORM_TEXT0 + n;
        return;
    }

    if ( !Q_stricmp( token, "bulge" ) ) {
        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader->name );
            return;
        }
        ds->bulgeWidth = atof( token );

        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader->name );
            return;
        }
        ds->bulgeHeight = atof( token );

        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader->name );
            return;
        }
        ds->bulgeSpeed = atof( token );

        ds->deformation = DEFORM_BULGE;
        return;
    }

    if ( !Q_stricmp( token, "wave" ) )
    {
        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing deformVertexes parm in shader '%s'\n", shader->name );
            return;
        }

        if ( atof( token ) != 0 )
        {
            ds->deformationSpread = 1.0f / atof( token );
        }
        else
        {
            ds->deformationSpread = 100.0f;
            Com_Printf( "WARNING: illegal div value of 0 in deformVertexes command for shader '%s'\n", shader->name );
        }

        ParseWaveForm( shader->name, text, &ds->deformationWave );
        ds->deformation = DEFORM_WAVE;
        return;
    }

    if ( !Q_stricmp( token, "normal" ) )
    {
        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing deformVertexes parm in shader '%s'\n", shader->name );
            return;
        }
        ds->deformationWave.amplitude = atof( token );

        token = COM_ParseExt( text, qfalse );
        if ( token[0] == 0 )
        {
            Com_Printf( "WARNING: missing deformVertexes parm in shader '%s'\n", shader->name );
            return;
        }
        ds->deformationWave.frequency = atof( token );

        ds->deformation = DEFORM_NORMALS;
        return;
    }

    if ( !Q_stricmp( token, "move" ) ) {
        int     i;

        for ( i = 0 ; i < 3 ; i++ ) {
            token = COM_ParseExt( text, qfalse );
            if ( token[0] == 0 ) {
                Com_Printf("WARNING: missing deformVertexes parm in shader '%s'\n", shader->name );
                return;
            }
            ds->moveVector[i] = atof( token );
        }

        ParseWaveForm( shader->name, text, &ds->deformationWave );
        ds->deformation = DEFORM_MOVE;
        return;
    }

    Com_Printf( "WARNING: unknown deformVertexes subtype '%s' found in shader '%s'\n", token, shader->name );
}

/*
===============
ParseSkyParms
skyParms <outerbox> <cloudheight> <innerbox>

2 R_FindImageFile calls
===============
*/
static void ParseSkyParms( shader_t *shader, char **text ) {
    char        *token;
    static char *suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
    char        pathname[MAX_QPATH];
    int         i;
    imgFlags_t imgFlags = IMGFLAG_MIPMAP | IMGFLAG_PICMIP;

    // outerbox
    token = COM_ParseExt( text, qfalse );
    if ( token[0] == 0 ) {
        Com_Printf( "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader->name );
        return;
    }
    if ( strcmp( token, "-" ) ) {
        for (i=0 ; i<6 ; i++) {
            Com_sprintf( pathname, sizeof(pathname), "%s_%s.png" , token, suf[i] );
            shader->sky.outerbox[i] = R_FindImageFile( pathname, IMGTYPE_COLORALPHA, imgFlags | IMGFLAG_CLAMPTOEDGE ); // no callback needed
            if ( ! shader->sky.outerbox[i] ) {
                shader->sky.outerbox[i] = tr.defaultImage;
            }
        }
    }

    // cloudheight
    token = COM_ParseExt( text, qfalse );
    if ( token[0] == 0 ) {
        Com_Printf( "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader->name );
        return;
    }
    shader->sky.cloudHeight = atof( token );
    if ( !shader->sky.cloudHeight ) {
        shader->sky.cloudHeight = 512;
    }

    R_InitSkyTexCoords( shader->sky.cloudHeight );

    // innerbox
    token = COM_ParseExt( text, qfalse );
    if ( token[0] == 0 ) {
        Com_Printf( "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader->name );
        return;
    }

    if ( strcmp( token, "-" ) ) {
        for (i=0 ; i<6 ; i++) {
            Com_sprintf( pathname, sizeof(pathname), "%s_%s.png" , token, suf[i] );
            shader->sky.innerbox[i] = R_FindImageFile( ( char * ) pathname, IMGTYPE_COLORALPHA, imgFlags ); // no callback needed
            if ( ! shader->sky.innerbox[i] ) {
                shader->sky.innerbox[i] = tr.defaultImage;
            }
        }
    }

    shader->isSky = qtrue;
}

/*
=================
ParseSort
no loading
=================
*/
void ParseSort( shader_t *shader, char **text ) {
    char    *token;

    token = COM_ParseExt( text, qfalse );
    if ( token[0] == 0 ) {
        Com_Printf( "WARNING: missing sort parameter in shader '%s'\n", shader->name );
        return;
    }

    if ( !Q_stricmp( token, "portal" ) ) {
        shader->sort = SS_PORTAL;
    } else if ( !Q_stricmp( token, "sky" ) ) {
        shader->sort = SS_ENVIRONMENT;
    } else if ( !Q_stricmp( token, "opaque" ) ) {
        shader->sort = SS_OPAQUE;
    }else if ( !Q_stricmp( token, "decal" ) ) {
        shader->sort = SS_DECAL;
    } else if ( !Q_stricmp( token, "seeThrough" ) ) {
        shader->sort = SS_SEE_THROUGH;
    } else if ( !Q_stricmp( token, "banner" ) ) {
        shader->sort = SS_BANNER;
    } else if ( !Q_stricmp( token, "additive" ) ) {
        shader->sort = SS_BLEND1;
    } else if ( !Q_stricmp( token, "nearest" ) ) {
        shader->sort = SS_NEAREST;
    } else if ( !Q_stricmp( token, "underwater" ) ) {
        shader->sort = SS_UNDERWATER;
    } else {
        shader->sort = atof( token );
    }
}

// this table is also present in q3map

typedef struct {
    char    *name;
    int     clearSolid, surfaceFlags, contents;
} infoParm_t;

static infoParm_t  infoParms[] = {
    // server relevant contents
    {"water",       1,  0,  CONTENTS_WATER },
    {"slime",       1,  0,  CONTENTS_SLIME },       // mildly damaging
    {"lava",        1,  0,  CONTENTS_LAVA },        // very damaging
    {"playerclip",  1,  0,  CONTENTS_PLAYERCLIP },
    {"monsterclip", 1,  0,  CONTENTS_MONSTERCLIP },
    {"nodrop",      1,  0,  CONTENTS_NODROP },      // don't drop items or leave bodies (death fog, lava, etc)
    {"nonsolid",    1,  SURF_NONSOLID,  0},                     // clears the solid flag

    // utility relevant attributes
    {"origin",      1,  0,  CONTENTS_ORIGIN },      // center of rotating brushes
    {"trans",       0,  0,  CONTENTS_TRANSLUCENT }, // don't eat contained surfaces
    {"detail",      0,  0,  CONTENTS_DETAIL },      // don't include in structural bsp
    {"structural",  0,  0,  CONTENTS_STRUCTURAL },  // force into structural bsp even if trnas
    {"areaportal",  1,  0,  CONTENTS_AREAPORTAL },  // divides areas
    {"clusterportal", 1,0,  CONTENTS_CLUSTERPORTAL },   // for bots
    {"donotenter",  1,  0,  CONTENTS_DONOTENTER },      // for bots

    {"fog",         1,  0,  CONTENTS_FOG},          // carves surfaces entering
    {"sky",         0,  SURF_SKY,       0 },        // emit light from an environment map
    {"lightfilter", 0,  SURF_LIGHTFILTER, 0 },      // filter light going through it
    {"alphashadow", 0,  SURF_ALPHASHADOW, 0 },      // test light on a per-pixel basis
    {"hint",        0,  SURF_HINT,      0 },        // use as a primary splitter

    // server attributes
    {"slick",       0,  SURF_SLICK,     0 },
    {"noimpact",    0,  SURF_NOIMPACT,  0 },        // don't make impact explosions or marks
    {"nomarks",     0,  SURF_NOMARKS,   0 },        // don't make impact marks, but still explode
    {"ladder",      0,  SURF_LADDER,    0 },
    {"nodamage",    0,  SURF_NODAMAGE,  0 },
    {"metalsteps",  0,  SURF_METALSTEPS,0 },
    {"flesh",       0,  SURF_FLESH,     0 },
    {"nosteps",     0,  SURF_NOSTEPS,   0 },

    // drawsurf attributes
    {"nodraw",      0,  SURF_NODRAW,    0 },    // don't generate a drawsurface (or a lightmap)
    {"pointlight",  0,  SURF_POINTLIGHT, 0 },   // sample lighting at vertexes
    {"nolightmap",  0,  SURF_NOLIGHTMAP,0 },    // don't generate a lightmap
    {"nodlight",    0,  SURF_NODLIGHT, 0 },     // don't ever add dynamic lights
    {"dust",        0,  SURF_DUST, 0}           // leave a dust trail when walking on this surface
};

/*
===============
ParseSurfaceParm

surfaceparm <name>

no loading
===============
*/
static void ParseSurfaceParm( shader_t *shader, char **text ) {
    char    *token;
    int     numInfoParms = ARRAY_LEN( infoParms );
    int     i;

    token = COM_ParseExt( text, qfalse );
    for ( i = 0 ; i < numInfoParms ; i++ ) {
        if ( !Q_stricmp( token, infoParms[i].name ) ) {
            shader->surfaceFlags |= infoParms[i].surfaceFlags;
            shader->contentFlags |= infoParms[i].contents;
#if 0
            if ( infoParms[i].clearSolid ) {
                si->contents &= ~CONTENTS_SOLID;
            }
#endif
            break;
        }
    }
}

/*
=================
ParseShader

The current text pointer is at the explicit text definition of the
shader.  Parse it into the global shader variable.  Later functions
will optimize it.

calls ParseStage    3 R_FindImageCalls
calls ParseSkyParms 2 R_FindImageCalls
=================
*/
static qboolean ParseShader( shader_t* shader, char **text )
{
    char *token;
    int s;

    s = 0;

    token = COM_ParseExt( text, qtrue );
    if ( token[0] != '{' )
    {
        Com_Printf( "WARNING: expecting '{', found '%s' instead in shader '%s'\n", token, shader->name );
        return qfalse;
    }

    while ( 1 )
    {
        token = COM_ParseExt( text, qtrue );
        if ( !token[0] )
        {
            Com_Printf( "WARNING: no concluding '}' in shader %s\n", shader->name );
            return qfalse;
        }

        // end of shader definition
        if ( token[0] == '}' )
        {
            break;
        }
        // stage definition
        else if ( token[0] == '{' )
        {
            if ( s >= MAX_SHADER_STAGES ) {
                Com_Printf( "WARNING: too many stages in shader %s (max is %i)\n", shader->name, MAX_SHADER_STAGES );
                return qfalse;
            }

            if ( ! ParseStage( shader, shader->stages[s], s, text ) ) // 3 R_FindImageFile calls
            {
                assert(0);
                return qfalse;
            }

            shader->stages[s]->active = qtrue;

            s++;

            continue;
        }
        // skip stuff that only the QuakeEdRadient needs
        else if ( !Q_stricmpn( token, "qer", 3 ) ) {
            SkipRestOfLine( text );
            continue;
        }
        // sun parms
        else if ( !Q_stricmp( token, "q3map_sun" ) || !Q_stricmp( token, "q3map_sunExt" ) || !Q_stricmp( token, "q3gl2_sun" ) ) {
            float   a, b;
            qboolean isGL2Sun = qfalse;

            if (!Q_stricmp( token, "q3gl2_sun" ) && r_sunShadows->integer )
            {
                isGL2Sun = qtrue;
                tr.sunShadows = qtrue;
            }

            token = COM_ParseExt( text, qfalse );
            tr.sunLight[0] = atof( token );
            token = COM_ParseExt( text, qfalse );
            tr.sunLight[1] = atof( token );
            token = COM_ParseExt( text, qfalse );
            tr.sunLight[2] = atof( token );

            VectorNormalize( tr.sunLight );

            token = COM_ParseExt( text, qfalse );
            a = atof( token );
            VectorScale( tr.sunLight, a, tr.sunLight);

            token = COM_ParseExt( text, qfalse );
            a = atof( token );
            a = a / 180 * M_PI;

            token = COM_ParseExt( text, qfalse );
            b = atof( token );
            b = b / 180 * M_PI;

            tr.sunDirection[0] = cos( a ) * cos( b );
            tr.sunDirection[1] = sin( a ) * cos( b );
            tr.sunDirection[2] = sin( b );

            if (isGL2Sun)
            {
                token = COM_ParseExt( text, qfalse );
                tr.sunShadowScale = atof(token);

                // parse twice, since older shaders may include mapLightScale before sunShadowScale
                if (token[0]) {
                    token = COM_ParseExt( text, qfalse );
                    if (token[0]) {
                        tr.sunShadowScale = atof(token);
                    }
                }
            }

            if (token[0]) {
                SkipRestOfLine( text );
            }
            continue;
        }
        // tonemap parms
        else if ( !Q_stricmp( token, "q3gl2_tonemap" ) ) {
            token = COM_ParseExt( text, qfalse );
            tr.toneMinAvgMaxLevel[0] = atof( token );
            token = COM_ParseExt( text, qfalse );
            tr.toneMinAvgMaxLevel[1] = atof( token );
            token = COM_ParseExt( text, qfalse );
            tr.toneMinAvgMaxLevel[2] = atof( token );

            token = COM_ParseExt( text, qfalse );
            tr.autoExposureMinMax[0] = atof( token );
            token = COM_ParseExt( text, qfalse );
            tr.autoExposureMinMax[1] = atof( token );

            SkipRestOfLine( text );
            continue;
        }
        else if ( !Q_stricmp( token, "deformVertexes" ) ) {
            ParseDeform( shader, text );
            continue;
        }
        else if ( !Q_stricmp( token, "tesssize" ) ) {
            SkipRestOfLine( text );
            continue;
        }
        else if ( !Q_stricmp( token, "clampTime" ) ) {
            token = COM_ParseExt( text, qfalse );
            if (token[0]) {
                shader->clampTime = atof(token);
            }
        }
        // skip stuff that only the q3map needs
        else if ( !Q_stricmpn( token, "q3map", 5 ) ) {
            SkipRestOfLine( text );
            continue;
        }
        // skip stuff that only q3map or the server needs
        else if ( !Q_stricmp( token, "surfaceParm" ) ) {
            ParseSurfaceParm( shader, text );
            continue;
        }
        // no mip maps
        else if ( !Q_stricmp( token, "nomipmaps" ) )
        {
            shader->noMipMaps = qtrue;
            shader->noPicMip = qtrue;
            continue;
        }
        // no picmip adjustment
        else if ( !Q_stricmp( token, "nopicmip" ) )
        {
            shader->noPicMip = qtrue;
            continue;
        }
        // polygonOffset
        else if ( !Q_stricmp( token, "polygonOffset" ) )
        {
            shader->polygonOffset = qtrue;
            continue;
        }
        // entityMergable, allowing sprite surfaces from multiple entities
        // to be merged into one batch.  This is a savings for smoke
        // puffs and blood, but can't be used for anything where the
        // shader calcs (not the surface function) reference the entity color or scroll
        else if ( !Q_stricmp( token, "entityMergable" ) )
        {
            shader->entityMergable = qtrue;
            continue;
        }
        // fogParms
        else if ( !Q_stricmp( token, "fogParms" ) )
        {
            if ( !ParseVector( shader->name, text, 3, shader->fogParms.color ) ) {
                return qfalse;
            }

            if ( r_greyscale->integer )
            {
                float luminance;

                luminance = LUMA( shader->fogParms.color[0], shader->fogParms.color[1], shader->fogParms.color[2] );
                VectorSet( shader->fogParms.color, luminance, luminance, luminance );
            }
            else if ( r_greyscale->value )
            {
                float luminance;

                luminance = LUMA( shader->fogParms.color[0], shader->fogParms.color[1], shader->fogParms.color[2] );
                shader->fogParms.color[0] = LERP( shader->fogParms.color[0], luminance, r_greyscale->value );
                shader->fogParms.color[1] = LERP( shader->fogParms.color[1], luminance, r_greyscale->value );
                shader->fogParms.color[2] = LERP( shader->fogParms.color[2], luminance, r_greyscale->value );
            }

            token = COM_ParseExt( text, qfalse );
            if ( !token[0] )
            {
                Com_Printf( "WARNING: missing parm for 'fogParms' keyword in shader '%s'\n", shader->name );
                continue;
            }
            shader->fogParms.depthForOpaque = atof( token );

            // skip any old gradient directions
            SkipRestOfLine( text );
            continue;
        }
        // portal
        else if ( !Q_stricmp(token, "portal") )
        {
            shader->sort = SS_PORTAL;
            shader->isPortal = qtrue;
            continue;
        }
        // skyparms <cloudheight> <outerbox> <innerbox>
        else if ( !Q_stricmp( token, "skyparms" ) )
        {
            ParseSkyParms( shader, text ); // 2 R_FindImageFile calls
            continue;
        }
        // light <value> determines flaring in q3map, not needed here
        else if ( !Q_stricmp(token, "light") )
        {
            COM_ParseExt( text, qfalse );
            continue;
        }
        // cull <face>
        else if ( !Q_stricmp( token, "cull") )
        {
            token = COM_ParseExt( text, qfalse );
            if ( token[0] == 0 )
            {
                Com_Printf( "WARNING: missing cull parms in shader '%s'\n", shader->name );
                continue;
            }

            if ( !Q_stricmp( token, "none" ) || !Q_stricmp( token, "twosided" ) || !Q_stricmp( token, "disable" ) )
            {
                shader->cullType = CT_TWO_SIDED;
            }
            else if ( !Q_stricmp( token, "back" ) || !Q_stricmp( token, "backside" ) || !Q_stricmp( token, "backsided" ) )
            {
                shader->cullType = CT_BACK_SIDED;
            }
            else
            {
                Com_Printf( "WARNING: invalid cull parm '%s' in shader '%s'\n", token, shader->name );
            }
            continue;
        }
        // sort
        else if ( !Q_stricmp( token, "sort" ) )
        {
            ParseSort( shader, text );
            continue;
        }
        else
        {
            Com_Printf( "WARNING: unknown general shader parameter '%s' in '%s'\n", token, shader->name );
            return qfalse;
        }
    }

    //
    // ignore shaders that don't have any stages, unless it is a sky or fog
    //
    if ( s == 0 && !shader->isSky && !(shader->contentFlags & CONTENTS_FOG ) ) {
        Com_Printf("\e[31mWARNING\e[0m: shader %s has no stages\n", shader->name);
        return qfalse;
    }

    shader->explicitlyDefined = qtrue;

    return qtrue;
}

/*
========================================================================================

SHADER OPTIMIZATION AND FOGGING

========================================================================================
*/

/*
===================
ComputeStageIteratorFunc

See if we can use on of the simple fastpath stage functions,
otherwise set to the generic stage function
===================
*/
static void ComputeStageIteratorFunc( shader_t *shader )
{
    shader->optimalStageIteratorFunc = RB_StageIteratorGeneric;

    //
    // see if this should go into the sky path
    //
    if ( shader->isSky )
    {
        shader->optimalStageIteratorFunc = RB_StageIteratorSky;
        return;
    }
}

/*
===================
ComputeVertexAttribs

Check which vertex attributes we only need, so we
don't need to submit/copy all of them.
===================
*/
static void ComputeVertexAttribs(shader_t *shader)
{
    int i, stage;

    // dlights always need ATTR_NORMAL
    shader->vertexAttribs = ATTR_POSITION | ATTR_NORMAL;

    // portals always need normals, for SurfIsOffscreen()
    if (shader->isPortal)
    {
        shader->vertexAttribs |= ATTR_NORMAL;
    }

    if (shader->defaultShader)
    {
        shader->vertexAttribs |= ATTR_TEXCOORD;
        return;
    }

    if(shader->numDeforms)
    {
        for ( i = 0; i < shader->numDeforms; i++)
        {
            deformStage_t  *ds = &shader->deforms[i];

            switch (ds->deformation)
            {
                case DEFORM_BULGE:
                    shader->vertexAttribs |= ATTR_NORMAL | ATTR_TEXCOORD;
                    break;

                case DEFORM_AUTOSPRITE:
                    shader->vertexAttribs |= ATTR_NORMAL | ATTR_COLOR;
                    break;

                case DEFORM_WAVE:
                case DEFORM_NORMALS:
                case DEFORM_TEXT0:
                case DEFORM_TEXT1:
                case DEFORM_TEXT2:
                case DEFORM_TEXT3:
                case DEFORM_TEXT4:
                case DEFORM_TEXT5:
                case DEFORM_TEXT6:
                case DEFORM_TEXT7:
                    shader->vertexAttribs |= ATTR_NORMAL;
                    break;

                default:
                case DEFORM_NONE:
                case DEFORM_MOVE:
                case DEFORM_PROJECTION_SHADOW:
                case DEFORM_AUTOSPRITE2:
                    break;
            }
        }
    }

    for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
    {
        shaderStage_t *pStage = shader->stages[stage];

        if ( ! pStage ) {
            break;
        }

        if ( !pStage->active )
        {
            break;
        }

        if (pStage->glslShaderGroup == tr.lightallShader)
        {
            shader->vertexAttribs |= ATTR_NORMAL;

            if ((pStage->glslShaderIndex & LIGHTDEF_LIGHTTYPE_MASK) && !(r_normalMapping->integer == 0 && r_specularMapping->integer == 0))
            {
                shader->vertexAttribs |= ATTR_TANGENT;
            }

            switch (pStage->glslShaderIndex & LIGHTDEF_LIGHTTYPE_MASK)
            {
                case LIGHTDEF_USE_LIGHTMAP:
                case LIGHTDEF_USE_LIGHT_VERTEX:
                    shader->vertexAttribs |= ATTR_LIGHTDIRECTION;
                    break;
                default:
                    break;
            }
        }

        for (i = 0; i < NUM_TEXTURE_BUNDLES; i++)
        {
            if ( pStage->bundle[i].image[0] == 0 )
            {
                continue;
            }

            switch(pStage->bundle[i].tcGen)
            {
                case TCGEN_TEXTURE:
                    shader->vertexAttribs |= ATTR_TEXCOORD;
                    break;
                case TCGEN_LIGHTMAP:
                    shader->vertexAttribs |= ATTR_LIGHTCOORD;
                    break;
                case TCGEN_ENVIRONMENT_MAPPED:
                    shader->vertexAttribs |= ATTR_NORMAL;
                    break;

                default:
                    break;
            }
        }

        switch(pStage->rgbGen)
        {
            case CGEN_EXACT_VERTEX:
            case CGEN_VERTEX:
            case CGEN_EXACT_VERTEX_LIT:
            case CGEN_VERTEX_LIT:
            case CGEN_ONE_MINUS_VERTEX:
                shader->vertexAttribs |= ATTR_COLOR;
                break;

            case CGEN_LIGHTING_DIFFUSE:
                shader->vertexAttribs |= ATTR_NORMAL;
                break;

            default:
                break;
        }

        switch(pStage->alphaGen)
        {
            case AGEN_LIGHTING_SPECULAR:
                shader->vertexAttribs |= ATTR_NORMAL;
                break;

            case AGEN_VERTEX:
            case AGEN_ONE_MINUS_VERTEX:
                shader->vertexAttribs |= ATTR_COLOR;
                break;

            default:
                break;
        }
    }
}

/*
 * CollapseStagedsToLightall
 * 3 R_FindImageFile ( all currently disabled )
*/

static void CollapseStagesToLightall(shader_t *shader, shaderStage_t *diffuse,
        shaderStage_t *normal, shaderStage_t *specular, shaderStage_t *lightmap,
        qboolean useLightVector, qboolean useLightVertex, qboolean parallax, qboolean tcgen)
{
    int defs = 0;

    // printf("shader %s has diffuse %s", shader.name, diffuse->bundle[0].image[0]->imgName);

    // reuse diffuse, mark others inactive
    diffuse->type = ST_GLSL;

    if (lightmap)
    {
        // printf(", lightmap");
        diffuse->bundle[TB_LIGHTMAP] = lightmap->bundle[0];
        defs |= LIGHTDEF_USE_LIGHTMAP;
    }
    else if (useLightVector)
    {
        defs |= LIGHTDEF_USE_LIGHT_VECTOR;
    }
    else if (useLightVertex)
    {
        defs |= LIGHTDEF_USE_LIGHT_VERTEX;
    }

    if (r_deluxeMapping->integer && tr.worldDeluxeMapping && lightmap && shader->lightmapIndex >= 0)
    {
        //ri.Printf(PRINT_ALL, ", deluxemap");
        diffuse->bundle[TB_DELUXEMAP] = lightmap->bundle[0];
        diffuse->bundle[TB_DELUXEMAP].image[0] = tr.deluxemaps[shader->lightmapIndex];
    }

#if 0
    // TODO turned of in q3config.cfg to avoid dealing with R_FindImageFile for now as needs callbacks ( maybe not )
    if (r_normalMapping->integer)
    {
        assert(0);
        image_t *diffuseImg;
        if (normal)
        {
            //ri.Printf(PRINT_ALL, ", normalmap %s", normal->bundle[0].image[0]->imgName);
            diffuse->bundle[TB_NORMALMAP] = normal->bundle[0];
            if (parallax && r_parallaxMapping->integer)
                defs |= LIGHTDEF_USE_PARALLAXMAP;

            VectorCopy4(normal->normalScale, diffuse->normalScale);
        }
        else if ((lightmap || useLightVector || useLightVertex) && (diffuseImg = diffuse->bundle[TB_DIFFUSEMAP].image[0]))
        {
            char normalName[MAX_QPATH];
            image_t *normalImg;
            imgFlags_t normalFlags = (diffuseImg->flags & ~IMGFLAG_GENNORMALMAP) | IMGFLAG_NOLIGHTSCALE;

            // try a normalheight image first
            COM_StripExtension(diffuseImg->imgName, normalName, MAX_QPATH);
            Q_strcat(normalName, MAX_QPATH, "_nh");
            // TODO no "_nh" files in any pak file that I currently have in ~/q3a

            assert(0);// do I need? normals ( for now? )

            // THIS CODE NEEDS callback
#if 0
            normalImg = R_FindImageFile(normalName, IMGTYPE_NORMALHEIGHT, normalFlags); //callback needed ( mayb not )

            if (normalImg)
            {
                parallax = qtrue;
            }
            else
            {
                // try a normal image ("_n" suffix)
                normalName[strlen(normalName) - 1] = '\0';
                normalImg = R_FindImageFile(normalName, IMGTYPE_NORMAL, normalFlags); //callback needed (maybe not)
            }

            if (normalImg)
            {
                diffuse->bundle[TB_NORMALMAP] = diffuse->bundle[0];
                diffuse->bundle[TB_NORMALMAP].numImageAnimations = 0;
                diffuse->bundle[TB_NORMALMAP].image[0] = normalImg;

                if (parallax && r_parallaxMapping->integer)
                    defs |= LIGHTDEF_USE_PARALLAXMAP;

                VectorSet4(diffuse->normalScale, r_baseNormalX->value, r_baseNormalY->value, 1.0f, r_baseParallax->value);
            }
#endif
        }
    }

    // TODO turned of in q3config.cfg to avoid dealing with R_FindImageFile for now
    if (r_specularMapping->integer)
    {
        assert(0);
        image_t *diffuseImg;
        if (specular)
        {
            //ri.Printf(PRINT_ALL, ", specularmap %s", specular->bundle[0].image[0]->imgName);
            diffuse->bundle[TB_SPECULARMAP] = specular->bundle[0];
            VectorCopy4(specular->specularScale, diffuse->specularScale);
        }
        else if ((lightmap || useLightVector || useLightVertex) && (diffuseImg = diffuse->bundle[TB_DIFFUSEMAP].image[0]))
        {
            char specularName[MAX_QPATH];
            image_t *specularImg;
            imgFlags_t specularFlags = (diffuseImg->flags & ~IMGFLAG_GENNORMALMAP) | IMGFLAG_NOLIGHTSCALE;

            COM_StripExtension(diffuseImg->imgName, specularName, MAX_QPATH);
            Q_strcat(specularName, MAX_QPATH, "_s");

            assert(0);
            // callback needed
#if 0
            specularImg = R_FindImageFile(specularName, IMGTYPE_COLORALPHA, specularFlags); // callback needed ( mayb not )

            if (specularImg)
            {
                diffuse->bundle[TB_SPECULARMAP] = diffuse->bundle[0];
                diffuse->bundle[TB_SPECULARMAP].numImageAnimations = 0;
                diffuse->bundle[TB_SPECULARMAP].image[0] = specularImg;

                VectorSet4(diffuse->specularScale, 1.0f, 1.0f, 1.0f, 1.0f);
            }
#endif
        }
    }
#endif

    if (tcgen || diffuse->bundle[0].numTexMods)
    {
        defs |= LIGHTDEF_USE_TCGEN_AND_TCMOD;
    }

    //ri.Printf(PRINT_ALL, ".\n");

    diffuse->glslShaderGroup = tr.lightallShader;
    diffuse->glslShaderIndex = defs;
}

static int CollapseStagesToGLSL( shader_t *shader)
{
    int i, j, numStages;
    qboolean skip = qfalse;

    // skip shaders with deforms
    if (shader->numDeforms != 0)
    {
        skip = qtrue;
    }

    if ( ! shader->stages[0]->active ) {
        // TODO all inactive ????
        //Com_Printf("%s shader %s not active?\n", __func__, shader->name);
    }

    // TODO mark on shader that stages were swapped TODO
    // TODO mark on shader that stages were swapped TODO
    // TODO mark on shader that stages were swapped TODO
    // TODO mark on shader that stages were swapped TODO
    // TODO mark on shader that stages were swapped TODO
    if (!skip)
    {
        // if 2+ stages and first stage is lightmap, switch them
        // this makes it easier for the later bits to process
        if (shader->stages[0]->active && shader->stages[0]->bundle[0].tcGen == TCGEN_LIGHTMAP && shader->stages[1]->active)
        {
            int blendBits = shader->stages[1]->stateBits & ( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS );

            if (blendBits == (GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO)
                    || blendBits == (GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR))
            {
                shader->stages_swapped = qtrue; // TODO remove, dont need?

                int stateBits0 = shader->stages[0]->stateBits;
                int stateBits1 = shader->stages[1]->stateBits;

                shaderStage_t *swapStage;

                swapStage = shader->stages[0];
                shader->stages[0] = shader->stages[1];
                shader->stages[1] = swapStage;

                shader->stages[0]->stateBits = stateBits0;
                shader->stages[1]->stateBits = stateBits1;
            }
        }
    }

    if (!skip)
    {
        // scan for shaders that aren't supported
        for (i = 0; i < MAX_SHADER_STAGES; i++)
        {
            shaderStage_t *pStage = shader->stages[i];

            if ( ! pStage ) {
                break;
            }

            if (!pStage->active) {
                break;
            }

            if (pStage->adjustColorsForFog)
            {
                skip = qtrue;
                break;
            }

            if (pStage->bundle[0].tcGen == TCGEN_LIGHTMAP)
            {
                int blendBits = pStage->stateBits & ( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS );

                if (blendBits != (GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO)
                        && blendBits != (GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR))
                {
                    skip = qtrue;
                    break;
                }
            }

            switch(pStage->bundle[0].tcGen)
            {
                case TCGEN_TEXTURE:
                case TCGEN_LIGHTMAP:
                case TCGEN_ENVIRONMENT_MAPPED:
                case TCGEN_VECTOR:
                    break;
                default:
                    skip = qtrue;
                    break;
            }

            switch(pStage->alphaGen)
            {
                case AGEN_LIGHTING_SPECULAR:
                case AGEN_PORTAL:
                    skip = qtrue;
                    break;
                default:
                    break;
            }
        }
    }

    if (!skip)
    {
        qboolean usedLightmap = qfalse;

        for (i = 0; i < MAX_SHADER_STAGES; i++)
        {
            shaderStage_t *diffuse, *normal, *specular, *lightmap;
            qboolean parallax, tcgen, diffuselit, vertexlit;

            shaderStage_t *pStage = shader->stages[i];

            if ( !pStage ) {
                continue;
            }

            if ( ! pStage->active ) {
                continue;
            }

            // skip normal and specular maps
            if (pStage->type != ST_COLORMAP) {
                continue;
            }

            // skip lightmaps
            if (pStage->bundle[0].tcGen == TCGEN_LIGHTMAP) {
                continue;
            }

            diffuse  = pStage;
            normal   = NULL;
            parallax = qfalse;
            specular = NULL;
            lightmap = NULL;

            // we have a diffuse map, find matching normal, specular, and lightmap
            for (j = i + 1; j < MAX_SHADER_STAGES; j++)
            {
                shaderStage_t *pStage2 = shader->stages[j];

                if ( ! pStage2 ) {
                    continue;
                }

                if (!pStage2->active) {
                    continue;
                }

                switch(pStage2->type)
                {
                    case ST_NORMALMAP:
                        if (!normal) {
                            normal = pStage2;
                        }
                        break;

                    case ST_NORMALPARALLAXMAP:
                        if (!normal) {
                            normal = pStage2;
                            parallax = qtrue;
                        }
                        break;

                    case ST_SPECULARMAP:
                        if (!specular) {
                            specular = pStage2;
                        }
                        break;

                    case ST_COLORMAP:
                        if (pStage2->bundle[0].tcGen == TCGEN_LIGHTMAP)
                        {
                            int blendBits = pStage->stateBits & ( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS );

                            // Only add lightmap to blendfunc filter stage if it's the first time lightmap is used
                            // otherwise it will cause the shader to be darkened by the lightmap multiple times.
                            if (!usedLightmap || (blendBits != (GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO)
                                        && blendBits != (GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR)))
                            {
                                lightmap = pStage2;
                                usedLightmap = qtrue;
                            }
                        }
                        break;

                    default:
                        break;
                }
            }

            tcgen = qfalse;
            if (diffuse->bundle[0].tcGen == TCGEN_ENVIRONMENT_MAPPED
                    || diffuse->bundle[0].tcGen == TCGEN_LIGHTMAP
                    || diffuse->bundle[0].tcGen == TCGEN_VECTOR)
            {
                tcgen = qtrue;
            }

            diffuselit = qfalse;
            if (diffuse->rgbGen == CGEN_LIGHTING_DIFFUSE)
            {
                diffuselit = qtrue;
            }

            vertexlit = qfalse;
            if (diffuse->rgbGen == CGEN_VERTEX_LIT || diffuse->rgbGen == CGEN_EXACT_VERTEX_LIT)
            {
                vertexlit = qtrue;
            }

            CollapseStagesToLightall(shader, diffuse, normal, specular, lightmap, diffuselit, vertexlit, parallax, tcgen);
        }

        // deactivate lightmap stages
        for (i = 0; i < MAX_SHADER_STAGES; i++)
        {
            shaderStage_t *pStage = shader->stages[i];

            if ( ! pStage ) {
                continue;
            }

            if ( ! pStage->active ) {
                continue;
            }

            if (pStage->bundle[0].tcGen == TCGEN_LIGHTMAP)
            {
                pStage->active = qfalse;
            }
        }
    }

    // deactivate normal and specular stages
    for (i = 0; i < MAX_SHADER_STAGES; i++)
    {
        shaderStage_t *pStage = shader->stages[i];

        if ( ! pStage ) {
            continue;
        }

        if ( ! pStage->active ) {
            continue;
        }

        if (pStage->type == ST_NORMALMAP)
        {
assert(0);
            pStage->active = qfalse;
        }

        if (pStage->type == ST_NORMALPARALLAXMAP)
        {
assert(0);
            pStage->active = qfalse;
        }

        if (pStage->type == ST_SPECULARMAP)
        {
assert(0);
            pStage->active = qfalse;
        }
    }

    // remove inactive stages by moving active stages to the 'front'
    numStages = 0;
    for (i = 0; i < MAX_SHADER_STAGES; i++)
    {
        shaderStage_t *stage = shader->stages[i];
        if ( ! stage ) {
            continue;
        }

        if ( ! stage->active ) {
            continue;
        }

        if (i == numStages) {
            numStages++;
            continue;
        }

        shader->stages[numStages] = shader->stages[i];
        shader->stages[i]->active = qfalse;
        numStages++;
    }

    // convert any remaining lightmap stages with no blending or blendfunc filter
    // to a lighting pass with a white texture
    // only do this with r_sunlightMode non-zero, as it's only for correct shadows.
    if (r_sunlightMode->integer && shader->numDeforms == 0)
    {
        for (i = 0; i < MAX_SHADER_STAGES; i++)
        {
            shaderStage_t *pStage = shader->stages[i];
            int blendBits;

            if ( !pStage ) {
                continue;
            }
            if ( ! pStage->active ) {
                continue;
            }

            if (pStage->adjustColorsForFog) {
                continue;
            }

            if (pStage->bundle[TB_DIFFUSEMAP].tcGen != TCGEN_LIGHTMAP) {
                continue;
            }

            blendBits = pStage->stateBits & (GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS);

            if (blendBits != 0 &&
                    blendBits != (GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO) &&
                    blendBits != (GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR)) {
                continue;
            }

            pStage->glslShaderGroup = tr.lightallShader;
            pStage->glslShaderIndex = LIGHTDEF_USE_LIGHTMAP;
            pStage->bundle[TB_LIGHTMAP] = pStage->bundle[TB_DIFFUSEMAP];
            pStage->bundle[TB_DIFFUSEMAP].image[0] = tr.whiteImage;
            pStage->bundle[TB_DIFFUSEMAP].isLightmap = qfalse;
            pStage->bundle[TB_DIFFUSEMAP].tcGen = TCGEN_TEXTURE;
        }
    }

    // convert any remaining lightingdiffuse stages to a lighting pass
    if (shader->numDeforms == 0)
    {
        for (i = 0; i < MAX_SHADER_STAGES; i++)
        {
            shaderStage_t *pStage = shader->stages[i];

            if ( ! pStage ) {
                continue;
            }

            if ( ! pStage->active ) {
                continue;
            }

            if (pStage->adjustColorsForFog) {
                continue;
            }

            if (pStage->rgbGen == CGEN_LIGHTING_DIFFUSE)
            {
                pStage->glslShaderGroup = tr.lightallShader;
                pStage->glslShaderIndex = LIGHTDEF_USE_LIGHT_VECTOR;

                if (pStage->bundle[0].tcGen != TCGEN_TEXTURE || pStage->bundle[0].numTexMods != 0) {
                    pStage->glslShaderIndex |= LIGHTDEF_USE_TCGEN_AND_TCMOD;
                }
            }
        }
    }

    return numStages;
}

/*
=============

FixRenderCommandList
https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=493
Arnout: this is a nasty issue. Shaders can be registered after drawsurfaces are generated
but before the frame is rendered. This will, for the duration of one frame, cause drawsurfaces
to be rendered with bad shaders. To fix this, need to go through all render commands and fix
sortedIndex.
==============
*/
static void FixRenderCommandList( int newShader ) {
    renderCommandList_t *cmdList = &backEndData->commands;

    if( cmdList ) {
        const void *curCmd = cmdList->cmds;

        while ( 1 ) {
            curCmd = PADP(curCmd, sizeof(void *));

            switch ( *(const int *)curCmd ) {
                case RC_SET_COLOR:
                    {
                        const setColorCommand_t *sc_cmd = (const setColorCommand_t *)curCmd;
                        curCmd = (const void *)(sc_cmd + 1);
                        break;
                    }
                case RC_STRETCH_PIC:
                    {
                        const stretchPicCommand_t *sp_cmd = (const stretchPicCommand_t *)curCmd;
                        curCmd = (const void *)(sp_cmd + 1);
                        break;
                    }
                case RC_DRAW_SURFS:
                    {
                        int i;
                        drawSurf_t  *drawSurf;
                        shader_t    *shader;
                        int         fogNum;
                        int         entityNum;
                        int         dlightMap;
                        int         pshadowMap;
                        int         sortedIndex;
                        const drawSurfsCommand_t *ds_cmd =  (const drawSurfsCommand_t *)curCmd;

                        for( i = 0, drawSurf = ds_cmd->drawSurfs; i < ds_cmd->numDrawSurfs; i++, drawSurf++ ) {
                            R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlightMap, &pshadowMap );
                            sortedIndex = (( drawSurf->sort >> QSORT_SHADERNUM_SHIFT ) & (MAX_SHADERS-1));
                            if( sortedIndex >= newShader ) {
                                sortedIndex++;
                                drawSurf->sort = (sortedIndex << QSORT_SHADERNUM_SHIFT) | entityNum | ( fogNum << QSORT_FOGNUM_SHIFT ) | ( (int)pshadowMap << QSORT_PSHADOW_SHIFT) | (int)dlightMap;
                            }
                        }
                        curCmd = (const void *)(ds_cmd + 1);
                        break;
                    }
                case RC_DRAW_BUFFER:
                    {
                        const drawBufferCommand_t *db_cmd = (const drawBufferCommand_t *)curCmd;
                        curCmd = (const void *)(db_cmd + 1);
                        break;
                    }
                case RC_SWAP_BUFFERS:
                    {
                        const swapBuffersCommand_t *sb_cmd = (const swapBuffersCommand_t *)curCmd;
                        curCmd = (const void *)(sb_cmd + 1);
                        break;
                    }
                case RC_END_OF_LIST:
                default:
                    return;
            }
        }
    }
}

/*
==============
SortNewShader

Positions the most recently created shader in the tr.sortedShaders[]
array so that the shader->sort key is sorted relative to the other
shaders.

Sets shader->sortedIndex
==============
*/
static void SortNewShader( shader_t *shader ) {
    int     i;

    if ( shader->sort <= SS_OPAQUE ) {
        shader->fogPass = FP_EQUAL;
    } else if ( shader->contentFlags & CONTENTS_FOG ) {
        shader->fogPass = FP_LE;
    }

    float sort = shader->sort;

    for ( i = tr.numShaders - 2 ; i >= 0 ; i-- ) {
        if ( tr.sortedShaders[ i ]->sort <= sort ) {
            break;
        }
        tr.sortedShaders[i+1] = tr.sortedShaders[i];
        tr.sortedShaders[i+1]->sortedIndex++;
    }

    FixRenderCommandList( i + 1 ); // Arnout: fix rendercommandlist https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=493

    shader->sortedIndex       = i + 1;
    tr.sortedShaders[ i + 1 ] = shader;
}

/*
=================
VertexLightingCollapse

If vertex lighting is enabled, only render a single
pass, trying to guess which is the correct one to best approximate
what it is supposed to look like.
=================
*/
static void VertexLightingCollapse( shader_t *shader ) {
    int     stage;
    shaderStage_t   *bestStage;
    int     bestImageRank;
    int     rank;

    // if we aren't opaque, just use the first pass
    if ( shader->sort == SS_OPAQUE ) {

        // pick the best texture for the single pass
        bestStage = shader->stages[0];
        bestImageRank = -999999;

        for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ ) {
            shaderStage_t *pStage = shader->stages[stage];

            if ( ! pStage ) {
                break;
            }

            if ( ! pStage->active ) {
                break;
            }

            rank = 0;

            if ( pStage->bundle[0].isLightmap ) {
                rank -= 100;
            }
            if ( pStage->bundle[0].tcGen != TCGEN_TEXTURE ) {
                rank -= 5;
            }
            if ( pStage->bundle[0].numTexMods ) {
                rank -= 5;
            }
            if ( pStage->rgbGen != CGEN_IDENTITY && pStage->rgbGen != CGEN_IDENTITY_LIGHTING ) {
                rank -= 3;
            }

            if ( rank > bestImageRank  ) {
                bestImageRank = rank;
                bestStage = pStage;
            }
        }

        shader->stages[0]->bundle[0] = bestStage->bundle[0]; //TODO copying a struct by value
        shader->stages[0]->stateBits &= ~( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS );
        shader->stages[0]->stateBits |= GLS_DEPTHMASK_TRUE;

        if ( shader->lightmapIndex == LIGHTMAP_NONE ) {
            shader->stages[0]->rgbGen = CGEN_LIGHTING_DIFFUSE;
        } else {
            shader->stages[0]->rgbGen = CGEN_EXACT_VERTEX;
        }

        shader->stages[0]->alphaGen = AGEN_SKIP;

    } else {
        // don't use a lightmap (tesla coils)
        if ( shader->stages[0]->bundle[0].isLightmap ) {
            shader->stages[0] = shader->stages[1]; // TODO copy struct by value
        }

        // if we were in a cross-fade cgen, hack it to normal
        if ( shader->stages[0]->rgbGen == CGEN_ONE_MINUS_ENTITY || shader->stages[1]->rgbGen == CGEN_ONE_MINUS_ENTITY ) {
            shader->stages[0]->rgbGen = CGEN_IDENTITY_LIGHTING;
        }
        if ( ( shader->stages[0]->rgbGen == CGEN_WAVEFORM && shader->stages[0]->rgbWave.func == GF_SAWTOOTH )
                && ( shader->stages[1]->rgbGen == CGEN_WAVEFORM && shader->stages[1]->rgbWave.func == GF_INVERSE_SAWTOOTH ) ) {
            shader->stages[0]->rgbGen = CGEN_IDENTITY_LIGHTING;
        }
        if ( ( shader->stages[0]->rgbGen == CGEN_WAVEFORM && shader->stages[0]->rgbWave.func == GF_INVERSE_SAWTOOTH )
                && ( shader->stages[1]->rgbGen == CGEN_WAVEFORM && shader->stages[1]->rgbWave.func == GF_SAWTOOTH ) ) {
            shader->stages[0]->rgbGen = CGEN_IDENTITY_LIGHTING;
        }
    }

    for ( stage = 1; stage < MAX_SHADER_STAGES; stage++ ) {
        shaderStage_t *pStage = shader->stages[stage];

        if ( !pStage) {
            break;
        }

        if ( ! pStage->active ) {
            break;
        }
        Com_Memset( pStage, 0, sizeof( shaderStage_t ) );
    }
}

/*
=================
FixFatLightmapTexCoords

Handle edge cases of altering lightmap texcoords for fat lightmap atlas
=================
*/
static void FixFatLightmapTexCoords(shader_t *shader)
{
    texModInfo_t *tmi;
    int lightmapnum;
    int stage;
    int size;
    int i;

    if ( !r_mergeLightmaps->integer || tr.fatLightmapCols <= 0) {
        return;
    }

    if ( shader->lightmapIndex < 0 ) {
        // no internal lightmap, texcoords were not modified
        return;
    }

    lightmapnum = shader->shader_realLightmapIndex;

    if (tr.worldDeluxeMapping) {
        lightmapnum >>= 1;
    }

    lightmapnum %= (tr.fatLightmapCols * tr.fatLightmapRows);

    for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ ) {
        shaderStage_t *pStage = shader->stages[stage];

        if ( ! pStage ) {
            break;
        }

        if ( ! pStage->active ) {
            break;
        }

        if ( pStage->bundle[0].isLightmap ) {
            // fix tcMod transform for internal lightmaps, it may be used by q3map2 lightstyles
            if ( pStage->bundle[0].tcGen == TCGEN_LIGHTMAP ) {
                for ( i = 0; i < pStage->bundle[0].numTexMods; i++ ) {
                    tmi = &pStage->bundle[0].texMods[i];

                    if ( tmi->type == TMOD_TRANSFORM ) {
                        tmi->translate[0] /= (float)tr.fatLightmapCols;
                        tmi->translate[1] /= (float)tr.fatLightmapRows;
                    }
                }
            }

            // fix tcGen environment for internal lightmaps to be limited to the sub-image of the atlas
            // this is done last so other tcMods are applied first in the 0.0 to 1.0 space
            if ( pStage->bundle[0].tcGen == TCGEN_ENVIRONMENT_MAPPED ) {
                if ( pStage->bundle[0].numTexMods == TR_MAX_TEXMODS ) {
                    Com_Printf( "WARNING: too many tcmods to fix lightmap texcoords for r_mergeLightmaps in shader '%s'", shader->name );
                } else {
                    tmi = &pStage->bundle[0].texMods[pStage->bundle[0].numTexMods];
                    pStage->bundle[0].numTexMods++;

                    tmi->matrix[0][0] = 1.0f / tr.fatLightmapCols;
                    tmi->matrix[0][1] = 0;
                    tmi->matrix[1][0] = 0;
                    tmi->matrix[1][1] = 1.0f / tr.fatLightmapRows;

                    tmi->translate[0] = ( lightmapnum % tr.fatLightmapCols ) / (float)tr.fatLightmapCols;
                    tmi->translate[1] = ( lightmapnum / tr.fatLightmapCols ) / (float)tr.fatLightmapRows;

                    tmi->type = TMOD_TRANSFORM;
                }
            }
        }
        // add a tcMod transform for external lightmaps to convert back to the original texcoords
        else if ( pStage->bundle[0].tcGen == TCGEN_LIGHTMAP ) {
            if ( pStage->bundle[0].numTexMods == TR_MAX_TEXMODS ) {
                Com_Printf( "WARNING: too many tcmods to fix lightmap texcoords for r_mergeLightmaps in shader '%s'", shader->name );
            } else {
                size = pStage->bundle[0].numTexMods * sizeof( texModInfo_t );

                if ( size ) {
assert(0);
                    //memmove( &pStage->bundle[0].texMods[1], &pStage->bundle[0].texMods[0], size );
                }

                tmi = &pStage->bundle[0].texMods[0];
                pStage->bundle[0].numTexMods++;

                tmi->matrix[0][0] = tr.fatLightmapCols;
                tmi->matrix[0][1] = 0;
                tmi->matrix[1][0] = 0;
                tmi->matrix[1][1] = tr.fatLightmapRows;

                tmi->translate[0] = -( lightmapnum % tr.fatLightmapCols );
                tmi->translate[1] = -( lightmapnum / tr.fatLightmapCols );

                tmi->type = TMOD_TRANSFORM;
            }
        }
    }
}

static void DestroyShader ( shader_t* shader ) {
    if ( ! shader ) {
        return;
    }

    for ( int i = 0 ; i < MAX_SHADER_STAGES ; i++ ) {
        shaderStage_t *stage = shader->stages[i];

        if ( ! stage ) {
            continue;
        }

        for ( int j = 0 ; j < NUM_TEXTURE_BUNDLES ; j++ ) {
            textureBundle_t *tb = &stage->bundle[j];

            if ( ! tb ) {
                continue;
            }

            for ( int x = 0 ; x < tb->numImageAnimations ; x++ ) {
                image_t *image = tb->image[x];

                if ( ! image ) {
                    continue;
                }

                Com_Printf("%s shader image exists? %s\n", __func__, shader->name);
                //assert(0);
            }
        }
    }
}
static shader_t *AllocShader(const char* name) {

    if ( tr.numShaders >= MAX_SHADERS ) {
        Com_Printf( "WARNING: %s - MAX_SHADERS hit\n", __func__);
        return tr.defaultShader;
    }

    shader_t *shader = malloc(sizeof(shader_t));

    if ( ! shader ) {
        Com_Error(ERR_FATAL, "malloc");
    }

    Com_Memset(shader, 0, sizeof(shader_t));

    Q_strncpyz( shader->name, name, sizeof( shader->name ) );

    for ( int i = 0 ; i < MAX_SHADER_STAGES ; i++ ) {
        shader->stages[i] = calloc(1, sizeof(shaderStage_t));
    }

    shader->stages_swapped    = qfalse;
    shader->stages[0]->active = qtrue;

    tr.shaders[ tr.numShaders ] = shader;
    shader->index = tr.numShaders;

    tr.sortedShaders[ tr.numShaders ] = shader;
    shader->sortedIndex = tr.numShaders;

    tr.numShaders++;

    int hash = generateHashValue(shader->name, FILE_HASH_SIZE);
    shader->next = hashTable[hash];
    hashTable[hash] = shader;

    return shader;
}

/*
===============
InitShaderEx
===============
*/

#define texModSize sizeof(texModInfo_t) * TR_MAX_TEXMODS * MAX_SHADER_STAGES
static shader_t * InitShaderEx( const char *name, int lightmapIndex, int realLightmapIndex ) {

    shader_t *shader = AllocShader(name);

    shader->lightmapIndex            = lightmapIndex;
    shader->shader_realLightmapIndex = realLightmapIndex;

    for ( int i = 0 ; i < MAX_SHADER_STAGES ; i++ ) {

        texModInfo_t *texMod = malloc(texModSize);
        if ( ! texMod ) {
            Com_Error(ERR_FATAL, "malloc");
        }

        memset(texMod, 0, texModSize);
        shader->stages[i]->bundle[0].texMods = texMod;

        // default normal/specular
        VectorSet4(shader->stages[i]->normalScale, 0.0f, 0.0f, 0.0f, 0.0f);
        if (r_pbr->integer)
        {
            shader->stages[i]->specularScale[0] = r_baseGloss->value;
        }
        else
        {
            shader->stages[i]->specularScale[0] =
            shader->stages[i]->specularScale[1] =
            shader->stages[i]->specularScale[2] = r_baseSpecular->value;
            shader->stages[i]->specularScale[3] = r_baseGloss->value;
        }
    }

    return shader;
}

/*
===============
InitShader
===============
*/
static shader_t* InitShader( const char *name, int lightmapIndex ) {
    return InitShaderEx( name, lightmapIndex, lightmapIndex );
}


/*
=========================
FinishShader

=========================
*/
static shader_t *FinishShader( shader_t *shader ) {

    // Com_Printf("%s\n", __func__);
    if ( shader->finished ) {
        Com_Printf("%s shader %s already finished\n", __func__, shader->name);
        return shader;
    }

    // Com_Printf("%s finishing shader %s\n", __func__, shader->name);

    SHADER_NOT_ACTIVE;

    qboolean        hasLightmapStage;
    qboolean        vertexLightmap;

    hasLightmapStage = qfalse;
    vertexLightmap   = qfalse;

    //
    // set sky stuff appropriate
    //
    if ( shader->isSky ) {
        shader->sort = SS_ENVIRONMENT;
    }

    //
    // set polygon offset
    //
    if ( shader->polygonOffset && !shader->sort ) {
        shader->sort = SS_DECAL;
    }

    int stage;

    //
    // set appropriate stage information
    //
    for ( stage = 0; stage < MAX_SHADER_STAGES; ) {
        shaderStage_t *pStage = shader->stages[stage];

        if ( ! pStage ) {
            break;
        }

        if ( ! pStage->active ) {
            break;
        }

        // check for a missing texture
        if ( !pStage->bundle[0].image[0] ) {
            pStage->active = qfalse;
            stage++;
            continue;
        }

        //
        // ditch this stage if it's detail and detail textures are disabled
        //
        if ( pStage->isDetail && !r_detailTextures->integer )
        {
            int index;

            for(index = stage + 1; index < MAX_SHADER_STAGES; index++)
            {
                if(!shader->stages[index]->active) {
                    break;
                }
            }

            if(index < MAX_SHADER_STAGES) {
                memmove(pStage, pStage + 1, sizeof(*pStage) * (index - stage));
            } else {
                if(stage + 1 < MAX_SHADER_STAGES) {
                    memmove(pStage, pStage + 1, sizeof(*pStage) * (index - stage - 1));
                }

                Com_Memset(&shader->stages[index - 1], 0, sizeof(shaderStage_t) * MAX_SHADER_STAGES); // TODO test
            }

            continue;
        }

        //
        // default texture coordinate generation
        //
        if ( pStage->bundle[0].isLightmap ) {
            if ( pStage->bundle[0].tcGen == TCGEN_BAD ) {
                pStage->bundle[0].tcGen = TCGEN_LIGHTMAP;
            }
            hasLightmapStage = qtrue;
        } else {
            if ( pStage->bundle[0].tcGen == TCGEN_BAD ) {
                pStage->bundle[0].tcGen = TCGEN_TEXTURE;
            }
        }

        // not a true lightmap but we want to leave existing
        // behaviour in place and not print out a warning
        //if (pStage->rgbGen == CGEN_VERTEX) {
        //  vertexLightmap = qtrue;
        //}

        //
        // determine sort order and fog color adjustment
        //
        if ( ( pStage->stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) &&
                ( shader->stages[0]->stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) ) {

            int blendSrcBits = pStage->stateBits & GLS_SRCBLEND_BITS;
            int blendDstBits = pStage->stateBits & GLS_DSTBLEND_BITS;

            // fog color adjustment only works for blend modes that have a contribution
            // that aproaches 0 as the modulate values aproach 0 --
            // GL_ONE, GL_ONE
            // GL_ZERO, GL_ONE_MINUS_SRC_COLOR
            // GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA

            // modulate, additive
            if ( ( ( blendSrcBits == GLS_SRCBLEND_ONE ) && ( blendDstBits == GLS_DSTBLEND_ONE ) ) ||
                    ( ( blendSrcBits == GLS_SRCBLEND_ZERO ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR ) ) ) {
                pStage->adjustColorsForFog = ACFF_MODULATE_RGB;
            }
            // strict blend
            else if ( ( blendSrcBits == GLS_SRCBLEND_SRC_ALPHA ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) )
            {
                pStage->adjustColorsForFog = ACFF_MODULATE_ALPHA;
            }
            // premultiplied alpha
            else if ( ( blendSrcBits == GLS_SRCBLEND_ONE ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) )
            {
                pStage->adjustColorsForFog = ACFF_MODULATE_RGBA;
            } else {
                // we can't adjust this one correctly, so it won't be exactly correct in fog
            }

            // don't screw with sort order if this is a portal or environment
            if ( !shader->sort ) {
                // see through item, like a grill or grate
                if ( pStage->stateBits & GLS_DEPTHMASK_TRUE ) {
                    shader->sort = SS_SEE_THROUGH;
                } else {
                    shader->sort = SS_BLEND0;
                }
            }
        }

        stage++;
    }

    // there are times when you will need to manually apply a sort to
    // opaque alpha tested shaders that have later blend passes
    if ( !shader->sort ) {
        shader->sort = SS_OPAQUE;
    }

    //
    // if we are in r_vertexLight mode, never use a lightmap texture
    //
    if ( stage > 1 && r_vertexLight->integer && !r_uiFullScreen->integer ) {
        VertexLightingCollapse(shader);
        hasLightmapStage = qfalse;
    }

    FixFatLightmapTexCoords(shader);

    //
    // look for multitexture potential
    //
    stage = CollapseStagesToGLSL(shader);

    if ( shader->lightmapIndex >= 0 && !hasLightmapStage ) {
        if (vertexLightmap) {
            Com_Printf( "WARNING: shader '%s' has VERTEX forced lightmap!\n", shader->name );
        } else {
            Com_Printf( "WARNING: shader '%s' has lightmap but no lightmap stage!\n", shader->name );
            // Don't set this, it will just add duplicate shaders to the hash
            //shader->lightmapIndex = LIGHTMAP_NONE;
        }
    }

    //
    // compute number of passes
    //
    shader->numUnfoggedPasses = stage;

    // fogonly shaders don't have any normal passes
    if (stage == 0 && !shader->isSky) {
        shader->sort = SS_FOG;
    }

    // determine which stage iterator function is appropriate
    ComputeStageIteratorFunc(shader);

    // determine which vertex attributes this shader needs
    ComputeVertexAttribs( shader );

    SortNewShader(shader);

    shader->finished = qtrue;

    return shader;
}

qboolean FinishShaders(void) {
    Com_Printf("\e[32m%s\e[0m\n", __func__);
    int numShadersProcessed = 0;

    qboolean allShadersFinished = qtrue;

    for ( int i = 0 ; i < tr.numShaders ; i++ ) {
        shader_t *shader = tr.shaders[i];

        if ( ! shader->finished ) {
            numShadersProcessed++;
            FinishShader(shader);
        }
        allShadersFinished &= shader->finished;
    }

    Com_Printf(" >> %s %d shaders finished\n", __func__, numShadersProcessed);
    if ( allShadersFinished ) {
        Com_Printf("\n   ALL shaders finished\n");
    }

    return numShadersProcessed > 0;
}
/*
====================
FindShaderInShaderText

Scans the combined text description of all the shader files for
the given shader name.

return NULL if not found

If found, it will return a valid shader
=====================
*/
static char *FindShaderInShaderText( const char *shadername ) {

    int hash = generateHashValue(shadername, MAX_SHADERTEXT_HASH);

    if(shaderTextHashTable[hash])
    {
        for (int i = 0; shaderTextHashTable[hash][i]; i++)
        {
            char *p     = shaderTextHashTable[hash][i];
            char *token = COM_ParseExt(&p, qtrue);

            if(!Q_stricmp(token, shadername)) {
                return p;
            }
        }
    }

    char *p = s_shaderText;

    if ( !p ) {
        Com_Error(ERR_FATAL, "s_shaderText is NULL");
        return NULL;
    }

    // look for label
    while ( 1 ) {
        char *token = COM_ParseExt( &p, qtrue );
        if ( token[0] == 0 ) {
            break;
        }

        if ( !Q_stricmp( token, shadername ) ) {
            return p;
        }
        else {
            // skip the definition
            SkipBracedSection( &p, 0 );
        }
    }

    return NULL;
}

/*
==================
R_FindShaderByName

Will always return a valid shader, but it might be the
default shader if the real one can't be found.

DOES NOT LOAD ANYTHING
==================
*/
static shader_t *R_FindShaderByName( const char *name ) {
    char        strippedName[MAX_QPATH];
    int         hash;
    shader_t    *sh;

    if ( (name==NULL) || (name[0] == 0) ) {
        return tr.defaultShader;
    }

    COM_StripExtension(name, strippedName, sizeof(strippedName));

    hash = generateHashValue(strippedName, FILE_HASH_SIZE);

    //
    // see if the shader is already loaded
    //
    for (sh=hashTable[hash]; sh; sh=sh->next) {
        // NOTE: if there was no shader or image available with the name strippedName
        // then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
        // have to check all default shaders otherwise for every call to R_FindShader
        // with that same strippedName a new default shader is created.
        if (Q_stricmp(sh->name, strippedName) == 0) {
            // match found
            return sh;
        }
    }

    return tr.defaultShader;
}

/*
===============
R_FindShader

Will always return a valid shader, but it might be the
default shader if the real one can't be found.

In the interest of not requiring an explicit shader text entry to
be defined for every single image used in the game, three default
shader behaviors can be auto-created for any image:

If lightmapIndex == LIGHTMAP_NONE, then the image will have
dynamic diffuse lighting applied to it, as appropriate for most
entity skin surfaces.

If lightmapIndex == LIGHTMAP_2D, then the image will be used
for 2D rendering unless an explicit shader is found

If lightmapIndex == LIGHTMAP_BY_VERTEX, then the image will use
the vertex rgba modulate values, as appropriate for misc_model
pre-lit surfaces.

Other lightmapIndex values will have a lightmap stage created
and src*dest blending applied with the texture, as appropriate for
most world construction surfaces.

1 R_FindImageFile call that needs callback

renderergl2/tr_bsp.c:
    ShaderForShaderNum
        returns shader to local alias of global <<<<<
    LoadFogs
        returns shader to a local and thrown away ...
renderergl2/tr_image.c:
    RegisterSkinCallback
        another call is if 0'd out
        returns shader to a aliased global
renderergl2/tr_model.c:
    R_LoadMD3
        return value thrown away, only used for index into global shader
    R_LoadMDR
        call not used

rest of the calls are from this file
RE_RegisterShaderLightMap
    returns shader to local to extract index

 RE_RegisterShader
    returns shader to local to extract index

RE_RegisterShaderNoMip
    returns shader to local to extract index

===============
*/
static shader_t *R_FindShaderEx( const char *name, int lightmapIndex, qboolean mipRawImage, int realLightmapIndex );
shader_t *R_FindShader( const char *name, int lightmapIndex, qboolean mipRawImage ) {
    return R_FindShaderEx( name, lightmapIndex, mipRawImage, lightmapIndex );
}

static shader_t *R_FindShaderEx( const char *name, int lightmapIndex, qboolean mipRawImage, int realLightmapIndex ) {
    assert(name);

    if ( name[0] == '\0' ) { // for models without names? ... in original source
        return tr.defaultShader;
    }

    if ( strlen( name ) >= MAX_QPATH ) {
        Com_Error( ERR_FATAL, "Shader name exceeds MAX_QPATH\n" );
    }

    // use (fullbright) vertex lighting if the bsp file doesn't have lightmaps
    if ( lightmapIndex >= 0 && lightmapIndex >= tr.numLightmaps ) {
        lightmapIndex = LIGHTMAP_BY_VERTEX;
    } else if ( lightmapIndex < LIGHTMAP_2D ) {
        // negative lightmap indexes cause stray pointers (think tr.lightmaps[lightmapIndex])
        Com_Printf( "WARNING: shader '%s' has invalid lightmap index of %d\n", name, lightmapIndex  );
        lightmapIndex = LIGHTMAP_BY_VERTEX;
    }

    char        strippedName[MAX_QPATH];
    COM_StripExtension(name, strippedName, sizeof(strippedName));

    int hash = generateHashValue(strippedName, FILE_HASH_SIZE);

    // TODO call R_FindShaderByName here
    // TODO call R_FindShaderByName here
    //
    // see if the shader is already loaded
    //
    for (shader_t *sh = hashTable[hash]; sh; sh = sh->next) {
        // NOTE: if there was no shader or image available with the name strippedName
        // then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
        // have to check all default shaders otherwise for every call to R_FindShader
        // with that same strippedName a new default shader is created.
        if ( (sh->lightmapIndex == lightmapIndex || sh->defaultShader) && !Q_stricmp(sh->name, strippedName)) {
            // match found
            //assert(0);
            return sh;
        }
    }

    shader_t *shader = InitShaderEx( strippedName, lightmapIndex, realLightmapIndex );

    //
    // attempt to define shader from an explicit parameter file
    //
    char *shaderText = FindShaderInShaderText( strippedName );
    if ( shaderText ) {
        // enable this when building a pak file to get a global list
        // of all explicit shaders
        if ( r_printShaders->integer ) {
            Com_Printf( "*SHADER* %s\n", name );
        }

        if ( ! ParseShader( shader, &shaderText ) ) {
            //assert(0);
            shader->defaultShader = qtrue; // had errors, so use default shader
        }

        assert(shader->stages[0]->active);
        return FinishShader(shader);
    }

    //
    // if not defined in the in-memory shader descriptions,
    // look for a single supported image file
    //

    imgFlags_t flags;

    flags = IMGFLAG_NONE;

    if (mipRawImage)
    {
        flags |= IMGFLAG_MIPMAP | IMGFLAG_PICMIP;

        if (r_genNormalMaps->integer)
            flags |= IMGFLAG_GENNORMALMAP;
    }
    else
    {
        flags |= IMGFLAG_CLAMPTOEDGE;
    }

    image_t* image = R_FindImageFile( name, IMGTYPE_COLORALPHA, flags );
    if ( ! image ) {
        Com_Printf("%s failed to find image %s\n", __func__, name);
        //assert(0);
        shader->defaultShader = qtrue;
        return FinishShader(shader);
    }

    //
    // create the default shading commands
    //
    if ( shader->lightmapIndex == LIGHTMAP_NONE ) {
        // dynamic colors at vertexes
        shader->stages[0]->bundle[0].image[0] = image;
        shader->stages[0]->active = qtrue;
        shader->stages[0]->rgbGen = CGEN_LIGHTING_DIFFUSE;
        shader->stages[0]->stateBits = GLS_DEFAULT;
    } else if ( shader->lightmapIndex == LIGHTMAP_BY_VERTEX ) {
        // explicit colors at vertexes
        shader->stages[0]->bundle[0].image[0] = image;
        shader->stages[0]->active = qtrue;
        shader->stages[0]->rgbGen = CGEN_EXACT_VERTEX;
        shader->stages[0]->alphaGen = AGEN_SKIP;
        shader->stages[0]->stateBits = GLS_DEFAULT;
    } else if ( shader->lightmapIndex == LIGHTMAP_2D ) {
        // GUI elements
        shader->stages[0]->bundle[0].image[0] = image;
        shader->stages[0]->active = qtrue;
        shader->stages[0]->rgbGen = CGEN_VERTEX;
        shader->stages[0]->alphaGen = AGEN_VERTEX;
        shader->stages[0]->stateBits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
    } else if ( shader->lightmapIndex == LIGHTMAP_WHITEIMAGE ) {
        // fullbright level
        shader->stages[0]->bundle[0].image[0] = tr.whiteImage;
        shader->stages[0]->active = qtrue;
        shader->stages[0]->rgbGen = CGEN_IDENTITY_LIGHTING;
        shader->stages[0]->stateBits = GLS_DEFAULT;

        shader->stages[1]->bundle[0].image[0] = image;
        shader->stages[1]->active = qtrue;
        shader->stages[1]->rgbGen = CGEN_IDENTITY;
        shader->stages[1]->stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
    } else {
        // two pass lightmap
        shader->stages[0]->bundle[0].image[0] = tr.lightmaps[shader->lightmapIndex];
        shader->stages[0]->bundle[0].isLightmap = qtrue;
        shader->stages[0]->active = qtrue;
        shader->stages[0]->rgbGen = CGEN_IDENTITY;  // lightmaps are scaled on creation
                                                    // for identitylight
        shader->stages[0]->stateBits = GLS_DEFAULT;

        shader->stages[1]->bundle[0].image[0] = image;
        shader->stages[1]->active = qtrue;
        shader->stages[1]->rgbGen = CGEN_IDENTITY;
        shader->stages[1]->stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
    }

    return FinishShader(shader);
}

#include "tr_shader_info.h"

void DumpShaderInfo(shader_info_t *si) {
    showi(si->bundle_target);
    showi(si->image_target);
    showi(si->stages_target);
    showi(si->sky_innerbox);
    showi(si->sky_outerbox);
    if ( si->shader ) {
        if ( si->shader->name[0])  {
            DumpShader(si->shader);
            show(si->shader->name);
            puts("\n");
        }
    } else if ( si->image ) {
        if ( *si->image ) {
            image_t *i = *si->image;
            show(i->imgName);
        }
    }
}

void R_ShaderSetImage (void *ptr, image_t *image) {
    shader_info_t *si = (shader_info_t*) ptr;
    assert(si); assert(image);
    //DumpShaderInfo(si);

    // Com_Printf("%s %s\n", __func__, image->imgName);

    int b = si->bundle_target;
    int i = si->image_target;
    int s = si->stages_target;
    int n = si->sky_innerbox;
    int o = si->sky_outerbox;

    shader_t* shader = si->shader;

    if ( shader ) {
        if ( shader->stages_swapped ) {
            Com_Printf("TODO swap stages\n");
            //assert(0);
        }

        if ( n == 0 && o == 0 ) {
            // setting stage s->bundle b->image
            shader->stages[s]->bundle[b].image[i] = image;
            shader->stages[s]->active = qtrue;
        } else {
            // setting sky box
            if ( o == - 1 ) {
                shader->sky.innerbox[n] = image;
                // TODO shader->stages[s]->active = qtrue; ????
            } else if ( n == -1 ) {
                shader->sky.outerbox[o] = image;
                // TODO shader->stages[s]->active = qtrue; ????
            } else {
                assert(0);
            }
        }

        //FinishShader(shader);

    } else if ( si->image ) {
        *si->image = image;
    } else {
        assert(0);
    }
}

#if 0
static
qhandle_t RE_RegisterShaderFromImage(const char *name, int lightmapIndex, image_t *image, qboolean mipRawImage) {
    assert(0); // TODO was only called from tr_font.c
    int         hash;
    shader_t    *sh;

    hash = generateHashValue(name, FILE_HASH_SIZE);

    // probably not necessary since this function
    // only gets called from tr_font.c with lightmapIndex == LIGHTMAP_2D
    // but better safe than sorry.
    if ( lightmapIndex >= tr.numLightmaps ) {
        lightmapIndex = LIGHTMAP_WHITEIMAGE;
    }

    //
    // see if the shader is already loaded
    //
    for (sh=hashTable[hash]; sh; sh=sh->next) {
        // NOTE: if there was no shader or image available with the name strippedName
        // then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
        // have to check all default shaders otherwise for every call to R_FindShader
        // with that same strippedName a new default shader is created.
        if ( (sh->lightmapIndex == lightmapIndex || sh->defaultShader) &&
                // index by name
                !Q_stricmp(sh->name, name)) {
            // match found
            return sh->index;
        }
    }

    assert(0);

    shader_t *shader = InitShader( name, lightmapIndex );

    //
    // create the default shading commands
    //
    if ( shader->lightmapIndex == LIGHTMAP_NONE ) {
        // dynamic colors at vertexes
        stages[0].bundle[0].image[0] = image;
        stages[0].active = qtrue;
        stages[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
        stages[0].stateBits = GLS_DEFAULT;
    } else if ( shader->lightmapIndex == LIGHTMAP_BY_VERTEX ) {
        // explicit colors at vertexes
        stages[0].bundle[0].image[0] = image;
        stages[0].active = qtrue;
        stages[0].rgbGen = CGEN_EXACT_VERTEX;
        stages[0].alphaGen = AGEN_SKIP;
        stages[0].stateBits = GLS_DEFAULT;
    } else if ( shader->lightmapIndex == LIGHTMAP_2D ) {
        // GUI elements
        stages[0].bundle[0].image[0] = image;
        stages[0].active = qtrue;
        stages[0].rgbGen = CGEN_VERTEX;
        stages[0].alphaGen = AGEN_VERTEX;
        stages[0].stateBits = GLS_DEPTHTEST_DISABLE |
            GLS_SRCBLEND_SRC_ALPHA |
            GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
    } else if ( shader->lightmapIndex == LIGHTMAP_WHITEIMAGE ) {
        // fullbright level
        stages[0].bundle[0].image[0] = tr.whiteImage;
        stages[0].active = qtrue;
        stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
        stages[0].stateBits = GLS_DEFAULT;

        stages[1].bundle[0].image[0] = image;
        stages[1].active = qtrue;
        stages[1].rgbGen = CGEN_IDENTITY;
        stages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
    } else {
        // two pass lightmap
        stages[0].bundle[0].image[0] = tr.lightmaps[shader->lightmapIndex];
        stages[0].bundle[0].isLightmap = qtrue;
        stages[0].active = qtrue;
        stages[0].rgbGen = CGEN_IDENTITY;   // lightmaps are scaled on creation
                                            // for identitylight
        stages[0].stateBits = GLS_DEFAULT;

        stages[1].bundle[0].image[0] = image;
        stages[1].active = qtrue;
        stages[1].rgbGen = CGEN_IDENTITY;
        stages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
    }

    sh = FinishShader();
    return sh->index;
}
#endif

/*
====================
RE_RegisterShaderLightMap

This is the exported shader entry point for the rest of the system
It will always return an index that will be valid.

This should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)

Called by R_RemapShader only
====================
*/
static qhandle_t RE_RegisterShaderLightMap( const char *name, int lightmapIndex ) {
    shader_t    *sh;

    if ( strlen( name ) >= MAX_QPATH ) {
        Com_Printf( "Shader name exceeds MAX_QPATH\n" );
        return 0;
    }

    sh = R_FindShader( name, lightmapIndex, qtrue );

    // we want to return 0 if the shader failed to
    // load for some reason, but R_FindShader should
    // still keep a name allocated for it, so if
    // something calls RE_RegisterShader again with
    // the same name, we don't try looking for it again
    if ( sh->defaultShader ) {
        return 0;
    }

    return sh->index;
}

/*
====================
RE_RegisterShader

This is the exported shader entry point for the rest of the system
It will always return an index that will be valid.

This should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)

There are approx 122 calls

some calls use the return value immediately in the function
change them to find shader, not "create shader" calls
-----------------------------------------------------------

cgame/cg_draw.c:
    RE_RegisterShader( item->icon ) );
    CG_DrawPic( 640 - size, y + ICON_SIZE / 2 - size / 2, size, size, RE_RegisterShader( item->icon ) );
    CG_DrawPic( x, y, 48, 48, RE_RegisterShader("gfx/2d/net.png" ) );

cgame/cg_info.c: ALL but three calls return shader index to global variable
    CG_DrawInformation uses the shader return value immediately in function  <<<<<<<<<<<<<<<<<<<<<<<<<
    make the return value variable a global static var

all others return value to be stored in global variable
---------------------------------------------------------------
cgame/cg_particles.c:
    one call returns shader to global variable.

cgame/cg_players.c:
    2 calls ( currently commented out ) that return shader to global

cgame/cg_weapons.c:
    15 calls *all* return to globals but some returns go to *ALIASES* of globals !!!!
    weaponInfo->ammoIcon   = RE_RegisterShader( item->icon );

    update to
    / *weaponInfo->ammoIcon   = * / RE_RegisterShader( &cg_items[ itemNum ].item.ammoIcon, item->icon );

    as opposed to

    cgs.media.lightningShader = RE_RegisterShader( "lightningBoltNew");
    cgs.media.lightningShader IS A GLOBAL so no problem

cgame/cg_main.c:
    approx 91 calls
    all calls return shader index to a global variable

client/cl_main.c:
    3* calls all return to global

client/cl_scrn.c:
    1 call
    Turn this call into a find shader call instead

    Turn all RE_RegisterShader calls that return to a global Async
    Turn all RE_RegisterShader calls that return to a global Async
    Turn all RE_RegisterShader calls that return to a global Async
====================
*/

qhandle_t RE_RegisterShader( const char *name ) {

    shader_t *sh = R_FindShader( name, LIGHTMAP_2D, qtrue );

    // we want to return 0 if the shader failed to
    // load for some reason, but R_FindShader should
    // still keep a name allocated for it, so if
    // something calls RE_RegisterShader again with
    // the same name, we don't try looking for it again
    if ( sh->defaultShader ) {
        Com_Printf("FAILURE returning %s shader as default shader 0 \n", name);
        //assert(0);
        return 0;
    }

    return sh->index;
}

/*
====================
RE_RegisterShaderNoMip

For menu graphics that should never be picmiped

see RE_RegisterShader
====================
*/
qhandle_t RE_RegisterShaderNoMip( const char *name ) {
    shader_t    *sh = R_FindShader( name, LIGHTMAP_2D, qfalse );

    // we want to return 0 if the shader failed to
    // load for some reason, but R_FindShader should
    // still keep a name allocated for it, so if
    // something calls RE_RegisterShader again with
    // the same name, we don't try looking for it again
    if ( sh->defaultShader ) {
        return 0;
    }

    return sh->index;
}

/*
====================
R_GetShaderByHandle

When a handle is passed in by another module, this range checks
it and returns a valid (possibly default) shader_t to be used internally.
====================
*/
shader_t *R_GetShaderByHandle( qhandle_t hShader ) {
    if ( hShader < 0 ) {
        Com_Printf( "R_GetShaderByHandle: out of range hShader '%d'\n", hShader );
        return tr.defaultShader;
    }
    if ( hShader >= tr.numShaders ) {
        Com_Printf( "R_GetShaderByHandle: out of range hShader '%d'\n", hShader );
        return tr.defaultShader;
    }
    return tr.shaders[hShader];
}

/*
===============
R_ShaderList_f

Dump information on all valid shaders to the console
A second parameter will cause it to print in sorted order
===============
*/
void    R_ShaderList_f (void) {
    int         i;
    int         count;
    shader_t    *shader;

    Com_Printf ("-----------------------\n");

    count = 0;
    for ( i = 0 ; i < tr.numShaders ; i++ ) {
        if ( Cmd_Argc() > 1 ) {
            shader = tr.sortedShaders[i];
        } else {
            shader = tr.shaders[i];
        }

        Com_Printf( "%i ", shader->numUnfoggedPasses );

        if (shader->lightmapIndex >= 0 ) {
            Com_Printf ("L ");
        } else {
            Com_Printf ("  ");
        }
        if ( shader->explicitlyDefined ) {
            Com_Printf( "E " );
        } else {
            Com_Printf( "  " );
        }

        if ( shader->optimalStageIteratorFunc == RB_StageIteratorGeneric ) {
            Com_Printf( "gen " );
        } else if ( shader->optimalStageIteratorFunc == RB_StageIteratorSky ) {
            Com_Printf( "sky " );
        } else {
            Com_Printf( "    " );
        }

        if ( shader->defaultShader ) {
            Com_Printf ( ": %s (DEFAULTED)\n", shader->name);
        } else {
            Com_Printf ( ": %s\n", shader->name);
        }
        count++;
    }
    Com_Printf ( "%i total shaders\n", count);
    Com_Printf ( "------------------\n");
}

static char * strtoken ( char *str, const char *delim ) {
    static char *current_string = NULL;

    if ( ! delim ) {
        return NULL;
    }

    if ( str ) {
        current_string = str;
    }

    if ( ! current_string ) {
        return NULL;
    }

    int strpos = 0;
    char * start = current_string;
    char c;

    if ( ! start || start[0] == '\0' ) {
        return NULL;
    }

    while ( '\0' != ( c = current_string[strpos] ) ) {
        int tpos = 0;
        char tc;
        while ( '\0' != ( tc = delim[tpos] ) ) {
            if ( tc == c ) {
                current_string[strpos] = '\0';
                current_string = &current_string[strpos+1];
                goto out;
            }
            tpos++;
        }
        strpos++;
    }

    if ( c == '\0' ) {
        current_string = NULL;
    }

out:
    return start;
}

// -----------------------------------------------------------------------------------------

#define MAX_SHADER_FILES    4096
static uint shader_buffer_slot = 0;
static char *shader_buffers[MAX_SHADER_FILES] = { NULL };
static char *shader_buffer_names[MAX_SHADER_FILES] = { NULL };
static qboolean shaders_loading_complete = qfalse;

/*
====================
processShaderFiles
Finds and loads all .shader files, combining them into
a single large text block that can be scanned for shader names
=====================
*/
static void processShaderFiles( void )
{
    long sum = 0, summand;

    int numShaderFiles = shader_buffer_slot;

    if ( numShaderFiles == 0 ) {
        Com_Error(ERR_FATAL, "no shaders to process");
    }

    Com_Printf("%d shaders to process\n", numShaderFiles);

    for ( int i = 0; i < numShaderFiles; i++ )
    {
        // Do a simple check on the shader structure in that file to make sure one bad shader file cannot fuck up all other shaders.
        char *p = shader_buffers[i];
        summand = strlen(p);

        COM_BeginParseSession(shader_buffer_names[i]);

        while(1)
        {
            char *token = COM_ParseExt(&p, qtrue);

            if ( ! *token ) {
                break;
            }

            //Com_Printf("token: %s\n", token);

            char shaderName[MAX_QPATH] = { 0 };
            Q_strncpyz(shaderName, token, sizeof(shaderName));
            int shaderLine = COM_GetCurrentParseLine();

            token = COM_ParseExt(&p, qtrue);
            if(token[0] != '{' || token[1] != '\0')
            {
                Com_Printf("WARNING: Ignoring shader.  Shader \"%s\" on line %d missing opening brace '%s' '%s'", shaderName, shaderLine, token, p);

                if (token[0])
                {
                    Com_Printf(" (found \"%s\" on line %d)", token, COM_GetCurrentParseLine());
                }

                Com_Printf(".\n");

                free(shader_buffers[i]);
                shader_buffers[i] = NULL;
                break;
            }

            if(!SkipBracedSection(&p, 1))
            {
                Com_Printf("WARNING: Ignoring shader. Shader \"%s\" on line %d missing closing brace.\n", shaderName, shaderLine);
                free(shader_buffers[i]);
                shader_buffers[i] = NULL;
                break;
            }
        }

        if (shader_buffers[i]) {
            sum += summand;
        }
    }

    if ( s_shaderText != NULL ) {
        Com_Error(ERR_FATAL, "s_shaderText is already set");
    }

    // build single large buffer
    int shader_text_length = sum + numShaderFiles * 2;
    s_shaderText = Hunk_Alloc( shader_text_length, h_low );
    memset(s_shaderText, 0, shader_text_length);// s_shaderText[ 0 ] = '\0';
    char *textEnd = s_shaderText;

    // free in reverse order, so the temp files are all dumped
    for ( int i = numShaderFiles - 1; i >= 0 ; i-- )
    {
        if ( !shader_buffers[i] ) {
            continue;
        }

        strcat( textEnd, shader_buffers[i] );
        strcat( textEnd, "\n" );

        textEnd += strlen( textEnd );

        free ( shader_buffers[i] );

        shader_buffers[i] = NULL;
    }

    COM_Compress( s_shaderText );

    // free up memory
    for ( int i = 0 ; i < numShaderFiles ; i++ ) {
        if ( shader_buffers[i] ) {
            free( shader_buffers[i] );
            shader_buffers[i] = NULL;
        }

        if ( shader_buffer_names[i] ) {
            free( shader_buffer_names[i] );
            shader_buffer_names[i] = NULL;
        }
    }

    int shaderTextHashTableSizes[MAX_SHADERTEXT_HASH];
    Com_Memset(shaderTextHashTableSizes, 0, sizeof(shaderTextHashTableSizes));
    int size = 0;

    char *p = s_shaderText;
    // look for shader names
    while ( 1 ) {
        char *token = COM_ParseExt( &p, qtrue );
        if ( token[0] == 0 ) {
            break;
        }

        int hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
        shaderTextHashTableSizes[hash]++;
        size++;
        SkipBracedSection(&p, 0);
    }

    size += MAX_SHADERTEXT_HASH;

    char *hashMem = Hunk_Alloc( size * sizeof(char *), h_low );

    for (int i = 0; i < MAX_SHADERTEXT_HASH; i++) {
        shaderTextHashTable[i] = (char **) hashMem; // TODO possible alignment fault
        hashMem = ((char *) hashMem) + ((shaderTextHashTableSizes[i] + 1) * sizeof(char *));
    }

    Com_Memset(shaderTextHashTableSizes, 0, sizeof(shaderTextHashTableSizes));

    p = s_shaderText;
    // look for shader names
    while ( 1 ) {
        char *oldp = p;
        char *token = COM_ParseExt( &p, qtrue );

        if ( token[0] == 0 ) {
            break;
        }

        int hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
        shaderTextHashTable[hash][shaderTextHashTableSizes[hash]++] = oldp;

        SkipBracedSection(&p, 0);
    }

    //printf("s_shaderText: '%s'\n", s_shaderText);
    return;
}

static void ScanAndLoadShaderFiles2( emscripten_fetch_t * fetch ) {

    const uint shader_list_len = fetch->numBytes;
    char * shader_list_text    = malloc(shader_list_len + 1);

    if ( ! shader_list_text ) {
        Com_Error(ERR_FATAL, "malloc");
    }

    memcpy(shader_list_text, fetch->data, shader_list_len);
    shader_list_text[shader_list_len] = '\0';

    char *shader_list_token = strtoken(shader_list_text, "\n");

    while ( shader_list_token )
    {
        if ( shader_list_token[0] == '\0' ) {
            Com_Printf("last line of shader list\n");
            break;
        }

        if ( shader_list_token[0] == '\n' ) {
            shader_list_token = strtoken(NULL, "\n");
            continue;
        }

        if ( shader_buffer_slot + 1 >= MAX_SHADER_FILES ) {
            Com_Error(ERR_FATAL, "too many shaders");
        }

        char *file = shader_list_token;

        //Com_Printf("shader from shader.list.%s filename:%s\n", map_name, file);

        void *data;
        int length;

        qboolean loading = FS_GetPreLoadedFile ( file, &data, &length );
        if ( ! loading ) {
            Com_Printf("loading of %s failed\n", file);
            assert(0);
        }

        char *shader_text = malloc(length + 1);

        if ( ! shader_text ) {
            Com_Error(ERR_FATAL, "malloc");
        }

        memcpy(shader_text, data, length);
        shader_text[length] = '\0';

        shader_buffer_names[shader_buffer_slot] = strdup(file);
        shader_buffers[shader_buffer_slot]      = shader_text;

        shader_buffer_slot++;

        shader_list_token = strtoken(NULL, "\n");
    }

    // Com_Printf("shader file list files loading complete\n");

    shaders_loading_complete = qtrue;

    free(shader_list_text);
    shader_list_text = NULL;
}

static void LoadShaderListFile() {
    shader_buffer_slot     = 0;
    memset ( shader_buffers, 0, MAX_SHADER_FILES);
    memset ( shader_buffer_names, 0, MAX_SHADER_FILES);
    shaders_loading_complete = qfalse;

    char buffer[MAX_STRING_CHARS];
    Com_sprintf ( buffer, MAX_STRING_CHARS - 1, "shader.list.%s", map_name);

    //Com_Printf("%s loading %s\n", __func__, buffer);

    // need to call async as the list is generated dynamically like the filelist.mapname
    if ( ! FS_GetFileAsync( buffer, &(emscripten_fetch_callback_t){
        .additionalData   = 0,
        .errFatal         = qtrue,
        .errMessage       = "Couldn't load shader.list.<mapname>",
        .error_callback   = NULL,
        .success_callback = (void(*)(void *))ScanAndLoadShaderFiles2,
        .userData         = NULL,
        .nocache          = qfalse,
    })) {
        Com_Printf("did not attempt to load shader.list");
        assert(0);
    };
}

/*
====================
CreateInternalShaders
====================
*/
static void CreateInternalShaders( void ) {
    tr.numShaders = 0;

    // init the default shader
    shader_t *shader = InitShader( "<default>", LIGHTMAP_NONE );
    shader->stages[0]->bundle[0].image[0] = tr.defaultImage;
    shader->stages[0]->active = qtrue;
    shader->stages[0]->stateBits = GLS_DEFAULT;
    tr.defaultShader = FinishShader(shader);

    assert(tr.defaultShader->finished);

    // shadow shader is just a marker
    shader = InitShader( "<stencil shadow>", LIGHTMAP_NONE );
    shader->stages[0]->bundle[0].image[0] = tr.defaultImage;
    shader->stages[0]->active = qtrue;
    shader->stages[0]->stateBits = GLS_DEFAULT;
    shader->sort = SS_STENCIL_SHADOW;

    tr.shadowShader = FinishShader(shader);

    assert(tr.shadowShader->finished);

    assert(tr.numShaders == 2);
}

static void CreateExternalShaders( void ) {
    // none of the images needed can be found ...

    return; // TODO

#if 0
    tr.projectionShadowShader = R_FindShader( "projectionShadow", LIGHTMAP_NONE, qtrue );
    tr.flareShader = R_FindShader( "flareShader", LIGHTMAP_NONE, qtrue );

    // Hack to make fogging work correctly on flares. Fog colors are calculated
    // in tr_flare.c already.
    if(!tr.flareShader->defaultShader)
    {
        int index;

        for(index = 0; index < tr.flareShader->numUnfoggedPasses; index++)
        {
            tr.flareShader->stages[index]->adjustColorsForFog = ACFF_NONE;
            tr.flareShader->stages[index]->stateBits |= GLS_DEPTHTEST_DISABLE;
        }
    }

    tr.sunShader = R_FindShader( "sun", LIGHTMAP_NONE, qtrue );

    tr.sunFlareShader = R_FindShader( "gfx/2d/sunflare", LIGHTMAP_NONE, qtrue);

    // HACK: if sunflare is missing, make one using the flare image or dlight image
    if (tr.sunFlareShader->defaultShader)
    {
        image_t *image;

        if (!tr.flareShader->defaultShader && tr.flareShader->stages[0] && tr.flareShader->stages[0]->bundle[0].image[0])
            image = tr.flareShader->stages[0]->bundle[0].image[0];
        else
            image = tr.dlightImage;

        InitShader( "gfx/2d/sunflare", LIGHTMAP_NONE );
        shader->_stages[0].bundle[0].image[0] = image;
        shader->_stages[0].active = qtrue;
        shader->_stages[0].stateBits = GLS_DEFAULT;
        tr.sunFlareShader = FinishShader( shader );
    }
#endif
}

/*
==================
R_InitShaders
==================
*/
void R_InitShaders( void ) {
    Com_Printf( "Initializing Shaders\n" );

    Com_Memset(hashTable, 0, sizeof(hashTable));

    CreateInternalShaders();

    LoadShaderListFile();
}

void R_InitShaders2(void) {
    if ( ! shaders_loading_complete ) {
        Com_Printf("shader loading incomplete\n");
        Com_Milliseconds();
        return;
    }

    printf("shaders_loading_complete: %d, shader_buffer_slot: %d\n", shaders_loading_complete, shader_buffer_slot);
    //printf("%s:\n%s\n", shader_buffer_names[0], shader_buffers[0]);

    emscripten_cancel_main_loop();
    processShaderFiles();

    CreateExternalShaders(); // returns as none of the files can be found

    Com_Printf( "Initializing Shaders complete\n" );

    assert(tr.defaultShader->defaultShader == qfalse);
    tr.defaultShader->defaultShader = qtrue;

    void Com_Init5(void);
    emscripten_set_main_loop( Com_Init5, 1, 0);
}

