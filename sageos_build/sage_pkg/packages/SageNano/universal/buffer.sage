

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
