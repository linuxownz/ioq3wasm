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
#include "snd_local.h"
#include "snd_public.h"
#include "snd_openal.h"

cvar_t *s_volume;
cvar_t *s_muted;
cvar_t *s_musicVolume;
cvar_t *s_doppler;
cvar_t *s_backend;
cvar_t *s_muteWhenMinimized;
cvar_t *s_muteWhenUnfocused;

/*
=================
S_StartSound
=================
*/
void S_StartSound( vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx )
{
    S_AL_StartSound( origin, entnum, entchannel, sfx );
}

/*
=================
S_StartLocalSound
=================
*/
void S_StartLocalSound( sfxHandle_t sfx, int channelNum )
{
    S_AL_StartLocalSound( sfx, channelNum );
}

/*
=================
S_StartBackgroundTrack
=================
*/
void S_StartBackgroundTrack( const char *intro, const char *loop )
{
    S_AL_StartBackgroundTrack( intro, loop );
}

/*
=================
S_StopBackgroundTrack
=================
*/
void S_StopBackgroundTrack( void )
{
    S_AL_StopBackgroundTrack( );
}

/*
=================
S_RawSamples
=================
*/
void S_RawSamples (int stream, int samples, int rate, int width, int channels, const byte *data, float volume, int entityNum)
{
    S_AL_RawSamples(stream, samples, rate, width, channels, data, volume, entityNum);
}

/*
=================
S_StopAllSounds
=================
*/
void S_StopAllSounds( void )
{
    S_AL_StopAllSounds( );
}

/*
=================
S_ClearLoopingSounds
=================
*/
void S_ClearLoopingSounds( qboolean killall )
{
    S_AL_ClearLoopingSounds( killall );
}

/*
=================
S_AddLoopingSound
=================
*/
void S_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx )
{
    S_AL_AddLoopingSound( entityNum, origin, velocity, sfx );
}

/*
=================
S_AddRealLoopingSound
=================
*/
void S_AddRealLoopingSound( int entityNum, const vec3_t origin,
        const vec3_t velocity, sfxHandle_t sfx )
{
    S_AL_AddRealLoopingSound( entityNum, origin, velocity, sfx );
}

/*
=================
S_StopLoopingSound
=================
*/
void S_StopLoopingSound( int entityNum )
{
    S_AL_StopLoopingSound( entityNum );
}

/*
=================
S_Respatialize
=================
*/
void S_Respatialize( int entityNum, const vec3_t origin,
        vec3_t axis[3], int inwater )
{
    S_AL_Respatialize( entityNum, origin, axis, inwater );
}

/*
=================
S_UpdateEntityPosition
=================
*/
void S_UpdateEntityPosition( int entityNum, const vec3_t origin )
{
    S_AL_UpdateEntityPosition( entityNum, origin );
}

/*
=================
S_Update
=================
*/
void S_Update( void )
{
    if(s_muted->integer)
    {
        if(!(s_muteWhenMinimized->integer && com_minimized->integer) &&
           !(s_muteWhenUnfocused->integer && com_unfocused->integer))
        {
            s_muted->integer = qfalse;
            s_muted->modified = qtrue;
        }
    }
    else
    {
        if((s_muteWhenMinimized->integer && com_minimized->integer) ||
           (s_muteWhenUnfocused->integer && com_unfocused->integer))
        {
            s_muted->integer = qtrue;
            s_muted->modified = qtrue;
        }
    }

    S_AL_Update( );
}

/*
=================
S_DisableSounds
=================
*/
void S_DisableSounds( void )
{
    S_AL_DisableSounds( );
}

/*
=================
S_BeginRegistration
=================
*/
void S_BeginRegistration( void )
{
    S_AL_BeginRegistration( );
}

/*
=================
S_RegisterSound
=================
*/
sfxHandle_t S_RegisterSound( const char *sample ) {
    return S_AL_RegisterSound( sample );
}

/*
=================
S_ClearSoundBuffer
=================
*/
void S_ClearSoundBuffer( void )
{
    S_AL_ClearSoundBuffer( );
}

