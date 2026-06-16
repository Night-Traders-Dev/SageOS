#!/bin/bash

# run_sagevm_bench.sh - Benchmark SageVM performance using the unified binary

set -e

SAGEVM="./sagevm"
BENCH_DIR="testsuite/benchmarks"
RESULTS_DIR="testsuite/benchmarks/results"
mkdir -p "$RESULTS_DIR"

# Basic execution function
run_benchmark() {
    local bench_file=$1
    local output_file="$RESULTS_DIR/$(basename "${bench_file%.*}")_sagevm.json"

    echo "Benchmarking $(basename "$bench_file")..."

    local sgvm_file="${bench_file}.sgvm"
    
    # Compile
    $SAGEVM compile "$bench_file" "$sgvm_file" > /dev/null

    # Run and measure
    start_time=$(date +%s%N)
    if $SAGEVM run "$sgvm_file" > /dev/null; then
        end_time=$(date +%s%N)
        duration=$(( (end_time - start_time) / 1000000 )) # milliseconds

        echo "{\"benchmark\": \"$(basename "$bench_file")\", \"runtime\": \"sagevm\", \"duration_ms\": $duration}" > "$output_file"
        echo "Done. Duration: ${duration}ms"
        rm -f "$sgvm_file"
        return 0
    else
        echo "Failed."
        rm -f "$sgvm_file"
        return 1
    fi
}

# Run through defined benchmarks
echo "Starting SageVM benchmark run..."

# Run micro-benchmarks
for bench in "$BENCH_DIR"/micro/*.sage; do
    # Skip large/infinite or currently unsupported benchmarks (like closures)
    bname=$(basename "$bench")
    if [[ "$bname" == "02_loop_sum_large.sage" || "$bname" == "09_recursion_closures.sage" || "$bname" == "backend_compare.sage" ]]; then
        continue
    fi
    # Run, capture error, handle failures
    if ! run_benchmark "$bench"; then
        echo "Warning: Benchmark $bench failed."
    fi
done

echo "Benchmark run complete. Results in $RESULTS_DIR"
