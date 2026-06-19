#!/usr/bin/env python3
"""
codestats.py — Recursive source line counter with animated Rich UI and scan history.

Scans a directory tree for source files (default extensions: .sage .svm .c .h .s .S),
skipping any directory trees you point it away from (handy for nested/circular git
submodules), and prints a structured, color-coded report. Every run is appended to a
JSON history file so you can track how the codebase grows over time.

Usage:
    python3 codestats.py [ROOT] [options]

Examples:
    python3 codestats.py .
    python3 codestats.py ~/projects/SageVM -x build -x "*/SageTree/*" -x .cache
    python3 codestats.py . --top 15 --label "pre-GC-migration"
    python3 codestats.py . --no-save --json-export latest.json
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import os
import sys
import time
from collections import deque
from dataclasses import dataclass, asdict, field
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from rich.console import Console, Group
from rich.table import Table
from rich.panel import Panel
from rich.progress import (
    Progress,
    SpinnerColumn,
    BarColumn,
    TextColumn,
    TimeElapsedColumn,
    MofNCompleteColumn,
)
from rich.live import Live
from rich.text import Text
from rich.rule import Rule
from rich.align import Align
from rich import box


# --------------------------------------------------------------------------- #
# Configuration
# --------------------------------------------------------------------------- #

DEFAULT_EXTENSIONS = {".sage", ".svm", ".c", ".h", ".s", ".S"}
DEFAULT_HISTORY_FILENAME = ".codestats_history.json"
# Directory names we always skip descending into, regardless of user excludes.
BUILTIN_SKIP_DIRS = {".git", ".hg", ".svn", ".jj"}

THEME = {
    "primary": "bold cyan",
    "accent": "bold magenta",
    "good": "bold green",
    "bad": "bold red",
    "warn": "bold yellow",
    "dim": "dim white",
}


# --------------------------------------------------------------------------- #
# Data model
# --------------------------------------------------------------------------- #

@dataclass
class FileStat:
    path: str
    ext: str
    total_lines: int
    blank_lines: int
    code_lines: int
    size_bytes: int


@dataclass
class ScanRecord:
    timestamp: str
    root: str
    label: Optional[str]
    duration_s: float
    files_scanned: int
    files_failed: int
    total_lines: int
    blank_lines: int
    code_lines: int
    total_bytes: int
    by_ext: Dict[str, Dict[str, int]]
    excluded_user: List[str]
    excluded_builtin_hits: int


# --------------------------------------------------------------------------- #
# Exclusion matching
# --------------------------------------------------------------------------- #

def is_excluded(dirname: str, rel_path: str, patterns: List[str]) -> bool:
    """True if a directory should be pruned entirely from the walk."""
    rel_norm = rel_path.replace(os.sep, "/")
    for pat in patterns:
        pat_norm = pat.strip("/")
        if dirname == pat_norm:
            return True
        if rel_norm == pat_norm:
            return True
        if fnmatch.fnmatch(rel_norm, pat_norm):
            return True
        if fnmatch.fnmatch(dirname, pat_norm):
            return True
    return False


# --------------------------------------------------------------------------- #
# Phase 1 — Discovery (animated, verbose)
# --------------------------------------------------------------------------- #

def discover_files(
    root: Path,
    extensions: set,
    user_excludes: List[str],
    console: Console,
) -> Tuple[List[str], List[str], int]:
    """
    Walk the tree, pruning excluded directories before descending into them.
    Returns (matched_file_paths, excluded_dir_relpaths, builtin_skip_count).
    """
    matched: List[str] = []
    excluded_hits: List[str] = []
    builtin_hits = 0
    dirs_visited = 0
    log_lines: deque = deque(maxlen=10)

    header = Panel(
        Align.center(Text(f"Discovering source files under {root}", style=THEME["primary"])),
        box=box.ROUNDED,
        border_style="cyan",
    )

    def render():
        body = Table.grid(padding=(0, 1))
        body.add_row(Text(f"directories visited: {dirs_visited}", style=THEME["dim"]),
                     Text(f"files matched: {len(matched)}", style=THEME["good"]),
                     Text(f"trees excluded: {len(excluded_hits)}", style=THEME["warn"]))
        log_table = Table.grid(padding=(0, 1))
        for line in log_lines:
            log_table.add_row(line)
        log_panel = Panel(log_table or Text("…", style=THEME["dim"]),
                           title="activity", border_style="grey50", box=box.SQUARE)
        return Group(header, body, log_panel)

    with Live(render(), console=console, refresh_per_second=12, transient=True) as live:
        for dirpath, dirnames, filenames in os.walk(str(root), topdown=True):
            dirs_visited += 1
            rel_dir = os.path.relpath(dirpath, root)
            rel_dir = "" if rel_dir == "." else rel_dir

            kept = []
            for d in sorted(dirnames):
                if d in BUILTIN_SKIP_DIRS:
                    builtin_hits += 1
                    continue
                rel_child = os.path.join(rel_dir, d) if rel_dir else d
                if is_excluded(d, rel_child, user_excludes):
                    excluded_hits.append(rel_child)
                    log_lines.append(Text(f"⊘ excluded tree:  {rel_child}/", style=THEME["bad"]))
                    continue
                kept.append(d)
            dirnames[:] = kept

            local_hits = 0
            for f in filenames:
                ext = Path(f).suffix
                if ext in extensions:
                    matched.append(os.path.join(dirpath, f))
                    local_hits += 1
            if local_hits:
                shown = rel_dir if rel_dir else "."
                log_lines.append(Text(f"✓ {shown}/  (+{local_hits} file{'s' if local_hits != 1 else ''})",
                                       style=THEME["accent"]))

            if dirs_visited % 5 == 0 or local_hits or rel_dir == "":
                live.update(render())

        live.update(render())

    return matched, excluded_hits, builtin_hits


# --------------------------------------------------------------------------- #
# Phase 2 — Counting (animated, verbose)
# --------------------------------------------------------------------------- #

def count_file(path: str) -> Optional[FileStat]:
    try:
        p = Path(path)
        size = p.stat().st_size
        total = 0
        blank = 0
        with open(path, "r", encoding="utf-8", errors="replace", newline="") as fh:
            for line in fh:
                total += 1
                if line.strip() == "":
                    blank += 1
        return FileStat(
            path=path, ext=p.suffix, total_lines=total,
            blank_lines=blank, code_lines=total - blank, size_bytes=size,
        )
    except (OSError, UnicodeDecodeError):
        return None


def count_with_ui(root: Path, files: List[str], console: Console) -> Tuple[List[FileStat], List[str]]:
    results: List[FileStat] = []
    errors: List[str] = []
    recent: deque = deque(maxlen=8)

    progress = Progress(
        SpinnerColumn(style="cyan"),
        TextColumn("[bold cyan]counting[/]"),
        BarColumn(bar_width=None),
        MofNCompleteColumn(),
        TextColumn("•"),
        TimeElapsedColumn(),
        TextColumn("•"),
        TextColumn("{task.fields[rate]:.0f} files/s", justify="right"),
        console=console,
        expand=True,
    )
    task_id = progress.add_task("count", total=len(files), rate=0.0)

    header = Panel(
        Align.center(Text(f"Counting lines — {len(files)} files queued", style=THEME["primary"])),
        box=box.ROUNDED,
        border_style="cyan",
    )

    def render():
        recent_table = Table.grid(padding=(0, 1))
        recent_table.add_column(ratio=3)
        recent_table.add_column(ratio=1, justify="right")
        for stat in recent:
            shown = os.path.relpath(stat.path, root)
            recent_table.add_row(Text(shown, style="white", overflow="ellipsis", no_wrap=True),
                                  Text(f"{stat.total_lines:>6} ln", style=THEME["good"]))
        recent_panel = Panel(recent_table or Text("…", style=THEME["dim"]),
                              title="recently scanned", border_style="grey50", box=box.SQUARE)
        return Group(header, progress, recent_panel)

    start = time.monotonic()
    with Live(render(), console=console, refresh_per_second=12, transient=True) as live:
        for i, fpath in enumerate(files, 1):
            stat = count_file(fpath)
            if stat is None:
                errors.append(fpath)
            else:
                results.append(stat)
                recent.append(stat)
            elapsed = max(time.monotonic() - start, 1e-6)
            progress.update(task_id, advance=1, rate=i / elapsed)
            if i % 3 == 0 or i == len(files):
                live.update(render())

    return results, errors


# --------------------------------------------------------------------------- #
# Aggregation
# --------------------------------------------------------------------------- #

def aggregate_by_ext(results: List[FileStat]) -> Dict[str, Dict[str, int]]:
    agg: Dict[str, Dict[str, int]] = {}
    for r in results:
        bucket = agg.setdefault(r.ext, {"files": 0, "total_lines": 0, "blank_lines": 0,
                                         "code_lines": 0, "bytes": 0})
        bucket["files"] += 1
        bucket["total_lines"] += r.total_lines
        bucket["blank_lines"] += r.blank_lines
        bucket["code_lines"] += r.code_lines
        bucket["bytes"] += r.size_bytes
    return agg


def human_bytes(n: int) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024:
            return f"{n:.0f}{unit}" if unit == "B" else f"{n:.1f}{unit}"
        n /= 1024
    return f"{n:.1f}TB"


# --------------------------------------------------------------------------- #
# Report rendering
# --------------------------------------------------------------------------- #

def render_ext_table(by_ext: Dict[str, Dict[str, int]], total_lines: int) -> Table:
    table = Table(title="By Extension", box=box.SIMPLE_HEAVY, show_lines=False,
                   title_style=THEME["primary"], header_style="bold")
    table.add_column("ext", style="bold cyan")
    table.add_column("files", justify="right")
    table.add_column("total lines", justify="right")
    table.add_column("code", justify="right", style=THEME["good"])
    table.add_column("blank", justify="right", style=THEME["dim"])
    table.add_column("size", justify="right")
    table.add_column("share", justify="right")

    files_sum = lines_sum = code_sum = blank_sum = bytes_sum = 0
    for ext in sorted(by_ext, key=lambda e: -by_ext[e]["total_lines"]):
        b = by_ext[ext]
        share = (b["total_lines"] / total_lines * 100) if total_lines else 0
        table.add_row(ext, str(b["files"]), f"{b['total_lines']:,}", f"{b['code_lines']:,}",
                       f"{b['blank_lines']:,}", human_bytes(b["bytes"]), f"{share:5.1f}%")
        files_sum += b["files"]; lines_sum += b["total_lines"]
        code_sum += b["code_lines"]; blank_sum += b["blank_lines"]; bytes_sum += b["bytes"]

    table.add_section()
    table.add_row("[bold]total[/]", f"[bold]{files_sum}[/]", f"[bold]{lines_sum:,}[/]",
                   f"[bold]{code_sum:,}[/]", f"[bold]{blank_sum:,}[/]",
                   f"[bold]{human_bytes(bytes_sum)}[/]", "[bold]100.0%[/]")
    return table


def render_top_files(results: List[FileStat], root: Path, top_n: int) -> Optional[Table]:
    if top_n <= 0 or not results:
        return None
    table = Table(title=f"Largest Files (top {top_n})", box=box.SIMPLE,
                   title_style=THEME["primary"], header_style="bold")
    table.add_column("#", justify="right", style=THEME["dim"])
    table.add_column("file")
    table.add_column("lines", justify="right", style=THEME["good"])
    table.add_column("ext", justify="center", style="cyan")
    ranked = sorted(results, key=lambda r: -r.total_lines)[:top_n]
    for i, r in enumerate(ranked, 1):
        table.add_row(str(i), os.path.relpath(r.path, root), f"{r.total_lines:,}", r.ext)
    return table


def render_meta_panel(record: ScanRecord) -> Panel:
    lines = [
        f"[bold]root:[/]              {record.root}",
        f"[bold]duration:[/]          {record.duration_s:.2f}s",
        f"[bold]files scanned:[/]     {record.files_scanned}"
        + (f"  [red]({record.files_failed} unreadable)[/]" if record.files_failed else ""),
        f"[bold]excluded trees:[/]    {len(record.excluded_user)}"
        + (f"  ({', '.join(record.excluded_user[:4])}{'…' if len(record.excluded_user) > 4 else ''})"
           if record.excluded_user else ""),
        f"[bold]builtin skipped:[/]   {record.excluded_builtin_hits} dirs (.git/.hg/.svn/.jj)",
        f"[bold]total size:[/]        {human_bytes(record.total_bytes)}",
    ]
    if record.label:
        lines.insert(0, f"[bold]label:[/]             {record.label}")
    return Panel("\n".join(lines), title="Scan Metadata", border_style="grey50", box=box.ROUNDED)


def render_delta_panel(prev: Optional[dict], current: ScanRecord) -> Panel:
    if prev is None:
        body = Text("No prior scan on record — this is the baseline.", style=THEME["dim"])
        return Panel(body, title="Change Since Last Scan", border_style="grey50", box=box.ROUNDED)

    d_lines = current.total_lines - prev["total_lines"]
    d_files = current.files_scanned - prev["files_scanned"]
    d_code = current.code_lines - prev["code_lines"]

    def fmt(n: int) -> Text:
        if n > 0:
            return Text(f"▲ +{n:,}", style=THEME["good"])
        if n < 0:
            return Text(f"▼ {n:,}", style=THEME["bad"])
        return Text("● 0", style=THEME["dim"])

    grid = Table.grid(padding=(0, 2))
    grid.add_column(); grid.add_column()
    grid.add_row("total lines:", fmt(d_lines))
    grid.add_row("code lines:", fmt(d_code))
    grid.add_row("files:", fmt(d_files))
    grid.add_row("since:", Text(prev["timestamp"], style=THEME["dim"]))
    return Panel(grid, title="Change Since Last Scan", border_style="grey50", box=box.ROUNDED)


def render_history_trend(console: Console, history: List[dict], limit: int = 10) -> None:
    if not history:
        return
    table = Table(title="Scan History", box=box.SIMPLE, title_style=THEME["primary"],
                   header_style="bold")
    table.add_column("when")
    table.add_column("label", style=THEME["dim"])
    table.add_column("files", justify="right")
    table.add_column("total lines", justify="right")
    table.add_column("Δ lines", justify="right")

    recent_history = history[-limit:]
    prev_lines = history[-limit - 1]["total_lines"] if len(history) > limit else None
    for i, entry in enumerate(recent_history):
        ts = entry["timestamp"].replace("T", " ").split(".")[0]
        delta = entry["total_lines"] - prev_lines if prev_lines is not None else None
        if delta is None:
            delta_txt = Text("—", style=THEME["dim"])
        elif delta > 0:
            delta_txt = Text(f"+{delta:,}", style=THEME["good"])
        elif delta < 0:
            delta_txt = Text(f"{delta:,}", style=THEME["bad"])
        else:
            delta_txt = Text("0", style=THEME["dim"])
        table.add_row(ts, entry.get("label") or "", str(entry["files_scanned"]),
                       f"{entry['total_lines']:,}", delta_txt)
        prev_lines = entry["total_lines"]
    console.print(table)


# --------------------------------------------------------------------------- #
# History persistence
# --------------------------------------------------------------------------- #

def load_history(path: Path) -> List[dict]:
    if not path.exists():
        return []
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except (json.JSONDecodeError, OSError):
        return []


def save_history(path: Path, history: List[dict]) -> None:
    with open(path, "w", encoding="utf-8") as f:
        json.dump(history, f, indent=2)


# --------------------------------------------------------------------------- #
# Main
# --------------------------------------------------------------------------- #

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="codestats.py",
        description="Recursive source line counter with animated Rich UI and scan history.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "examples:\n"
            "  codestats.py .\n"
            "  codestats.py ~/proj/SageVM -x build -x \"*/SageTree/*\" -x .cache\n"
            "  codestats.py . --top 15 --label pre-GC-migration\n"
        ),
    )
    p.add_argument("root", nargs="?", default=".", help="root directory to scan (default: .)")
    p.add_argument("-x", "--exclude", action="append", default=[], metavar="PATTERN",
                    help="directory name or glob to prune entirely from the walk; repeatable. "
                         "Matches the bare directory name or its path relative to root "
                         "(e.g. -x vendor -x \"third_party/*\" -x submodules/old)")
    p.add_argument("--ext", action="append", default=None, metavar=".EXT",
                    help="override default extension set; repeatable "
                         "(default: .sage .svm .c .h .s .S)")
    p.add_argument("--history-file", default=None, metavar="PATH",
                    help="path to history JSON (default: <root>/.codestats_history.json)")
    p.add_argument("--no-save", action="store_true", help="don't record this run in history")
    p.add_argument("--no-history", action="store_true", help="don't print the history trend table")
    p.add_argument("--top", type=int, default=10, metavar="N",
                    help="show top N largest files by line count (0 disables, default: 10)")
    p.add_argument("--json-export", default=None, metavar="PATH",
                    help="also write this scan's result as a standalone JSON file")
    p.add_argument("--label", default=None, help="optional note attached to this scan in history")
    p.add_argument("--quiet", action="store_true",
                    help="skip the animated scan UI, print only the final report")
    return p


def main() -> None:
    args = build_parser().parse_args()
    console = Console()

    root = Path(args.root).expanduser().resolve()
    if not root.exists() or not root.is_dir():
        console.print(f"[bold red]error:[/] root path does not exist or is not a directory: {root}")
        sys.exit(1)

    extensions = set(args.ext) if args.ext else set(DEFAULT_EXTENSIONS)
    history_path = Path(args.history_file).expanduser().resolve() if args.history_file \
        else root / DEFAULT_HISTORY_FILENAME

    console.print(Rule(Text("SOURCE SCANNER", style="bold white on dark_cyan"), style="cyan"))
    console.print(Align.center(Text(f"{root}", style=THEME["dim"])))
    console.print(Align.center(Text(f"extensions: {' '.join(sorted(extensions))}", style=THEME["dim"])))
    console.print()

    t0 = time.monotonic()

    if args.quiet:
        files, excluded_dirs, builtin_hits = [], [], 0
        for dirpath, dirnames, filenames in os.walk(str(root), topdown=True):
            rel_dir = os.path.relpath(dirpath, root)
            rel_dir = "" if rel_dir == "." else rel_dir
            kept = []
            for d in dirnames:
                if d in BUILTIN_SKIP_DIRS:
                    builtin_hits += 1
                    continue
                rel_child = os.path.join(rel_dir, d) if rel_dir else d
                if is_excluded(d, rel_child, args.exclude):
                    excluded_dirs.append(rel_child)
                    continue
                kept.append(d)
            dirnames[:] = kept
            for f in filenames:
                if Path(f).suffix in extensions:
                    files.append(os.path.join(dirpath, f))
        results, errors = [], []
        for fpath in files:
            stat = count_file(fpath)
            (results if stat else errors).append(stat or fpath)
    else:
        files, excluded_dirs, builtin_hits = discover_files(root, extensions, args.exclude, console)
        if not files:
            console.print("[yellow]No matching files found.[/]")
            sys.exit(0)
        results, errors = count_with_ui(root, files, console)

    duration = time.monotonic() - t0
    by_ext = aggregate_by_ext(results)
    total_lines = sum(r.total_lines for r in results)
    blank_lines = sum(r.blank_lines for r in results)
    code_lines = sum(r.code_lines for r in results)
    total_bytes = sum(r.size_bytes for r in results)

    record = ScanRecord(
        timestamp=datetime.now().isoformat(timespec="seconds"),
        root=str(root),
        label=args.label,
        duration_s=duration,
        files_scanned=len(results),
        files_failed=len(errors),
        total_lines=total_lines,
        blank_lines=blank_lines,
        code_lines=code_lines,
        total_bytes=total_bytes,
        by_ext=by_ext,
        excluded_user=excluded_dirs,
        excluded_builtin_hits=builtin_hits,
    )

    # ---- report ---- #
    console.print(Rule("Scan Report", style="cyan"))
    console.print(render_ext_table(by_ext, total_lines))
    console.print()

    top_table = render_top_files(results, root, args.top)
    if top_table:
        console.print(top_table)
        console.print()

    history = load_history(history_path)
    console.print(render_meta_panel(record))
    console.print(render_delta_panel(history[-1] if history else None, record))

    if errors:
        console.print(Panel(
            "\n".join(os.path.relpath(e, root) if isinstance(e, str) else e for e in errors[:10])
            + (f"\n… and {len(errors) - 10} more" if len(errors) > 10 else ""),
            title=f"[bold red]{len(errors)} file(s) could not be read[/]",
            border_style="red", box=box.ROUNDED,
        ))

    if not args.no_save:
        history.append(asdict(record))
        save_history(history_path, history)
        console.print(f"[dim]scan recorded → {history_path}[/]")

    if not args.no_history:
        console.print()
        render_history_trend(console, history if not args.no_save else history + [asdict(record)])

    if args.json_export:
        with open(args.json_export, "w", encoding="utf-8") as f:
            json.dump(asdict(record), f, indent=2)
        console.print(f"[dim]exported → {args.json_export}[/]")

    console.print(Rule(style="cyan"))


if __name__ == "__main__":
    main()
