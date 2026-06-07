#include "value.h"
#include "module.h"
#include "env.h"
#include "io.h"
#include "parser.h"
#include "lexer.h"
#include "interpreter.h"
#include "repl.h"
#include "sage_libc_shim.h"
#include "console.h"
#include "scheduler.h"
#include "scheduler_ipc_ext.h"
#include "ipc.h"
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
#include "dmesg.h"
#include "metal_vm.h"
#include "ast.h"
#include "gc.h"
#include "sage_shell_entry.h"
#include <string.h>
#include <stdio.h>

// External kernel functions
extern void ata_timer_tick(void);
extern void console_periodic_flip(void);
extern void timer_irq(void);
extern SageOSBootInfo* console_boot_info(void);
extern uint32_t console_get_fg(void);

// --- Global Interpreter Lock (GIL) ---
static thread_t *g_gil_owner = NULL;

void sage_gil_acquire(void) {
    thread_t *curr = sched_current_thread();
    while (g_gil_owner != NULL && g_gil_owner != curr) {
        sched_yield();
    }
    g_gil_owner = curr;
}

void sage_gil_release(void) {
    if (g_gil_owner == sched_current_thread()) {
        g_gil_owner = NULL;
    }
}

static Env* g_sage_env = NULL;
static ModuleCache* g_sage_cache = NULL;

void register_sageos_natives(ModuleCache* cache);

void sage_repl_init(void) {
    sage_gil_acquire();
    if (!g_sage_env) {
        extern int g_repl_mode;
        g_repl_mode = 1;
        g_sage_cache = create_module_cache();
        add_search_path(g_sage_cache, "/lib/sagelang");
        add_search_path(g_sage_cache, "/etc/sagelang");
        add_search_path(g_sage_cache, ".");
        global_module_cache = g_sage_cache;
        g_sage_env = env_create(NULL);
        register_sageos_natives(g_sage_cache);
        init_stdlib(g_sage_env);
    }
    sage_gil_release();
}

void sage_execute_source(const char* source, const char* name) {
    if (!source) {
        return;
    }

    sage_gil_acquire();

    int jmp_result = setjmp(g_repl_error_jmp);

    if (jmp_result == 0) {
        init_lexer(source, name);
        parser_init();

        Stmt* first_program = NULL;
        Stmt* last_program = NULL;
        while (1) {
            Stmt* program = parse();

            if (!program) {
                break;
            }

            interpret(program, g_sage_env);

            // Find the end of the parsed program statement chain
            Stmt* end = program;
            while (end->next != NULL) {
                end = end->next;
            }

            if (first_program == NULL) {
                first_program = program;
            } else {
                last_program->next = program;
            }
            last_program = end;
        }

        if (first_program != NULL) {
            free_stmt(first_program);
        }

    } else {
        console_write("\n[RUNTIME MANAGER EXCEPTION CAUGHT!]\n");
    }

    sage_gil_release();
}

__attribute__((noinline)) void sage_execute(const char* mod) {
    console_write("\nsage_execute entry: ");
    if (mod) console_write(mod);
    else console_write("NULL");
    
    sage_repl_init();
    console_write("\nsage_execute: after sage_repl_init");
    
    if (mod == NULL || *mod == 0) {
        console_write("\nsage_execute: empty mod, returning");
        return;
    }
    
    // Heuristic to decide if this is a file path or direct code
    bool is_path = (mod[0] == '/');
    console_write("\nsage_execute: is_path=");
    console_write(is_path ? "true" : "false");
    
    if (is_path) {
        console_write("\nsage_execute: handling path directly");
        VfsStat *st = (VfsStat*)malloc(sizeof(VfsStat));
        if (!st) return;
        int err = vfs_stat(mod, st);
        if (err != VFS_OK) {
            console_write("\nsage: vfs_stat failed for ");
            console_write(mod);
            free(st);
            return;
        }
        
        console_write("\nsage_execute: vfs_stat SUCCESS. size=");
        console_u32((uint32_t)st->size);

        char* source = (char*)malloc((size_t)st->size + 1);
        if (!source) {
            console_write("\nsage: out of memory reading ");
            console_write(mod);
            free(st);
            return;
        }

        int read_bytes = vfs_read(mod, 0, source, (size_t)st->size);
        source[st->size] = 0;
        console_write("\nsage_execute: vfs_read returned ");
        console_u32((uint32_t)read_bytes);

        if (read_bytes >= 4 && source[0] == 'S' && source[1] == 'G' && source[2] == 'V' && source[3] == 'M') {
            console_write("\nsage: executing bytecode artifact: ");
            console_write(mod);
            // Run as bytecode via MetalVM
            MetalVM *vm = (MetalVM*)malloc(sizeof(MetalVM));
            if (vm) {
                metal_vm_init(vm);
                vm->write_char = bridge_write_char;
                vm->read_char = bridge_read_char;
                register_natives(vm);
                if (metal_vm_load_binary(vm, (const uint8_t*)source, (int)read_bytes)) {
                    metal_vm_run(vm);
                } else {
                    console_write("\nsage: error loading bytecode artifact from disk: ");
                    console_write(mod);
                    console_write("\n");
                }
                free(vm);
            }
        } else {
            console_write("\nsage: interpreting source: ");
            console_write(mod);
            sage_execute_source(source, mod);
        }

        free(source);
        free(st);
        return;
    }

    console_write("\nsage_execute: calling sage_execute_source");
    sage_execute_source(mod, "direct_code");
}

