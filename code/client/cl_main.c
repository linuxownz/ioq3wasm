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
// cl_main.c  -- client main loop

#include "client.h"
#include <limits.h>
#include "../qcommon/net_ws_client.h"

#include "emscripten.h"
#include "emscripten/html5.h"

static void CL_ServerInfoPacket( netadr_t from, msg_t *msg );
const char* ConnStateToString( const connstate_t connstate );
void Con_Init(void);
void Con_Close(void);

cvar_t  *cl_nodelta;
cvar_t  *cl_debugMove;

cvar_t  *cl_noprint;

cvar_t  *cl_timeout;
cvar_t  *cl_maxpackets;
cvar_t  *cl_packetdup;
cvar_t  *cl_timeNudge;
cvar_t  *cl_showTimeDelta;
//cvar_t    *cl_freezeDemo;

cvar_t  *cl_shownet;
cvar_t  *cl_showSend;
//cvar_t    *cl_timedemo;
//cvar_t    *cl_timedemoLog;
//cvar_t    *cl_autoRecordDemo;
cvar_t  *cl_aviFrameRate;
cvar_t  *cl_aviMotionJpeg;
//cvar_t    *cl_forceavidemo;

cvar_t  *cl_freelook;
cvar_t  *cl_sensitivity;

cvar_t  *cl_mouseAccel;
cvar_t  *cl_mouseAccelOffset;
cvar_t  *cl_mouseAccelStyle;
cvar_t  *cl_showMouseRate;

cvar_t  *m_pitch;
cvar_t  *m_yaw;
cvar_t  *m_forward;
cvar_t  *m_side;
cvar_t  *m_filter;

cvar_t  *j_pitch;
cvar_t  *j_yaw;
cvar_t  *j_forward;
cvar_t  *j_side;
cvar_t  *j_up;
cvar_t  *j_pitch_axis;
cvar_t  *j_yaw_axis;
cvar_t  *j_forward_axis;
cvar_t  *j_side_axis;
cvar_t  *j_up_axis;

cvar_t  *cl_activeAction;

cvar_t  *cl_motdString;

cvar_t  *cl_allowDownload; // TODO remove always download
cvar_t  *cl_conXOffset;

cvar_t  *cl_serverStatusResendTime;

cvar_t  *cl_guidServerUniq;

cvar_t  *cl_consoleKeys;

cvar_t  *cl_rate;

clientActive_t      cl;
clientConnection_t  clc;
clientStatic_t      cls;

char cl_reconnectArgs[MAX_OSPATH];

ping_t  cl_pinglist[MAX_PINGREQUESTS];

typedef struct serverStatus_s
{
    char string[BIG_INFO_STRING];
    netadr_t address;
    int time, startTime;
    qboolean pending;
    qboolean print;
    qboolean retrieved;
} serverStatus_t;

serverStatus_t cl_serverStatusList[MAX_SERVERSTATUSREQUESTS];

static int noGameRestart = qfalse;

void CL_CheckForResend( void );
void CL_ServerStatusResponse( netadr_t from, msg_t *msg );

/*
=======================================================================

CLIENT RELIABLE COMMAND COMMUNICATION

=======================================================================
*/

/*
======================
CL_AddReliableCommand

The given command will be transmitted to the server, and is guaranteed to
not have future usercmd_t executed before it is executed
======================
*/
void CL_AddReliableCommand(const char *cmd, qboolean isDisconnectCmd)
{
    int unacknowledged = clc.reliableSequence - clc.reliableAcknowledge;

    // if we would be losing an old command that hasn't been acknowledged,
    // we must drop the connection
    // also leave one slot open for the disconnect command in this case.

    if ((isDisconnectCmd && unacknowledged > MAX_RELIABLE_COMMANDS) ||
        (!isDisconnectCmd && unacknowledged >= MAX_RELIABLE_COMMANDS))
    {
        if(com_errorEntered)
            return;
        else
            Com_Error(ERR_DROP, "Client command overflow");
    }

    Q_strncpyz(clc.reliableCommands[++clc.reliableSequence & (MAX_RELIABLE_COMMANDS - 1)], cmd, sizeof(*clc.reliableCommands));
}

//======================================================================

/*
=====================
CL_ShutdownAll
=====================
*/
void CL_ShutdownAll(qboolean shutdownRef)
{
    // clear sounds
    S_DisableSounds();

    // shutdown CGame
    CL_ShutdownCGame();

    // shutdown UI
    // CL_ShutdownUI(); // TODO

    // shutdown the renderer
    RE_Shutdown(qfalse);        // don't destroy window or context

    cls.uiStarted = qfalse;
    cls.cgameStarted = qfalse;
    cls.rendererStarted = qfalse;
    cls.soundRegistered = qfalse;
}

/*
=================
CL_ClearMemory

Called by Com_GameRestart
=================
*/
void CL_ClearMemory(qboolean shutdownRef)
{
    // shutdown all the client stuff
    CL_ShutdownAll(shutdownRef);

    // if not running a server clear the whole hunk
    if ( 1 ) { //!com _sv_running || !com _sv_running->integer )
        // clear the whole hunk
        Hunk_Clear();
        // clear collision map data
        CM_ClearMap();
    }
    else {
        // clear all the client data on the hunk
        Hunk_ClearToMark();
    }
}

/*
=================
CL_FlushMemory

Called by CL_MapLoading, CL_Connect_f, CL_PlayDemo_f, and CL_ParseGamestate
the only way a client gets into a game
Also called by Com_Error
=================
*/
void CL_FlushMemory(void)
{
    CL_ClearMemory(qfalse);
    CL_StartHunkUsers(qfalse);
}

/*
=====================
CL_MapLoading

A local server is starting to load a map, so update the
screen to let the user know about it, then dump all client
memory on the hunk from cgame, ui, and renderer
=====================
*/
#if 0
void CL_MapLoading( void ) {
    // THIS IS DEDICATED ONLY NOTHING CLIENT SIDE CALLS THIS
    if ( com_dedicated->integer ) {
        Com_Error(ERR_FATAL, "client not dedicated REMOVE");
        clc.state = CA_DISCONNECTED;
        ////Key_SetCatcher( KEYCATCH_CONSOLE );
        return;
    }

    if ( !com_cl_running->integer ) {
        Com_Error(ERR_FATAL, "client not running");
        return;
    }

    //Con_Close();
    //Key_SetCatcher( 0 );


    // if we are already connected to the local host, stay connected
    if ( clc.state >= CA_CONNECTED && !Q_stricmp( clc.servername, "localhost" ) ) {
        clc.state = CA_CONNECTED;       // so the connect screen is drawn
        Com_Memset( cls.updateInfoString, 0, sizeof( cls.updateInfoString ) );
        Com_Memset( clc.serverMessage, 0, sizeof( clc.serverMessage ) );
        Com_Memset( &cl.gameState, 0, sizeof( cl.gameState ) );
        clc.lastPacketSentTime = -9999;
        SCR_UpdateScreen();
    } else {
        // clear nextmap so the cinematic shutdown doesn't execute it
        Cvar_Set( "nextmap", "" );
        CL_Disconnect( qtrue );
        Q_strncpyz( clc.servername, "localhost", sizeof(clc.servername) );
        clc.state = CA_CHALLENGING;     // so the connect screen is drawn
        //Key_SetCatcher( 0 );
        SCR_UpdateScreen();
        clc.connectTime = -RETRANSMIT_TIMEOUT;

        Com_Error(ERR_FATAL, "we shouldn't get here\n");

        //NET_StringToAdr( clc.servername, &clc.serverAddress, NA_UNSPEC);
        // we don't need a challenge on the localhost

        CL_CheckForResend();
    }
}
#endif

/*
=====================
CL_ClearState
Called before parsing a gamestate
=====================
*/
void CL_ClearState (void) {
    S_StopAllSounds();
    Com_Memset( &cl, 0, sizeof( cl ) ); // TODO clears cl
}

/*
=====================
CL_Disconnect

Called when a connection, demo, or cinematic is being terminated.
Goes from a connected state to either a menu state or a console state
Sends a disconnect message to the server
This is also called on Com_Error and Com_Quit, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect( qboolean showMainMenu ) {
    UNUSED(showMainMenu);

    if ( !com_cl_running || !com_cl_running->integer ) {
        return;
    }

    // shutting down the client so leave full screen mode
    Cvar_Set("r_uiFullScreen", "0"); // TODO

#if 0
// TODO
if ( showMainMenu ) {
    VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_NONE );
}
#endif

    S_ClearSoundBuffer();

    // send a disconnect message to the server
    // send it a few times in case one is dropped
    if ( clc.state >= CA_CONNECTED ) {
        CL_AddReliableCommand("disconnect", qtrue);
        CL_WritePacket();
        CL_WritePacket();
        CL_WritePacket();
    }

    CL_ClearState ();

    // wipe the client connection
    Com_Memset( &clc, 0, sizeof( clc ) );// TODO clears clc

    clc.state = CA_DISCONNECTED;

    // not connected to a pure server anymore
    cl_connectedToPureServer = qtrue; // TODO changed to true
}

/*
===================
CL_ForwardCommandToServer

adds the current command line as a clientCommand
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void CL_ForwardCommandToServer( const char *string ) {
    char *cmd = Cmd_Argv(0);

    // ignore key up commands
    if ( cmd[0] == '-' ) {
        return;
    }

    if ( clc.demoplaying || clc.state < CA_CONNECTED || cmd[0] == '+' ) {
        Com_Printf ("Unknown command \"%s" S_COLOR_WHITE "\"\n", cmd);
        return;
    }

    if ( Cmd_Argc() > 1 ) {
        CL_AddReliableCommand(string, qfalse);
    } else {
        CL_AddReliableCommand(cmd, qfalse);
    }
}

/*
======================================================================

CONSOLE COMMANDS

======================================================================
*/

/*
==================
CL_ForwardToServer_f
==================
*/
void CL_ForwardToServer_f( void ) {
    if ( clc.state != CA_ACTIVE || clc.demoplaying ) {
        Com_Printf ("Not connected to a server.\n");
        return;
    }

    // don't forward the first argument
    if ( Cmd_Argc() > 1 ) {
        CL_AddReliableCommand(Cmd_Args(), qfalse);
    }
}

