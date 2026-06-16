#!/bin/bash

# run_compare.sh - Benchmark comparison: sage script vs. compiled sgvm binary

set -e

BENCH_FILE="testsuite/benchmarks/micro/02_loop_sum.sage"
SAGE_BIN="./.deps/SageLang/core/sage"
SGVM_COMPILER="sgvmc.sage"
SGVM_BIN="./.deps/SageLang/core/sgvm"
SGVM_FILE="02_loop_sum.sgvm"

# Clean up
rm -f "$SGVM_FILE"

echo "Compiling benchmark for sgvm..."
$SAGE_BIN -I src "$SGVM_COMPILER" "$BENCH_FILE" "$SGVM_FILE"

echo "--- Performance Comparison ---"

# Measure Sage script execution (using AST backend for stability)
echo "Running sage script (ast)..."
time $SAGE_BIN --runtime ast "$BENCH_FILE" > /dev/null

# Measure SGVM execution (if supported/stable)
echo "Running sgvm binary..."
if [ -x "$SGVM_BIN" ]; then
    time $SGVM_BIN "$SGVM_FILE" > /dev/null
else
    echo "SGVM binary not found or not executable."
fi
