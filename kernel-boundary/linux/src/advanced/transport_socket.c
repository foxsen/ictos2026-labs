#include "transport.h"
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct { int pair[2]; int fd; } socket_state_t;
static int transfer(int fd, void *buffer, size_t bytes, bool sending)
{
    size_t offset = 0;
    while (offset < bytes) {
        ssize_t n = sending ? send(fd, (char *)buffer + offset, bytes - offset, MSG_NOSIGNAL)
                            : recv(fd, (char *)buffer + offset, bytes - offset, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { if (!n) errno = EPIPE; return -1; }
        offset += (size_t)n;
    }
    return 0;
}
static int initialize(ipc_transport_t *t)
{
    socket_state_t *s = malloc(sizeof(*s));
    if (!s) return -1;
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, s->pair)) { free(s); return -1; }
    s->fd = -1;
    t->state = s;
    return 0;
}
static int select_end(ipc_transport_t *t, bool server)
{
    socket_state_t *s = t->state;
    s->fd = s->pair[server ? 1 : 0];
    close(s->pair[server ? 0 : 1]);
    return 0;
}
static int request(ipc_transport_t *t, const void *req, void *rep, size_t bytes)
{
    socket_state_t *s = t->state;
    if (transfer(s->fd, (void *)req, bytes, true)) return -1;
    return transfer(s->fd, rep, bytes, false);
}
static int receive(ipc_transport_t *t, void *req, size_t bytes)
{ return transfer(((socket_state_t *)t->state)->fd, req, bytes, false); }
static int reply(ipc_transport_t *t, const void *rep, size_t bytes)
{ return transfer(((socket_state_t *)t->state)->fd, (void *)rep, bytes, true); }
static void destroy(ipc_transport_t *t)
{
    socket_state_t *s = t->state;
    if (!s) return;
    if (s->fd >= 0) close(s->fd);
    else { close(s->pair[0]); close(s->pair[1]); }
    free(s);
    t->state = NULL;
}
const ipc_ops_t socket_ops = {"socket", initialize, select_end, request, receive, reply, destroy};