static void sage_supervisor_thread(void *arg) {
    (void)arg;
    console_write("\n[SUPERVISOR] Starting supervisor thread...");
    
    ThreadState *ts = (ThreadState*)calloc(1, sizeof(ThreadState));
    if (!ts) {
        console_write("\n[SUPERVISOR] Failed to allocate thread state!\n");
        return;
    }
    gc_register_thread(ts);

    console_write("\n[SUPERVISOR] Launching /etc/sagelang/runtime_manager.sage...");
    // dmesg_log("RUNTIME: Launching System Supervisor (/etc/sagelang/runtime_manager.sage)...");
    console_write("\n[SUPERVISOR] About to call sage_execute...");
    sage_execute("/etc/sagelang/runtime_manager.sage");
    
    console_write("\n[SUPERVISOR] Supervisor script execution finished.");
    extern void sageos_set_boot_stage(int stage);
    sageos_set_boot_stage(7); // STAGE_7_USERSPACE_SESSION
    
    gc_unregister_thread(ts);
    free(ts);

    console_write("\n[SUPERVISOR] Calling sys_exit(0)...");
    extern void sys_exit(int code);
    sys_exit(0);
}

void sage_runtime_init(void) {
    dmesg_log("RUNTIME: Initializing SGVM Core...");
    sage_repl_init();
    dmesg_log("RUNTIME: SGVM Runtime Bring-up complete.");
}

void sage_import_module(void* vm, const char* name) {
    (void)vm;
    (void)name;
    sage_repl_init();
}

void sage_execute_init(void) {
    thread_t *t = sched_create_thread("supervisor", sage_supervisor_thread, NULL, THREAD_PRIORITY_NORMAL);
    if (t) {
        dmesg_log("RUNTIME: Successfully spawned System Supervisor (PID 1) in background.");
    } else {
        dmesg_log("RUNTIME: FAILED to spawn System Supervisor!");
    }
}

static void sage_task_entry(void *arg) {
    char *script_path = (char *)arg;
    if (!script_path) return;

    /* Check if it's bytecode or source */
    VfsStat st;
    if (vfs_stat(script_path, &st) == VFS_OK) {
        uint8_t *data = malloc((size_t)st.size);
        if (data) {
            vfs_read(script_path, 0, data, (size_t)st.size);
        }
        
        if (data && st.size >= 4 && data[0] == 'S' && data[1] == 'G' && data[2] == 'V' && data[3] == 'M') {
            // Run as bytecode via MetalVM
            MetalVM vm;
            metal_vm_init(&vm);
            vm.write_char = bridge_write_char;
            vm.read_char = bridge_read_char;
            register_natives(&vm);
            if (metal_vm_load_binary(&vm, (const uint8_t*)data, (int)st.size)) {
                metal_vm_run(&vm);
            } else {
                console_write("\nsage: error loading bytecode artifact: ");
                console_write(script_path);
                console_write("\n");
            }
            free(data);
        } else {
            if (data) free(data);
            // Run as source via AST interpreter
            ThreadState *ts = (ThreadState*)calloc(1, sizeof(ThreadState));
            if (ts) {
                gc_register_thread(ts);
            }
            
            sage_execute(script_path);
            
            if (ts) {
                gc_unregister_thread(ts);
                free(ts);
            }
        }
    }
    
    free(script_path);
    extern void sys_exit(int code);
    sys_exit(0);
}

// --- OS Natives ---

static Value n_os_timer_poll(int argCount, Value* args) {
    (void)argCount; (void)args;
    extern void timer_poll(void);
    
    sage_gil_release();
    timer_poll();
    sage_gil_acquire();
    
    return val_nil();
}