/*
==================
CL_Disconnect_f
==================
*/
void CL_Disconnect_f( void ) {
    if ( clc.state != CA_DISCONNECTED && clc.state != CA_CINEMATIC ) {
        Com_Error (ERR_DISCONNECT, "Disconnected from server");
    }
}

/*
================
CL_Reconnect_f

================
*/
void CL_Reconnect_f( void ) {
    if ( ! strlen( cl_reconnectArgs ) ) {
        return;
    }

    Com_Printf("reconnect disabled\n");
    //Cbuf_AddText( va("connect %s\n", cl_reconnectArgs ) );
}

static const char * CL_BeforeUnload(int eventType, const void *reserved, void *userData) {
    CL_Disconnect(qfalse);
    return "";
    //return "Are you sure?";
}

static void CL_Connected_f(void) {
    Com_Printf("%s connected\n", __func__);
    clc.state = CA_CONNECTING;

    emscripten_set_beforeunload_callback ( NULL, CL_BeforeUnload );
}

/*
================
CL_Connect_f
================
*/

static void CL_Connect_f( void ) {
    // TODO for websocket, just need the port, can get website address from browser, but check against mine to stop pointing elsewhere

    // clear any previous "server full" type messages
    clc.serverMessage[0] = 0;

    noGameRestart = qtrue;
    CL_Disconnect( qtrue );
    Con_Close(); // TODO

    // port will be provided
    if (clc.serverAddress.port == 0) {
        clc.serverAddress.port = BigShort( PORT_SERVER );
    }

    clc.state = CA_CONNECTING;

    // Set a client challenge number that ideally is mirrored back by the server.
    clc.challenge = (((unsigned int)rand() << 16) ^ (unsigned int)rand()) ^ Com_Milliseconds();

    //Key_SetCatcher( 0 );
    clc.connectTime        = -99999;   // CL_CheckForResend() will fire immediately
    clc.connectPacketCount      = 0;

	// server connection string
	// Cvar_Set( "cl_currentServerAddress", server ); // TODO added not tested

    //Com_Printf("\n\n>> CALLING connect from %s <<<<<<<<<<<\n\n", __func__);

    if ( ! WebSocketConnect( CL_Connected_f ) ) {
        Com_Error(ERR_FATAL, "failed to init ws");
    }
}

/*
==================
CL_CompletePlayerName
==================
*/
static void CL_CompletePlayerName( char *args, int argNum )
{
    if( argNum == 2 )
    {
        char        names[MAX_CLIENTS][MAX_NAME_LENGTH];
        const char  *namesPtr[MAX_CLIENTS];
        int         i;
        int         clientCount;
        int         nameCount;
        const char *info;
        const char *name;

        //configstring
        info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO];
        clientCount = atoi( Info_ValueForKey( info, "sv_maxclients" ) );

        nameCount = 0;

        for( i = 0; i < clientCount; i++ ) {
            if( i == clc.clientNum )
                continue;

            info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_PLAYERS+i];

            name = Info_ValueForKey( info, "n" );
            if( name[0] == '\0' )
                continue;
            Q_strncpyz( names[nameCount], name, sizeof(names[nameCount]) );
            Q_CleanStr( names[nameCount] );

            namesPtr[nameCount] = names[nameCount];
            nameCount++;
        }
        qsort( (void*)namesPtr, nameCount, sizeof( namesPtr[0] ), Com_strCompare );

        Field_CompletePlayerName( namesPtr, nameCount );
    }
}

/*
=================
CL_SendPureChecksums
=================
*/
void CL_SendPureChecksums( void ) { // TODO UPDATE me
    char cMsg[MAX_INFO_VALUE];

    // if we are pure we need to send back a command with our referenced pk3 checksums
    Com_sprintf(cMsg, sizeof(cMsg), "cp %d %s", cl.serverId, "TODO" );

    CL_AddReliableCommand(cMsg, qfalse);
}

/*
=================
CL_ResetPureClientAtServer
=================
*/
void CL_ResetPureClientAtServer( void ) {
    CL_AddReliableCommand("vdr", qfalse); // TODO what is this?
}

/*
=================
CL_Vid_Restart_f

Restart the video subsystem

we also have to reload the UI and CGame because the renderer
doesn't know what graphics to reload
=================
*/
void CL_Vid_Restart_f( void ) {

    Com_Printf("\n%s disabled\n", __func__);
    return;

    // don't let them loop during the restart
    S_StopAllSounds();

    // clear the whole hunk
    Hunk_Clear();

    // shutdown the UI
    //CL_ShutdownUI(); // TODO

    // shutdown the CGame
    CL_ShutdownCGame();

    // shutdown the renderer and clear the renderer interface
    CL_ShutdownRef();

    // reinitialize the filesystem if the game directory or checksum has changed
    cls.rendererStarted = qfalse;
    cls.uiStarted       = qfalse;
    cls.cgameStarted    = qfalse;
    cls.soundRegistered = qfalse;

    // initialize the renderer interface
    CL_InitRef();

    void CL_InitRenderer( void );
    // startup all the client stuff
    CL_InitRenderer(); // TODO this is the change to get reconnect to work???

    // start the cgame if connected
    if(clc.state > CA_CONNECTED && clc.state != CA_CINEMATIC)
    {
        cls.cgameStarted = qtrue;
        CL_InitCGame();
        // send pure checksums
        //CL_SendPureChecksums();
    }
}

/*
=================
CL_Snd_Restart
Restart the sound subsystem
=================
*/
void CL_Snd_Shutdown(void)
{
    S_Shutdown();
    cls.soundStarted = qfalse;
}

/*
=================
CL_Snd_Restart_f

Restart the sound subsystem
The cgame and game must also be forced to restart because
handles will be invalid
=================
*/
void CL_Snd_Restart_f(void)
{
assert(0);
    CL_Snd_Shutdown();
    // sound will be reinitialized by vid_restart
    CL_Vid_Restart_f();
}

/*
==================
CL_Configstrings_f
==================
*/
void CL_Configstrings_f( void ) {
    int     i;
    int     ofs;

    if ( clc.state != CA_ACTIVE ) {
        Com_Printf( "Not connected to a server.\n");
        return;
    }

    for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
        ofs = cl.gameState.stringOffsets[ i ];
        if ( !ofs ) {
            continue;
        }
        Com_Printf( "%4i: %s\n", i, cl.gameState.stringData + ofs );
    }
}

/*
==============
CL_Clientinfo_f
==============
*/
void CL_Clientinfo_f( void ) {
    Com_Printf( "--------- Client Information ---------\n" );
    Com_Printf( "state: %i\n", clc.state );
    Com_Printf( "Server: %s\n", clc.servername );
    Com_Printf ("User info settings:\n");
    Info_Print( Cvar_InfoString( CVAR_USERINFO ) );
    Com_Printf( "--------------------------------------\n" );
}

/*
=================
CL_DownloadsComplete

Called when all downloading has been completed
=================
*/
qboolean CL_CgameStarted(void) {
    return clc.state == CA_LOADING && cls.cgameStarted;
}

qboolean CL_CgameRunning(void) {
    return clc.state == CA_ACTIVE;
}

static void CL_DownloadsComplete( void ) { // TODO

    Com_Printf("%s clc.downloadRestart %d\n", __func__, clc.downloadRestart);

    // if we downloaded files we need to restart the file system
    if (clc.downloadRestart) {
        clc.downloadRestart = qfalse;

        FS_Restart(clc.checksumFeed); // We possibly downloaded a pak, restart the file system to load it

        // inform the server so we get new gamestate info
        CL_AddReliableCommand("donedl", qfalse);

        // by sending the donedl command we request a new gamestate
        // so we don't want to load stuff yet
        return;
    }

    // let the client game init and load data
    clc.state = CA_LOADING;

    // Pump the loop, this may change gamestate!
    Com_EventLoop();

    // if the gamestate was changed by calling Com_EventLoop
    // then we loaded everything already and we don't want to do it again.
    if ( clc.state != CA_LOADING ) {
        assert(0);
        return;
    }

    // starting to load a map so we get out of full screen ui mode
    Cvar_Set("r_uiFullScreen", "0");

    // flush client memory and start loading stuff
    // this will also (re)load the UI
    // if this is a local client then only the client part of the hunk
    // will be cleared, note that this is done after the hunk mark has been set

    Com_Printf("Skipping CL_FlushMemory\n");
    // TODO skip CL_FlushMemory as it just re-inits, something we just finished
    //CL_FlushMemory();

    // initialize the CGame
    cls.cgameStarted = qtrue;
    CL_InitCGame(); // called from Com_Frame when clc.state == CA_LOADING && cls.cgameStarted;

    // set pure checksums
    CL_SendPureChecksums();

    CL_WritePacket();
    CL_WritePacket();
    CL_WritePacket();
}

/*
=================
CL_BeginDownload

Requests a file to download from the server.  Stores it in the current
game directory.
=================
*/
void CL_BeginDownload( const char *localName, const char *remoteName ) {

    Com_Error (ERR_FATAL, "%s got called ...", __func__);

    Com_Printf("***** CL_BeginDownload *****\n"
                "Localname: %s\n"
                "Remotename: %s\n"
                "****************************\n", localName, remoteName);

    Q_strncpyz ( clc.downloadName, localName, sizeof(clc.downloadName) );
    Com_sprintf( clc.downloadTempName, sizeof(clc.downloadTempName), "%s.tmp", localName );

    // Set so UI gets access to it
    Cvar_Set( "cl_downloadName", remoteName );
    Cvar_Set( "cl_downloadSize", "0" );
    Cvar_Set( "cl_downloadCount", "0" );
    Cvar_SetValue( "cl_downloadTime", cls.realtime );

    clc.downloadBlock = 0; // Starting new file
    clc.downloadCount = 0;

    CL_AddReliableCommand(va("download %s", remoteName), qfalse);
}

