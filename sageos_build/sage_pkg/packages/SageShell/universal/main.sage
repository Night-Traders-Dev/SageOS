import sys
import io
import ui

let HISTORY = []
let HISTORY_INDEX = 0
let LAST_EXEC_TIME = 0.0

proc check_tty():
    return sys.exec("[ -t 0 ]") == 0

let IS_TTY = check_tty()

proc load_history():
    let home = sys.getenv("HOME")
    if home == nil:
        return
    let h_file = home + "/.sageshell_history"
    if io.exists(h_file):
        let content = io.readfile(h_file)
        if content != nil:
            let lines = split(content, chr(10))
            for i in range(len(lines)):
                let l = trim(lines[i])
                if len(l) > 0:
                    push(HISTORY, l)

proc save_history(line):
    let home = sys.getenv("HOME")
    if home == nil:
        return
    let h_file = home + "/.sageshell_history"
    io.appendfile(h_file, line + chr(10))

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

proc get_cwd():
    sys.exec("pwd > /tmp/sage_cwd")
    let cwd = io.readfile("/tmp/sage_cwd")
    if cwd == nil:
        return "/"
    let c = trim(cwd)
    let h = sys.getenv("HOME")
    if h != nil and starts_with(c, h):
        return "~" + c[len(h):len(c)]
    return c

proc get_user():
    let u = sys.getenv("USER")
    if u == nil:
        return "user"
    return u

let CWD = get_cwd()
let USER = get_user()

let ENV_PATH = sys.getenv("PATH")
let HOME = sys.getenv("HOME")

if HOME != nil:
    let worker_path = HOME + "/.sagepkg/packages/SageUtils/universal/ui_worker.sage"
    if io.exists("packages/SageUtils/universal/ui_worker.sage"):
        worker_path = "packages/SageUtils/universal/ui_worker.sage"
    sys.exec("sage " + worker_path + " >/dev/null 2>&1 &")

if ENV_PATH == nil or len(ENV_PATH) < 5:
    ENV_PATH = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

if HOME != nil:
    let s_bin = HOME + "/.sagepkg/bin"
    if not str_contains(ENV_PATH, s_bin):
        ENV_PATH = s_bin + ":" + ENV_PATH
    else:
        let parts = split(ENV_PATH, ":")
        let new_path = s_bin
        for i in range(len(parts)):
            if parts[i] != s_bin:
                new_path = new_path + ":" + parts[i]
        ENV_PATH = new_path

proc print_line_raw(l):
    io.writefile("/dev/stdout", l)

proc is_builtin(cmd):
    if cmd == "exit" or cmd == "quit" or cmd == "help" or cmd == "cd" or cmd == "clear" or cmd == "export" or cmd == "env" or cmd == "version" or cmd == "source" or cmd == "reload" or cmd == "debug" or cmd == "history":
        return true
    return false

proc highlight(line):
    if len(line) == 0:
        return ""
    let result = ""
    let parts = []
    let current = ""
    let in_string = false
    for i in range(len(line)):
        let ch = line[i]
        if ch == chr(34):
            in_string = not in_string
            current = current + ch
            if not in_string:
                push(parts, [current, "string"])
                current = ""
        elif in_string:
            current = current + ch
        elif ch == " ":
            if len(current) > 0:
                push(parts, [current, "word"])
                current = ""
            push(parts, [" ", "space"])
        else:
            current = current + ch
    if len(current) > 0:
        push(parts, [current, "word"])

    for i in range(len(parts)):
        let p = parts[i]
        let text = p[0]
        let type = p[1]
        if type == "string":
            result = result + ui.YELLOW + text + ui.RESET
        elif type == "word":
            if is_builtin(text):
                result = result + ui.CYAN + ui.BOLD + text + ui.RESET
            elif starts_with(text, "-"):
                result = result + ui.GREY + text + ui.RESET
            else:
                result = result + text
        else:
            result = result + text
    return result

proc find_suggestion(line):
    if len(line) == 0:
        return ""
    for i in range(len(HISTORY)):
        let h = HISTORY[len(HISTORY) - 1 - i]
        if starts_with(h, line):
            return h[len(line):len(h)]
    return ""

