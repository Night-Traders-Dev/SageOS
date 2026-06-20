#!/usr/bin/env python3
from __future__ import annotations

import os
import sys
import shutil
import argparse
import threading
import multiprocessing
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from rich.console import Console, Group
from rich.live import Live
from rich.table import Table
from rich.text import Text
from rich.panel import Panel
from rich.progress import Progress, SpinnerColumn, TextColumn, BarColumn, TaskID

# Initialize rich console for beautiful text handling
console = Console()

# Define the targets relative to SageOS root
TARGETS = [
    ".",  # SageOS root itself
    "SageLang",
    "SageVM",
    "SageBoot",
    os.path.join("arch", "arm64"),
    os.path.join("arch", "rv64"),
    os.path.join("arch", "x64"),
]

# Live detail panel state: repo_name -> most recent output line (e.g. which
# nested submodule of SageLang is currently being cloned/checked out).
# Guarded by a lock since multiple worker threads write to it concurrently.
_status_lock = threading.Lock()
_status_lines: dict[str, str] = {}


def set_status(repo_name: str, line: str) -> None:
    with _status_lock:
        _status_lines[repo_name] = line


def clear_status(repo_name: str) -> None:
    with _status_lock:
        _status_lines.pop(repo_name, None)


def render_status_panel() -> Table:
    """Builds the 'what's actually happening right now' table shown below
    the main progress bars. Rebuilt fresh on every Live refresh tick."""
    table = Table(box=None, show_header=False, padding=(0, 1, 0, 0))
    table.add_column("repo", style="cyan", no_wrap=True, width=12)
    table.add_column("detail", style="dim", no_wrap=True, overflow="ellipsis")
    with _status_lock:
        items = sorted(_status_lines.items())
    if not items:
        table.add_row("", "[dim italic]waiting for git output...[/dim italic]")
    for name, line in items:
        # Wrap raw git output in Text() rather than passing a plain string:
        # git messages can legitimately contain '[' ']' (branch names, ref
        # updates, etc.) which Rich would otherwise try to parse as markup.
        table.add_row(name, Text(line))
    return table


class LiveActivityPanel:
    """Wraps render_status_panel() so the outer Live re-renders it fresh on
    every refresh tick, instead of freezing a snapshot taken once at the
    moment this object was composed into the display."""

    def __rich_console__(self, console, options):
        yield Panel(
            render_status_panel(),
            title="Live Git Activity",
            border_style="grey50",
            padding=(0, 1),
        )


def stream_subprocess(command: list[str], cwd: str, on_line) -> tuple[int, str]:
    """Runs `command`, streaming its combined stdout/stderr as it arrives.

    Git's progress meters (clone/fetch percentages, "Submodule path 'X':
    checked out 'Y'") use '\\r' to overwrite a line in place rather than
    always terminating with '\\n'. Reading with readline()/iteration would
    block waiting for a '\\n' that may never come until the whole command
    finishes, which defeats the point of live progress. So we read one
    character at a time and treat both '\\r' and '\\n' as segment breaks.

    `on_line(line)` is called for every non-empty segment as it streams in.
    Returns (returncode, full_combined_output) for error reporting.
    """
    process = subprocess.Popen(
        command,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    chunks: list[str] = []
    buf = ""
    assert process.stdout is not None
    while True:
        ch = process.stdout.read(1)
        if ch == "":
            break  # EOF: process closed stdout
        if ch in ("\r", "\n"):
            line = buf.strip()
            if line:
                on_line(line)
                chunks.append(line)
            buf = ""
        else:
            buf += ch

    line = buf.strip()
    if line:
        on_line(line)
        chunks.append(line)

    returncode = process.wait()
    return returncode, "\n".join(chunks)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="SageOS Parallel Sync Engine")
    parser.add_argument(
        "--bootstrap", "--init",
        dest="bootstrap",
        action="store_true",
        help=(
            "Wipe any existing submodule directories and re-fetch them fresh "
            "via 'git submodule update --init --remote' before syncing. "
            "Required on a fresh clone, where submodule dirs aren't yet "
            "independent git repos and the normal sync steps fail."
        ),
    )
    return parser.parse_args()


