CC = gcc
CFLAGS = -O3 -march=native -Wall -I./headers
SRC_DIR = sources
RUN_DIR = runnables
BIN_DIR = .
COMMON_SRCS = $(SRC_DIR)/sqmatrix.c $(SRC_DIR)/benchmark.c
LOCKS_SRC   = $(SRC_DIR)/locks.c
TARGET_SHMEM    = ipc-shmem
TARGET_BENCH_MT = mt-benchmark

all: $(TARGET_SHMEM) $(TARGET_BENCH_MT)

$(TARGET_SHMEM): $(RUN_DIR)/ipc-shmem.c $(COMMON_SRCS) $(LOCKS_SRC)
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/$@

$(TARGET_BENCH_MT): $(RUN_DIR)/mt-benchmark.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/$@ -pthread

clean:
	rm -f $(TARGET_SHMEM) $(TARGET_BENCH_MT)

help:
	@echo "Available commands:"
	@echo "  make         - Build ipc_shmem and mt-benchmark programs"
	@echo "  make clean   - Delete ipc_shmem and mt-benchmark programs"
	@echo "  make help    - Show commands to use"