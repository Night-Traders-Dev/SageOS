#!/usr/bin/env python3
import argparse
import datetime
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path

try:
    import matplotlib.pyplot as plt
    HAS_MATPLOTLIB = True
except ImportError:
    plt = None
    HAS_MATPLOTLIB = False
from rich.align import Align
from rich.console import Console
from rich.panel import Panel
from rich.progress import Progress, SpinnerColumn, TextColumn, TimeElapsedColumn
from rich.table import Table
from rich.text import Text

ROOT = Path(__file__).resolve().parent
BUILD_DIR = ROOT / "build"
HISTORY_FILE = ROOT / "benchmark_history.json"
CHART_FILE = ROOT / "benchmark_chart.png"

ARCH_TARGETS = [
    ("x64", "virt", BUILD_DIR / "virt_x86_64"),
    ("arm64", "virt", BUILD_DIR / "virt_aarch64"),
    ("rv64", "virt", BUILD_DIR / "virt_riscv64"),
]

console = Console()


def human_bytes(size: int) -> str:
    units = ["B", "KiB", "MiB", "GiB", "TiB"]
    value = float(size)
    for unit in units:
        if value < 1024.0 or unit == units[-1]:
            return f"{value:,.2f} {unit}"
        value /= 1024.0
    return f"{size} B"


def get_directory_stats(path: Path) -> dict:
    stats = {
        "total_size": 0,
        "file_count": 0,
        "object_count": 0,
        "object_size": 0,
        "files": [],
    }
    if not path.exists():
        return stats

    for blob in sorted(path.rglob("*")):
        if not blob.is_file():
            continue
        size = blob.stat().st_size
        stats["total_size"] += size
        stats["file_count"] += 1
        if blob.suffix == ".o":
            stats["object_count"] += 1
            stats["object_size"] += size
        stats["files"].append((str(blob.relative_to(path)), size))
    return stats


def save_results_json(history_file: Path, run_data: dict) -> None:
    history = []
    if history_file.exists():
        try:
            with history_file.open("r", encoding="utf-8") as f:
                history = json.load(f)
        except json.JSONDecodeError:
            history = []

    history.append(run_data)
    with history_file.open("w", encoding="utf-8") as f:
        json.dump(history, f, indent=2)


def generate_chart(history_file: Path, chart_file: Path) -> bool:
    if not HAS_MATPLOTLIB:
        console.print(Panel(Text("Matplotlib is not installed. Skipping chart generation.", style="bold yellow"), border_style="yellow"))
        return False

    if not history_file.exists():
        return False
    with history_file.open("r", encoding="utf-8") as f:
        history = json.load(f)
    if not history:
        return False

    timestamps = [datetime.datetime.fromisoformat(run["timestamp"]) for run in history]
    archs = sorted({row["arch"] for run in history for row in run["results"]})

    fig, axes = plt.subplots(2, 1, figsize=(12, 9), constrained_layout=True)
    fig.suptitle("SageOS Virt Build Benchmark History", fontsize=18, fontweight="bold")

    for arch in archs:
        durations = []
        for run in history:
            row = next((r for r in run["results"] if r["arch"] == arch), None)
            durations.append(row["duration_seconds"] if row else None)
        axes[0].plot(timestamps, durations, marker="o", label=f"{arch} build time")

    axes[0].set_ylabel("Build Time (seconds)", fontsize=12)
    axes[0].set_xlabel("Run Timestamp", fontsize=12)
    axes[0].grid(True, linestyle="--", alpha=0.4)
    axes[0].legend()

    for arch in archs:
        kernel_sizes = []
        dir_sizes = []
        for run in history:
            row = next((r for r in run["results"] if r["arch"] == arch), None)
            kernel_sizes.append(row["kernel_size"] / 1024 if row else None)
            dir_sizes.append(row["dir_size"] / 1024 if row else None)
        axes[1].plot(timestamps, kernel_sizes, marker="s", linestyle="-", label=f"{arch} kernel size (KiB)")
        axes[1].plot(timestamps, dir_sizes, marker="^", linestyle="--", label=f"{arch} build size (KiB)")

    axes[1].set_ylabel("Size (KiB)", fontsize=12)
    axes[1].set_xlabel("Run Timestamp", fontsize=12)
    axes[1].grid(True, linestyle="--", alpha=0.4)
    axes[1].legend(loc="upper left", fontsize=9)

    fig.autofmt_xdate(rotation=25)
    fig.savefig(chart_file, dpi=200)
    return True


