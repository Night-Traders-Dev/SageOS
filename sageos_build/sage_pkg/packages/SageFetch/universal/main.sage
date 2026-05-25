import sys
import io
import string

# ANSI Colors
let ESC = chr(27)
let RESET = ESC + "[0m"
let BOLD = ESC + "[1m"
let GREEN = ESC + "[32m"
let BLUE = ESC + "[34m"
let CYAN = ESC + "[36m"
let RED = ESC + "[31m"
let MAGENTA = ESC + "[35m"
let YELLOW = ESC + "[33m"

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

proc split_lines(s):
    let lines = []
    if s == nil:
        return lines
    let current = ""
    for i in range(len(s)):
        if s[i] == "\n":
            push(lines, current)
            current = ""
        else:
            current = current + s[i]
    if len(current) > 0:
        push(lines, current)
    return lines

proc read_proc_file(path):
    let tmp = "/tmp/sage_proc_tmp"
    sys.exec("cat " + path + " > " + tmp + " 2>/dev/null")
    let content = io.readfile(tmp)
    return content

proc get_os_info():
    let content = io.readfile("/etc/os-release")
    if content == nil:
        return "Linux"
    let lines = split_lines(content)
    for line in lines:
        if len(line) > 12:
            if string.substr(line, 0, 12) == "PRETTY_NAME=":
                let name = string.substr(line, 12, len(line) - 12)
                # Remove quotes
                if len(name) > 1 and name[0] == "\"":
                    return string.substr(name, 1, len(name) - 2)
                return name
    return "Linux"

proc get_kernel_info():
    let content = read_proc_file("/proc/version")
    if content == nil or len(content) == 0:
        return sys.platform
    let parts = []
    let current = ""
    for i in range(len(content)):
        if content[i] == " ":
            if len(current) > 0:
                push(parts, current)
            current = ""
        else:
            current = current + content[i]
    if len(current) > 0:
        push(parts, current)
    
    if len(parts) > 2:
        return parts[2]
    return sys.platform

proc get_uptime_info():
    let content = read_proc_file("/proc/uptime")
    if content == nil or len(content) == 0:
        return "unknown"
    let i = 0
    let seconds_str = ""
    while i < len(content) and content[i] != " ":
        seconds_str = seconds_str + content[i]
        i = i + 1
    
    let dot = -1
    for j in range(len(seconds_str)):
        if seconds_str[j] == ".":
            dot = j
    
    let int_part = seconds_str
    if dot != -1:
        int_part = string.substr(seconds_str, 0, dot)
    
    let total_seconds = tonumber(int_part)
    if total_seconds == nil:
        return "unknown"

    let hours = (total_seconds / 3600) | 0
    let minutes = ((total_seconds - hours * 3600) / 60) | 0
    
    let result = ""
    if hours > 0:
        result = str(hours) + " hours, "
    result = result + str(minutes) + " mins"
    return result

proc get_shell_info():
    let shell = sys.getenv("SHELL")
    if shell == nil:
        # Try to detect via parent process
        sys.exec("ps -p $PPID -o comm= > /tmp/sage_shell_tmp")
        shell = trim(io.readfile("/tmp/sage_shell_tmp"))
        if shell == "" or shell == nil:
            return "unknown"
    
    # Just show the basename
    if string.contains(shell, "/"):
        let last_slash = -1
        for i in range(len(shell)):
            if shell[i] == "/":
                last_slash = i
        shell = string.substr(shell, last_slash + 1, len(shell) - last_slash - 1)
    
    return shell

proc get_cpu_info():
    # Try lscpu first for ARM models
    sys.exec("lscpu > /tmp/sage_lscpu 2>/dev/null")
    let lscpu_content = io.readfile("/tmp/sage_lscpu")
    if lscpu_content != nil and len(lscpu_content) > 0:
        let lines = split_lines(lscpu_content)
        for line in lines:
            if string.contains(line, "Model name:"):
                let parts = split(line, ":")
                if len(parts) > 1:
                    return trim(parts[1])

    # Fallback to /proc/cpuinfo
    let content = read_proc_file("/proc/cpuinfo")
    if content == nil or len(content) == 0:
        return "unknown"
    let lines = split_lines(content)
    for line in lines:
        if len(line) > 13:
            if string.substr(line, 0, 13) == "model name\t: ":
                return string.substr(line, 13, len(line) - 13)
        if len(line) > 11:
            if string.substr(line, 0, 11) == "Model\t\t: ":
                return string.substr(line, 11, len(line) - 11)
        # ARM-specific: decode CPU part number to a human-readable name
        if string.contains(line, "CPU part"):
            let parts = split(line, ":")
            if len(parts) > 1:
                let part = trim(parts[1])
                return arm_part_name(part)

    return "Generic CPU"