def verify_root_directory() -> str:
    """Ensures script runs inside SageOS directory and returns absolute path."""
    current_dir = os.getcwd()
    folder_name = os.path.basename(current_dir)

    if folder_name != "SageOS":
        console.print(f"[bold red]❌ Error:[/bold red] Not in the SageOS directory. Current directory is: [yellow]{folder_name}[/yellow]", err=True)
        sys.exit(1)

    return current_dir


def bootstrap_target(root_path: str, target: str) -> tuple[bool, str]:
    """Wipes an existing submodule directory (if present) and re-fetches it
    fresh via `git submodule update --init --remote`, run from the
    superproject root. Returns (success, target)."""
    full_path = os.path.abspath(os.path.join(root_path, target))

    if os.path.isdir(full_path):
        console.print(f"[magenta]🧹 {target}[/magenta]: removing existing directory...")
        try:
            shutil.rmtree(full_path)
        except OSError as e:
            console.print(f"[bold red]❌ {target} failed to remove existing directory:[/bold red] {e}")
            return False, target

    console.print(f"[magenta]⬇️  {target}[/magenta]: fetching fresh...")
    try:
        subprocess.run(
            ["git", "submodule", "update", "--init", "--remote", "--", target],
            cwd=root_path,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True,
        )
    except subprocess.CalledProcessError as e:
        console.print(f"[bold red]❌ {target} failed to bootstrap:[/bold red]\n{e.stderr.strip()}")
        return False, target

    console.print(f"[bold green]✅ {target}[/bold green] bootstrapped")
    return True, target


def run_bootstrap_phase(root_path: str) -> list[str]:
    """Runs the wipe+refetch step for every submodule target.

    This is intentionally SEQUENTIAL, not threaded. Each call mutates the
    superproject's own .git/config and .git/modules; running several of
    these concurrently from different threads risks colliding on git's
    lock files (index.lock / config.lock) since they all write to the
    same root .git directory. The per-submodule steps in the normal sync
    phase are safe to parallelize because by then each submodule has its
    own independent .git, so threads no longer touch shared state.

    Returns the list of targets that failed to bootstrap, so they can be
    excluded from the subsequent sync phase.
    """
    console.print("[bold blue]🧱 Bootstrap mode:[/bold blue] wiping and re-fetching submodules fresh\n")
    failed = []
    for target in TARGETS:
        if target == ".":
            continue  # never wipe the superproject itself
        success, name = bootstrap_target(root_path, target)
        if not success:
            failed.append(name)
    console.print("")
    return failed