/*
=================
CL_NextDownload

A download completed or failed
=================
*/
void CL_NextDownload(void)
{
    char *s;
    char *remoteName, *localName;

    // A download has finished, check whether this matches a referenced checksum
    if(*clc.downloadName)
    {
        char *zippath = "/TODO"; // TODO FS_BuildOSPath(Cvar_VariableString("fs_homepath"), clc.downloadName, "");
        zippath[strlen(zippath)-1] = '\0';

        if ( qfalse ) { // TODO !FS_CompareZipChecksum(zippath))
            Com_Error(ERR_DROP, "Incorrect checksum for file: %s", clc.downloadName);
        }
    }

    *clc.downloadTempName = *clc.downloadName = 0;
    Cvar_Set("cl_downloadName", "");

    // We are looking to start a download here
    if (*clc.downloadList) {
        s = clc.downloadList;

        // format is:
        //  @remotename@localname@remotename@localname, etc.

        if (*s == '@') {
            s++;
        }

        remoteName = s;

        if ( (s = strchr(s, '@')) == NULL ) {
            CL_DownloadsComplete();
            return;
        }

        *s++ = 0;
        localName = s;
        if ( (s = strchr(s, '@')) != NULL ) {
            *s++ = 0;
        } else {
            s = localName + strlen(localName); // point at the nul byte
        }

        if((cl_allowDownload->integer & DLF_NO_UDP)) {
            Com_Error(ERR_FATAL, "UDP Downloads are "
                "disabled on your client. "
                "(cl_allowDownload is %d)",
                cl_allowDownload->integer);
            return;
        }
        else {
            CL_BeginDownload( localName, remoteName );
        }

        clc.downloadRestart = qtrue;

        // move over the rest
        memmove( clc.downloadList, s, strlen(s) + 1);

        return;
    }

    CL_DownloadsComplete();
}

