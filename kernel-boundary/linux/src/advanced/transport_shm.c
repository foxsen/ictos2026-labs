#define _GNU_SOURCE
#include "transport.h"
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/mman.h>

/* Student implementation area. The layout is a suggestion, not a protocol.
 * You may change it; keep transport.h and the common harness contract intact.
 * Document who owns each buffer and the meaning of every state transition.
 * For Linux process-shared futexes, use aligned 32-bit words, not *_PRIVATE.
 */
typedef struct {
    _Alignas(64) _Atomic uint32_t request_state;
    _Alignas(64) _Atomic uint32_t reply_state;
    _Alignas(64) unsigned char request[IPC_MAX_PACKET];
    _Alignas(64) unsigned char reply[IPC_MAX_PACKET];
} shm_region_t;

static int initialize(ipc_transport_t *t)
{
    shm_region_t *s = mmap(NULL, sizeof(*s), PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (s == MAP_FAILED) return -1;
    atomic_init(&s->request_state, 0);
    atomic_init(&s->reply_state, 0);
    t->state = s;
    return 0;
}
static int select_end(ipc_transport_t *t, bool server)
{ (void)t; (void)server; return 0; }
static int unfinished(void)
{
    fputs("STUDENT TODO: implement shared-memory data transfer and process-shared notification in transport_shm.c\n", stderr);
    errno = ENOSYS;
    return -1;
}
static int request(ipc_transport_t *t, const void *req, void *rep, size_t bytes)
{
    /* TODO: publish the request, notify the server, wait for the matching reply,
     * copy it to rep, and make the reply buffer reusable without a data race.
     * Design the state machine and memory ordering before filling this in.
     */
    (void)t; (void)req; (void)rep; (void)bytes;
    return unfinished();
}
static int receive(ipc_transport_t *t, void *req, size_t bytes)
{
    /* TODO: wait for a published request and copy exactly bytes to req.
     * Decide when the producer may safely reuse its request buffer.
     */
    (void)t; (void)req; (void)bytes;
    return unfinished();
}
static int reply(ipc_transport_t *t, const void *rep, size_t bytes)
{
    /* TODO: publish the server's reply and notify the client. */
    (void)t; (void)rep; (void)bytes;
    return unfinished();
}
static void destroy(ipc_transport_t *t)
{
    if (t->state) munmap(t->state, sizeof(shm_region_t));
    t->state = NULL;
}
const ipc_ops_t shm_ops = {"shm", initialize, select_end, request, receive, reply, destroy};
