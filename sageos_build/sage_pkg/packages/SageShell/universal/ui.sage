let ESC = chr(27)
let RESET = ESC + "[0m"
let BOLD = ESC + "[1m"
let DIM = ESC + "[2m"
let ITALIC = ESC + "[3m"
let UNDERLINE = ESC + "[4m"

let GREEN = ESC + "[32m"
let BLUE = ESC + "[34m"
let CYAN = ESC + "[36m"
let RED = ESC + "[31m"
let GREY = ESC + "[90m"
let MAGENTA = ESC + "[35m"
let YELLOW = ESC + "[33m"
let WHITE = ESC + "[37m"

let BG_BLACK = ESC + "[40m"
let BG_RED = ESC + "[41m"
let BG_GREEN = ESC + "[42m"
let BG_YELLOW = ESC + "[43m"
let BG_BLUE = ESC + "[44m"
let BG_MAGENTA = ESC + "[45m"
let BG_CYAN = ESC + "[46m"
let BG_WHITE = ESC + "[47m"

let STATE_FILE = "/tmp/sage_ui_state"

let LAST_BAR = ""

proc draw_status_bar(IS_TTY, CWD):
    if not IS_TTY:
        return
    let state = get_cached_state()
    let time = state[0]
    let temp = state[1]
    let size_parts = split(state[2], " ")
    let rows = tonumber(size_parts[0])
    let cols = tonumber(size_parts[1])

    let left = " 🐚 SageShell " + BOLD + CWD + RESET
    let mid = time
    let right = temp + " "

    let left_len = len(left) - 10 
    let mid_len = len(mid)
    let right_len = len(right)

    let pad_left_len = (cols / 2 | 0) - left_len - (mid_len / 2 | 0)
    if pad_left_len < 1:
        pad_left_len = 1

    let pad_right_len = cols - left_len - pad_left_len - mid_len - right_len
    if pad_right_len < 1:
        pad_right_len = 1

    let bar = ESC + "[44;37m" + left
    for i in range(pad_left_len):
        bar = bar + " "
    bar = bar + mid
    for i in range(pad_right_len):
        bar = bar + " "
    bar = bar + right + RESET

    if bar != LAST_BAR:
        let output = ESC + "[s" + ESC + "[" + str(rows) + ";1H" + bar + ESC + "[u"
        io.writefile("/dev/stdout", output)
        LAST_BAR = bar

proc print_prompt(USER, CWD, git_info, last_exec_time):
    let time_str = ""
    if last_exec_time > 0.0:
        if last_exec_time > 1.0:
            time_str = " ⏳ " + str((last_exec_time * 10 | 0) / 10.0) + "s "
        else:
            time_str = " ⏳ " + str((last_exec_time * 1000 | 0)) + "ms "

    let p_user = BG_CYAN + BG_BLUE + BOLD + WHITE + " 🐚 " + USER + " " + RESET
    let p_cwd = BG_BLUE + BOLD + WHITE + " " + CWD + " " + RESET
    
    let p_git = ""
    if len(git_info) > 0:
        p_git = BG_MAGENTA + BOLD + WHITE + " 🌿 " + git_info + " " + RESET

    let p_time = ""
    if len(time_str) > 0:
        p_time = BG_BLACK + YELLOW + time_str + RESET

    let header = "\r" + ESC + "[K" + p_user + p_cwd + p_git + p_time + "\n"
    let prompt_sym = "\r" + ESC + "[K" + GREEN + BOLD + "❯" + RESET + " "
    io.writefile("/dev/stdout", header + prompt_sym)

proc set_scrolling_region(rows):
    io.writefile("/dev/stdout", ESC + "[1;" + str(rows - 1) + "r")

proc reset_scrolling_region():
    io.writefile("/dev/stdout", ESC + "[r")

proc get_cached_state():
    let content = io.readfile(STATE_FILE)
    if content == nil:
        return ["N/A", "N/A", "24 80", ""]
    let parts = split(content, "|")
    if len(parts) < 4:
        push(parts, "")
    return parts
