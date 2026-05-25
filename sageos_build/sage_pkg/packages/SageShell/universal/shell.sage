import sys
import io
from ui import *

# These are assumed to be accessible or passed to the procedures
# In the original file they were global or accessible within the same scope.
# We will need to make sure they are accessible.

proc trim(s):
    if s == nil:
        return ""
    let start_idx = 0
    while start_idx < len(s) and (s[start_idx] == " " or s[start_idx] == "\n" or s[start_idx] == "\r" or s[start_idx] == "\t"):
        start_idx = start_idx + 1
    let end_idx = len(s) - 1
    while end_idx >= start_idx and (s[end_idx] == " " or s[end_idx] == "\n" or s[end_idx] == "\r" or s[end_idx] == "\t"):
        end_idx = end_idx - 1
    if end_idx < start_idx:
        return ""
    let result = ""
    for i in range(end_idx - start_idx + 1):
        result = result + s[start_idx + i]
    return result

proc starts_with(s, prefix):
    if len(s) < len(prefix):
        return false
    return s[0:len(prefix)] == prefix

proc str_contains(s, sub):
    return len(split(s, sub)) > 1

proc split_first(s, sep):
    for i in range(len(s)):
        if s[i] == sep:
            return [s[0:i], s[i+1:len(s)]]
    return [s, ""]

proc is_builtin(cmd):
    if cmd == "exit" or cmd == "quit" or cmd == "help" or cmd == "cd" or cmd == "clear" or cmd == "export" or cmd == "env" or cmd == "version" or cmd == "source" or cmd == "reload" or cmd == "debug" or cmd == "history":
        return true
    return false

proc highlight(line, command_exists, ENV_PATH):
    if len(line) == 0:
        return ""
    let result = ""
    let parts = []
    let current = ""
    let in_string = false
    for i in range(len(line)):
        let c = line[i]
        if c == " " and not in_string:
            if len(current) > 0:
                push(parts, current)
            push(parts, " ")
            current = ""
        elif c == chr(34) or c == chr(39):
            in_string = not in_string
            current = current + c
        else:
            current = current + c
    if len(current) > 0:
            push(parts, current)
    let cmd_found = false
    for i in range(len(parts)):
        let p = parts[i]
        if p == " ":
            result = result + " "
            continue
        if not cmd_found:
            cmd_found = true
            if is_builtin(p):
                result = result + ui.MAGENTA + ui.BOLD + p + ui.RESET
            elif command_exists(p):
                result = result + ui.GREEN + ui.BOLD + p + ui.RESET
            else:
                result = result + ui.RED + p + ui.RESET
        elif p[0] == "-":
                result = result + ui.CYAN + p + ui.RESET
        elif p[0] == chr(34) or p[0] == chr(39):
                result = result + ui.YELLOW + p + ui.RESET
        else:
                result = result + p
    return result

proc command_exists(cmd, ENV_PATH):
    if is_builtin(cmd):
        return true
    if len(cmd) == 0:
        return false
    if starts_with(cmd, "./") or starts_with(cmd, "/"):
        return (sys.exec("test -x " + cmd) == 0)
    let check_cmd = "PATH=" + ENV_PATH + " which " + cmd + " > /dev/null 2>&1"
    return sys.exec(check_cmd) == 0

proc find_suggestion(line, HISTORY):
    if len(line) == 0:
        return ""
    for i in range(len(HISTORY)):
        let h = HISTORY[len(HISTORY) - 1 - i]
        if starts_with(h, line):
            return h[len(line):len(h)]
    return ""

proc get_completions(line, ENV_PATH):
    let last_space = -1
    for i in range(len(line)):
        if line[i] == " ":
            last_space = i
    let word = line[last_space+1:len(line)]
    let results = []
    if last_space == -1:
        let builtins = ["exit", "quit", "help", "cd", "clear", "export", "env", "version"]
        for i in range(len(builtins)):
            if starts_with(builtins[i], word):
                    push(results, builtins[i])
        let path = ENV_PATH
        if path != nil:
            let dirs = split(path, ":")
            for i in range(len(dirs)):
                let d = dirs[i]
                if io.isdir(d):
                    sys.exec("ls -1 " + d + " 2>/dev/null > /tmp/sage_path_ls")
                    let content = io.readfile("/tmp/sage_path_ls")
                    if content != nil:
                        let files = split(content, chr(10))
                        for j in range(len(files)):
                            let f = trim(files[j])
                            if starts_with(f, word):
                                let exists = false
                                for k in range(len(results)):
                                    if results[k] == f:
                                        exists = true
                                if not exists:
                                    push(results, f)
    else:
        let dir = "."
        let prefix = word
        if str_contains(word, "/"):
            let last_slash = -1
            for i in range(len(word)):
                if word[i] == "/":
                    last_slash = i
            dir = word[0:last_slash+1]
            if dir == "":
                    dir = "/"
            prefix = word[last_slash+1:len(word)]
        sys.exec("ls -1 -F " + dir + " 2>/dev/null > /tmp/sage_ls")
        let content = io.readfile("/tmp/sage_ls")
        if content != nil:
            let files = split(content, chr(10))
            for i in range(len(files)):
                let f = trim(files[i])
                if starts_with(f, prefix):
                    if dir == "." or dir == "./":
                        push(results, f)
                    else:
                        push(results, dir + f)
    return results

