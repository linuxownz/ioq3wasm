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
// cl_cgame.c  -- client system interaction with client game

#include "client.h"
#include "../cgame/cg_public.h" // TODO move around, change name import these

extern qboolean loadCamera(const char *name);
extern void startCamera(int time);
extern qboolean getCameraInfo(int time, vec3_t *origin, vec3_t *angles);

void Con_Close(void);

/*
====================
CL_GetGameState
====================
*/
void CL_GetGameState( gameState_t *gs ) {
    *gs = cl.gameState;
}

/*
====================
CL_GetGlconfig
====================
*/
void CL_GetGlconfig( glconfig_t *glconfig ) {
    *glconfig = cls.glconfig;
}

/*
====================
CL_GetUserCmd
====================
*/
qboolean CL_GetUserCmd( int cmdNumber, usercmd_t *ucmd ) {
    // cmds[cmdNumber] is the last properly generated command

    // can't return anything that we haven't created yet
    if ( cmdNumber > cl.cmdNumber ) {
        Com_Error( ERR_FATAL, "CL_GetUserCmd: %i >= %i", cmdNumber, cl.cmdNumber );
        Com_Error( ERR_DROP, "CL_GetUserCmd: %i >= %i", cmdNumber, cl.cmdNumber );
    }

    // the usercmd has been overwritten in the wrapping
    // buffer because it is too far out of date
    if ( cmdNumber <= cl.cmdNumber - CMD_BACKUP ) {
        return qfalse;
    }

    *ucmd = cl.cmds[ cmdNumber & CMD_MASK ];

    return qtrue;
}

int CL_GetCurrentCmdNumber( void ) {
    return cl.cmdNumber;
}

/*
====================
CL_GetParseEntityState
====================
*/
qboolean CL_GetParseEntityState( int parseEntityNumber, entityState_t *state ) {
    // can't return anything that hasn't been parsed yet
    if ( parseEntityNumber >= cl.parseEntitiesNum ) {
        Com_Error( ERR_DROP, "CL_GetParseEntityState: %i >= %i",
            parseEntityNumber, cl.parseEntitiesNum );
    }

    // can't return anything that has been overwritten in the circular buffer
    if ( parseEntityNumber <= cl.parseEntitiesNum - MAX_PARSE_ENTITIES ) {
        return qfalse;
    }

    *state = cl.parseEntities[ parseEntityNumber & ( MAX_PARSE_ENTITIES - 1 ) ];
    return qtrue;
}

/*
====================
CL_GetCurrentSnapshotNumber
====================
*/
void CL_GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime ) {
    *snapshotNumber = cl.snap.messageNum;
    *serverTime = cl.snap.serverTime;
}

/*
====================
CL_GetSnapshot
====================
*/
qboolean CL_GetSnapshot( int snapshotNumber, snapshot_t *snapshot ) {
    clSnapshot_t    *clSnap;
    int             i, count;

    if ( snapshotNumber > cl.snap.messageNum ) {
        Com_Error( ERR_DROP, "CL_GetSnapshot: snapshotNumber > cl.snapshot.messageNum" );
    }

    // if the frame has fallen out of the circular buffer, we can't return it
    if ( cl.snap.messageNum - snapshotNumber >= PACKET_BACKUP ) {
        return qfalse;
    }

    // if the frame is not valid, we can't return it
    clSnap = &cl.snapshots[snapshotNumber & PACKET_MASK];
    if ( !clSnap->valid ) {
        return qfalse;
    }

    // if the entities in the frame have fallen out of their
    // circular buffer, we can't return it
    if ( cl.parseEntitiesNum - clSnap->parseEntitiesNum >= MAX_PARSE_ENTITIES ) {
        return qfalse;
    }

    // write the snapshot
    snapshot->snapFlags = clSnap->snapFlags;
    snapshot->serverCommandSequence = clSnap->serverCommandNum;
    snapshot->ping = clSnap->ping;
    snapshot->serverTime = clSnap->serverTime;
    Com_Memcpy( snapshot->areamask, clSnap->areamask, sizeof( snapshot->areamask ) );
    snapshot->ps = clSnap->ps;
    count = clSnap->numEntities;
    if ( count > MAX_ENTITIES_IN_SNAPSHOT ) {
        Com_DPrintf( "CL_GetSnapshot: truncated %i entities to %i\n", count, MAX_ENTITIES_IN_SNAPSHOT );
        count = MAX_ENTITIES_IN_SNAPSHOT;
    }
    snapshot->numEntities = count;
    for ( i = 0 ; i < count ; i++ ) {
        snapshot->entities[i] =
            cl.parseEntities[ ( clSnap->parseEntitiesNum + i ) & (MAX_PARSE_ENTITIES-1) ];
    }

    // FIXME: configstring changes and server commands!!!

    return qtrue;
}