def run_git_sync(repo_path: str, repo_name: str, progress: Progress, task_id: TaskID, *, skip_global_submodule_update: bool = False):
    """Executes sequential git steps for a single repository, updating its
    progress bar and streaming live detail (e.g. which nested submodule of
    SageLang is currently being cloned/checked out) into the status panel.

    skip_global_submodule_update: set True only for the SageOS root task.
    Root's "git submodule update --init --remote" (no path arg) checks out
    *every* submodule's working tree, which is the exact same directory
    each dedicated submodule thread is concurrently operating on itself.
    Running both at once races on that submodule's .git (lock collisions).
    Since every submodule already gets its own thread handling this for
    itself, root doesn't need to repeat it globally.
    """
    steps = [("Syncing Submodules", ["git", "submodule", "sync"])]
    if not skip_global_submodule_update:
        # --progress forces git to emit clone/checkout progress even though
        # stdout/stderr here is a pipe, not a tty (git suppresses progress
        # output by default when not attached to a terminal). Without it,
        # SageLang's nested submodules would only become visible *after*
        # each one finished, defeating the point of a live activity panel.
        steps.append(("Updating Submodules", ["git", "submodule", "update", "--init", "--remote", "--progress"]))
    steps.append(("Pulling Changes", ["git", "pull", "origin", "main", "--progress"]))

    # 3 major git commands per repository
    progress.update(task_id, total=len(steps))

    for description, command in steps:
        progress.update(task_id, description=f"[cyan]{repo_name}[/cyan]: {description}...")
        set_status(repo_name, f"starting: {description}")

        returncode, output = stream_subprocess(
            command, repo_path, on_line=lambda line: set_status(repo_name, line)
        )

        if returncode != 0:
            progress.update(
                task_id,
                description=f"[bold red]❌ {repo_name} failed during '{description}'[/bold red]"
            )
            set_status(repo_name, f"failed during {description}")
            # Print the failed command details clearly out of the progress workflow
            console.print(f"\n[bold red]Failure in {repo_name}:[/bold red]\n{output.strip()}")
            return False, repo_name

        progress.advance(task_id, advance=1)

    clear_status(repo_name)
    progress.update(task_id, description=f"[bold green]✅ {repo_name} Complete[/bold green]")
    return True, repo_name


def main():
    args = parse_args()
    root_path = verify_root_directory()

    bootstrap_failures: list[str] = []
    if args.bootstrap:
        bootstrap_failures = run_bootstrap_phase(root_path)

    # Calculate CPU Cores for maximum parallelization
    cpu_cores = multiprocessing.cpu_count()
    console.print(f"[bold blue]🚀 Starting SageOS Parallel Sync Engine[/bold blue] (Jobs: {cpu_cores} threads)\n")

    # Set up rich tracking bars. Constructed directly (not as its own context
    # manager) because it's composed into our own outer Live below alongside
    # the activity panel — Progress only needs to own its own Live when used
    # standalone via `with Progress(...) as progress:`.
    progress = Progress(
        SpinnerColumn(),
        TextColumn("{task.description}"),
        BarColumn(bar_width=40),
        TextColumn("[progress.percentage]{task.percentage:>3.0f}%"),
        console=console,
    )
    display = Group(progress, LiveActivityPanel())

    with Live(display, console=console, refresh_per_second=10, transient=False) as live:

        futures = []
        # Leverage thread pool up to CPU core count
        with ThreadPoolExecutor(max_workers=cpu_cores) as executor:
            for target in TARGETS:
                if target in bootstrap_failures:
                    console.print(f"[bold yellow]⚠️ Skipping {target} after failed bootstrap[/bold yellow]")
                    continue

                full_path = os.path.abspath(os.path.join(root_path, target))
                display_name = "SageOS Root" if target == "." else target

                if os.path.isdir(full_path):
                    # Spawn a unique visual task bar for each repository tracking live state
                    task_id = progress.add_task(description=f"[yellow]Queued {display_name}...[/yellow]", total=None)
                    futures.append(executor.submit(
                        run_git_sync, full_path, display_name, progress, task_id,
                        skip_global_submodule_update=(target == ".")
                    ))
                else:
                    # Non-blocking log if sub-directory structure isn't populated yet
                    console.print(f"[bold yellow]⚠️ Skipping missing directory:[/bold yellow] {target}")

            # Keep execution block open until all parallel tasks finish
            results = [future.result() for future in as_completed(futures)]

        # Make sure the final frame reflects the last status/progress update,
        # rather than whatever the last 100ms auto-refresh tick happened to catch.
        live.refresh()

    # Quick final summary health check
    failures = [name for success, name in results if not success] + bootstrap_failures
    if failures:
        console.print(f"\n[bold red]❌ Finished with errors.[/bold red] Failed repositories: {', '.join(failures)}")
        sys.exit(1)
    else:
        console.print("\n[bold green]🎉 All repositories and submodules synced cleanly simultaneously![/bold green]")


if __name__ == "__main__":
    main()
