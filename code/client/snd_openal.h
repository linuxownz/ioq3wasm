#ifndef _SND_OPENAL_H_
#define _SND_OPENAL_H_

sfxHandle_t
     S_AL_RegisterSound( const char *sample);
void S_AL_StartSound( vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx );
void S_AL_StartLocalSound(sfxHandle_t sfx, int channel);
void S_AL_StartBackgroundTrack( const char *intro, const char *loop );
void S_AL_StopBackgroundTrack( void );
void S_AL_RawSamples(int stream, int samples, int rate, int width, int channels, const byte *data, float volume, int entityNum);
void S_AL_StopAllSounds( void );
void S_AL_ClearLoopingSounds( qboolean killall );
void S_AL_AddLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx);
void S_AL_AddRealLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx);
void S_AL_StopLoopingSound(int entityNum );
void S_AL_Respatialize( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater ) ;
void S_AL_UpdateEntityPosition( int entityNum, const vec3_t origin );
void S_AL_Update( void ) ;
void S_AL_DisableSounds( void );
void S_AL_BeginRegistration( void ) ;
void S_AL_ClearSoundBuffer( void );
void S_AL_SoundInfo(void);
void S_AL_SoundList(void);
void S_AL_Shutdown( void );

#endif
