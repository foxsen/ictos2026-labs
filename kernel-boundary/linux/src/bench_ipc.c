#define _GNU_SOURCE

#include <errno.h>
#include <inttypes.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    unsigned samples;
    unsigned iterations;
    int cpu;
} options_t;

static const size_t message_sizes[] = {1, 64, 1024, 4096};

static void usage(const char *program)
{
    fprintf(stderr,
            "usage: %s [--samples N] [--iterations N] [--cpu N]\n",
            program);
}

static unsigned parse_unsigned(const char *text, const char *name)
{
    char *end = NULL;
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0 || value > UINT32_MAX) {
        fprintf(stderr, "invalid %s: %s\n", name, text);
        exit(EXIT_FAILURE);
    }
    return (unsigned)value;
}

static int parse_cpu(const char *text)
{
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 0 || value > INT32_MAX) {
        fprintf(stderr, "invalid cpu: %s\n", text);
        exit(EXIT_FAILURE);
    }
    return (int)value;
}

static options_t parse_options(int argc, char **argv)
{
    options_t options = {.samples = 51, .iterations = 1000, .cpu = -1};
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--samples") == 0 && i + 1 < argc) {
            options.samples = parse_unsigned(argv[++i], "samples");
        } else if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            options.iterations = parse_unsigned(argv[++i], "iterations");
        } else if (strcmp(argv[i], "--cpu") == 0 && i + 1 < argc) {
            options.cpu = parse_cpu(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            exit(EXIT_SUCCESS);
        } else {
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    return options;
}

static void pin_to_cpu(int cpu)
{
    if (cpu < 0) {
        return;
    }
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET((unsigned)cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        fprintf(stderr, "sched_setaffinity(cpu=%d): %s\n", cpu, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static uint64_t now_ns(void)
{
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &time) != 0) {
        perror("clock_gettime(CLOCK_MONOTONIC_RAW)");
        exit(EXIT_FAILURE);
    }
    return (uint64_t)time.tv_sec * UINT64_C(1000000000) + (uint64_t)time.tv_nsec;
}

static void transfer_exact(int fd, unsigned char *buffer, size_t length, int sending)
{
    size_t offset = 0;
    while (offset < length) {
        ssize_t result = sending
                             ? send(fd, buffer + offset, length - offset, MSG_NOSIGNAL)
                             : recv(fd, buffer + offset, length - offset, 0);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            if (result == 0) {
                fprintf(stderr, "unexpected EOF during IPC transfer\n");
            } else {
                perror(sending ? "send" : "recv");
            }
            exit(EXIT_FAILURE);
        }
        offset += (size_t)result;
    }
}

static void echo_server(int fd, size_t message_size)
{
    unsigned char *buffer = malloc(message_size);
    if (buffer == NULL) {
        perror("malloc");
        _exit(EXIT_FAILURE);
    }

    for (;;) {
        size_t offset = 0;
        while (offset < message_size) {
            ssize_t result = recv(fd, buffer + offset, message_size - offset, 0);
            if (result < 0 && errno == EINTR) {
                continue;
            }
            if (result == 0) {
                free(buffer);
                close(fd);
                _exit(EXIT_SUCCESS);
            }
            if (result < 0) {
                perror("server recv");
                _exit(EXIT_FAILURE);
            }
            offset += (size_t)result;
        }
        transfer_exact(fd, buffer, message_size, 1);
    }
}

static void benchmark_size(const options_t *options, size_t message_size)
{
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
        perror("socketpair");
        exit(EXIT_FAILURE);
    }

    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (child == 0) {
        close(sockets[0]);
        pin_to_cpu(options->cpu);
        echo_server(sockets[1], message_size);
    }

    close(sockets[1]);
    unsigned char *buffer = malloc(message_size);
    if (buffer == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memset(buffer, 0x5a, message_size);

    for (unsigned i = 0; i < 50; ++i) {
        transfer_exact(sockets[0], buffer, message_size, 1);
        transfer_exact(sockets[0], buffer, message_size, 0);
    }

    for (unsigned sample = 0; sample < options->samples; ++sample) {
        uint64_t start = now_ns();
        for (unsigned iteration = 0; iteration < options->iterations; ++iteration) {
            transfer_exact(sockets[0], buffer, message_size, 1);
            transfer_exact(sockets[0], buffer, message_size, 0);
        }
        uint64_t elapsed = now_ns() - start;
        printf("linux,unix_socket_round_trip,%zu,%u,%u,%" PRIu64 ",%.3f\n",
               message_size,
               sample,
               options->iterations,
               elapsed,
               (double)elapsed / (double)options->iterations);
    }

    free(buffer);
    close(sockets[0]);
    int status = 0;
    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        exit(EXIT_FAILURE);
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
        fprintf(stderr, "echo server exited abnormally\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char **argv)
{
    const options_t options = parse_options(argc, argv);
    signal(SIGPIPE, SIG_IGN);
    pin_to_cpu(options.cpu);
    fprintf(stderr,
            "bench_ipc: cpu=%d samples=%u iterations=%u pid=%ld\n",
            options.cpu,
            options.samples,
            options.iterations,
            (long)getpid());
    puts("platform,benchmark,message_bytes,sample,iterations,total_ns,ns_per_op");

    for (size_t i = 0; i < sizeof(message_sizes) / sizeof(message_sizes[0]); ++i) {
        benchmark_size(&options, message_sizes[i]);
    }
    return EXIT_SUCCESS;
}
