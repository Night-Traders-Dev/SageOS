# UI Flashing Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate UI flickering in SageShell and SageNano by offloading slow system calls to a background worker and optimizing the rendering logic.

**Architecture:** A background worker script will periodically update a state file with terminal info, time, and temperature. SageShell and SageNano will read this file synchronously instead of spawning slow sub-processes on every keypress. Rendering will be consolidated into single-write operations.

**Tech Stack:** SageLang, Shell (bash)

---

### Task 1: Create the Background UI Worker

**Files:**
- Create: `packages/SageUtils/universal/ui_worker.sage`

- [ ] **Step 1: Write the worker script**

```python
import sys
import io

let STATE_FILE = "/tmp/sage_ui_state"

proc get_temp():
    sys.exec("cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null > /tmp/worker_temp")
    let t = io.readfile("/tmp/worker_temp")
    if t == nil or len(t) == 0: return "N/A"
    let mc = tonumber(t)
    if mc == nil: return "N/A"
    return str((mc / 1000 * 9 / 5 + 32) | 0) + "°F"

proc get_time():
    sys.exec("date +%H:%M:%S > /tmp/worker_time")
    return io.readfile("/tmp/worker_time")

proc get_term_size():
    sys.exec("stty size 2>/dev/null > /tmp/worker_size")
    return io.readfile("/tmp/worker_size")

while true:
    let time = trim(get_time())
    let temp = get_temp()
    let size = trim(get_term_size())
    
    let content = time + "|" + temp + "|" + size
    io.writefile(STATE_FILE, content)
    sys.exec("sleep 1")
```

- [ ] **Step 2: Verify the worker script runs**

Run: `sage packages/SageUtils/universal/ui_worker.sage & sleep 2 && cat /tmp/sage_ui_state && kill $!`
Expected: Output in format `HH:MM:SS|TEMP|ROWS COLS`

- [ ] **Step 3: Commit**

```bash
git add packages/SageUtils/universal/ui_worker.sage
git commit -m "feat: add background UI state worker"
```

### Task 2: Optimize SageShell Rendering

**Files:**
- Modify: `packages/SageShell/universal/main.sage`

- [ ] **Step 1: Implement state file reading and background worker launch**

```python
# In global scope or init
let STATE_FILE = "/tmp/sage_ui_state"
sys.exec("sage " + HOME + "/.sagepkg/packages/SageUtils/universal/ui_worker.sage &")

proc get_cached_state():
    let content = io.readfile(STATE_FILE)
    if content == nil: return ["N/A", "N/A", "24 80"]
    return split(content, "|")
```

- [ ] **Step 2: Update draw_status_bar to use cached state**

```python
proc draw_status_bar():
    let state = get_cached_state()
    let time = state[0]
    let temp = state[1]
    let size_parts = split(state[2], " ")
    let rows = tonumber(size_parts[0])
    let cols = tonumber(size_parts[1])
    
    # ... build bar string using time/temp/cols ...
    # Use a single sys.exec/printf for the whole bar
```

- [ ] **Step 3: Commit**

```bash
git add packages/SageShell/universal/main.sage
git commit -m "perf: optimize SageShell status bar with cached state"
```

### Task 3: Fix SageNano Flashing and Rendering

**Files:**
- Modify: `packages/SageNano/universal/ui.sage`
- Modify: `packages/SageNano/universal/editor.sage`

- [ ] **Step 1: Fix get_term_size in ui.sage**

```python
proc get_term_size():
    let content = io.readfile("/tmp/sage_ui_state")
    if content != nil:
        let parts = split(content, "|")
        if len(parts) >= 3:
            let size_parts = split(parts[2], " ")
            return [tonumber(size_parts[0]), tonumber(size_parts[1])]
    return [24, 80]
```

- [ ] **Step 2: Optimize Editor.draw in editor.sage to use single output write**

```python
proc draw():
    # ... build 'out' string ...
    io.writefile("/tmp/sage_nano_draw", out)
    sys.exec("cat /tmp/sage_nano_draw") # Single flush
```

- [ ] **Step 3: Commit**

```bash
git add packages/SageNano/universal/ui.sage packages/SageNano/universal/editor.sage
git commit -m "fix: resolve SageNano flashing and use cached UI state"
```
