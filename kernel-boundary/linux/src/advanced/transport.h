#ifndef ICTOS_TRANSPORT_H
#define ICTOS_TRANSPORT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IPC_MAX_PAYLOAD 4096u
#define IPC_SEQUENCE_BYTES sizeof(uint64_t)
#define IPC_MAX_PACKET (IPC_SEQUENCE_BYTES + IPC_MAX_PAYLOAD)

/* One outstanding request. Each packet = uint64_t sequence + N payload bytes.
 * init runs before fork; after_fork selects the endpoint in each process.
 * Operations transfer exactly bytes, returning 0 or -1 with errno set.
 * The transport must not fabricate a reply: the server must receive and echo it.
 * All pointers supplied to operations refer to PRIVATE process-local buffers.
 */
typedef struct { void *state; } ipc_transport_t;
typedef struct {
    const char *name;
    int (*init)(ipc_transport_t *);
    int (*after_fork)(ipc_transport_t *, bool server);
    int (*request)(ipc_transport_t *, const void *, void *, size_t);
    int (*receive)(ipc_transport_t *, void *, size_t);
    int (*reply)(ipc_transport_t *, const void *, size_t);
    void (*destroy)(ipc_transport_t *);
} ipc_ops_t;
extern const ipc_ops_t socket_ops;
extern const ipc_ops_t shm_ops;
#endif
