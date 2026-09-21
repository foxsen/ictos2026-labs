#define _GNU_SOURCE
#include "transport.h"
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    const ipc_ops_t *ops;
    bool check;
    unsigned samples, iterations, warmup, bytes, seed;
    int client_cpu, server_cpu;
} options_t;
static void usage(const char *p)
{
    fprintf(stderr, "usage: %s --transport socket|shm --mode check|bench "
            "--client-cpu N --server-cpu N [--message-bytes 1..4096] "
            "[--samples N] [--iterations N] [--warmup N] [--seed N]\n", p);
}
static unsigned number(const char *s)
{
    char *end; errno = 0;
    unsigned long n = strtoul(s, &end, 10);
    if (errno || *end || !*s || *s == '-' || n > UINT_MAX) {
        fprintf(stderr, "invalid number: %s\n", s); exit(2);
    }
    return (unsigned)n;
}
static options_t parse(int argc, char **argv)
{
    options_t o = {.ops=&socket_ops, .check=true, .samples=1, .iterations=5000,
                   .warmup=50, .bytes=64, .seed=1, .client_cpu=-1, .server_cpu=-1};
    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i], "--help")) { usage(argv[0]); exit(0); }
        if (i+1 == argc) { usage(argv[0]); exit(2); }
        const char *key=argv[i], *value=argv[++i];
        if (!strcmp(key,"--transport")) {
            if (!strcmp(value,"socket")) o.ops=&socket_ops;
            else if (!strcmp(value,"shm")) o.ops=&shm_ops;
            else { usage(argv[0]); exit(2); }
        } else if (!strcmp(key,"--mode")) {
            if (strcmp(value,"check") && strcmp(value,"bench")) { usage(argv[0]); exit(2); }
            o.check=!strcmp(value,"check");
        } else if (!strcmp(key,"--samples")) o.samples=number(value);
        else if (!strcmp(key,"--iterations")) o.iterations=number(value);
        else if (!strcmp(key,"--warmup")) o.warmup=number(value);
        else if (!strcmp(key,"--message-bytes")) o.bytes=number(value);
        else if (!strcmp(key,"--seed")) o.seed=number(value);
        else if (!strcmp(key,"--client-cpu") || !strcmp(key,"--server-cpu")) {
            unsigned cpu=number(value);
            if (cpu >= CPU_SETSIZE) { fputs("CPU id exceeds CPU_SETSIZE\n",stderr); exit(2); }
            if (!strcmp(key,"--client-cpu")) o.client_cpu=(int)cpu; else o.server_cpu=(int)cpu;
        } else { usage(argv[0]); exit(2); }
    }
    if (!o.samples || !o.iterations || !o.bytes || o.bytes>IPC_MAX_PAYLOAD ||
        o.client_cpu<0 || o.server_cpu<0) { usage(argv[0]); exit(2); }
    return o;
}
static int pin(int cpu)
{
    cpu_set_t allowed, target;
    if (sched_getaffinity(0,sizeof(allowed),&allowed)) return -1;
    if (!CPU_ISSET(cpu,&allowed)) { errno=EINVAL; return -1; }
    CPU_ZERO(&target); CPU_SET(cpu,&target);
    return sched_setaffinity(0,sizeof(target),&target);
}
static uint64_t now_ns(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC_RAW,&t)) { perror("clock"); exit(1); }
    return (uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;
}
static unsigned char payload_byte(uint64_t seq, unsigned seed, size_t i)
{
    uint64_t x=seq ^ ((uint64_t)seed<<32) ^ ((uint64_t)i*UINT64_C(0x9e3779b97f4a7c15));
    x ^= x>>30; x *= UINT64_C(0xbf58476d1ce4e5b9); x ^= x>>27;
    return (unsigned char)(x ^ (x>>32));
}
static void fill(unsigned char *packet, uint64_t seq, const options_t *o)
{
    memcpy(packet,&seq,IPC_SEQUENCE_BYTES);
    for (size_t i=0;i<o->bytes;i++) packet[IPC_SEQUENCE_BYTES+i]=payload_byte(seq,o->seed,i);
}
static int validate(const unsigned char *packet, uint64_t seq, const options_t *o)
{
    uint64_t actual; memcpy(&actual,packet,IPC_SEQUENCE_BYTES);
    if (actual!=seq) {
        fprintf(stderr,"sequence mismatch: expected=%"PRIu64" actual=%"PRIu64"\n",seq,actual);
        errno=EPROTO; return -1;
    }
    uint64_t payload_seq=o->check ? seq : 0;
    for (size_t i=0;i<o->bytes;i++) {
        if (packet[IPC_SEQUENCE_BYTES+i]!=payload_byte(payload_seq,o->seed,i)) {
            fprintf(stderr,"payload mismatch: sequence=%"PRIu64" offset=%zu\n",seq,i);
            errno=EPROTO; return -1;
        }
    }
    return 0;
}
static int server(ipc_transport_t *t, const options_t *o, uint64_t total, int ready_fd)
{
    if (pin(o->server_cpu) || o->ops->after_fork(t,true)) { perror("server setup"); return 1; }
    unsigned char ready=1;
    if (write(ready_fd,&ready,1)!=1) return 1;
    close(ready_fd);
    unsigned char request[IPC_MAX_PACKET], reply[IPC_MAX_PACKET];
    const size_t packet_bytes=IPC_SEQUENCE_BYTES+o->bytes;
    for (uint64_t seq=1; seq<=total; seq++) {
        if (o->check && (seq+o->seed)%17==0) sched_yield();
        if (o->ops->receive(t,request,packet_bytes)) { perror("server receive"); return 1; }
        if (o->check && validate(request,seq,o)) return 1;
        memcpy(reply, request, packet_bytes);
        if (o->check && (seq+o->seed)%23==0) sched_yield();
        if (o->ops->reply(t,reply,packet_bytes)) { perror("server reply"); return 1; }
    }
    o->ops->destroy(t);
    return 0;
}
static int exchange(ipc_transport_t *t, const options_t *o, unsigned char *request,
                    unsigned char *reply, uint64_t seq)
{
    if (o->check) {
        fill(request,seq,o);
        memset(reply,0xa5,IPC_SEQUENCE_BYTES+o->bytes);
        if ((seq+o->seed)%13==0) sched_yield();
    } else memcpy(request,&seq,IPC_SEQUENCE_BYTES);
    if (o->ops->request(t,request,reply,IPC_SEQUENCE_BYTES+o->bytes)) return -1;
    return o->check ? validate(reply,seq,o) : 0;
}
int main(int argc,char **argv)
{
    options_t o=parse(argc,argv);
    signal(SIGPIPE,SIG_IGN);
    ipc_transport_t t={0};
    if (o.ops->init(&t)) { perror("transport init"); return 1; }
    int ready_pipe[2];
    if (pipe(ready_pipe)) { perror("pipe"); o.ops->destroy(&t); return 1; }
    uint64_t total=(uint64_t)o.samples*o.iterations+o.warmup;
    fprintf(stderr,"transport=%s mode=%s client_cpu=%d server_cpu=%d payload=%u packet=%zu samples=%u iterations=%u warmup=%u seed=%u\n",
            o.ops->name,o.check?"check":"bench",o.client_cpu,o.server_cpu,o.bytes,
            IPC_SEQUENCE_BYTES+o.bytes,o.samples,o.iterations,o.warmup,o.seed);
    pid_t child=fork();
    if (child<0) { perror("fork"); close(ready_pipe[0]); close(ready_pipe[1]); o.ops->destroy(&t); return 1; }
    if (!child) { close(ready_pipe[0]); _exit(server(&t,&o,total,ready_pipe[1])); }
    close(ready_pipe[1]);
    int result=1;
    if (pin(o.client_cpu) || o.ops->after_fork(&t,false)) { perror("client setup"); goto stop; }
    unsigned char ready=0;
    ssize_t n;
    do { n=read(ready_pipe[0],&ready,1); } while(n<0 && errno==EINTR);
    close(ready_pipe[0]); ready_pipe[0]=-1;
    if(n!=1 || ready!=1) { fputs("server did not become ready\n",stderr); goto stop; }
    unsigned char request[IPC_MAX_PACKET], reply[IPC_MAX_PACKET];
    fill(request,0,&o);
    uint64_t seq=0;
    for(unsigned i=0;i<o.warmup;i++)
        if(exchange(&t,&o,request,reply,++seq)) { perror("warmup"); goto stop; }
    if(!o.check) puts("platform,transport,placement,client_cpu,server_cpu,message_bytes,packet_bytes,sample,iterations,total_ns,ns_per_op");
    for(unsigned sample=0;sample<o.samples;sample++) {
        uint64_t start=now_ns();
        for(unsigned i=0;i<o.iterations;i++)
            if(exchange(&t,&o,request,reply,++seq)) { perror("exchange"); goto stop; }
        uint64_t elapsed=now_ns()-start;
        if(!o.check) {
            if(validate(reply,seq,&o)) goto stop;
            printf("linux,%s,%s,%d,%d,%u,%zu,%u,%u,%"PRIu64",%.3f\n",o.ops->name,
                   o.client_cpu==o.server_cpu?"same":"different",o.client_cpu,o.server_cpu,
                   o.bytes,IPC_SEQUENCE_BYTES+o.bytes,sample,o.iterations,elapsed,(double)elapsed/o.iterations);
        }
    }
    int status;
    pid_t waited;
    do { waited=waitpid(child,&status,0); } while(waited<0 && errno==EINTR);
    if(waited!=child || !WIFEXITED(status) || WEXITSTATUS(status)) {
        fputs("server failed\n",stderr); goto stop;
    }
    child=-1;
    if(o.check) printf("CHECK PASS transport=%s payload=%u round_trips=%"PRIu64"\n",o.ops->name,o.bytes,total);
    result=0;
stop:
    if(ready_pipe[0]>=0) close(ready_pipe[0]);
    if(child>0) { kill(child,SIGKILL); while(waitpid(child,NULL,0)<0 && errno==EINTR) {} }
    o.ops->destroy(&t);
    return result;
}
