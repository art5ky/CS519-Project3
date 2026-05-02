CC = gcc
CFLAGS = -O3 -march=native -Wall -I./headers
SRC_DIR = sources
RUN_DIR = runnables
BIN_DIR = .
COMMON_SRCS = $(SRC_DIR)/sqmatrix.c $(SRC_DIR)/benchmark.c
LOCKS_SRC   = $(SRC_DIR)/locks.c
TARGET_SHMEM = ipc_shmem

all: $(TARGET_SHMEM)

$(TARGET_SHMEM): $(RUN_DIR)/ipc-shmem.c $(COMMON_SRCS) $(LOCKS_SRC)
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/$@

clean:
	rm -f $(TARGET_SHMEM)

help:
	@echo "Available commands:"
	@echo "  make         - Build ipc_shmem, benchmark_mt program"
	@echo "  make clean   - Delete ipc_shmem, benchmark_mt program"
	@echo "  make help    - Show commands to use"