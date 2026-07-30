/* See LICENSE file for copyright and license details. */
#include "IPCClient.h"

IPCClient*
ipc_client_new(int fd)
{
    IPCClient* c = calloc(1, sizeof(IPCClient));
    if (!c)
        return NULL;
    c->fd = fd;
    c->subscriptions = 0;
    c->buffer = NULL;
    c->buffer_size = 0;
    c->next = NULL;
    c->prev = NULL;
    return c;
}

void
ipc_client_free(IPCClient* c)
{
    if (!c)
        return;
    if (c->buffer)
        free(c->buffer);
    free(c);
}

void
ipc_list_add_client(IPCClientList* list, IPCClient* nc)
{
    if (!*list)
    {
        *list = nc;
        return;
    }
    nc->next = *list;
    (*list)->prev = nc;
    *list = nc;
}

void
ipc_list_remove_client(IPCClientList* list, IPCClient* c)
{
    if (!c || !*list)
        return;

    if (*list == c)
        *list = c->next;
    if (c->next)
        c->next->prev = c->prev;
    if (c->prev)
        c->prev->next = c->next;

    c->next = NULL;
    c->prev = NULL;
}

IPCClient*
ipc_list_get_client(IPCClientList list, int fd)
{
    for (IPCClient* c = list; c; c = c->next)
    {
        if (c->fd == fd)
            return c;
    }
    return NULL;
}
