#!/usr/bin/env python3
from __future__ import annotations

import os
import sys
import shutil
import argparse
import multiprocessing
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from rich.console import Console
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
    """Executes sequential git steps for a single repository, updating its progress bar.

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
        steps.append(("Updating Submodules", ["git", "submodule", "update", "--init", "--remote"]))
    steps.append(("Pulling Changes", ["git", "pull", "origin", "main"]))

    # 3 major git commands per repository
    progress.update(task_id, total=len(steps))

    for description, command in steps:
        progress.update(task_id, description=f"[cyan]{repo_name}[/cyan]: {description}...")
        try:
            # Run command; capture output to prevent terminal intermixing
            result = subprocess.run(
                command,
                cwd=repo_path,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=True
            )
            progress.advance(task_id, advance=1)
        except subprocess.CalledProcessError as e:
            progress.update(
                task_id,
                description=f"[bold red]❌ {repo_name} failed during '{description}'[/bold red]"
            )
            # Print the failed command details clearly out of the progress workflow
            console.print(f"\n[bold red]Failure in {repo_name}:[/bold red]\n{e.stderr.strip()}")
            return False, repo_name

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

    # Set up rich tracking bars
    with Progress(
        SpinnerColumn(),
        TextColumn("{task.description}"),
        BarColumn(bar_width=40),
        TextColumn("[progress.percentage]{task.percentage:>3.0f}%"),
        transient=False
    ) as progress:

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

    # Quick final summary health check
    failures = [name for success, name in results if not success] + bootstrap_failures
    if failures:
        console.print(f"\n[bold red]❌ Finished with errors.[/bold red] Failed repositories: {', '.join(failures)}")
        sys.exit(1)
    else:
        console.print("\n[bold green]🎉 All repositories and submodules synced cleanly simultaneously![/bold green]")


if __name__ == "__main__":
    main()
