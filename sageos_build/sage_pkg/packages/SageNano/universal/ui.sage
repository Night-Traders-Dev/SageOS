import sys
import io

let ESC = chr(27)
let RESET = ESC + "[0m"
let REVERSE = ESC + "[7m"
let BOLD = ESC + "[1m"

proc get_term_size():
    let content = io.readfile("/tmp/sage_ui_state")
    if content != nil:
        let parts = split(content, "|")
        if len(parts) >= 3:
            let size_parts = split(parts[2], " ")
            if len(size_parts) >= 2:
                return [tonumber(size_parts[0]), tonumber(size_parts[1])]
    return [24, 80]

proc draw_title_bar(cols, filename, modified):
    let title = "  SageNano 1.0.1"
    let status = ""
    if modified:
        status = "Modified"
    
    let f_display = filename
    if f_display == "":
        f_display = "New Buffer"
    
    let center = f_display
    if status != "":
        center = center + " (" + status + ")"
        
    let left_pad = (cols - len(title) - len(center)) / 2 | 0
    if left_pad < 0:
        left_pad = 0
    
    let line = title
    for i in range(left_pad):
        line = line + " "
    line = line + center
    while len(line) < cols:
        line = line + " "
    
    return REVERSE + line + RESET + "\r\n"

proc draw_status_bar(cols, message):
    let line = message
    while len(line) < cols:
        line = line + " "
    return REVERSE + line + RESET + "\r\n"

proc draw_shortcut_bar(cols):
    let s1 = "^G Help      ^O Write Out  ^W Where Is   ^K Cut Text   ^J Justify    ^C Cur Pos"
    let s2 = "^X Exit      ^R Read File  ^\\ Replace    ^U Uncut Text ^T To Spell   ^Y Prev Page"
    
    # We'll actually only implement a subset but show the 1:1 UI
    let line1 = s1
    while len(line1) < cols:
        line1 = line1 + " "
    
    let line2 = s2
    while len(line2) < cols:
        line2 = line2 + " "
    
    return REVERSE + line1 + RESET + "\r\n" + REVERSE + line2 + RESET

proc set_cursor(y, x):
    return ESC + "[" + str(y) + ";" + str(x) + "H"

proc hide_cursor():
    return ESC + "[?25l"

proc show_cursor():
    return ESC + "[?25h"

proc clear_line():
    return ESC + "[K"

proc clear_screen():
    return ESC + "[2J" + ESC + "[H"

proc enter_alt_buffer():
    return ESC + "[?1049h"

proc exit_alt_buffer():
    return ESC + "[?1049l"
