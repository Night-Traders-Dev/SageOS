import sys
import io

let STATE_FILE = "/tmp/sage_ui_state"

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

proc get_temp():
    # Try thermal zones first (common on ARM/Raspberry Pi/Orange Pi)
    sys.exec("cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null > /tmp/worker_temp")
    let t = io.readfile("/tmp/worker_temp")
    
    # Try k10temp (AMD CPUs)
    if t == nil or len(t) == 0:
        sys.exec("cat /sys/class/hwmon/hwmon*/temp1_input 2>/dev/null | head -n 1 > /tmp/worker_temp")
        t = io.readfile("/tmp/worker_temp")
    
    # Try coretemp (Intel CPUs)
    if t == nil or len(t) == 0:
        sys.exec("cat /sys/class/hwmon/hwmon*/temp2_input 2>/dev/null | head -n 1 > /tmp/worker_temp")
        t = io.readfile("/tmp/worker_temp")

    if t == nil or len(trim(t)) == 0:
        return "N/A"
    
    let mc = tonumber(trim(t))
    if mc == nil:
        return "N/A"
    
    # Values might be in millidegrees or degrees depending on source
    # Most sysfs paths use millidegrees (e.g. 38000 = 38C)
    if mc > 1000:
        mc = mc / 1000
    
    return str((mc * 9 / 5 + 32) | 0) + "°F"

proc get_time():
    sys.exec("date +%H:%M:%S > /tmp/worker_time")
    return io.readfile("/tmp/worker_time")

proc get_term_size():
    sys.exec("stty size 2>/dev/null > /tmp/worker_size")
    let size = trim(io.readfile("/tmp/worker_size"))
    if size == nil or len(size) == 0:
        return "24 80"
    return size

proc get_git_info():
    sys.exec("git rev-parse --is-inside-work-tree > /dev/null 2>&1")
    if sys.exec("test $? -ne 0") == 0:
        return ""
    
    sys.exec("git branch --show-current > /tmp/worker_git_branch 2>/dev/null")
    let b = trim(io.readfile("/tmp/worker_git_branch"))
    
    sys.exec("git status --porcelain > /tmp/worker_git_status 2>/dev/null")
    let st = trim(io.readfile("/tmp/worker_git_status"))
    let dirty = ""
    if len(st) > 0:
        dirty = "*"
    
    if b != "":
        return b + dirty
    return ""

while true:
    let time = trim(get_time())
    let temp = get_temp()
    let size = get_term_size()
    let git = get_git_info()
    
    let content = time + "|" + temp + "|" + size + "|" + git
    io.writefile(STATE_FILE + ".tmp", content)
    sys.exec("mv " + STATE_FILE + ".tmp " + STATE_FILE)
    sys.sleep(1)