# Translate ARM CPU part numbers (from /proc/cpuinfo) to core names.
# Part numbers are defined in the ARM Architecture Reference Manuals.
proc arm_part_name(part):
    if part == "0xd03": 
        return "ARM Cortex-A53"
    if part == "0xd04": 
        return "ARM Cortex-A35"
    if part == "0xd05": 
        return "ARM Cortex-A55"
    if part == "0xd06": 
        return "ARM Cortex-A65"
    if part == "0xd07": 
        return "ARM Cortex-A57"
    if part == "0xd08": 
        return "ARM Cortex-A72"
    if part == "0xd09": 
        return "ARM Cortex-A73"
    if part == "0xd0a": 
        return "ARM Cortex-A75"
    if part == "0xd0b": 
        return "ARM Cortex-A76"
    if part == "0xd0c": 
        return "ARM Neoverse-N1"
    if part == "0xd0d": 
        return "ARM Cortex-A77"
    if part == "0xd0e": 
        return "ARM Cortex-A76AE"
    if part == "0xd40": 
        return "ARM Neoverse-V1"
    if part == "0xd41": 
        return "ARM Cortex-A78"
    if part == "0xd42": 
        return "ARM Cortex-A78AE"
    if part == "0xd44": 
        return "ARM Cortex-X1"
    if part == "0xd46": 
        return "ARM Cortex-A510"
    if part == "0xd47": 
        return "ARM Cortex-A710"
    if part == "0xd48": 
        return "ARM Cortex-X2"
    if part == "0xd4b": 
        return "ARM Cortex-A78C"
    if part == "0xd4c": 
        return "ARM Neoverse-V2"
    if part == "0xd4d": 
        return "ARM Cortex-A715"
    if part == "0xd4e": 
        return "ARM Cortex-X3"
    if part == "0xd80": 
        return "ARM Cortex-A520"
    if part == "0xd81": 
        return "ARM Cortex-A720"
    if part == "0xd84": 
        return "ARM Cortex-X4"
    # Qualcomm Krait / Kryo
    if part == "0x04d": 
        return "Qualcomm Krait 300"
    if part == "0x06f": 
        return "Qualcomm Krait 400"
    if part == "0x201": 
        return "Qualcomm Kryo 260 Silver"
    if part == "0x205": 
        return "Qualcomm Kryo 260 Gold"
    if part == "0x211": 
        return "Qualcomm Kryo 360 Silver"
    if part == "0x215": 
        return "Qualcomm Kryo 360 Gold"
    if part == "0x803": 
        return "Qualcomm Kryo 385 Silver"
    if part == "0x804": 
        return "Qualcomm Kryo 385 Gold"
    if part == "0x805": 
        return "Qualcomm Kryo 485 Silver"
    if part == "0x806": 
        return "Qualcomm Kryo 485 Gold"
    # Apple
    if part == "0x022": 
        return "Apple Icestorm"
    if part == "0x023": 
        return "Apple Firestorm"
    if part == "0x024": 
        return "Apple Blizzard"
    if part == "0x025": 
        return "Apple Avalanche"
    # Unknown
    return "ARM Part " + part

proc get_gpu_info():
    # 1. Try lspci (common for x86)
    sys.exec("lspci 2>/dev/null | grep -iE 'vga|3d|2d' > /tmp/sage_gpu")
    let out = io.readfile("/tmp/sage_gpu")
    if out != nil and len(out) > 0:
        let lines = split_lines(out)
        for line in lines:
            if string.contains(line, "controller: "):
                let parts = split(line, "controller: ")
                if len(parts) > 1:
                    return trim(parts[1])
            if string.contains(line, ": "):
                let parts = split(line, ": ")
                if len(parts) > 1:
                    return trim(parts[1])
        return trim(lines[0])

    # 2. Try DRM (common for ARM/Integrated)
    sys.exec("grep -v '^$' /sys/class/drm/card0/device/uevent 2>/dev/null | grep 'DRIVER=' > /tmp/sage_gpu_drm")
    let drm = io.readfile("/tmp/sage_gpu_drm")
    if drm != nil and len(drm) > 0:
        if string.contains(drm, "DRIVER="):
            let parts = split(trim(drm), "=")
            if len(parts) > 1:
                let drv = parts[1]
                if drv == "panfrost":
                    return "ARM Mali (Panfrost)"
                if drv == "lima":
                    return "ARM Mali (Lima)"
                if drv == "etnaviv":
                    return "Vivante (Etnaviv)"
                if drv == "msm":
                    return "Qualcomm Adreno"
                if drv == "vc4":
                    return "Broadcom VC4"
                if drv == "v3d":
                    return "Broadcom V3D"
                if drv == "i915":
                    return "Intel Graphics"
                if drv == "amdgpu":
                    return "AMD Radeon"
                if drv == "nouveau":
                    return "NVIDIA (Nouveau)"
                return drv + " GPU"

    # 3. Last fallback: check device tree (ARM)
    sys.exec("cat /proc/device-tree/model 2>/dev/null > /tmp/sage_dt")
    let dt = io.readfile("/tmp/sage_dt")
    if dt != nil and string.contains(dt, "Orange Pi"):
        if string.contains(dt, "5"):
            return "ARM Mali-G610 MP4"
        if string.contains(dt, "3"):
            return "ARM Mali-G52"
        return "ARM Mali"

    return "unknown"

