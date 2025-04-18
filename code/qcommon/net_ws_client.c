/*
 * network websocket implementation to replace the regular network stuff
 */

#include "q_shared_client.h"
#include "net_ws_client.h"

#define MAX_QUEUE_SIZE 64

// TODO server disconnect

int Sys_Milliseconds (void);

typedef enum {
    NOT_CONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
} ConnState;

typedef struct _msg {
    char *data;
    int len;
    int time;
} msg_t;

typedef struct {
    msg_t msgs[MAX_QUEUE_SIZE];
    int head, tail;
} message_queue_t;

static ConnState connection_state = NOT_CONNECTED;
static EMSCRIPTEN_WEBSOCKET_T socket = -1;
static qboolean ws_inited = qfalse;
static message_queue_t msg_queue;

extern char commandLine[MAX_STRING_CHARS];

static char *EmscriptenWebSocketReadyStateToString ( unsigned short state ) {
    switch ( state ) {
        case 0: return "CONNECTING"; // Socket has been created. The connection is not yet open.
        case 1: return "OPEN";       // The connection is open and ready to communicate.
        case 2: return "CLOSING";    // The connection is in the process of closing.
        case 3: return "CLOSED";     // The connection is closed or couldn't be opened.
    }
    return "??";
}

static char * EmscriptenResultToString( int result ) {
    switch ( result ) {
        case EMSCRIPTEN_RESULT_SUCCESS             :return "EMSCRIPTEN_RESULT_SUCCESS";
        case EMSCRIPTEN_RESULT_DEFERRED            :return "EMSCRIPTEN_RESULT_DEFERRED";
        case EMSCRIPTEN_RESULT_NOT_SUPPORTED       :return "EMSCRIPTEN_RESULT_NOT_SUPPORTED";
        case EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED :return "EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED";
        case EMSCRIPTEN_RESULT_INVALID_TARGET      :return "EMSCRIPTEN_RESULT_INVALID_TARGET";
        case EMSCRIPTEN_RESULT_UNKNOWN_TARGET      :return "EMSCRIPTEN_RESULT_UNKNOWN_TARGET";
        case EMSCRIPTEN_RESULT_INVALID_PARAM       :return "EMSCRIPTEN_RESULT_INVALID_PARAM";
        case EMSCRIPTEN_RESULT_FAILED              :return "EMSCRIPTEN_RESULT_FAILED";
        case EMSCRIPTEN_RESULT_NO_DATA             :return "EMSCRIPTEN_RESULT_NO_DATA";
        case EMSCRIPTEN_RESULT_TIMED_OUT           :return "EMSCRIPTEN_RESULT_TIMED_OUT";
    }

    return "?";
}

static void WebsocketsQueueInit(message_queue_t *queue) {
    queue->head = queue->tail = 0;
    memset(&msg_queue, 0, sizeof(msg_queue));
}

static void WebsocketsQueuePush(message_queue_t *queue, msg_t *message)
{
    if ( ! ws_inited ) {
        Com_Error(ERR_FATAL, "ws not initted");
    }

    int new_tail = (queue->tail + 1) % MAX_QUEUE_SIZE;

    if ( new_tail == queue->head ) {
        // queue is full
        //Com_Error ( ERR_FATAL, "websocket queue full");
        return;
    }

    msg_t *p = &queue->msgs[queue->tail];

    p->len  = message->len;
    p->time = message->time;
    p->data = message->data;

    if ( p->len <= 0 ) {
        Com_Error(ERR_FATAL,"%s p->len <= 0", __func__);
    }

    assert(p->len > 0);

    queue->tail = new_tail;
}

static msg_t *WebsocketsQueuePop(message_queue_t *queue)
{
    if ( ! ws_inited ) {
        return NULL;
        Com_Error(ERR_FATAL, "ws not initted");
    }

    if (queue->tail == queue->head) {
        // queue empty
        return NULL;
    }

    msg_t *msg  = &queue->msgs[queue->head];
    queue->head = (queue->head + 1) % MAX_QUEUE_SIZE;

    return msg;
}


static const char *ConnectionStateToString( const ConnState state) {
    switch ( state ) {
        case NOT_CONNECTED: return "NOT_CONNECTED";
        case CONNECTING:    return "CONNECTING";
        case CONNECTED:     return "CONNECTED";
        case ERROR:         return "ERROR";
    }

    return "UNKNOWN";
}

qboolean WebSocketInit() {
    if (!emscripten_websocket_is_supported()) {
        printf("WebSockets are not supported, cannot continue!\n");
        return qfalse;
    } else if ( connection_state != NOT_CONNECTED ) {
        printf("connection state is NOT_CONNECTED in init: '%s'\n", ConnectionStateToString(connection_state));
        return qfalse;
    }

    WebsocketsQueueInit(&msg_queue);

    ws_inited = qtrue;

    connection_state = NOT_CONNECTED;

    return qtrue;
}

qboolean WebSocketShutdown() {
    emscripten_websocket_close(socket, 0, "WebSocketShutdown()");
    connection_state = NOT_CONNECTED;
    return qtrue;
}