proc process_command(cmd_line, CWD, USER, ENV_PATH, HISTORY, LAST_EXEC_TIME):
    if len(cmd_line) == 0:
        LAST_EXEC_TIME = 0.0
        return [true, CWD, ENV_PATH, LAST_EXEC_TIME]

    let start_t = sys.clock()

    if cmd_line == "exit" or cmd_line == "quit":
        return [false, CWD, ENV_PATH, LAST_EXEC_TIME]
    
    let is_handled = false
    if cmd_line == "clear":
        sys.exec("clear")
        is_handled = true
    elif cmd_line == "version":
        print ui.BOLD + "SageShell" + ui.RESET + " v2.0.0"
        print "Architecture: universal"
        is_handled = true
    elif cmd_line == "debug":
        print "CWD:  " + CWD
        print "USER: " + USER
        print "PATH: " + ENV_PATH
        print "HIST: " + str(len(HISTORY))
        is_handled = true
    elif cmd_line == "history":
        for i in range(len(HISTORY)):
            print " " + str(i + 1) + "  " + HISTORY[i]
        is_handled = true
    elif cmd_line == "reload":
        let h = sys.getenv("HOME")
        if h != nil:
            print "Reloading configuration..."
            # This is tricky as process_command is now modular. 
            # We might need a callback to evaluate commands.
        is_handled = true
    elif cmd_line == "help":
        print "SageShell - A modern, dynamic shell in SageLang"
        print "Built-ins: cd, clear, help, exit, export, env, version, source, reload, debug, history"
        print "Features: Git integration, execution timing, syntax highlighting, autosuggestions, tab completion"
        print "Keys: ←/→ move cursor  ↑/↓ history  ^A/^E home/end  Tab complete  ^C cancel  ^D EOF"
        is_handled = true
    elif starts_with(cmd_line, "source "):
        let parts = split_first(cmd_line, " ")
        let file = trim(parts[1])
        if io.exists(file):
            let content = io.readfile(file)
            let lines = split(content, chr(10))
            for i in range(len(lines)):
                # Again, callback needed for process_command
                pass
        else:
            print "source: no such file: " + file
        is_handled = true
    elif cmd_line == "env":
        print "PATH=" + ENV_PATH
        is_handled = true
    elif starts_with(cmd_line, "export "):
        let parts = split_first(cmd_line, " ")
        let kv = trim(parts[1])
        if str_contains(kv, "="):
            let kv_parts = split_first(kv, "=")
            let key = trim(kv_parts[0])
            let val = trim(kv_parts[1])
            if key == "PATH":
                if str_contains(val, "$PATH"):
                    let v_parts = split(val, "$PATH")
                    val = v_parts[0] + ENV_PATH
                    if len(v_parts) > 1:
                            val = val + v_parts[1]
                ENV_PATH = val
        is_handled = true
    elif starts_with(cmd_line, "cd "):
        let parts = split_first(cmd_line, " ")
        let target = trim(parts[1])
        if len(target) == 0:
            let h = sys.getenv("HOME")
            if h != nil:
                target = h
        let check_cmd = "cd " + CWD + " && cd '" + target + "' 2>/dev/null && pwd > /tmp/sage_cwd_new || echo 'ERROR' > /tmp/sage_cwd_new"
        sys.exec(check_cmd)
        let res = trim(io.readfile("/tmp/sage_cwd_new"))
        if res == "ERROR":
            print "cd: no such file or directory: " + target
        else:
            if len(res) > 0:
                CWD = res
                let h = sys.getenv("HOME")
                if h != nil and starts_with(CWD, h):
                    CWD = "~" + CWD[len(h):len(CWD)]
        is_handled = true

    if not is_handled:
        let exec_cmd = "cd " + CWD + " && PATH=" + chr(34) + ENV_PATH + chr(34) + " " + cmd_line
        sys.exec(exec_cmd)
        # Re-evaluating CWD
        sys.exec("pwd > /tmp/sage_cwd")
        let cwd_out = io.readfile("/tmp/sage_cwd")
        if cwd_out != nil:
            CWD = trim(cwd_out)
            let h = sys.getenv("HOME")
            if h != nil and starts_with(CWD, h):
                CWD = "~" + CWD[len(h):len(CWD)]

    LAST_EXEC_TIME = sys.clock() - start_t
    return [true, CWD, ENV_PATH, LAST_EXEC_TIME]
