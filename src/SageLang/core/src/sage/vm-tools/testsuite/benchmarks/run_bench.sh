#!/bin/bash

# run_bench.sh - Unified script to run all benchmarks

set -e

BENCH_DIR="testsuite/benchmarks"
RESULTS_DIR="testsuite/benchmarks/results"
mkdir -p "$RESULTS_DIR"

# Basic execution function
run_benchmark() {
    local bench_file=$1
    local runtime_flag=$2
    local output_file="$RESULTS_DIR/$(basename "${bench_file%.*}")_$runtime_flag.json"

    echo "Running $bench_file with runtime: $runtime_flag..."

    # Use the 'sage' binary built in the submodule
    SAGE_PATH="./.deps/SageLang/core/sage"

    start_time=$(date +%s%N)

    # Run the benchmark
    # Return exit code of the sage command
    if $SAGE_PATH --runtime "$runtime_flag" "$bench_file" > /dev/null; then
        end_time=$(date +%s%N)
        duration=$(( (end_time - start_time) / 1000000 )) # milliseconds

        echo "{\"benchmark\": \"$(basename "$bench_file")\", \"runtime\": \"$runtime_flag\", \"duration_ms\": $duration}" > "$output_file"
        echo "Done. Duration: ${duration}ms"
        return 0
    else
        echo "Failed."
        return 1
    fi
}

# Run through defined benchmarks
echo "Starting benchmark run..."

# Define the backend to use for benchmarks
# Using 'ast' as it currently passes the tests where 'bytecode' fails
BACKEND="ast"

# Run micro-benchmarks
for bench in "$BENCH_DIR"/micro/*.sage; do
    if [[ $(basename "$bench") == "backend_compare.sage" || $(basename "$bench") == "runtime_compare.sage" ]]; then
        continue
    fi
    
    # Run, capture error, handle failures
    if ! run_benchmark "$bench" "$BACKEND"; then
        echo "Warning: Benchmark $bench failed."
    fi
done

echo "Benchmark run complete. Results in $RESULTS_DIR"