/*
=================
CL_InitDownloads

After receiving a valid game state, we valid the cgame and local zip files here
and determine if we need to download them
TODO I need to replace the functionality to check IDBStore for all files and correct version???
=================
*/
void CL_InitDownloads(void) {
    // char missingfiles[1024];

    Com_Printf("%s\n", __func__);

    Com_Printf("Need paks: %s\n", clc.downloadList );

    if ( qtrue ) { // *clc.downloadList ) {
        // if autodownloading is not enabled on the server
        clc.state = CA_CONNECTED;

        *clc.downloadTempName = *clc.downloadName = 0;
        Cvar_Set( "cl_downloadName", "" );

        //CL_NextDownload();
        //return;
    }

    //assert(0);
    // TODO is this wrong???  this will never be called......BUGBUG XXX FIXME cb
    // TODO is this wrong???  this will never be called......BUGBUG XXX FIXME cb
    // TODO is this wrong???  this will never be called......BUGBUG XXX FIXME cb

    // clc.downloadRestart = qtrue; == true if map name has changed

    CL_DownloadsComplete();

    // TODO is this wrong???  this will never be called......BUGBUG XXX FIXME cb
    // TODO is this wrong???  this will never be called......BUGBUG XXX FIXME cb
    // TODO is this wrong???  this will never be called......BUGBUG XXX FIXME cb
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CL_CheckForResend( void ) {
    int     port;
    char    info[MAX_INFO_STRING];
    char    data[MAX_INFO_STRING + 10];

    // don't send anything if playing back a demo
    if ( clc.demoplaying ) { // TODO remove me
        assert(0);
        return;
    }

    // resend if we haven't gotten a reply yet
    if ( clc.state != CA_CONNECTING && clc.state != CA_CHALLENGING ) {
        return;
    }

    if ( cls.realtime - clc.connectTime < RETRANSMIT_TIMEOUT ) {
        return;
    }

    clc.connectTime = cls.realtime; // for retransmit requests
    clc.connectPacketCount++;

    switch ( clc.state ) {
    case CA_CONNECTING:
        // requesting a challenge .. IPv6 users always get in as authorize server supports no ipv6.

        // The challenge request shall be followed by a client challenge so no malicious server can hijack this connection.
        // Add the gamename so the server knows we're running the correct game or can reject the client
        // with a meaningful message
        Com_sprintf(data, sizeof(data), "getchallenge %d %s", clc.challenge, com_gamename->string);

        NET_OutOfBandPrint(NS_CLIENT, clc.serverAddress, "%s", data);
        break;

    case CA_CHALLENGING:
        // sending back the challenge
        port = Cvar_VariableValue ("net_qport");

        Q_strncpyz( info, Cvar_InfoString( CVAR_USERINFO ), sizeof( info ) );

        Info_SetValueForKey( info, "protocol",  va("%i", com_protocol->integer));
        Info_SetValueForKey( info, "qport",     va("%i", port ) );
        Info_SetValueForKey( info, "challenge", va("%i", clc.challenge ) );

        Com_sprintf( data, sizeof(data), "connect \"%s\"", info );

        NET_OutOfBandData( NS_CLIENT, clc.serverAddress, (byte *) data, strlen ( data ) );
        // the most current userinfo has been sent, so watch for any
        // newer changes to userinfo variables
        cvar_modifiedFlags &= ~CVAR_USERINFO;
        break;

    default:
        Com_Error( ERR_FATAL, "CL_CheckForResend: bad clc.state" );
    }
}

/*
===================
CL_InitServerInfo
===================
*/
void CL_InitServerInfo( serverInfo_t *server, netadr_t *address ) {
    server->adr            = *address;
    server->clients        = 0;
    server->hostName[0]    = '\0';
    server->mapName[0]     = '\0';
    server->maxClients     = 0;
    server->maxPing        = 0;
    server->minPing        = 0;
    server->ping           = -1;
    server->game[0]        = '\0';
    server->gameType       = 0;
    server->netType        = 0;
    server->punkbuster     = 0;
    server->g_humanplayers = 0;
    server->g_needpass     = 0;
}

#define MAX_SERVERSPERPACKET    256

/*
===================
CL_ServersResponsePacket
===================
*/
static void CL_ServersResponsePacket( const netadr_t* from, msg_t *msg, qboolean extended ) {
    netadr_t addresses[MAX_SERVERSPERPACKET];
    int   i, j, count, total;
    int   numservers;
    byte* buffptr;
    byte* buffend;

    // Com_Printf("CL_ServersResponsePacket from %s\n", NET_AdrToStringwPort(*from));

    if (cls.numglobalservers == -1) {
        // state to detect lack of servers or lack of response
        cls.numglobalservers = 0;
        cls.numGlobalServerAddresses = 0;
    }

    // parse through server response string
    numservers = 0;
    buffptr    = msg->data;
    buffend    = buffptr + msg->cursize;

    // advance to initial token
    do
    {
        if(*buffptr == '\\' || (extended && *buffptr == '/'))
            break;

        buffptr++;
    } while (buffptr < buffend);

    while (buffptr + 1 < buffend)
    {
        // IPv4 address
        if (*buffptr == '\\')
        {
            buffptr++;

            if (buffend - buffptr < sizeof(addresses[numservers].ip) + sizeof(addresses[numservers].port) + 1)
                break;

            for(i = 0; i < sizeof(addresses[numservers].ip); i++)
                addresses[numservers].ip[i] = *buffptr++;

            addresses[numservers].type = NA_IP;
        }
        // IPv6 address, if it's an extended response
        else if (extended && *buffptr == '/')
        {
            buffptr++;

            if (buffend - buffptr < sizeof(addresses[numservers].ip6) + sizeof(addresses[numservers].port) + 1)
                break;

            for(i = 0; i < sizeof(addresses[numservers].ip6); i++)
                addresses[numservers].ip6[i] = *buffptr++;

            addresses[numservers].type = NA_IP6;
            addresses[numservers].scope_id = from->scope_id;
        }
        else
            // syntax error!
            break;

        // parse out port
        addresses[numservers].port = (*buffptr++) << 8;
        addresses[numservers].port += *buffptr++;
        addresses[numservers].port = BigShort( addresses[numservers].port );

        // syntax check
        if (*buffptr != '\\' && *buffptr != '/')
            break;

        numservers++;
        if (numservers >= MAX_SERVERSPERPACKET)
            break;
    }

    count = cls.numglobalservers;

    for (i = 0; i < numservers && count < MAX_GLOBAL_SERVERS; i++) {
        // build net address
        serverInfo_t *server = &cls.globalServers[count];

        // Tequila: It's possible to have sent many master server requests. Then
        // we may receive many times the same addresses from the master server.
        // We just avoid to add a server if it is still in the global servers list.
        for (j = 0; j < count; j++)
        {
            //if (NET_CompareAdr(cls.globalServers[j].adr, addresses[i])) // TODO
                break;
        }

        if (j < count)
            continue;

        CL_InitServerInfo( server, &addresses[i] );
        // advance to next slot
        count++;
    }

    // if getting the global list
    if ( count >= MAX_GLOBAL_SERVERS && cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS )
    {
        // if we couldn't store the servers in the main list anymore
        for (; i < numservers && cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS; i++)
        {
            // just store the addresses in an additional list
            cls.globalServerAddresses[cls.numGlobalServerAddresses++] = addresses[i];
        }
    }

    cls.numglobalservers = count;
    total = count + cls.numGlobalServerAddresses;

    Com_Printf("%d servers parsed (total %d)\n", numservers, total);
}

/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
static void CL_ConnectionlessPacket( netadr_t from, msg_t *msg ) {
    char    *s;
    char    *c;
    int challenge = 0;

    MSG_BeginReadingOOB( msg );
    MSG_ReadLong( msg );    // skip the -1

    s = MSG_ReadStringLine( msg );

    Cmd_TokenizeString( s );

    c = Cmd_Argv(0);

    //Com_Printf ("CL packet %s\n", c);

    // challenge from the server we are connecting to
    if (!Q_stricmp(c, "challengeResponse"))
    {
        char *strver;
        int ver;

        if (clc.state != CA_CONNECTING)
        {
            Com_Printf("Unwanted challenge response received. Ignored.\n");
            return;
        }

        c = Cmd_Argv(2);
        if(*c)
            challenge = atoi(c);

        strver = Cmd_Argv(3);
        if(*strver)
        {
            ver = atoi(strver);

            if(ver != com_protocol->integer)
            {
                {
                    Com_Printf(S_COLOR_YELLOW "Warning: Server reports protocol version %d, we have %d. " "Trying anyways.\n", ver, com_protocol->integer);
                }
            }
        }

        if(!*c || challenge != clc.challenge)
        {
            Com_Printf("Bad challenge for challengeResponse. Ignored.\n");
            return;
        }

        // start sending challenge response instead of challenge request packets
        clc.challenge = atoi(Cmd_Argv(1));
        clc.state = CA_CHALLENGING;
        clc.connectPacketCount = 0;
        clc.connectTime = -99999;

        // take this address as the new server address.  This allows
        // a server proxy to hand off connections to multiple servers
        clc.serverAddress = from;
        //Com_Printf ("challengeResponse: %d\n", clc.challenge);
        return;
    }

    // server connection
    if ( !Q_stricmp(c, "connectResponse") ) {
        if ( clc.state >= CA_CONNECTED ) {
            Com_Printf ("Dup connect received. Ignored.\n");
            return;
        }
        if ( clc.state != CA_CHALLENGING ) {
            Com_Printf ("connectResponse packet while not connecting. Ignored.\n");
            return;
        }

        // TODO
        /*if ( !NET_CompareAdr( from, clc.serverAddress ) ) {
            Com_Printf( "connectResponse from wrong address. Ignored.\n" );
            return;
        }*/

        c = Cmd_Argv(1);

        if(*c)
            challenge = atoi(c);
        else
        {
            Com_Printf("Bad connectResponse received. Ignored.\n");
            return;
        }

        if(challenge != clc.challenge)
        {
            Com_Printf("ConnectResponse with bad challenge received. Ignored.\n");
            return;
        }

        Netchan_Setup(NS_CLIENT, &clc.netchan, from, Cvar_VariableValue("net_qport"), clc.challenge, qfalse);

        clc.state = CA_CONNECTED;
        clc.lastPacketSentTime = -9999;     // send first packet immediately
        return;
    }

    // server responding to an info broadcast
    if ( !Q_stricmp(c, "infoResponse") ) {
        CL_ServerInfoPacket( from, msg );
        return;
    }

    // server responding to a get playerlist
    if ( !Q_stricmp(c, "statusResponse") ) {
        CL_ServerStatusResponse( from, msg );
        return;
    }

    // echo request from server
    if ( !Q_stricmp(c, "echo") ) {
        // NOTE: we may have to add exceptions for auth and update servers
        //if ( NET_CompareAdr( from, clc.serverAddress ) || NET_CompareAdr( from, cls.rconAddress ) ) { // TODO
            NET_OutOfBandPrint( NS_CLIENT, from, "%s", Cmd_Argv(1) );
        //}
        return;
    }

    // cd check
    if ( !Q_stricmp(c, "keyAuthorize") ) {
        // we don't use these now, so dump them on the floor
        return;
    }

    // echo request from server
    if ( !Q_stricmp(c, "print") ) {
        // NOTE: we may have to add exceptions for auth and update servers
        //if ( NET_CompareAdr( from, clc.serverAddress ) || NET_CompareAdr( from, cls.rconAddress ) ) { // TODO
            s = MSG_ReadString( msg );

            Q_strncpyz( clc.serverMessage, s, sizeof( clc.serverMessage ) );
            Com_Printf( "%s", s );
        //}
        return;
    }

    // list of servers sent back by a master server (classic)
    if ( !Q_strncmp(c, "getserversResponse", 18) ) {
        CL_ServersResponsePacket( &from, msg, qfalse );
        return;
    }

    // list of servers sent back by a master server (extended)
    if ( !Q_strncmp(c, "getserversExtResponse", 21) ) {
        CL_ServersResponsePacket( &from, msg, qtrue );
        return;
    }

    Com_DPrintf ("Unknown connectionless packet command.\n");
}


/*
=================
CL_PacketEvent

A packet has arrived from the main event loop
=================
*/
void CL_PacketEvent( netadr_t from, msg_t *msg ) {
    int     headerBytes;

    clc.lastPacketTime = cls.realtime;

    if ( msg->cursize >= 4 && *(int *)msg->data == -1 ) { // TODO possible alignment fault   this check is to find 0xFF, 0xFF, 0xFF, 0xFF at beginning of data...
        CL_ConnectionlessPacket( from, msg );
        return;
    }

    if ( clc.state < CA_CONNECTED ) {
        return;     // can't be a valid sequenced packet
    }

    if ( msg->cursize < 4 ) {
        Com_Printf ("Runt packet\n");//TODO
        return;
    }

    //
    // packet from server
    // TODO
    /*if ( !NET_CompareAdr( from, clc.netchan.remoteAddress ) ) {
        Com_DPrintf ("%s:sequenced packet without connection\n" , NET_AdrToStringwPort( from ) );
        // FIXME: send a client disconnect?
        return;
    }*/

    if (!CL_Netchan_Process( &clc.netchan, msg) ) {
        return;     // out of order, duplicated, etc
    }

    // the header is different lengths for reliable and unreliable messages
    headerBytes = msg->readcount;

    // track the last message received so it can be returned in
    // client messages, allowing the server to detect a dropped
    // gamestate
    clc.serverMessageSequence = LittleLong( *(int *)msg->data ); // TODO fixme possible alignment fault

    clc.lastPacketTime = cls.realtime;

    CL_ParseServerMessage( msg );

    // we don't know if it is ok to save a demo message until
    // after we have parsed the frame
    if ( headerBytes ) { // TODO
    //if ( clc.demorecording && !clc.demowaiting ) {
        // TODO CL_WriteDemoMessage( msg, headerBytes );
    }
}

/*
==================
CL_CheckTimeout

==================
*/
void CL_CheckTimeout( void ) {
    cl.timeoutcount = 0;
    return;

    if (
            ! CL_CheckPaused()        &&
            clc.state >= CA_CONNECTED &&
            clc.state != CA_CINEMATIC &&
            cls.realtime - clc.lastPacketTime > cl_timeout->value*1000
    ) {
        if (++cl.timeoutcount > 5) {    // timeoutcount saves debugger
            Com_Printf ("\nServer connection timed out.\n");
            Com_Error(ERR_FATAL,"%s timeout", __func__);
            CL_Disconnect( qtrue );
            return;
        }
    } else {
        cl.timeoutcount = 0;
    }
}

/*
==================
CL_CheckPaused
Check whether client has been paused.
==================
*/
qboolean CL_CheckPaused(void) // TODO
{
    cvar_t *cl_paused = Cvar_Get("cl_paused", "0", 0);

    // if cl_paused->modified is set, the cvar has only been changed in
    // this frame. Keep paused in this frame to ensure the server doesn't
    // lag behind.

    //if(cl_paused->integer || cl_paused->modified)
    //   return qtrue;

    return cl_paused->integer == 1;
}

/*
==================
CL_CheckUserinfo
==================
*/
void CL_CheckUserinfo( void ) {
    // don't add reliable commands when not yet connected
    if(clc.state < CA_CONNECTED)
        return;

    // don't overflow the reliable command buffer when paused
    if(CL_CheckPaused())
        return;

    // send a reliable userinfo update if needed
    if(cvar_modifiedFlags & CVAR_USERINFO)
    {
        cvar_modifiedFlags &= ~CVAR_USERINFO;
        CL_AddReliableCommand(va("userinfo \"%s\"", Cvar_InfoString( CVAR_USERINFO ) ), qfalse);
    }
}

const char* ConnStateToString( const connstate_t connstate ) {
    switch ( connstate ) {
        case CA_UNINITIALIZED: return "CA_UNINITIALIZED";
        case CA_DISCONNECTED:  return "CA_DISCONNECTED"; // not talking to a server
        case CA_AUTHORIZING:   return "CA_AUTHORIZING";  // not used any more, was checking cd key
        case CA_CONNECTING:    return "CA_CONNECTING";   // sending request packets to the server
        case CA_CHALLENGING:   return "CA_CHALLENGING";  // sending challenge packets to the server
        case CA_CONNECTED:     return "CA_CONNECTED";    // netchan_t established, getting gamestate
        case CA_LOADING:       return "CA_LOADING";      // only during cgame initialization, never during main loop
        case CA_PRIMED:        return "CA_PRIMED";       // got gamestate, waiting for first frame
        case CA_ACTIVE:        return "CA_ACTIVE";       // game views should be displayed
        case CA_CINEMATIC:     return "CA_CINEMATIC";    // playing a cinematic or a static pic; not connected to a server
    }

    return "UNKNOWN";
}

void CL_DumpClientState() {
// clientActive_t      cl;
// clientConnection_t  clc;
// clientStatic_t      cls;

    /*
    show(cl);
    showi(cl);
    showf(cl);

    show(clc);
    showi(clc);
    showf(clc);

    show(cls);
    showi(cls);
    showf(cls);
    */

    Com_Printf("\nDumping Client state:\n-------------------\n");
    Com_Printf("Client Active:\n-------------------------\n");
    show (cl.mapname);
    showf(cl.cgameSensitivity);
    showi(cl.cgameUserCmdValue);
    showi(cl.cmdNumber);
    showi(cl.extrapolatedSnapshot);
    showi(cl.newSnapshots);
    showi(cl.serverTimeDelta);
    showi(cl.mouseDx[0]);
    showi(cl.mouseDx[1]);
    showi(cl.mouseDy[0]);
    showi(cl.mouseDy[1]);
    showi(cl.snap.snapFlags);
    showf(cl.viewangles[0]);
    showf(cl.viewangles[1]);
    showf(cl.viewangles[2]);

    Com_Printf("Client Connection:\n-------------------\n");
    show (clc.servername);
    Com_Printf("%s\n", ConnStateToString(clc.state));
    showi(clc.challenge);
    showi(clc.connectTime);
    showi(clc.connectPacketCount);

    Com_Printf("Client Static:\n------------------------\n");
    showi(cls.cgameStarted);
    showi(cls.framecount);
    showi(cls.frametime);
    showi(cls.realtime);
    showi(cls.rendererStarted);
    showi(cls.soundStarted);
    showi(cls.uiStarted);
}

/*
==================
CL_Frame
==================
*/
void CL_Frame ( int msec ) {

    if ( !com_cl_running->integer ) {
        Com_Error(ERR_FATAL, "client not running");
        return;
    }

    if ( clc.state == CA_DISCONNECTED ) {
        S_StopAllSounds();
        ////VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_MAIN ); // TODO putting vmcalls back
        // TODO show main menu
    }

    // save the msec before checking pause
    cls.realFrametime = msec;

    // decide the simulation time
    cls.frametime = msec;

    cls.realtime += cls.frametime;

    if ( cl_timegraph->integer ) {
        SCR_DebugGraph ( cls.realFrametime * 0.25 );
    }
    // see if we need to update any userinfo
    CL_CheckUserinfo();

    // if we haven't gotten a packet in a long time,
    // drop the connection
    CL_CheckTimeout();

    // send intentions now
    CL_SendCmd();

    // resend a connection request if necessary
    CL_CheckForResend();

    // decide on the serverTime to render
    CL_SetCGameTime();

    // update the screen
    SCR_UpdateScreen();

    // update audio
    S_Update();

    // advance local effects for next frame
    //SCR_RunCinematic(); // TODO

    void Con_RunConsole(void);
    Con_RunConsole(); // TODO

    cls.framecount++;
}

/*
============
CL_ShutdownRef
============
*/
void CL_ShutdownRef( void ) {
   RE_Shutdown( qtrue );
}

/*
============
CL_InitRenderer
============
*/
/*static*/ void CL_InitRenderer( void ) {
ONCE;
    // this sets up the renderer and calls R_Init
    RE_BeginRegistration( &cls.glconfig );
}

void CL_InitRenderer2( void ) {
ONCE;
    // this sets up the renderer and calls R_Init
    void RE_ContinueRegistration( glconfig_t *glconfigOut );
    RE_ContinueRegistration( &cls.glconfig );

    // TODO is this the fix to get into game?
    cls.rendererStarted = qtrue;
}

void CL_InitRenderer3(void) {
ONCE;
    void RE_FinishRegistration ( glconfig_t *glconfigOut );
    RE_FinishRegistration( &cls.glconfig);

    cls.charSetShader = RE_RegisterShader( "gfx/2d/bigchars" );
    cls.whiteShader   = RE_RegisterShader( "white" );
    cls.consoleShader = RE_RegisterShader( "console" );
}

void CL_InitSound(void) {
ONCE;
    if ( ! cls.soundStarted ) {
        cls.soundStarted = qtrue;
        S_Init();
    }
}
/*
============================
CL_StartHunkUsers

After the server has cleared the hunk, these will need to be restarted
This is the only place that any of these functions are called from
============================
*/
void CL_StartHunkUsers( qboolean rendererOnly ) {
    if ( ! com_cl_running ) {
        Com_Printf("client not running\n");
        return;
    }

    if ( ! com_cl_running->integer ) {
        Com_Printf("client not running\n");
        return;
    }

    if ( ! cls.rendererStarted ) {
LINE;
        CL_InitRenderer();
        cls.rendererStarted = qtrue;
    }
LINE;
    if ( rendererOnly ) {
LINE;
        return;
    }

    if ( ! cls.soundStarted ) {
        cls.soundStarted = qtrue;
        S_Init();
    }

    if ( ! cls.soundRegistered ) {
        cls.soundRegistered = qtrue;
        S_BeginRegistration();
    }

    if ( ! cls.uiStarted ) {
        cls.uiStarted = qtrue;
        //CL_InitUI(); // TODO
    }
}

/*
============
CL_RefMalloc
============
*/
void *CL_RefMalloc( int size ) {
    return Z_TagMalloc( size, TAG_RENDERER );
}

int CL_ScaledMilliseconds(void) {
    return Sys_Milliseconds()*com_timescale->value;
}

/*
============
CL_InitRef
============
*/
void CL_InitRef( void ) {
    Com_Printf( "----- Initializing Renderer ----\n" );

    // TODO memset glConfig, etc here??
    // unpause so the cgame definitely gets a snapshot and renders a frame
    //Cvar_Set( "cl_paused", "0" );
}

void CL_SetModel_f( void ) {
    char    *arg;
    char    name[256];

    arg = Cmd_Argv( 1 );
    if (arg[0]) {
        Cvar_Set( "model", arg );
        Cvar_Set( "headmodel", arg );
    } else {
        Cvar_VariableStringBuffer( "model", name, sizeof(name) );
        Com_Printf("model is set to %s\n", name);
    }
}

void CL_Sayto_f( void ) {
    char        *rawname;
    char        name[MAX_NAME_LENGTH];
    char        cleanName[MAX_NAME_LENGTH];
    const char  *info;
    int         count;
    int         i;
    int         clientNum;
    char        *p;

    if ( Cmd_Argc() < 3 ) {
        Com_Printf ("sayto <player name> <text>\n");
        return;
    }

    rawname = Cmd_Argv(1);

    Com_FieldStringToPlayerName( name, MAX_NAME_LENGTH, rawname );

    info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO];
    count = atoi( Info_ValueForKey( info, "sv_maxclients" ) );

    clientNum = -1;
    for( i = 0; i < count; i++ ) {

        info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_PLAYERS+i];
        Q_strncpyz( cleanName, Info_ValueForKey( info, "n" ), sizeof(cleanName) );
        Q_CleanStr( cleanName );

        if ( !Q_stricmp( cleanName, name ) ) {
            clientNum = i;
            break;
        }
    }
    if( clientNum <= -1 )
    {
        Com_Printf ("No such player name: %s.\n", name);
        return;
    }

    p = Cmd_ArgsFrom(2);

    if ( *p == '"' ) {
        p++;
        p[strlen(p)-1] = 0;
    }

    CL_AddReliableCommand(va("tell %i \"%s\"", clientNum, p ), qfalse);
}

