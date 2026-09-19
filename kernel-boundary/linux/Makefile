CC ?= cc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Wpedantic
LDFLAGS ?=

BIN_DIR := bin
PROGRAMS := $(BIN_DIR)/bench_syscall $(BIN_DIR)/bench_ipc

.PHONY: all clean

all: $(PROGRAMS)

$(BIN_DIR):
	mkdir -p $@

$(BIN_DIR)/bench_syscall: src/bench_syscall.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/bench_ipc: src/bench_ipc.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(PROGRAMS)
