#!/usr/bin/env python3
import os
import sys
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

def verify_root_directory() -> str:
    """Ensures script runs inside SageOS directory and returns absolute path."""
    current_dir = os.getcwd()
    folder_name = os.path.basename(current_dir)
    
    if folder_name != "SageOS":
        console.print(f"[bold red]❌ Error:[/bold red] Not in the SageOS directory. Current directory is: [yellow]{folder_name}[/yellow]", err=True)
        sys.exit(1)
        
    return current_dir

def run_git_sync(repo_path: str, repo_name: str, progress: Progress, task_id: TaskID):
    """Executes sequential git steps for a single repository, updating its progress bar."""
    steps = [
        ("Syncing Submodules", ["git", "submodule", "sync"]),
        ("Updating Submodules", ["git", "submodule", "update", "--init", "--remote"]),
        ("Pulling Changes", ["git", "pull", "origin", "main"])
    ]
    
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
    root_path = verify_root_directory()
    
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
                full_path = os.path.abspath(os.path.join(root_path, target))
                display_name = "SageOS Root" if target == "." else target
                
                if os.path.isdir(full_path):
                    # Spawn a unique visual task bar for each repository tracking live state
                    task_id = progress.add_task(description=f"[yellow]Queued {display_name}...[/yellow]", total=None)
                    futures.append(executor.submit(run_git_sync, full_path, display_name, progress, task_id))
                else:
                    # Non-blocking log if sub-directory structure isn't populated yet
                    console.print(f"[bold yellow]⚠️ Skipping missing directory:[/bold yellow] {target}")

            # Keep execution block open until all parallel tasks finish
            results = [future.result() for future in as_completed(futures)]
            
    # Quick final summary health check
    failures = [name for success, name in results if not success]
    if failures:
        console.print(f"\n[bold red]❌ Finished with errors.[/bold red] Failed repositories: {', '.join(failures)}")
        sys.exit(1)
    else:
        console.print("\n[bold green]🎉 All repositories and submodules synced cleanly simultaneously![/bold green]")

if __name__ == "__main__":
    main()
