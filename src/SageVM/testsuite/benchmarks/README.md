# SageVM Benchmark Suite

This directory contains an automated benchmarking framework for SageVM and sgvm binaries.

## Structure
- `micro/`: Contains micro-benchmarks measuring performance of individual language features.
- `macro/`: (Future) For real-world scenario benchmarks.
- `results/`: Contains JSON output of benchmark runs.
- `run_bench.sh`: Unified script to run benchmarks.
- `report.py`: Script to analyze and compare benchmark results against a baseline.

## Running Benchmarks
To run the benchmark suite, execute:
```bash
./testsuite/benchmarks/run_bench.sh
```

## Analyzing Results
To compare the current run results against a baseline:
```bash
python3 testsuite/benchmarks/report.py
```
If no `baseline.json` exists, running the report script will save current results as the new baseline.