/*
====================
CL_Init
====================
*/

void CL_Init( void ) {
ONCE;
    Com_Printf( "----- Client Initialization -----\n" );

    if ( com_fullyInitialized ) {
        Com_Error(ERR_FATAL,"CL_Init(com_fullyInitialized true");
    }

    CL_ClearState();
    Com_Memset( &clc, 0, sizeof( clc ) );
    Com_Memset( &cls, 0, sizeof( cls ) );

    cl_connectedToPureServer = qtrue;
    clc.state    = CA_DISCONNECTED;
    cls.realtime = 0;

    Con_Init();

    CL_InitInput ();

    //
    // register our variables
    //
    cl_noprint       = Cvar_Get( "cl_noprint", "0", 0 );
    cl_timeout       = Cvar_Get( "cl_timeout", "200", 0);
    cl_timeNudge     = Cvar_Get( "cl_timeNudge", "0", CVAR_TEMP );
    cl_shownet       = Cvar_Get( "cl_shownet", "0", CVAR_TEMP );
    cl_showSend      = Cvar_Get( "cl_showSend", "0", CVAR_TEMP );
    cl_showTimeDelta = Cvar_Get( "cl_showTimeDelta", "0", CVAR_TEMP );
    cl_activeAction  = Cvar_Get( "activeAction", "", CVAR_TEMP );

    cl_aviFrameRate  = Cvar_Get( "cl_aviFrameRate", "25", CVAR_ARCHIVE);
    cl_aviMotionJpeg = Cvar_Get( "cl_aviMotionJpeg", "1", CVAR_ARCHIVE);

    cl_yawspeed      = Cvar_Get( "cl_yawspeed", "140", CVAR_ARCHIVE);
    cl_pitchspeed    = Cvar_Get( "cl_pitchspeed", "140", CVAR_ARCHIVE);
    cl_anglespeedkey = Cvar_Get( "cl_anglespeedkey", "1.5", 0);
    cl_maxpackets    = Cvar_Get( "cl_maxpackets", "30", CVAR_ARCHIVE );
    cl_packetdup     = Cvar_Get( "cl_packetdup", "1", CVAR_ARCHIVE );

    cl_run           = Cvar_Get( "cl_run", "1", CVAR_ARCHIVE);
    cl_sensitivity   = Cvar_Get( "sensitivity", "5", CVAR_ARCHIVE);
    cl_mouseAccel    = Cvar_Get( "cl_mouseAccel", "0", CVAR_ARCHIVE);
    cl_freelook      = Cvar_Get( "cl_freelook", "1", CVAR_ARCHIVE );

    // 0: legacy mouse acceleration
    // 1: new implementation
    cl_mouseAccelStyle = Cvar_Get( "cl_mouseAccelStyle", "0", CVAR_ARCHIVE );
    // offset for the power function (for style 1, ignored otherwise)
    // this should be set to the max rate value
    cl_mouseAccelOffset = Cvar_Get( "cl_mouseAccelOffset", "5", CVAR_ARCHIVE );
    Cvar_CheckRange(cl_mouseAccelOffset, 0.001f, 50000.0f, qfalse);

    cl_showMouseRate = Cvar_Get ("cl_showmouserate", "0", 0);

    cl_allowDownload = Cvar_Get ("cl_allowDownload", "0", CVAR_ARCHIVE);

    cl_conXOffset = Cvar_Get ("cl_conXOffset", "0", 0);

    cl_serverStatusResendTime = Cvar_Get ("cl_serverStatusResendTime", "750", 0);

    // init autoswitch so the ui will have it correctly even
    // if the cgame hasn't been started
    Cvar_Get ("cg_autoswitch", "1", CVAR_ARCHIVE);

    m_pitch   = Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE);
    m_yaw     = Cvar_Get ("m_yaw", "0.022", CVAR_ARCHIVE);
    m_forward = Cvar_Get ("m_forward", "0.25", CVAR_ARCHIVE);
    m_side    = Cvar_Get ("m_side", "0.25", CVAR_ARCHIVE);
#if 1
    // TODO
    m_filter = Cvar_Get ("m_filter", "1", CVAR_ARCHIVE);
#else
    m_filter = Cvar_Get ("m_filter", "0", CVAR_ARCHIVE);