proc get_completions(line):
    let results = []
    let parts = split(line, " ")
    let last = parts[len(parts)-1]
    
    # Files
    sys.exec("ls -a > /tmp/sage_ls")
    let content = io.readfile("/tmp/sage_ls")
    if content != nil:
        let lines = split(content, chr(10))
        for i in range(len(lines)):
            let f = trim(lines[i])
            if f != "." and f != ".." and starts_with(f, last):
                push(results, f)
    
    # Commands from PATH
    let path_parts = split(ENV_PATH, ":")
    for i in range(len(path_parts)):
        let dir = path_parts[i]
        if io.exists(dir):
            sys.exec("ls " + dir + " > /tmp/sage_path_ls")
            let c = io.readfile("/tmp/sage_path_ls")
            if c != nil:
                let lns = split(c, chr(10))
                for j in range(len(lns)):
                    let f = trim(lns[j])
                    if starts_with(f, last):
                        push(results, f)
    return results

proc restore_terminal():
    sys.exec("stty icanon echo")

proc sage_readline():
    if not IS_TTY:
        sys.exec("read -r line_in && echo $line_in > /tmp/sage_in || echo 'EOF' > /tmp/sage_in")
        let res = trim(io.readfile("/tmp/sage_in"))
        if res == "EOF":
            return nil
        return res

    let line = ""
    let cursor = 0
    let suggestion = ""
    let h_search = ""
    let last_sec = ""
    let needs_redraw = true
    let first_draw = true

    sys.exec("stty -icanon -echo min 0 time 0")

    while true:
        let state = ui.get_cached_state()
        let current_time = state[0]
        let git_info = state[3]
        if current_time != last_sec:
            ui.draw_status_bar(IS_TTY, CWD)
            last_sec = current_time

        if needs_redraw:
            if cursor == len(line):
                suggestion = find_suggestion(line)
            else:
                suggestion = ""

            if not first_draw:
                # Move up one line to clear the top part of the two-line prompt
                io.writefile("/dev/stdout", ui.ESC + "[1A")
            
            io.writefile("/dev/stdout", "\r" + ui.ESC + "[K")
            ui.print_prompt(USER, CWD, git_info, LAST_EXEC_TIME)
            first_draw = false

            if cursor > 0:
                print_line_raw(highlight(line[0:cursor]))
            let after = line[cursor:len(line)]
            if len(after) > 0:
                print_line_raw(after)

            if cursor == len(line) and len(suggestion) > 0:
                print_line_raw(ui.GREY + ui.ITALIC + suggestion + ui.RESET)
                for i in range(len(suggestion)):
                    io.writefile("/dev/stdout", "\b")

            if len(after) > 0:
                for i in range(len(after)):
                    io.writefile("/dev/stdout", "\b")

            needs_redraw = false

        sys.exec("dd bs=1 count=1 2>/dev/null > /tmp/sage_key")
        let k = io.readfile("/tmp/sage_key")
        if k == nil or len(k) == 0:
            sys.sleep(0.02)
            continue

        needs_redraw = true
        let ch = k[0]
        let code = ord(ch)

        if code == 10 or code == 13:
            print ""
            restore_terminal()
            return line

        if code == 127 or code == 8:
            if cursor > 0:
                line = line[0:cursor-1] + line[cursor:len(line)]
                cursor = cursor - 1
                h_search = ""
            continue

        if code == 12:
            sys.exec("clear")
            continue

        if code == 4:
            if len(line) == 0:
                restore_terminal()
                return nil
            continue

        if code == 3:
            io.writefile("/dev/stdout", "\r" + ui.ESC + "[K^C\r\n")
            restore_terminal()
            return ""

        if code == 1:
            cursor = 0
            continue

        if code == 5:
            cursor = len(line)
            continue

        if code == 9:
            if cursor == len(line) and len(suggestion) > 0:
                line = line + suggestion
                cursor = len(line)
            else:
                let comps = get_completions(line[0:cursor])
                if len(comps) == 1:
                    let last_space = -1
                    for i in range(len(line)):
                        if line[i] == " ":
                            last_space = i
                    line = line[0:last_space+1] + comps[0]
                    cursor = len(line)
                elif len(comps) > 1:
                    print ""
                    let comp_line = ""
                    for i in range(len(comps)):
                        comp_line = comp_line + comps[i] + "  "
                        if len(comp_line) > 60:
                            print comp_line
                            comp_line = ""
                    if len(comp_line) > 0:
                            print comp_line
            continue

        if code == 27:
            sys.exec("dd bs=1 count=1 2>/dev/null > /tmp/sage_key")
            let next1 = io.readfile("/tmp/sage_key")
            if next1 != nil and ord(next1[0]) == 91:
                sys.exec("dd bs=1 count=1 2>/dev/null > /tmp/sage_key")
                let next2 = io.readfile("/tmp/sage_key")
                if next2 != nil:
                    let d = ord(next2[0])
                    if d == 65:
                        if h_search == "":
                            h_search = line[0:cursor]
                        let idx = HISTORY_INDEX - 1
                        while idx >= 0:
                            if starts_with(HISTORY[idx], h_search):
                                HISTORY_INDEX = idx
                                line = HISTORY[idx]
                                cursor = len(line)
                                break
                            idx = idx - 1
                        continue
                    if d == 66:
                        if h_search == "":
                            h_search = line[0:cursor]
                        let idx = HISTORY_INDEX + 1
                        let found = false
                        while idx < len(HISTORY):
                            if starts_with(HISTORY[idx], h_search):
                                HISTORY_INDEX = idx
                                line = HISTORY[idx]
                                cursor = len(line)
                                found = true
                                break
                            idx = idx + 1
                        if not found:
                            line = h_search
                            cursor = len(line)
                            HISTORY_INDEX = len(HISTORY)
                        continue
                    if d == 67:
                        if cursor < len(line):
                            cursor = cursor + 1
                        elif len(suggestion) > 0:
                            line = line + suggestion
                            cursor = len(line)
                        continue
                    if d == 68:
                        if cursor > 0:
                            cursor = cursor - 1
                        continue
                    if d == 72:
                        cursor = 0
                        continue
                    if d == 70:
                        cursor = len(line)
                        continue
                    if d == 51:
                        sys.exec("dd bs=1 count=1 2>/dev/null > /dev/null")
                        if cursor < len(line):
                            line = line[0:cursor] + line[cursor+1:len(line)]
                        continue
                    if d == 49:
                        sys.exec("dd bs=2 count=1 2>/dev/null > /dev/null")
                        continue
            continue

        if code >= 32 and code <= 126:
            line = line[0:cursor] + ch + line[cursor:len(line)]
            cursor = cursor + 1
            h_search = ""

    restore_terminal()
    return line