/*
=====================
CL_SetUserCmdValue
=====================
*/
void CL_SetUserCmdValue( int userCmdValue, float sensitivityScale ) {
    cl.cgameUserCmdValue = userCmdValue;
    cl.cgameSensitivity = sensitivityScale;
}

/*
=====================
CL_AddCgameCommand
=====================
*/
void CL_AddCgameCommand( const char *cmdName ) {
    Cmd_AddCommand( cmdName, NULL );
}

/*
=====================
CL_ConfigstringModified
=====================
*/
void CL_ConfigstringModified( void ) {
    char        *old, *s;
    int         i, index;
    char        *dup;
    gameState_t oldGs;
    int         len;

    index = atoi( Cmd_Argv(1) );
    if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
        Com_Error( ERR_DROP, "CL_ConfigstringModified: bad index %i", index );
    }
    // get everything after "cs <num>"
    s = Cmd_ArgsFrom(2);

    old = cl.gameState.stringData + cl.gameState.stringOffsets[ index ];
    if ( !strcmp( old, s ) ) {
        return;     // unchanged
    }

    // build the new gameState_t
    oldGs = cl.gameState;

    Com_Memset( &cl.gameState, 0, sizeof( cl.gameState ) );

    // leave the first 0 for uninitialized strings
    cl.gameState.dataCount = 1;

    for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
        if ( i == index ) {
            dup = s;
        } else {
            dup = oldGs.stringData + oldGs.stringOffsets[ i ];
        }
        if ( !dup[0] ) {
            continue;       // leave with the default empty string
        }

        len = strlen( dup );

        if ( len + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
            Com_Error( ERR_DROP, "MAX_GAMESTATE_CHARS exceeded" );
        }

        // append it to the gameState string buffer
        cl.gameState.stringOffsets[ i ] = cl.gameState.dataCount;
        Com_Memcpy( cl.gameState.stringData + cl.gameState.dataCount, dup, len + 1 );
        cl.gameState.dataCount += len + 1;
    }

    if ( index == CS_SYSTEMINFO ) {
        // parse serverId and other cvars
        CL_SystemInfoChanged();
    }
}

