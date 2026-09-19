#define _GNU_SOURCE

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    unsigned samples;
    unsigned iterations;
    int cpu;
} options_t;

static volatile long result_sink;

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
    options_t options = {.samples = 101, .iterations = 10000, .cpu = -1};

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

__attribute__((noinline)) static long ordinary_function(long value)
{
    __asm__ volatile("" : "+r"(value) : : "memory");
    return value + 1;
}

typedef enum {
    BENCH_EMPTY,
    BENCH_FUNCTION,
    BENCH_GETPID_RAW,
    BENCH_CLOCK_VDSO,
    BENCH_CLOCK_RAW,
} benchmark_kind_t;

typedef struct {
    const char *name;
    benchmark_kind_t kind;
} benchmark_t;

static long run_batch(benchmark_kind_t kind, unsigned iterations)
{
    long value = 0;
    struct timespec time = {0};

    for (unsigned i = 0; i < iterations; ++i) {
        switch (kind) {
        case BENCH_EMPTY:
            __asm__ volatile("" : "+r"(value) : : "memory");
            break;
        case BENCH_FUNCTION:
            value = ordinary_function(value);
            break;
        case BENCH_GETPID_RAW:
            value = syscall(SYS_getpid);
            break;
        case BENCH_CLOCK_VDSO:
            value = clock_gettime(CLOCK_MONOTONIC, &time);
            break;
        case BENCH_CLOCK_RAW:
            value = syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &time);
            break;
        }
    }

    result_sink = value + time.tv_nsec;
    return value;
}

int main(int argc, char **argv)
{
    const benchmark_t benchmarks[] = {
        {"empty_loop", BENCH_EMPTY},
        {"ordinary_function", BENCH_FUNCTION},
        {"raw_getpid", BENCH_GETPID_RAW},
        {"clock_gettime_vdso_candidate", BENCH_CLOCK_VDSO},
        {"raw_clock_gettime", BENCH_CLOCK_RAW},
    };
    const options_t options = parse_options(argc, argv);
    pin_to_cpu(options.cpu);

    fprintf(stderr,
            "bench_syscall: cpu=%d samples=%u iterations=%u pid=%ld\n",
            options.cpu,
            options.samples,
            options.iterations,
            (long)getpid());
    puts("platform,benchmark,message_bytes,sample,iterations,total_ns,ns_per_op");

    for (size_t benchmark = 0; benchmark < sizeof(benchmarks) / sizeof(benchmarks[0]); ++benchmark) {
        run_batch(benchmarks[benchmark].kind, options.iterations / 10 + 1);
        for (unsigned sample = 0; sample < options.samples; ++sample) {
            uint64_t start = now_ns();
            run_batch(benchmarks[benchmark].kind, options.iterations);
            uint64_t elapsed = now_ns() - start;
            printf("linux,%s,0,%u,%u,%" PRIu64 ",%.3f\n",
                   benchmarks[benchmark].name,
                   sample,
                   options.iterations,
                   elapsed,
                   (double)elapsed / (double)options.iterations);
        }
    }

    return result_sink == LONG_MIN ? EXIT_FAILURE : EXIT_SUCCESS;
}
