/* See LICENSE file for copyright and license details. */
#include "ipc.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_tree.h>

#include "util.h"
#include "yajl_dumps.h"

static struct sockaddr_un sockaddr;
static struct epoll_event sock_epoll_event;
static IPCClientList ipc_clients = NULL;
static int sock_fd = -1;
static IPCCommand* ipc_commands;
static unsigned int ipc_commands_len;
static const uint32_t MAX_MESSAGE_SIZE = 1000000;
static const int IPC_SOCKET_BACKLOG = 5;

static int
ipc_create_socket(const char* filename)
{
    const size_t addr_size = sizeof(struct sockaddr_un);
    const int sock_type = SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC;

    unlink(filename);

    memset(&sockaddr, 0, addr_size);
    sockaddr.sun_family = AF_LOCAL;
    strncpy(sockaddr.sun_path, filename, sizeof(sockaddr.sun_path) - 1);

    sock_fd = socket(AF_LOCAL, sock_type, 0);
    if (sock_fd == -1)
    {
        fputs("Failed to create socket\n", stderr);
        return -1;
    }

    if (bind(sock_fd, (const struct sockaddr*)&sockaddr, addr_size) == -1)
    {
        fputs("Failed to bind socket\n", stderr);
        return -1;
    }

    if (listen(sock_fd, IPC_SOCKET_BACKLOG) < 0)
    {
        fputs("Failed to listen for connections on socket\n", stderr);
        return -1;
    }

    return sock_fd;
}

static int
ipc_recv_message(int fd, uint8_t* msg_type, uint32_t* reply_size,
                 uint8_t** reply)
{
    uint32_t read_bytes = 0;
    const int32_t to_read = sizeof(dwm_ipc_header_t);
    char header[to_read];
    char* walk = header;

    while (read_bytes < to_read)
    {
        const ssize_t n = read(fd, header + read_bytes, to_read - read_bytes);
        if (n == 0) return -2;
        else if (n == -1) return -1;
        read_bytes += n;
    }

    if (memcmp(walk, IPC_MAGIC, IPC_MAGIC_LEN) != 0)
        return -3;
    walk += IPC_MAGIC_LEN;

    memcpy(reply_size, walk, sizeof(uint32_t));
    walk += sizeof(uint32_t);

    if (*reply_size > MAX_MESSAGE_SIZE)
        return -4;

    memcpy(msg_type, walk, sizeof(uint8_t));
    walk += sizeof(uint8_t);

    if (*reply_size > 0)
        (*reply) = malloc(*reply_size);
    else
        return 0;

    read_bytes = 0;
    while (read_bytes < *reply_size)
    {
        const ssize_t n = read(fd, *reply + read_bytes, *reply_size - read_bytes);
        if (n == 0)
        {
            free(*reply);
            return -2;
        }
        else if (n == -1)
        {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
            free(*reply);
            return -1;
        }
        read_bytes += n;
    }

    return 0;
}

static ssize_t
ipc_write_message(int fd, const void* buf, size_t count)
{
    size_t written = 0;
    while (written < count)
    {
        const ssize_t n = write(fd, (uint8_t*)buf + written, count - written);
        if (n == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return written;
            else if (errno == EINTR)
                continue;
            else
                return n;
        }
        written += n;
    }
    return written;
}

static void
ipc_event_init_message(yajl_gen* gen)
{
    *gen = yajl_gen_alloc(NULL);
    yajl_gen_config(*gen, yajl_gen_beautify, 1);
}

static void
ipc_reply_init_message(yajl_gen* gen)
{
    *gen = yajl_gen_alloc(NULL);
    yajl_gen_config(*gen, yajl_gen_beautify, 1);
}

void
ipc_prepare_send_message(IPCClient* c, IPCMessageType msg_type,
                         uint32_t msg_size, const char* msg)
{
    dwm_ipc_header_t header = {
        .magic = IPC_MAGIC_ARR,
        .size = msg_size,
        .type = msg_type
    };

    uint32_t total_size = sizeof(dwm_ipc_header_t) + msg_size;
    c->buffer = realloc(c->buffer, c->buffer_size + total_size);

    memcpy(c->buffer + c->buffer_size, &header, sizeof(dwm_ipc_header_t));
    c->buffer_size += sizeof(dwm_ipc_header_t);

    if (msg_size > 0 && msg)
    {
        memcpy(c->buffer + c->buffer_size, msg, msg_size);
        c->buffer_size += msg_size;
    }

    c->event.events |= EPOLLOUT;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, c->fd, &c->event);
}