#endif

    j_pitch =        Cvar_Get ("j_pitch",        "0.022", CVAR_ARCHIVE);
    j_yaw =          Cvar_Get ("j_yaw",          "-0.022", CVAR_ARCHIVE);
    j_forward =      Cvar_Get ("j_forward",      "-0.25", CVAR_ARCHIVE);
    j_side =         Cvar_Get ("j_side",         "0.25", CVAR_ARCHIVE);
    j_up =           Cvar_Get ("j_up",           "0", CVAR_ARCHIVE);

    j_pitch_axis =   Cvar_Get ("j_pitch_axis",   "3", CVAR_ARCHIVE);
    j_yaw_axis =     Cvar_Get ("j_yaw_axis",     "2", CVAR_ARCHIVE);
    j_forward_axis = Cvar_Get ("j_forward_axis", "1", CVAR_ARCHIVE);
    j_side_axis =    Cvar_Get ("j_side_axis",    "0", CVAR_ARCHIVE);
    j_up_axis =      Cvar_Get ("j_up_axis",      "4", CVAR_ARCHIVE);

    Cvar_CheckRange(j_pitch_axis, 0, MAX_JOYSTICK_AXIS-1, qtrue);
    Cvar_CheckRange(j_yaw_axis, 0, MAX_JOYSTICK_AXIS-1, qtrue);
    Cvar_CheckRange(j_forward_axis, 0, MAX_JOYSTICK_AXIS-1, qtrue);
    Cvar_CheckRange(j_side_axis, 0, MAX_JOYSTICK_AXIS-1, qtrue);
    Cvar_CheckRange(j_up_axis, 0, MAX_JOYSTICK_AXIS-1, qtrue);

    cl_motdString = Cvar_Get( "cl_motdString", "", CVAR_ROM );

    cl_guidServerUniq = Cvar_Get ("cl_guidServerUniq", "1", CVAR_ARCHIVE);

    // ~ and `, as keys and characters
    cl_consoleKeys = Cvar_Get( "cl_consoleKeys", "~ ` 0x7e 0x60", CVAR_ARCHIVE); // TODO gain/lose focus

    // userinfo
    Cvar_Get ("name", "UnnamedPlayer", CVAR_USERINFO | CVAR_ARCHIVE );
    cl_rate = Cvar_Get ("rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE );
    Cvar_Get ("snaps", "20", CVAR_USERINFO | CVAR_ARCHIVE );
    Cvar_Get ("model", "sarge", CVAR_USERINFO | CVAR_ARCHIVE );
    Cvar_Get ("headmodel", "sarge", CVAR_USERINFO | CVAR_ARCHIVE );
    Cvar_Get ("team_model", "james", CVAR_USERINFO | CVAR_ARCHIVE );
    Cvar_Get ("team_headmodel", "*james", CVAR_USERINFO | CVAR_ARCHIVE );
    Cvar_Get ("g_redTeam", "Stroggs", CVAR_SERVERINFO | CVAR_ARCHIVE);
    Cvar_Get ("g_blueTeam", "Pagans", CVAR_SERVERINFO | CVAR_ARCHIVE);
    Cvar_Get ("color1",  "4", CVAR_USERINFO | CVAR_ARCHIVE );
    Cvar_Get ("color2", "5", CVAR_USERINFO | CVAR_ARCHIVE );
    Cvar_Get ("handicap", "100", CVAR_USERINFO | CVAR_ARCHIVE );
    Cvar_Get ("teamtask", "0", CVAR_USERINFO );
    Cvar_Get ("sex", "male", CVAR_USERINFO | CVAR_ARCHIVE );
    Cvar_Get ("cl_anonymous", "0", CVAR_USERINFO | CVAR_ARCHIVE );

    Cvar_Get ("cg_predictItems", "1", CVAR_USERINFO | CVAR_ARCHIVE );

    // cgame might not be initialized before menu is used
    Cvar_Get ("cg_viewsize", "100", CVAR_ARCHIVE );
    // Make sure cg_stereoSeparation is zero as that variable is deprecated and should not be used anymore.
    Cvar_Get ("cg_stereoSeparation", "0", CVAR_ROM);

    // register our commands
    Cmd_AddCommand ("cmd", CL_ForwardToServer_f); // TODO remove?
    Cmd_AddCommand ("configstrings", CL_Configstrings_f);
    Cmd_AddCommand ("clientinfo", CL_Clientinfo_f);
    Cmd_AddCommand ("snd_restart", CL_Snd_Restart_f);
    Cmd_AddCommand ("vid_restart", CL_Vid_Restart_f);
    Cmd_AddCommand ("disconnect", CL_Disconnect_f);
    //Cmd_AddCommand ("record", CL_Record_f);
    //Cmd_AddCommand ("demo", CL_PlayDemo_f);
    //Cmd_SetCommandCompletionFunc( "demo", CL_CompleteDemoName );
    //Cmd_AddCommand ("cinematic", CL_PlayCinematic_f);
    //Cmd_AddCommand ("stoprecord", CL_StopRecord_f);
    Cmd_AddCommand ("connect", CL_Connect_f);
    Cmd_AddCommand ("reconnect", CL_Reconnect_f);
    //Cmd_AddCommand ("localservers", CL_LocalServers_f);
    //Cmd_AddCommand ("globalservers", CL_GlobalServers_f);
    //Cmd_AddCommand ("rcon", CL_Rcon_f);
    //Cmd_SetCommandCompletionFunc( "rcon", CL_CompleteRcon );
    //Cmd_AddCommand ("ping", CL_Ping_f );
    //Cmd_AddCommand ("serverstatus", CL_ServerStatus_f );
    //Cmd_AddCommand ("showip", CL_ShowIP_f );
    //Cmd_AddCommand ("fs_openedList", CL_OpenedPK3List_f );
    //Cmd_AddCommand ("fs_referencedList", CL_ReferencedPK3List_f );
    Cmd_AddCommand ("model", CL_SetModel_f );
    //Cmd_AddCommand ("video", CL_Video_f );
    //Cmd_AddCommand ("stopvideo", CL_StopVideo_f );

    Cmd_AddCommand ("sayto", CL_Sayto_f );
    Cmd_SetCommandCompletionFunc( "sayto", CL_CompletePlayerName );

    CL_InitRef();

    SCR_Init ();

    Cbuf_Execute (); // TODO need?

    Cvar_Set( "cl_running", "1" );

    Com_Printf( "----- Client Initialization Complete -----\n" );
}

/*
===============
CL_Shutdown

===============
*/
void CL_Shutdown(char *finalmsg, qboolean disconnect, qboolean quit)
{
    static qboolean recursive = qfalse;

    // check whether the client is running at all.
    if(!(com_cl_running && com_cl_running->integer)) {
        return;
    }

    Com_Printf( "----- Client Shutdown (%s) -----\n", finalmsg );

    if ( recursive ) {
        Com_Printf( "WARNING: Recursive shutdown\n" );
        return;
    }
    recursive = qtrue;

    noGameRestart = quit;

    if(disconnect) {
        CL_Disconnect(qtrue);
    }

    CL_ClearMemory(qtrue);
    CL_Snd_Shutdown();

    Cmd_RemoveCommand ("cmd");
    Cmd_RemoveCommand ("configstrings");
    Cmd_RemoveCommand ("clientinfo");
    Cmd_RemoveCommand ("snd_restart");
    Cmd_RemoveCommand ("vid_restart");
    Cmd_RemoveCommand ("disconnect");
    Cmd_RemoveCommand ("record");
    Cmd_RemoveCommand ("demo");
    //Cmd_RemoveCommand ("cinematic");
    Cmd_RemoveCommand ("stoprecord");
    Cmd_RemoveCommand ("connect");
    Cmd_RemoveCommand ("reconnect");
    //Cmd_RemoveCommand ("localservers");
    //Cmd_RemoveCommand ("globalservers");
    //Cmd_RemoveCommand ("rcon");
    //Cmd_RemoveCommand ("ping");
    Cmd_RemoveCommand ("serverstatus");
    Cmd_RemoveCommand ("showip");
    Cmd_RemoveCommand ("fs_openedList");
    Cmd_RemoveCommand ("fs_referencedList");
    Cmd_RemoveCommand ("model");
    Cmd_RemoveCommand ("video");
    //Cmd_RemoveCommand ("stopvideo");

    CL_ShutdownInput();
    // Con_Shutdown(); // TODO

    Cvar_Set( "cl_running", "0" );

    recursive = qfalse;

    Com_Memset( &cls, 0, sizeof( cls ) );

    Com_Printf( "-----------------------\n" );
}

#if 0
static void CL_SetServerInfo(serverInfo_t *server, const char *info, int ping) {
    if (server) {
        if (info) {
            server->clients = atoi(Info_ValueForKey(info, "clients"));
            Q_strncpyz(server->hostName,Info_ValueForKey(info, "hostname"), MAX_NAME_LENGTH);
            Q_strncpyz(server->mapName, Info_ValueForKey(info, "mapname"), MAX_NAME_LENGTH);
            server->maxClients = atoi(Info_ValueForKey(info, "sv_maxclients"));
            Q_strncpyz(server->game,Info_ValueForKey(info, "game"), MAX_NAME_LENGTH);
            server->gameType = atoi(Info_ValueForKey(info, "gametype"));
            server->netType = atoi(Info_ValueForKey(info, "nettype"));
            server->minPing = atoi(Info_ValueForKey(info, "minping"));
            server->maxPing = atoi(Info_ValueForKey(info, "maxping"));
            server->punkbuster = atoi(Info_ValueForKey(info, "punkbuster"));
            server->g_humanplayers = atoi(Info_ValueForKey(info, "g_humanplayers"));
            server->g_needpass = atoi(Info_ValueForKey(info, "g_needpass"));
        }
        server->ping = ping;
    }
}
#endif

