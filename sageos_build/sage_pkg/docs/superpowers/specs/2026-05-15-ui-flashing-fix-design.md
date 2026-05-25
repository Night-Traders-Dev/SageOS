# Design Doc: UI Flashing Fix for SageShell and SageNano

## 1. Problem Statement
Users report that the UI "flashes" or flickers on every character typed in both `SageShell` and `SageNano`.

### Root Causes
1. **Inefficient Redraws:** Both applications redraw complex UI elements (like status bars) on every keypress.
2. **Synchronous I/O:** Every redraw involves multiple `sys.exec` calls to external programs (`stty`, `date`, `cat`) and temporary files. These are slow and block the main UI loop.
3. **Bug in SageNano:** A specific bug in `get_term_size` prints a shell command string directly to stdout instead of executing it silently.
4. **Redundant Updates:** The clock in the status bar forces a full redraw every second, even when the user is idle.

## 2. Proposed Solution

### Phase 1: Background Data Fetching (Async/Parallel)
We will offload slow system calls to a background process.
- **Background Worker:** A separate Sage script (or a shell background process) will run in parallel.
- **Shared State:** The worker will fetch terminal size, system time, and temperature, writing them to a temporary "state file" (e.g., `/tmp/sage_ui_state.json`).
- **Non-Blocking Read:** The main applications will read this file instead of spawning new processes.

### Phase 2: Caching and Throttling
- **Cache Terminal Size:** Terminal dimensions will be cached and only refreshed periodically or upon a `SIGWINCH`-like event (if supported) or a timeout.
- **Time Comparison:** Redraws of the status bar will only occur if the displayed data (like the current second) has actually changed.

### Phase 3: Rendering Optimization
- **Buffer Consolidation:** UI components will be built as a single string and sent to the terminal in one `write` or `print` call to minimize screen "tearing."
- **Cursor Management:** Ensure the cursor is hidden during large redraws and restored immediately after.
- **Fix SageNano Bug:** Correct the `get_term_size` function to use proper execution instead of `print`.

## 3. Implementation Details

### SageShell
- Modify `sage_readline` loop to check the state file.
- Launch `sage_ui_worker.sage` on shell startup.
- Update `draw_status_bar` to be more surgical.

### SageNano
- Apply similar state file reading logic.
- Fix the `ui.sage` bug where it prints the `stty` command.
- Optimize the `Editor.draw` method in `editor.sage`.

### SageLang / sys.exec
- Investigate if `sys.exec` needs enhancement in the core language to better support capturing output without temporary files.

## 4. Success Criteria
- Zero visible flickering when typing at normal speeds.
- Status bar clock updates smoothly without affecting input responsiveness.
- SageNano no longer prints command strings to the editor buffer.