static void
ipc_event_prepare_send_message(yajl_gen gen, IPCEvent event)
{
    const unsigned char* buffer;
    size_t len = 0;

    yajl_gen_get_buf(gen, &buffer, &len);
    len++;

    for (IPCClient* c = ipc_clients; c; c = c->next)
    {
        if (c->subscriptions & event)
        {
            ipc_prepare_send_message(c, IPC_TYPE_EVENT, len, (char*)buffer);
        }
    }

    yajl_gen_free(gen);
}

static void
ipc_reply_prepare_send_message(yajl_gen gen, IPCClient* c,
                               IPCMessageType msg_type)
{
    const unsigned char* buffer;
    size_t len = 0;

    yajl_gen_get_buf(gen, &buffer, &len);
    len++;

    ipc_prepare_send_message(c, msg_type, len, (const char*)buffer);
    yajl_gen_free(gen);
}

static int
ipc_get_ipc_command(const char* name, IPCCommand* ipc_command)
{
    for (unsigned int i = 0; i < ipc_commands_len; i++)
    {
        if (strcmp(ipc_commands[i].name, name) == 0)
        {
            *ipc_command = ipc_commands[i];
            return 0;
        }
    }
    return -1;
}

static int
ipc_parse_run_command(const char* msg, IPCParsedCommand* command)
{
    char errbuf[1024];
    yajl_val node = yajl_tree_parse(msg, errbuf, sizeof(errbuf));
    if (!node) return -1;

    const char* command_path[] = {"command", NULL};
    yajl_val command_val = yajl_tree_get(node, command_path, yajl_t_string);
    if (!command_val)
    {
        yajl_tree_free(node);
        return -1;
    }

    command->name = strdup(YAJL_GET_STRING(command_val));
    IPCCommand ipc_cmd;
    if (ipc_get_ipc_command(command->name, &ipc_cmd) < 0)
    {
        free(command->name);
        yajl_tree_free(node);
        return -2;
    }

    const char* args_path[] = {"args", NULL};
    yajl_val args_val = yajl_tree_get(node, args_path, yajl_t_array);
    if (!args_val || YAJL_GET_ARRAY(args_val)->len != ipc_cmd.argc)
    {
        free(command->name);
        yajl_tree_free(node);
        return -3;
    }

    command->argc = ipc_cmd.argc;
    command->args = calloc(command->argc, sizeof(Arg));
    command->arg_types = calloc(command->argc, sizeof(ArgType));

    for (unsigned int i = 0; i < command->argc; i++)
    {
        yajl_val arg_val = YAJL_GET_ARRAY(args_val)->values[i];
        command->arg_types[i] = ipc_cmd.arg_types[i];

        switch (ipc_cmd.arg_types[i])
        {
        case ARG_TYPE_UINT:
            command->args[i].ui = (unsigned int)YAJL_GET_INTEGER(arg_val);
            break;
        case ARG_TYPE_SINT:
            command->args[i].i = (int)YAJL_GET_INTEGER(arg_val);
            break;
        case ARG_TYPE_FLOAT:
            command->args[i].f = (float)YAJL_GET_DOUBLE(arg_val);
            break;
        case ARG_TYPE_STR:
            command->args[i].v = strdup(YAJL_GET_STRING(arg_val));
            break;
        default:
            break;
        }
    }

    yajl_tree_free(node);
    return 0;
}

static void
ipc_free_parsed_command_members(IPCParsedCommand* command)
{
    if (!command) return;
    if (command->name) free(command->name);
    if (command->args)
    {
        for (unsigned int i = 0; i < command->argc; i++)
        {
            if (command->arg_types[i] == ARG_TYPE_STR && command->args[i].v)
                free((void*)command->args[i].v);
        }
        free(command->args);
    }
    if (command->arg_types) free(command->arg_types);
}

static int
ipc_run_command(IPCParsedCommand* command)
{
    IPCCommand ipc_cmd;
    if (ipc_get_ipc_command(command->name, &ipc_cmd) < 0)
        return -1;

    if (command->argc == 1)
    {
        ipc_cmd.func.single_param(&command->args[0]);
    }
    else if (command->argc > 1)
    {
        ipc_cmd.func.array_param(command->args, command->argc);
    }
    else
    {
        Arg a = {0};
        ipc_cmd.func.single_param(&a);
    }
    return 0;
}