static Value n_os_spawn_task(int argCount, Value* args) {
    if (argCount < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return val_number(-1);
    }
    const char *name = AS_STRING(args[0]);
    const char *script_path = AS_STRING(args[1]);
    console_write("\n[OS] spawning task: "); console_write(name);
    console_write(" path: "); console_write(script_path);

    char *path_copy = malloc(strlen(script_path) + 1);
    if (!path_copy) return val_number(-2);
    strcpy(path_copy, script_path);

    thread_t *t = sched_create_thread(name, sage_task_entry, path_copy, THREAD_PRIORITY_NORMAL);
    if (!t) {
        console_write(" -> FAILED");
        free(path_copy);
        return val_number(-3);
    }
    console_write(" -> SUCCESS PID="); console_u32((uint32_t)t->id);

    t->permissions |= PERM_VFS_CAP_ONLY;

    thread_t *parent = sched_current_thread();
    if (parent) {
        thread_ipc_ext_t *parent_ext = thread_ipc_ext(parent);
        thread_ipc_ext_t *child_ext = thread_ipc_ext(t);
        for (int i = 0; i < IPC_CAP_MAX_PER_TASK; i++) {
            child_ext->cap_table.caps[i] = parent_ext->cap_table.caps[i];
        }
        child_ext->cap_table.next_free = parent_ext->cap_table.next_free;
    }

    return val_number(t->id);
}

static Value n_os_version(int argCount, Value* args) {
    (void)argCount; (void)args;
    return val_string(SAGEOS_VERSION);
}

static Value n_os_gc_collect(int argCount, Value* args) {
    (void)argCount; (void)args;
    gc_collect();
    return val_nil();
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

static Value n_os_process_exists(int argCount, Value* args) {
    if (argCount >= 1 && IS_NUMBER(args[0])) {
        uint32_t pid = (uint32_t)AS_NUMBER(args[0]);
        thread_t *t = sched_get_thread_by_id(pid);
        return val_bool(t != NULL && t->state != THREAD_STATE_TERMINATED && t->state != THREAD_STATE_UNUSED);
    }
    return val_bool(false);
}

extern Value dict_keys(Value* dict);
static Value n_dict_keys(int argCount, Value* args) {
    if (argCount < 1 || !IS_DICT(args[0])) return val_nil();
    return dict_keys(&args[0]);
}

void register_sageos_natives(ModuleCache* cache) {
    Module* os = create_native_module(cache, "os");
    env_define(os->env, "timer_poll", 10, val_native(n_os_timer_poll));
    env_define(os->env, "spawn_task", 10, val_native(n_os_spawn_task));
    env_define(os->env, "version", 7, val_native(n_os_version));
    env_define(os->env, "gc_collect", 10, val_native(n_os_gc_collect));
    env_define(os->env, "write_char", 10, val_native(n_os_write_char));
    env_define(os->env, "write_str", 9, val_native(n_os_write_str));
    env_define(os->env, "process_exists", 14, val_native(n_os_process_exists));
    
    // Add to global env as well
    env_define_const(g_sage_env, "dict_keys", 9, val_native(n_dict_keys));

    Module* log = create_native_module(cache, "log");
    // Just use dmesg_log for all log levels for now
    extern Value n_os_dmesg_log(int argCount, Value* args);
    env_define(log->env, "info",  4, val_native(n_os_dmesg_log));
    env_define(log->env, "warn",  4, val_native(n_os_dmesg_log));
    env_define(log->env, "error", 5, val_native(n_os_dmesg_log));
}

Value n_os_dmesg_log(int argCount, Value* args) {
    if (argCount >= 1 && IS_STRING(args[0])) {
        dmesg_log(AS_STRING(args[0]));
    }
    return val_nil();
}

// --- Stubs for unsupported features ---

void* aot_init(void) { return NULL; }
void aot_free(void* a) { (void)a; }
void aot_set_var_type(void* a, const char* n, int t) { (void)a; (void)n; (void)t; }
void aot_compile_program(void* a, void* p) { (void)a; (void)p; }
void aot_compile_to_binary(void* a, const char* path) { (void)a; (void)path; }
void run_passes(void* p) { (void)p; }
void* parse_program(void* s, void* p) { (void)s;(void)p; return NULL; }

void bytecode_program_free(void* p) { (void)p; }
void vm_mark_roots(void* v) { (void)v; }
void vm_execute_program(void* v, void* p) { (void)v; (void)p; }
void bytecode_program_init(void* p) { (void)p; }
int bytecode_compile_program(void* p, void* s, int m, char* e, size_t es) { (void)p;(void)s;(void)m;(void)e;(void)es; return 0; }
int bytecode_program_read_file(void* p, const char* path, char* e, size_t es) { (void)p;(void)path;(void)e;(void)es; return 0; }
int bytecode_program_write_file(void* p, const char* path, char* e, size_t es) { (void)p;(void)path;(void)e;(void)es; return 0; }

// g_gc_root_stack is now provided by interpreter.c with __thread support
// EnvRootNode* g_gc_root_stack = NULL;
