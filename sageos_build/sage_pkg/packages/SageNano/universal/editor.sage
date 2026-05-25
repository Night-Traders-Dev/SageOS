import sys
import io
from ui import get_term_size, draw_title_bar, draw_status_bar, draw_shortcut_bar, set_cursor, hide_cursor, show_cursor, clear_line, REVERSE, RESET, BOLD
from buffer import Buffer

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
        self.message = "Read " + str(len(lns)) + " lines"

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
        self.message = "Wrote " + str(len(self.buffer.lines)) + " lines"

    proc draw():
        let size = get_term_size()
        self.cols = size[1]
        self.rows = size[0]
        
        # Adjust scroll
        if self.buffer.cy < self.scroll_y:
            self.scroll_y = self.buffer.cy
        if self.buffer.cy >= self.scroll_y + self.rows - 4:
            self.scroll_y = self.buffer.cy - (self.rows - 4) + 1
            
        let out = hide_cursor()
        out = out + set_cursor(1, 1)
        out = out + draw_title_bar(self.cols, self.filename, self.buffer.modified)
        
        for y in range(self.rows - 4):
            let file_y = self.scroll_y + y
            if file_y < len(self.buffer.lines):
                let l = self.buffer.lines[file_y]
                out = out + l[0:self.cols]
            out = out + clear_line() + "\r\n"
            
        out = out + draw_status_bar(self.cols, self.message)
        out = out + draw_shortcut_bar(self.cols)
        
        let screen_y = self.buffer.cy - self.scroll_y + 2
        let screen_x = self.buffer.cx + 1
        out = out + set_cursor(screen_y, screen_x)
        out = out + show_cursor()
        
        io.writefile("/dev/stdout", out)

    proc prompt(p):
        self.message = p
        self.draw()
        let result = ""
        sys.exec("stty icanon echo")
        sys.exec("read -r val && echo $val > /tmp/sage_nano_prompt")
        let res = io.readfile("/tmp/sage_nano_prompt")
        sys.exec("stty raw -echo")
        if res != nil:
            result = trim(res)
        return result

    proc search():
        let query = self.prompt("Search: ")
        if query == "":
            return
        self.search_query = query
        
        # Search forward from current pos
        for y in range(len(self.buffer.lines) - self.buffer.cy):
            let file_y = self.buffer.cy + y
            let line = self.buffer.lines[file_y]
            let start = 0
            if file_y == self.buffer.cy:
                start = self.buffer.cx + 1
            
            if start < len(line):
                let sub = line[start:len(line)]
                if len(split(sub, query)) > 1:
                    # Find exact index
                    for x in range(len(sub) - len(query) + 1):
                        if sub[x:x+len(query)] == query:
                            self.buffer.cy = file_y
                            self.buffer.cx = start + x
                            self.message = "Found " + query
                            return
        self.message = "Not found: " + query

    proc show_help():
        sys.exec("clear")
        print REVERSE + " SageNano Help " + RESET
        print " ^G (F1)      Display this help text"
        print " ^X (F2)      Exit from nano"
        print " ^O (F3)      Write the current file to disk"
        print " ^R (F5)      Insert another file into the current one"
        print " ^W (F6)      Search for a string or a regular expression"
        print " ^K (F9)      Cut the current line and store it in the cutbuffer"
        print " ^U (F10)     Uncut from the cutbuffer into the current line"
        print " ^C (F11)     Display the position of the cursor"
        print " ^Y (PgUp)    Go one screenful up"
        print " ^V (PgDn)    Go one screenful down"
        print ""
        print " Press any key to continue"
        sys.exec("dd bs=1 count=1 2>/dev/null > /dev/null")

    proc process_key(code, ch):
        self.message = ""
        if code == 24: # ^X
            if self.buffer.modified:
                let ans = self.prompt("Save modified buffer? (y/n): ")
                if ans == "y" or ans == "Y":
                    self.save()
            self.running = false
        elif code == 15: # ^O
            self.save()
        elif code == 18: # ^R
            let path = self.prompt("File to insert [from ./]: ")
            if path != "" and io.exists(path):
                let content = io.readfile(path)
                if content != nil:
                    let lns = split(content, chr(10))
                    for i in range(len(lns)):
                        let l = lns[i]
                        # For simplicity, we'll insert as new lines after current
                        let new_lines = []
                        for j in range(self.buffer.cy + 1):
                            push(new_lines, self.buffer.lines[j])
                        push(new_lines, l)
                        for j in range(len(self.buffer.lines) - self.buffer.cy - 1):
                            push(new_lines, self.buffer.lines[self.buffer.cy + 1 + j])
                        self.buffer.lines = new_lines
                        self.buffer.cy = self.buffer.cy + 1
                    self.message = "Inserted " + str(len(lns)) + " lines"
            else:
                self.message = "File not found"
        elif code == 7: # ^G
            self.show_help()
        elif code == 23: # ^W
            self.search()
        elif code == 11: # ^K
            self.buffer.cut_line()
        elif code == 21: # ^U
            self.buffer.uncut_text()
        elif code == 3: # ^C
            self.message = "line " + str(self.buffer.cy + 1) + "/" + str(len(self.buffer.lines)) + ", col " + str(self.buffer.cx + 1) + "/" + str(len(self.buffer.lines[self.buffer.cy]) + 1)
        elif code == 25: # ^Y (Prev Page)
            for i in range(self.rows - 4):
                self.buffer.move_up()
        elif code == 22: # ^V (Next Page)
            for i in range(self.rows - 4):
                self.buffer.move_down()
        elif code == 13 or code == 10: # Enter
            self.buffer.do_enter()
        elif code == 127 or code == 8: # Backspace
            self.buffer.do_backspace()
        elif code == 27: # Esc
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
                    elif d == 51: # Delete key (Esc[3~)
                        sys.exec("dd bs=1 count=1 2>/dev/null > /dev/null")
                        self.buffer.do_delete()
                    elif d == 53: # PgUp (Esc[5~)
                        sys.exec("dd bs=1 count=1 2>/dev/null > /dev/null")
                        for i in range(self.rows - 4):
                            self.buffer.move_up()
                    elif d == 54: # PgDn (Esc[6~)
                        sys.exec("dd bs=1 count=1 2>/dev/null > /dev/null")
                        for i in range(self.rows - 4):
                            self.buffer.move_down()
        elif code >= 32 and code <= 126:
            self.buffer.insert_char(ch)

    proc loop():
        sys.exec("stty raw -echo")
        while self.running:
            self.draw()
            sys.exec("dd bs=1 count=1 2>/dev/null > /tmp/sage_nano_key")
            let k = io.readfile("/tmp/sage_nano_key")
            if k == nil or len(k) == 0:
                continue
            self.process_key(ord(k[0]), k[0])
        sys.exec("stty icanon echo")
        sys.exec("clear")

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