static int
ipc_handle_run_command(IPCClient* c, const char* msg)
{
    IPCParsedCommand command = {0};
    int res = ipc_parse_run_command(msg, &command);

    yajl_gen gen;
    ipc_reply_init_message(&gen);

    if (res == 0)
    {
        ipc_run_command(&command);
        ipc_free_parsed_command_members(&command);
        YMAP(YSTR("result");
        YSTR("success");)
    }
    else
    {
        YMAP(YSTR("result");
        YSTR("failure");
        YSTR("reason");
        YSTR("Failed parsing command or arguments");)
    }

    ipc_reply_prepare_send_message(gen, c, IPC_TYPE_RUN_COMMAND);
    return 0;
}

static int
ipc_handle_get_monitors(IPCClient* c)
{
    yajl_gen gen;
    ipc_reply_init_message(&gen);
    dump_monitors(gen, mons, selmon);
    ipc_reply_prepare_send_message(gen, c, IPC_TYPE_GET_MONITORS);
    return 0;
}

static int
ipc_handle_get_tags(IPCClient* c)
{
    yajl_gen gen;
    ipc_reply_init_message(&gen);
    dump_tags(gen, tags, 9);
    ipc_reply_prepare_send_message(gen, c, IPC_TYPE_GET_TAGS);
    return 0;
}

static int
ipc_handle_get_layouts(IPCClient* c)
{
    yajl_gen gen;
    ipc_reply_init_message(&gen);
    dump_layouts(gen, layouts, 3);
    ipc_reply_prepare_send_message(gen, c, IPC_TYPE_GET_LAYOUTS);
    return 0;
}

static int
ipc_handle_get_dwm_client(IPCClient* c, const char* msg)
{
    char errbuf[1024];
    yajl_val node = yajl_tree_parse(msg, errbuf, sizeof(errbuf));
    yajl_gen gen;
    ipc_reply_init_message(&gen);

    if (node)
    {
        const char* path[] = {"client_window_id", NULL};
        yajl_val val = yajl_tree_get(node, path, yajl_t_number);
        if (val)
        {
            Window w = (Window)YAJL_GET_INTEGER(val);
            Client* cl = wintoclient(w);
            dump_client(gen, cl);
        }
        else
        {
            dump_error(gen, "Missing client_window_id parameter");
        }
        yajl_tree_free(node);
    }
    else
    {
        dump_error(gen, "Invalid JSON payload");
    }

    ipc_reply_prepare_send_message(gen, c, IPC_TYPE_GET_DWM_CLIENT);
    return 0;
}

static int
ipc_handle_subscribe(IPCClient* c, const char* msg)
{
    char errbuf[1024];
    yajl_val node = yajl_tree_parse(msg, errbuf, sizeof(errbuf));
    yajl_gen gen;
    ipc_reply_init_message(&gen);

    if (node)
    {
        const char* event_path[] = {"event", NULL};
        const char* action_path[] = {"action", NULL};
        yajl_val event_val = yajl_tree_get(node, event_path, yajl_t_string);
        yajl_val action_val = yajl_tree_get(node, action_path, yajl_t_number);

        if (event_val && action_val)
        {
            const char* ev = YAJL_GET_STRING(event_val);
            int action = (int)YAJL_GET_INTEGER(action_val);
            IPCEvent event_bit = 0;

            if (!strcmp(ev, "tag_change")) event_bit = IPC_EVENT_TAG_CHANGE;
            else if (!strcmp(ev, "client_focus_change")) event_bit = IPC_EVENT_CLIENT_FOCUS_CHANGE;
            else if (!strcmp(ev, "layout_change")) event_bit = IPC_EVENT_LAYOUT_CHANGE;
            else if (!strcmp(ev, "monitor_focus_change")) event_bit = IPC_EVENT_MONITOR_FOCUS_CHANGE;
            else if (!strcmp(ev, "focused_title_change")) event_bit = IPC_EVENT_FOCUSED_TITLE_CHANGE;
            else if (!strcmp(ev, "focused_state_change")) event_bit = IPC_EVENT_FOCUSED_STATE_CHANGE;

            if (action == IPC_ACTION_SUBSCRIBE) c->subscriptions |= event_bit;
            else if (action == IPC_ACTION_UNSUBSCRIBE) c->subscriptions &= ~event_bit;

            YMAP(YSTR("result");
            YSTR("success");)
        }
        else
        {
            YMAP(YSTR("result");
            YSTR("failure");)
        }
        yajl_tree_free(node);
    }
    else
    {
        YMAP(YSTR("result");
        YSTR("failure");)
    }

    ipc_reply_prepare_send_message(gen, c, IPC_TYPE_SUBSCRIBE);
    return 0;
}

