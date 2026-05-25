import sys
import io

# ============================================================================
# Version
# ============================================================================
let SAGENANO_VERSION = "2.1.2"

# ============================================================================
# ANSI / Terminal helpers  (inlined from ui.sage)
# ============================================================================
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
    sys.exec("stty size > /tmp/sage_term_size 2>/dev/null")
    let sz = io.readfile("/tmp/sage_term_size")
    if sz != nil:
        let sp = split(trim(sz), " ")
        if len(sp) >= 2:
            return [tonumber(sp[0]), tonumber(sp[1])]
    return [24, 80]

proc draw_title_bar(cols, filename, modified):
    let title = "  SageNano " + SAGENANO_VERSION
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
    let s1 = "^G Help      ^O Write Out  ^W Where Is   ^K Cut Text   ^C Cur Pos    ^X Exit"
    let s2 = "^Y Prev Page ^V Next Page  ^R Read File  ^U Uncut Text"
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

proc clear_eol():
    return ESC + "[K"

proc enter_alt():
    return ESC + "[?1049h" + ESC + "[2J" + ESC + "[H"

proc exit_alt():
    return ESC + "[?1049l"

# ============================================================================
# Buffer  (inlined from buffer.sage)
# ============================================================================
class Buffer:
    proc init():
        self.lines = [""]
        self.cx = 0
        self.cy = 0
        self.cut_buffer = []
        self.modified = false

    proc insert_char(ch):
        let l = self.lines[self.cy]
        self.lines[self.cy] = l[0:self.cx] + ch + l[self.cx:len(l)]
        self.cx = self.cx + 1
        self.modified = true

    proc do_enter():
        let l = self.lines[self.cy]
        let left = l[0:self.cx]
        let right = l[self.cx:len(l)]
        self.lines[self.cy] = left
        let new_lines = []
        for i in range(self.cy + 1):
            push(new_lines, self.lines[i])
        push(new_lines, right)
        for i in range(len(self.lines) - self.cy - 1):
            push(new_lines, self.lines[self.cy + 1 + i])
        self.lines = new_lines
        self.cy = self.cy + 1
        self.cx = 0
        self.modified = true

    proc do_backspace():
        if self.cx > 0:
            let l = self.lines[self.cy]
            self.lines[self.cy] = l[0:self.cx-1] + l[self.cx:len(l)]
            self.cx = self.cx - 1
            self.modified = true
        else:
            if self.cy > 0:
                let l = self.lines[self.cy]
                let prev = self.lines[self.cy - 1]
                self.lines[self.cy - 1] = prev + l
                self.cx = len(prev)
                let new_lines = []
                for i in range(self.cy):
                    push(new_lines, self.lines[i])
                for i in range(len(self.lines) - self.cy - 1):
                    push(new_lines, self.lines[self.cy + 1 + i])
                self.lines = new_lines
                self.cy = self.cy - 1
                self.modified = true

    proc do_delete():
        if self.cx < len(self.lines[self.cy]):
            let l = self.lines[self.cy]
            self.lines[self.cy] = l[0:self.cx] + l[self.cx+1:len(l)]
            self.modified = true
        elif self.cy < len(self.lines) - 1:
            let l = self.lines[self.cy]
            let next_l = self.lines[self.cy + 1]
            self.lines[self.cy] = l + next_l
            let new_lines = []
            for i in range(self.cy + 1):
                push(new_lines, self.lines[i])
            for i in range(len(self.lines) - self.cy - 2):
                push(new_lines, self.lines[self.cy + 2 + i])
            self.lines = new_lines
            self.modified = true

    proc cut_line():
        if len(self.lines) == 1 and self.lines[0] == "":
            return
        let line = self.lines[self.cy]
        push(self.cut_buffer, line)
        if len(self.lines) > 1:
            let new_lines = []
            for i in range(self.cy):
                push(new_lines, self.lines[i])
            for i in range(len(self.lines) - self.cy - 1):
                push(new_lines, self.lines[self.cy + 1 + i])
            self.lines = new_lines
            if self.cy >= len(self.lines):
                self.cy = len(self.lines) - 1
        else:
            self.lines = [""]
            self.cx = 0
        self.cx = 0
        self.modified = true

    proc uncut_text():
        if len(self.cut_buffer) == 0:
            return
        for i in range(len(self.cut_buffer)):
            let line = self.cut_buffer[i]
            let new_lines = []
            for j in range(self.cy):
                push(new_lines, self.lines[j])
            push(new_lines, line)
            for j in range(len(self.lines) - self.cy):
                push(new_lines, self.lines[self.cy + j])
            self.lines = new_lines
            self.cy = self.cy + 1
        self.cx = 0
        self.modified = true
        self.cut_buffer = []

    proc move_up():
        if self.cy > 0:
            self.cy = self.cy - 1
            if self.cx > len(self.lines[self.cy]):
                self.cx = len(self.lines[self.cy])

    proc move_down():
        if self.cy < len(self.lines) - 1:
            self.cy = self.cy + 1
            if self.cx > len(self.lines[self.cy]):
                self.cx = len(self.lines[self.cy])

    proc move_left():
        if self.cx > 0:
            self.cx = self.cx - 1
        elif self.cy > 0:
            self.cy = self.cy - 1
            self.cx = len(self.lines[self.cy])

    proc move_right():
        if self.cx < len(self.lines[self.cy]):
            self.cx = self.cx + 1
        elif self.cy < len(self.lines) - 1:
            self.cy = self.cy + 1
            self.cx = 0