/*
===================
CL_GetServerCommand
Set up argc/argv for the given command
===================
*/
qboolean CL_GetServerCommand( int serverCommandNumber ) {
    char    *s;
    char    *cmd;
    static char bigConfigString[BIG_INFO_STRING];
    int argc;

    // if we have irretrievably lost a reliable command, drop the connection
    if ( serverCommandNumber <= clc.serverCommandSequence - MAX_RELIABLE_COMMANDS ) {
        // when a demo record was started after the client got a whole bunch of
        // reliable commands then the client never got those first reliable commands
        Com_Error( ERR_DROP, "CL_GetServerCommand: a reliable command was cycled out" );
        return qfalse;
    }

    if ( serverCommandNumber > clc.serverCommandSequence ) {
        Com_Error( ERR_DROP, "CL_GetServerCommand: requested a command not received" );
        return qfalse;
    }

    s = clc.serverCommands[ serverCommandNumber & ( MAX_RELIABLE_COMMANDS - 1 ) ];
    clc.lastExecutedServerCommand = serverCommandNumber;

    Com_DPrintf( "serverCommand: %i : %s\n", serverCommandNumber, s );
rescan:
    Cmd_TokenizeString( s );
    cmd = Cmd_Argv(0);
    argc = Cmd_Argc();

    if ( !strcmp( cmd, "disconnect" ) ) {
        // https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=552
        // allow server to indicate why they were disconnected
        if ( argc >= 2 )
            Com_Error( ERR_SERVERDISCONNECT, "Server disconnected - %s", Cmd_Argv( 1 ) );
        else
            Com_Error( ERR_SERVERDISCONNECT, "Server disconnected" );
    }

    if ( !strcmp( cmd, "bcs0" ) ) {
        Com_sprintf( bigConfigString, BIG_INFO_STRING, "cs %s \"%s", Cmd_Argv(1), Cmd_Argv(2) );
        return qfalse;
    }

    if ( !strcmp( cmd, "bcs1" ) ) {
        s = Cmd_Argv(2);
        if( strlen(bigConfigString) + strlen(s) >= BIG_INFO_STRING ) {
            Com_Error( ERR_DROP, "bcs exceeded BIG_INFO_STRING" );
        }
        strcat( bigConfigString, s );
        return qfalse;
    }

    if ( !strcmp( cmd, "bcs2" ) ) {
        s = Cmd_Argv(2);
        if( strlen(bigConfigString) + strlen(s) + 1 >= BIG_INFO_STRING ) {
            Com_Error( ERR_DROP, "bcs exceeded BIG_INFO_STRING" );
        }
        strcat( bigConfigString, s );
        strcat( bigConfigString, "\"" );
        s = bigConfigString;
        goto rescan;
    }

    if ( !strcmp( cmd, "cs" ) ) {
        CL_ConfigstringModified();
        // reparse the string, because CL_ConfigstringModified may have done another Cmd_TokenizeString()
        Cmd_TokenizeString( s );
        return qtrue;
    }

    if ( !strcmp( cmd, "map_restart" ) ) {
        // clear notify lines and outgoing commands before passing
        // the restart to the cgame
        // Con_ClearNotify(); // TODO
        // reparse the string, because Con_ClearNotify() may have done another Cmd_TokenizeString()
        Cmd_TokenizeString( s );
        Com_Memset( cl.cmds, 0, sizeof( cl.cmds ) );
        return qtrue;
    }

    // the clientLevelShot command is used during development
    // to generate 128*128 screenshots from the intermission
    // point of levels for the menu system to use
    // we pass it along to the cgame to make appropriate adjustments,
    // but we also clear the console and notify lines here
    if ( !strcmp( cmd, "clientLevelShot" ) ) {
        // don't do it if we aren't running the server locally,
        // otherwise malicious remote servers could overwrite
        // the existing thumbnails

        // close the console
        // Con_Close(); // TODO
        // take a special screenshot next frame
        // Cbuf_AddText( "wait ; wait ; wait ; wait ; screenshot levelshot\n" ); // TODO
        return qtrue;
    }

    // we may want to put a "connect to other server" command here

    // cgame can now act on the command
    return qtrue;
}

/*
====================
CL_ShutdonwCGame
====================
*/
void CG_Shutdown( void );
void CL_ShutdownCGame( void ) {
    CG_Shutdown(); // vmMain(CG_SHUTDOWN, 0, 0, 0);
    cls.cgameStarted = qfalse;
}

/*
====================
CL_InitCGame
Should only be called by CL_StartHunkUsers
BUT IT'S NOT CALLED BY CL_StartHunkUsers
IT'S CALLED BY
    CL_DownloadsComplete
    CL_Vid_Restart_f
====================
*/

int t1, t2; // TODO

void CL_InitCGame( void ) {

    clc.state = CA_LOADING;

    t1 = Sys_Milliseconds();

    Con_Close(); // TODO put away the console

    // find the current mapname
    const char *info    = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
    const char *mapname = Info_ValueForKey( info, "mapname" );

    Com_sprintf( cl.mapname, sizeof( cl.mapname ), "maps/%s.bsp", mapname );

    // init for this gamestate
    // use the lastExecutedServerCommand instead of the serverCommandSequence
    // otherwise server commands sent just before a gamestate are dropped
    CG_Init( clc.serverMessageSequence, clc.lastExecutedServerCommand, clc.clientNum ); // VM_Call( cgvm, CG_INIT, clc.serverMessageSequence, clc.lastExecutedServerCommand, clc.clientNum );
}