proc get_mem_info():
    let content = read_proc_file("/proc/meminfo")
    if content == nil or len(content) == 0:
        return "unknown"
    let lines = split_lines(content)
    let total_kb = 0
    let available_kb = 0
    
    for line in lines:
        let t_line = trim(line)
        if len(t_line) > 9:
            if string.substr(t_line, 0, 9) == "MemTotal:":
                let parts = []
                let curr = ""
                for c in range(len(t_line)):
                    if t_line[c] == " " or t_line[c] == "\t":
                        if len(curr) > 0:
                            push(parts, curr)
                        curr = ""
                    else:
                        curr = curr + t_line[c]
                if len(curr) > 0:
                    push(parts, curr)
                if len(parts) > 1:
                    total_kb = tonumber(parts[1])
        
        if len(t_line) > 13:
            if string.substr(t_line, 0, 13) == "MemAvailable:":
                let parts = []
                let curr = ""
                for c in range(len(t_line)):
                    if t_line[c] == " " or t_line[c] == "\t":
                        if len(curr) > 0:
                            push(parts, curr)
                        curr = ""
                    else:
                        curr = curr + t_line[c]
                if len(curr) > 0:
                    push(parts, curr)
                if len(parts) > 1:
                    available_kb = tonumber(parts[1])

    if total_kb > 0:
        let used_kb = total_kb - available_kb
        return str(used_kb / 1024 | 0) + "MiB / " + str(total_kb / 1024 | 0) + "MiB"
    return "unknown"

proc get_user_host():
    let user = sys.getenv("USER")
    if user == nil:
        user = "user"
    let host = io.readfile("/etc/hostname")
    if host == nil:
        host = "sage"
    return GREEN + BOLD + user + RESET + "@" + GREEN + BOLD + trim(host) + RESET

# ASCII Art for Sage
let logo = [
    GREEN + "      .-------.       " + RESET,
    GREEN + "    .'  _____  '.     " + RESET,
    GREEN + "   /   /     \\   \\    " + RESET,
    GREEN + "  |   |       |   |   " + RESET,
    GREEN + "  |   |  " + YELLOW + "SAGE" + GREEN + "  |   |   " + RESET,
    GREEN + "  |   |       |   |   " + RESET,
    GREEN + "   \\   \\_____/   /    " + RESET,
    GREEN + "    '.         .'     " + RESET,
    GREEN + "      '-------'       ",
    GREEN + "         ||           " + RESET,
    GREEN + "         ||           " + RESET
]

let user_host_line = get_user_host()
let dash = "------------------------"

let os_info = get_os_info()
let kernel_info = get_kernel_info()
let uptime_info = get_uptime_info()
let shell_info = get_shell_info()
let cpu_info = get_cpu_info()
let gpu_info = get_gpu_info()
let mem_info = get_mem_info()

let info = [
    user_host_line,
    dash,
    CYAN + BOLD + "OS:      " + RESET + str(os_info),
    CYAN + BOLD + "Kernel:  " + RESET + str(kernel_info),
    CYAN + BOLD + "Uptime:  " + RESET + str(uptime_info),
    CYAN + BOLD + "Shell:   " + RESET + str(shell_info),
    CYAN + BOLD + "CPU:     " + RESET + str(cpu_info),
    CYAN + BOLD + "GPU:     " + RESET + str(gpu_info),
    CYAN + BOLD + "Memory:  " + RESET + str(mem_info),
    "",
    "   " + ESC + "[40m  " + ESC + "[41m  " + ESC + "[42m  " + ESC + "[43m  " + ESC + "[44m  " + ESC + "[45m  " + ESC + "[46m  " + ESC + "[47m  " + RESET
]

let max_lines = len(logo)
if len(info) > max_lines:
    max_lines = len(info)

for i in range(max_lines):
    let line = ""
    if i < len(logo):
        line = logo[i]
    else:
        line = "                        " # 24 spaces
    
    line = line + "   "
    
    if i < len(info):
        line = line + str(info[i])
    
    print line

print ""
