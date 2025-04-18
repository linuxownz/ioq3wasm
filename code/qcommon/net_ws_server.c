// https://github.com/cloudflare/doom-wasm/blob/main/src/net_websockets.c
#include "../qcommon/q_shared_server.h"
#include "net_ws_server.h"
#include <arpa/inet.h>
#include <stdlib.h>

extern char *ws_getaddress(ws_cli_conn_t client);
extern char *ws_getport(ws_cli_conn_t client);
extern int   ws_close_client(ws_cli_conn_t client);
extern int   ws_get_state(ws_cli_conn_t client);
extern void  ws_ping(ws_cli_conn_t client, int threshold);
extern int   ws_sendframe_bcast( uint16_t port, const char *msg, uint64_t size, int type);
extern int   ws_sendframe_bin_bcast(uint16_t port, const char *msg, uint64_t size);
extern int   ws_sendframe_bin(ws_cli_conn_t client, const char *msg, uint64_t size);
extern int   ws_sendframe_txt_bcast(uint16_t port, const char *msg);
extern int   ws_sendframe_txt(ws_cli_conn_t client, const char *msg);
extern int   ws_sendframe( ws_cli_conn_t client, const char *msg, uint64_t size, int type);
extern int   ws_socket(struct ws_server *ws_srv);

//#define MAX_QUEUE_SIZE 64 # TODO
#define MAX_QUEUE_SIZE 16128
static qboolean ws_inited = qfalse;

typedef struct {
    net_packet_t *packets[MAX_QUEUE_SIZE * 4];
    int head, tail, count;
} packet_queue_t;

static packet_queue_t client_queue;

typedef struct {
    ws_cli_conn_t   cli;
    uint32_t         ip;
    unsigned short port;
    uint64_t bytes_sent;
    uint64_t bytes_recv;
    int      start_time;
} client_t;

client_t clients[MAX_CLIENTS];

static void WebsocketsQueueInit(packet_queue_t *queue) {
    queue->head = queue->tail = 0;
    memset(&client_queue, 0, sizeof(client_queue));
}

static void WebsocketsQueuePush(packet_queue_t *queue, net_packet_t *packet)
{
    if ( ! ws_inited ) {
        Com_Error(ERR_FATAL, "ws not initted");
    }

    int new_tail = (queue->tail + 1) % MAX_QUEUE_SIZE;

    if ( new_tail == queue->head ) {
        // queue is full
        Com_Error ( ERR_FATAL, "queue full");
        return;
    }

    queue->count++;
    queue->packets[queue->tail] = packet;
    queue->tail                 = new_tail;
}

static net_packet_t *WebsocketsQueuePop(packet_queue_t *queue)
{
    if ( ! ws_inited ) {
        Com_Error(ERR_FATAL, "ws not initted");
    }

    if (queue->tail == queue->head) {
        // queue empty
        return NULL;
    }

    queue->count--;
    net_packet_t *packet  = queue->packets[queue->head];
    queue->head = (queue->head + 1) % MAX_QUEUE_SIZE;

    return packet;
}

static int FindClientByIp ( uint32_t ip, unsigned short port ) {
    // add client to clients
    for ( int i = MAX_CLIENTS - 1 ; i >= 0 ; i-- ) {
        client_t c = clients[i];

        if ( c.cli ) {
            if ( ip == c.ip && port == c.port ) {
                return i;
            }
        }
    }

    return -1;
}

static int FindClient( ws_cli_conn_t client ) {
    char *cli = ws_getaddress(client);
    char *prt = ws_getport(client);

    struct in_addr in;
    inet_aton(cli, &in);

    uint32_t ip = in.s_addr;
    unsigned short port = atoi ( prt );

    // add client to clients
    for ( int i = MAX_CLIENTS - 1 ; i >= 0 ; i-- ) {
        client_t c = clients[i];

        if ( c.cli ) {
            if ( ip == c.ip && port == c.port ) {
                if ( client == c.cli ) {
                    return i;
                } else {
                    Com_Error(ERR_FATAL, "ip == ip and port == port but cli != cli?");
                }
            }
        }
    }

    return -1;
}

static qboolean AddClient(ws_cli_conn_t client) {

    if ( FindClient(client) == -1 ) {
        for ( int i = 0 ; i < MAX_CLIENTS ; i++ ) {
            if ( 0 == clients[i].cli ) {
                struct in_addr in;
                inet_aton(ws_getaddress(client), &in);

                uint32_t ip = in.s_addr;
                unsigned short port = atoi ( ws_getport(client) );

                clients[i].cli  = client;
                clients[i].ip   = ip;
                clients[i].port = port;
                clients[i].start_time = Sys_Milliseconds();
                clients[i].bytes_recv = 0;
                clients[i].bytes_sent = 0;

                return qtrue;
            }
        }
    }
    return qfalse;
}

