#include <stdio.h>
#include <string.h>

#include "../wsServer/include/base64.h"
#include "../wsServer/include/sha1.h"
#include "../wsServer/include/utf8.h"
#include "../wsServer/include/ws.h"

typedef struct _net_packet_s
{
    byte *data;
    size_t len;
    size_t alloced;
    uint32_t ip;
    unsigned short port;
} net_packet_t;

#include "../qcommon/qcommon_server.h"

int      NET_Websockets_Start (const int port);
qboolean NET_Websockets_RecvPacket(net_packet_t *packet);
int      NET_Websockets_SendPacket(net_packet_t *packet);
int      NET_Websockets_GetQueueLength(void);