void CL_InitCGame3(void) {
LINE;
    Com_Printf("TODO add check that CG_Init is complete\n");

    // TODO some check that CG_Init has finished loading

    emscripten_cancel_main_loop();

    // reset any CVAR_CHEAT cvars registered by cgame
    if ( !cl_connectedToCheatServer ) {
        Cvar_SetCheatState();
    }

    // we will send a usercmd this frame, which
    // will cause the server to send us the first snapshot
    clc.state = CA_PRIMED;

    t2 = Sys_Milliseconds();

    Com_Printf( "CL_InitCGame: %5.2f seconds\n", (t2-t1)/1000.0 );

    // have the renderer touch all its images, so they are present
    // on the card even if the driver does deferred loading
    RE_EndRegistration();

    // make sure everything is paged in
    /*if (!Sys_LowPhysicalMemory()) {
        Com_TouchMemory();
    }*/

    // clear anything that got printed
    // Con_ClearNotify (); // TODO

    emscripten_set_main_loop(Com_Frame, -1, 0);
}

/*
====================
CL_GameCommand

See if the current console command is claimed by the cgame
====================
*/
qboolean CL_GameCommand( void ) {
    return CG_ConsoleCommand(); // return VM_Call( cgvm, CG_CONSOLE_COMMAND ); // TODO
}

/*
=====================
CL_CGameRendering
=====================
*/
void CL_CGameRendering( stereoFrame_t stereo ) {
    CG_DrawActiveFrame( cl.serverTime, stereo );// VM_Call( cgvm, CG_DRAW_ACTIVE_FRAME, cl.serverTime, stereo, clc.demoplaying );
}

/*
=================
CL_AdjustTimeDelta

Adjust the clients view of server time.

We attempt to have cl.serverTime exactly equal the server's view
of time plus the timeNudge, but with variable latencies over
the internet it will often need to drift a bit to match conditions.

Our ideal time would be to have the adjusted time approach, but not pass,
the very latest snapshot.

Adjustments are only made when a new snapshot arrives with a rational
latency, which keeps the adjustment process framerate independent and
prevents massive overadjustment during times of significant packet loss
or bursted delayed packets.
=================
*/

#define RESET_TIME  500

void CL_AdjustTimeDelta( void ) {
    int     newDelta;
    int     deltaDelta;

    cl.newSnapshots = qfalse;

    newDelta = cl.snap.serverTime - cls.realtime;
    deltaDelta = abs( newDelta - cl.serverTimeDelta );

    if ( deltaDelta > RESET_TIME ) {
        cl.serverTimeDelta = newDelta;
        cl.oldServerTime = cl.snap.serverTime;  // FIXME: is this a problem for cgame?
        cl.serverTime = cl.snap.serverTime;
        if ( cl_showTimeDelta->integer ) {
            Com_Printf( "<RESET> " );
        }
    } else if ( deltaDelta > 100 ) {
        // fast adjust, cut the difference in half
        if ( cl_showTimeDelta->integer ) {
            Com_Printf( "<FAST> " );
        }
        cl.serverTimeDelta = ( cl.serverTimeDelta + newDelta ) >> 1;
    } else {
        // slow drift adjust, only move 1 or 2 msec

        // if any of the frames between this and the previous snapshot
        // had to be extrapolated, nudge our sense of time back a little
        // the granularity of +1 / -2 is too high for timescale modified frametimes
        if ( com_timescale->value == 0 || com_timescale->value == 1 ) {
            if ( cl.extrapolatedSnapshot ) {
                cl.extrapolatedSnapshot = qfalse;
                cl.serverTimeDelta -= 2;
            } else {
                // otherwise, move our sense of time forward to minimize total latency
                cl.serverTimeDelta++;
            }
        }
    }

    if ( cl_showTimeDelta->integer ) {
        Com_Printf( "%i ", cl.serverTimeDelta );
    }
}