static qboolean RemoveClient(ws_cli_conn_t client) {

    ws_close_client(client);

    int pos = FindClient(client);

    if ( -1 == pos ) {
        Com_Printf("failed to find client to remove\n");
        return qfalse;
    }

    client_t c = clients[pos];
    Com_Printf("Closing client:\n");
    Com_Printf("bytes sent:%lu\n", c.bytes_sent);
    Com_Printf("bytes recv:%lu\n", c.bytes_recv);

    memset(&clients[pos], 0, sizeof(client_t));

    return qtrue;
}

static net_packet_t *NET_Websockets_NewPacket( uint64_t size, uint32_t ip, unsigned short port )
{
    net_packet_t *n = malloc(sizeof(net_packet_t));

    if ( NULL == n ) {
        Com_Error(ERR_FATAL, "malloc");
    }

    memset(n, 0, sizeof(net_packet_t));

    n->data    = malloc ( size );

    if ( NULL == n->data ) {
        Com_Error ( ERR_FATAL, "malloc" );
    }

    n->alloced = qtrue;
    n->len     = size;
    n->ip      = ip;
    n->port    = port;

    return n;
}

qboolean NET_Websockets_RecvPacket(net_packet_t *packet)
{
    if ( ! ws_inited ) {
        Com_Error(ERR_FATAL, "ws not initted");
    }

    net_packet_t *p = WebsocketsQueuePop(&client_queue);

    if ( NULL != p) {
        packet->data    = p->data;
        packet->alloced = p->alloced;
        packet->ip      = p->ip;
        packet->port    = p->port;
        packet->len     = p->len;

        // NET_GetPacket frees data
        return qtrue;
    }

    return qfalse;
}

int NET_Websockets_SendPacket(net_packet_t *packet)
{
    int res = -1;

    if ( ! ws_inited ) {
        Com_Error(ERR_FATAL, "ws not initted");
    }

    int pos = FindClientByIp(packet->ip, packet->port);

    if ( pos >= 0 ) {
        clients[pos].bytes_sent += packet->len;

        res = ws_sendframe_bin(clients[pos].cli, (const char *)packet->data, packet->len);

        if ( packet->alloced ) {
            free(packet->data); // TODO
            packet->data = NULL;
        }
    } else {
        // TODO client is remove but server is still trying to send
        //Com_Printf("WARNING:FindClientByIp FAILED\n"); // TODO
        // Com_Error(ERR_FATAL,"FindClientByIp failed");
    }

    return res;
}

int NET_Websockets_GetQueueLength() {
    if ( ! ws_inited ) {
        Com_Error(ERR_FATAL, "ws not initted");
    }

    if ( client_queue.count ) {
        // printf("%s %d\n", __func__, client_queue.count);
    }

    return client_queue.count;
}

// MAX_CLIENTS set to 8 if not overridden in wsServer
// #define'd as MAX_CLIENTS 64 in qcommon/q_shared.h
// TODO test
static void onopen(ws_cli_conn_t client)
{
    // Com_Printf("onopen: %s %s\n", ws_getaddress(client), ws_getport(client));
    AddClient(client);
}

static void onclose(ws_cli_conn_t client)
{
    // char *cli; cli = ws_getaddress(client); printf("Connection closed, addr: %s\n", cli);
    RemoveClient(client);
}

static void onmessage(ws_cli_conn_t client, const unsigned char *msg, uint64_t size, int type)
{
    UNUSED(type);

    char *cli = ws_getaddress(client);
    char *prt = ws_getport(client);

    // printf("I receive a message: %s (size: %" PRId64 ", type: %d), from: %s\n", msg, size, type, cli);

    struct in_addr in;
    inet_aton(cli, &in);

    uint32_t ip         = in.s_addr;
    unsigned short port = atoi ( prt );

    net_packet_t *packet = NET_Websockets_NewPacket(size, ip, port);

    memcpy(packet->data, msg, size);

    WebsocketsQueuePush(&client_queue, packet);

    int pos = FindClient(client);
    if ( pos >= 0 ) {
        clients[pos].bytes_recv += size;
    } else {
        Com_Printf("FAILED to find client in onmessage\n");
    }
}

int NET_Websockets_Start (const int port)
{
    Com_Printf("NET_Websockets_Start(port:%d)\n", port);

    WebsocketsQueueInit(&client_queue);

    ws_socket(&(struct ws_server){
        /*
         * Bind host:
         * localhost -> localhost/127.0.0.1
         * 0.0.0.0   -> global IPv4
         * ::        -> global IPv4+IPv6 (DualStack)
         */
        .host          = "0.0.0.0", // TODO
        .port          = port,
        .thread_loop   = 1,
        .timeout_ms    = 1000,
        .cert          = "/etc/certs/domain.cert.pem",
        .cert_key      = "/etc/certs/private.key.pem",
        .evs.onopen    = onopen,
        .evs.onclose   = onclose,
        .evs.onmessage = onmessage
    });

    ws_inited = qtrue;

    return 0;
}

