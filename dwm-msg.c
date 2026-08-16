/* See LICENSE file for copyright and license details. */
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <yajl/yajl_gen.h>

#define IPC_MAGIC "DWM-IPC"
#define IPC_MAGIC_ARR { 'D', 'W', 'M', '-', 'I', 'P', 'C' }
#define IPC_MAGIC_LEN 7

const char* DEFAULT_SOCKET_PATH = "/tmp/dwm.sock";
static int sock_fd = -1;

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

typedef struct dwm_ipc_header
{
    uint8_t magic[IPC_MAGIC_LEN];
    uint32_t size;
    uint8_t type;
} __attribute__((packed)

)
dwm_ipc_header_t;

static int
connect_to_socket(const char* path)
{
    struct sockaddr_un sockaddr;
    sock_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sock_fd < 0) return -1;

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sun_family = AF_LOCAL;
    strncpy(sockaddr.sun_path, path, sizeof(sockaddr.sun_path) - 1);

    if (connect(sock_fd, (const struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0)
    {
        close(sock_fd);
        return -1;
    }
    return sock_fd;
}

static int
send_ipc_message(IPCMessageType type, uint32_t size, const char* payload)
{
    dwm_ipc_header_t header = {
        .magic = IPC_MAGIC_ARR,
        .size = size,
        .type = type
    };

    if (write(sock_fd, &header, sizeof(header)) != sizeof(header))
        return -1;
    if (size > 0 && payload)
    {
        if (write(sock_fd, payload, size) != (ssize_t)size)
            return -1;
    }
    return 0;
}

static int
read_ipc_reply(void)
{
    dwm_ipc_header_t header;
    if (read(sock_fd, &header, sizeof(header)) != sizeof(header)) return -1;

    if (memcmp(header.magic, IPC_MAGIC, IPC_MAGIC_LEN) != 0) return -1;

    if (header.size > 0)
    {
        char* buf = malloc(header.size + 1);
        if (!buf) return -1;
        ssize_t read_bytes = 0;
        while ((uint32_t)read_bytes < header.size)
        {
            ssize_t n = read(sock_fd, buf + read_bytes, header.size - read_bytes);
            if (n <= 0) break;
            read_bytes += n;
        }
        buf[header.size] = '\0';
        puts(buf);
        free(buf);
    }
    return 0;
}

int
main(int argc, char* argv[])
{
    const char* sock_path = DEFAULT_SOCKET_PATH;
    if (connect_to_socket(sock_path) < 0)
    {
        fprintf(stderr, "Error: Could not connect to dwm socket at %s\n", sock_path);
        return EXIT_FAILURE;
    }

    if (argc < 2)
    {
        fprintf(stderr, "Usage: dwm-msg <get_monitors|get_tags|get_layouts|run_command ...>\n");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    if (!strcmp(argv[1], "get_monitors"))
    {
        send_ipc_message(IPC_TYPE_GET_MONITORS, 0, NULL);
        read_ipc_reply();
    }
    else if (!strcmp(argv[1], "get_tags"))
    {
        send_ipc_message(IPC_TYPE_GET_TAGS, 0, NULL);
        read_ipc_reply();
    }
    else if (!strcmp(argv[1], "get_layouts"))
    {
        send_ipc_message(IPC_TYPE_GET_LAYOUTS, 0, NULL);
        read_ipc_reply();
    }
    else
    {
        fprintf(stderr, "Unknown or unsupported command argument: %s\n", argv[1]);
    }

    close(sock_fd);
    return EXIT_SUCCESS;
}
