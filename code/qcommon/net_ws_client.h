/*
 * net_ws.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <emscripten/websocket.h>
#include "q_shared_client.h"

#ifndef __NET_WS_H__
#define __NET_WS_H__

qboolean WebSocketInit(void);
qboolean WebSocketConnect(void *p);
qboolean WebSocketShutdown(void);
int WebSocket_GetEvent(const unsigned char *p, int maxsize);
qboolean WebSocket_SendEvent(const unsigned char *p, int maxsize);
#endif
