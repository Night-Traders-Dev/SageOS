#include <stdint.h>
#include <stddef.h>

void shell_run(void) {}
void* metal_string_get(void* s) { return s; }
void* mv_nil(void) { return NULL; }
void* mv_str(const char* s) { return (void*)s; }
void* mv_num(int n) { return NULL; }
void* metal_dict_new(void) { return NULL; }
void* metal_string_intern(const char* s) { return (void*)s; }
void metal_dict_set(void* d, const char* k, void* v) {}
void* metal_array_new(void) { return NULL; }
void metal_array_push(void* a, void* v) {}
void metal_vm_init(void* vm) {}
void metal_vm_register_native(void* vm, const char* name, void* fn) {}
void metal_vm_load_binary(void* vm, void* b) {}
void metal_vm_run(void* vm) {}
void metal_vm_call(void* vm, const char* fn, int argc, void* args) {}
void* metal_dict_get(void* d, const char* k) { return NULL; }
int metal_array_len(void* a) { return 0; }
void* metal_array_get(void* a, int i) { return NULL; }

// Dummy strncpy/strcpy/strcat
char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}
char *strcpy(char *dest, const char *src) {
    char *d = dest; while (*src) *d++ = *src++; *d = '\0'; return dest;
}
char *strcat(char *dest, const char *src) {
    char *d = dest; while (*d) d++; while (*src) *d++ = *src++; *d = '\0'; return dest;
}

// n_os_ stubs
int n_len(void* v) { return 0; }
void* n_os_strlen(void* vm, void* args, int argc) { return NULL; }
void* n_os_starts_with(void* vm, void* args, int argc) { return NULL; }
int n_os_array_len(void* a) { return 0; }
void n_os_array_push(void* a, void* v) {}
void n_os_write_str(void* s) {}
void n_os_num_to_str(int n) {}
void n_os_stat(const char* path) {}
void* mv_ptr(void* p) { return NULL; }

// ATA & Boot
int ata_read_sector(uint32_t lba, uint16_t *buffer) { return 0; }
int ata_write_sector(uint32_t lba, uint16_t *buffer) { return 0; }
int ata_is_available(void) { return 0; }
void* kernel_get_boot_info(void) { return NULL; }

// Runtime stubs to satisfy linker
typedef struct {} jmp_buf;
int setjmp(jmp_buf env) { return 0; }
void longjmp(jmp_buf env, int val) {}
double strtod(const char *nptr, char **endptr) { return 0.0; }
int access(const char *pathname, int mode) { return -1; }
int mkstemps(char *template, int suffixlen) { return -1; }
int unlink(const char *pathname) { return -1; }
