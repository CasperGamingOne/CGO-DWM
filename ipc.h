/* See LICENSE file for copyright and license details. */
#ifndef IPC_H_
#define IPC_H_

#include <stdint.h>
#include <sys/epoll.h>
#include <yajl/yajl_gen.h>

#include "dwm.h"
#include "IPCClient.h"

#define IPC_MAGIC "DWM-IPC"
#define IPC_MAGIC_ARR { 'D', 'W', 'M', '-', 'I', 'P', 'C'}
#define IPC_MAGIC_LEN 7

#define IPCCOMMAND(FUNC, ARGC, TYPES)                                          \
  { #FUNC, {FUNC }, ARGC, (ArgType[ARGC])TYPES }

typedef enum IPCMessageType
{
    IPC_TYPE_RUN_COMMAND = 0,
    IPC_TYPE_GET_MONITORS = 1,
    IPC_TYPE_GET_TAGS = 2,
    IPC_TYPE_GET_LAYOUTS = 3,
    IPC_TYPE_GET_DWM_CLIENT = 4,
    IPC_TYPE_SUBSCRIBE = 5,
    IPC_TYPE_EVENT = 6
} IPCMessageType;

typedef enum IPCEvent
{
    IPC_EVENT_TAG_CHANGE = 1 << 0,
    IPC_EVENT_CLIENT_FOCUS_CHANGE = 1 << 1,
    IPC_EVENT_LAYOUT_CHANGE = 1 << 2,
    IPC_EVENT_MONITOR_FOCUS_CHANGE = 1 << 3,
    IPC_EVENT_FOCUSED_TITLE_CHANGE = 1 << 4,
    IPC_EVENT_FOCUSED_STATE_CHANGE = 1 << 5
} IPCEvent;

typedef enum IPCSubscriptionAction
{
    IPC_ACTION_UNSUBSCRIBE = 0,
    IPC_ACTION_SUBSCRIBE = 1
} IPCSubscriptionAction;

typedef struct dwm_ipc_header
{
    uint8_t magic[IPC_MAGIC_LEN];
    uint32_t size;
    uint8_t type;
} __attribute__((packed)

)
dwm_ipc_header_t;

typedef enum ArgType
{
    ARG_TYPE_NONE = 0,
    ARG_TYPE_UINT = 1,
    ARG_TYPE_SINT = 2,
    ARG_TYPE_FLOAT = 3,
    ARG_TYPE_PTR = 4,
    ARG_TYPE_STR = 5
} ArgType;

typedef union ArgFunction
{
    void (*single_param)(const Arg*);
    void (*array_param)(const Arg*, int);
} ArgFunction;

typedef struct IPCCommand
{
    char* name;
    ArgFunction func;
    unsigned int argc;
    ArgType* arg_types;
} IPCCommand;

typedef struct IPCParsedCommand
{
    char* name;
    Arg* args;
    ArgType* arg_types;
    unsigned int argc;
} IPCParsedCommand;

int ipc_init(const char* socket_path, const int p_epoll_fd,
             IPCCommand commands[], const int commands_len);
void ipc_cleanup(void);
int ipc_get_sock_fd(void);
IPCClient* ipc_get_client(int fd);
int ipc_is_client_registered(int fd);

int ipc_accept_client(void);
int ipc_drop_client(IPCClient * c);
int ipc_read_client(IPCClient* c, uint8_t* msg_type, uint32_t* msg_size, char** msg);
int ipc_write_client(IPCClient * c);

void ipc_prepare_send_message(IPCClient* c, IPCMessageType msg_type,
                              uint32_t msg_size, const char* msg);
int ipc_send_event(const char* event_name, IPCEvent event, ...);

#endif /* IPC_H_ */
