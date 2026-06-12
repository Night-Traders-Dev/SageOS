import json
import os
import sys
import matplotlib.pyplot as plt
import numpy as np

RESULTS_DIR = "testsuite/benchmarks/results"

def load_results():
    results = {}
    if not os.path.exists(RESULTS_DIR):
        return results
    for filename in os.listdir(RESULTS_DIR):
        if filename.endswith(".json"):
            with open(os.path.join(RESULTS_DIR, filename), 'r') as f:
                data = json.load(f)
                key = f"{data['benchmark']}_{data['runtime']}"
                results[key] = data['duration_ms']
    return results

def generate_chart(baseline, current, output_file="testsuite/benchmarks/results/benchmark_comparison.png"):
    benchmarks = sorted(list(set(baseline.keys()) & set(current.keys())))
    if not benchmarks:
        print("No common benchmarks to plot.")
        return

    base_vals = [baseline[b] for b in benchmarks]
    curr_vals = [current[b] for b in benchmarks]

    x = np.arange(len(benchmarks))
    width = 0.35

    fig, ax = plt.subplots(figsize=(12, 6))
    rects1 = ax.bar(x - width/2, base_vals, width, label='Baseline', color='skyblue')
    rects2 = ax.bar(x + width/2, curr_vals, width, label='Current', color='orange')

    ax.set_ylabel('Duration (ms)')
    ax.set_title('Benchmark Comparison: Baseline vs Current')
    ax.set_xticks(x)
    ax.set_xticklabels(benchmarks, rotation=45, ha='right')
    ax.legend()

    plt.tight_layout()
    plt.savefig(output_file)
    print(f"\nChart saved to {output_file}")

def compare(baseline_file="baseline.json"):
    current = load_results()
    if not os.path.exists(baseline_file):
        print(f"No baseline found at {baseline_file}. Saving current results as baseline.")
        with open(baseline_file, 'w') as f:
            json.dump(current, f, indent=4)
        return

    with open(baseline_file, 'r') as f:
        baseline = json.load(f)

    print(f"{'Benchmark':<30} | {'Baseline (ms)':<15} | {'Current (ms)':<15} | {'Diff (%)':<10}")
    print("-" * 80)
    for key, current_val in current.items():
        if key in baseline:
            base_val = baseline[key]
            diff = ((current_val - base_val) / base_val) * 100
            print(f"{key:<30} | {base_val:<15} | {current_val:<15} | {diff:>+9.2f}%")
        else:
            print(f"{key:<30} | {'N/A':<15} | {current_val:<15} | {'N/A':<10}")
    
    generate_chart(baseline, current)

if __name__ == "__main__":
    compare()