#if 0
static void CL_SetServerInfoByAddress(netadr_t from, const char *info, int ping) {
    int i;

    for (i = 0; i < MAX_OTHER_SERVERS; i++) {
        if (NET_CompareAdr(from, cls.localServers[i].adr)) {
            CL_SetServerInfo(&cls.localServers[i], info, ping);
        }
    }

    for (i = 0; i < MAX_GLOBAL_SERVERS; i++) {
        if (NET_CompareAdr(from, cls.globalServers[i].adr)) {
            CL_SetServerInfo(&cls.globalServers[i], info, ping);
        }
    }

    for (i = 0; i < MAX_OTHER_SERVERS; i++) {
        if (NET_CompareAdr(from, cls.favoriteServers[i].adr)) {
            CL_SetServerInfo(&cls.favoriteServers[i], info, ping);
        }
    }

}
#endif

/*
===================
CL_ServerInfoPacket
===================
*/
static void CL_ServerInfoPacket( netadr_t from, msg_t *msg ) {
    Com_Error(ERR_FATAL, "I guess I need this");
    // int     i, type;
    // char    info[MAX_INFO_STRING];
    // char    *infoString;
    // int     prot;
    // char    *gamename;
    // qboolean gameMismatch;

    char    *infoString;
    infoString = MSG_ReadString( msg );
    Com_Printf("%s %s infoString:%s\n", __FILE__, __func__, infoString);

    return; // DONT think I need any of this TODO

#if 0
    // if this isn't the correct gamename, ignore it
    gamename = Info_ValueForKey( infoString, "gamename" );

    gameMismatch = !*gamename || strcmp(gamename, com_gamename->string) != 0;

    if (gameMismatch)
    {
        Com_DPrintf( "Game mismatch in info packet: %s\n", infoString );
        return;
    }

    // if this isn't the correct protocol version, ignore it
    prot = atoi( Info_ValueForKey( infoString, "protocol" ) );

    if(prot != com_protocol->integer)
    {
        Com_DPrintf( "Different protocol info packet: %s\n", infoString );
        return;
    }

    // iterate servers waiting for ping response
    for (i=0; i<MAX_PINGREQUESTS; i++)
    {
        if ( cl_pinglist[i].adr.port && !cl_pinglist[i].time && NET_CompareAdr( from, cl_pinglist[i].adr ) )
        {
            // calc ping time
            cl_pinglist[i].time = Sys_Milliseconds() - cl_pinglist[i].start;
            Com_DPrintf( "ping time %dms from %s\n", cl_pinglist[i].time, NET_AdrToString( from ) );

            // save of info
            Q_strncpyz( cl_pinglist[i].info, infoString, sizeof( cl_pinglist[i].info ) );

            // tack on the net type
            // NOTE: make sure these types are in sync with the netnames strings in the UI
            switch (from.type)
            {
                case NA_BROADCAST:
                case NA_IP:
                    type = 1;
                    break;
                case NA_IP6:
                    type = 2;
                    break;
                default:
                    type = 0;
                    break;
            }
            Info_SetValueForKey( cl_pinglist[i].info, "nettype", va("%d", type) );
            CL_SetServerInfoByAddress(from, infoString, cl_pinglist[i].time);

            return;
        }
    }

    // if not just sent a local broadcast or pinging local servers
    if (cls.pingUpdateSource != AS_LOCAL) {
        return;
    }

    for ( i = 0 ; i < MAX_OTHER_SERVERS ; i++ ) {
        // empty slot
        if ( cls.localServers[i].adr.port == 0 ) {
            break;
        }

        // avoid duplicate
        if ( NET_CompareAdr( from, cls.localServers[i].adr ) ) {
            return;
        }
    }

    if ( i == MAX_OTHER_SERVERS ) {
        Com_DPrintf( "MAX_OTHER_SERVERS hit, dropping infoResponse\n" );
        return;
    }

    // add this to the list
    cls.numlocalservers = i+1;
    CL_InitServerInfo( &cls.localServers[i], &from );

    Q_strncpyz( info, MSG_ReadString( msg ), MAX_INFO_STRING );
    if (strlen(info)) {
        if (info[strlen(info)-1] != '\n') {
            Q_strcat(info, sizeof(info), "\n");
        }
        Com_Printf( "%s: %s", NET_AdrToStringwPort( from ), info );
    }
#endif
}

/*
===================
CL_ServerStatusResponse
===================
*/
void CL_ServerStatusResponse( netadr_t from, msg_t *msg ) {
    char    *s;
    char    info[MAX_INFO_STRING];
    int     i, l, score, ping;
    int     len;
    serverStatus_t *serverStatus;

    serverStatus = NULL;
    for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
        //if ( NET_CompareAdr( from, cl_serverStatusList[i].address ) ) {
            serverStatus = &cl_serverStatusList[i];
            break;
        //}
    }
    // if we didn't request this server status
    if (!serverStatus) {
        return;
    }

    s = MSG_ReadStringLine( msg );

    len = 0;
    Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "%s", s);

    if (serverStatus->print) {
        Com_Printf("Server settings:\n");
        // print cvars
        while (*s) {
            for (i = 0; i < 2 && *s; i++) {
                if (*s == '\\')
                    s++;
                l = 0;
                while (*s) {
                    info[l++] = *s;
                    if (l >= MAX_INFO_STRING-1)
                        break;
                    s++;
                    if (*s == '\\') {
                        break;
                    }
                }
                info[l] = '\0';
                if (i) {
                    Com_Printf("%s\n", info);
                }
                else {
                    Com_Printf("%-24s", info);
                }
            }
        }
    }

    len = strlen(serverStatus->string);
    Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\");

    if (serverStatus->print) {
        Com_Printf("\nPlayers:\n");
        Com_Printf("num: score: ping: name:\n");
    }
    for (i = 0, s = MSG_ReadStringLine( msg ); *s; s = MSG_ReadStringLine( msg ), i++) {

        len = strlen(serverStatus->string);
        Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\%s", s);

        if (serverStatus->print) {
            score = ping = 0;
            sscanf(s, "%d %d", &score, &ping);
            s = strchr(s, ' ');
            if (s)
                s = strchr(s+1, ' ');
            if (s)
                s++;
            else
                s = "unknown";
            Com_Printf("%-2d   %-3d    %-3d   %s\n", i, score, ping, s );
        }
    }
    len = strlen(serverStatus->string);
    Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\");

    serverStatus->time = Com_Milliseconds();
    serverStatus->address = from;
    serverStatus->pending = qfalse;
    if (serverStatus->print) {
        serverStatus->retrieved = qtrue;
    }
}

/*
==================
CL_LocalServers_f
==================
*/
void CL_LocalServers_f( void ) {
    char        *message;
    int         i, j;
    netadr_t    to;

    Com_Error(ERR_FATAL, "Scanning for servers on the local network...\n");

    // reset the list, waiting for response
    cls.numlocalservers = 0;
    cls.pingUpdateSource = AS_LOCAL;

    for (i = 0; i < MAX_OTHER_SERVERS; i++) {
        qboolean b = cls.localServers[i].visible;
        Com_Memset(&cls.localServers[i], 0, sizeof(cls.localServers[i]));
        cls.localServers[i].visible = b;
    }
    Com_Memset( &to, 0, sizeof( to ) );

    // The 'xxx' in the message is a challenge that will be echoed back
    // by the server.  We don't care about that here, but master servers
    // can use that to prevent spoofed server responses from invalid ip
    message = "\377\377\377\377getinfo xxx";

    // send each message twice in case one is dropped
    for ( i = 0 ; i < 2 ; i++ ) {
        // send a broadcast packet on each server port
        // we support multiple server ports so a single machine
        // can nicely run multiple servers
        for ( j = 0 ; j < NUM_SERVER_PORTS ; j++ ) {
            to.port = BigShort( (short)(PORT_SERVER + j) );

            to.type = NA_BROADCAST;
            NET_SendPacket( strlen( message ), message );
            to.type = NA_MULTICAST6;
            NET_SendPacket( strlen( message ), message );
        }
    }
}

/*
==================
CL_GlobalServers_f

Originally master 0 was Internet and master 1 was MPlayer.
ioquake3 2008; added support for requesting five separate master servers using 0-4.
ioquake3 2017; made master 0 fetch all master servers and 1-5 request a single master server.
==================
*/
#if 0
static void CL_GlobalServers_f( void ) {
    netadr_t    to;
    int         count, i, masterNum;
    char        command[1024], *masteraddress;

    if ((count = Cmd_Argc()) < 3 || (masterNum = atoi(Cmd_Argv(1))) < 0 || masterNum > MAX_MASTER_SERVERS)
    {
        Com_Printf("usage: globalservers <master# 0-%d> <protocol> [keywords]\n", MAX_MASTER_SERVERS);
        return;
    }

    // request from all master servers
    if ( masterNum == 0 ) {
        int numAddress = 0;

        for ( i = 1; i <= MAX_MASTER_SERVERS; i++ ) {
            sprintf(command, "sv_master%d", i);
            masteraddress = Cvar_VariableString(command);

            if(!*masteraddress)
                continue;

            numAddress++;

            Com_sprintf(command, sizeof(command), "globalservers %d %s %s\n", i, Cmd_Argv(2), Cmd_ArgsFrom(3));
            Cbuf_AddText(command);
        }

        if ( !numAddress ) {
            Com_Printf( "CL_GlobalServers_f: Error: No master server addresses.\n");
        }
        return;
    }

    sprintf(command, "sv_master%d", masterNum);
    masteraddress = Cvar_VariableString(command);

    if(!*masteraddress)
    {
        Com_Printf( "CL_GlobalServers_f: Error: No master server address given.\n");
        return;
    }

    // reset the list, waiting for response
    // -1 is used to distinguish a "no response"

    i = NET_StringToAdr(masteraddress, &to, NA_UNSPEC);

    if(!i)
    {
        Com_Printf( "CL_GlobalServers_f: Error: could not resolve address of master %s\n", masteraddress);
        return;
    }
    else if(i == 2)
        to.port = BigShort(PORT_MASTER);

    Com_Printf("Requesting servers from %s (%s)...\n", masteraddress, NET_AdrToStringwPort(to));

    cls.numglobalservers = -1;
    cls.pingUpdateSource = AS_GLOBAL;

    // Use the extended query for IPv6 masters
    if (to.type == NA_IP6 || to.type == NA_MULTICAST6)
    {
        int v4enabled = Cvar_VariableIntegerValue("net_enabled") & NET_ENABLEV4;

        if(v4enabled)
        {
            Com_sprintf(command, sizeof(command), "getserversExt %s %s",
                com_gamename->string, Cmd_Argv(2));
        }
        else
        {
            Com_sprintf(command, sizeof(command), "getserversExt %s %s ipv6",
                com_gamename->string, Cmd_Argv(2));
        }
    }
    else if ( !Q_stricmp( com_gamename->string, LEGACY_MASTER_GAMENAME ) )
        Com_sprintf(command, sizeof(command), "getservers %s",
            Cmd_Argv(2));
    else
        Com_sprintf(command, sizeof(command), "getservers %s %s",
            com_gamename->string, Cmd_Argv(2));

    for (i=3; i < count; i++)
    {
        Q_strcat(command, sizeof(command), " ");
        Q_strcat(command, sizeof(command), Cmd_Argv(i));
    }

    NET_OutOfBandPrint( NS_SERVER, to, "%s", command );
}
#endif


