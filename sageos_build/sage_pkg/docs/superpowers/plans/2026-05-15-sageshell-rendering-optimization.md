# SageShell Rendering Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate flickering and security vulnerabilities in SageShell rendering by replacing shell-based printing with direct stdout writes.

**Architecture:** Use `io.writefile("/dev/stdout", ...)` for all terminal output to avoid the overhead and security risks of `sys.exec("printf ...")` and `sys.exec("cat ... | tr -d '\\n'")`.

**Tech Stack:** SageLang, standard `io` library.

---

### Task 1: Optimize Prompt and Line Rendering

**Files:**
- Modify: `packages/SageShell/universal/main.sage`

- [ ] **Step 1: Reproduce performance issue (Mental/Manual)**
We know that `sys.exec` on every keypress is slow.

- [ ] **Step 2: Implement optimized `print_prompt`**
Replace `sys.exec("cat ...")` with `io.writefile("/dev/stdout", ...)`.

```sage
proc print_prompt():
    let p = GREEN + USER + RESET + " " + CYAN + BOLD + CWD + RESET + " 🌿 "
    io.writefile("/dev/stdout", p)
```

- [ ] **Step 3: Implement optimized `print_line_raw`**
Replace `sys.exec("cat ...")` with `io.writefile("/dev/stdout", ...)`.

```sage
proc print_line_raw(l):
    io.writefile("/dev/stdout", l)
```

- [ ] **Step 4: Verify changes**
Run `sage packages/SageShell/universal/main.sage` and type. Observe reduced flickering.

### Task 2: Secure and Optimize Status Bar

**Files:**
- Modify: `packages/SageShell/universal/main.sage`

- [ ] **Step 1: Identify shell injection vulnerability**
The current `draw_status_bar` uses `sys.exec("printf '" + ... + "'")` which is vulnerable if `CWD` contains single quotes.

- [ ] **Step 2: Implement secure `draw_status_bar`**
Use `io.writefile("/dev/stdout", ...)` instead of `sys.exec`.

```sage
    # Save cursor, move to bottom, print bar, restore cursor
    let output = ESC + "[s" + ESC + "[" + str(rows) + ";1H" + bar + ESC + "[u"
    io.writefile("/dev/stdout", output)
```

- [ ] **Step 3: Verify security and performance**
Run `sage packages/SageShell/universal/main.sage`. Ensure status bar still renders correctly and handles paths with special characters (e.g., `mkdir "it's_a_test" && cd "it's_a_test"`).

### Task 3: Consistency and Cleanup

**Files:**
- Modify: `packages/SageShell/universal/main.sage`

- [ ] **Step 1: Ensure `STATE_FILE` consistency**
Verify `STATE_FILE` matches what's used in `SageUtils/universal/ui_worker.sage`. (Currently it is `/tmp/sage_ui_state` in both, so just a quick check).

- [ ] **Step 2: Remove redundant temporary files**
Remove `io.writefile("/tmp/sage_prompt", p)` and `io.writefile("/tmp/sage_line", l)` calls as they are no longer needed.

- [ ] **Step 3: Final verification**
Run the shell and perform a full smoke test: typing, history, tab completion, status bar updates.
