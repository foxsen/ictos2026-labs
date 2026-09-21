CC ?= cc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Wpedantic
LDFLAGS ?=

BIN_DIR := bin
PROGRAMS := $(BIN_DIR)/bench_syscall $(BIN_DIR)/bench_ipc
ADVANCED := $(BIN_DIR)/bench_ipc_compare
ADVANCED_SRC := src/advanced/bench_compare.c src/advanced/transport_socket.c src/advanced/transport_shm.c

.PHONY: all advanced test clean

all: $(PROGRAMS)

advanced: $(ADVANCED)

$(ADVANCED): $(ADVANCED_SRC) src/advanced/transport.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(ADVANCED_SRC) -o $@ $(LDFLAGS)

test:
	python3 -m unittest discover -s ../tests -p 'test_advanced.py' -v

$(BIN_DIR):
	mkdir -p $@

$(BIN_DIR)/bench_syscall: src/bench_syscall.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/bench_ipc: src/bench_ipc.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(PROGRAMS) $(ADVANCED)