# ============================================================================
# Helpers
# ============================================================================
proc trim(s):
    if s == nil:
        return ""
    let si = 0
    while si < len(s) and (s[si] == " " or s[si] == "\n" or s[si] == "\r" or s[si] == "\t"):
        si = si + 1
    let ei = len(s) - 1
    while ei >= si and (s[ei] == " " or s[ei] == "\n" or s[ei] == "\r" or s[ei] == "\t"):
        ei = ei - 1
    if ei < si:
        return ""
    let res = ""
    for i in range(ei - si + 1):
        res = res + s[si + i]
    return res

# ============================================================================
# Editor  (inlined from editor.sage)
# ============================================================================
class Editor:
    proc init():
        self.buffer = Buffer()
        self.filename = ""
        self.message = ""
        self.scroll_y = 0
        self.running = true
        self.cols = 80
        self.rows = 24
        self.search_query = ""
        # Flicker-reduction: cache of last rendered lines
        self.prev_lines = []
        self.prev_scroll = -1
        self.force_redraw = true

    proc load(path):
        if not io.exists(path):
            self.filename = path
            self.message = "New File"
            return
        let content = io.readfile(path)
        if content == nil:
            self.message = "Error reading " + path
            return
        self.filename = path
        let lns = split(content, chr(10))
        if len(lns) == 0:
            lns = [""]
        self.buffer.lines = lns
        self.buffer.modified = false
        let _lword = "lines"
        if len(lns) == 1:
            _lword = "line"
        self.message = "Read " + str(len(lns)) + " " + _lword
        self.force_redraw = true

    proc save():
        if self.filename == "":
            self.message = "No filename"
            return
        let content = ""
        for i in range(len(self.buffer.lines)):
            content = content + self.buffer.lines[i]
            if i < len(self.buffer.lines) - 1:
                content = content + chr(10)
        io.writefile(self.filename, content)
        self.buffer.modified = false
        let _lword2 = "lines"
        if len(self.buffer.lines) == 1:
            _lword2 = "line"
        self.message = "Wrote " + str(len(self.buffer.lines)) + " " + _lword2

    proc draw():
        let size = get_term_size()
        self.cols = size[1]
        self.rows = size[0]

        # Clamp rows/cols to sensible minimums
        if self.rows < 6:
            self.rows = 6
        if self.cols < 20:
            self.cols = 20

        let edit_rows = self.rows - 4   # title + 2 shortcut + 1 status

        # Adjust scroll
        if self.buffer.cy < self.scroll_y:
            self.scroll_y = self.buffer.cy
        if self.buffer.cy >= self.scroll_y + edit_rows:
            self.scroll_y = self.buffer.cy - edit_rows + 1

        # Build per-line content (plain text, clipped to cols)
        let new_lines = []
        for y in range(edit_rows):
            let file_y = self.scroll_y + y
            if file_y < len(self.buffer.lines):
                let l = self.buffer.lines[file_y]
                if len(l) > self.cols:
                    push(new_lines, l[0:self.cols])
                else:
                    push(new_lines, l)
            else:
                push(new_lines, "~")

        # --- Differential rendering ---
        let out = hide_cursor()

        if self.force_redraw or self.prev_scroll != self.scroll_y or len(self.prev_lines) != len(new_lines):
            # Full redraw: move to (1,1) then paint everything
            out = out + set_cursor(1, 1)
            out = out + draw_title_bar(self.cols, self.filename, self.buffer.modified)
            for y in range(edit_rows):
                out = out + new_lines[y] + clear_eol() + "\r\n"
            out = out + draw_status_bar(self.cols, self.message)
            out = out + draw_shortcut_bar(self.cols)
            self.force_redraw = false
        else:
            # Partial redraw: only repaint lines that changed + status bar
            # Title bar (row 1)
            out = out + set_cursor(1, 1)
            out = out + draw_title_bar(self.cols, self.filename, self.buffer.modified)
            # Content lines (rows 2 .. edit_rows+1)
            for y in range(edit_rows):
                let prev = ""
                if y < len(self.prev_lines):
                    prev = self.prev_lines[y]
                if new_lines[y] != prev:
                    out = out + set_cursor(y + 2, 1)
                    out = out + new_lines[y] + clear_eol()
            # Status bar is always refreshed (message may change)
            out = out + set_cursor(edit_rows + 2, 1)
            out = out + draw_status_bar(self.cols, self.message)
            # Shortcut bar only changes if cols changed — force a refresh there too
            out = out + set_cursor(edit_rows + 3, 1)
            out = out + draw_shortcut_bar(self.cols)

        # Cache this frame
        self.prev_lines = new_lines
        self.prev_scroll = self.scroll_y

        # Place cursor at correct edit position
        let screen_y = self.buffer.cy - self.scroll_y + 2
        let screen_x = self.buffer.cx + 1
        out = out + set_cursor(screen_y, screen_x)
        out = out + show_cursor()

        io.writefile("/dev/stdout", out)

    proc prompt(p):
        # Stay in raw mode — read chars manually so keystrokes never echo
        # into the content area.  The status bar line is used for input.
        let result = ""
        let edit_rows = self.rows - 4
        let status_row = edit_rows + 2   # 1-indexed row of the status bar

        # Paint the prompt text in the status bar
        let bar = p
        while len(bar) < self.cols:
            bar = bar + " "
        let header = hide_cursor() + set_cursor(status_row, 1) + REVERSE + bar + RESET
        io.writefile("/dev/stdout", header)

        # Position cursor just after the prompt text (still in status bar row)
        io.writefile("/dev/stdout", set_cursor(status_row, len(p) + 1) + show_cursor())

        while true:
            sys.exec("dd bs=1 count=1 2>/dev/null > /tmp/sage_nano_key")
            let k = io.readfile("/tmp/sage_nano_key")
            if k == nil or len(k) == 0:
                continue
            let code = ord(k[0])

            if code == 13 or code == 10:   # Enter — accept
                return result
            elif code == 27 or code == 7:  # Esc / ^G — cancel
                return ""
            elif code == 127 or code == 8: # Backspace
                if len(result) > 0:
                    result = result[0:len(result)-1]
                    io.writefile("/dev/stdout", "\b \b")
            elif code >= 32 and code <= 126:
                result = result + k[0]
                io.writefile("/dev/stdout", k[0])  # echo char into status bar

        return result

    proc search():
        let query = self.prompt("Search: ")
        if query == "":
            return
        self.search_query = query
        for y in range(len(self.buffer.lines) - self.buffer.cy):
            let file_y = self.buffer.cy + y
            let line = self.buffer.lines[file_y]
            let start = 0
            if file_y == self.buffer.cy:
                start = self.buffer.cx + 1
            if start < len(line):
                let sub = line[start:len(line)]
                if len(split(sub, query)) > 1:
                    for x in range(len(sub) - len(query) + 1):
                        if sub[x:x+len(query)] == query:
                            self.buffer.cy = file_y
                            self.buffer.cx = start + x
                            self.message = "Found: " + query
                            self.force_redraw = true
                            return
        self.message = "Not found: " + query

    proc show_help():
        sys.exec("clear")
        print REVERSE + " SageNano Help " + RESET
        print " ^G (F1)      Display this help text"
        print " ^X           Exit"
        print " ^O (F3)      Write the current file to disk"
        print " ^R (F5)      Insert another file"
        print " ^W (F6)      Search for a string"
        print " ^K (F9)      Cut current line"
        print " ^U (F10)     Uncut from cut buffer"
        print " ^C (F11)     Display cursor position"
        print " ^Y (PgUp)    Go one screenful up"
        print " ^V (PgDn)    Go one screenful down"
        print ""
        print " Press any key to continue"
        sys.exec("dd bs=1 count=1 2>/dev/null > /dev/null")
        self.force_redraw = true

    proc process_key(code, ch):
        self.message = ""
        if code == 24:   # ^X — Exit
            if self.buffer.modified:
                let ans = self.prompt("Save modified buffer? (y/n): ")
                if ans == "y" or ans == "Y":
                    self.save()
            self.running = false
        elif code == 15: # ^O — Save
            self.save()
        elif code == 18: # ^R — Insert file
            let path = self.prompt("File to insert [from ./]: ")
            if path != "" and io.exists(path):
                let content = io.readfile(path)
                if content != nil:
                    let lns = split(content, chr(10))
                    for i in range(len(lns)):
                        let l = lns[i]
                        let new_lines = []
                        for j in range(self.buffer.cy + 1):
                            push(new_lines, self.buffer.lines[j])
                        push(new_lines, l)
                        for j in range(len(self.buffer.lines) - self.buffer.cy - 1):
                            push(new_lines, self.buffer.lines[self.buffer.cy + 1 + j])
                        self.buffer.lines = new_lines
                        self.buffer.cy = self.buffer.cy + 1
                    self.message = "Inserted " + str(len(lns)) + " lines"
                    self.force_redraw = true
            else:
                self.message = "File not found"
        elif code == 7:  # ^G — Help
            self.show_help()
        elif code == 23: # ^W — Search
            self.search()
        elif code == 11: # ^K — Cut
            self.buffer.cut_line()
            self.force_redraw = true
        elif code == 21: # ^U — Uncut
            self.buffer.uncut_text()
            self.force_redraw = true
        elif code == 3:  # ^C — Cursor position
            self.message = "line " + str(self.buffer.cy + 1) + "/" + str(len(self.buffer.lines)) + ", col " + str(self.buffer.cx + 1)
        elif code == 25: # ^Y — Page up
            for i in range(self.rows - 4):
                self.buffer.move_up()
            self.force_redraw = true
        elif code == 22: # ^V — Page down
            for i in range(self.rows - 4):
                self.buffer.move_down()
            self.force_redraw = true
        elif code == 13 or code == 10: # Enter
            self.buffer.do_enter()
            self.force_redraw = true
        elif code == 127 or code == 8: # Backspace
            self.buffer.do_backspace()
        elif code == 27: # Escape sequences (arrows, Delete, PgUp, PgDn)
            sys.exec("dd bs=1 count=1 2>/dev/null > /tmp/sage_nano_key")
            let n1 = io.readfile("/tmp/sage_nano_key")
            if n1 != nil and ord(n1[0]) == 91:
                sys.exec("dd bs=1 count=1 2>/dev/null > /tmp/sage_nano_key")
                let n2 = io.readfile("/tmp/sage_nano_key")
                if n2 != nil:
                    let d = ord(n2[0])
                    if d == 65:
                        self.buffer.move_up()
                    elif d == 66:
                        self.buffer.move_down()
                    elif d == 67:
                        self.buffer.move_right()
                    elif d == 68:
                        self.buffer.move_left()
                    elif d == 51: # Delete (Esc[3~)
                        sys.exec("dd bs=1 count=1 2>/dev/null > /dev/null")
                        self.buffer.do_delete()
                    elif d == 53: # PgUp (Esc[5~)
                        sys.exec("dd bs=1 count=1 2>/dev/null > /dev/null")
                        for i in range(self.rows - 4):
                            self.buffer.move_up()
                        self.force_redraw = true
                    elif d == 54: # PgDn (Esc[6~)
                        sys.exec("dd bs=1 count=1 2>/dev/null > /dev/null")
                        for i in range(self.rows - 4):
                            self.buffer.move_down()
                        self.force_redraw = true
        elif code >= 32 and code <= 126:
            self.buffer.insert_char(ch)

    proc restore_terminal():
        sys.exec("stty icanon echo")
        io.writefile("/dev/stdout", exit_alt())

    proc loop():
        # Enter alternate screen buffer to prevent scroll and reduce flicker
        io.writefile("/dev/stdout", enter_alt())
        sys.exec("stty raw -echo")
        while self.running:
            self.draw()
            sys.exec("dd bs=1 count=1 2>/dev/null > /tmp/sage_nano_key")
            let k = io.readfile("/tmp/sage_nano_key")
            if k == nil or len(k) == 0:
                continue
            self.process_key(ord(k[0]), k[0])
        restore_terminal()

# ============================================================================
# Entry point
# ============================================================================
proc main():
    let ed = Editor()
    let args = sys.args()
    if len(args) > 2:
        ed.load(args[2])
    ed.loop()

main()