int
ipc_init(const char* socket_path, const int p_epoll_fd,
         IPCCommand commands[], const int commands_len)
{
    epoll_fd = p_epoll_fd;
    ipc_commands = commands;
    ipc_commands_len = commands_len;

    if (ipc_create_socket(socket_path) < 0)
        return -1;

    sock_epoll_event.events = EPOLLIN;
    sock_epoll_event.data.fd = sock_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &sock_epoll_event) < 0)
    {
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

void
ipc_cleanup(void)
{
    while (ipc_clients)
        ipc_drop_client(ipc_clients);

    if (sock_fd != -1)
    {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock_fd, NULL);
        close(sock_fd);
        unlink(sockaddr.sun_path);
        sock_fd = -1;
    }
}

int
ipc_get_sock_fd(void)
{
    return sock_fd;
}

IPCClient*
ipc_get_client(int fd)
{
    return ipc_list_get_client(ipc_clients, fd);
}

int
ipc_is_client_registered(int fd)
{
    return ipc_get_client(fd) != NULL;
}

int
ipc_accept_client(void)
{
    int fd = accept(sock_fd, NULL, NULL);
    if (fd < 0) return -1;

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    IPCClient* c = ipc_client_new(fd);
    if (!c)
    {
        close(fd);
        return -1;
    }

    c->event.events = EPOLLIN;
    c->event.data.fd = fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &c->event) < 0)
    {
        ipc_client_free(c);
        close(fd);
        return -1;
    }

    ipc_list_add_client(&ipc_clients, c);
    return fd;
}

int
ipc_drop_client(IPCClient* c)
{
    if (!c) return -1;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, c->fd, NULL);
    close(c->fd);
    ipc_list_remove_client(&ipc_clients, c);
    ipc_client_free(c);
    return 0;
}

int
ipc_read_client(IPCClient* c, uint8_t* msg_type, uint32_t* msg_size, char** msg)
{
    uint8_t* payload = NULL;
    int res = ipc_recv_message(c->fd, msg_type, msg_size, &payload);
    if (res < 0) return res;

    *msg = (char*)payload;

    switch (*msg_type)
    {
    case IPC_TYPE_RUN_COMMAND:
        ipc_handle_run_command(c, *msg);
        break;
    case IPC_TYPE_GET_MONITORS:
        ipc_handle_get_monitors(c);
        break;
    case IPC_TYPE_GET_TAGS:
        ipc_handle_get_tags(c);
        break;
    case IPC_TYPE_GET_LAYOUTS:
        ipc_handle_get_layouts(c);
        break;
    case IPC_TYPE_GET_DWM_CLIENT:
        ipc_handle_get_dwm_client(c, *msg);
        break;
    case IPC_TYPE_SUBSCRIBE:
        ipc_handle_subscribe(c, *msg);
        break;
    default:
        break;
    }

    return 0;
}

int
ipc_write_client(IPCClient* c)
{
    if (!c || c->buffer_size == 0) return 0;

    ssize_t written = ipc_write_message(c->fd, c->buffer, c->buffer_size);
    if (written < 0) return -1;

    if ((uint32_t)written < c->buffer_size)
    {
        memmove(c->buffer, c->buffer + written, c->buffer_size - written);
        c->buffer_size -= written;
    }
    else
    {
        free(c->buffer);
        c->buffer = NULL;
        c->buffer_size = 0;
        c->event.events &= ~EPOLLOUT;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, c->fd, &c->event);
    }

    return 0;
}

int
ipc_send_event(const char* event_name, IPCEvent event, ...)
{
    if (!ipc_clients) return 0;

    yajl_gen gen;
    ipc_event_init_message(&gen);

    YMAP(
        YSTR("event");
    YSTR(event_name);
    )

    ipc_event_prepare_send_message(gen, event);
    return 0;
}