/*
=================
S_SoundInfo
=================
*/
void S_SoundInfo( void )
{
    S_AL_SoundInfo( );
}

/*
=================
S_SoundList
=================
*/
void S_SoundList( void )
{
    S_AL_SoundList( );
}


//=============================================================================

/*
=================
S_Play_f
=================
*/
void S_Play_f( void ) {
    int         i;
    int         c;
    sfxHandle_t h;

    c = Cmd_Argc();

    if( c < 2 ) {
        Com_Printf ("Usage: play <sound filename> [sound filename] [sound filename] ...\n");
        return;
    }

    for( i = 1; i < c; i++ ) {
        h = S_AL_RegisterSound( Cmd_Argv(i));

        if( h ) {
            S_AL_StartLocalSound( h, CHAN_LOCAL_SOUND );
        }
    }
}

/*
=================
S_Music_f
=================
*/
void S_Music_f( void ) {
    int c = Cmd_Argc();

    if ( c == 2 ) {
        S_AL_StartBackgroundTrack( Cmd_Argv(1), NULL );
    } else if ( c == 3 ) {
        S_AL_StartBackgroundTrack( Cmd_Argv(1), Cmd_Argv(2) );
    } else {
        Com_Printf ("Usage: music <musicfile> [loopfile]\n");
        return;
    }

}

/*
=================
S_Music_f
=================
*/
void S_StopMusic_f( void )
{
    S_AL_StopBackgroundTrack();
}


//=============================================================================

/*
=================
S_Init
=================
*/
void S_Init( void )
{
    cvar_t      *cv;
    qboolean    started = qfalse;

    Com_Printf( "------ Initializing Sound ------\n" );

    s_volume            = Cvar_Get( "s_volume",          "0.5", CVAR_ARCHIVE );
    s_musicVolume       = Cvar_Get( "s_musicvolume",     "0.5", CVAR_ARCHIVE );
    s_muted             = Cvar_Get( "s_muted",             "0", CVAR_ROM);
    s_doppler           = Cvar_Get( "s_doppler",           "1", CVAR_ARCHIVE );
    s_backend           = Cvar_Get( "s_backend",           "",  CVAR_ROM );
    s_muteWhenMinimized = Cvar_Get( "s_muteWhenMinimized", "1", CVAR_ARCHIVE );
    s_muteWhenUnfocused = Cvar_Get( "s_muteWhenUnfocused", "1", CVAR_ARCHIVE );

    cv = Cvar_Get( "s_initsound", "1", 0 );

    if( !cv->integer ) {
        Com_Printf( "Sound disabled.\n" );
        Com_Printf( "--------------------------------\n");
        return;
    }

    S_CodecInit( );

    Cmd_AddCommand( "play", S_Play_f );
    Cmd_AddCommand( "music", S_Music_f );
    Cmd_AddCommand( "stopmusic", S_StopMusic_f );
    Cmd_AddCommand( "s_list", S_SoundList );
    Cmd_AddCommand( "s_stop", S_StopAllSounds );
    Cmd_AddCommand( "s_info", S_SoundInfo );

    cv = Cvar_Get( "s_useOpenAL", "1", CVAR_ARCHIVE | CVAR_LATCH );
    if( cv->integer ) {
        started = S_AL_Init( );
        Cvar_Set( "s_backend", "OpenAL" );
    } else {
        Com_Error(ERR_FATAL, "must use OpenAL s_useOpenAL");
    }

    if( started ) {
        S_SoundInfo( );
        Com_Printf( "Sound initialization successful.\n" );
    } else {
        Com_Printf( "Sound initialization failed.\n" );
    }

    Com_Printf( "--------------------------------\n");
}

/*
=================
S_Shutdown
=================
*/
void S_Shutdown( void )
{
    S_AL_Shutdown();

    Cmd_RemoveCommand( "play" );
    Cmd_RemoveCommand( "music");
    Cmd_RemoveCommand( "stopmusic");
    Cmd_RemoveCommand( "s_list" );
    Cmd_RemoveCommand( "s_stop" );
    Cmd_RemoveCommand( "s_info" );

    S_CodecShutdown( );
}