int WebSocket_GetEvent(const unsigned char *p, int maxsize)
{
    if ( connection_state != CONNECTED ) {
        //printf("%s: incorrect state:'%s'\n", __func__, ConnectionStateToString(connection_state));
        //return -1;
    }

    if ( maxsize <= 0 ) {
        Com_Error(ERR_FATAL,"maxsize 0");
    }

    // copy data into dest static buffer and free
    msg_t *msg = WebsocketsQueuePop(&msg_queue);
    if ( msg ) {

        if ( msg->len == 0 ) {
            Com_Error(ERR_FATAL,"msg len = 0");
        }

        int len = MIN(msg->len, maxsize);
        memcpy ( (char *)p, msg->data, len );
        free(msg->data);
        memset(msg, 0, sizeof(msg_t));
        return len;
    }

    return -1;
}

qboolean WebSocket_SendEvent( const unsigned char *p, int maxsize ) {
    unsigned short readyState = 0;
    emscripten_websocket_get_ready_state( socket, &readyState );
    if ( readyState != 1 ) {
        //printf("%s readyState: %s\n", __func__, EmscriptenWebSocketReadyStateToString (readyState ) );
    }

    if ( connection_state != CONNECTED ) {
        //printf("%s: incorrect state:'%s'\n", __func__, ConnectionStateToString(connection_state));
        //printf("trying to send %s\n", p);
        return -1;
    }

    EMSCRIPTEN_RESULT res = emscripten_websocket_send_binary ( socket, (void *)p, maxsize );
    if ( res != EMSCRIPTEN_RESULT_SUCCESS ) {
        printf("%s res %d:%s\n", __func__, res, EmscriptenResultToString(res));
    }
    return res == EMSCRIPTEN_RESULT_SUCCESS;
}

static EM_BOOL WebSocketOpen(int eventType, const EmscriptenWebSocketOpenEvent *e, void *userData)
{
    connection_state = CONNECTED;

    //printf("connection complete open event (eventType=%d, userData=%ld)\n", eventType, (long)userData);

    if ( userData ) {
        void(*onopen)() = (void(*)())userData;
        //printf("calling onopen callback\n\n");
        (*onopen)(); // just calls CL_Connected_f which does nothing
        //printf("calling onopen callback DONE \n\n");
    }

    return EM_TRUE;
}

static EM_BOOL WebSocketClose(int eventType, const EmscriptenWebSocketCloseEvent *e, void *userData)
{
    connection_state = NOT_CONNECTED;

    //printf("close(eventType=%d, wasClean=%d, code=%d, reason=%s, userData=%ld)\n", eventType, e->wasClean, e->code, e->reason, (long)userData);
    emscripten_websocket_delete(socket);
    socket = -1;
    return EM_TRUE;
}

static EM_BOOL WebSocketError(int eventType, const EmscriptenWebSocketErrorEvent *e, void *userData)
{
    //emscripten_websocket_close(socket);
    emscripten_websocket_delete(socket);

    connection_state = ERROR;

    if ( userData ) {
        void(*onerror)() = (void(*)())userData;
        //printf("calling onerror callback\n\n");
        (*onerror)();
        //printf("calling onerror callback DONE \n\n");
    }

    printf("error(eventType=%d, %s userData=%ld)\n", eventType, EmscriptenWebSocketReadyStateToString(eventType), (long)userData);
    socket = -1;
    return EM_TRUE;
}

static EM_BOOL WebSocketMessage(int eventType, const EmscriptenWebSocketMessageEvent *e, void *userData)
{
    connection_state = CONNECTED;

    msg_t msg;

    msg.len = e->numBytes;
    msg.data = malloc ( msg.len );

    if ( msg.data == NULL ) {
        Com_Error(ERR_FATAL, "malloc");
    }

    memcpy ( msg.data, e->data, msg.len );
    msg.time = Sys_Milliseconds();

    WebsocketsQueuePush(&msg_queue, &msg);

    return EM_TRUE;
}

qboolean WebSocketConnect(void *p)
{
    EmscriptenWebSocketCreateAttributes attr;
    emscripten_websocket_init_create_attributes(&attr);

    attr.url = "ws://localhost:8081"; // TODO
                                      //
    char *hostname = emscripten_run_script_string("window.location.hostname;");

    char url[MAX_QPATH];
    char *port = strstr(commandLine, "port=");
    if ( port ) {
        Com_sprintf(url, MAX_QPATH - 1, "wss://%s:%s", hostname, port + 5); // TODO ws vs wss
        attr.url = url; // TODO
    }

    Com_Printf("Connecting to url: %s\n", url);

    socket = emscripten_websocket_new(&attr);

    if ( socket <= 0 ) {
        printf("WebSocket creation failed, error code %d!\n", (EMSCRIPTEN_RESULT)socket);
        return qfalse;
    }

    emscripten_websocket_set_onopen_callback    ( socket,    p, WebSocketOpen);
    emscripten_websocket_set_onclose_callback   ( socket, NULL, WebSocketClose);
    emscripten_websocket_set_onerror_callback   ( socket, NULL, WebSocketError);
    emscripten_websocket_set_onmessage_callback ( socket, NULL, WebSocketMessage);

    connection_state = CONNECTING;

    return qtrue;
}

