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

#include <errno.h>

#include "../qcommon/q_shared_client.h"
#include "../qcommon/qcommon_client.h"
#include "../qcommon/net_ws_client.h"

#define INVALID_SOCKET      -1
#define SOCKET_ERROR        -1
#define closesocket         close
#define ioctlsocket         ioctl
typedef int ioctlarg_t;
#define socketError         errno

//static cvar_t *net_ip;
//static cvar_t *net_port;

//typedef int SOCKET;
//static SOCKET ip_socket = INVALID_SOCKET;

/*
====================
NET_ErrorString
====================
*/
char *NET_ErrorString( void ) {
    return strerror(errno);
}

/*
==================
NET_GetPacket
Receive one packet
==================
*/
static qboolean NET_GetPacket(msg_t *net_message)
{
    int ret = WebSocket_GetEvent(net_message->data, net_message->maxsize);

    if ( -1 == ret ) {
        return qfalse;
        // nothing to get
        //Com_Error(ERR_FATAL, "no data...\n");
    } else if ( 0 == ret ) {
        Com_Printf(" #################### data size 0\n");
        return qfalse;
        //Com_Error(ERR_FATAL, "data size 0\n");
    }

    net_message->readcount = 0;

    if( ret >= net_message->maxsize ) {
        Com_Error( ERR_FATAL, "Oversize packet from the server maxsize:%d size:%d\n", net_message->maxsize, ret );
        return qfalse;
    }

    net_message->cursize = ret;

    return qtrue;
}

/*
==================
Sys_SendPacket
==================
*/
void Sys_SendPacket( int length, const void *data ) {

    qboolean result = WebSocket_SendEvent((void *)data, length);

    if ( ! result ) {
        Com_Printf("%s WebSocket_SendEvent, failed\n", __func__);
    }
}

/*
====================
NET_IPSocket creates the socket to be replaced
====================
*/
#if 0
// unused
static SOCKET NET_IPSocket( char *net_interface, int port, int *err ) {
    UNUSED(net_interface); UNUSED(port); UNUSED(err);

    Com_Error(ERR_FATAL, "NET_IPSocket");

    return -1;
}
#endif

/*
====================
NET_OpenIP
====================
*/
#if 0
// unused?
void NET_OpenIP( void ) {
    int     err;
    int     port;

    port = net_port->integer;
    net_ip = Cvar_Get( "net_ip", "0.0.0.0", CVAR_LATCH );

    ip_socket = NET_IPSocket( net_ip->string, port, &err );

    if(ip_socket == INVALID_SOCKET)
        Com_Printf( "WARNING: Couldn't bind to a v4 ip address.\n");
}
#endif

/*
====================
NET_Config
====================
*/
void NET_Config( qboolean enableNetworking ) {
    UNUSED(enableNetworking);
}

/*
====================
NET_Init
====================
*/
void NET_Init( void ) {

    MSG_initHuffman();

    if ( ! WebSocketInit() ) {
        Com_Error(ERR_FATAL, "Failed to init network");
    }

    Cmd_AddCommand ("net_restart", NET_Restart_f);
}

/*
====================
NET_Shutdown
====================
*/
void NET_Shutdown( void ) {
    // TODO this needs to be implemented
}

/*
====================
NET_Event

Called from NET_Sleep which uses select() to determine which sockets have seen action.
CALLED from qcommon/common.c Com_Frame
====================
*/

void NET_Event()
{
    byte bufData[MAX_MSGLEN + 1]; // TODO
    netadr_t from = {0};
    msg_t netmsg;

    while(1)
    {
        MSG_Init(&netmsg, bufData, sizeof(bufData));

        if(NET_GetPacket(&netmsg))
        {
            if(qfalse) // TODO net_dropsim->value > 0.0f && net_dropsim->value <= 100.0f)
            {
                // com_dropsim->value percent of incoming packets get dropped.
                //if(rand() < (int) (((double) RAND_MAX) / 100.0 * (double) net_dropsim->value))
                //    continue;          // drop this packet
            }

            CL_PacketEvent(from, &netmsg);
        }
        else {
            break;
        }
    }
}

/*
====================
NET_Restart_f
====================
*/
void NET_Restart_f(void)
{
    NET_Config(qtrue);
}

