#include "value.h"
#include "module.h"
#include "env.h"
#include "io.h"
#include "console.h"
#include "scheduler.h"
#include "serial.h"
#include "keyboard.h"
#include "ata.h"
#include "sdhci.h"
#include "net.h"
#include "acpi.h"
#include "wifi_qca6174.h"
#include "pci.h"
#include "smp.h"
#include "swap.h"
#include "sysinfo.h"
#include "idt.h"
#include "bootinfo.h"
#include "vfs.h"
#include "shell.h"
#include "version.h"
#include <string.h>
#include <stdio.h>

// External kernel functions
extern void ata_timer_tick(void);
extern void console_periodic_flip(void);
extern void timer_irq(void);
extern SageOSBootInfo* console_boot_info(void);
extern uint32_t console_get_fg(void);

// --- Input Helpers ---

#define SAGE_KEY_UP      1001
#define SAGE_KEY_DOWN    1002
#define SAGE_KEY_RIGHT   1003
#define SAGE_KEY_LEFT    1004
#define SAGE_KEY_HOME    1005
#define SAGE_KEY_END     1006
#define SAGE_KEY_DELETE  1008

static int key_event_code(const KeyEvent *ev) {
    if (!ev || !ev->pressed) return -1;
    if (ev->extended) {
        switch (ev->scancode) {
        case 0x48: return SAGE_KEY_UP;
        case 0x50: return SAGE_KEY_DOWN;
        case 0x4D: return SAGE_KEY_RIGHT;
        case 0x4B: return SAGE_KEY_LEFT;
        case 0x47: return SAGE_KEY_HOME;
        case 0x4F: return SAGE_KEY_END;
        case 0x53: return SAGE_KEY_DELETE;
        default: return -1;
        }
    }
    if (ev->ascii) return (int)(unsigned char)ev->ascii;
    return -1;
}

// --- OS Natives ---

static Value n_os_version(int argCount, Value* args) {
    (void)argCount; (void)args;
    return val_string(SAGEOS_VERSION);
}

static Value n_os_write_char(int argCount, Value* args) {
    if (argCount >= 1 && IS_NUMBER(args[0])) {
        console_putc((char)AS_NUMBER(args[0]));
    }
    return val_nil();
}

static Value n_os_write_str(int argCount, Value* args) {
    if (argCount >= 1 && IS_STRING(args[0])) {
        console_write(AS_STRING(args[0]));
    }
    return val_nil();
}

static Value n_os_read_char(int argCount, Value* args) {
    (void)argCount; (void)args;
    KeyEvent ev;
    for (;;) {
        if (!keyboard_wait_event(&ev)) return val_number(-1.0);
        int code = key_event_code(&ev);
        if (code >= 0) return val_number((double)code);
    }
}

static Value n_os_read_key(int argCount, Value* args) {
    return n_os_read_char(argCount, args);
}

static Value n_os_poll_char(int argCount, Value* args) {
    (void)argCount; (void)args;
    KeyEvent ev;
    if (keyboard_poll_any_event(&ev) && ev.pressed && ev.ascii) {
        return val_number((double)(unsigned char)ev.ascii);
    }
    return val_number(-1.0);
}

static Value n_os_poll_key(int argCount, Value* args) {
    (void)argCount; (void)args;
    KeyEvent ev;
    if (!keyboard_poll_any_event(&ev)) return val_number(-1.0);
    return val_number((double)key_event_code(&ev));
}

static Value n_os_set_color_hex(int argCount, Value* args) {
    if (argCount >= 1 && IS_NUMBER(args[0])) {
        console_set_fg((uint32_t)AS_NUMBER(args[0]));
    }
    return val_nil();
}

static Value n_os_get_color(int argCount, Value* args) {
    (void)argCount; (void)args;
    return val_number((double)console_get_fg());
}

static uint32_t g_input_start_row = 0;
static uint32_t g_input_start_col = 0;

static Value n_os_input_begin(int argCount, Value* args) {
    (void)argCount; (void)args;
    console_get_cursor(&g_input_start_row, &g_input_start_col);
    return val_nil();
}

static void serial_raw(const char *s) {
    while (*s) serial_putc(*s++);
}
static void serial_dec(uint32_t v) {
    char tmp[12]; int n = 0;
    if (v == 0) { serial_putc('0'); return; }
    while (v && n < (int)sizeof(tmp)) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n > 0) serial_putc(tmp[--n]);
}