/*
==================
CL_GetPing
==================
*/
#if 0
void CL_GetPing( int n, char *buf, int buflen, int *pingtime )
{
    const char  *str;
    int     time;
    int     maxPing;

    if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port)
    {
        // empty or invalid slot
        buf[0]    = '\0';
        *pingtime = 0;
        return;
    }

    str = NET_AdrToStringwPort( cl_pinglist[n].adr );
    Q_strncpyz( buf, str, buflen );

    time = cl_pinglist[n].time;
    if (!time)
    {
        // check for timeout
        time = Sys_Milliseconds() - cl_pinglist[n].start;
        maxPing = Cvar_VariableIntegerValue( "cl_maxPing" );
        if( maxPing < 100 ) {
            maxPing = 100;
        }
        if (time < maxPing)
        {
            // not timed out yet
            time = 0;
        }
    }

    CL_SetServerInfoByAddress(cl_pinglist[n].adr, cl_pinglist[n].info, cl_pinglist[n].time);

    *pingtime = time;
}
#endif

/*
==================
CL_GetPingInfo
==================
*/
void CL_GetPingInfo( int n, char *buf, int buflen )
{
    if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port)
    {
        // empty or invalid slot
        if (buflen)
            buf[0] = '\0';
        return;
    }

    Q_strncpyz( buf, cl_pinglist[n].info, buflen );
}

/*
==================
CL_ClearPing
==================
*/
void CL_ClearPing( int n )
{
    if (n < 0 || n >= MAX_PINGREQUESTS)
        return;

    cl_pinglist[n].adr.port = 0;
}

/*
==================
CL_GetPingQueueCount
==================
*/
int CL_GetPingQueueCount( void )
{
    int     i;
    int     count;
    ping_t* pingptr;

    count   = 0;
    pingptr = cl_pinglist;

    for (i=0; i<MAX_PINGREQUESTS; i++, pingptr++ ) {
        if (pingptr->adr.port) {
            count++;
        }
    }

    return (count);
}

/*
==================
CL_GetFreePing
==================
*/
ping_t* CL_GetFreePing( void )
{
    ping_t* pingptr;
    ping_t* best;
    int     oldest;
    int     i;
    int     time;

    pingptr = cl_pinglist;
    for (i=0; i<MAX_PINGREQUESTS; i++, pingptr++ )
    {
        // find free ping slot
        if (pingptr->adr.port)
        {
            if (!pingptr->time)
            {
                if (Sys_Milliseconds() - pingptr->start < 500)
                {
                    // still waiting for response
                    continue;
                }
            }
            else if (pingptr->time < 500)
            {
                // results have not been queried
                continue;
            }
        }

        // clear it
        pingptr->adr.port = 0;
        return (pingptr);
    }

    // use oldest entry
    pingptr = cl_pinglist;
    best    = cl_pinglist;
    oldest  = INT_MIN;
    for (i=0; i<MAX_PINGREQUESTS; i++, pingptr++ )
    {
        // scan for oldest
        time = Sys_Milliseconds() - pingptr->start;
        if (time > oldest)
        {
            oldest = time;
            best   = pingptr;
        }
    }

    return (best);
}

/*
==================
CL_Ping_f
==================
*/
#if 0
void CL_Ping_f( void ) {
    netadr_t    to;
    ping_t*     pingptr;
    char*       server;
    int         argc;
    netadrtype_t    family = NA_UNSPEC;

    argc = Cmd_Argc();

    if ( argc != 2 && argc != 3 ) {
        Com_Printf( "usage: ping [-4|-6] server\n");
        return;
    }

    if(argc == 2)
        server = Cmd_Argv(1);
    else
    {
        if(!strcmp(Cmd_Argv(1), "-4"))
            family = NA_IP;
        else if(!strcmp(Cmd_Argv(1), "-6"))
            family = NA_IP6;
        else
            Com_Printf( "warning: only -4 or -6 as address type understood.\n");

        server = Cmd_Argv(2);
    }

    Com_Memset( &to, 0, sizeof(netadr_t) );

    if ( !NET_StringToAdr( server, &to, family ) ) {
        return;
    }

    pingptr = CL_GetFreePing();

    memcpy( &pingptr->adr, &to, sizeof (netadr_t) );
    pingptr->start = Sys_Milliseconds();
    pingptr->time  = 0;

    CL_SetServerInfoByAddress(pingptr->adr, NULL, 0);

    NET_OutOfBandPrint( NS_CLIENT, to, "getinfo xxx" );
}
#endif

/*
==================
CL_UpdateVisiblePings_f
==================
*/
#if 0
qboolean CL_UpdateVisiblePings_f(int source) {
    int         slots, i;
    char        buff[MAX_STRING_CHARS];
    int         pingTime;
    int         max;
    qboolean status = qfalse;

    if (source < 0 || source > AS_FAVORITES) {
        return qfalse;
    }

    cls.pingUpdateSource = source;

    slots = CL_GetPingQueueCount();
    if (slots < MAX_PINGREQUESTS) {
        serverInfo_t *server = NULL;

        switch (source) {
            case AS_LOCAL :
                server = &cls.localServers[0];
                max = cls.numlocalservers;
            break;
            case AS_GLOBAL :
                server = &cls.globalServers[0];
                max = cls.numglobalservers;
            break;
            case AS_FAVORITES :
                server = &cls.favoriteServers[0];
                max = cls.numfavoriteservers;
            break;
            default:
                return qfalse;
        }
        for (i = 0; i < max; i++) {
            if (server[i].visible) {
                if (server[i].ping == -1) {
                    int j;

                    if (slots >= MAX_PINGREQUESTS) {
                        break;
                    }
                    for (j = 0; j < MAX_PINGREQUESTS; j++) {
                        if (!cl_pinglist[j].adr.port) {
                            continue;
                        }
                        if (NET_CompareAdr( cl_pinglist[j].adr, server[i].adr)) {
                            // already on the list
                            break;
                        }
                    }
                    if (j >= MAX_PINGREQUESTS) {
                        status = qtrue;
                        for (j = 0; j < MAX_PINGREQUESTS; j++) {
                            if (!cl_pinglist[j].adr.port) {
                                break;
                            }
                        }
                        memcpy(&cl_pinglist[j].adr, &server[i].adr, sizeof(netadr_t));
                        cl_pinglist[j].start = Sys_Milliseconds();
                        cl_pinglist[j].time = 0;
                        NET_OutOfBandPrint( NS_CLIENT, cl_pinglist[j].adr, "getinfo xxx" );
                        slots++;
                    }
                }
                // if the server has a ping higher than cl_maxPing or
                // the ping packet got lost
                else if (server[i].ping == 0) {
                    // if we are updating global servers
                    if (source == AS_GLOBAL) {
                        //
                        if ( cls.numGlobalServerAddresses > 0 ) {
                            // overwrite this server with one from the additional global servers
                            cls.numGlobalServerAddresses--;
                            CL_InitServerInfo(&server[i], &cls.globalServerAddresses[cls.numGlobalServerAddresses]);
                            // NOTE: the server[i].visible flag stays untouched
                        }
                    }
                }
            }
        }
    }

    if (slots) {
        status = qtrue;
    }
    for (i = 0; i < MAX_PINGREQUESTS; i++) {
        if (!cl_pinglist[i].adr.port) {
            continue;
        }
        CL_GetPing( i, buff, MAX_STRING_CHARS, &pingTime );
        if (pingTime != 0) {
            CL_ClearPing(i);
            status = qtrue;
        }
    }

    return status;
}
#endif

/*
==================
CL_ServerStatus_f
==================
*/
#if 0
void CL_ServerStatus_f(void) {
    netadr_t    to, *toptr = NULL;
    char        *server;
    serverStatus_t *serverStatus;
    int         argc;
    netadrtype_t    family = NA_UNSPEC;

    argc = Cmd_Argc();

    if ( argc != 2 && argc != 3 )
    {
        if (clc.state != CA_ACTIVE || clc.demoplaying)
        {
            Com_Printf ("Not connected to a server.\n");
            Com_Printf( "usage: serverstatus [-4|-6] server\n");
            return;
        }

        toptr = &clc.serverAddress;
    }

    if(!toptr)
    {
        Com_Memset( &to, 0, sizeof(netadr_t) );

        if(argc == 2)
            server = Cmd_Argv(1);
        else
        {
            if(!strcmp(Cmd_Argv(1), "-4"))
                family = NA_IP;
            else if(!strcmp(Cmd_Argv(1), "-6"))
                family = NA_IP6;
            else
                Com_Printf( "warning: only -4 or -6 as address type understood.\n");

            server = Cmd_Argv(2);
        }

        toptr = &to;
        if ( !NET_StringToAdr( server, toptr, family ) )
            return;
    }

    NET_OutOfBandPrint( NS_CLIENT, *toptr, "getstatus" );

    serverStatus = CL_GetServerStatus( *toptr );
    serverStatus->address = *toptr;
    serverStatus->print = qtrue;
    serverStatus->pending = qtrue;
}
#endif

/*
==================
CL_ShowIP_f
==================
*/
//void CL_ShowIP_f(void) {
    //Sys_ShowIP();
//}