proc process_command(cmd_line):
    if len(cmd_line) == 0:
        LAST_EXEC_TIME = 0.0
        return true

    let start_t = sys.clock()

    if cmd_line == "exit" or cmd_line == "quit":
        return false
    
    let is_handled = false
    if cmd_line == "clear":
        sys.exec("clear")
        is_handled = true
    elif cmd_line == "version":
        print ui.BOLD + "SageShell" + ui.RESET + " v2.1.3"
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
            process_command("source " + h + "/.sageshellrc")
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
                process_command(trim(lines[i]))
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
        CWD = get_cwd()

    LAST_EXEC_TIME = sys.clock() - start_t
    return true

proc main():
    load_history()
    print ui.BG_CYAN + ui.BG_BLUE + ui.BOLD + ui.WHITE + " Welcome to SageShell v2.1.3 " + ui.RESET
    print ui.ITALIC + "Type 'help' for commands, 'exit' to quit." + ui.RESET

    let h = sys.getenv("HOME")
    if h != nil:
        let rc = h + "/.sageshellrc"
        if io.exists(rc):
            let content = io.readfile(rc)
            let lines = split(content, chr(10))
            for i in range(len(lines)):
                process_command(trim(lines[i]))

    let last_rows = 0
    while true:
        let state = ui.get_cached_state()
        let size_parts = split(state[2], " ")
        let rows = tonumber(size_parts[0])
        if rows != last_rows:
            ui.set_scrolling_region(rows)
            last_rows = rows

        HISTORY_INDEX = len(HISTORY)
        let cmd_line = sage_readline()
        if cmd_line == nil:
            ui.reset_scrolling_region()
            print "exit"
            break
        
        cmd_line = trim(cmd_line)
        if len(cmd_line) == 0:
            LAST_EXEC_TIME = 0.0
            continue

        if len(HISTORY) == 0 or HISTORY[len(HISTORY)-1] != cmd_line:
            push(HISTORY, cmd_line)
            save_history(cmd_line)

        if not process_command(cmd_line):
            ui.reset_scrolling_region()
            break

main()