static Value n_os_line_redraw(int argCount, Value* args) {
    if (argCount < 4) return val_nil();
    const char *line = AS_STRING(args[0]);
    int pos = (int)AS_NUMBER(args[1]);
    int erase_len = (int)AS_NUMBER(args[2]);
    const char *hint = AS_STRING(args[3]);
    
    int line_len = strlen(line);
    int hint_len = (hint && *hint && strncmp(hint, line, line_len) == 0) ? strlen(hint) : 0;
    int visible_len = hint_len > line_len ? hint_len : line_len;
    
    int saved_echo = console_get_serial_echo();
    console_set_serial_echo(0);
    console_set_cursor(g_input_start_row, g_input_start_col);
    console_write(line);
    if (hint_len > line_len) {
        uint32_t old = console_get_fg();
        console_set_fg(0x606060);
        console_write(hint + line_len);
        console_set_fg(old);
    }
    if (erase_len > visible_len) {
        for (int i = 0; i < erase_len - visible_len; i++) console_putc(' ');
    }
    uint32_t off = g_input_start_col + (uint32_t)pos;
    console_set_cursor(g_input_start_row + off / console_cols(), off % console_cols());
    console_set_serial_echo(saved_echo);
    
    // Serial redraw
    serial_putc('\r');
    serial_raw("\033[0K");
    serial_raw("root@sageos:/# ");
    serial_raw(line);
    if (hint_len > line_len) { serial_raw("\033[90m"); serial_raw(hint + line_len); serial_raw("\033[0m"); }
    serial_putc('\r');
    serial_raw("\033[");
    serial_dec((uint32_t)(15 + pos + 1)); // 15 is len of prompt
    serial_putc('G');
    
    return val_nil();
}

static Value n_os_strlen(int argCount, Value* args) {
    if (argCount < 1 || !IS_STRING(args[0])) return val_number(0);
    return val_number((double)strlen(AS_STRING(args[0])));
}

static Value n_os_char_at(int argCount, Value* args) {
    if (argCount < 2 || !IS_STRING(args[0])) return val_number(-1.0);
    const char* s = AS_STRING(args[0]);
    int idx = (int)AS_NUMBER(args[1]);
    if (idx < 0 || idx >= (int)strlen(s)) return val_number(-1.0);
    return val_number((double)(unsigned char)s[idx]);
}

static Value n_os_substr(int argCount, Value* args) {
    if (argCount < 3 || !IS_STRING(args[0])) return val_string("");
    const char* s = AS_STRING(args[0]);
    int from = (int)AS_NUMBER(args[1]);
    int to = (int)AS_NUMBER(args[2]);
    int len = strlen(s);
    if (from < 0) from = 0; if (to > len) to = len;
    if (from >= to) return val_string("");
    return val_string_len(s + from, to - from);
}

static Value n_os_chr(int argCount, Value* args) {
    if (argCount < 1) return val_string("");
    char buf[2]; buf[0] = (char)AS_NUMBER(args[0]); buf[1] = 0;
    return val_string(buf);
}

static Value n_os_array_push(int argCount, Value* args) {
    if (argCount < 2 || !IS_ARRAY(args[0])) return val_nil();
    array_push(&args[0], args[1]);
    return val_nil();
}

static Value n_os_shell_suggestion(int argCount, Value* args) {
    if (argCount < 1 || !IS_STRING(args[0])) return val_string("");
    return val_string(shell_suggestion(AS_STRING(args[0])));
}

static Value n_os_shell_completion_common(int argCount, Value* args) {
    if (argCount < 1 || !IS_STRING(args[0])) return val_string("");
    return val_string(shell_completion_common_prefix(AS_STRING(args[0])));
}

static Value n_os_shell_print_completions(int argCount, Value* args) {
    if (argCount < 1 || !IS_STRING(args[0])) return val_nil();
    shell_print_completions(AS_STRING(args[0]));
    return val_nil();
}

static Value n_os_console_clear(int argCount, Value* args) {
    (void)argCount; (void)args;
    console_clear(); return val_nil();
}

// --- Module Registration ---

void register_sageos_natives(ModuleCache* cache) {
    // We register these globally as well for the shell script compatibility
    Environment* env = g_global_env;

    env_define(env, "os_write_char", 13, val_native(n_os_write_char));
    env_define(env, "os_write_str", 12, val_native(n_os_write_str));
    env_define(env, "os_read_char", 12, val_native(n_os_read_char));
    env_define(env, "os_read_key", 11, val_native(n_os_read_key));
    env_define(env, "os_poll_char", 12, val_native(n_os_poll_char));
    env_define(env, "os_poll_key", 11, val_native(n_os_poll_key));
    env_define(env, "os_set_color_hex", 16, val_native(n_os_set_color_hex));
    env_define(env, "os_get_color", 12, val_native(n_os_get_color));
    env_define(env, "os_input_begin", 14, val_native(n_os_input_begin));
    env_define(env, "os_line_redraw", 14, val_native(n_os_line_redraw));
    env_define(env, "os_strlen", 9, val_native(n_os_strlen));
    env_define(env, "os_char_at", 10, val_native(n_os_char_at));
    env_define(env, "os_substr", 9, val_native(n_os_substr));
    env_define(env, "os_chr", 6, val_native(n_os_chr));
    env_define(env, "os_array_push", 13, val_native(n_os_array_push));
    env_define(env, "os_shell_suggestion", 18, val_native(n_os_shell_suggestion));
    env_define(env, "os_shell_completion_common", 25, val_native(n_os_shell_completion_common));
    env_define(env, "os_shell_print_completions", 25, val_native(n_os_shell_print_completions));
    env_define(env, "os_console_clear", 16, val_native(n_os_console_clear));
    env_define(env, "os_version_string", 17, val_native(n_os_version));

    // Also register the 'os' module
    Module* os = create_native_module(cache, "os");
    env_define(os->env, "write_str", 9, val_native(n_os_write_str));
    // ... add more to 'os' module if needed ...
}