void dumpclstate(void){
    printf("client active:\n");
    printf("cl.snap.cmdNum %d\n",   cl.snap.cmdNum);
    printf("cl.snap.deltaNum %d\n", cl.snap.deltaNum);
    printf("cl.snap.messageNum %d\n", cl.snap.messageNum);
    printf("cl.snap.numEntities %d\n", cl.snap.numEntities);
    printf("cl.snap.ping %d\n", cl.snap.ping);
    printf("cl.snap.serverCommandNum %d\n", cl.snap.serverCommandNum);
    printf("cl.snap.snapFlags %d\n", cl.snap.snapFlags);
    printf("cl.snap.valid %d\n", cl.snap.valid);
    printf("\n");
}
/*
==================
CL_FirstSnapshot
==================
*/
static void CL_FirstSnapshot( void ) {
    // ignore snapshots that don't have entities
    if ( cl.snap.snapFlags & SNAPFLAG_NOT_ACTIVE ) {
        //dumpclstate();
        return;
    }

    clc.state = CA_ACTIVE; // The only place this is set

    // set the timedelta so we are exactly on this first frame
    cl.serverTimeDelta = cl.snap.serverTime - cls.realtime;
    cl.oldServerTime = cl.snap.serverTime;

    clc.timeDemoBaseTime = cl.snap.serverTime; // TODO

    // if this is the first frame of active play,
    // execute the contents of activeAction now
    // this is to allow scripting a timedemo to start right
    // after loading
    if ( cl_activeAction->string[0] ) {
        Cbuf_AddText( cl_activeAction->string );
        Cvar_Set( "activeAction", "" );
    }
}

/*
==================
CL_SetCGameTime
==================
*/
void CL_SetCGameTime( void ) {

    // getting a valid frame message ends the connection process
    if ( clc.state != CA_ACTIVE ) {

        if ( clc.state != CA_PRIMED ) {
            return;
        }

        if ( cl.newSnapshots ) {
            cl.newSnapshots = qfalse;
            CL_FirstSnapshot();
        }

        if ( clc.state != CA_ACTIVE ) {
            return;
        }
    }

    // if we have gotten to this point, cl.snap is guaranteed to be valid
    if ( !cl.snap.valid ) {
        Com_Error( ERR_DROP, "CL_SetCGameTime: !cl.snap.valid" );
    }

    if ( cl.snap.serverTime < cl.oldFrameServerTime ) {
        Com_Error( ERR_DROP, "cl.snap.serverTime < cl.oldFrameServerTime" );
    }

    cl.oldFrameServerTime = cl.snap.serverTime;

    // get our current view of time
    {
        // cl_timeNudge is a user adjustable cvar that allows more
        // or less latency to be added in the interest of better
        // smoothness or better responsiveness.
        int tn;

        tn = cl_timeNudge->integer;
        if (tn<-30) {
            tn = -30;
        } else if (tn>30) {
            tn = 30;
        }

        cl.serverTime = cls.realtime + cl.serverTimeDelta - tn;

        // guarantee that time will never flow backwards, even if
        // serverTimeDelta made an adjustment or cl_timeNudge was changed
        if ( cl.serverTime < cl.oldServerTime ) {
            cl.serverTime = cl.oldServerTime;
        }
        cl.oldServerTime = cl.serverTime;

        // note if we are almost past the latest frame (without timeNudge),
        // so we will try and adjust back a bit when the next snapshot arrives
        if ( cls.realtime + cl.serverTimeDelta >= cl.snap.serverTime - 5 ) {
            cl.extrapolatedSnapshot = qtrue;
        }
    }

    // if we have gotten new snapshots, drift serverTimeDelta
    // don't do this every frame, or a period of packet loss would
    // make a huge adjustment
    if ( cl.newSnapshots ) {
        CL_AdjustTimeDelta();
    }
}

