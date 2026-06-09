# scripts/chroot_stubs.sage
# Comprehensive FFI stubs for SageOS kernel functions when running in a host container.

import sys

# Global state for input buffering
let input_buffer = ""
let input_index = 0

# --- I/O Stubs ---

proc os_read_key():
    if input_index >= len(input_buffer):
        let raw = input()
        if raw == nil:
            return 10
        end
        input_buffer = raw + "\n"
        input_index = 0
    end
    
    if len(input_buffer) == 0:
        return 0
    end

    let key = input_buffer[input_index]
    input_index = input_index + 1
    
    return ord(key)
end

proc os_write_char(c):
    # Stub
end

proc os_write_str(s):
    print(s)
end

proc os_strlen(s):
    if s == nil: return 0 end
    return len(s)
end

proc os_num_to_str(n):
    return str(n)
end

proc os_substr(s, start, length):
    if s == nil: return "" end
    let slen = len(s)
    let end_idx = start + length
    if end_idx > slen: end_idx = slen end
    return s[start:end_idx]
end

proc os_chr(c):
    return chr(c)
end

proc os_streq(a, b):
    if a == b: return 1 end
    return 0
end

proc os_char_at(s, i):
    if s == nil: return 0 end
    if i < 0 or i >= len(s): return 0 end
    return ord(s[i])
end

# --- System Info Stubs ---

proc os_battery_percent(): return 100 end
proc os_cpu_percent(): return 5 end
proc os_ram_used_mb(): return 512 end
proc os_ram_total_mb(): return 16384 end
proc os_battery_info(): return "Battery: 100% (AC)" end
proc os_cpu_name(): return "Host CPU" end
proc os_arch(): return "x86_64" end
proc os_build(): return "SageContainer" end
proc os_host(): return "Linux" end
proc os_is_qemu(): return 0 end
proc os_sysinfo(): 
    print("SageOS Container v0.9.0")
    print("Running on: " + os_host() + " " + os_arch())
end
proc os_version_string(): return "0.9.0-container" end
proc os_uptime_str(): return "0:00:01" end
proc os_ram_used_str(): return "512MB" end
proc os_ram_total_str(): return "16GB" end

# --- ACPI / Hardware Stubs ---
proc os_acpi_battery(): return 0 end
proc os_acpi_fadt(): return 0 end
proc os_acpi_lid(): return 0 end
proc os_acpi_madt(): return 0 end
proc os_acpi_summary(): return "ACPI not available" end
proc os_acpi_tables(): return 0 end
proc os_pci_info(): print("PCI not available") end
proc os_sdhci_info(): print("SDHCI not available") end
proc os_smp_info(): print("SMP: 1 Core") end
proc os_smp_cpu_count(): return 1 end
proc os_smp_boot_aps(): return 0 end
proc os_sched_cpu_count(): return 1 end
proc os_sched_thread_count(): return 1 end
proc os_timer_info(): return "Timer: Host Clock" end

# --- Framebuffer Stubs ---
proc os_fb_available(): return 0 end
proc os_fb_base_str(): return "0" end
proc os_fb_height_str(): return "0" end
proc os_fb_width_str(): return "0" end
proc os_fb_pps_str(): return "0" end

# --- Dmesg Stubs ---
proc os_dmesg_log(s): print("[kernel] " + s) end
proc os_dmesg_get_char(i): return 0 end
proc os_dmesg_get_head(): return 0 end
proc os_dmesg_get_size(): return 0 end
proc os_dmesg_get_total(): return 0 end

# --- Shell / UI Stubs ---
proc os_get_color(): return 7 end
proc os_set_color(c): end
proc os_set_color_hex(h): end
proc os_console_clear(): print("\033[2J\033[H") end
proc os_input_begin(): end
proc os_input_backend(): return "host" end

proc os_line_redraw(line, pos, old_len, suggestion):
end

# --- Shell Completion / Logic Stubs ---
proc os_shell_completion_common(path, prefix): return "" end

proc os_shell_exec(cmd):
    if cmd == "exit" or cmd == "quit":
        sys.exit(0)
    end
    print("Container Exec: " + cmd)
end

proc os_shell_print_completions(path, prefix): end
proc os_shell_suggestion(line, pos): return "" end
proc os_path_exists(p):
    return 0
end

# --- Lifecycle Stubs ---
proc os_halt(): sys.exit(0) end
proc os_reboot(): sys.exit(0) end
proc os_shutdown(): sys.exit(0) end
proc os_suspend(): end
proc os_qemu_exit(n): sys.exit(n) end

# --- VM / Bytecode Stubs ---
proc os_sage_exec(bc): print("VM Exec Request") end

proc os_array_push(arr, val):
    push(arr, val)
end

# --- Storage Stubs ---
proc os_swap_available(): return 0 end
proc os_swap_total_mb(): return 0 end
proc os_swap_used_mb(): return 0 end

# --- Other missing stubs ---
proc os_is_paging_enabled(): return 1 end
proc os_get_kernel_version(): return "0.9.0" end

# --- Global Namespace Injection ---
proc exit(n):
    sys.exit(n)
end