def run_build(arch: str, device: str, clean: bool, target_dir: Path) -> dict:
    if clean and target_dir.exists():
        shutil.rmtree(target_dir)

    command = ["./sageos.sh", arch, device, "build"]
    console.print(Panel.fit(f"[bold white]Benchmarking build[/bold white] [green]{arch}/{device}[/green]", border_style="blue"))

    with Progress(
        SpinnerColumn(style="green"),
        TextColumn("[progress.description]{task.description}"),
        TimeElapsedColumn(),
        console=console,
        transient=True,
    ) as progress:
        task = progress.add_task("Preparing build…", start=False)
        progress.start_task(task)
        time.sleep(0.1)
        progress.update(task, description="Running build command...")
        start = time.perf_counter()
        try:
            result = subprocess.run(
                command,
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=True,
            )
        except subprocess.CalledProcessError as exc:
            console.print(Panel(Text(f"Build failed for {arch}/{device}\n\n{exc.stdout}\n{exc.stderr}", style="bold red"), title="Build Error", border_style="red"))
            raise
        elapsed = time.perf_counter() - start
        progress.update(task, description="Gathering artifact stats...")
        time.sleep(0.1)

    stats = get_directory_stats(target_dir)
    kernel_path = target_dir / "kernel.elf"
    kernel_size = kernel_path.stat().st_size if kernel_path.exists() else 0
    header = f"Build completed in {elapsed:.2f}s"
    console.print(Panel(Text(header, style="bold green"), subtitle=str(target_dir), subtitle_align="right", border_style="green"))

    return {
        "arch": arch,
        "device": device,
        "target_dir": str(target_dir),
        "duration_seconds": elapsed,
        "kernel_size": kernel_size,
        "dir_size": stats["total_size"],
        "file_count": stats["file_count"],
        "object_count": stats["object_count"],
        "object_size": stats["object_size"],
        "files": stats["files"],
    }


def build_summary(results: list[dict]) -> None:
    summary = Table(title="SageOS Virt Build Benchmark", expand=True, show_lines=True)
    summary.add_column("Arch", style="bold cyan")
    summary.add_column("Build Target")
    summary.add_column("Duration", justify="right")
    summary.add_column("Kernel Image", justify="right")
    summary.add_column("Build Dir Size", justify="right")
    summary.add_column("Files", justify="right")
    summary.add_column("Obj Files", justify="right")
    summary.add_column("Obj Size", justify="right")

    for row in results:
        summary.add_row(
            row["arch"],
            row["device"],
            f"{row['duration_seconds']:.2f}s",
            human_bytes(row["kernel_size"]),
            human_bytes(row["dir_size"]),
            str(row["file_count"]),
            str(row["object_count"]),
            human_bytes(row["object_size"]),
        )

    console.print(summary)

    for row in results:
        detail = Table(title=f"{row['arch']} Artifact Breakdown", show_header=True, header_style="bold magenta")
        detail.add_column("File", style="white")
        detail.add_column("Size", justify="right")
        files = sorted(row["files"], key=lambda item: item[1], reverse=True)
        top_files = files[:10]
        for path, size in top_files:
            detail.add_row(str(path), human_bytes(size))
        if len(files) > len(top_files):
            detail.add_row("...", "")
        console.print(detail)


def parse_args():
    parser = argparse.ArgumentParser(description="Benchmark SageOS virt builds and report image/component sizes.")
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Remove previous virt build artifacts before benchmarking.",
    )
    return parser.parse_args()


def main() -> int:
    if sys.version_info < (3, 9):
        console.print("[red]Python 3.9+ is required.[/red]")
        return 1

    args = parse_args()
    results = []

    console.print(Panel(Align.center(Text("SageOS Virt Build Benchmark", style="bold white"), vertical="middle"), border_style="bright_blue"))
    for arch, device, target_dir in ARCH_TARGETS:
        result = run_build(arch, device, clean=args.clean, target_dir=target_dir)
        results.append(result)

    build_summary(results)

    run_data = {
        "timestamp": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "results": results,
    }
    save_results_json(HISTORY_FILE, run_data)
    chart_generated = generate_chart(HISTORY_FILE, CHART_FILE)
    if chart_generated:
        console.print(Panel(Text(f"Saved benchmark history to {HISTORY_FILE.name}\nChart generated at {CHART_FILE.name}", style="bold green"), border_style="cyan"))
    else:
        console.print(Panel(Text(f"Saved benchmark history to {HISTORY_FILE.name}\nChart generation skipped.", style="bold yellow"), border_style="yellow"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
