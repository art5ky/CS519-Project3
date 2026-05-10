#!/bin/bash
# CS519 Project 3 - Full experiment harness
# Runs all conditions with proper cleanup and process discipline.

set -u

# Configuration
LOG=results.csv
TRIALS=10
MATRIX_SIZE=10000
WARMUP_SEC=5
SETTLE_SEC=3
DEFAULT_SPINNERS=32

# Spinner thread counts for the scalability sweep (set to empty to skip)
SPINNER_COUNTS=(8 16 32 64)

# ----- Helpers -----

cleanup() {
    pkill -9 mt-benchmark 2>/dev/null
    pkill -9 ipc-shmem 2>/dev/null
    sleep 1
}

verify_clean() {
    if pgrep -x mt-benchmark > /dev/null || pgrep -x ipc-shmem > /dev/null; then
        echo "  WARNING: stale processes detected, force-killing"
        cleanup
        sleep 1
    fi
}

run_matrix() {
    # Outputs "runtime_s,csw" or "FAIL,FAIL" on parse failure
    local out
    out=$(./ipc-shmem "$MATRIX_SIZE" 2>&1)
    local rt csw
    rt=$(echo "$out" | grep "Total runtime:" | awk '{print $3}')
    csw=$(echo "$out" | grep "Involuntary context switches:" | awk '{print $4}')
    if [ -z "$rt" ] || [ -z "$csw" ]; then
        echo "FAIL,FAIL"
    else
        echo "$rt,$csw"
    fi
}

run_with_spinner() {
    # Args: mode (normal|coop|yield), num_threads
    local mode=$1
    local threads=$2
    ./mt-benchmark "$mode" "$threads" > /tmp/spinner.log 2>&1 &
    local spin_pid=$!
    sleep "$WARMUP_SEC"

    local result
    result=$(run_matrix)

    kill -INT "$spin_pid" 2>/dev/null
    wait "$spin_pid" 2>/dev/null
    sleep "$SETTLE_SEC"
    verify_clean

    echo "$result"
}

# ----- Main -----

echo "condition,spinners,trial,runtime_s,csw" > "$LOG"

# Initial cleanup
cleanup

# 1. Standalone (no spinner)
echo "=== Standalone (no spinner) ==="
for i in $(seq 1 $TRIALS); do
    verify_clean
    out=$(run_matrix)
    echo "standalone,0,$i,$out" | tee -a "$LOG"
    sleep "$SETTLE_SEC"
done

# 2. Headline conditions: 32 spinner threads, all 3 modes
for mode in normal coop yield; do
    echo "=== $mode spinner ($DEFAULT_SPINNERS threads) ==="
    for i in $(seq 1 $TRIALS); do
        verify_clean
        out=$(run_with_spinner "$mode" "$DEFAULT_SPINNERS")
        echo "$mode,$DEFAULT_SPINNERS,$i,$out" | tee -a "$LOG"
    done
done

# 3. Scalability sweep: vary spinner thread count for normal vs coop
if [ "${#SPINNER_COUNTS[@]}" -gt 0 ]; then
    for threads in "${SPINNER_COUNTS[@]}"; do
        # Skip 32 since it's already covered above
        if [ "$threads" -eq "$DEFAULT_SPINNERS" ]; then
            continue
        fi
        for mode in normal coop; do
            echo "=== $mode spinner ($threads threads) ==="
            for i in $(seq 1 $TRIALS); do
                verify_clean
                out=$(run_with_spinner "$mode" "$threads")
                echo "$mode,$threads,$i,$out" | tee -a "$LOG"
            done
        done
    done
fi

# Final cleanup
cleanup

echo ""
echo "Done. Results saved to $LOG"
echo "Total trials: $(tail -n +2 $LOG | wc -l)"