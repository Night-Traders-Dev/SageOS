#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdatomic.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

typedef struct SageValue SageValue;
typedef struct SageGcHeader SageGcHeader;
typedef struct SageGcFrame SageGcFrame;

typedef struct {
    int count;
    int capacity;
    SageValue* elements;
} SageArray;

typedef struct {
    char** keys;
    SageValue* values;
    int count;
    int capacity;
} SageDict;

typedef struct {
    SageValue* elements;
    int count;
} SageTuple;

typedef enum {
    SAGE_TAG_NIL,
    SAGE_TAG_NUMBER,
    SAGE_TAG_BOOL,
    SAGE_TAG_STRING,
    SAGE_TAG_ARRAY,
    SAGE_TAG_DICT,
    SAGE_TAG_TUPLE,
    SAGE_TAG_FUNCTION,
    SAGE_TAG_CLIB,
    SAGE_TAG_POINTER,
    SAGE_TAG_THREAD,
    SAGE_TAG_MUTEX,
    SAGE_TAG_BYTES
} SageTag;

typedef struct {
    unsigned char* data;
    int count;
} SageBytes;

struct SageValue {
    SageTag type;
    union {
        double number;
        int boolean;
        const char* string;
        SageArray* array;
        SageDict* dict;
        SageTuple* tuple;
        void* function;
        void* clib;
        void* pointer;
        void* thread;
        void* mutex;
        SageBytes* bytes;
    } as;
};

typedef struct {
    int defined;
    SageValue value;
} SageSlot;

typedef enum {
    SAGE_GC_STRING,
    SAGE_GC_ARRAY,
    SAGE_GC_DICT,
    SAGE_GC_TUPLE
} SageGcKind;

struct SageGcHeader {
    unsigned char marked;
    unsigned char kind;
    size_t size;
    SageGcHeader* next;
};

struct SageGcFrame {
    SageGcFrame* prev;
    SageSlot** slots;
    int slot_count;
};

typedef struct {
    SageGcHeader* objects;
    SageGcFrame* frames;
    int object_count;
    int collections;
    int pin_count;
    unsigned long bytes_allocated;
    unsigned long bytes_freed;
    unsigned long next_gc_bytes;
    int next_gc_objects;
    int enabled;
} SageGcState;

#define SAGE_GC_MIN_TRIGGER_BYTES 65536UL
#define SAGE_GC_MIN_TRIGGER_OBJECTS 128
static SageGcState sage_gc = {NULL, NULL, 0, 0, 0, 0, 0, SAGE_GC_MIN_TRIGGER_BYTES, SAGE_GC_MIN_TRIGGER_OBJECTS, 1};

/* Exception handling via setjmp/longjmp */
#define SAGE_MAX_TRY_DEPTH 1024
static jmp_buf sage_try_stack[SAGE_MAX_TRY_DEPTH];
static SageValue sage_exception_value;
static int sage_try_depth = 0;

static void sage_fail(const char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

static unsigned long sage_gc_live_bytes(void) {
    return sage_gc.bytes_allocated - sage_gc.bytes_freed;
}

static void sage_gc_recompute_thresholds(unsigned long reclaimed_bytes, int reclaimed_objects) {
    unsigned long live_bytes = sage_gc_live_bytes();
    int live_objects = sage_gc.object_count;
    unsigned long byte_padding = live_bytes / 2;
    int object_padding = live_objects / 2;
    if (byte_padding < (SAGE_GC_MIN_TRIGGER_BYTES / 2)) byte_padding = SAGE_GC_MIN_TRIGGER_BYTES / 2;
    if (object_padding < (SAGE_GC_MIN_TRIGGER_OBJECTS / 2)) object_padding = SAGE_GC_MIN_TRIGGER_OBJECTS / 2;
    if (reclaimed_bytes <= live_bytes / 8) {
        byte_padding /= 2;
        if (byte_padding < (SAGE_GC_MIN_TRIGGER_BYTES / 2)) byte_padding = SAGE_GC_MIN_TRIGGER_BYTES / 2;
    } else if (reclaimed_bytes >= live_bytes) {
        byte_padding *= 2;
    }
    if (reclaimed_objects <= live_objects / 8) {
        object_padding /= 2;
        if (object_padding < (SAGE_GC_MIN_TRIGGER_OBJECTS / 2)) object_padding = SAGE_GC_MIN_TRIGGER_OBJECTS / 2;
    } else if (reclaimed_objects >= live_objects) {
        object_padding *= 2;
    }
    sage_gc.next_gc_bytes = live_bytes + byte_padding;
    if (sage_gc.next_gc_bytes < SAGE_GC_MIN_TRIGGER_BYTES) sage_gc.next_gc_bytes = SAGE_GC_MIN_TRIGGER_BYTES;
    sage_gc.next_gc_objects = live_objects + object_padding;
    if (sage_gc.next_gc_objects < SAGE_GC_MIN_TRIGGER_OBJECTS) sage_gc.next_gc_objects = SAGE_GC_MIN_TRIGGER_OBJECTS;
}

static int sage_gc_try_mark(void* object) {
    if (object == NULL) return 0;
    SageGcHeader* header = ((SageGcHeader*)object) - 1;
    if (header->marked) return 0;
    header->marked = 1;
    return 1;
}

static void sage_gc_mark_value(SageValue value);

static void sage_gc_mark_roots(void) {
    for (SageGcFrame* frame = sage_gc.frames; frame != NULL; frame = frame->prev) {
        if (frame->slots == NULL) continue;
        for (int i = 0; i < frame->slot_count; i++) {
            if (frame->slots[i] != NULL && frame->slots[i]->defined) {
                sage_gc_mark_value(frame->slots[i]->value);
            }
        }
    }
    if (sage_try_depth > 0) sage_gc_mark_value(sage_exception_value);
}

static size_t sage_gc_release_object(SageGcHeader* header) {
    void* object = (void*)(header + 1);
    size_t freed = sizeof(SageGcHeader) + header->size;
    switch ((SageGcKind)header->kind) {
        case SAGE_GC_STRING:
            break;
        case SAGE_GC_ARRAY: {
            SageArray* array = (SageArray*)object;
            freed += sizeof(SageValue) * (size_t)array->capacity;
            free(array->elements);
            break;
        }
        case SAGE_GC_DICT: {
            SageDict* dict = (SageDict*)object;
            freed += sizeof(char*) * (size_t)dict->capacity;
            freed += sizeof(SageValue) * (size_t)dict->capacity;
            for (int i = 0; i < dict->count; i++) {
                if (dict->keys[i] != NULL) {
                    freed += strlen(dict->keys[i]) + 1;
                    free(dict->keys[i]);
                }
            }
            free(dict->keys);
            free(dict->values);
            break;
        }
        case SAGE_GC_TUPLE: {
            SageTuple* tuple = (SageTuple*)object;
            freed += sizeof(SageValue) * (size_t)tuple->count;
            free(tuple->elements);
            break;
        }
    }
    return freed;
}

static void sage_gc_collect(void) {
    if (!sage_gc.enabled) return;
    unsigned long before_bytes = sage_gc_live_bytes();
    int before_objects = sage_gc.object_count;
    sage_gc_mark_roots();
    SageGcHeader** current = &sage_gc.objects;
    while (*current != NULL) {
        SageGcHeader* header = *current;
        if (!header->marked) {
            *current = header->next;
            sage_gc.object_count--;
            sage_gc.bytes_freed += sage_gc_release_object(header);
            free(header);
        } else {
            header->marked = 0;
            current = &header->next;
        }
    }
    sage_gc.collections++;
    sage_gc_recompute_thresholds(before_bytes - sage_gc_live_bytes(), before_objects - sage_gc.object_count);
}

static int sage_gc_should_collect(size_t incoming_size) {
    if (!sage_gc.enabled || sage_gc.pin_count > 0) return 0;
    if ((sage_gc.object_count + 1) >= sage_gc.next_gc_objects) return 1;
    return sage_gc_live_bytes() + (unsigned long)sizeof(SageGcHeader) + (unsigned long)incoming_size >= sage_gc.next_gc_bytes;
}

static void* sage_gc_alloc(SageGcKind kind, size_t size) {
    if (sage_gc.frames != NULL && sage_gc_should_collect(size)) sage_gc_collect();
    size_t total = sizeof(SageGcHeader) + size;
    SageGcHeader* header = (SageGcHeader*)malloc(total);
    if (header == NULL) sage_fail("Runtime Error: out of memory");
    header->marked = 0;
    header->kind = (unsigned char)kind;
    header->size = size;
    header->next = sage_gc.objects;
    sage_gc.objects = header;
    sage_gc.object_count++;
    sage_gc.bytes_allocated += (unsigned long)total;
    return (void*)(header + 1);
}

static void sage_gc_push_frame(SageGcFrame* frame, SageSlot** slots, int slot_count) {
    frame->prev = sage_gc.frames;
    frame->slots = slots;
    frame->slot_count = slot_count;
    sage_gc.frames = frame;
}

static void sage_gc_pop_frame(SageGcFrame* frame) {
    if (sage_gc.frames == frame) sage_gc.frames = frame->prev;
}

static void sage_gc_pin(void) { sage_gc.pin_count++; }
static void sage_gc_unpin(void) { if (sage_gc.pin_count > 0) sage_gc.pin_count--; }

static SageValue sage_gc_return(SageGcFrame* frame, SageValue value) {
    sage_gc_pop_frame(frame);
    return value;
}

static void sage_gc_shutdown(void) {
    SageGcHeader* object = sage_gc.objects;
    while (object != NULL) {
        SageGcHeader* next = object->next;
        sage_gc.bytes_freed += sage_gc_release_object(object);
        free(object);
        object = next;
    }
    sage_gc.objects = NULL;
    sage_gc.object_count = 0;
}

static void sage_gc_mark_value(SageValue value) {
    switch (value.type) {
        case SAGE_TAG_STRING:
            (void)sage_gc_try_mark((void*)value.as.string);
            return;
        case SAGE_TAG_ARRAY:
            if (sage_gc_try_mark(value.as.array)) {
                for (int i = 0; i < value.as.array->count; i++) sage_gc_mark_value(value.as.array->elements[i]);
            }
            return;
        case SAGE_TAG_DICT:
            if (sage_gc_try_mark(value.as.dict)) {
                for (int i = 0; i < value.as.dict->count; i++) sage_gc_mark_value(value.as.dict->values[i]);
            }
            return;
        case SAGE_TAG_TUPLE:
            if (sage_gc_try_mark(value.as.tuple)) {
                for (int i = 0; i < value.as.tuple->count; i++) sage_gc_mark_value(value.as.tuple->elements[i]);
            }
            return;
        default:
            return;
    }
}

static char* sage_dup_string(const char* text) {
    size_t len = strlen(text);
    char* copy = (char*)malloc(len + 1);
    if (copy == NULL) sage_fail("Runtime Error: out of memory");
    memcpy(copy, text, len + 1);
    return copy;
}

static char* sage_gc_copy_string(const char* text) {
    size_t len = strlen(text);
    char* copy = (char*)sage_gc_alloc(SAGE_GC_STRING, len + 1);
    memcpy(copy, text, len + 1);
    return copy;
}

static SageArray* sage_new_array(void) {
    SageArray* array = (SageArray*)sage_gc_alloc(SAGE_GC_ARRAY, sizeof(SageArray));
    array->count = 0;
    array->capacity = 0;
    array->elements = NULL;
    return array;
}

static SageValue sage_nil(void) { SageValue v; v.type = SAGE_TAG_NIL; v.as.number = 0; return v; }
static SageValue sage_number(double value) { SageValue v; v.type = SAGE_TAG_NUMBER; v.as.number = value; return v; }
static SageValue sage_bool(int value) { SageValue v; v.type = SAGE_TAG_BOOL; v.as.boolean = value ? 1 : 0; return v; }
static SageValue sage_string(const char* value) { SageValue v; v.type = SAGE_TAG_STRING; v.as.string = sage_gc_copy_string(value == NULL ? "" : value); return v; }
static SageValue sage_string_take(char* value) { SageValue v = sage_string(value == NULL ? "" : value); free(value); return v; }
static SageValue sage_array(void) { SageValue v; v.type = SAGE_TAG_ARRAY; v.as.array = sage_new_array(); return v; }
static SageValue sage_function(void* fn) { SageValue v; v.type = SAGE_TAG_FUNCTION; v.as.function = fn; return v; }

static SageValue sage_ffi_open(SageValue libname) {
    if (libname.type != SAGE_TAG_STRING) return sage_nil();
    void* handle = dlopen(libname.as.string, RTLD_NOW);
    if (!handle) return sage_nil();
    SageValue v; v.type = SAGE_TAG_CLIB; v.as.clib = handle; return v;
}
static SageValue sage_ffi_close(SageValue handle) {
    if (handle.type != SAGE_TAG_CLIB) return sage_nil();
    dlclose(handle.as.clib);
    return sage_nil();
}
static SageValue sage_ffi_call(SageValue handle, SageValue name, SageValue args) { return sage_nil(); }
static SageValue sage_ffi_call_full(SageValue handle, SageValue name, SageValue args, SageValue rt) { return sage_nil(); }

static SageValue sage_atomic_new(SageValue val) {
    SageValue* atom = malloc(sizeof(SageValue));
    *atom = val;
    SageValue v; v.type = SAGE_TAG_POINTER; v.as.pointer = atom; return v;
}
static SageValue sage_atomic_load(SageValue atom) {
    if (atom.type != SAGE_TAG_POINTER) return sage_nil();
    return *(SageValue*)atom.as.pointer;
}
static SageValue sage_atomic_store(SageValue atom, SageValue val) {
    if (atom.type != SAGE_TAG_POINTER) return sage_nil();
    *(SageValue*)atom.as.pointer = val;
    return val;
}
static SageValue sage_atomic_add(SageValue atom, SageValue val) { return sage_nil(); }
static SageValue sage_atomic_cas(SageValue atom, SageValue old, SageValue new_val) { return sage_nil(); }
static SageValue sage_atomic_exchange(SageValue atom, SageValue val) { return sage_nil(); }

static SageValue sage_sem_new(SageValue val) {
    sem_t* sem = malloc(sizeof(sem_t));
    sem_init(sem, 0, (unsigned int)val.as.number);
    SageValue v; v.type = SAGE_TAG_POINTER; v.as.pointer = sem; return v;
}
static SageValue sage_sem_wait(SageValue sem) {
    if (sem.type != SAGE_TAG_POINTER) return sage_nil();
    sem_wait((sem_t*)sem.as.pointer);
    return sage_nil();
}
static SageValue sage_sem_post(SageValue sem) {
    if (sem.type != SAGE_TAG_POINTER) return sage_nil();
    sem_post((sem_t*)sem.as.pointer);
    return sage_nil();
}
static SageValue sage_sem_trywait(SageValue sem) {
    if (sem.type != SAGE_TAG_POINTER) return sage_bool(0);
    return sage_bool(sem_trywait((sem_t*)sem.as.pointer) == 0);
}
static SageSlot sage_slot_undefined(void) { SageSlot slot; slot.defined = 0; slot.value = sage_nil(); return slot; }

static SageValue sage_make_dict(void) {
    SageDict* dict = (SageDict*)sage_gc_alloc(SAGE_GC_DICT, sizeof(SageDict));
    dict->keys = NULL;
    dict->values = NULL;
    dict->count = 0;
    dict->capacity = 0;
    SageValue v; v.type = SAGE_TAG_DICT; v.as.dict = dict;
    return v;
}

static void sage_dict_set(SageDict* dict, const char* key, SageValue value) {
    for (int i = 0; i < dict->count; i++) {
        if (strcmp(dict->keys[i], key) == 0) {
            dict->values[i] = value;
            return;
        }
    }
    if (dict->count >= dict->capacity) {
        int cap = dict->capacity == 0 ? 4 : dict->capacity * 2;
        dict->keys = (char**)realloc(dict->keys, sizeof(char*) * (size_t)cap);
        dict->values = (SageValue*)realloc(dict->values, sizeof(SageValue) * (size_t)cap);
        if (dict->keys == NULL || dict->values == NULL) sage_fail("Runtime Error: out of memory");
        dict->capacity = cap;
    }
    dict->keys[dict->count] = sage_dup_string(key);
    dict->values[dict->count] = value;
    dict->count++;
}

static SageValue sage_make_dict_from_entries(int count, const char** keys, const SageValue* values) {
    sage_gc_pin();
    SageValue dict = sage_make_dict();
    for (int i = 0; i < count; i++) {
        sage_dict_set(dict.as.dict, keys[i], values[i]);
    }
    sage_gc_unpin();
    return dict;
}

static SageValue sage_dict_get(SageDict* dict, const char* key) {
    for (int i = 0; i < dict->count; i++) {
        if (strcmp(dict->keys[i], key) == 0) return dict->values[i];
    }
    return sage_nil();
}

static SageValue sage_make_tuple(int count, const SageValue* values) {
    sage_gc_pin();
    SageTuple* tuple = (SageTuple*)sage_gc_alloc(SAGE_GC_TUPLE, sizeof(SageTuple));
    tuple->count = count;
    tuple->elements = (SageValue*)malloc(sizeof(SageValue) * (size_t)count);
    if (tuple->elements == NULL && count > 0) sage_fail("Runtime Error: out of memory");
    for (int i = 0; i < count; i++) tuple->elements[i] = values[i];
    SageValue v; v.type = SAGE_TAG_TUPLE; v.as.tuple = tuple;
    sage_gc_unpin();
    return v;
}

static void sage_raise(SageValue value) {
    if (sage_try_depth > 0) {
        sage_exception_value = value;
        longjmp(sage_try_stack[sage_try_depth - 1], 1);
    }
    fputs("Unhandled exception: ", stderr);
    if (value.type == SAGE_TAG_STRING) fputs(value.as.string, stderr);
    else fputs("(unknown)", stderr);
    fputc('\n', stderr);
    exit(1);
}

static void sage_array_reserve(SageArray* array, int needed) {
    if (array->capacity >= needed) return;
    int capacity = array->capacity == 0 ? 4 : array->capacity;
    while (capacity < needed) capacity *= 2;
    SageValue* elements = (SageValue*)realloc(array->elements, sizeof(SageValue) * (size_t)capacity);
    if (elements == NULL) sage_fail("Runtime Error: out of memory");
    array->elements = elements;
    array->capacity = capacity;
}

static void sage_array_push_raw(SageArray* array, SageValue value) {
    sage_array_reserve(array, array->count + 1);
    array->elements[array->count++] = value;
}

static SageValue sage_make_array(int count, const SageValue* values) {
    sage_gc_pin();
    SageValue array = sage_array();
    for (int i = 0; i < count; i++) {
        sage_array_push_raw(array.as.array, values[i]);
    }
    sage_gc_unpin();
    return array;
}

static int sage_truthy(SageValue value) {
    if (value.type == SAGE_TAG_NIL) return 0;
    if (value.type == SAGE_TAG_BOOL) return value.as.boolean;
    if (value.type == SAGE_TAG_NUMBER) return value.as.number != 0.0;
    if (value.type == SAGE_TAG_STRING) return value.as.string[0] != '\0';
    return 1;
}

static SageValue sage_load_slot(const SageSlot* slot, const char* name) {
    if (!slot->defined) {
        fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", name);
        exit(1);
    }
    return slot->value;
}

static void sage_define_slot(SageSlot* slot, SageValue value) {
    slot->defined = 1;
    slot->value = value;
}

static SageValue sage_assign_slot(SageSlot* slot, const char* name, SageValue value) {
    if (!slot->defined) {
        fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", name);
        exit(1);
    }
    slot->value = value;
    return value;
}

static int sage_values_equal(SageValue left, SageValue right) {
    if (left.type != right.type) return 0;
    switch (left.type) {
        case SAGE_TAG_NIL: return 1;
        case SAGE_TAG_NUMBER: return left.as.number == right.as.number;
        case SAGE_TAG_BOOL: return left.as.boolean == right.as.boolean;
        case SAGE_TAG_STRING: return strcmp(left.as.string, right.as.string) == 0;
        case SAGE_TAG_ARRAY: {
            if (left.as.array == right.as.array) return 1;
            if (left.as.array->count != right.as.array->count) return 0;
            for (int i = 0; i < left.as.array->count; i++) {
                if (!sage_values_equal(left.as.array->elements[i], right.as.array->elements[i])) return 0;
            }
            return 1;
        }
        case SAGE_TAG_DICT: return left.as.dict == right.as.dict;
        case SAGE_TAG_TUPLE: {
            if (left.as.tuple == right.as.tuple) return 1;
            if (left.as.tuple->count != right.as.tuple->count) return 0;
            for (int i = 0; i < left.as.tuple->count; i++) {
                if (!sage_values_equal(left.as.tuple->elements[i], right.as.tuple->elements[i])) return 0;
            }
            return 1;
        }
    }
    return 0;
}

static void sage_print_value(SageValue value) {
    switch (value.type) {
        case SAGE_TAG_NUMBER: {
            double d = value.as.number;
            if (d == (double)(long long)d && d >= -1e15 && d <= 1e15)
                printf("%lld", (long long)d);
            else
                printf("%g", d);
            break;
        }
        case SAGE_TAG_BOOL: fputs(value.as.boolean ? "true" : "false", stdout); break;
        case SAGE_TAG_STRING: fputs(value.as.string, stdout); break;
        case SAGE_TAG_ARRAY:
            fputc('[', stdout);
            for (int i = 0; i < value.as.array->count; i++) {
                if (i > 0) fputs(", ", stdout);
                sage_print_value(value.as.array->elements[i]);
            }
            fputc(']', stdout);
            break;
        case SAGE_TAG_DICT:
            fputc('{', stdout);
            for (int i = 0; i < value.as.dict->count; i++) {
                if (i > 0) fputs(", ", stdout);
                printf("\"%s\": ", value.as.dict->keys[i]);
                sage_print_value(value.as.dict->values[i]);
            }
            fputc('}', stdout);
            break;
        case SAGE_TAG_TUPLE:
            fputc('(', stdout);
            for (int i = 0; i < value.as.tuple->count; i++) {
                if (i > 0) fputs(", ", stdout);
                sage_print_value(value.as.tuple->elements[i]);
            }
            fputc(')', stdout);
            break;
        case SAGE_TAG_NIL: fputs("nil", stdout); break;
    }
}

static void sage_print_ln(SageValue value) {
    sage_print_value(value);
    fputc('\n', stdout);
}

static SageValue sage_str(SageValue value) {
    char buffer[64];
    switch (value.type) {
        case SAGE_TAG_STRING: return value;
        case SAGE_TAG_NUMBER: {
            double d = value.as.number;
            if (d == (double)(long long)d && d >= -1e15 && d <= 1e15)
                snprintf(buffer, sizeof(buffer), "%lld", (long long)d);
            else
                snprintf(buffer, sizeof(buffer), "%g", d);
            return sage_string(buffer);
        }
        case SAGE_TAG_BOOL:
            return sage_string(value.as.boolean ? "true" : "false");
        case SAGE_TAG_NIL:
            return sage_string("nil");
        case SAGE_TAG_ARRAY:
            return sage_string("<array>");
        case SAGE_TAG_DICT:
            return sage_string("<dict>");
        case SAGE_TAG_TUPLE:
            return sage_string("<tuple>");
    }
    return sage_string("nil");
}

static SageValue sage_int(SageValue value) {
    if (value.type == SAGE_TAG_NUMBER) return sage_number((double)(long long)value.as.number);
    if (value.type == SAGE_TAG_STRING) return sage_number((double)atof(value.as.string));
    if (value.type == SAGE_TAG_BOOL) return sage_number((double)value.as.boolean);
    return sage_number(0);
}

static SageValue sage_abs(SageValue value) {
    if (value.type == SAGE_TAG_NUMBER) return sage_number(fabs(value.as.number));
    return sage_nil();
}
static SageValue sage_sqrt(SageValue value) {
    if (value.type == SAGE_TAG_NUMBER) return sage_number(sqrt(value.as.number));
    return sage_nil();
}

static SageValue sage_native_random(void) { return sage_number((double)rand() / (double)RAND_MAX); }
static SageValue sage_native_sin(SageValue v) { return sage_number(sin(v.as.number)); }
static SageValue sage_native_cos(SageValue v) { return sage_number(cos(v.as.number)); }
static SageValue sage_native_tan(SageValue v) { return sage_number(tan(v.as.number)); }
static SageValue sage_native_floor(SageValue v) { return sage_number(floor(v.as.number)); }
static SageValue sage_native_ceil(SageValue v) { return sage_number(ceil(v.as.number)); }
static SageValue sage_native_pow(SageValue a, SageValue b) { return sage_number(pow(a.as.number, b.as.number)); }
static SageValue sage_native_exp(SageValue v) { return sage_number(exp(v.as.number)); }
static SageValue sage_native_log(SageValue v) { return sage_number(log(v.as.number)); }
static SageValue sage_native_sqrt(SageValue v) { return sage_number(sqrt(v.as.number)); }

static SageValue sage_native_thread_mutex(void) {
    pthread_mutex_t* m = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(m, NULL);
    SageValue v; v.type = SAGE_TAG_MUTEX; v.as.mutex = m; return v;
}
static SageValue sage_native_thread_lock(SageValue m) {
    if (m.type == SAGE_TAG_MUTEX) pthread_mutex_lock((pthread_mutex_t*)m.as.mutex);
    return sage_nil();
}
static SageValue sage_native_thread_unlock(SageValue m) {
    if (m.type == SAGE_TAG_MUTEX) pthread_mutex_unlock((pthread_mutex_t*)m.as.mutex);
    return sage_nil();
}
static void* sage_thread_wrapper(void* arg) {
    (void)arg;
    return NULL;
}
static SageValue sage_native_thread_spawn(SageValue fn, SageValue arg) {
    pthread_t* t = malloc(sizeof(pthread_t));
    (void)fn; (void)arg;
    pthread_create(t, NULL, sage_thread_wrapper, NULL);
    SageValue v; v.type = SAGE_TAG_THREAD; v.as.thread = t; return v;
}
static SageValue sage_native_thread_sleep(SageValue ms) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ms.as.number / 1000);
    ts.tv_nsec = (long)((ms.as.number - (double)(ts.tv_sec * 1000)) * 1000000);
    nanosleep(&ts, NULL);
    return sage_nil();
}
static SageValue sage_native_thread_id(void) { return sage_number((double)(uintptr_t)pthread_self()); }

static SageValue sage_native_io_readbytes(SageValue path) {
    if (path.type != SAGE_TAG_STRING) return sage_nil();
    FILE* f = fopen(path.as.string, "rb");
    if (!f) return sage_nil();
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* data = (unsigned char*)malloc((size_t)size);
    if (data) fread(data, 1, (size_t)size, f);
    fclose(f);
    if (!data) return sage_nil();
    SageBytes* bytes = (SageBytes*)malloc(sizeof(SageBytes));
    bytes->data = data; bytes->count = (int)size;
    SageValue v; v.type = SAGE_TAG_BYTES; v.as.bytes = bytes; return v;
}
static SageValue sage_native_io_readfile(SageValue path) { return sage_native_io_readbytes(path); }
static SageValue sage_native_io_writefile(SageValue path, SageValue data) {
    if (path.type != SAGE_TAG_STRING || data.type != SAGE_TAG_STRING) return sage_nil();
    FILE* f = fopen(path.as.string, "wb");
    if (!f) return sage_nil();
    fwrite(data.as.string, 1, strlen(data.as.string), f);
    fclose(f);
    return sage_bool(1);
}

extern int sage_argc;
extern char** sage_argv;
static SageValue sage_native_sys_args(void) {
    SageValue arr = sage_array();
    for (int i = 0; i < sage_argc; i++) {
        sage_array_push_raw(arr.as.array, sage_string(sage_argv[i]));
    }
    return arr;
}
static SageValue sage_native_sys_getenv(SageValue name) {
    if (name.type != SAGE_TAG_STRING) return sage_nil();
    char* val = getenv(name.as.string);
    return val ? sage_string(val) : sage_nil();
}
static SageValue sage_native_sys_clock(void) { return sage_number((double)clock() / CLOCKS_PER_SEC); }

static SageValue sage_init_native_module(const char* name) {
    /* For now, just return an empty dict; real native modules should be linked */
    return sage_make_dict();
}

static SageValue sage_len(SageValue value) {
    if (value.type == SAGE_TAG_STRING) return sage_number((double)strlen(value.as.string));
    if (value.type == SAGE_TAG_ARRAY) return sage_number((double)value.as.array->count);
    if (value.type == SAGE_TAG_DICT) return sage_number((double)value.as.dict->count);
    if (value.type == SAGE_TAG_TUPLE) return sage_number((double)value.as.tuple->count);
    if (value.type == SAGE_TAG_BYTES) return sage_number((double)value.as.bytes->count);
    return sage_nil();
}

static SageValue sage_index(SageValue collection, SageValue index) {
    if (collection.type == SAGE_TAG_ARRAY && index.type == SAGE_TAG_NUMBER) {
        int idx = (int)index.as.number;
        if (idx < 0 || idx >= collection.as.array->count) return sage_nil();
        return collection.as.array->elements[idx];
    }
    if (collection.type == SAGE_TAG_BYTES && index.type == SAGE_TAG_NUMBER) {
        int idx = (int)index.as.number;
        if (idx < 0 || idx >= collection.as.bytes->count) return sage_nil();
        return sage_number((double)collection.as.bytes->data[idx]);
    }
    if (collection.type == SAGE_TAG_DICT && index.type == SAGE_TAG_STRING) {
        return sage_dict_get(collection.as.dict, index.as.string);
    }
    if (collection.type == SAGE_TAG_TUPLE && index.type == SAGE_TAG_NUMBER) {
        int idx = (int)index.as.number;
        if (idx < 0 || idx >= collection.as.tuple->count) return sage_nil();
        return collection.as.tuple->elements[idx];
    }
    if (collection.type == SAGE_TAG_STRING && index.type == SAGE_TAG_NUMBER) {
        int idx = (int)index.as.number;
        int len = (int)strlen(collection.as.string);
        if (idx < 0 || idx >= len) return sage_nil();
        char buf[2] = {collection.as.string[idx], '\0'};
        return sage_string(buf);
    }
    return sage_nil();
}

static SageValue sage_slice(SageValue array, SageValue start, SageValue end) {
    if (array.type != SAGE_TAG_ARRAY) return sage_nil();
    sage_gc_pin();
    int start_index = 0;
    int end_index = array.as.array->count;
    if (start.type == SAGE_TAG_NUMBER) start_index = (int)start.as.number;
    else if (start.type != SAGE_TAG_NIL) { sage_gc_unpin(); return sage_nil(); }
    if (end.type == SAGE_TAG_NUMBER) end_index = (int)end.as.number;
    else if (end.type != SAGE_TAG_NIL) { sage_gc_unpin(); return sage_nil(); }
    if (start_index < 0) start_index = array.as.array->count + start_index;
    if (end_index < 0) end_index = array.as.array->count + end_index;
    if (start_index < 0) start_index = 0;
    if (end_index > array.as.array->count) end_index = array.as.array->count;
    if (start_index >= end_index) { SageValue empty = sage_array(); sage_gc_unpin(); return empty; }
    SageValue result = sage_array();
    for (int i = start_index; i < end_index; i++) {
        sage_array_push_raw(result.as.array, array.as.array->elements[i]);
    }
    sage_gc_unpin();
    return result;
}

static SageValue sage_push(SageValue array, SageValue value) {
    if (array.type != SAGE_TAG_ARRAY) return sage_nil();
    sage_array_push_raw(array.as.array, value);
    return sage_nil();
}

static SageValue sage_pop(SageValue array) {
    if (array.type != SAGE_TAG_ARRAY || array.as.array->count == 0) return sage_nil();
    return array.as.array->elements[--array.as.array->count];
}

static SageValue sage_array_extend(SageValue target, SageValue source) {
    if (target.type != SAGE_TAG_ARRAY || source.type != SAGE_TAG_ARRAY) return sage_nil();
    SageArray* dst = target.as.array;
    SageArray* src = source.as.array;
    if (src->count > 0) {
        sage_array_reserve(dst, dst->count + src->count);
        memcpy(dst->elements + dst->count, src->elements, sizeof(SageValue) * (size_t)src->count);
        dst->count += src->count;
    }
    return sage_nil();
}

static SageValue sage_array_reverse(SageValue array) {
    if (array.type != SAGE_TAG_ARRAY) return sage_nil();
    SageArray* src = array.as.array;
    sage_gc_pin();
    SageValue result = sage_array();
    if (src->count > 0) {
        SageArray* dst = result.as.array;
        sage_array_reserve(dst, src->count);
        dst->count = src->count;
        for (int i = 0; i < src->count; i++) {
            dst->elements[i] = src->elements[src->count - 1 - i];
        }
    }
    sage_gc_unpin();
    return result;
}

static SageValue sage_range2(SageValue start, SageValue end) {
    if (start.type != SAGE_TAG_NUMBER || end.type != SAGE_TAG_NUMBER) return sage_nil();
    sage_gc_pin();
    SageValue result = sage_array();
    for (int i = (int)start.as.number; i < (int)end.as.number; i++) {
        sage_array_push_raw(result.as.array, sage_number((double)i));
    }
    sage_gc_unpin();
    return result;
}

static SageValue sage_range1(SageValue end) {
    return sage_range2(sage_number(0), end);
}

static SageValue sage_add(SageValue left, SageValue right) {
    if (left.type == SAGE_TAG_NUMBER && right.type == SAGE_TAG_NUMBER) {
        return sage_number(left.as.number + right.as.number);
    }
    if (left.type == SAGE_TAG_STRING && right.type == SAGE_TAG_STRING) {
        size_t len1 = strlen(left.as.string);
        size_t len2 = strlen(right.as.string);
        char* result = (char*)malloc(len1 + len2 + 1);
        if (result == NULL) sage_fail("Runtime Error: out of memory");
        memcpy(result, left.as.string, len1);
        memcpy(result + len1, right.as.string, len2 + 1);
        return sage_string_take(result);
    }
    sage_fail("Runtime Error: Operands must be numbers or strings.");
    return sage_nil();
}

static SageValue sage_sub(SageValue left, SageValue right) {
    if (left.type != SAGE_TAG_NUMBER || right.type != SAGE_TAG_NUMBER) sage_fail("Runtime Error: Operands must be numbers.");
    return sage_number(left.as.number - right.as.number);
}
static SageValue sage_mul(SageValue left, SageValue right) {
    if (left.type != SAGE_TAG_NUMBER || right.type != SAGE_TAG_NUMBER) sage_fail("Runtime Error: Operands must be numbers.");
    return sage_number(left.as.number * right.as.number);
}
static SageValue sage_div(SageValue left, SageValue right) {
    if (left.type != SAGE_TAG_NUMBER || right.type != SAGE_TAG_NUMBER) sage_fail("Runtime Error: Operands must be numbers.");
    if (right.as.number == 0) return sage_nil();
    return sage_number(left.as.number / right.as.number);
}
static SageValue sage_mod(SageValue left, SageValue right) {
    if (left.type != SAGE_TAG_NUMBER || right.type != SAGE_TAG_NUMBER) sage_fail("Runtime Error: Operands must be numbers.");
    if (right.as.number == 0) return sage_nil();
    return sage_number(fmod(left.as.number, right.as.number));
}
static SageValue sage_eq(SageValue left, SageValue right) { return sage_bool(sage_values_equal(left, right)); }
static SageValue sage_neq(SageValue left, SageValue right) { return sage_bool(!sage_values_equal(left, right)); }
static SageValue sage_gt(SageValue left, SageValue right) {
    if (left.type != SAGE_TAG_NUMBER || right.type != SAGE_TAG_NUMBER) sage_fail("Runtime Error: Operands must be numbers.");
    return sage_bool(left.as.number > right.as.number);
}
static SageValue sage_lt(SageValue left, SageValue right) {
    if (left.type != SAGE_TAG_NUMBER || right.type != SAGE_TAG_NUMBER) sage_fail("Runtime Error: Operands must be numbers.");
    return sage_bool(left.as.number < right.as.number);
}
static SageValue sage_gte(SageValue left, SageValue right) {
    if (left.type != SAGE_TAG_NUMBER || right.type != SAGE_TAG_NUMBER) sage_fail("Runtime Error: Operands must be numbers.");
    return sage_bool(left.as.number >= right.as.number);
}
static SageValue sage_lte(SageValue left, SageValue right) {
    if (left.type != SAGE_TAG_NUMBER || right.type != SAGE_TAG_NUMBER) sage_fail("Runtime Error: Operands must be numbers.");
    return sage_bool(left.as.number <= right.as.number);
}
static SageValue sage_not(SageValue value) { return sage_bool(!sage_truthy(value)); }
static SageValue sage_and(SageValue left, SageValue right) { return sage_bool(sage_truthy(left) && sage_truthy(right)); }
static SageValue sage_or(SageValue left, SageValue right) { return sage_bool(sage_truthy(left) || sage_truthy(right)); }
static SageValue sage_bit_not(SageValue value) {
    if (value.type != SAGE_TAG_NUMBER) sage_fail("Runtime Error: Bitwise NOT operand must be a number.");
    return sage_number((double)(~(long long)value.as.number));
}
static SageValue sage_bit_and(SageValue left, SageValue right) {
    if (left.type != SAGE_TAG_NUMBER || right.type != SAGE_TAG_NUMBER) sage_fail("Runtime Error: Operands must be numbers.");
    return sage_number((double)(((long long)left.as.number) & ((long long)right.as.number)));
}
static SageValue sage_bit_or(SageValue left, SageValue right) {
    if (left.type != SAGE_TAG_NUMBER || right.type != SAGE_TAG_NUMBER) sage_fail("Runtime Error: Operands must be numbers.");
    return sage_number((double)(((long long)left.as.number) | ((long long)right.as.number)));
}
static SageValue sage_bit_xor(SageValue left, SageValue right) {
    if (left.type != SAGE_TAG_NUMBER || right.type != SAGE_TAG_NUMBER) sage_fail("Runtime Error: Operands must be numbers.");
    return sage_number((double)(((long long)left.as.number) ^ ((long long)right.as.number)));
}
static SageValue sage_lshift(SageValue left, SageValue right) {
    if (left.type != SAGE_TAG_NUMBER || right.type != SAGE_TAG_NUMBER) sage_fail("Runtime Error: Operands must be numbers.");
    return sage_number((double)(((long long)left.as.number) << ((long long)right.as.number)));
}
static SageValue sage_rshift(SageValue left, SageValue right) {
    if (left.type != SAGE_TAG_NUMBER || right.type != SAGE_TAG_NUMBER) sage_fail("Runtime Error: Operands must be numbers.");
    return sage_number((double)(((long long)left.as.number) >> ((long long)right.as.number)));
}

static SageValue sage_tonumber(SageValue value) {
    if (value.type == SAGE_TAG_NUMBER) return value;
    if (value.type == SAGE_TAG_STRING) {
        char* end;
        double result = strtod(value.as.string, &end);
        if (end != value.as.string && *end == '\0') return sage_number(result);
    }
    return sage_nil();
}

static SageValue sage_dict_keys_fn(SageValue dict_val) {
    if (dict_val.type != SAGE_TAG_DICT) return sage_array();
    sage_gc_pin();
    SageValue result = sage_array();
    for (int i = 0; i < dict_val.as.dict->count; i++) {
        sage_array_push_raw(result.as.array, sage_string(dict_val.as.dict->keys[i]));
    }
    sage_gc_unpin();
    return result;
}

static SageValue sage_dict_values_fn(SageValue dict_val) {
    if (dict_val.type != SAGE_TAG_DICT) return sage_array();
    sage_gc_pin();
    SageValue result = sage_array();
    for (int i = 0; i < dict_val.as.dict->count; i++) {
        sage_array_push_raw(result.as.array, dict_val.as.dict->values[i]);
    }
    sage_gc_unpin();
    return result;
}

static SageValue sage_dict_has_fn(SageValue dict_val, SageValue key) {
    if (dict_val.type != SAGE_TAG_DICT || key.type != SAGE_TAG_STRING) return sage_bool(0);
    for (int i = 0; i < dict_val.as.dict->count; i++) {
        if (strcmp(dict_val.as.dict->keys[i], key.as.string) == 0) return sage_bool(1);
    }
    return sage_bool(0);
}

static SageValue sage_dict_delete_fn(SageValue dict_val, SageValue key) {
    if (dict_val.type != SAGE_TAG_DICT || key.type != SAGE_TAG_STRING) return sage_nil();
    SageDict* dict = dict_val.as.dict;
    for (int i = 0; i < dict->count; i++) {
        if (strcmp(dict->keys[i], key.as.string) == 0) {
            free(dict->keys[i]);
            for (int j = i; j < dict->count - 1; j++) {
                dict->keys[j] = dict->keys[j + 1];
                dict->values[j] = dict->values[j + 1];
            }
            dict->count--;
            return sage_bool(1);
        }
    }
    return sage_bool(0);
}

static SageValue sage_chr(SageValue v) {
    if (v.type != SAGE_TAG_NUMBER) return sage_nil();
    char buf[2] = { (char)(int)v.as.number, 0 };
    return sage_string(buf);
}

static SageValue sage_ord(SageValue v) {
    if (v.type != SAGE_TAG_STRING || v.as.string == NULL || v.as.string[0] == 0) return sage_nil();
    return sage_number((double)(unsigned char)v.as.string[0]);
}

static SageValue sage_type(SageValue v) {
    switch (v.type) {
        case SAGE_TAG_NIL: return sage_string("nil");
        case SAGE_TAG_NUMBER: return sage_string("number");
        case SAGE_TAG_BOOL: return sage_string("bool");
        case SAGE_TAG_STRING: return sage_string("string");
        case SAGE_TAG_ARRAY: return sage_string("array");
        case SAGE_TAG_DICT: return sage_string("dict");
        default: return sage_string("unknown");
    }
}

static SageValue sage_startswith(SageValue s, SageValue prefix) {
    if (s.type != SAGE_TAG_STRING || prefix.type != SAGE_TAG_STRING) return sage_bool(0);
    return sage_bool(strncmp(s.as.string, prefix.as.string, strlen(prefix.as.string)) == 0);
}

static SageValue sage_endswith(SageValue s, SageValue suffix) {
    if (s.type != SAGE_TAG_STRING || suffix.type != SAGE_TAG_STRING) return sage_bool(0);
    size_t slen = strlen(s.as.string), suflen = strlen(suffix.as.string);
    if (suflen > slen) return sage_bool(0);
    return sage_bool(strcmp(s.as.string + slen - suflen, suffix.as.string) == 0);
}

static SageValue sage_contains(SageValue haystack, SageValue needle) {
    if (haystack.type != SAGE_TAG_STRING || needle.type != SAGE_TAG_STRING) return sage_bool(0);
    return sage_bool(strstr(haystack.as.string, needle.as.string) != NULL);
}

static SageValue sage_indexof(SageValue haystack, SageValue needle) {
    if (haystack.type != SAGE_TAG_STRING || needle.type != SAGE_TAG_STRING) return sage_nil();
    char* found = strstr(haystack.as.string, needle.as.string);
    if (found == NULL) return sage_number(-1);
    return sage_number((double)(found - haystack.as.string));
}

static void sage_index_set(SageValue c, SageValue k, SageValue v) {
    if (c.type == SAGE_TAG_ARRAY && k.type == SAGE_TAG_NUMBER) {
        int i = (int)k.as.number;
        if (i >= 0 && i < c.as.array->count) c.as.array->elements[i] = v;
        return;
    }
    if (c.type == SAGE_TAG_DICT && k.type == SAGE_TAG_STRING) {
        SageDict* d = c.as.dict;
        for (int i = 0; i < d->count; i++) {
            if (strcmp(d->keys[i], k.as.string) == 0) { d->values[i] = v; return; }
        }
        if (d->count >= d->capacity) {
            int nc = d->capacity == 0 ? 4 : d->capacity * 2;
            d->keys = realloc(d->keys, sizeof(char*) * nc);
            d->values = realloc(d->values, sizeof(SageValue) * nc);
            d->capacity = nc;
        }
        { size_t l = strlen(k.as.string); d->keys[d->count] = malloc(l+1); memcpy(d->keys[d->count], k.as.string, l+1); }
        d->values[d->count] = v;
        d->count++;
    }
}

static SageValue sage_gc_collect_fn(void) {
    sage_gc_collect();
    return sage_nil();
}

static SageValue sage_gc_enable_fn(void) {
    sage_gc.enabled = 1;
    return sage_nil();
}

static SageValue sage_gc_disable_fn(void) {
    sage_gc.enabled = 0;
    return sage_nil();
}

static SageValue sage_gc_stats_fn(void) {
    int next_gc = sage_gc.next_gc_objects - sage_gc.object_count;
    if (next_gc < 0) next_gc = 0;
    return sage_make_dict_from_entries(7,
        (const char*[]){"bytes_allocated", "current_bytes", "num_objects", "collections", "objects_freed", "next_gc", "next_gc_bytes"},
        (SageValue[]){
            sage_number((double)sage_gc.bytes_allocated),
            sage_number((double)sage_gc_live_bytes()),
            sage_number((double)sage_gc.object_count),
            sage_number((double)sage_gc.collections),
            sage_number(0),
            sage_number((double)next_gc),
            sage_number((double)sage_gc.next_gc_bytes)
        });
}

static SageValue sage_gc_collections_fn(void) {
    return sage_number((double)sage_gc.collections);
}

#include <ctype.h>
static SageValue sage_upper(SageValue value) {
    if (value.type != SAGE_TAG_STRING) return sage_nil();
    size_t len = strlen(value.as.string);
    char* result = (char*)malloc(len + 1);
    if (result == NULL) sage_fail("Runtime Error: out of memory");
    for (size_t i = 0; i < len; i++) result[i] = (char)toupper((unsigned char)value.as.string[i]);
    result[len] = '\0';
    return sage_string_take(result);
}
static SageValue sage_lower(SageValue value) {
    if (value.type != SAGE_TAG_STRING) return sage_nil();
    size_t len = strlen(value.as.string);
    char* result = (char*)malloc(len + 1);
    if (result == NULL) sage_fail("Runtime Error: out of memory");
    for (size_t i = 0; i < len; i++) result[i] = (char)tolower((unsigned char)value.as.string[i]);
    result[len] = '\0';
    return sage_string_take(result);
}
static SageValue sage_strip_fn(SageValue value) {
    if (value.type != SAGE_TAG_STRING) return sage_nil();
    const char* s = value.as.string;
    while (*s && isspace((unsigned char)*s)) s++;
    const char* end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) end--;
    size_t len = (size_t)(end - s);
    char* result = (char*)malloc(len + 1);
    if (result == NULL) sage_fail("Runtime Error: out of memory");
    memcpy(result, s, len);
    result[len] = '\0';
    return sage_string_take(result);
}

static SageValue sage_split_fn(SageValue str_val, SageValue delim_val) {
    if (str_val.type != SAGE_TAG_STRING || delim_val.type != SAGE_TAG_STRING) return sage_array();
    sage_gc_pin();
    const char* s = str_val.as.string;
    const char* delim = delim_val.as.string;
    size_t dlen = strlen(delim);
    SageValue result = sage_array();
    if (dlen == 0) {
        for (size_t i = 0; s[i]; i++) {
            char buf[2] = {s[i], '\0'};
            sage_array_push_raw(result.as.array, sage_string(buf));
        }
        sage_gc_unpin();
        return result;
    }
    const char* start = s;
    const char* found;
    while ((found = strstr(start, delim)) != NULL) {
        size_t len = (size_t)(found - start);
        char* part = (char*)malloc(len + 1);
        if (part == NULL) sage_fail("Runtime Error: out of memory");
        memcpy(part, start, len);
        part[len] = '\0';
        sage_array_push_raw(result.as.array, sage_string_take(part));
        start = found + dlen;
    }
    sage_array_push_raw(result.as.array, sage_string(start));
    sage_gc_unpin();
    return result;
}

static SageValue sage_join_fn(SageValue arr_val, SageValue delim_val) {
    if (arr_val.type != SAGE_TAG_ARRAY || delim_val.type != SAGE_TAG_STRING) return sage_nil();
    SageArray* arr = arr_val.as.array;
    const char* delim = delim_val.as.string;
    size_t dlen = strlen(delim);
    if (arr->count == 0) return sage_string("");
    size_t total = 0;
    for (int i = 0; i < arr->count; i++) {
        if (arr->elements[i].type == SAGE_TAG_STRING) total += strlen(arr->elements[i].as.string);
        if (i > 0) total += dlen;
    }
    char* result = (char*)malloc(total + 1);
    if (result == NULL) sage_fail("Runtime Error: out of memory");
    char* p = result;
    for (int i = 0; i < arr->count; i++) {
        if (i > 0) { memcpy(p, delim, dlen); p += dlen; }
        if (arr->elements[i].type == SAGE_TAG_STRING) {
            size_t len = strlen(arr->elements[i].as.string);
            memcpy(p, arr->elements[i].as.string, len);
            p += len;
        }
    }
    *p = '\0';
    return sage_string_take(result);
}

static SageValue sage_replace_fn(SageValue str_val, SageValue old_val, SageValue new_val) {
    if (str_val.type != SAGE_TAG_STRING || old_val.type != SAGE_TAG_STRING || new_val.type != SAGE_TAG_STRING)
        return sage_nil();
    const char* s = str_val.as.string;
    const char* old_s = old_val.as.string;
    const char* new_s = new_val.as.string;
    size_t old_len = strlen(old_s);
    size_t new_len = strlen(new_s);
    if (old_len == 0) return sage_string(s);
    size_t count = 0;
    const char* tmp = s;
    while ((tmp = strstr(tmp, old_s)) != NULL) { count++; tmp += old_len; }
    size_t result_len = strlen(s) + count * (new_len - old_len);
    char* result = (char*)malloc(result_len + 1);
    if (result == NULL) sage_fail("Runtime Error: out of memory");
    char* p = result;
    while (*s) {
        if (strncmp(s, old_s, old_len) == 0) {
            memcpy(p, new_s, new_len);
            p += new_len;
            s += old_len;
        } else {
            *p++ = *s++;
        }
    }
    *p = '\0';
    return sage_string_take(result);
}

#include <stdint.h>

typedef struct {
    void* ptr;
    size_t size;
    int owned;
} SagePointer;

static SageValue sage_mem_alloc(SageValue size_val) {
    if (size_val.type != SAGE_TAG_NUMBER) { fputs("mem_alloc(): expects number\n", stderr); return sage_nil(); }
    size_t size = (size_t)size_val.as.number;
    if (size == 0 || size > 1024*1024*64) { fputs("mem_alloc(): invalid size\n", stderr); return sage_nil(); }
    SagePointer* sp = (SagePointer*)malloc(sizeof(SagePointer));
    if (sp == NULL) sage_fail("Runtime Error: out of memory");
    sp->ptr = calloc(1, size);
    if (sp->ptr == NULL) { free(sp); sage_fail("Runtime Error: out of memory"); }
    sp->size = size;
    sp->owned = 1;
    SageValue v; v.type = SAGE_TAG_NUMBER; v.as.number = (double)(uintptr_t)sp;
    return v;
}

static SagePointer* sage_as_pointer(SageValue v) {
    if (v.type != SAGE_TAG_NUMBER) return NULL;
    return (SagePointer*)(uintptr_t)v.as.number;
}

static SageValue sage_mem_free(SageValue ptr_val) {
    SagePointer* sp = sage_as_pointer(ptr_val);
    if (sp == NULL) { fputs("mem_free(): expects pointer\n", stderr); return sage_nil(); }
    if (sp->ptr && sp->owned) { free(sp->ptr); sp->ptr = NULL; sp->size = 0; }
    free(sp);
    return sage_nil();
}

static SageValue sage_mem_read(SageValue ptr_val, SageValue off_val, SageValue type_val) {
    SagePointer* sp = sage_as_pointer(ptr_val);
    if (sp == NULL || sp->ptr == NULL || off_val.type != SAGE_TAG_NUMBER || type_val.type != SAGE_TAG_STRING)
        return sage_nil();
    size_t offset = (size_t)off_val.as.number;
    const char* type = type_val.as.string;
    unsigned char* base = (unsigned char*)sp->ptr + offset;
    if (strcmp(type, "byte") == 0) { return sage_number((double)*base); }
    if (strcmp(type, "int") == 0) { int v; memcpy(&v, base, sizeof(int)); return sage_number((double)v); }
    if (strcmp(type, "double") == 0) { double v; memcpy(&v, base, sizeof(double)); return sage_number(v); }
    if (strcmp(type, "string") == 0) { return sage_string((const char*)base); }
    return sage_nil();
}

static SageValue sage_mem_write(SageValue ptr_val, SageValue off_val, SageValue type_val, SageValue val) {
    SagePointer* sp = sage_as_pointer(ptr_val);
    if (sp == NULL || sp->ptr == NULL || off_val.type != SAGE_TAG_NUMBER || type_val.type != SAGE_TAG_STRING)
        return sage_nil();
    size_t offset = (size_t)off_val.as.number;
    const char* type = type_val.as.string;
    unsigned char* base = (unsigned char*)sp->ptr + offset;
    if (strcmp(type, "byte") == 0 && val.type == SAGE_TAG_NUMBER) { *base = (unsigned char)val.as.number; }
    else if (strcmp(type, "int") == 0 && val.type == SAGE_TAG_NUMBER) { int v = (int)val.as.number; memcpy(base, &v, sizeof(int)); }
    else if (strcmp(type, "double") == 0 && val.type == SAGE_TAG_NUMBER) { double v = val.as.number; memcpy(base, &v, sizeof(double)); }
    return sage_nil();
}

static SageValue sage_mem_size(SageValue ptr_val) {
    SagePointer* sp = sage_as_pointer(ptr_val);
    if (sp == NULL) return sage_nil();
    return sage_number((double)sp->size);
}

static int sage_struct_type_info(const char* type, size_t* out_size, size_t* out_align) {
    if (strcmp(type,"char")==0||strcmp(type,"byte")==0) { *out_size=1; *out_align=1; return 0; }
    if (strcmp(type,"short")==0) { *out_size=sizeof(short); *out_align=sizeof(short); return 0; }
    if (strcmp(type,"int")==0) { *out_size=sizeof(int); *out_align=sizeof(int); return 0; }
    if (strcmp(type,"long")==0) { *out_size=sizeof(long); *out_align=sizeof(long); return 0; }
    if (strcmp(type,"float")==0) { *out_size=sizeof(float); *out_align=sizeof(float); return 0; }
    if (strcmp(type,"double")==0) { *out_size=sizeof(double); *out_align=sizeof(double); return 0; }
    if (strcmp(type,"ptr")==0) { *out_size=sizeof(void*); *out_align=sizeof(void*); return 0; }
    return -1;
}

static SageValue sage_struct_def(SageValue fields) {
    if (fields.type != SAGE_TAG_ARRAY) return sage_nil();
    sage_gc_pin();
    SageValue def = sage_make_dict();
    size_t offset = 0, max_align = 1;
    for (int i = 0; i < fields.as.array->count; i++) {
        SageValue pair = fields.as.array->elements[i];
        if (pair.type != SAGE_TAG_ARRAY || pair.as.array->count < 2) continue;
        if (pair.as.array->elements[0].type != SAGE_TAG_STRING ||
            pair.as.array->elements[1].type != SAGE_TAG_STRING) continue;
        const char* name = pair.as.array->elements[0].as.string;
        const char* type = pair.as.array->elements[1].as.string;
        size_t fsize, falign;
        if (sage_struct_type_info(type, &fsize, &falign) != 0) continue;
        if (falign > max_align) max_align = falign;
        size_t rem = offset % falign;
        if (rem != 0) offset += falign - rem;
        /* store field: "name" -> [offset, type] */
        SageValue field_info = sage_make_array(2, (SageValue[]){
            sage_number((double)offset), sage_string(type)
        });
        sage_dict_set(def.as.dict, name, field_info);
        offset += fsize;
    }
    size_t rem = offset % max_align;
    if (rem != 0) offset += max_align - rem;
    sage_dict_set(def.as.dict, "__size__", sage_number((double)offset));
    sage_dict_set(def.as.dict, "__align__", sage_number((double)max_align));
    sage_gc_unpin();
    return def;
}

static SageValue sage_struct_new(SageValue def) {
    if (def.type != SAGE_TAG_DICT) return sage_nil();
    SageValue size_val = sage_dict_get(def.as.dict, "__size__");
    if (size_val.type != SAGE_TAG_NUMBER) return sage_nil();
    size_t size = (size_t)size_val.as.number;
    SagePointer* sp = (SagePointer*)malloc(sizeof(SagePointer));
    if (sp == NULL) sage_fail("Runtime Error: out of memory");
    sp->ptr = calloc(1, size);
    if (sp->ptr == NULL) { free(sp); sage_fail("Runtime Error: out of memory"); }
    sp->size = size;
    sp->owned = 1;
    SageValue v; v.type = SAGE_TAG_NUMBER; v.as.number = (double)(uintptr_t)sp;
    return v;
}

static SageValue sage_struct_get(SageValue ptr_val, SageValue def, SageValue field_name) {
    SagePointer* sp = sage_as_pointer(ptr_val);
    if (sp == NULL || sp->ptr == NULL || def.type != SAGE_TAG_DICT || field_name.type != SAGE_TAG_STRING)
        return sage_nil();
    SageValue info = sage_dict_get(def.as.dict, field_name.as.string);
    if (info.type != SAGE_TAG_ARRAY || info.as.array->count < 2) return sage_nil();
    size_t offset = (size_t)info.as.array->elements[0].as.number;
    const char* type = info.as.array->elements[1].as.string;
    unsigned char* base = (unsigned char*)sp->ptr + offset;
    if (strcmp(type,"char")==0||strcmp(type,"byte")==0) return sage_number((double)*base);
    if (strcmp(type,"short")==0) { short v; memcpy(&v,base,sizeof(short)); return sage_number((double)v); }
    if (strcmp(type,"int")==0) { int v; memcpy(&v,base,sizeof(int)); return sage_number((double)v); }
    if (strcmp(type,"long")==0) { long v; memcpy(&v,base,sizeof(long)); return sage_number((double)v); }
    if (strcmp(type,"float")==0) { float v; memcpy(&v,base,sizeof(float)); return sage_number((double)v); }
    if (strcmp(type,"double")==0) { double v; memcpy(&v,base,sizeof(double)); return sage_number(v); }
    return sage_nil();
}

static SageValue sage_struct_set(SageValue ptr_val, SageValue def, SageValue field_name, SageValue val) {
    SagePointer* sp = sage_as_pointer(ptr_val);
    if (sp == NULL || sp->ptr == NULL || def.type != SAGE_TAG_DICT || field_name.type != SAGE_TAG_STRING)
        return sage_nil();
    SageValue info = sage_dict_get(def.as.dict, field_name.as.string);
    if (info.type != SAGE_TAG_ARRAY || info.as.array->count < 2) return sage_nil();
    size_t offset = (size_t)info.as.array->elements[0].as.number;
    const char* type = info.as.array->elements[1].as.string;
    unsigned char* base = (unsigned char*)sp->ptr + offset;
    if (val.type != SAGE_TAG_NUMBER) return sage_nil();
    if (strcmp(type,"char")==0||strcmp(type,"byte")==0) { *base = (unsigned char)val.as.number; }
    else if (strcmp(type,"short")==0) { short v=(short)val.as.number; memcpy(base,&v,sizeof(short)); }
    else if (strcmp(type,"int")==0) { int v=(int)val.as.number; memcpy(base,&v,sizeof(int)); }
    else if (strcmp(type,"long")==0) { long v=(long)val.as.number; memcpy(base,&v,sizeof(long)); }
    else if (strcmp(type,"float")==0) { float v=(float)val.as.number; memcpy(base,&v,sizeof(float)); }
    else if (strcmp(type,"double")==0) { double v=val.as.number; memcpy(base,&v,sizeof(double)); }
    return sage_nil();
}

static SageValue sage_struct_size(SageValue def) {
    if (def.type != SAGE_TAG_DICT) return sage_nil();
    return sage_dict_get(def.as.dict, "__size__");
}

typedef SageValue (*SageMethodFn)(SageValue, int, SageValue*);
typedef struct { const char* class_name; const char* method_name; SageMethodFn fn; } SageMethodEntry;
typedef struct { const char* name; const char* parent; } SageClassEntry;
#define SAGE_MAX_METHODS 256
#define SAGE_MAX_CLASSES 64
static SageMethodEntry sage_method_table[SAGE_MAX_METHODS];
static int sage_method_count = 0;
static SageClassEntry sage_class_registry[SAGE_MAX_CLASSES];
static int sage_class_count = 0;

static void sage_register_class(const char* name, const char* parent) {
    if (sage_class_count >= SAGE_MAX_CLASSES) sage_fail("too many classes");
    sage_class_registry[sage_class_count].name = name;
    sage_class_registry[sage_class_count].parent = parent;
    sage_class_count++;
}

static void sage_register_method(const char* cls, const char* name, SageMethodFn fn) {
    if (sage_method_count >= SAGE_MAX_METHODS) sage_fail("too many methods");
    sage_method_table[sage_method_count].class_name = cls;
    sage_method_table[sage_method_count].method_name = name;
    sage_method_table[sage_method_count].fn = fn;
    sage_method_count++;
}

static SageValue sage_call_method(SageValue obj, const char* method, int argc, SageValue* argv) {
    if (obj.type != SAGE_TAG_DICT) {
        fprintf(stderr, "Runtime Error: method call on non-instance (type=%d).\n", obj.type);
        exit(1);
    }
    SageValue class_val = sage_dict_get(obj.as.dict, "__class__");
    if (class_val.type != SAGE_TAG_STRING) {
        fprintf(stderr, "Runtime Error: no __class__ on instance (method=%s class_val_type=%d).\n", method, class_val.type);
        exit(1);
    }
    const char* current = class_val.as.string;
    while (current != NULL) {
        for (int i = 0; i < sage_method_count; i++) {
            if (strcmp(sage_method_table[i].class_name, current) == 0 &&
                strcmp(sage_method_table[i].method_name, method) == 0) {
                return sage_method_table[i].fn(obj, argc, argv);
            }
        }
        const char* parent = NULL;
        for (int j = 0; j < sage_class_count; j++) {
            if (strcmp(sage_class_registry[j].name, current) == 0) {
                parent = sage_class_registry[j].parent;
                break;
            }
        }
        current = parent;
    }
    fprintf(stderr, "Runtime Error: Undefined method '%s'.\n", method);
    exit(1);
    return sage_nil();
}

static SageValue sage_construct(const char* class_name, const char* parent_name, int argc, SageValue* argv) {
    sage_gc_pin();
    SageValue inst = sage_make_dict();
    sage_dict_set(inst.as.dict, "__class__", sage_string(class_name));
    if (parent_name != NULL) sage_dict_set(inst.as.dict, "__parent__", sage_string(parent_name));
    sage_gc_unpin();
    const char* current = class_name;
    while (current != NULL) {
        for (int i = 0; i < sage_method_count; i++) {
            if (strcmp(sage_method_table[i].class_name, current) == 0 &&
                strcmp(sage_method_table[i].method_name, "init") == 0) {
                sage_method_table[i].fn(inst, argc, argv);
                return inst;
            }
        }
        const char* parent = NULL;
        for (int j = 0; j < sage_class_count; j++) {
            if (strcmp(sage_class_registry[j].name, current) == 0) {
                parent = sage_class_registry[j].parent;
                break;
            }
        }
        current = parent;
    }
    return inst;
}

static SageValue sage_arch_fn(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return sage_string("x86_64");
#elif defined(__aarch64__) || defined(_M_ARM64)
    return sage_string("aarch64");
#elif defined(__riscv) && __riscv_xlen == 64
    return sage_string("rv64");
#else
    return sage_string("unknown");
#endif
}

#include <time.h>
static SageValue sage_clock_fn(void) {
    return sage_number((double)clock() / CLOCKS_PER_SEC);
}
static SageValue sage_input_fn(SageValue prompt) {
    if (prompt.type == SAGE_TAG_STRING) fputs(prompt.as.string, stdout);
    char buf[4096];
    if (fgets(buf, sizeof(buf), stdin) == NULL) return sage_nil();
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
    return sage_string(buf);
}
static SageValue sage_sys_args(void) {
    extern int sage_argc; extern char** sage_argv;
    SageValue list = sage_array();
    for(int i=0; i<sage_argc; i++) sage_push(list, sage_string(sage_argv[i]));
    return list;
}
static SageValue sage_sys_exec(SageValue cmd) {
    if(cmd.type != SAGE_TAG_STRING) return sage_number(-1);
    return sage_number(system(cmd.as.string));
}
static SageValue sage_io_readfile(SageValue p) {
    if(p.type != SAGE_TAG_STRING) return sage_nil();
    FILE* f = fopen(p.as.string, "rb"); if(!f) return sage_nil();
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = malloc(size + 1); if(!buf) { fclose(f); return sage_nil(); }
    fread(buf, 1, size, f); buf[size] = 0; fclose(f);
    return sage_string_take(buf);
}
static SageValue sage_io_writefile(SageValue p, SageValue c) {
    if(p.type != SAGE_TAG_STRING || c.type != SAGE_TAG_STRING) return sage_bool(0);
    FILE* f = fopen(p.as.string, "wb"); if(!f) return sage_bool(0);
    fwrite(c.as.string, 1, strlen(c.as.string), f); fclose(f); return sage_bool(1);
}
static SageValue sage_io_writebytes(SageValue p, SageValue arr) {
    if(p.type != SAGE_TAG_STRING || arr.type != SAGE_TAG_ARRAY) return sage_bool(0);
    FILE* f = fopen(p.as.string, "wb"); if(!f) return sage_bool(0);
    SageArray* a = arr.as.array;
    unsigned char* buf = malloc(a->count);
    for(int i=0; i<a->count; i++) buf[i] = (unsigned char)a->elements[i].as.number;
    fwrite(buf, 1, a->count, f); fclose(f); free(buf); return sage_bool(1);
}
static SageValue sage_io_readbytes(SageValue p) {
    if(p.type != SAGE_TAG_STRING) return sage_nil();
    FILE* f = fopen(p.as.string, "rb"); if(!f) return sage_nil();
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    SageValue arr = sage_array();
    if (size > 0) {
        unsigned char* buf = malloc(size);
        fread(buf, 1, size, f);
        for(int i=0; i<size; i++) sage_push(arr, sage_number((double)buf[i]));
        free(buf);
    }
    fclose(f);
    return arr;
}
static SageValue sage_io_exists(SageValue p) {
    if(p.type != SAGE_TAG_STRING) return sage_bool(0);
    FILE* f = fopen(p.as.string, "r"); if(f){ fclose(f); return sage_bool(1); } return sage_bool(0);
}
static SageValue sage_string_substr(SageValue s, SageValue start, SageValue len) {
    if(s.type != SAGE_TAG_STRING || start.type != SAGE_TAG_NUMBER || len.type != SAGE_TAG_NUMBER) return sage_nil();
    int st = (int)start.as.number; int l = (int)len.as.number;
    int slen = strlen(s.as.string);
    if(st < 0 || st > slen) return sage_string("");
    if(l < 0) l = 0; if(st + l > slen) l = slen - st;
    char* buf = malloc(l + 1); if(!buf) return sage_nil();
    memcpy(buf, s.as.string + st, l); buf[l] = 0;
    return sage_string_take(buf);
}

static SageValue sage_fn_main_99();
static SageValue sage_method_MetalVM_init(SageValue _self, int _argc, SageValue* _argv);
static SageValue sage_method_MetalVM_setup_builtins(SageValue _self, int _argc, SageValue* _argv);
static SageValue sage_method_MetalVM_run(SageValue _self, int _argc, SageValue* _argv);
static SageValue sage_method_MetalVM_run_step(SageValue _self, int _argc, SageValue* _argv);
static SageValue sage_method_SGVMUtils_my_int(SageValue _self, int _argc, SageValue* _argv);
static SageValue sage_method_SGVMUtils_hex_to_byte(SageValue _self, int _argc, SageValue* _argv);
static SageValue sage_method_SGVMUtils_split_lines(SageValue _self, int _argc, SageValue* _argv);
static SageValue sage_method_SGVMUtils_my_substr(SageValue _self, int _argc, SageValue* _argv);
static SageValue sage_method_SGVMUtils_parse_int_field(SageValue _self, int _argc, SageValue* _argv);
static SageValue sage_method_SGVMUtils_parse_hex_byte(SageValue _self, int _argc, SageValue* _argv);
static SageValue sage_method_SGVMUtils_trim(SageValue _self, int _argc, SageValue* _argv);
static SageValue sage_method_SGVMUtils_read_be16(SageValue _self, int _argc, SageValue* _argv);
static SageValue sage_method_SGVMUtils_read_be32(SageValue _self, int _argc, SageValue* _argv);
static SageValue sage_method_SGVMUtils_unpack_double(SageValue _self, int _argc, SageValue* _argv);

static SageSlot sage_global_g_gil_98;
static SageSlot sage_global_OP_HALT_97;
static SageSlot sage_global_OP_GPU_CMD_DISPATCH_96;
static SageSlot sage_global_OP_GPU_CMD_PUSH_CONST_95;
static SageSlot sage_global_OP_GPU_UPDATE_UNIFORM_94;
static SageSlot sage_global_OP_GPU_RESET_FENCE_93;
static SageSlot sage_global_OP_GPU_WAIT_FENCE_92;
static SageSlot sage_global_OP_GPU_PRESENT_91;
static SageSlot sage_global_OP_GPU_ACQUIRE_IMG_90;
static SageSlot sage_global_OP_GPU_SUBMIT_SYNC_89;
static SageSlot sage_global_OP_GPU_CMD_DRAW_IDX_88;
static SageSlot sage_global_OP_GPU_CMD_BIND_IB_87;
static SageSlot sage_global_OP_GPU_CMD_BIND_VB_86;
static SageSlot sage_global_OP_GPU_CMD_SET_SC_85;
static SageSlot sage_global_OP_GPU_CMD_SET_VP_84;
static SageSlot sage_global_OP_GPU_CMD_BIND_DS_83;
static SageSlot sage_global_OP_GPU_CMD_BIND_GP_82;
static SageSlot sage_global_OP_GPU_CMD_DRAW_81;
static SageSlot sage_global_OP_GPU_CMD_END_RP_80;
static SageSlot sage_global_OP_GPU_CMD_BEGIN_RP_79;
static SageSlot sage_global_OP_GPU_END_COMMANDS_78;
static SageSlot sage_global_OP_GPU_BEGIN_COMMANDS_77;
static SageSlot sage_global_OP_GPU_UPDATE_INPUT_76;
static SageSlot sage_global_OP_GPU_MOUSE_DELTA_75;
static SageSlot sage_global_OP_GPU_MOUSE_POS_74;
static SageSlot sage_global_OP_GPU_KEY_DOWN_73;
static SageSlot sage_global_OP_GPU_KEY_PRESSED_72;
static SageSlot sage_global_OP_GPU_GET_TIME_71;
static SageSlot sage_global_OP_GPU_WINDOW_SHOULD_CLOSE_70;
static SageSlot sage_global_OP_GPU_POLL_EVENTS_69;
static SageSlot sage_global_OP_RAISE_68;
static SageSlot sage_global_OP_END_TRY_67;
static SageSlot sage_global_OP_SETUP_TRY_66;
static SageSlot sage_global_OP_INHERIT_65;
static SageSlot sage_global_OP_METHOD_64;
static SageSlot sage_global_OP_CLASS_63;
static SageSlot sage_global_OP_IMPORT_62;
static SageSlot sage_global_OP_LOOP_BACK_61;
static SageSlot sage_global_OP_CONTINUE_60;
static SageSlot sage_global_OP_BREAK_59;
static SageSlot sage_global_OP_ARRAY_LEN_58;
static SageSlot sage_global_OP_DUP_57;
static SageSlot sage_global_OP_POP_ENV_56;
static SageSlot sage_global_OP_PUSH_ENV_55;
static SageSlot sage_global_OP_RETURN_54;
static SageSlot sage_global_OP_EXEC_AST_STMT_53;
static SageSlot sage_global_OP_PRINT_52;
static SageSlot sage_global_OP_DICT_51;
static SageSlot sage_global_OP_TUPLE_50;
static SageSlot sage_global_OP_ARRAY_49;
static SageSlot sage_global_OP_CALL_METHOD_48;
static SageSlot sage_global_OP_CALL_47;
static SageSlot sage_global_OP_JUMP_IF_FALSE_46;
static SageSlot sage_global_OP_JUMP_45;
static SageSlot sage_global_OP_TRUTHY_44;
static SageSlot sage_global_OP_NOT_43;
static SageSlot sage_global_OP_SHIFT_RIGHT_42;
static SageSlot sage_global_OP_SHIFT_LEFT_41;
static SageSlot sage_global_OP_BIT_NOT_40;
static SageSlot sage_global_OP_BIT_XOR_39;
static SageSlot sage_global_OP_BIT_OR_38;
static SageSlot sage_global_OP_BIT_AND_37;
static SageSlot sage_global_OP_LESS_EQUAL_36;
static SageSlot sage_global_OP_LESS_35;
static SageSlot sage_global_OP_GREATER_EQUAL_34;
static SageSlot sage_global_OP_GREATER_33;
static SageSlot sage_global_OP_NOT_EQUAL_32;
static SageSlot sage_global_OP_EQUAL_31;
static SageSlot sage_global_OP_NEGATE_30;
static SageSlot sage_global_OP_MOD_29;
static SageSlot sage_global_OP_DIV_28;
static SageSlot sage_global_OP_MUL_27;
static SageSlot sage_global_OP_SUB_26;
static SageSlot sage_global_OP_ADD_25;
static SageSlot sage_global_OP_SLICE_24;
static SageSlot sage_global_OP_LOAD_FUNCTION_23;
static SageSlot sage_global_OP_SET_INDEX_22;
static SageSlot sage_global_OP_GET_INDEX_21;
static SageSlot sage_global_OP_SET_PROPERTY_20;
static SageSlot sage_global_OP_GET_PROPERTY_19;
static SageSlot sage_global_OP_DEFINE_FUNCTION_18;
static SageSlot sage_global_OP_SET_GLOBAL_17;
static SageSlot sage_global_OP_DEFINE_GLOBAL_16;
static SageSlot sage_global_OP_GET_GLOBAL_15;
static SageSlot sage_global_OP_POP_14;
static SageSlot sage_global_OP_FALSE_13;
static SageSlot sage_global_OP_TRUE_12;
static SageSlot sage_global_OP_NIL_11;
static SageSlot sage_global_OP_CONSTANT_10;
static SageSlot sage_global_sgvm_core_9;
static SageSlot sage_global_ml_native_8;
static SageSlot sage_global_gpu_7;
static SageSlot sage_global_host_thread_6;
static SageSlot sage_global_net_5;
static SageSlot sage_global_math_4;
static SageSlot sage_global_sgvm_vm_3;
static SageSlot sage_global_io_2;
static SageSlot sage_global_sys_1;

static SageValue sage_method_MetalVM_init(SageValue _self, int _argc, SageValue* _argv) {
    SageSlot sage_local_self_100 = sage_slot_undefined();
    SageSlot* sage_gc_roots[1] = {&sage_local_self_100};
    SageGcFrame sage_gc_frame;
    sage_gc_push_frame(&sage_gc_frame, sage_gc_roots, 1);
    sage_define_slot(&sage_local_self_100, _self);
    (void)_argc;
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_make_array(0, NULL); sage_dict_set(_obj.as.dict, "stack", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_make_array(0, NULL); sage_dict_set(_obj.as.dict, "constants", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_make_array(0, NULL); sage_dict_set(_obj.as.dict, "chunks", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_make_dict(); sage_dict_set(_obj.as.dict, "globals", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_make_array(0, NULL); sage_dict_set(_obj.as.dict, "scopes", _val); _val;});
    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_100, "self"), sage_string("scopes")), sage_make_dict());
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_make_array(0, NULL); sage_dict_set(_obj.as.dict, "handlers", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_number(0); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_make_array(0, NULL); sage_dict_set(_obj.as.dict, "code", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_bool(0); sage_dict_set(_obj.as.dict, "halted", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_bool(0); sage_dict_set(_obj.as.dict, "is_throwing", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_nil(); sage_dict_set(_obj.as.dict, "exception_value", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_bool(0); sage_dict_set(_obj.as.dict, "trace", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_make_dict(); sage_dict_set(_obj.as.dict, "modules", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_construct("SGVMUtils", NULL, 0, NULL); sage_dict_set(_obj.as.dict, "utils", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_number(65536); sage_dict_set(_obj.as.dict, "max_stack_depth", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_number(0); sage_dict_set(_obj.as.dict, "call_depth", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_number(1024); sage_dict_set(_obj.as.dict, "max_call_depth", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_nil(); sage_dict_set(_obj.as.dict, "return_value", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_bool(0); sage_dict_set(_obj.as.dict, "returning", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_100, "self"); SageValue _val = sage_make_array(0, NULL); sage_dict_set(_obj.as.dict, "call_stack", _val); _val;});
    (void)sage_call_method(sage_load_slot(&sage_local_self_100, "self"), "setup_builtins", 0, NULL);
    return sage_gc_return(&sage_gc_frame, sage_nil());
}

static SageValue sage_method_MetalVM_setup_builtins(SageValue _self, int _argc, SageValue* _argv) {
    SageSlot sage_local_self_101 = sage_slot_undefined();
    SageSlot* sage_gc_roots[1] = {&sage_local_self_101};
    SageGcFrame sage_gc_frame;
    sage_gc_push_frame(&sage_gc_frame, sage_gc_roots, 1);
    sage_define_slot(&sage_local_self_101, _self);
    (void)_argc;
    (void)sage_index_set(sage_index(sage_load_slot(&sage_local_self_101, "self"), sage_string("globals")), sage_string("math"), sage_load_slot(&sage_global_math_4, "math"));
    (void)sage_index_set(sage_index(sage_load_slot(&sage_local_self_101, "self"), sage_string("globals")), sage_string("io"), sage_load_slot(&sage_global_io_2, "io"));
    (void)sage_index_set(sage_index(sage_load_slot(&sage_local_self_101, "self"), sage_string("globals")), sage_string("sys"), sage_load_slot(&sage_global_sys_1, "sys"));
    (void)sage_index_set(sage_index(sage_load_slot(&sage_local_self_101, "self"), sage_string("globals")), sage_string("net"), sage_load_slot(&sage_global_net_5, "net"));
    (void)sage_index_set(sage_index(sage_load_slot(&sage_local_self_101, "self"), sage_string("globals")), sage_string("thread"), sage_load_slot(&sage_global_host_thread_6, "host_thread"));
    (void)sage_index_set(sage_index(sage_load_slot(&sage_local_self_101, "self"), sage_string("globals")), sage_string("gpu"), sage_load_slot(&sage_global_gpu_7, "gpu"));
    (void)sage_index_set(sage_index(sage_load_slot(&sage_local_self_101, "self"), sage_string("globals")), sage_string("ml_native"), sage_load_slot(&sage_global_ml_native_8, "ml_native"));
    (void)sage_index_set(sage_index(sage_load_slot(&sage_local_self_101, "self"), sage_string("globals")), sage_string("clock"), sage_string("__builtin_clock"));
    (void)sage_index_set(sage_index(sage_load_slot(&sage_local_self_101, "self"), sage_string("globals")), sage_string("str"), sage_string("__builtin_str"));
    (void)sage_index_set(sage_index(sage_load_slot(&sage_local_self_101, "self"), sage_string("globals")), sage_string("int"), sage_string("__builtin_int"));
    (void)sage_index_set(sage_index(sage_load_slot(&sage_local_self_101, "self"), sage_string("globals")), sage_string("tonumber"), sage_string("__builtin_tonumber"));
    (void)sage_index_set(sage_index(sage_load_slot(&sage_local_self_101, "self"), sage_string("globals")), sage_string("len"), sage_string("__builtin_len"));
    (void)sage_index_set(sage_index(sage_load_slot(&sage_local_self_101, "self"), sage_string("globals")), sage_string("print"), sage_string("__builtin_print"));
    (void)sage_index_set(sage_index(sage_load_slot(&sage_local_self_101, "self"), sage_string("globals")), sage_string("range"), sage_string("__builtin_range"));
    return sage_gc_return(&sage_gc_frame, sage_nil());
}

static SageValue sage_method_MetalVM_run(SageValue _self, int _argc, SageValue* _argv) {
    SageSlot sage_local_code_103 = sage_slot_undefined();
    SageSlot sage_local_self_102 = sage_slot_undefined();
    SageSlot* sage_gc_roots[2] = {&sage_local_code_103, &sage_local_self_102};
    SageGcFrame sage_gc_frame;
    sage_gc_push_frame(&sage_gc_frame, sage_gc_roots, 2);
    sage_define_slot(&sage_local_self_102, _self);
    sage_define_slot(&sage_local_code_103, _argv[0]);
    (void)_argc;
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_102, "self"); SageValue _val = sage_load_slot(&sage_local_code_103, "code"); sage_dict_set(_obj.as.dict, "code", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_102, "self"); SageValue _val = sage_number(0); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_102, "self"); SageValue _val = sage_bool(0); sage_dict_set(_obj.as.dict, "halted", _val); _val;});
    (void)sage_native_thread_lock(sage_load_slot(&sage_global_g_gil_98, "g_gil"));
    while (sage_truthy(sage_and(sage_not(sage_index(sage_load_slot(&sage_local_self_102, "self"), sage_string("halted"))), sage_lt(sage_index(sage_load_slot(&sage_local_self_102, "self"), sage_string("ip")), sage_len(sage_index(sage_load_slot(&sage_local_self_102, "self"), sage_string("code"))))))) {
        if (sage_truthy(sage_not(sage_call_method(sage_load_slot(&sage_local_self_102, "self"), "run_step", 0, NULL)))) {
            break;
        }
    }
    (void)sage_native_thread_unlock(sage_load_slot(&sage_global_g_gil_98, "g_gil"));
    return sage_gc_return(&sage_gc_frame, sage_nil());
}

static SageValue sage_method_MetalVM_run_step(SageValue _self, int _argc, SageValue* _argv) {
    SageSlot sage_local_gx_181 = sage_slot_undefined();
    SageSlot sage_local_gy_180 = sage_slot_undefined();
    SageSlot sage_local_gz_179 = sage_slot_undefined();
    SageSlot sage_local_st_178 = sage_slot_undefined();
    SageSlot sage_local_data_177 = sage_slot_undefined();
    SageSlot sage_local_s_176 = sage_slot_undefined();
    SageSlot sage_local_f_175 = sage_slot_undefined();
    SageSlot sage_local_idxc_174 = sage_slot_undefined();
    SageSlot sage_local_fidx_173 = sage_slot_undefined();
    SageSlot sage_local_vo_172 = sage_slot_undefined();
    SageSlot sage_local_buf_171 = sage_slot_undefined();
    SageSlot sage_local_sx_170 = sage_slot_undefined();
    SageSlot sage_local_sy_169 = sage_slot_undefined();
    SageSlot sage_local_sw_168 = sage_slot_undefined();
    SageSlot sage_local_sh_167 = sage_slot_undefined();
    SageSlot sage_local_vx_166 = sage_slot_undefined();
    SageSlot sage_local_vy_165 = sage_slot_undefined();
    SageSlot sage_local_vw_164 = sage_slot_undefined();
    SageSlot sage_local_vh_163 = sage_slot_undefined();
    SageSlot sage_local_mind_162 = sage_slot_undefined();
    SageSlot sage_local_maxd_161 = sage_slot_undefined();
    SageSlot sage_local_lay_160 = sage_slot_undefined();
    SageSlot sage_local_set_159 = sage_slot_undefined();
    SageSlot sage_local_bp_158 = sage_slot_undefined();
    SageSlot sage_local_gp_157 = sage_slot_undefined();
    SageSlot sage_local_verts_156 = sage_slot_undefined();
    SageSlot sage_local_inst_155 = sage_slot_undefined();
    SageSlot sage_local_fv_154 = sage_slot_undefined();
    SageSlot sage_local_fi_153 = sage_slot_undefined();
    SageSlot sage_local_cmd_152 = sage_slot_undefined();
    SageSlot sage_local_rp_151 = sage_slot_undefined();
    SageSlot sage_local_fb_150 = sage_slot_undefined();
    SageSlot sage_local_w_149 = sage_slot_undefined();
    SageSlot sage_local_ca_148 = sage_slot_undefined();
    SageSlot sage_local_h_147 = sage_slot_undefined();
    SageSlot sage_local_handler_146 = sage_slot_undefined();
    SageSlot sage_local_e_145 = sage_slot_undefined();
    SageSlot sage_local_mname_144 = sage_slot_undefined();
    SageSlot sage_local_k_143 = sage_slot_undefined();
    SageSlot sage_local_keys_142 = sage_slot_undefined();
    SageSlot sage_local_methods_141 = sage_slot_undefined();
    SageSlot sage_local_parent_140 = sage_slot_undefined();
    SageSlot sage_local_func_139 = sage_slot_undefined();
    SageSlot sage_local_cls_138 = sage_slot_undefined();
    SageSlot sage_local_frame_137 = sage_slot_undefined();
    SageSlot sage_local_h_method_136 = sage_slot_undefined();
    SageSlot sage_local_method_135 = sage_slot_undefined();
    SageSlot sage_local_init_func_134 = sage_slot_undefined();
    SageSlot sage_local_instance_133 = sage_slot_undefined();
    SageSlot sage_local_arg_name_132 = sage_slot_undefined();
    SageSlot sage_local_callee_131 = sage_slot_undefined();
    SageSlot sage_local_args_130 = sage_slot_undefined();
    SageSlot sage_local_argc_129 = sage_slot_undefined();
    SageSlot sage_local_func_obj_128 = sage_slot_undefined();
    SageSlot sage_local_chunk_idx_127 = sage_slot_undefined();
    SageSlot sage_local_name_idx_126 = sage_slot_undefined();
    SageSlot sage_local_start_idx_125 = sage_slot_undefined();
    SageSlot sage_local_end_idx_124 = sage_slot_undefined();
    SageSlot sage_local_obj_123 = sage_slot_undefined();
    SageSlot sage_local_key_122 = sage_slot_undefined();
    SageSlot sage_local_d_121 = sage_slot_undefined();
    SageSlot sage_local_t_120 = sage_slot_undefined();
    SageSlot sage_local_j_119 = sage_slot_undefined();
    SageSlot sage_local_arr_118 = sage_slot_undefined();
    SageSlot sage_local_count_117 = sage_slot_undefined();
    SageSlot sage_local_cond_116 = sage_slot_undefined();
    SageSlot sage_local_target_115 = sage_slot_undefined();
    SageSlot sage_local_a_114 = sage_slot_undefined();
    SageSlot sage_local_b_113 = sage_slot_undefined();
    SageSlot sage_local_updated_112 = sage_slot_undefined();
    SageSlot sage_local_val_111 = sage_slot_undefined();
    SageSlot sage_local_si_110 = sage_slot_undefined();
    SageSlot sage_local_found_109 = sage_slot_undefined();
    SageSlot sage_local_name_108 = sage_slot_undefined();
    SageSlot sage_local_idx_107 = sage_slot_undefined();
    SageSlot sage_local_op_106 = sage_slot_undefined();
    SageSlot sage_local_ut_105 = sage_slot_undefined();
    SageSlot sage_local_self_104 = sage_slot_undefined();
    SageSlot* sage_gc_roots[78] = {&sage_local_gx_181, &sage_local_gy_180, &sage_local_gz_179, &sage_local_st_178, &sage_local_data_177, &sage_local_s_176, &sage_local_f_175, &sage_local_idxc_174, &sage_local_fidx_173, &sage_local_vo_172, &sage_local_buf_171, &sage_local_sx_170, &sage_local_sy_169, &sage_local_sw_168, &sage_local_sh_167, &sage_local_vx_166, &sage_local_vy_165, &sage_local_vw_164, &sage_local_vh_163, &sage_local_mind_162, &sage_local_maxd_161, &sage_local_lay_160, &sage_local_set_159, &sage_local_bp_158, &sage_local_gp_157, &sage_local_verts_156, &sage_local_inst_155, &sage_local_fv_154, &sage_local_fi_153, &sage_local_cmd_152, &sage_local_rp_151, &sage_local_fb_150, &sage_local_w_149, &sage_local_ca_148, &sage_local_h_147, &sage_local_handler_146, &sage_local_e_145, &sage_local_mname_144, &sage_local_k_143, &sage_local_keys_142, &sage_local_methods_141, &sage_local_parent_140, &sage_local_func_139, &sage_local_cls_138, &sage_local_frame_137, &sage_local_h_method_136, &sage_local_method_135, &sage_local_init_func_134, &sage_local_instance_133, &sage_local_arg_name_132, &sage_local_callee_131, &sage_local_args_130, &sage_local_argc_129, &sage_local_func_obj_128, &sage_local_chunk_idx_127, &sage_local_name_idx_126, &sage_local_start_idx_125, &sage_local_end_idx_124, &sage_local_obj_123, &sage_local_key_122, &sage_local_d_121, &sage_local_t_120, &sage_local_j_119, &sage_local_arr_118, &sage_local_count_117, &sage_local_cond_116, &sage_local_target_115, &sage_local_a_114, &sage_local_b_113, &sage_local_updated_112, &sage_local_val_111, &sage_local_si_110, &sage_local_found_109, &sage_local_name_108, &sage_local_idx_107, &sage_local_op_106, &sage_local_ut_105, &sage_local_self_104};
    SageGcFrame sage_gc_frame;
    sage_gc_push_frame(&sage_gc_frame, sage_gc_roots, 78);
    sage_define_slot(&sage_local_self_104, _self);
    (void)_argc;
    sage_define_slot(&sage_local_ut_105, sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("utils")));
    if (sage_truthy(sage_gte(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")))))) {
        return sage_gc_return(&sage_gc_frame, sage_bool(0));
    }
    sage_define_slot(&sage_local_op_106, sage_int(sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")))));
    if (sage_truthy(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("trace")))) {
        sage_print_ln(sage_add(sage_add(sage_add(sage_add(sage_add(sage_string("IP: "), sage_str(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")))), sage_string(" OP: ")), sage_str(sage_load_slot(&sage_local_op_106, "op"))), sage_string(" Stack: ")), sage_str(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")))));
    }
    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(1)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_CONSTANT_10, "OP_CONSTANT")))) {
        sage_define_slot(&sage_local_idx_107, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
        (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("constants")), sage_load_slot(&sage_local_idx_107, "idx")));
    }
    else {
        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_NIL_11, "OP_NIL")))) {
            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_nil());
        }
        else {
            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_TRUE_12, "OP_TRUE")))) {
                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_bool(1));
            }
            else {
                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_FALSE_13, "OP_FALSE")))) {
                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_bool(0));
                }
                else {
                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_POP_14, "OP_POP")))) {
                        (void)sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")));
                    }
                    else {
                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_DUP_57, "OP_DUP")))) {
                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_sub(sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))), sage_number(1))));
                        }
                        else {
                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GET_GLOBAL_15, "OP_GET_GLOBAL")))) {
                                sage_define_slot(&sage_local_idx_107, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                sage_define_slot(&sage_local_name_108, sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("constants")), sage_load_slot(&sage_local_idx_107, "idx")));
                                sage_define_slot(&sage_local_found_109, sage_bool(0));
                                sage_define_slot(&sage_local_si_110, sage_sub(sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes"))), sage_number(1)));
                                while (sage_truthy(sage_gte(sage_load_slot(&sage_local_si_110, "si"), sage_number(0)))) {
                                    if (sage_truthy(sage_dict_has_fn(sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")), sage_load_slot(&sage_local_si_110, "si")), sage_load_slot(&sage_local_name_108, "name")))) {
                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_index(sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")), sage_load_slot(&sage_local_si_110, "si")), sage_load_slot(&sage_local_name_108, "name")));
                                        sage_define_slot(&sage_local_found_109, sage_bool(1));
                                        sage_define_slot(&sage_local_si_110, sage_sub(sage_number(0), sage_number(1)));
                                    }
                                    else {
                                        sage_define_slot(&sage_local_si_110, sage_sub(sage_load_slot(&sage_local_si_110, "si"), sage_number(1)));
                                    }
                                }
                                if (sage_truthy(sage_not(sage_load_slot(&sage_local_found_109, "found")))) {
                                    if (sage_truthy(sage_dict_has_fn(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("globals")), sage_load_slot(&sage_local_name_108, "name")))) {
                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("globals")), sage_load_slot(&sage_local_name_108, "name")));
                                    }
                                    else {
                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_nil());
                                    }
                                }
                            }
                            else {
                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_DEFINE_GLOBAL_16, "OP_DEFINE_GLOBAL")))) {
                                    sage_define_slot(&sage_local_idx_107, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                    sage_define_slot(&sage_local_name_108, sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("constants")), sage_load_slot(&sage_local_idx_107, "idx")));
                                    (void)sage_index_set(sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")), sage_sub(sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes"))), sage_number(1))), sage_load_slot(&sage_local_name_108, "name"), sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                }
                                else {
                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_SET_GLOBAL_17, "OP_SET_GLOBAL")))) {
                                        sage_define_slot(&sage_local_idx_107, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                        (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                        sage_define_slot(&sage_local_name_108, sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("constants")), sage_load_slot(&sage_local_idx_107, "idx")));
                                        sage_define_slot(&sage_local_val_111, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                        sage_define_slot(&sage_local_si_110, sage_sub(sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes"))), sage_number(1)));
                                        sage_define_slot(&sage_local_updated_112, sage_bool(0));
                                        while (sage_truthy(sage_gte(sage_load_slot(&sage_local_si_110, "si"), sage_number(0)))) {
                                            if (sage_truthy(sage_dict_has_fn(sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")), sage_load_slot(&sage_local_si_110, "si")), sage_load_slot(&sage_local_name_108, "name")))) {
                                                (void)sage_index_set(sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")), sage_load_slot(&sage_local_si_110, "si")), sage_load_slot(&sage_local_name_108, "name"), sage_load_slot(&sage_local_val_111, "val"));
                                                sage_define_slot(&sage_local_updated_112, sage_bool(1));
                                                sage_define_slot(&sage_local_si_110, sage_sub(sage_number(0), sage_number(1)));
                                            }
                                            else {
                                                sage_define_slot(&sage_local_si_110, sage_sub(sage_load_slot(&sage_local_si_110, "si"), sage_number(1)));
                                            }
                                        }
                                        if (sage_truthy(sage_not(sage_load_slot(&sage_local_updated_112, "updated")))) {
                                            (void)sage_index_set(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("globals")), sage_load_slot(&sage_local_name_108, "name"), sage_load_slot(&sage_local_val_111, "val"));
                                        }
                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_local_val_111, "val"));
                                    }
                                    else {
                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_ADD_25, "OP_ADD")))) {
                                            sage_define_slot(&sage_local_b_113, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                            sage_define_slot(&sage_local_a_114, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_add(sage_load_slot(&sage_local_a_114, "a"), sage_load_slot(&sage_local_b_113, "b")));
                                        }
                                        else {
                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_SUB_26, "OP_SUB")))) {
                                                sage_define_slot(&sage_local_b_113, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                sage_define_slot(&sage_local_a_114, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_sub(sage_load_slot(&sage_local_a_114, "a"), sage_load_slot(&sage_local_b_113, "b")));
                                            }
                                            else {
                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_MUL_27, "OP_MUL")))) {
                                                    sage_define_slot(&sage_local_b_113, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                    sage_define_slot(&sage_local_a_114, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_mul(sage_load_slot(&sage_local_a_114, "a"), sage_load_slot(&sage_local_b_113, "b")));
                                                }
                                                else {
                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_DIV_28, "OP_DIV")))) {
                                                        sage_define_slot(&sage_local_b_113, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                        sage_define_slot(&sage_local_a_114, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_div(sage_load_slot(&sage_local_a_114, "a"), sage_load_slot(&sage_local_b_113, "b")));
                                                    }
                                                    else {
                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_MOD_29, "OP_MOD")))) {
                                                            sage_define_slot(&sage_local_b_113, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                            sage_define_slot(&sage_local_a_114, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_mod(sage_load_slot(&sage_local_a_114, "a"), sage_load_slot(&sage_local_b_113, "b")));
                                                        }
                                                        else {
                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_NEGATE_30, "OP_NEGATE")))) {
                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_sub(sage_number(0), sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")))));
                                                            }
                                                            else {
                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_EQUAL_31, "OP_EQUAL")))) {
                                                                    sage_define_slot(&sage_local_b_113, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                    sage_define_slot(&sage_local_a_114, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_eq(sage_load_slot(&sage_local_a_114, "a"), sage_load_slot(&sage_local_b_113, "b")));
                                                                }
                                                                else {
                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_NOT_EQUAL_32, "OP_NOT_EQUAL")))) {
                                                                        sage_define_slot(&sage_local_b_113, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                        sage_define_slot(&sage_local_a_114, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_neq(sage_load_slot(&sage_local_a_114, "a"), sage_load_slot(&sage_local_b_113, "b")));
                                                                    }
                                                                    else {
                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GREATER_33, "OP_GREATER")))) {
                                                                            sage_define_slot(&sage_local_b_113, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                            sage_define_slot(&sage_local_a_114, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_gt(sage_load_slot(&sage_local_a_114, "a"), sage_load_slot(&sage_local_b_113, "b")));
                                                                        }
                                                                        else {
                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GREATER_EQUAL_34, "OP_GREATER_EQUAL")))) {
                                                                                sage_define_slot(&sage_local_b_113, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                sage_define_slot(&sage_local_a_114, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_gte(sage_load_slot(&sage_local_a_114, "a"), sage_load_slot(&sage_local_b_113, "b")));
                                                                            }
                                                                            else {
                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_LESS_35, "OP_LESS")))) {
                                                                                    sage_define_slot(&sage_local_b_113, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                    sage_define_slot(&sage_local_a_114, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_lt(sage_load_slot(&sage_local_a_114, "a"), sage_load_slot(&sage_local_b_113, "b")));
                                                                                }
                                                                                else {
                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_LESS_EQUAL_36, "OP_LESS_EQUAL")))) {
                                                                                        sage_define_slot(&sage_local_b_113, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                        sage_define_slot(&sage_local_a_114, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_lte(sage_load_slot(&sage_local_a_114, "a"), sage_load_slot(&sage_local_b_113, "b")));
                                                                                    }
                                                                                    else {
                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_BIT_AND_37, "OP_BIT_AND")))) {
                                                                                            sage_define_slot(&sage_local_b_113, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                            sage_define_slot(&sage_local_a_114, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_bit_and(sage_load_slot(&sage_local_a_114, "a"), sage_load_slot(&sage_local_b_113, "b")));
                                                                                        }
                                                                                        else {
                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_BIT_OR_38, "OP_BIT_OR")))) {
                                                                                                sage_define_slot(&sage_local_b_113, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                sage_define_slot(&sage_local_a_114, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_bit_or(sage_load_slot(&sage_local_a_114, "a"), sage_load_slot(&sage_local_b_113, "b")));
                                                                                            }
                                                                                            else {
                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_BIT_XOR_39, "OP_BIT_XOR")))) {
                                                                                                    sage_define_slot(&sage_local_b_113, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                    sage_define_slot(&sage_local_a_114, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_bit_xor(sage_load_slot(&sage_local_a_114, "a"), sage_load_slot(&sage_local_b_113, "b")));
                                                                                                }
                                                                                                else {
                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_BIT_NOT_40, "OP_BIT_NOT")))) {
                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_bit_not(sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")))));
                                                                                                    }
                                                                                                    else {
                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_SHIFT_LEFT_41, "OP_SHIFT_LEFT")))) {
                                                                                                            sage_define_slot(&sage_local_b_113, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                            sage_define_slot(&sage_local_a_114, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_lshift(sage_load_slot(&sage_local_a_114, "a"), sage_load_slot(&sage_local_b_113, "b")));
                                                                                                        }
                                                                                                        else {
                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_SHIFT_RIGHT_42, "OP_SHIFT_RIGHT")))) {
                                                                                                                sage_define_slot(&sage_local_b_113, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                sage_define_slot(&sage_local_a_114, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_rshift(sage_load_slot(&sage_local_a_114, "a"), sage_load_slot(&sage_local_b_113, "b")));
                                                                                                            }
                                                                                                            else {
                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_NOT_43, "OP_NOT")))) {
                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_not(sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")))));
                                                                                                                }
                                                                                                                else {
                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_TRUTHY_44, "OP_TRUTHY")))) {
                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_not(sage_not(sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))))));
                                                                                                                    }
                                                                                                                    else {
                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_JUMP_45, "OP_JUMP")))) {
                                                                                                                            (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                        }
                                                                                                                        else {
                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_JUMP_IF_FALSE_46, "OP_JUMP_IF_FALSE")))) {
                                                                                                                                sage_define_slot(&sage_local_target_115, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                                                                                                                (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                sage_define_slot(&sage_local_cond_116, sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_sub(sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))), sage_number(1))));
                                                                                                                                if (sage_truthy(sage_not(sage_load_slot(&sage_local_cond_116, "cond")))) {
                                                                                                                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_load_slot(&sage_local_target_115, "target"); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                }
                                                                                                                            }
                                                                                                                            else {
                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_LOOP_BACK_61, "OP_LOOP_BACK")))) {
                                                                                                                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_sub(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))})); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                }
                                                                                                                                else {
                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_PRINT_52, "OP_PRINT")))) {
                                                                                                                                        sage_print_ln(sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                    }
                                                                                                                                    else {
                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_ARRAY_49, "OP_ARRAY")))) {
                                                                                                                                            sage_define_slot(&sage_local_count_117, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                                                                                                                            (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                            sage_define_slot(&sage_local_arr_118, sage_make_array(0, NULL));
                                                                                                                                            sage_define_slot(&sage_local_j_119, sage_number(0));
                                                                                                                                            while (sage_truthy(sage_lt(sage_load_slot(&sage_local_j_119, "j"), sage_load_slot(&sage_local_count_117, "count")))) {
                                                                                                                                                (void)sage_push(sage_load_slot(&sage_local_arr_118, "arr"), sage_nil());
                                                                                                                                                sage_define_slot(&sage_local_j_119, sage_add(sage_load_slot(&sage_local_j_119, "j"), sage_number(1)));
                                                                                                                                            }
                                                                                                                                            (void)sage_assign_slot(&sage_local_j_119, "j", sage_number(0));
                                                                                                                                            while (sage_truthy(sage_lt(sage_load_slot(&sage_local_j_119, "j"), sage_load_slot(&sage_local_count_117, "count")))) {
                                                                                                                                                (void)sage_index_set(sage_load_slot(&sage_local_arr_118, "arr"), sage_sub(sage_sub(sage_load_slot(&sage_local_count_117, "count"), sage_number(1)), sage_load_slot(&sage_local_j_119, "j")), sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                sage_define_slot(&sage_local_j_119, sage_add(sage_load_slot(&sage_local_j_119, "j"), sage_number(1)));
                                                                                                                                            }
                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_local_arr_118, "arr"));
                                                                                                                                        }
                                                                                                                                        else {
                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_TUPLE_50, "OP_TUPLE")))) {
                                                                                                                                                sage_define_slot(&sage_local_count_117, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                                                                                                                                (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                sage_define_slot(&sage_local_t_120, sage_make_array(0, NULL));
                                                                                                                                                sage_define_slot(&sage_local_j_119, sage_number(0));
                                                                                                                                                while (sage_truthy(sage_lt(sage_load_slot(&sage_local_j_119, "j"), sage_load_slot(&sage_local_count_117, "count")))) {
                                                                                                                                                    (void)sage_push(sage_load_slot(&sage_local_t_120, "t"), sage_nil());
                                                                                                                                                    sage_define_slot(&sage_local_j_119, sage_add(sage_load_slot(&sage_local_j_119, "j"), sage_number(1)));
                                                                                                                                                }
                                                                                                                                                (void)sage_assign_slot(&sage_local_j_119, "j", sage_number(0));
                                                                                                                                                while (sage_truthy(sage_lt(sage_load_slot(&sage_local_j_119, "j"), sage_load_slot(&sage_local_count_117, "count")))) {
                                                                                                                                                    (void)sage_index_set(sage_load_slot(&sage_local_t_120, "t"), sage_sub(sage_sub(sage_load_slot(&sage_local_count_117, "count"), sage_number(1)), sage_load_slot(&sage_local_j_119, "j")), sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                    sage_define_slot(&sage_local_j_119, sage_add(sage_load_slot(&sage_local_j_119, "j"), sage_number(1)));
                                                                                                                                                }
                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_local_t_120, "t"));
                                                                                                                                            }
                                                                                                                                            else {
                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_DICT_51, "OP_DICT")))) {
                                                                                                                                                    sage_define_slot(&sage_local_count_117, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                                                                                                                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                    sage_define_slot(&sage_local_d_121, sage_make_dict());
                                                                                                                                                    sage_define_slot(&sage_local_j_119, sage_number(0));
                                                                                                                                                    while (sage_truthy(sage_lt(sage_load_slot(&sage_local_j_119, "j"), sage_load_slot(&sage_local_count_117, "count")))) {
                                                                                                                                                        sage_define_slot(&sage_local_val_111, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                        sage_define_slot(&sage_local_key_122, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                        (void)sage_index_set(sage_load_slot(&sage_local_d_121, "d"), sage_load_slot(&sage_local_key_122, "key"), sage_load_slot(&sage_local_val_111, "val"));
                                                                                                                                                        sage_define_slot(&sage_local_j_119, sage_add(sage_load_slot(&sage_local_j_119, "j"), sage_number(1)));
                                                                                                                                                    }
                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_local_d_121, "d"));
                                                                                                                                                }
                                                                                                                                                else {
                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GET_INDEX_21, "OP_GET_INDEX")))) {
                                                                                                                                                        sage_define_slot(&sage_local_idx_107, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                        sage_define_slot(&sage_local_obj_123, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_index(sage_load_slot(&sage_local_obj_123, "obj"), sage_load_slot(&sage_local_idx_107, "idx")));
                                                                                                                                                    }
                                                                                                                                                    else {
                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_SET_INDEX_22, "OP_SET_INDEX")))) {
                                                                                                                                                            sage_define_slot(&sage_local_val_111, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                            sage_define_slot(&sage_local_idx_107, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                            sage_define_slot(&sage_local_obj_123, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                            (void)sage_index_set(sage_load_slot(&sage_local_obj_123, "obj"), sage_load_slot(&sage_local_idx_107, "idx"), sage_load_slot(&sage_local_val_111, "val"));
                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_local_val_111, "val"));
                                                                                                                                                        }
                                                                                                                                                        else {
                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_SLICE_24, "OP_SLICE")))) {
                                                                                                                                                                sage_define_slot(&sage_local_end_idx_124, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                sage_define_slot(&sage_local_start_idx_125, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                sage_define_slot(&sage_local_obj_123, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_slice(sage_load_slot(&sage_local_obj_123, "obj"), sage_load_slot(&sage_local_start_idx_125, "start_idx"), sage_load_slot(&sage_local_end_idx_124, "end_idx")));
                                                                                                                                                            }
                                                                                                                                                            else {
                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_ARRAY_LEN_58, "OP_ARRAY_LEN")))) {
                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_len(sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")))));
                                                                                                                                                                }
                                                                                                                                                                else {
                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_PUSH_ENV_55, "OP_PUSH_ENV")))) {
                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")), sage_make_dict());
                                                                                                                                                                    }
                                                                                                                                                                    else {
                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_POP_ENV_56, "OP_POP_ENV")))) {
                                                                                                                                                                            (void)sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")));
                                                                                                                                                                        }
                                                                                                                                                                        else {
                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_DEFINE_FUNCTION_18, "OP_DEFINE_FUNCTION")))) {
                                                                                                                                                                                sage_define_slot(&sage_local_name_idx_126, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                                                                                                                                                                sage_define_slot(&sage_local_chunk_idx_127, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2))}));
                                                                                                                                                                                (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(4)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                                                sage_define_slot(&sage_local_name_108, sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("constants")), sage_load_slot(&sage_local_name_idx_126, "name_idx")));
                                                                                                                                                                                sage_define_slot(&sage_local_func_obj_128, sage_make_dict_from_entries(3, (const char*[]){"__type__", "__chunk__", "__name__"}, (SageValue[]){sage_string("function"), sage_load_slot(&sage_local_chunk_idx_127, "chunk_idx"), sage_load_slot(&sage_local_name_108, "name")}));
                                                                                                                                                                                (void)sage_index_set(sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")), sage_sub(sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes"))), sage_number(1))), sage_load_slot(&sage_local_name_108, "name"), sage_load_slot(&sage_local_func_obj_128, "func_obj"));
                                                                                                                                                                            }
                                                                                                                                                                            else {
                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_LOAD_FUNCTION_23, "OP_LOAD_FUNCTION")))) {
                                                                                                                                                                                    sage_define_slot(&sage_local_chunk_idx_127, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                                                                                                                                                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_make_dict_from_entries(2, (const char*[]){"__type__", "__chunk__"}, (SageValue[]){sage_string("function"), sage_load_slot(&sage_local_chunk_idx_127, "chunk_idx")}));
                                                                                                                                                                                }
                                                                                                                                                                                else {
                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_CALL_47, "OP_CALL")))) {
                                                                                                                                                                                        sage_define_slot(&sage_local_argc_129, sage_int(sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")))));
                                                                                                                                                                                        (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(1)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                                                        sage_define_slot(&sage_local_args_130, sage_make_array(0, NULL));
                                                                                                                                                                                        sage_define_slot(&sage_local_j_119, sage_number(0));
                                                                                                                                                                                        while (sage_truthy(sage_lt(sage_load_slot(&sage_local_j_119, "j"), sage_load_slot(&sage_local_argc_129, "argc")))) {
                                                                                                                                                                                            (void)sage_push(sage_load_slot(&sage_local_args_130, "args"), sage_nil());
                                                                                                                                                                                            sage_define_slot(&sage_local_j_119, sage_add(sage_load_slot(&sage_local_j_119, "j"), sage_number(1)));
                                                                                                                                                                                        }
                                                                                                                                                                                        (void)sage_assign_slot(&sage_local_j_119, "j", sage_number(0));
                                                                                                                                                                                        while (sage_truthy(sage_lt(sage_load_slot(&sage_local_j_119, "j"), sage_load_slot(&sage_local_argc_129, "argc")))) {
                                                                                                                                                                                            (void)sage_index_set(sage_load_slot(&sage_local_args_130, "args"), sage_sub(sage_sub(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(1)), sage_load_slot(&sage_local_j_119, "j")), sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                            sage_define_slot(&sage_local_j_119, sage_add(sage_load_slot(&sage_local_j_119, "j"), sage_number(1)));
                                                                                                                                                                                        }
                                                                                                                                                                                        sage_define_slot(&sage_local_callee_131, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_type(sage_load_slot(&sage_local_callee_131, "callee")), sage_string("dict")))) {
                                                                                                                                                                                            if (sage_truthy(sage_dict_has_fn(sage_load_slot(&sage_local_callee_131, "callee"), sage_string("__type__")))) {
                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_index(sage_load_slot(&sage_local_callee_131, "callee"), sage_string("__type__")), sage_string("function")))) {
                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("call_stack")), sage_make_dict_from_entries(2, (const char*[]){"ip", "code"}, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code"))}));
                                                                                                                                                                                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("chunks")), sage_index(sage_load_slot(&sage_local_callee_131, "callee"), sage_string("__chunk__"))); sage_dict_set(_obj.as.dict, "code", _val); _val;});
                                                                                                                                                                                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_number(0); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")), sage_make_dict());
                                                                                                                                                                                                    (void)sage_assign_slot(&sage_local_j_119, "j", sage_number(0));
                                                                                                                                                                                                    while (sage_truthy(sage_lt(sage_load_slot(&sage_local_j_119, "j"), sage_load_slot(&sage_local_argc_129, "argc")))) {
                                                                                                                                                                                                        sage_define_slot(&sage_local_arg_name_132, sage_add(sage_string("__arg"), sage_str(sage_load_slot(&sage_local_j_119, "j"))));
                                                                                                                                                                                                        (void)sage_index_set(sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")), sage_sub(sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes"))), sage_number(1))), sage_load_slot(&sage_local_arg_name_132, "arg_name"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_load_slot(&sage_local_j_119, "j")));
                                                                                                                                                                                                        sage_define_slot(&sage_local_j_119, sage_add(sage_load_slot(&sage_local_j_119, "j"), sage_number(1)));
                                                                                                                                                                                                    }
                                                                                                                                                                                                }
                                                                                                                                                                                                else {
                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_index(sage_load_slot(&sage_local_callee_131, "callee"), sage_string("__type__")), sage_string("class")))) {
                                                                                                                                                                                                        sage_define_slot(&sage_local_instance_133, sage_make_dict_from_entries(2, (const char*[]){"__type__", "__class__"}, (SageValue[]){sage_string("instance"), sage_load_slot(&sage_local_callee_131, "callee")}));
                                                                                                                                                                                                        if (sage_truthy(sage_dict_has_fn(sage_index(sage_load_slot(&sage_local_callee_131, "callee"), sage_string("__methods__")), sage_string("init")))) {
                                                                                                                                                                                                            sage_define_slot(&sage_local_init_func_134, sage_index(sage_index(sage_load_slot(&sage_local_callee_131, "callee"), sage_string("__methods__")), sage_string("init")));
                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("call_stack")), sage_make_dict_from_entries(4, (const char*[]){"ip", "code", "__is_constructor__", "__instance__"}, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_bool(1), sage_load_slot(&sage_local_instance_133, "instance")}));
                                                                                                                                                                                                            (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("chunks")), sage_index(sage_load_slot(&sage_local_init_func_134, "init_func"), sage_string("__chunk__"))); sage_dict_set(_obj.as.dict, "code", _val); _val;});
                                                                                                                                                                                                            (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_number(0); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")), sage_make_dict());
                                                                                                                                                                                                            (void)sage_index_set(sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")), sage_sub(sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes"))), sage_number(1))), sage_string("__arg0"), sage_load_slot(&sage_local_instance_133, "instance"));
                                                                                                                                                                                                            (void)sage_assign_slot(&sage_local_j_119, "j", sage_number(0));
                                                                                                                                                                                                            while (sage_truthy(sage_lt(sage_load_slot(&sage_local_j_119, "j"), sage_load_slot(&sage_local_argc_129, "argc")))) {
                                                                                                                                                                                                                sage_define_slot(&sage_local_arg_name_132, sage_add(sage_string("__arg"), sage_str(sage_add(sage_load_slot(&sage_local_j_119, "j"), sage_number(1)))));
                                                                                                                                                                                                                (void)sage_index_set(sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")), sage_sub(sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes"))), sage_number(1))), sage_load_slot(&sage_local_arg_name_132, "arg_name"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_load_slot(&sage_local_j_119, "j")));
                                                                                                                                                                                                                sage_define_slot(&sage_local_j_119, sage_add(sage_load_slot(&sage_local_j_119, "j"), sage_number(1)));
                                                                                                                                                                                                            }
                                                                                                                                                                                                        }
                                                                                                                                                                                                        else {
                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_local_instance_133, "instance"));
                                                                                                                                                                                                        }
                                                                                                                                                                                                    }
                                                                                                                                                                                                    else {
                                                                                                                                                                                                        sage_print_ln(sage_string("Error: Callee dict is not a function or class"));
                                                                                                                                                                                                    }
                                                                                                                                                                                                }
                                                                                                                                                                                            }
                                                                                                                                                                                            else {
                                                                                                                                                                                                sage_print_ln(sage_string("Error: Callee dict has no __type__"));
                                                                                                                                                                                            }
                                                                                                                                                                                        }
                                                                                                                                                                                        else {
                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_type(sage_load_slot(&sage_local_callee_131, "callee")), sage_string("string")))) {
                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_callee_131, "callee"), sage_string("__builtin_clock")))) {
                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_clock_fn());
                                                                                                                                                                                                }
                                                                                                                                                                                                else {
                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_callee_131, "callee"), sage_string("__builtin_str")))) {
                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_str(sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0))));
                                                                                                                                                                                                    }
                                                                                                                                                                                                    else {
                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_callee_131, "callee"), sage_string("__builtin_int")))) {
                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_int(sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0))));
                                                                                                                                                                                                        }
                                                                                                                                                                                                        else {
                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_callee_131, "callee"), sage_string("__builtin_tonumber")))) {
                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_tonumber(sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0))));
                                                                                                                                                                                                            }
                                                                                                                                                                                                            else {
                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_callee_131, "callee"), sage_string("__builtin_len")))) {
                                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_len(sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0))));
                                                                                                                                                                                                                }
                                                                                                                                                                                                                else {
                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_callee_131, "callee"), sage_string("__builtin_print")))) {
                                                                                                                                                                                                                        sage_print_ln(sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)));
                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_nil());
                                                                                                                                                                                                                    }
                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_callee_131, "callee"), sage_string("__builtin_range")))) {
                                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_range1(sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0))));
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                            sage_print_ln(sage_add(sage_string("Error: Unknown builtin: "), sage_load_slot(&sage_local_callee_131, "callee")));
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                    }
                                                                                                                                                                                                                }
                                                                                                                                                                                                            }
                                                                                                                                                                                                        }
                                                                                                                                                                                                    }
                                                                                                                                                                                                }
                                                                                                                                                                                            }
                                                                                                                                                                                            else {
                                                                                                                                                                                                if (sage_truthy(sage_or(sage_eq(sage_type(sage_load_slot(&sage_local_callee_131, "callee")), sage_string("function")), sage_eq(sage_type(sage_load_slot(&sage_local_callee_131, "callee")), sage_string("native fn"))))) {
                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(0)))) {
                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 1, (SageValue[]){sage_load_slot(&sage_local_callee_131, "callee")}));
                                                                                                                                                                                                    }
                                                                                                                                                                                                    else {
                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(1)))) {
                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 2, (SageValue[]){sage_load_slot(&sage_local_callee_131, "callee"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0))}));
                                                                                                                                                                                                        }
                                                                                                                                                                                                        else {
                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(2)))) {
                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 3, (SageValue[]){sage_load_slot(&sage_local_callee_131, "callee"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1))}));
                                                                                                                                                                                                            }
                                                                                                                                                                                                            else {
                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(3)))) {
                                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 4, (SageValue[]){sage_load_slot(&sage_local_callee_131, "callee"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2))}));
                                                                                                                                                                                                                }
                                                                                                                                                                                                                else {
                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(4)))) {
                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 5, (SageValue[]){sage_load_slot(&sage_local_callee_131, "callee"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3))}));
                                                                                                                                                                                                                    }
                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(5)))) {
                                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 6, (SageValue[]){sage_load_slot(&sage_local_callee_131, "callee"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(4))}));
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(6)))) {
                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 7, (SageValue[]){sage_load_slot(&sage_local_callee_131, "callee"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(4)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(5))}));
                                                                                                                                                                                                                            }
                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(7)))) {
                                                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 8, (SageValue[]){sage_load_slot(&sage_local_callee_131, "callee"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(4)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(5)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(6))}));
                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                else {
                                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(8)))) {
                                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 9, (SageValue[]){sage_load_slot(&sage_local_callee_131, "callee"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(4)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(5)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(6)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(7))}));
                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                                        sage_print_ln(sage_string("Error: Host call with >8 args not implemented"));
                                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_nil());
                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                }
                                                                                                                                                                                                                            }
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                    }
                                                                                                                                                                                                                }
                                                                                                                                                                                                            }
                                                                                                                                                                                                        }
                                                                                                                                                                                                    }
                                                                                                                                                                                                }
                                                                                                                                                                                                else {
                                                                                                                                                                                                    sage_print_ln(sage_string("Error: Callee not a function or builtin name"));
                                                                                                                                                                                                }
                                                                                                                                                                                            }
                                                                                                                                                                                        }
                                                                                                                                                                                    }
                                                                                                                                                                                    else {
                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_CALL_METHOD_48, "OP_CALL_METHOD")))) {
                                                                                                                                                                                            sage_define_slot(&sage_local_name_idx_126, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                                                                                                                                                                            sage_define_slot(&sage_local_argc_129, sage_int(sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)))));
                                                                                                                                                                                            (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(3)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                                                            sage_define_slot(&sage_local_name_108, sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("constants")), sage_load_slot(&sage_local_name_idx_126, "name_idx")));
                                                                                                                                                                                            sage_define_slot(&sage_local_args_130, sage_make_array(0, NULL));
                                                                                                                                                                                            sage_define_slot(&sage_local_j_119, sage_number(0));
                                                                                                                                                                                            while (sage_truthy(sage_lt(sage_load_slot(&sage_local_j_119, "j"), sage_load_slot(&sage_local_argc_129, "argc")))) {
                                                                                                                                                                                                (void)sage_push(sage_load_slot(&sage_local_args_130, "args"), sage_nil());
                                                                                                                                                                                                sage_define_slot(&sage_local_j_119, sage_add(sage_load_slot(&sage_local_j_119, "j"), sage_number(1)));
                                                                                                                                                                                            }
                                                                                                                                                                                            (void)sage_assign_slot(&sage_local_j_119, "j", sage_number(0));
                                                                                                                                                                                            while (sage_truthy(sage_lt(sage_load_slot(&sage_local_j_119, "j"), sage_load_slot(&sage_local_argc_129, "argc")))) {
                                                                                                                                                                                                (void)sage_index_set(sage_load_slot(&sage_local_args_130, "args"), sage_sub(sage_sub(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(1)), sage_load_slot(&sage_local_j_119, "j")), sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                sage_define_slot(&sage_local_j_119, sage_add(sage_load_slot(&sage_local_j_119, "j"), sage_number(1)));
                                                                                                                                                                                            }
                                                                                                                                                                                            sage_define_slot(&sage_local_obj_123, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_type(sage_load_slot(&sage_local_obj_123, "obj")), sage_string("dict")))) {
                                                                                                                                                                                                sage_define_slot(&sage_local_method_135, sage_nil());
                                                                                                                                                                                                if (sage_truthy(sage_and(sage_dict_has_fn(sage_load_slot(&sage_local_obj_123, "obj"), sage_string("__methods__")), sage_dict_has_fn(sage_index(sage_load_slot(&sage_local_obj_123, "obj"), sage_string("__methods__")), sage_load_slot(&sage_local_name_108, "name"))))) {
                                                                                                                                                                                                    (void)sage_assign_slot(&sage_local_method_135, "method", sage_index(sage_index(sage_load_slot(&sage_local_obj_123, "obj"), sage_string("__methods__")), sage_load_slot(&sage_local_name_108, "name")));
                                                                                                                                                                                                }
                                                                                                                                                                                                else {
                                                                                                                                                                                                    if (sage_truthy(sage_and(sage_dict_has_fn(sage_load_slot(&sage_local_obj_123, "obj"), sage_string("__class__")), sage_dict_has_fn(sage_index(sage_index(sage_load_slot(&sage_local_obj_123, "obj"), sage_string("__class__")), sage_string("__methods__")), sage_load_slot(&sage_local_name_108, "name"))))) {
                                                                                                                                                                                                        (void)sage_assign_slot(&sage_local_method_135, "method", sage_index(sage_index(sage_index(sage_load_slot(&sage_local_obj_123, "obj"), sage_string("__class__")), sage_string("__methods__")), sage_load_slot(&sage_local_name_108, "name")));
                                                                                                                                                                                                    }
                                                                                                                                                                                                }
                                                                                                                                                                                                if (sage_truthy(sage_neq(sage_load_slot(&sage_local_method_135, "method"), sage_nil()))) {
                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("call_stack")), sage_make_dict_from_entries(2, (const char*[]){"ip", "code"}, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code"))}));
                                                                                                                                                                                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("chunks")), sage_index(sage_load_slot(&sage_local_method_135, "method"), sage_string("__chunk__"))); sage_dict_set(_obj.as.dict, "code", _val); _val;});
                                                                                                                                                                                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_number(0); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")), sage_make_dict());
                                                                                                                                                                                                    (void)sage_index_set(sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")), sage_sub(sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes"))), sage_number(1))), sage_string("__arg0"), sage_load_slot(&sage_local_obj_123, "obj"));
                                                                                                                                                                                                    (void)sage_assign_slot(&sage_local_j_119, "j", sage_number(0));
                                                                                                                                                                                                    while (sage_truthy(sage_lt(sage_load_slot(&sage_local_j_119, "j"), sage_load_slot(&sage_local_argc_129, "argc")))) {
                                                                                                                                                                                                        sage_define_slot(&sage_local_arg_name_132, sage_add(sage_string("__arg"), sage_str(sage_add(sage_load_slot(&sage_local_j_119, "j"), sage_number(1)))));
                                                                                                                                                                                                        (void)sage_index_set(sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")), sage_sub(sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes"))), sage_number(1))), sage_load_slot(&sage_local_arg_name_132, "arg_name"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_load_slot(&sage_local_j_119, "j")));
                                                                                                                                                                                                        sage_define_slot(&sage_local_j_119, sage_add(sage_load_slot(&sage_local_j_119, "j"), sage_number(1)));
                                                                                                                                                                                                    }
                                                                                                                                                                                                }
                                                                                                                                                                                                else {
                                                                                                                                                                                                    if (sage_truthy(sage_dict_has_fn(sage_load_slot(&sage_local_obj_123, "obj"), sage_load_slot(&sage_local_name_108, "name")))) {
                                                                                                                                                                                                        sage_define_slot(&sage_local_h_method_136, sage_index(sage_load_slot(&sage_local_obj_123, "obj"), sage_load_slot(&sage_local_name_108, "name")));
                                                                                                                                                                                                        if (sage_truthy(sage_or(sage_eq(sage_type(sage_load_slot(&sage_local_h_method_136, "h_method")), sage_string("function")), sage_eq(sage_type(sage_load_slot(&sage_local_h_method_136, "h_method")), sage_string("native fn"))))) {
                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(0)))) {
                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 1, (SageValue[]){sage_load_slot(&sage_local_h_method_136, "h_method")}));
                                                                                                                                                                                                            }
                                                                                                                                                                                                            else {
                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(1)))) {
                                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 2, (SageValue[]){sage_load_slot(&sage_local_h_method_136, "h_method"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0))}));
                                                                                                                                                                                                                }
                                                                                                                                                                                                                else {
                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(2)))) {
                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 3, (SageValue[]){sage_load_slot(&sage_local_h_method_136, "h_method"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1))}));
                                                                                                                                                                                                                    }
                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(3)))) {
                                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 4, (SageValue[]){sage_load_slot(&sage_local_h_method_136, "h_method"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2))}));
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(4)))) {
                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 5, (SageValue[]){sage_load_slot(&sage_local_h_method_136, "h_method"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3))}));
                                                                                                                                                                                                                            }
                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(5)))) {
                                                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 6, (SageValue[]){sage_load_slot(&sage_local_h_method_136, "h_method"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(4))}));
                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                else {
                                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(6)))) {
                                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 7, (SageValue[]){sage_load_slot(&sage_local_h_method_136, "h_method"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(4)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(5))}));
                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(7)))) {
                                                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 8, (SageValue[]){sage_load_slot(&sage_local_h_method_136, "h_method"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(4)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(5)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(6))}));
                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(8)))) {
                                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 9, (SageValue[]){sage_load_slot(&sage_local_h_method_136, "h_method"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(4)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(5)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(6)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(7))}));
                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                                sage_print_ln(sage_string("Error: Host method call with >8 args not implemented"));
                                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_nil());
                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                }
                                                                                                                                                                                                                            }
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                    }
                                                                                                                                                                                                                }
                                                                                                                                                                                                            }
                                                                                                                                                                                                        }
                                                                                                                                                                                                        else {
                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_local_h_method_136, "h_method"));
                                                                                                                                                                                                        }
                                                                                                                                                                                                    }
                                                                                                                                                                                                    else {
                                                                                                                                                                                                        sage_print_ln(sage_add(sage_add(sage_string("Error: Method "), sage_load_slot(&sage_local_name_108, "name")), sage_string(" not found")));
                                                                                                                                                                                                    }
                                                                                                                                                                                                }
                                                                                                                                                                                            }
                                                                                                                                                                                            else {
                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_type(sage_load_slot(&sage_local_obj_123, "obj")), sage_string("module")))) {
                                                                                                                                                                                                    if (sage_truthy(sage_dict_has_fn(sage_load_slot(&sage_local_obj_123, "obj"), sage_load_slot(&sage_local_name_108, "name")))) {
                                                                                                                                                                                                        sage_define_slot(&sage_local_val_111, sage_index(sage_load_slot(&sage_local_obj_123, "obj"), sage_load_slot(&sage_local_name_108, "name")));
                                                                                                                                                                                                        if (sage_truthy(sage_or(sage_eq(sage_type(sage_load_slot(&sage_local_val_111, "val")), sage_string("function")), sage_eq(sage_type(sage_load_slot(&sage_local_val_111, "val")), sage_string("native fn"))))) {
                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(0)))) {
                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 1, (SageValue[]){sage_load_slot(&sage_local_val_111, "val")}));
                                                                                                                                                                                                            }
                                                                                                                                                                                                            else {
                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(1)))) {
                                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 2, (SageValue[]){sage_load_slot(&sage_local_val_111, "val"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0))}));
                                                                                                                                                                                                                }
                                                                                                                                                                                                                else {
                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(2)))) {
                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 3, (SageValue[]){sage_load_slot(&sage_local_val_111, "val"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1))}));
                                                                                                                                                                                                                    }
                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(3)))) {
                                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 4, (SageValue[]){sage_load_slot(&sage_local_val_111, "val"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2))}));
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(4)))) {
                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 5, (SageValue[]){sage_load_slot(&sage_local_val_111, "val"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3))}));
                                                                                                                                                                                                                            }
                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(5)))) {
                                                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 6, (SageValue[]){sage_load_slot(&sage_local_val_111, "val"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(4))}));
                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                else {
                                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(6)))) {
                                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 7, (SageValue[]){sage_load_slot(&sage_local_val_111, "val"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(4)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(5))}));
                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(7)))) {
                                                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 8, (SageValue[]){sage_load_slot(&sage_local_val_111, "val"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(4)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(5)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(6))}));
                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(8)))) {
                                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 9, (SageValue[]){sage_load_slot(&sage_local_val_111, "val"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(4)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(5)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(6)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(7))}));
                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                                sage_print_ln(sage_string("Error: Host module call with >8 args not implemented"));
                                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_nil());
                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                }
                                                                                                                                                                                                                            }
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                    }
                                                                                                                                                                                                                }
                                                                                                                                                                                                            }
                                                                                                                                                                                                        }
                                                                                                                                                                                                        else {
                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_local_val_111, "val"));
                                                                                                                                                                                                        }
                                                                                                                                                                                                    }
                                                                                                                                                                                                    else {
                                                                                                                                                                                                        sage_print_ln(sage_add(sage_add(sage_string("Error: Module attribute "), sage_load_slot(&sage_local_name_108, "name")), sage_string(" not found")));
                                                                                                                                                                                                    }
                                                                                                                                                                                                }
                                                                                                                                                                                                else {
                                                                                                                                                                                                    if (sage_truthy(sage_dict_has_fn(sage_load_slot(&sage_local_obj_123, "obj"), sage_load_slot(&sage_local_name_108, "name")))) {
                                                                                                                                                                                                        sage_define_slot(&sage_local_val_111, sage_index(sage_load_slot(&sage_local_obj_123, "obj"), sage_load_slot(&sage_local_name_108, "name")));
                                                                                                                                                                                                        if (sage_truthy(sage_or(sage_eq(sage_type(sage_load_slot(&sage_local_val_111, "val")), sage_string("function")), sage_eq(sage_type(sage_load_slot(&sage_local_val_111, "val")), sage_string("native fn"))))) {
                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(0)))) {
                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 1, (SageValue[]){sage_load_slot(&sage_local_val_111, "val")}));
                                                                                                                                                                                                            }
                                                                                                                                                                                                            else {
                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(1)))) {
                                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 2, (SageValue[]){sage_load_slot(&sage_local_val_111, "val"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0))}));
                                                                                                                                                                                                                }
                                                                                                                                                                                                                else {
                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(2)))) {
                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 3, (SageValue[]){sage_load_slot(&sage_local_val_111, "val"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1))}));
                                                                                                                                                                                                                    }
                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(3)))) {
                                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 4, (SageValue[]){sage_load_slot(&sage_local_val_111, "val"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2))}));
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(4)))) {
                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 5, (SageValue[]){sage_load_slot(&sage_local_val_111, "val"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3))}));
                                                                                                                                                                                                                            }
                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(5)))) {
                                                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 6, (SageValue[]){sage_load_slot(&sage_local_val_111, "val"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(4))}));
                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                else {
                                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(6)))) {
                                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 7, (SageValue[]){sage_load_slot(&sage_local_val_111, "val"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(4)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(5))}));
                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(7)))) {
                                                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 8, (SageValue[]){sage_load_slot(&sage_local_val_111, "val"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(4)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(5)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(6))}));
                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_argc_129, "argc"), sage_number(8)))) {
                                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_sys_1, "sys"), "call", 9, (SageValue[]){sage_load_slot(&sage_local_val_111, "val"), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(0)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(1)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(2)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(3)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(4)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(5)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(6)), sage_index(sage_load_slot(&sage_local_args_130, "args"), sage_number(7))}));
                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                                sage_print_ln(sage_string("Error: Host primitive method call with >8 args not implemented"));
                                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_nil());
                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                }
                                                                                                                                                                                                                            }
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                    }
                                                                                                                                                                                                                }
                                                                                                                                                                                                            }
                                                                                                                                                                                                        }
                                                                                                                                                                                                        else {
                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_local_val_111, "val"));
                                                                                                                                                                                                        }
                                                                                                                                                                                                    }
                                                                                                                                                                                                }
                                                                                                                                                                                            }
                                                                                                                                                                                        }
                                                                                                                                                                                        else {
                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_RETURN_54, "OP_RETURN")))) {
                                                                                                                                                                                                sage_define_slot(&sage_local_val_111, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                if (sage_truthy(sage_gt(sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("call_stack"))), sage_number(0)))) {
                                                                                                                                                                                                    (void)sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")));
                                                                                                                                                                                                    sage_define_slot(&sage_local_frame_137, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("call_stack"))));
                                                                                                                                                                                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_index(sage_load_slot(&sage_local_frame_137, "frame"), sage_string("ip")); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                                                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_index(sage_load_slot(&sage_local_frame_137, "frame"), sage_string("code")); sage_dict_set(_obj.as.dict, "code", _val); _val;});
                                                                                                                                                                                                    if (sage_truthy(sage_dict_has_fn(sage_load_slot(&sage_local_frame_137, "frame"), sage_string("__is_constructor__")))) {
                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_index(sage_load_slot(&sage_local_frame_137, "frame"), sage_string("__instance__")));
                                                                                                                                                                                                    }
                                                                                                                                                                                                    else {
                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_local_val_111, "val"));
                                                                                                                                                                                                    }
                                                                                                                                                                                                }
                                                                                                                                                                                                else {
                                                                                                                                                                                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_bool(1); sage_dict_set(_obj.as.dict, "halted", _val); _val;});
                                                                                                                                                                                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_load_slot(&sage_local_val_111, "val"); sage_dict_set(_obj.as.dict, "return_value", _val); _val;});
                                                                                                                                                                                                }
                                                                                                                                                                                            }
                                                                                                                                                                                            else {
                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_HALT_97, "OP_HALT")))) {
                                                                                                                                                                                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_bool(1); sage_dict_set(_obj.as.dict, "halted", _val); _val;});
                                                                                                                                                                                                }
                                                                                                                                                                                                else {
                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_CLASS_63, "OP_CLASS")))) {
                                                                                                                                                                                                        sage_define_slot(&sage_local_idx_107, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                                                                                                                                                                                        (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                                                                        sage_define_slot(&sage_local_name_108, sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("constants")), sage_load_slot(&sage_local_idx_107, "idx")));
                                                                                                                                                                                                        sage_define_slot(&sage_local_cls_138, sage_make_dict_from_entries(3, (const char*[]){"__type__", "__name__", "__methods__"}, (SageValue[]){sage_string("class"), sage_load_slot(&sage_local_name_108, "name"), sage_make_dict()}));
                                                                                                                                                                                                        (void)sage_index_set(sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes")), sage_sub(sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("scopes"))), sage_number(1))), sage_load_slot(&sage_local_name_108, "name"), sage_load_slot(&sage_local_cls_138, "cls"));
                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_local_cls_138, "cls"));
                                                                                                                                                                                                    }
                                                                                                                                                                                                    else {
                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_METHOD_64, "OP_METHOD")))) {
                                                                                                                                                                                                            sage_define_slot(&sage_local_idx_107, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                                                                                                                                                                                            (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                                                                            sage_define_slot(&sage_local_name_108, sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("constants")), sage_load_slot(&sage_local_idx_107, "idx")));
                                                                                                                                                                                                            sage_define_slot(&sage_local_func_139, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                            sage_define_slot(&sage_local_cls_138, sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_sub(sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))), sage_number(1))));
                                                                                                                                                                                                            (void)sage_index_set(sage_index(sage_load_slot(&sage_local_cls_138, "cls"), sage_string("__methods__")), sage_load_slot(&sage_local_name_108, "name"), sage_load_slot(&sage_local_func_139, "func"));
                                                                                                                                                                                                        }
                                                                                                                                                                                                        else {
                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_INHERIT_65, "OP_INHERIT")))) {
                                                                                                                                                                                                                sage_define_slot(&sage_local_cls_138, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                sage_define_slot(&sage_local_parent_140, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_type(sage_load_slot(&sage_local_parent_140, "parent")), sage_string("dict")))) {
                                                                                                                                                                                                                    if (sage_truthy(sage_dict_has_fn(sage_load_slot(&sage_local_parent_140, "parent"), sage_string("__methods__")))) {
                                                                                                                                                                                                                        sage_define_slot(&sage_local_methods_141, sage_index(sage_load_slot(&sage_local_parent_140, "parent"), sage_string("__methods__")));
                                                                                                                                                                                                                        sage_define_slot(&sage_local_keys_142, sage_dict_keys_fn(sage_load_slot(&sage_local_methods_141, "methods")));
                                                                                                                                                                                                                        sage_define_slot(&sage_local_k_143, sage_number(0));
                                                                                                                                                                                                                        while (sage_truthy(sage_lt(sage_load_slot(&sage_local_k_143, "k"), sage_len(sage_load_slot(&sage_local_keys_142, "keys"))))) {
                                                                                                                                                                                                                            sage_define_slot(&sage_local_mname_144, sage_index(sage_load_slot(&sage_local_keys_142, "keys"), sage_load_slot(&sage_local_k_143, "k")));
                                                                                                                                                                                                                            if (sage_truthy(sage_not(sage_dict_has_fn(sage_index(sage_load_slot(&sage_local_cls_138, "cls"), sage_string("__methods__")), sage_load_slot(&sage_local_mname_144, "mname"))))) {
                                                                                                                                                                                                                                (void)sage_index_set(sage_index(sage_load_slot(&sage_local_cls_138, "cls"), sage_string("__methods__")), sage_load_slot(&sage_local_mname_144, "mname"), sage_index(sage_load_slot(&sage_local_methods_141, "methods"), sage_load_slot(&sage_local_mname_144, "mname")));
                                                                                                                                                                                                                            }
                                                                                                                                                                                                                            sage_define_slot(&sage_local_k_143, sage_add(sage_load_slot(&sage_local_k_143, "k"), sage_number(1)));
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                    }
                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                        sage_define_slot(&sage_local_keys_142, sage_dict_keys_fn(sage_load_slot(&sage_local_parent_140, "parent")));
                                                                                                                                                                                                                        sage_define_slot(&sage_local_k_143, sage_number(0));
                                                                                                                                                                                                                        while (sage_truthy(sage_lt(sage_load_slot(&sage_local_k_143, "k"), sage_len(sage_load_slot(&sage_local_keys_142, "keys"))))) {
                                                                                                                                                                                                                            sage_define_slot(&sage_local_mname_144, sage_index(sage_load_slot(&sage_local_keys_142, "keys"), sage_load_slot(&sage_local_k_143, "k")));
                                                                                                                                                                                                                            if (sage_truthy(sage_not(sage_dict_has_fn(sage_index(sage_load_slot(&sage_local_cls_138, "cls"), sage_string("__methods__")), sage_load_slot(&sage_local_mname_144, "mname"))))) {
                                                                                                                                                                                                                                (void)sage_index_set(sage_index(sage_load_slot(&sage_local_cls_138, "cls"), sage_string("__methods__")), sage_load_slot(&sage_local_mname_144, "mname"), sage_index(sage_load_slot(&sage_local_parent_140, "parent"), sage_load_slot(&sage_local_mname_144, "mname")));
                                                                                                                                                                                                                            }
                                                                                                                                                                                                                            sage_define_slot(&sage_local_k_143, sage_add(sage_load_slot(&sage_local_k_143, "k"), sage_number(1)));
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                    }
                                                                                                                                                                                                                }
                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_local_cls_138, "cls"));
                                                                                                                                                                                                            }
                                                                                                                                                                                                            else {
                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GET_PROPERTY_19, "OP_GET_PROPERTY")))) {
                                                                                                                                                                                                                    sage_define_slot(&sage_local_idx_107, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                                                                                                                                                                                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                                                                                    sage_define_slot(&sage_local_name_108, sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("constants")), sage_load_slot(&sage_local_idx_107, "idx")));
                                                                                                                                                                                                                    sage_define_slot(&sage_local_obj_123, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_type(sage_load_slot(&sage_local_obj_123, "obj")), sage_string("dict")))) {
                                                                                                                                                                                                                        if (sage_truthy(sage_dict_has_fn(sage_load_slot(&sage_local_obj_123, "obj"), sage_load_slot(&sage_local_name_108, "name")))) {
                                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_index(sage_load_slot(&sage_local_obj_123, "obj"), sage_load_slot(&sage_local_name_108, "name")));
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                            if (sage_truthy(sage_and(sage_dict_has_fn(sage_load_slot(&sage_local_obj_123, "obj"), sage_string("__class__")), sage_dict_has_fn(sage_index(sage_index(sage_load_slot(&sage_local_obj_123, "obj"), sage_string("__class__")), sage_string("__methods__")), sage_load_slot(&sage_local_name_108, "name"))))) {
                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_index(sage_index(sage_index(sage_load_slot(&sage_local_obj_123, "obj"), sage_string("__class__")), sage_string("__methods__")), sage_load_slot(&sage_local_name_108, "name")));
                                                                                                                                                                                                                            }
                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                if (sage_truthy(sage_and(sage_dict_has_fn(sage_load_slot(&sage_local_obj_123, "obj"), sage_string("__methods__")), sage_dict_has_fn(sage_index(sage_load_slot(&sage_local_obj_123, "obj"), sage_string("__methods__")), sage_load_slot(&sage_local_name_108, "name"))))) {
                                                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_index(sage_index(sage_load_slot(&sage_local_obj_123, "obj"), sage_string("__methods__")), sage_load_slot(&sage_local_name_108, "name")));
                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                else {
                                                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_nil());
                                                                                                                                                                                                                                }
                                                                                                                                                                                                                            }
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                    }
                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_index(sage_load_slot(&sage_local_obj_123, "obj"), sage_load_slot(&sage_local_name_108, "name")));
                                                                                                                                                                                                                    }
                                                                                                                                                                                                                }
                                                                                                                                                                                                                else {
                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_SET_PROPERTY_20, "OP_SET_PROPERTY")))) {
                                                                                                                                                                                                                        sage_define_slot(&sage_local_idx_107, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                                                                                                                                                                                                        (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                                                                                        sage_define_slot(&sage_local_name_108, sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("constants")), sage_load_slot(&sage_local_idx_107, "idx")));
                                                                                                                                                                                                                        sage_define_slot(&sage_local_val_111, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                        sage_define_slot(&sage_local_obj_123, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_type(sage_load_slot(&sage_local_obj_123, "obj")), sage_string("dict")))) {
                                                                                                                                                                                                                            (void)sage_index_set(sage_load_slot(&sage_local_obj_123, "obj"), sage_load_slot(&sage_local_name_108, "name"), sage_load_slot(&sage_local_val_111, "val"));
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                            (void)sage_index_set(sage_load_slot(&sage_local_obj_123, "obj"), sage_load_slot(&sage_local_name_108, "name"), sage_load_slot(&sage_local_val_111, "val"));
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_local_val_111, "val"));
                                                                                                                                                                                                                    }
                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_IMPORT_62, "OP_IMPORT")))) {
                                                                                                                                                                                                                            sage_define_slot(&sage_local_idx_107, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                                                                                                                                                                                                            (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                                                                                            sage_define_slot(&sage_local_name_108, sage_index(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("constants")), sage_load_slot(&sage_local_idx_107, "idx")));
                                                                                                                                                                                                                            {
                                                                                                                                                                                                                                if (sage_try_depth >= SAGE_MAX_TRY_DEPTH) sage_fail("Runtime Error: try nesting too deep (max 1024)");
                                                                                                                                                                                                                                int _caught = 0;
                                                                                                                                                                                                                                sage_try_depth++;
                                                                                                                                                                                                                                if (setjmp(sage_try_stack[sage_try_depth - 1]) == 0) {
                                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_name_108, "name"), sage_string("math")))) {
                                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_global_math_4, "math"));
                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_name_108, "name"), sage_string("io")))) {
                                                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_global_io_2, "io"));
                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_name_108, "name"), sage_string("sys")))) {
                                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_global_sys_1, "sys"));
                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_name_108, "name"), sage_string("net")))) {
                                                                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_global_net_5, "net"));
                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                                else {
                                                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_name_108, "name"), sage_string("gpu")))) {
                                                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_global_gpu_7, "gpu"));
                                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_name_108, "name"), sage_string("ml_native")))) {
                                                                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_global_ml_native_8, "ml_native"));
                                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_name_108, "name"), sage_string("thread")))) {
                                                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_load_slot(&sage_global_host_thread_6, "host_thread"));
                                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_make_dict_from_entries(2, (const char*[]){"__type__", "__name__"}, (SageValue[]){sage_string("module"), sage_load_slot(&sage_local_name_108, "name")}));
                                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                } else {
                                                                                                                                                                                                                                    _caught = 1;
                                                                                                                                                                                                                                    sage_define_slot(&sage_local_e_145, sage_exception_value);
                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                sage_try_depth--;
                                                                                                                                                                                                                                if (_caught) {
                                                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_make_dict_from_entries(2, (const char*[]){"__type__", "__name__"}, (SageValue[]){sage_string("module"), sage_load_slot(&sage_local_name_108, "name")}));
                                                                                                                                                                                                                                }
                                                                                                                                                                                                                            }
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_SETUP_TRY_66, "OP_SETUP_TRY")))) {
                                                                                                                                                                                                                                sage_define_slot(&sage_local_handler_146, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                                                                                                                                                                                                                (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("handlers")), sage_make_dict_from_entries(2, (const char*[]){"ip", "stack_size"}, (SageValue[]){sage_load_slot(&sage_local_handler_146, "handler"), sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")))}));
                                                                                                                                                                                                                            }
                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_END_TRY_67, "OP_END_TRY")))) {
                                                                                                                                                                                                                                    (void)sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("handlers")));
                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                else {
                                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_RAISE_68, "OP_RAISE")))) {
                                                                                                                                                                                                                                        sage_define_slot(&sage_local_val_111, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                        (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_load_slot(&sage_local_val_111, "val"); sage_dict_set(_obj.as.dict, "exception_value", _val); _val;});
                                                                                                                                                                                                                                        (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_bool(1); sage_dict_set(_obj.as.dict, "is_throwing", _val); _val;});
                                                                                                                                                                                                                                        if (sage_truthy(sage_gt(sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("handlers"))), sage_number(0)))) {
                                                                                                                                                                                                                                            sage_define_slot(&sage_local_h_147, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("handlers"))));
                                                                                                                                                                                                                                            (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_index(sage_load_slot(&sage_local_h_147, "h"), sage_string("ip")); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                                                                                                            while (sage_truthy(sage_gt(sage_len(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))), sage_index(sage_load_slot(&sage_local_h_147, "h"), sage_string("stack_size"))))) {
                                                                                                                                                                                                                                                (void)sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")));
                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("exception_value")));
                                                                                                                                                                                                                                            (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_bool(0); sage_dict_set(_obj.as.dict, "is_throwing", _val); _val;});
                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                                            sage_print_ln(sage_add(sage_string("Unhandled exception: "), sage_str(sage_load_slot(&sage_local_val_111, "val"))));
                                                                                                                                                                                                                                            (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_bool(1); sage_dict_set(_obj.as.dict, "halted", _val); _val;});
                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_EXEC_AST_STMT_53, "OP_EXEC_AST_STMT")))) {
                                                                                                                                                                                                                                            sage_define_slot(&sage_local_idx_107, sage_call_method(sage_load_slot(&sage_local_ut_105, "ut"), "read_be16", 2, (SageValue[]){sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("code")), sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip"))}));
                                                                                                                                                                                                                                            (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_add(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("ip")), sage_number(2)); sage_dict_set(_obj.as.dict, "ip", _val); _val;});
                                                                                                                                                                                                                                            sage_print_ln(sage_string("Warning: OP_EXEC_AST_STMT delegation not implemented"));
                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_BREAK_59, "OP_BREAK")))) {
                                                                                                                                                                                                                                                sage_print_ln(sage_string("Error: Unexpected loop break opcode"));
                                                                                                                                                                                                                                                (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_bool(1); sage_dict_set(_obj.as.dict, "halted", _val); _val;});
                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_CONTINUE_60, "OP_CONTINUE")))) {
                                                                                                                                                                                                                                                    sage_print_ln(sage_string("Error: Unexpected loop continue opcode"));
                                                                                                                                                                                                                                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_bool(1); sage_dict_set(_obj.as.dict, "halted", _val); _val;});
                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                                else {
                                                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_POLL_EVENTS_69, "OP_GPU_POLL_EVENTS")))) {
                                                                                                                                                                                                                                                        (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "poll_events", 0, NULL);
                                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_WINDOW_SHOULD_CLOSE_70, "OP_GPU_WINDOW_SHOULD_CLOSE")))) {
                                                                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "window_should_close", 0, NULL));
                                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_GET_TIME_71, "OP_GPU_GET_TIME")))) {
                                                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "get_time", 0, NULL));
                                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_KEY_PRESSED_72, "OP_GPU_KEY_PRESSED")))) {
                                                                                                                                                                                                                                                                    (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "key_pressed", 1, (SageValue[]){sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")))}));
                                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                                                else {
                                                                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_KEY_DOWN_73, "OP_GPU_KEY_DOWN")))) {
                                                                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "key_down", 1, (SageValue[]){sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")))}));
                                                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_MOUSE_POS_74, "OP_GPU_MOUSE_POS")))) {
                                                                                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "mouse_pos", 0, NULL));
                                                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_MOUSE_DELTA_75, "OP_GPU_MOUSE_DELTA")))) {
                                                                                                                                                                                                                                                                                (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "mouse_delta", 0, NULL));
                                                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_UPDATE_INPUT_76, "OP_GPU_UPDATE_INPUT")))) {
                                                                                                                                                                                                                                                                                    (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "update_input", 0, NULL);
                                                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                                                                else {
                                                                                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_BEGIN_COMMANDS_77, "OP_GPU_BEGIN_COMMANDS")))) {
                                                                                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "begin_commands", 1, (SageValue[]){sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")))}));
                                                                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_END_COMMANDS_78, "OP_GPU_END_COMMANDS")))) {
                                                                                                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "end_commands", 1, (SageValue[]){sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")))}));
                                                                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_CMD_BEGIN_RP_79, "OP_GPU_CMD_BEGIN_RP")))) {
                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_ca_148, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_h_147, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_w_149, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_fb_150, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_rp_151, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_cmd_152, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "cmd_begin_render_pass", 6, (SageValue[]){sage_load_slot(&sage_local_cmd_152, "cmd"), sage_load_slot(&sage_local_rp_151, "rp"), sage_load_slot(&sage_local_fb_150, "fb"), sage_load_slot(&sage_local_w_149, "w"), sage_load_slot(&sage_local_h_147, "h"), sage_load_slot(&sage_local_ca_148, "ca")});
                                                                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_CMD_END_RP_80, "OP_GPU_CMD_END_RP")))) {
                                                                                                                                                                                                                                                                                                    (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "cmd_end_render_pass", 1, (SageValue[]){sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")))});
                                                                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                                                                                else {
                                                                                                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_CMD_DRAW_81, "OP_GPU_CMD_DRAW")))) {
                                                                                                                                                                                                                                                                                                        sage_define_slot(&sage_local_fi_153, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                        sage_define_slot(&sage_local_fv_154, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                        sage_define_slot(&sage_local_inst_155, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                        sage_define_slot(&sage_local_verts_156, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                        sage_define_slot(&sage_local_cmd_152, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                        (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "cmd_draw", 5, (SageValue[]){sage_load_slot(&sage_local_cmd_152, "cmd"), sage_load_slot(&sage_local_verts_156, "verts"), sage_load_slot(&sage_local_inst_155, "inst"), sage_load_slot(&sage_local_fv_154, "fv"), sage_load_slot(&sage_local_fi_153, "fi")});
                                                                                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_CMD_BIND_GP_82, "OP_GPU_CMD_BIND_GP")))) {
                                                                                                                                                                                                                                                                                                            sage_define_slot(&sage_local_gp_157, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                            sage_define_slot(&sage_local_cmd_152, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                            (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "cmd_bind_graphics_pipeline", 2, (SageValue[]){sage_load_slot(&sage_local_cmd_152, "cmd"), sage_load_slot(&sage_local_gp_157, "gp")});
                                                                                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_CMD_BIND_DS_83, "OP_GPU_CMD_BIND_DS")))) {
                                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_bp_158, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_set_159, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_lay_160, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_cmd_152, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "cmd_bind_descriptor_set", 4, (SageValue[]){sage_load_slot(&sage_local_cmd_152, "cmd"), sage_load_slot(&sage_local_lay_160, "lay"), sage_load_slot(&sage_local_set_159, "set"), sage_load_slot(&sage_local_bp_158, "bp")});
                                                                                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_CMD_SET_VP_84, "OP_GPU_CMD_SET_VP")))) {
                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_maxd_161, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_mind_162, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_vh_163, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_vw_164, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_vy_165, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_vx_166, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_cmd_152, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                    (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "cmd_set_viewport", 7, (SageValue[]){sage_load_slot(&sage_local_cmd_152, "cmd"), sage_load_slot(&sage_local_vx_166, "vx"), sage_load_slot(&sage_local_vy_165, "vy"), sage_load_slot(&sage_local_vw_164, "vw"), sage_load_slot(&sage_local_vh_163, "vh"), sage_load_slot(&sage_local_mind_162, "mind"), sage_load_slot(&sage_local_maxd_161, "maxd")});
                                                                                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                                                                                                else {
                                                                                                                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_CMD_SET_SC_85, "OP_GPU_CMD_SET_SC")))) {
                                                                                                                                                                                                                                                                                                                        sage_define_slot(&sage_local_sh_167, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                        sage_define_slot(&sage_local_sw_168, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                        sage_define_slot(&sage_local_sy_169, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                        sage_define_slot(&sage_local_sx_170, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                        sage_define_slot(&sage_local_cmd_152, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                        (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "cmd_set_scissor", 5, (SageValue[]){sage_load_slot(&sage_local_cmd_152, "cmd"), sage_load_slot(&sage_local_sx_170, "sx"), sage_load_slot(&sage_local_sy_169, "sy"), sage_load_slot(&sage_local_sw_168, "sw"), sage_load_slot(&sage_local_sh_167, "sh")});
                                                                                                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_CMD_BIND_VB_86, "OP_GPU_CMD_BIND_VB")))) {
                                                                                                                                                                                                                                                                                                                            sage_define_slot(&sage_local_buf_171, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                            sage_define_slot(&sage_local_cmd_152, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                            (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "cmd_bind_vertex_buffer", 2, (SageValue[]){sage_load_slot(&sage_local_cmd_152, "cmd"), sage_load_slot(&sage_local_buf_171, "buf")});
                                                                                                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_CMD_BIND_IB_87, "OP_GPU_CMD_BIND_IB")))) {
                                                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_buf_171, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_cmd_152, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "cmd_bind_index_buffer", 2, (SageValue[]){sage_load_slot(&sage_local_cmd_152, "cmd"), sage_load_slot(&sage_local_buf_171, "buf")});
                                                                                                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_CMD_DRAW_IDX_88, "OP_GPU_CMD_DRAW_IDX")))) {
                                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_fi_153, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_vo_172, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_fidx_173, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_inst_155, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_idxc_174, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_cmd_152, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                    (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "cmd_draw_indexed", 6, (SageValue[]){sage_load_slot(&sage_local_cmd_152, "cmd"), sage_load_slot(&sage_local_idxc_174, "idxc"), sage_load_slot(&sage_local_inst_155, "inst"), sage_load_slot(&sage_local_fidx_173, "fidx"), sage_load_slot(&sage_local_vo_172, "vo"), sage_load_slot(&sage_local_fi_153, "fi")});
                                                                                                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                                                                                                                else {
                                                                                                                                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_SUBMIT_SYNC_89, "OP_GPU_SUBMIT_SYNC")))) {
                                                                                                                                                                                                                                                                                                                                        sage_define_slot(&sage_local_f_175, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                        sage_define_slot(&sage_local_s_176, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                        sage_define_slot(&sage_local_w_149, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                        sage_define_slot(&sage_local_cmd_152, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                        (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "submit_with_sync", 4, (SageValue[]){sage_load_slot(&sage_local_cmd_152, "cmd"), sage_load_slot(&sage_local_w_149, "w"), sage_load_slot(&sage_local_s_176, "s"), sage_load_slot(&sage_local_f_175, "f")}));
                                                                                                                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_ACQUIRE_IMG_90, "OP_GPU_ACQUIRE_IMG")))) {
                                                                                                                                                                                                                                                                                                                                            (void)sage_push(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")), sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "acquire_next_image", 1, (SageValue[]){sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")))}));
                                                                                                                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_PRESENT_91, "OP_GPU_PRESENT")))) {
                                                                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_idx_107, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_s_176, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                                (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "present", 2, (SageValue[]){sage_load_slot(&sage_local_s_176, "s"), sage_load_slot(&sage_local_idx_107, "idx")});
                                                                                                                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_WAIT_FENCE_92, "OP_GPU_WAIT_FENCE")))) {
                                                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_t_120, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_f_175, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                                    (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "wait_fence", 2, (SageValue[]){sage_load_slot(&sage_local_f_175, "f"), sage_load_slot(&sage_local_t_120, "t")});
                                                                                                                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                                                                                                                                else {
                                                                                                                                                                                                                                                                                                                                                    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_RESET_FENCE_93, "OP_GPU_RESET_FENCE")))) {
                                                                                                                                                                                                                                                                                                                                                        (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "reset_fence", 1, (SageValue[]){sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack")))});
                                                                                                                                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                                                                                                                                    else {
                                                                                                                                                                                                                                                                                                                                                        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_UPDATE_UNIFORM_94, "OP_GPU_UPDATE_UNIFORM")))) {
                                                                                                                                                                                                                                                                                                                                                            sage_define_slot(&sage_local_data_177, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                                            sage_define_slot(&sage_local_h_147, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                                            (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "update_uniform", 2, (SageValue[]){sage_load_slot(&sage_local_h_147, "h"), sage_load_slot(&sage_local_data_177, "data")});
                                                                                                                                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                                                                                                                                        else {
                                                                                                                                                                                                                                                                                                                                                            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_CMD_PUSH_CONST_95, "OP_GPU_CMD_PUSH_CONST")))) {
                                                                                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_data_177, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_st_178, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_lay_160, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                                                sage_define_slot(&sage_local_cmd_152, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                                                (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "cmd_push_constants", 4, (SageValue[]){sage_load_slot(&sage_local_cmd_152, "cmd"), sage_load_slot(&sage_local_lay_160, "lay"), sage_load_slot(&sage_local_st_178, "st"), sage_load_slot(&sage_local_data_177, "data")});
                                                                                                                                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                                                                                                                                            else {
                                                                                                                                                                                                                                                                                                                                                                if (sage_truthy(sage_eq(sage_load_slot(&sage_local_op_106, "op"), sage_load_slot(&sage_global_OP_GPU_CMD_DISPATCH_96, "OP_GPU_CMD_DISPATCH")))) {
                                                                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_gz_179, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_gy_180, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_gx_181, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                                                    sage_define_slot(&sage_local_cmd_152, sage_pop(sage_index(sage_load_slot(&sage_local_self_104, "self"), sage_string("stack"))));
                                                                                                                                                                                                                                                                                                                                                                    (void)sage_call_method(sage_load_slot(&sage_global_gpu_7, "gpu"), "cmd_dispatch", 4, (SageValue[]){sage_load_slot(&sage_local_cmd_152, "cmd"), sage_load_slot(&sage_local_gx_181, "gx"), sage_load_slot(&sage_local_gy_180, "gy"), sage_load_slot(&sage_local_gz_179, "gz")});
                                                                                                                                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                                                                                                                                                else {
                                                                                                                                                                                                                                                                                                                                                                    sage_print_ln(sage_add(sage_string("Unknown OP: "), sage_str(sage_load_slot(&sage_local_op_106, "op"))));
                                                                                                                                                                                                                                                                                                                                                                    (void)({SageValue _obj = sage_load_slot(&sage_local_self_104, "self"); SageValue _val = sage_bool(1); sage_dict_set(_obj.as.dict, "halted", _val); _val;});
                                                                                                                                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                                }
                                                                                                                                                                                                                                            }
                                                                                                                                                                                                                                        }
                                                                                                                                                                                                                                    }
                                                                                                                                                                                                                                }
                                                                                                                                                                                                                            }
                                                                                                                                                                                                                        }
                                                                                                                                                                                                                    }
                                                                                                                                                                                                                }
                                                                                                                                                                                                            }
                                                                                                                                                                                                        }
                                                                                                                                                                                                    }
                                                                                                                                                                                                }
                                                                                                                                                                                            }
                                                                                                                                                                                        }
                                                                                                                                                                                    }
                                                                                                                                                                                }
                                                                                                                                                                            }
                                                                                                                                                                        }
                                                                                                                                                                    }
                                                                                                                                                                }
                                                                                                                                                            }
                                                                                                                                                        }
                                                                                                                                                    }
                                                                                                                                                }
                                                                                                                                            }
                                                                                                                                        }
                                                                                                                                    }
                                                                                                                                }
                                                                                                                            }
                                                                                                                        }
                                                                                                                    }
                                                                                                                }
                                                                                                            }
                                                                                                        }
                                                                                                    }
                                                                                                }
                                                                                            }
                                                                                        }
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return sage_gc_return(&sage_gc_frame, sage_bool(1));
    return sage_gc_return(&sage_gc_frame, sage_nil());
}

static SageValue sage_method_SGVMUtils_my_int(SageValue _self, int _argc, SageValue* _argv) {
    SageSlot sage_local_x_183 = sage_slot_undefined();
    SageSlot sage_local_self_182 = sage_slot_undefined();
    SageSlot* sage_gc_roots[2] = {&sage_local_x_183, &sage_local_self_182};
    SageGcFrame sage_gc_frame;
    sage_gc_push_frame(&sage_gc_frame, sage_gc_roots, 2);
    sage_define_slot(&sage_local_self_182, _self);
    sage_define_slot(&sage_local_x_183, _argv[0]);
    (void)_argc;
    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_x_183, "x"), sage_nil()))) {
        return sage_gc_return(&sage_gc_frame, sage_number(0));
    }
    return sage_gc_return(&sage_gc_frame, sage_int(sage_load_slot(&sage_local_x_183, "x")));
    return sage_gc_return(&sage_gc_frame, sage_nil());
}

static SageValue sage_method_SGVMUtils_hex_to_byte(SageValue _self, int _argc, SageValue* _argv) {
    SageSlot sage_local_i_191 = sage_slot_undefined();
    SageSlot sage_local_c2_190 = sage_slot_undefined();
    SageSlot sage_local_c1_189 = sage_slot_undefined();
    SageSlot sage_local_v2_188 = sage_slot_undefined();
    SageSlot sage_local_v1_187 = sage_slot_undefined();
    SageSlot sage_local_chars_186 = sage_slot_undefined();
    SageSlot sage_local_h_185 = sage_slot_undefined();
    SageSlot sage_local_self_184 = sage_slot_undefined();
    SageSlot* sage_gc_roots[8] = {&sage_local_i_191, &sage_local_c2_190, &sage_local_c1_189, &sage_local_v2_188, &sage_local_v1_187, &sage_local_chars_186, &sage_local_h_185, &sage_local_self_184};
    SageGcFrame sage_gc_frame;
    sage_gc_push_frame(&sage_gc_frame, sage_gc_roots, 8);
    sage_define_slot(&sage_local_self_184, _self);
    sage_define_slot(&sage_local_h_185, _argv[0]);
    (void)_argc;
    sage_define_slot(&sage_local_chars_186, sage_string("0123456789abcdef"));
    sage_define_slot(&sage_local_v1_187, sage_number(0));
    sage_define_slot(&sage_local_v2_188, sage_number(0));
    sage_define_slot(&sage_local_c1_189, sage_index(sage_load_slot(&sage_local_h_185, "h"), sage_number(0)));
    sage_define_slot(&sage_local_c2_190, sage_index(sage_load_slot(&sage_local_h_185, "h"), sage_number(1)));
    if (sage_truthy(sage_and(sage_gte(sage_ord(sage_load_slot(&sage_local_c1_189, "c1")), sage_number(65)), sage_lte(sage_ord(sage_load_slot(&sage_local_c1_189, "c1")), sage_number(70))))) {
        (void)sage_assign_slot(&sage_local_c1_189, "c1", sage_chr(sage_add(sage_ord(sage_load_slot(&sage_local_c1_189, "c1")), sage_number(32))));
    }
    if (sage_truthy(sage_and(sage_gte(sage_ord(sage_load_slot(&sage_local_c2_190, "c2")), sage_number(65)), sage_lte(sage_ord(sage_load_slot(&sage_local_c2_190, "c2")), sage_number(70))))) {
        (void)sage_assign_slot(&sage_local_c2_190, "c2", sage_chr(sage_add(sage_ord(sage_load_slot(&sage_local_c2_190, "c2")), sage_number(32))));
    }
    sage_define_slot(&sage_local_i_191, sage_number(0));
    while (sage_truthy(sage_lt(sage_load_slot(&sage_local_i_191, "i"), sage_number(16)))) {
        if (sage_truthy(sage_eq(sage_index(sage_load_slot(&sage_local_chars_186, "chars"), sage_load_slot(&sage_local_i_191, "i")), sage_load_slot(&sage_local_c1_189, "c1")))) {
            (void)sage_assign_slot(&sage_local_v1_187, "v1", sage_load_slot(&sage_local_i_191, "i"));
        }
        if (sage_truthy(sage_eq(sage_index(sage_load_slot(&sage_local_chars_186, "chars"), sage_load_slot(&sage_local_i_191, "i")), sage_load_slot(&sage_local_c2_190, "c2")))) {
            (void)sage_assign_slot(&sage_local_v2_188, "v2", sage_load_slot(&sage_local_i_191, "i"));
        }
        (void)sage_assign_slot(&sage_local_i_191, "i", sage_add(sage_load_slot(&sage_local_i_191, "i"), sage_number(1)));
    }
    return sage_gc_return(&sage_gc_frame, sage_add(sage_mul(sage_load_slot(&sage_local_v1_187, "v1"), sage_number(16)), sage_load_slot(&sage_local_v2_188, "v2")));
    return sage_gc_return(&sage_gc_frame, sage_nil());
}

static SageValue sage_method_SGVMUtils_split_lines(SageValue _self, int _argc, SageValue* _argv) {
    SageSlot sage_local_is_cr_200 = sage_slot_undefined();
    SageSlot sage_local_is_nl_199 = sage_slot_undefined();
    SageSlot sage_local_char_val_198 = sage_slot_undefined();
    SageSlot sage_local_i_197 = sage_slot_undefined();
    SageSlot sage_local_nl_196 = sage_slot_undefined();
    SageSlot sage_local_current_195 = sage_slot_undefined();
    SageSlot sage_local_lines_194 = sage_slot_undefined();
    SageSlot sage_local_s_193 = sage_slot_undefined();
    SageSlot sage_local_self_192 = sage_slot_undefined();
    SageSlot* sage_gc_roots[9] = {&sage_local_is_cr_200, &sage_local_is_nl_199, &sage_local_char_val_198, &sage_local_i_197, &sage_local_nl_196, &sage_local_current_195, &sage_local_lines_194, &sage_local_s_193, &sage_local_self_192};
    SageGcFrame sage_gc_frame;
    sage_gc_push_frame(&sage_gc_frame, sage_gc_roots, 9);
    sage_define_slot(&sage_local_self_192, _self);
    sage_define_slot(&sage_local_s_193, _argv[0]);
    (void)_argc;
    sage_define_slot(&sage_local_lines_194, sage_make_array(0, NULL));
    sage_define_slot(&sage_local_current_195, sage_string(""));
    sage_define_slot(&sage_local_nl_196, sage_chr(sage_number(10)));
    sage_define_slot(&sage_local_i_197, sage_number(0));
    while (sage_truthy(sage_lt(sage_load_slot(&sage_local_i_197, "i"), sage_len(sage_load_slot(&sage_local_s_193, "s"))))) {
        sage_define_slot(&sage_local_char_val_198, sage_index(sage_load_slot(&sage_local_s_193, "s"), sage_load_slot(&sage_local_i_197, "i")));
        sage_define_slot(&sage_local_is_nl_199, sage_bool(0));
        sage_define_slot(&sage_local_is_cr_200, sage_bool(0));
        if (sage_truthy(sage_eq(sage_type(sage_load_slot(&sage_local_char_val_198, "char_val")), sage_string("number")))) {
            (void)sage_assign_slot(&sage_local_is_nl_199, "is_nl", sage_eq(sage_load_slot(&sage_local_char_val_198, "char_val"), sage_number(10)));
            (void)sage_assign_slot(&sage_local_is_cr_200, "is_cr", sage_eq(sage_load_slot(&sage_local_char_val_198, "char_val"), sage_number(13)));
        }
        else {
            (void)sage_assign_slot(&sage_local_is_nl_199, "is_nl", sage_eq(sage_load_slot(&sage_local_char_val_198, "char_val"), sage_load_slot(&sage_local_nl_196, "nl")));
            (void)sage_assign_slot(&sage_local_is_cr_200, "is_cr", sage_eq(sage_load_slot(&sage_local_char_val_198, "char_val"), sage_chr(sage_number(13))));
        }
        if (sage_truthy(sage_load_slot(&sage_local_is_nl_199, "is_nl"))) {
            (void)sage_push(sage_load_slot(&sage_local_lines_194, "lines"), sage_load_slot(&sage_local_current_195, "current"));
            (void)sage_assign_slot(&sage_local_current_195, "current", sage_string(""));
        }
        else {
            if (sage_truthy(sage_not(sage_load_slot(&sage_local_is_cr_200, "is_cr")))) {
                if (sage_truthy(sage_eq(sage_type(sage_load_slot(&sage_local_char_val_198, "char_val")), sage_string("number")))) {
                    (void)sage_assign_slot(&sage_local_current_195, "current", sage_add(sage_load_slot(&sage_local_current_195, "current"), sage_chr(sage_load_slot(&sage_local_char_val_198, "char_val"))));
                }
                else {
                    (void)sage_assign_slot(&sage_local_current_195, "current", sage_add(sage_load_slot(&sage_local_current_195, "current"), sage_load_slot(&sage_local_char_val_198, "char_val")));
                }
            }
        }
        (void)sage_assign_slot(&sage_local_i_197, "i", sage_add(sage_load_slot(&sage_local_i_197, "i"), sage_number(1)));
    }
    if (sage_truthy(sage_gt(sage_len(sage_load_slot(&sage_local_current_195, "current")), sage_number(0)))) {
        (void)sage_push(sage_load_slot(&sage_local_lines_194, "lines"), sage_load_slot(&sage_local_current_195, "current"));
    }
    return sage_gc_return(&sage_gc_frame, sage_load_slot(&sage_local_lines_194, "lines"));
    return sage_gc_return(&sage_gc_frame, sage_nil());
}

static SageValue sage_method_SGVMUtils_my_substr(SageValue _self, int _argc, SageValue* _argv) {
    SageSlot sage_local_i_206 = sage_slot_undefined();
    SageSlot sage_local_res_205 = sage_slot_undefined();
    SageSlot sage_local_length_204 = sage_slot_undefined();
    SageSlot sage_local_start_203 = sage_slot_undefined();
    SageSlot sage_local_s_202 = sage_slot_undefined();
    SageSlot sage_local_self_201 = sage_slot_undefined();
    SageSlot* sage_gc_roots[6] = {&sage_local_i_206, &sage_local_res_205, &sage_local_length_204, &sage_local_start_203, &sage_local_s_202, &sage_local_self_201};
    SageGcFrame sage_gc_frame;
    sage_gc_push_frame(&sage_gc_frame, sage_gc_roots, 6);
    sage_define_slot(&sage_local_self_201, _self);
    sage_define_slot(&sage_local_s_202, _argv[0]);
    sage_define_slot(&sage_local_start_203, _argv[1]);
    sage_define_slot(&sage_local_length_204, _argv[2]);
    (void)_argc;
    sage_define_slot(&sage_local_res_205, sage_string(""));
    sage_define_slot(&sage_local_i_206, sage_number(0));
    while (sage_truthy(sage_lt(sage_load_slot(&sage_local_i_206, "i"), sage_load_slot(&sage_local_length_204, "length")))) {
        if (sage_truthy(sage_lt(sage_add(sage_load_slot(&sage_local_start_203, "start"), sage_load_slot(&sage_local_i_206, "i")), sage_len(sage_load_slot(&sage_local_s_202, "s"))))) {
            (void)sage_assign_slot(&sage_local_res_205, "res", sage_add(sage_load_slot(&sage_local_res_205, "res"), sage_index(sage_load_slot(&sage_local_s_202, "s"), sage_add(sage_load_slot(&sage_local_start_203, "start"), sage_load_slot(&sage_local_i_206, "i")))));
        }
        (void)sage_assign_slot(&sage_local_i_206, "i", sage_add(sage_load_slot(&sage_local_i_206, "i"), sage_number(1)));
    }
    return sage_gc_return(&sage_gc_frame, sage_load_slot(&sage_local_res_205, "res"));
    return sage_gc_return(&sage_gc_frame, sage_nil());
}

static SageValue sage_method_SGVMUtils_parse_int_field(SageValue _self, int _argc, SageValue* _argv) {
    SageSlot sage_local_numval_212 = sage_slot_undefined();
    SageSlot sage_local_trimmed_211 = sage_slot_undefined();
    SageSlot sage_local_sub_210 = sage_slot_undefined();
    SageSlot sage_local_offset_209 = sage_slot_undefined();
    SageSlot sage_local_line_208 = sage_slot_undefined();
    SageSlot sage_local_self_207 = sage_slot_undefined();
    SageSlot* sage_gc_roots[6] = {&sage_local_numval_212, &sage_local_trimmed_211, &sage_local_sub_210, &sage_local_offset_209, &sage_local_line_208, &sage_local_self_207};
    SageGcFrame sage_gc_frame;
    sage_gc_push_frame(&sage_gc_frame, sage_gc_roots, 6);
    sage_define_slot(&sage_local_self_207, _self);
    sage_define_slot(&sage_local_line_208, _argv[0]);
    sage_define_slot(&sage_local_offset_209, _argv[1]);
    (void)_argc;
    sage_define_slot(&sage_local_sub_210, sage_call_method(sage_load_slot(&sage_local_self_207, "self"), "my_substr", 3, (SageValue[]){sage_load_slot(&sage_local_line_208, "line"), sage_load_slot(&sage_local_offset_209, "offset"), sage_len(sage_load_slot(&sage_local_line_208, "line"))}));
    sage_define_slot(&sage_local_trimmed_211, sage_call_method(sage_load_slot(&sage_local_self_207, "self"), "trim", 1, (SageValue[]){sage_load_slot(&sage_local_sub_210, "sub")}));
    sage_define_slot(&sage_local_numval_212, sage_tonumber(sage_load_slot(&sage_local_trimmed_211, "trimmed")));
    return sage_gc_return(&sage_gc_frame, sage_call_method(sage_load_slot(&sage_local_self_207, "self"), "my_int", 1, (SageValue[]){sage_load_slot(&sage_local_numval_212, "numval")}));
    return sage_gc_return(&sage_gc_frame, sage_nil());
}

static SageValue sage_method_SGVMUtils_parse_hex_byte(SageValue _self, int _argc, SageValue* _argv) {
    SageSlot sage_local_bval_217 = sage_slot_undefined();
    SageSlot sage_local_sub_216 = sage_slot_undefined();
    SageSlot sage_local_offset_215 = sage_slot_undefined();
    SageSlot sage_local_hex_214 = sage_slot_undefined();
    SageSlot sage_local_self_213 = sage_slot_undefined();
    SageSlot* sage_gc_roots[5] = {&sage_local_bval_217, &sage_local_sub_216, &sage_local_offset_215, &sage_local_hex_214, &sage_local_self_213};
    SageGcFrame sage_gc_frame;
    sage_gc_push_frame(&sage_gc_frame, sage_gc_roots, 5);
    sage_define_slot(&sage_local_self_213, _self);
    sage_define_slot(&sage_local_hex_214, _argv[0]);
    sage_define_slot(&sage_local_offset_215, _argv[1]);
    (void)_argc;
    sage_define_slot(&sage_local_sub_216, sage_call_method(sage_load_slot(&sage_local_self_213, "self"), "my_substr", 3, (SageValue[]){sage_load_slot(&sage_local_hex_214, "hex"), sage_load_slot(&sage_local_offset_215, "offset"), sage_number(2)}));
    sage_define_slot(&sage_local_bval_217, sage_call_method(sage_load_slot(&sage_local_self_213, "self"), "hex_to_byte", 1, (SageValue[]){sage_load_slot(&sage_local_sub_216, "sub")}));
    return sage_gc_return(&sage_gc_frame, sage_call_method(sage_load_slot(&sage_local_self_213, "self"), "my_int", 1, (SageValue[]){sage_load_slot(&sage_local_bval_217, "bval")}));
    return sage_gc_return(&sage_gc_frame, sage_nil());
}

static SageValue sage_method_SGVMUtils_trim(SageValue _self, int _argc, SageValue* _argv) {
    SageSlot sage_local_eidx_221 = sage_slot_undefined();
    SageSlot sage_local_start_220 = sage_slot_undefined();
    SageSlot sage_local_s_219 = sage_slot_undefined();
    SageSlot sage_local_self_218 = sage_slot_undefined();
    SageSlot* sage_gc_roots[4] = {&sage_local_eidx_221, &sage_local_start_220, &sage_local_s_219, &sage_local_self_218};
    SageGcFrame sage_gc_frame;
    sage_gc_push_frame(&sage_gc_frame, sage_gc_roots, 4);
    sage_define_slot(&sage_local_self_218, _self);
    sage_define_slot(&sage_local_s_219, _argv[0]);
    (void)_argc;
    if (sage_truthy(sage_eq(sage_len(sage_load_slot(&sage_local_s_219, "s")), sage_number(0)))) {
        return sage_gc_return(&sage_gc_frame, sage_string(""));
    }
    sage_define_slot(&sage_local_start_220, sage_number(0));
    while (sage_truthy(sage_lt(sage_load_slot(&sage_local_start_220, "start"), sage_len(sage_load_slot(&sage_local_s_219, "s"))))) {
        if (sage_truthy(sage_lte(sage_ord(sage_index(sage_load_slot(&sage_local_s_219, "s"), sage_load_slot(&sage_local_start_220, "start"))), sage_number(32)))) {
            (void)sage_assign_slot(&sage_local_start_220, "start", sage_add(sage_load_slot(&sage_local_start_220, "start"), sage_number(1)));
        }
        else {
            break;
        }
    }
    sage_define_slot(&sage_local_eidx_221, sage_len(sage_load_slot(&sage_local_s_219, "s")));
    while (sage_truthy(sage_gt(sage_load_slot(&sage_local_eidx_221, "eidx"), sage_load_slot(&sage_local_start_220, "start")))) {
        if (sage_truthy(sage_lte(sage_ord(sage_index(sage_load_slot(&sage_local_s_219, "s"), sage_sub(sage_load_slot(&sage_local_eidx_221, "eidx"), sage_number(1)))), sage_number(32)))) {
            (void)sage_assign_slot(&sage_local_eidx_221, "eidx", sage_sub(sage_load_slot(&sage_local_eidx_221, "eidx"), sage_number(1)));
        }
        else {
            break;
        }
    }
    return sage_gc_return(&sage_gc_frame, sage_call_method(sage_load_slot(&sage_local_self_218, "self"), "my_substr", 3, (SageValue[]){sage_load_slot(&sage_local_s_219, "s"), sage_load_slot(&sage_local_start_220, "start"), sage_sub(sage_load_slot(&sage_local_eidx_221, "eidx"), sage_load_slot(&sage_local_start_220, "start"))}));
    return sage_gc_return(&sage_gc_frame, sage_nil());
}

static SageValue sage_method_SGVMUtils_read_be16(SageValue _self, int _argc, SageValue* _argv) {
    SageSlot sage_local_off_224 = sage_slot_undefined();
    SageSlot sage_local_bs_223 = sage_slot_undefined();
    SageSlot sage_local_self_222 = sage_slot_undefined();
    SageSlot* sage_gc_roots[3] = {&sage_local_off_224, &sage_local_bs_223, &sage_local_self_222};
    SageGcFrame sage_gc_frame;
    sage_gc_push_frame(&sage_gc_frame, sage_gc_roots, 3);
    sage_define_slot(&sage_local_self_222, _self);
    sage_define_slot(&sage_local_bs_223, _argv[0]);
    sage_define_slot(&sage_local_off_224, _argv[1]);
    (void)_argc;
    return sage_gc_return(&sage_gc_frame, sage_add(sage_mul(sage_call_method(sage_load_slot(&sage_local_self_222, "self"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_bs_223, "bs"), sage_load_slot(&sage_local_off_224, "off"))}), sage_number(256)), sage_call_method(sage_load_slot(&sage_local_self_222, "self"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_bs_223, "bs"), sage_add(sage_load_slot(&sage_local_off_224, "off"), sage_number(1)))})));
    return sage_gc_return(&sage_gc_frame, sage_nil());
}

static SageValue sage_method_SGVMUtils_read_be32(SageValue _self, int _argc, SageValue* _argv) {
    SageSlot sage_local_off_227 = sage_slot_undefined();
    SageSlot sage_local_bs_226 = sage_slot_undefined();
    SageSlot sage_local_self_225 = sage_slot_undefined();
    SageSlot* sage_gc_roots[3] = {&sage_local_off_227, &sage_local_bs_226, &sage_local_self_225};
    SageGcFrame sage_gc_frame;
    sage_gc_push_frame(&sage_gc_frame, sage_gc_roots, 3);
    sage_define_slot(&sage_local_self_225, _self);
    sage_define_slot(&sage_local_bs_226, _argv[0]);
    sage_define_slot(&sage_local_off_227, _argv[1]);
    (void)_argc;
    return sage_gc_return(&sage_gc_frame, sage_add(sage_add(sage_add(sage_mul(sage_call_method(sage_load_slot(&sage_local_self_225, "self"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_bs_226, "bs"), sage_load_slot(&sage_local_off_227, "off"))}), sage_number(16777216)), sage_mul(sage_call_method(sage_load_slot(&sage_local_self_225, "self"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_bs_226, "bs"), sage_add(sage_load_slot(&sage_local_off_227, "off"), sage_number(1)))}), sage_number(65536))), sage_mul(sage_call_method(sage_load_slot(&sage_local_self_225, "self"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_bs_226, "bs"), sage_add(sage_load_slot(&sage_local_off_227, "off"), sage_number(2)))}), sage_number(256))), sage_call_method(sage_load_slot(&sage_local_self_225, "self"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_bs_226, "bs"), sage_add(sage_load_slot(&sage_local_off_227, "off"), sage_number(3)))})));
    return sage_gc_return(&sage_gc_frame, sage_nil());
}

static SageValue sage_method_SGVMUtils_unpack_double(SageValue _self, int _argc, SageValue* _argv) {
    SageSlot sage_local_i_244 = sage_slot_undefined();
    SageSlot sage_local_e_243 = sage_slot_undefined();
    SageSlot sage_local_p2_242 = sage_slot_undefined();
    SageSlot sage_local_mantissa_241 = sage_slot_undefined();
    SageSlot sage_local_exp_240 = sage_slot_undefined();
    SageSlot sage_local_sign_239 = sage_slot_undefined();
    SageSlot sage_local_b7_238 = sage_slot_undefined();
    SageSlot sage_local_b6_237 = sage_slot_undefined();
    SageSlot sage_local_b5_236 = sage_slot_undefined();
    SageSlot sage_local_b4_235 = sage_slot_undefined();
    SageSlot sage_local_b3_234 = sage_slot_undefined();
    SageSlot sage_local_b2_233 = sage_slot_undefined();
    SageSlot sage_local_b1_232 = sage_slot_undefined();
    SageSlot sage_local_b0_231 = sage_slot_undefined();
    SageSlot sage_local_off_230 = sage_slot_undefined();
    SageSlot sage_local_bs_229 = sage_slot_undefined();
    SageSlot sage_local_self_228 = sage_slot_undefined();
    SageSlot* sage_gc_roots[17] = {&sage_local_i_244, &sage_local_e_243, &sage_local_p2_242, &sage_local_mantissa_241, &sage_local_exp_240, &sage_local_sign_239, &sage_local_b7_238, &sage_local_b6_237, &sage_local_b5_236, &sage_local_b4_235, &sage_local_b3_234, &sage_local_b2_233, &sage_local_b1_232, &sage_local_b0_231, &sage_local_off_230, &sage_local_bs_229, &sage_local_self_228};
    SageGcFrame sage_gc_frame;
    sage_gc_push_frame(&sage_gc_frame, sage_gc_roots, 17);
    sage_define_slot(&sage_local_self_228, _self);
    sage_define_slot(&sage_local_bs_229, _argv[0]);
    sage_define_slot(&sage_local_off_230, _argv[1]);
    (void)_argc;
    sage_define_slot(&sage_local_b0_231, sage_call_method(sage_load_slot(&sage_local_self_228, "self"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_bs_229, "bs"), sage_load_slot(&sage_local_off_230, "off"))}));
    sage_define_slot(&sage_local_b1_232, sage_call_method(sage_load_slot(&sage_local_self_228, "self"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_bs_229, "bs"), sage_add(sage_load_slot(&sage_local_off_230, "off"), sage_number(1)))}));
    sage_define_slot(&sage_local_b2_233, sage_call_method(sage_load_slot(&sage_local_self_228, "self"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_bs_229, "bs"), sage_add(sage_load_slot(&sage_local_off_230, "off"), sage_number(2)))}));
    sage_define_slot(&sage_local_b3_234, sage_call_method(sage_load_slot(&sage_local_self_228, "self"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_bs_229, "bs"), sage_add(sage_load_slot(&sage_local_off_230, "off"), sage_number(3)))}));
    sage_define_slot(&sage_local_b4_235, sage_call_method(sage_load_slot(&sage_local_self_228, "self"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_bs_229, "bs"), sage_add(sage_load_slot(&sage_local_off_230, "off"), sage_number(4)))}));
    sage_define_slot(&sage_local_b5_236, sage_call_method(sage_load_slot(&sage_local_self_228, "self"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_bs_229, "bs"), sage_add(sage_load_slot(&sage_local_off_230, "off"), sage_number(5)))}));
    sage_define_slot(&sage_local_b6_237, sage_call_method(sage_load_slot(&sage_local_self_228, "self"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_bs_229, "bs"), sage_add(sage_load_slot(&sage_local_off_230, "off"), sage_number(6)))}));
    sage_define_slot(&sage_local_b7_238, sage_call_method(sage_load_slot(&sage_local_self_228, "self"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_bs_229, "bs"), sage_add(sage_load_slot(&sage_local_off_230, "off"), sage_number(7)))}));
    sage_define_slot(&sage_local_sign_239, sage_number(1));
    if (sage_truthy(sage_eq(sage_int(sage_div(sage_load_slot(&sage_local_b0_231, "b0"), sage_number(128))), sage_number(1)))) {
        (void)sage_assign_slot(&sage_local_sign_239, "sign", sage_sub(sage_number(0), sage_number(1)));
    }
    sage_define_slot(&sage_local_exp_240, sage_add(sage_mul(sage_int(sage_mod(sage_load_slot(&sage_local_b0_231, "b0"), sage_number(128))), sage_number(16)), sage_int(sage_div(sage_load_slot(&sage_local_b1_232, "b1"), sage_number(16)))));
    sage_define_slot(&sage_local_mantissa_241, sage_number(1));
    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_exp_240, "exp"), sage_number(0)))) {
        (void)sage_assign_slot(&sage_local_mantissa_241, "mantissa", sage_number(0));
        (void)sage_assign_slot(&sage_local_exp_240, "exp", sage_number(1));
    }
    (void)sage_assign_slot(&sage_local_mantissa_241, "mantissa", sage_add(sage_load_slot(&sage_local_mantissa_241, "mantissa"), sage_div(sage_int(sage_mod(sage_load_slot(&sage_local_b1_232, "b1"), sage_number(16))), sage_number(16))));
    (void)sage_assign_slot(&sage_local_mantissa_241, "mantissa", sage_add(sage_load_slot(&sage_local_mantissa_241, "mantissa"), sage_div(sage_load_slot(&sage_local_b2_233, "b2"), sage_number(4096))));
    (void)sage_assign_slot(&sage_local_mantissa_241, "mantissa", sage_add(sage_load_slot(&sage_local_mantissa_241, "mantissa"), sage_div(sage_load_slot(&sage_local_b3_234, "b3"), sage_number(1048576))));
    (void)sage_assign_slot(&sage_local_mantissa_241, "mantissa", sage_add(sage_load_slot(&sage_local_mantissa_241, "mantissa"), sage_div(sage_load_slot(&sage_local_b4_235, "b4"), sage_number(268435456))));
    (void)sage_assign_slot(&sage_local_mantissa_241, "mantissa", sage_add(sage_load_slot(&sage_local_mantissa_241, "mantissa"), sage_div(sage_load_slot(&sage_local_b5_236, "b5"), sage_number(68719476736))));
    (void)sage_assign_slot(&sage_local_mantissa_241, "mantissa", sage_add(sage_load_slot(&sage_local_mantissa_241, "mantissa"), sage_div(sage_load_slot(&sage_local_b6_237, "b6"), sage_number(17592186044416))));
    (void)sage_assign_slot(&sage_local_mantissa_241, "mantissa", sage_add(sage_load_slot(&sage_local_mantissa_241, "mantissa"), sage_div(sage_load_slot(&sage_local_b7_238, "b7"), sage_number(4503599627370496))));
    sage_define_slot(&sage_local_p2_242, sage_number(1));
    sage_define_slot(&sage_local_e_243, sage_sub(sage_load_slot(&sage_local_exp_240, "exp"), sage_number(1023)));
    if (sage_truthy(sage_gt(sage_load_slot(&sage_local_e_243, "e"), sage_number(0)))) {
        sage_define_slot(&sage_local_i_244, sage_number(0));
        while (sage_truthy(sage_lt(sage_load_slot(&sage_local_i_244, "i"), sage_load_slot(&sage_local_e_243, "e")))) {
            (void)sage_assign_slot(&sage_local_p2_242, "p2", sage_mul(sage_load_slot(&sage_local_p2_242, "p2"), sage_number(2)));
            (void)sage_assign_slot(&sage_local_i_244, "i", sage_add(sage_load_slot(&sage_local_i_244, "i"), sage_number(1)));
        }
    }
    else {
        if (sage_truthy(sage_lt(sage_load_slot(&sage_local_e_243, "e"), sage_number(0)))) {
            sage_define_slot(&sage_local_i_244, sage_number(0));
            while (sage_truthy(sage_lt(sage_load_slot(&sage_local_i_244, "i"), sage_sub(sage_number(0), sage_load_slot(&sage_local_e_243, "e"))))) {
                (void)sage_assign_slot(&sage_local_p2_242, "p2", sage_div(sage_load_slot(&sage_local_p2_242, "p2"), sage_number(2)));
                (void)sage_assign_slot(&sage_local_i_244, "i", sage_add(sage_load_slot(&sage_local_i_244, "i"), sage_number(1)));
            }
        }
    }
    return sage_gc_return(&sage_gc_frame, sage_mul(sage_mul(sage_load_slot(&sage_local_sign_239, "sign"), sage_load_slot(&sage_local_mantissa_241, "mantissa")), sage_load_slot(&sage_local_p2_242, "p2")));
    return sage_gc_return(&sage_gc_frame, sage_nil());
}

static SageValue sage_fn_main_99() {
    SageSlot sage_local_idx_265 = sage_slot_undefined();
    SageSlot sage_local_chunk_code_264 = sage_slot_undefined();
    SageSlot sage_local_clen_263 = sage_slot_undefined();
    SageSlot sage_local_c_262 = sage_slot_undefined();
    SageSlot sage_local_chunk_count_261 = sage_slot_undefined();
    SageSlot sage_local_c_idx_260 = sage_slot_undefined();
    SageSlot sage_local_k_259 = sage_slot_undefined();
    SageSlot sage_local_s_258 = sage_slot_undefined();
    SageSlot sage_local_slen_257 = sage_slot_undefined();
    SageSlot sage_local_t_256 = sage_slot_undefined();
    SageSlot sage_local_j_255 = sage_slot_undefined();
    SageSlot sage_local_const_count_254 = sage_slot_undefined();
    SageSlot sage_local_function_count_253 = sage_slot_undefined();
    SageSlot sage_local_metal_vm_252 = sage_slot_undefined();
    SageSlot sage_local_core_utils_251 = sage_slot_undefined();
    SageSlot sage_local_off_250 = sage_slot_undefined();
    SageSlot sage_local_data_249 = sage_slot_undefined();
    SageSlot sage_local_i_248 = sage_slot_undefined();
    SageSlot sage_local_debug_247 = sage_slot_undefined();
    SageSlot sage_local_input_file_246 = sage_slot_undefined();
    SageSlot sage_local_args_245 = sage_slot_undefined();
    SageSlot* sage_gc_roots[21] = {&sage_local_idx_265, &sage_local_chunk_code_264, &sage_local_clen_263, &sage_local_c_262, &sage_local_chunk_count_261, &sage_local_c_idx_260, &sage_local_k_259, &sage_local_s_258, &sage_local_slen_257, &sage_local_t_256, &sage_local_j_255, &sage_local_const_count_254, &sage_local_function_count_253, &sage_local_metal_vm_252, &sage_local_core_utils_251, &sage_local_off_250, &sage_local_data_249, &sage_local_i_248, &sage_local_debug_247, &sage_local_input_file_246, &sage_local_args_245};
    SageGcFrame sage_gc_frame;
    sage_gc_push_frame(&sage_gc_frame, sage_gc_roots, 21);
    sage_define_slot(&sage_local_args_245, sage_native_sys_args());
    sage_define_slot(&sage_local_input_file_246, sage_string(""));
    sage_define_slot(&sage_local_debug_247, sage_bool(0));
    sage_define_slot(&sage_local_i_248, sage_number(0));
    while (sage_truthy(sage_lt(sage_load_slot(&sage_local_i_248, "i"), sage_len(sage_load_slot(&sage_local_args_245, "args"))))) {
        if (sage_truthy(sage_endswith(sage_index(sage_load_slot(&sage_local_args_245, "args"), sage_load_slot(&sage_local_i_248, "i")), sage_string(".sgvm")))) {
            (void)sage_assign_slot(&sage_local_input_file_246, "input_file", sage_index(sage_load_slot(&sage_local_args_245, "args"), sage_load_slot(&sage_local_i_248, "i")));
        }
        else {
            if (sage_truthy(sage_eq(sage_index(sage_load_slot(&sage_local_args_245, "args"), sage_load_slot(&sage_local_i_248, "i")), sage_string("--debug")))) {
                (void)sage_assign_slot(&sage_local_debug_247, "debug", sage_bool(1));
            }
        }
        (void)sage_assign_slot(&sage_local_i_248, "i", sage_add(sage_load_slot(&sage_local_i_248, "i"), sage_number(1)));
    }
    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_input_file_246, "input_file"), sage_string("")))) {
        sage_print_ln(sage_string("Usage: sgvm <file.sgvm> [--debug]"));
        return sage_gc_return(&sage_gc_frame, sage_nil());
    }
    sage_define_slot(&sage_local_data_249, sage_native_io_readbytes(sage_load_slot(&sage_local_input_file_246, "input_file")));
    if (sage_truthy(sage_eq(sage_load_slot(&sage_local_data_249, "data"), sage_nil()))) {
        sage_print_ln(sage_string("Error: Could not read file"));
        return sage_gc_return(&sage_gc_frame, sage_nil());
    }
    sage_define_slot(&sage_local_off_250, sage_number(0));
    sage_define_slot(&sage_local_core_utils_251, sage_construct("SGVMUtils", NULL, 0, NULL));
    if (sage_truthy(sage_and(sage_and(sage_gt(sage_len(sage_load_slot(&sage_local_data_249, "data")), sage_number(2)), sage_eq(sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_data_249, "data"), sage_number(0))}), sage_number(35))), sage_eq(sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_data_249, "data"), sage_number(1))}), sage_number(33))))) {
        while (sage_truthy(sage_and(sage_lt(sage_load_slot(&sage_local_off_250, "off"), sage_len(sage_load_slot(&sage_local_data_249, "data"))), sage_neq(sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_data_249, "data"), sage_load_slot(&sage_local_off_250, "off"))}), sage_number(10))))) {
            (void)sage_assign_slot(&sage_local_off_250, "off", sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(1)));
        }
        if (sage_truthy(sage_lt(sage_load_slot(&sage_local_off_250, "off"), sage_len(sage_load_slot(&sage_local_data_249, "data"))))) {
            (void)sage_assign_slot(&sage_local_off_250, "off", sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(1)));
        }
    }
    if (sage_truthy(sage_or(sage_or(sage_or(sage_or(sage_lt(sage_sub(sage_len(sage_load_slot(&sage_local_data_249, "data")), sage_load_slot(&sage_local_off_250, "off")), sage_number(4)), sage_neq(sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_data_249, "data"), sage_load_slot(&sage_local_off_250, "off"))}), sage_number(83))), sage_neq(sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_data_249, "data"), sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(1)))}), sage_number(71))), sage_neq(sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_data_249, "data"), sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(2)))}), sage_number(86))), sage_neq(sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_data_249, "data"), sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(3)))}), sage_number(77))))) {
        sage_print_ln(sage_string("Error: Invalid SGVM header"));
        return sage_gc_return(&sage_gc_frame, sage_nil());
    }
    sage_define_slot(&sage_local_metal_vm_252, sage_construct("MetalVM", NULL, 0, (SageValue[]){sage_nil()}));
    (void)({SageValue _obj = sage_load_slot(&sage_local_metal_vm_252, "metal_vm"); SageValue _val = sage_load_slot(&sage_local_debug_247, "debug"); sage_dict_set(_obj.as.dict, "trace", _val); _val;});
    (void)sage_assign_slot(&sage_local_off_250, "off", sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(6)));
    if (sage_truthy(sage_gt(sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(4)), sage_len(sage_load_slot(&sage_local_data_249, "data"))))) {
        sage_print_ln(sage_string("Error: Truncated SGVM file header"));
        return sage_gc_return(&sage_gc_frame, sage_nil());
    }
    sage_define_slot(&sage_local_function_count_253, sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "my_int", 1, (SageValue[]){sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "read_be16", 2, (SageValue[]){sage_load_slot(&sage_local_data_249, "data"), sage_load_slot(&sage_local_off_250, "off")})}));
    (void)sage_assign_slot(&sage_local_off_250, "off", sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(2)));
    sage_define_slot(&sage_local_const_count_254, sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "my_int", 1, (SageValue[]){sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "read_be16", 2, (SageValue[]){sage_load_slot(&sage_local_data_249, "data"), sage_load_slot(&sage_local_off_250, "off")})}));
    (void)sage_assign_slot(&sage_local_off_250, "off", sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(2)));
    sage_define_slot(&sage_local_j_255, sage_number(0));
    while (sage_truthy(sage_lt(sage_load_slot(&sage_local_j_255, "j"), sage_load_slot(&sage_local_const_count_254, "const_count")))) {
        if (sage_truthy(sage_gte(sage_load_slot(&sage_local_off_250, "off"), sage_len(sage_load_slot(&sage_local_data_249, "data"))))) {
            sage_print_ln(sage_string("Error: Truncated constant pool"));
            return sage_gc_return(&sage_gc_frame, sage_nil());
        }
        sage_define_slot(&sage_local_t_256, sage_index(sage_load_slot(&sage_local_data_249, "data"), sage_load_slot(&sage_local_off_250, "off")));
        (void)sage_assign_slot(&sage_local_off_250, "off", sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(1)));
        if (sage_truthy(sage_eq(sage_load_slot(&sage_local_t_256, "t"), sage_number(1)))) {
            if (sage_truthy(sage_gt(sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(8)), sage_len(sage_load_slot(&sage_local_data_249, "data"))))) {
                sage_print_ln(sage_string("Error: Truncated double constant"));
                return sage_gc_return(&sage_gc_frame, sage_nil());
            }
            (void)sage_push(sage_index(sage_load_slot(&sage_local_metal_vm_252, "metal_vm"), sage_string("constants")), sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "unpack_double", 2, (SageValue[]){sage_load_slot(&sage_local_data_249, "data"), sage_load_slot(&sage_local_off_250, "off")}));
            (void)sage_assign_slot(&sage_local_off_250, "off", sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(8)));
        }
        else {
            if (sage_truthy(sage_eq(sage_load_slot(&sage_local_t_256, "t"), sage_number(3)))) {
                if (sage_truthy(sage_gt(sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(2)), sage_len(sage_load_slot(&sage_local_data_249, "data"))))) {
                    sage_print_ln(sage_string("Error: Truncated string constant length"));
                    return sage_gc_return(&sage_gc_frame, sage_nil());
                }
                sage_define_slot(&sage_local_slen_257, sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "my_int", 1, (SageValue[]){sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "read_be16", 2, (SageValue[]){sage_load_slot(&sage_local_data_249, "data"), sage_load_slot(&sage_local_off_250, "off")})}));
                (void)sage_assign_slot(&sage_local_off_250, "off", sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(2)));
                if (sage_truthy(sage_gt(sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_load_slot(&sage_local_slen_257, "slen")), sage_len(sage_load_slot(&sage_local_data_249, "data"))))) {
                    sage_print_ln(sage_string("Error: Truncated string constant value"));
                    return sage_gc_return(&sage_gc_frame, sage_nil());
                }
                sage_define_slot(&sage_local_s_258, sage_string(""));
                sage_define_slot(&sage_local_k_259, sage_number(0));
                while (sage_truthy(sage_lt(sage_load_slot(&sage_local_k_259, "k"), sage_load_slot(&sage_local_slen_257, "slen")))) {
                    (void)sage_assign_slot(&sage_local_s_258, "s", sage_add(sage_load_slot(&sage_local_s_258, "s"), sage_chr(sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "my_int", 1, (SageValue[]){sage_index(sage_load_slot(&sage_local_data_249, "data"), sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_load_slot(&sage_local_k_259, "k")))}))));
                    (void)sage_assign_slot(&sage_local_k_259, "k", sage_add(sage_load_slot(&sage_local_k_259, "k"), sage_number(1)));
                }
                (void)sage_push(sage_index(sage_load_slot(&sage_local_metal_vm_252, "metal_vm"), sage_string("constants")), sage_load_slot(&sage_local_s_258, "s"));
                (void)sage_assign_slot(&sage_local_off_250, "off", sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_load_slot(&sage_local_slen_257, "slen")));
            }
            else {
                sage_print_ln(sage_add(sage_string("Error: Invalid constant type: "), sage_str(sage_load_slot(&sage_local_t_256, "t"))));
                return sage_gc_return(&sage_gc_frame, sage_nil());
            }
        }
        (void)sage_assign_slot(&sage_local_j_255, "j", sage_add(sage_load_slot(&sage_local_j_255, "j"), sage_number(1)));
    }
    if (sage_truthy(sage_load_slot(&sage_local_debug_247, "debug"))) {
        sage_print_ln(sage_add(sage_string("Constants count: "), sage_str(sage_len(sage_index(sage_load_slot(&sage_local_metal_vm_252, "metal_vm"), sage_string("constants"))))));
        sage_define_slot(&sage_local_c_idx_260, sage_number(0));
        while (sage_truthy(sage_lt(sage_load_slot(&sage_local_c_idx_260, "c_idx"), sage_len(sage_index(sage_load_slot(&sage_local_metal_vm_252, "metal_vm"), sage_string("constants")))))) {
            sage_print_ln(sage_add(sage_add(sage_add(sage_string("Const "), sage_str(sage_load_slot(&sage_local_c_idx_260, "c_idx"))), sage_string(": ")), sage_str(sage_index(sage_index(sage_load_slot(&sage_local_metal_vm_252, "metal_vm"), sage_string("constants")), sage_load_slot(&sage_local_c_idx_260, "c_idx")))));
            (void)sage_assign_slot(&sage_local_c_idx_260, "c_idx", sage_add(sage_load_slot(&sage_local_c_idx_260, "c_idx"), sage_number(1)));
        }
        sage_print_ln(sage_add(sage_add(sage_add(sage_string("data len: "), sage_str(sage_len(sage_load_slot(&sage_local_data_249, "data")))), sage_string(" off: ")), sage_str(sage_load_slot(&sage_local_off_250, "off"))));
    }
    if (sage_truthy(sage_gt(sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(4)), sage_len(sage_load_slot(&sage_local_data_249, "data"))))) {
        sage_print_ln(sage_string("Error: Truncated chunk count"));
        return sage_gc_return(&sage_gc_frame, sage_nil());
    }
    sage_define_slot(&sage_local_chunk_count_261, sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "my_int", 1, (SageValue[]){sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "read_be32", 2, (SageValue[]){sage_load_slot(&sage_local_data_249, "data"), sage_load_slot(&sage_local_off_250, "off")})}));
    (void)sage_assign_slot(&sage_local_off_250, "off", sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(4)));
    sage_define_slot(&sage_local_c_262, sage_number(0));
    while (sage_truthy(sage_lt(sage_load_slot(&sage_local_c_262, "c"), sage_load_slot(&sage_local_chunk_count_261, "chunk_count")))) {
        if (sage_truthy(sage_gt(sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(4)), sage_len(sage_load_slot(&sage_local_data_249, "data"))))) {
            sage_print_ln(sage_string("Error: Truncated chunk header"));
            return sage_gc_return(&sage_gc_frame, sage_nil());
        }
        sage_define_slot(&sage_local_clen_263, sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "my_int", 1, (SageValue[]){sage_call_method(sage_load_slot(&sage_local_core_utils_251, "core_utils"), "read_be32", 2, (SageValue[]){sage_load_slot(&sage_local_data_249, "data"), sage_load_slot(&sage_local_off_250, "off")})}));
        (void)sage_assign_slot(&sage_local_off_250, "off", sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_number(4)));
        if (sage_truthy(sage_gt(sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_load_slot(&sage_local_clen_263, "clen")), sage_len(sage_load_slot(&sage_local_data_249, "data"))))) {
            sage_print_ln(sage_string("Error: Truncated chunk data"));
            return sage_gc_return(&sage_gc_frame, sage_nil());
        }
        sage_define_slot(&sage_local_chunk_code_264, sage_make_array(0, NULL));
        sage_define_slot(&sage_local_k_259, sage_number(0));
        while (sage_truthy(sage_lt(sage_load_slot(&sage_local_k_259, "k"), sage_load_slot(&sage_local_clen_263, "clen")))) {
            (void)sage_push(sage_load_slot(&sage_local_chunk_code_264, "chunk_code"), sage_index(sage_load_slot(&sage_local_data_249, "data"), sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_load_slot(&sage_local_k_259, "k"))));
            (void)sage_assign_slot(&sage_local_k_259, "k", sage_add(sage_load_slot(&sage_local_k_259, "k"), sage_number(1)));
        }
        (void)sage_push(sage_index(sage_load_slot(&sage_local_metal_vm_252, "metal_vm"), sage_string("chunks")), sage_load_slot(&sage_local_chunk_code_264, "chunk_code"));
        (void)sage_assign_slot(&sage_local_off_250, "off", sage_add(sage_load_slot(&sage_local_off_250, "off"), sage_load_slot(&sage_local_clen_263, "clen")));
        (void)sage_assign_slot(&sage_local_c_262, "c", sage_add(sage_load_slot(&sage_local_c_262, "c"), sage_number(1)));
    }
    if (sage_truthy(sage_load_slot(&sage_local_debug_247, "debug"))) {
        sage_print_ln(sage_add(sage_string("Functions count: "), sage_str(sage_load_slot(&sage_local_function_count_253, "function_count"))));
        sage_print_ln(sage_add(sage_string("Chunks count: "), sage_str(sage_len(sage_index(sage_load_slot(&sage_local_metal_vm_252, "metal_vm"), sage_string("chunks"))))));
    }
    sage_define_slot(&sage_local_idx_265, sage_load_slot(&sage_local_function_count_253, "function_count"));
    while (sage_truthy(sage_and(sage_lt(sage_load_slot(&sage_local_idx_265, "idx"), sage_len(sage_index(sage_load_slot(&sage_local_metal_vm_252, "metal_vm"), sage_string("chunks")))), sage_not(sage_index(sage_load_slot(&sage_local_metal_vm_252, "metal_vm"), sage_string("is_throwing")))))) {
        (void)sage_call_method(sage_load_slot(&sage_local_metal_vm_252, "metal_vm"), "run", 1, (SageValue[]){sage_index(sage_index(sage_load_slot(&sage_local_metal_vm_252, "metal_vm"), sage_string("chunks")), sage_load_slot(&sage_local_idx_265, "idx"))});
        (void)sage_assign_slot(&sage_local_idx_265, "idx", sage_add(sage_load_slot(&sage_local_idx_265, "idx"), sage_number(1)));
    }
    return sage_gc_return(&sage_gc_frame, sage_nil());
}

int sage_argc; char** sage_argv;
int main(int argc, char** argv) {
    sage_argc = argc; sage_argv = argv;
    sage_global_g_gil_98 = sage_slot_undefined();
    sage_global_OP_HALT_97 = sage_slot_undefined();
    sage_global_OP_GPU_CMD_DISPATCH_96 = sage_slot_undefined();
    sage_global_OP_GPU_CMD_PUSH_CONST_95 = sage_slot_undefined();
    sage_global_OP_GPU_UPDATE_UNIFORM_94 = sage_slot_undefined();
    sage_global_OP_GPU_RESET_FENCE_93 = sage_slot_undefined();
    sage_global_OP_GPU_WAIT_FENCE_92 = sage_slot_undefined();
    sage_global_OP_GPU_PRESENT_91 = sage_slot_undefined();
    sage_global_OP_GPU_ACQUIRE_IMG_90 = sage_slot_undefined();
    sage_global_OP_GPU_SUBMIT_SYNC_89 = sage_slot_undefined();
    sage_global_OP_GPU_CMD_DRAW_IDX_88 = sage_slot_undefined();
    sage_global_OP_GPU_CMD_BIND_IB_87 = sage_slot_undefined();
    sage_global_OP_GPU_CMD_BIND_VB_86 = sage_slot_undefined();
    sage_global_OP_GPU_CMD_SET_SC_85 = sage_slot_undefined();
    sage_global_OP_GPU_CMD_SET_VP_84 = sage_slot_undefined();
    sage_global_OP_GPU_CMD_BIND_DS_83 = sage_slot_undefined();
    sage_global_OP_GPU_CMD_BIND_GP_82 = sage_slot_undefined();
    sage_global_OP_GPU_CMD_DRAW_81 = sage_slot_undefined();
    sage_global_OP_GPU_CMD_END_RP_80 = sage_slot_undefined();
    sage_global_OP_GPU_CMD_BEGIN_RP_79 = sage_slot_undefined();
    sage_global_OP_GPU_END_COMMANDS_78 = sage_slot_undefined();
    sage_global_OP_GPU_BEGIN_COMMANDS_77 = sage_slot_undefined();
    sage_global_OP_GPU_UPDATE_INPUT_76 = sage_slot_undefined();
    sage_global_OP_GPU_MOUSE_DELTA_75 = sage_slot_undefined();
    sage_global_OP_GPU_MOUSE_POS_74 = sage_slot_undefined();
    sage_global_OP_GPU_KEY_DOWN_73 = sage_slot_undefined();
    sage_global_OP_GPU_KEY_PRESSED_72 = sage_slot_undefined();
    sage_global_OP_GPU_GET_TIME_71 = sage_slot_undefined();
    sage_global_OP_GPU_WINDOW_SHOULD_CLOSE_70 = sage_slot_undefined();
    sage_global_OP_GPU_POLL_EVENTS_69 = sage_slot_undefined();
    sage_global_OP_RAISE_68 = sage_slot_undefined();
    sage_global_OP_END_TRY_67 = sage_slot_undefined();
    sage_global_OP_SETUP_TRY_66 = sage_slot_undefined();
    sage_global_OP_INHERIT_65 = sage_slot_undefined();
    sage_global_OP_METHOD_64 = sage_slot_undefined();
    sage_global_OP_CLASS_63 = sage_slot_undefined();
    sage_global_OP_IMPORT_62 = sage_slot_undefined();
    sage_global_OP_LOOP_BACK_61 = sage_slot_undefined();
    sage_global_OP_CONTINUE_60 = sage_slot_undefined();
    sage_global_OP_BREAK_59 = sage_slot_undefined();
    sage_global_OP_ARRAY_LEN_58 = sage_slot_undefined();
    sage_global_OP_DUP_57 = sage_slot_undefined();
    sage_global_OP_POP_ENV_56 = sage_slot_undefined();
    sage_global_OP_PUSH_ENV_55 = sage_slot_undefined();
    sage_global_OP_RETURN_54 = sage_slot_undefined();
    sage_global_OP_EXEC_AST_STMT_53 = sage_slot_undefined();
    sage_global_OP_PRINT_52 = sage_slot_undefined();
    sage_global_OP_DICT_51 = sage_slot_undefined();
    sage_global_OP_TUPLE_50 = sage_slot_undefined();
    sage_global_OP_ARRAY_49 = sage_slot_undefined();
    sage_global_OP_CALL_METHOD_48 = sage_slot_undefined();
    sage_global_OP_CALL_47 = sage_slot_undefined();
    sage_global_OP_JUMP_IF_FALSE_46 = sage_slot_undefined();
    sage_global_OP_JUMP_45 = sage_slot_undefined();
    sage_global_OP_TRUTHY_44 = sage_slot_undefined();
    sage_global_OP_NOT_43 = sage_slot_undefined();
    sage_global_OP_SHIFT_RIGHT_42 = sage_slot_undefined();
    sage_global_OP_SHIFT_LEFT_41 = sage_slot_undefined();
    sage_global_OP_BIT_NOT_40 = sage_slot_undefined();
    sage_global_OP_BIT_XOR_39 = sage_slot_undefined();
    sage_global_OP_BIT_OR_38 = sage_slot_undefined();
    sage_global_OP_BIT_AND_37 = sage_slot_undefined();
    sage_global_OP_LESS_EQUAL_36 = sage_slot_undefined();
    sage_global_OP_LESS_35 = sage_slot_undefined();
    sage_global_OP_GREATER_EQUAL_34 = sage_slot_undefined();
    sage_global_OP_GREATER_33 = sage_slot_undefined();
    sage_global_OP_NOT_EQUAL_32 = sage_slot_undefined();
    sage_global_OP_EQUAL_31 = sage_slot_undefined();
    sage_global_OP_NEGATE_30 = sage_slot_undefined();
    sage_global_OP_MOD_29 = sage_slot_undefined();
    sage_global_OP_DIV_28 = sage_slot_undefined();
    sage_global_OP_MUL_27 = sage_slot_undefined();
    sage_global_OP_SUB_26 = sage_slot_undefined();
    sage_global_OP_ADD_25 = sage_slot_undefined();
    sage_global_OP_SLICE_24 = sage_slot_undefined();
    sage_global_OP_LOAD_FUNCTION_23 = sage_slot_undefined();
    sage_global_OP_SET_INDEX_22 = sage_slot_undefined();
    sage_global_OP_GET_INDEX_21 = sage_slot_undefined();
    sage_global_OP_SET_PROPERTY_20 = sage_slot_undefined();
    sage_global_OP_GET_PROPERTY_19 = sage_slot_undefined();
    sage_global_OP_DEFINE_FUNCTION_18 = sage_slot_undefined();
    sage_global_OP_SET_GLOBAL_17 = sage_slot_undefined();
    sage_global_OP_DEFINE_GLOBAL_16 = sage_slot_undefined();
    sage_global_OP_GET_GLOBAL_15 = sage_slot_undefined();
    sage_global_OP_POP_14 = sage_slot_undefined();
    sage_global_OP_FALSE_13 = sage_slot_undefined();
    sage_global_OP_TRUE_12 = sage_slot_undefined();
    sage_global_OP_NIL_11 = sage_slot_undefined();
    sage_global_OP_CONSTANT_10 = sage_slot_undefined();
    sage_global_sgvm_core_9 = sage_slot_undefined();
    sage_global_ml_native_8 = sage_slot_undefined();
    sage_global_gpu_7 = sage_slot_undefined();
    sage_global_host_thread_6 = sage_slot_undefined();
    sage_global_net_5 = sage_slot_undefined();
    sage_global_math_4 = sage_slot_undefined();
    sage_global_sgvm_vm_3 = sage_slot_undefined();
    sage_global_io_2 = sage_slot_undefined();
    sage_global_sys_1 = sage_slot_undefined();
    SageSlot* sage_gc_global_roots[98] = {&sage_global_g_gil_98, &sage_global_OP_HALT_97, &sage_global_OP_GPU_CMD_DISPATCH_96, &sage_global_OP_GPU_CMD_PUSH_CONST_95, &sage_global_OP_GPU_UPDATE_UNIFORM_94, &sage_global_OP_GPU_RESET_FENCE_93, &sage_global_OP_GPU_WAIT_FENCE_92, &sage_global_OP_GPU_PRESENT_91, &sage_global_OP_GPU_ACQUIRE_IMG_90, &sage_global_OP_GPU_SUBMIT_SYNC_89, &sage_global_OP_GPU_CMD_DRAW_IDX_88, &sage_global_OP_GPU_CMD_BIND_IB_87, &sage_global_OP_GPU_CMD_BIND_VB_86, &sage_global_OP_GPU_CMD_SET_SC_85, &sage_global_OP_GPU_CMD_SET_VP_84, &sage_global_OP_GPU_CMD_BIND_DS_83, &sage_global_OP_GPU_CMD_BIND_GP_82, &sage_global_OP_GPU_CMD_DRAW_81, &sage_global_OP_GPU_CMD_END_RP_80, &sage_global_OP_GPU_CMD_BEGIN_RP_79, &sage_global_OP_GPU_END_COMMANDS_78, &sage_global_OP_GPU_BEGIN_COMMANDS_77, &sage_global_OP_GPU_UPDATE_INPUT_76, &sage_global_OP_GPU_MOUSE_DELTA_75, &sage_global_OP_GPU_MOUSE_POS_74, &sage_global_OP_GPU_KEY_DOWN_73, &sage_global_OP_GPU_KEY_PRESSED_72, &sage_global_OP_GPU_GET_TIME_71, &sage_global_OP_GPU_WINDOW_SHOULD_CLOSE_70, &sage_global_OP_GPU_POLL_EVENTS_69, &sage_global_OP_RAISE_68, &sage_global_OP_END_TRY_67, &sage_global_OP_SETUP_TRY_66, &sage_global_OP_INHERIT_65, &sage_global_OP_METHOD_64, &sage_global_OP_CLASS_63, &sage_global_OP_IMPORT_62, &sage_global_OP_LOOP_BACK_61, &sage_global_OP_CONTINUE_60, &sage_global_OP_BREAK_59, &sage_global_OP_ARRAY_LEN_58, &sage_global_OP_DUP_57, &sage_global_OP_POP_ENV_56, &sage_global_OP_PUSH_ENV_55, &sage_global_OP_RETURN_54, &sage_global_OP_EXEC_AST_STMT_53, &sage_global_OP_PRINT_52, &sage_global_OP_DICT_51, &sage_global_OP_TUPLE_50, &sage_global_OP_ARRAY_49, &sage_global_OP_CALL_METHOD_48, &sage_global_OP_CALL_47, &sage_global_OP_JUMP_IF_FALSE_46, &sage_global_OP_JUMP_45, &sage_global_OP_TRUTHY_44, &sage_global_OP_NOT_43, &sage_global_OP_SHIFT_RIGHT_42, &sage_global_OP_SHIFT_LEFT_41, &sage_global_OP_BIT_NOT_40, &sage_global_OP_BIT_XOR_39, &sage_global_OP_BIT_OR_38, &sage_global_OP_BIT_AND_37, &sage_global_OP_LESS_EQUAL_36, &sage_global_OP_LESS_35, &sage_global_OP_GREATER_EQUAL_34, &sage_global_OP_GREATER_33, &sage_global_OP_NOT_EQUAL_32, &sage_global_OP_EQUAL_31, &sage_global_OP_NEGATE_30, &sage_global_OP_MOD_29, &sage_global_OP_DIV_28, &sage_global_OP_MUL_27, &sage_global_OP_SUB_26, &sage_global_OP_ADD_25, &sage_global_OP_SLICE_24, &sage_global_OP_LOAD_FUNCTION_23, &sage_global_OP_SET_INDEX_22, &sage_global_OP_GET_INDEX_21, &sage_global_OP_SET_PROPERTY_20, &sage_global_OP_GET_PROPERTY_19, &sage_global_OP_DEFINE_FUNCTION_18, &sage_global_OP_SET_GLOBAL_17, &sage_global_OP_DEFINE_GLOBAL_16, &sage_global_OP_GET_GLOBAL_15, &sage_global_OP_POP_14, &sage_global_OP_FALSE_13, &sage_global_OP_TRUE_12, &sage_global_OP_NIL_11, &sage_global_OP_CONSTANT_10, &sage_global_sgvm_core_9, &sage_global_ml_native_8, &sage_global_gpu_7, &sage_global_host_thread_6, &sage_global_net_5, &sage_global_math_4, &sage_global_sgvm_vm_3, &sage_global_io_2, &sage_global_sys_1};
    SageGcFrame sage_gc_main_frame;
    sage_gc_push_frame(&sage_gc_main_frame, sage_gc_global_roots, 98);
    sage_register_class("MetalVM", NULL);
    sage_register_method("MetalVM", "init", sage_method_MetalVM_init);
    sage_register_method("MetalVM", "setup_builtins", sage_method_MetalVM_setup_builtins);
    sage_register_method("MetalVM", "run", sage_method_MetalVM_run);
    sage_register_method("MetalVM", "run_step", sage_method_MetalVM_run_step);
    sage_register_class("SGVMUtils", NULL);
    sage_register_method("SGVMUtils", "my_int", sage_method_SGVMUtils_my_int);
    sage_register_method("SGVMUtils", "hex_to_byte", sage_method_SGVMUtils_hex_to_byte);
    sage_register_method("SGVMUtils", "split_lines", sage_method_SGVMUtils_split_lines);
    sage_register_method("SGVMUtils", "my_substr", sage_method_SGVMUtils_my_substr);
    sage_register_method("SGVMUtils", "parse_int_field", sage_method_SGVMUtils_parse_int_field);
    sage_register_method("SGVMUtils", "parse_hex_byte", sage_method_SGVMUtils_parse_hex_byte);
    sage_register_method("SGVMUtils", "trim", sage_method_SGVMUtils_trim);
    sage_register_method("SGVMUtils", "read_be16", sage_method_SGVMUtils_read_be16);
    sage_register_method("SGVMUtils", "read_be32", sage_method_SGVMUtils_read_be32);
    sage_register_method("SGVMUtils", "unpack_double", sage_method_SGVMUtils_unpack_double);
    sage_define_slot(&sage_global_sys_1, sage_init_native_module("sys"));
    sage_define_slot(&sage_global_io_2, sage_init_native_module("io"));
    sage_define_slot(&sage_global_sgvm_vm_3, sage_init_native_module("sgvm_vm"));
    sage_define_slot(&sage_global_io_2, sage_init_native_module("io"));
    sage_define_slot(&sage_global_math_4, sage_init_native_module("math"));
    sage_define_slot(&sage_global_net_5, sage_init_native_module("net"));
    sage_define_slot(&sage_global_host_thread_6, sage_init_native_module("thread"));
    sage_define_slot(&sage_global_sys_1, sage_init_native_module("sys"));
    sage_define_slot(&sage_global_gpu_7, sage_init_native_module("gpu"));
    sage_define_slot(&sage_global_ml_native_8, sage_init_native_module("ml_native"));
    sage_define_slot(&sage_global_sgvm_core_9, sage_init_native_module("sgvm_core"));
    sage_define_slot(&sage_global_OP_CONSTANT_10, sage_number(0));
    sage_define_slot(&sage_global_OP_NIL_11, sage_number(1));
    sage_define_slot(&sage_global_OP_TRUE_12, sage_number(2));
    sage_define_slot(&sage_global_OP_FALSE_13, sage_number(3));
    sage_define_slot(&sage_global_OP_POP_14, sage_number(4));
    sage_define_slot(&sage_global_OP_GET_GLOBAL_15, sage_number(5));
    sage_define_slot(&sage_global_OP_DEFINE_GLOBAL_16, sage_number(6));
    sage_define_slot(&sage_global_OP_SET_GLOBAL_17, sage_number(7));
    sage_define_slot(&sage_global_OP_DEFINE_FUNCTION_18, sage_number(8));
    sage_define_slot(&sage_global_OP_GET_PROPERTY_19, sage_number(9));
    sage_define_slot(&sage_global_OP_SET_PROPERTY_20, sage_number(10));
    sage_define_slot(&sage_global_OP_GET_INDEX_21, sage_number(11));
    sage_define_slot(&sage_global_OP_SET_INDEX_22, sage_number(12));
    sage_define_slot(&sage_global_OP_LOAD_FUNCTION_23, sage_number(13));
    sage_define_slot(&sage_global_OP_SLICE_24, sage_number(14));
    sage_define_slot(&sage_global_OP_ADD_25, sage_number(15));
    sage_define_slot(&sage_global_OP_SUB_26, sage_number(16));
    sage_define_slot(&sage_global_OP_MUL_27, sage_number(17));
    sage_define_slot(&sage_global_OP_DIV_28, sage_number(18));
    sage_define_slot(&sage_global_OP_MOD_29, sage_number(19));
    sage_define_slot(&sage_global_OP_NEGATE_30, sage_number(20));
    sage_define_slot(&sage_global_OP_EQUAL_31, sage_number(21));
    sage_define_slot(&sage_global_OP_NOT_EQUAL_32, sage_number(22));
    sage_define_slot(&sage_global_OP_GREATER_33, sage_number(23));
    sage_define_slot(&sage_global_OP_GREATER_EQUAL_34, sage_number(24));
    sage_define_slot(&sage_global_OP_LESS_35, sage_number(25));
    sage_define_slot(&sage_global_OP_LESS_EQUAL_36, sage_number(26));
    sage_define_slot(&sage_global_OP_BIT_AND_37, sage_number(27));
    sage_define_slot(&sage_global_OP_BIT_OR_38, sage_number(28));
    sage_define_slot(&sage_global_OP_BIT_XOR_39, sage_number(29));
    sage_define_slot(&sage_global_OP_BIT_NOT_40, sage_number(30));
    sage_define_slot(&sage_global_OP_SHIFT_LEFT_41, sage_number(31));
    sage_define_slot(&sage_global_OP_SHIFT_RIGHT_42, sage_number(32));
    sage_define_slot(&sage_global_OP_NOT_43, sage_number(33));
    sage_define_slot(&sage_global_OP_TRUTHY_44, sage_number(34));
    sage_define_slot(&sage_global_OP_JUMP_45, sage_number(35));
    sage_define_slot(&sage_global_OP_JUMP_IF_FALSE_46, sage_number(36));
    sage_define_slot(&sage_global_OP_CALL_47, sage_number(37));
    sage_define_slot(&sage_global_OP_CALL_METHOD_48, sage_number(38));
    sage_define_slot(&sage_global_OP_ARRAY_49, sage_number(39));
    sage_define_slot(&sage_global_OP_TUPLE_50, sage_number(40));
    sage_define_slot(&sage_global_OP_DICT_51, sage_number(41));
    sage_define_slot(&sage_global_OP_PRINT_52, sage_number(42));
    sage_define_slot(&sage_global_OP_EXEC_AST_STMT_53, sage_number(43));
    sage_define_slot(&sage_global_OP_RETURN_54, sage_number(44));
    sage_define_slot(&sage_global_OP_PUSH_ENV_55, sage_number(45));
    sage_define_slot(&sage_global_OP_POP_ENV_56, sage_number(46));
    sage_define_slot(&sage_global_OP_DUP_57, sage_number(47));
    sage_define_slot(&sage_global_OP_ARRAY_LEN_58, sage_number(48));
    sage_define_slot(&sage_global_OP_BREAK_59, sage_number(49));
    sage_define_slot(&sage_global_OP_CONTINUE_60, sage_number(50));
    sage_define_slot(&sage_global_OP_LOOP_BACK_61, sage_number(51));
    sage_define_slot(&sage_global_OP_IMPORT_62, sage_number(52));
    sage_define_slot(&sage_global_OP_CLASS_63, sage_number(53));
    sage_define_slot(&sage_global_OP_METHOD_64, sage_number(54));
    sage_define_slot(&sage_global_OP_INHERIT_65, sage_number(55));
    sage_define_slot(&sage_global_OP_SETUP_TRY_66, sage_number(56));
    sage_define_slot(&sage_global_OP_END_TRY_67, sage_number(57));
    sage_define_slot(&sage_global_OP_RAISE_68, sage_number(58));
    sage_define_slot(&sage_global_OP_GPU_POLL_EVENTS_69, sage_number(59));
    sage_define_slot(&sage_global_OP_GPU_WINDOW_SHOULD_CLOSE_70, sage_number(60));
    sage_define_slot(&sage_global_OP_GPU_GET_TIME_71, sage_number(61));
    sage_define_slot(&sage_global_OP_GPU_KEY_PRESSED_72, sage_number(62));
    sage_define_slot(&sage_global_OP_GPU_KEY_DOWN_73, sage_number(63));
    sage_define_slot(&sage_global_OP_GPU_MOUSE_POS_74, sage_number(64));
    sage_define_slot(&sage_global_OP_GPU_MOUSE_DELTA_75, sage_number(65));
    sage_define_slot(&sage_global_OP_GPU_UPDATE_INPUT_76, sage_number(66));
    sage_define_slot(&sage_global_OP_GPU_BEGIN_COMMANDS_77, sage_number(67));
    sage_define_slot(&sage_global_OP_GPU_END_COMMANDS_78, sage_number(68));
    sage_define_slot(&sage_global_OP_GPU_CMD_BEGIN_RP_79, sage_number(69));
    sage_define_slot(&sage_global_OP_GPU_CMD_END_RP_80, sage_number(70));
    sage_define_slot(&sage_global_OP_GPU_CMD_DRAW_81, sage_number(71));
    sage_define_slot(&sage_global_OP_GPU_CMD_BIND_GP_82, sage_number(72));
    sage_define_slot(&sage_global_OP_GPU_CMD_BIND_DS_83, sage_number(73));
    sage_define_slot(&sage_global_OP_GPU_CMD_SET_VP_84, sage_number(74));
    sage_define_slot(&sage_global_OP_GPU_CMD_SET_SC_85, sage_number(75));
    sage_define_slot(&sage_global_OP_GPU_CMD_BIND_VB_86, sage_number(76));
    sage_define_slot(&sage_global_OP_GPU_CMD_BIND_IB_87, sage_number(77));
    sage_define_slot(&sage_global_OP_GPU_CMD_DRAW_IDX_88, sage_number(78));
    sage_define_slot(&sage_global_OP_GPU_SUBMIT_SYNC_89, sage_number(79));
    sage_define_slot(&sage_global_OP_GPU_ACQUIRE_IMG_90, sage_number(80));
    sage_define_slot(&sage_global_OP_GPU_PRESENT_91, sage_number(81));
    sage_define_slot(&sage_global_OP_GPU_WAIT_FENCE_92, sage_number(82));
    sage_define_slot(&sage_global_OP_GPU_RESET_FENCE_93, sage_number(83));
    sage_define_slot(&sage_global_OP_GPU_UPDATE_UNIFORM_94, sage_number(84));
    sage_define_slot(&sage_global_OP_GPU_CMD_PUSH_CONST_95, sage_number(85));
    sage_define_slot(&sage_global_OP_GPU_CMD_DISPATCH_96, sage_number(86));
    sage_define_slot(&sage_global_OP_HALT_97, sage_number(255));
    sage_define_slot(&sage_global_sgvm_core_9, sage_init_native_module("sgvm_core"));
    sage_define_slot(&sage_global_OP_CONSTANT_10, sage_number(0));
    sage_define_slot(&sage_global_OP_NIL_11, sage_number(1));
    sage_define_slot(&sage_global_OP_TRUE_12, sage_number(2));
    sage_define_slot(&sage_global_OP_FALSE_13, sage_number(3));
    sage_define_slot(&sage_global_OP_POP_14, sage_number(4));
    sage_define_slot(&sage_global_OP_GET_GLOBAL_15, sage_number(5));
    sage_define_slot(&sage_global_OP_DEFINE_GLOBAL_16, sage_number(6));
    sage_define_slot(&sage_global_OP_SET_GLOBAL_17, sage_number(7));
    sage_define_slot(&sage_global_OP_DEFINE_FUNCTION_18, sage_number(8));
    sage_define_slot(&sage_global_OP_GET_PROPERTY_19, sage_number(9));
    sage_define_slot(&sage_global_OP_SET_PROPERTY_20, sage_number(10));
    sage_define_slot(&sage_global_OP_GET_INDEX_21, sage_number(11));
    sage_define_slot(&sage_global_OP_SET_INDEX_22, sage_number(12));
    sage_define_slot(&sage_global_OP_LOAD_FUNCTION_23, sage_number(13));
    sage_define_slot(&sage_global_OP_SLICE_24, sage_number(14));
    sage_define_slot(&sage_global_OP_ADD_25, sage_number(15));
    sage_define_slot(&sage_global_OP_SUB_26, sage_number(16));
    sage_define_slot(&sage_global_OP_MUL_27, sage_number(17));
    sage_define_slot(&sage_global_OP_DIV_28, sage_number(18));
    sage_define_slot(&sage_global_OP_MOD_29, sage_number(19));
    sage_define_slot(&sage_global_OP_NEGATE_30, sage_number(20));
    sage_define_slot(&sage_global_OP_EQUAL_31, sage_number(21));
    sage_define_slot(&sage_global_OP_NOT_EQUAL_32, sage_number(22));
    sage_define_slot(&sage_global_OP_GREATER_33, sage_number(23));
    sage_define_slot(&sage_global_OP_GREATER_EQUAL_34, sage_number(24));
    sage_define_slot(&sage_global_OP_LESS_35, sage_number(25));
    sage_define_slot(&sage_global_OP_LESS_EQUAL_36, sage_number(26));
    sage_define_slot(&sage_global_OP_BIT_AND_37, sage_number(27));
    sage_define_slot(&sage_global_OP_BIT_OR_38, sage_number(28));
    sage_define_slot(&sage_global_OP_BIT_XOR_39, sage_number(29));
    sage_define_slot(&sage_global_OP_BIT_NOT_40, sage_number(30));
    sage_define_slot(&sage_global_OP_SHIFT_LEFT_41, sage_number(31));
    sage_define_slot(&sage_global_OP_SHIFT_RIGHT_42, sage_number(32));
    sage_define_slot(&sage_global_OP_NOT_43, sage_number(33));
    sage_define_slot(&sage_global_OP_TRUTHY_44, sage_number(34));
    sage_define_slot(&sage_global_OP_JUMP_45, sage_number(35));
    sage_define_slot(&sage_global_OP_JUMP_IF_FALSE_46, sage_number(36));
    sage_define_slot(&sage_global_OP_CALL_47, sage_number(37));
    sage_define_slot(&sage_global_OP_CALL_METHOD_48, sage_number(38));
    sage_define_slot(&sage_global_OP_ARRAY_49, sage_number(39));
    sage_define_slot(&sage_global_OP_TUPLE_50, sage_number(40));
    sage_define_slot(&sage_global_OP_DICT_51, sage_number(41));
    sage_define_slot(&sage_global_OP_PRINT_52, sage_number(42));
    sage_define_slot(&sage_global_OP_EXEC_AST_STMT_53, sage_number(43));
    sage_define_slot(&sage_global_OP_RETURN_54, sage_number(44));
    sage_define_slot(&sage_global_OP_PUSH_ENV_55, sage_number(45));
    sage_define_slot(&sage_global_OP_POP_ENV_56, sage_number(46));
    sage_define_slot(&sage_global_OP_DUP_57, sage_number(47));
    sage_define_slot(&sage_global_OP_ARRAY_LEN_58, sage_number(48));
    sage_define_slot(&sage_global_OP_BREAK_59, sage_number(49));
    sage_define_slot(&sage_global_OP_CONTINUE_60, sage_number(50));
    sage_define_slot(&sage_global_OP_LOOP_BACK_61, sage_number(51));
    sage_define_slot(&sage_global_OP_IMPORT_62, sage_number(52));
    sage_define_slot(&sage_global_OP_CLASS_63, sage_number(53));
    sage_define_slot(&sage_global_OP_METHOD_64, sage_number(54));
    sage_define_slot(&sage_global_OP_INHERIT_65, sage_number(55));
    sage_define_slot(&sage_global_OP_SETUP_TRY_66, sage_number(56));
    sage_define_slot(&sage_global_OP_END_TRY_67, sage_number(57));
    sage_define_slot(&sage_global_OP_RAISE_68, sage_number(58));
    sage_define_slot(&sage_global_OP_GPU_POLL_EVENTS_69, sage_number(59));
    sage_define_slot(&sage_global_OP_GPU_WINDOW_SHOULD_CLOSE_70, sage_number(60));
    sage_define_slot(&sage_global_OP_GPU_GET_TIME_71, sage_number(61));
    sage_define_slot(&sage_global_OP_GPU_KEY_PRESSED_72, sage_number(62));
    sage_define_slot(&sage_global_OP_GPU_KEY_DOWN_73, sage_number(63));
    sage_define_slot(&sage_global_OP_GPU_MOUSE_POS_74, sage_number(64));
    sage_define_slot(&sage_global_OP_GPU_MOUSE_DELTA_75, sage_number(65));
    sage_define_slot(&sage_global_OP_GPU_UPDATE_INPUT_76, sage_number(66));
    sage_define_slot(&sage_global_OP_GPU_BEGIN_COMMANDS_77, sage_number(67));
    sage_define_slot(&sage_global_OP_GPU_END_COMMANDS_78, sage_number(68));
    sage_define_slot(&sage_global_OP_GPU_CMD_BEGIN_RP_79, sage_number(69));
    sage_define_slot(&sage_global_OP_GPU_CMD_END_RP_80, sage_number(70));
    sage_define_slot(&sage_global_OP_GPU_CMD_DRAW_81, sage_number(71));
    sage_define_slot(&sage_global_OP_GPU_CMD_BIND_GP_82, sage_number(72));
    sage_define_slot(&sage_global_OP_GPU_CMD_BIND_DS_83, sage_number(73));
    sage_define_slot(&sage_global_OP_GPU_CMD_SET_VP_84, sage_number(74));
    sage_define_slot(&sage_global_OP_GPU_CMD_SET_SC_85, sage_number(75));
    sage_define_slot(&sage_global_OP_GPU_CMD_BIND_VB_86, sage_number(76));
    sage_define_slot(&sage_global_OP_GPU_CMD_BIND_IB_87, sage_number(77));
    sage_define_slot(&sage_global_OP_GPU_CMD_DRAW_IDX_88, sage_number(78));
    sage_define_slot(&sage_global_OP_GPU_SUBMIT_SYNC_89, sage_number(79));
    sage_define_slot(&sage_global_OP_GPU_ACQUIRE_IMG_90, sage_number(80));
    sage_define_slot(&sage_global_OP_GPU_PRESENT_91, sage_number(81));
    sage_define_slot(&sage_global_OP_GPU_WAIT_FENCE_92, sage_number(82));
    sage_define_slot(&sage_global_OP_GPU_RESET_FENCE_93, sage_number(83));
    sage_define_slot(&sage_global_OP_GPU_UPDATE_UNIFORM_94, sage_number(84));
    sage_define_slot(&sage_global_OP_GPU_CMD_PUSH_CONST_95, sage_number(85));
    sage_define_slot(&sage_global_OP_GPU_CMD_DISPATCH_96, sage_number(86));
    sage_define_slot(&sage_global_OP_HALT_97, sage_number(255));
    sage_define_slot(&sage_global_sgvm_core_9, sage_init_native_module("sgvm_core"));
    sage_define_slot(&sage_global_OP_CONSTANT_10, sage_number(0));
    sage_define_slot(&sage_global_OP_NIL_11, sage_number(1));
    sage_define_slot(&sage_global_OP_TRUE_12, sage_number(2));
    sage_define_slot(&sage_global_OP_FALSE_13, sage_number(3));
    sage_define_slot(&sage_global_OP_POP_14, sage_number(4));
    sage_define_slot(&sage_global_OP_GET_GLOBAL_15, sage_number(5));
    sage_define_slot(&sage_global_OP_DEFINE_GLOBAL_16, sage_number(6));
    sage_define_slot(&sage_global_OP_SET_GLOBAL_17, sage_number(7));
    sage_define_slot(&sage_global_OP_DEFINE_FUNCTION_18, sage_number(8));
    sage_define_slot(&sage_global_OP_GET_PROPERTY_19, sage_number(9));
    sage_define_slot(&sage_global_OP_SET_PROPERTY_20, sage_number(10));
    sage_define_slot(&sage_global_OP_GET_INDEX_21, sage_number(11));
    sage_define_slot(&sage_global_OP_SET_INDEX_22, sage_number(12));
    sage_define_slot(&sage_global_OP_LOAD_FUNCTION_23, sage_number(13));
    sage_define_slot(&sage_global_OP_SLICE_24, sage_number(14));
    sage_define_slot(&sage_global_OP_ADD_25, sage_number(15));
    sage_define_slot(&sage_global_OP_SUB_26, sage_number(16));
    sage_define_slot(&sage_global_OP_MUL_27, sage_number(17));
    sage_define_slot(&sage_global_OP_DIV_28, sage_number(18));
    sage_define_slot(&sage_global_OP_MOD_29, sage_number(19));
    sage_define_slot(&sage_global_OP_NEGATE_30, sage_number(20));
    sage_define_slot(&sage_global_OP_EQUAL_31, sage_number(21));
    sage_define_slot(&sage_global_OP_NOT_EQUAL_32, sage_number(22));
    sage_define_slot(&sage_global_OP_GREATER_33, sage_number(23));
    sage_define_slot(&sage_global_OP_GREATER_EQUAL_34, sage_number(24));
    sage_define_slot(&sage_global_OP_LESS_35, sage_number(25));
    sage_define_slot(&sage_global_OP_LESS_EQUAL_36, sage_number(26));
    sage_define_slot(&sage_global_OP_BIT_AND_37, sage_number(27));
    sage_define_slot(&sage_global_OP_BIT_OR_38, sage_number(28));
    sage_define_slot(&sage_global_OP_BIT_XOR_39, sage_number(29));
    sage_define_slot(&sage_global_OP_BIT_NOT_40, sage_number(30));
    sage_define_slot(&sage_global_OP_SHIFT_LEFT_41, sage_number(31));
    sage_define_slot(&sage_global_OP_SHIFT_RIGHT_42, sage_number(32));
    sage_define_slot(&sage_global_OP_NOT_43, sage_number(33));
    sage_define_slot(&sage_global_OP_TRUTHY_44, sage_number(34));
    sage_define_slot(&sage_global_OP_JUMP_45, sage_number(35));
    sage_define_slot(&sage_global_OP_JUMP_IF_FALSE_46, sage_number(36));
    sage_define_slot(&sage_global_OP_CALL_47, sage_number(37));
    sage_define_slot(&sage_global_OP_CALL_METHOD_48, sage_number(38));
    sage_define_slot(&sage_global_OP_ARRAY_49, sage_number(39));
    sage_define_slot(&sage_global_OP_TUPLE_50, sage_number(40));
    sage_define_slot(&sage_global_OP_DICT_51, sage_number(41));
    sage_define_slot(&sage_global_OP_PRINT_52, sage_number(42));
    sage_define_slot(&sage_global_OP_EXEC_AST_STMT_53, sage_number(43));
    sage_define_slot(&sage_global_OP_RETURN_54, sage_number(44));
    sage_define_slot(&sage_global_OP_PUSH_ENV_55, sage_number(45));
    sage_define_slot(&sage_global_OP_POP_ENV_56, sage_number(46));
    sage_define_slot(&sage_global_OP_DUP_57, sage_number(47));
    sage_define_slot(&sage_global_OP_ARRAY_LEN_58, sage_number(48));
    sage_define_slot(&sage_global_OP_BREAK_59, sage_number(49));
    sage_define_slot(&sage_global_OP_CONTINUE_60, sage_number(50));
    sage_define_slot(&sage_global_OP_LOOP_BACK_61, sage_number(51));
    sage_define_slot(&sage_global_OP_IMPORT_62, sage_number(52));
    sage_define_slot(&sage_global_OP_CLASS_63, sage_number(53));
    sage_define_slot(&sage_global_OP_METHOD_64, sage_number(54));
    sage_define_slot(&sage_global_OP_INHERIT_65, sage_number(55));
    sage_define_slot(&sage_global_OP_SETUP_TRY_66, sage_number(56));
    sage_define_slot(&sage_global_OP_END_TRY_67, sage_number(57));
    sage_define_slot(&sage_global_OP_RAISE_68, sage_number(58));
    sage_define_slot(&sage_global_OP_GPU_POLL_EVENTS_69, sage_number(59));
    sage_define_slot(&sage_global_OP_GPU_WINDOW_SHOULD_CLOSE_70, sage_number(60));
    sage_define_slot(&sage_global_OP_GPU_GET_TIME_71, sage_number(61));
    sage_define_slot(&sage_global_OP_GPU_KEY_PRESSED_72, sage_number(62));
    sage_define_slot(&sage_global_OP_GPU_KEY_DOWN_73, sage_number(63));
    sage_define_slot(&sage_global_OP_GPU_MOUSE_POS_74, sage_number(64));
    sage_define_slot(&sage_global_OP_GPU_MOUSE_DELTA_75, sage_number(65));
    sage_define_slot(&sage_global_OP_GPU_UPDATE_INPUT_76, sage_number(66));
    sage_define_slot(&sage_global_OP_GPU_BEGIN_COMMANDS_77, sage_number(67));
    sage_define_slot(&sage_global_OP_GPU_END_COMMANDS_78, sage_number(68));
    sage_define_slot(&sage_global_OP_GPU_CMD_BEGIN_RP_79, sage_number(69));
    sage_define_slot(&sage_global_OP_GPU_CMD_END_RP_80, sage_number(70));
    sage_define_slot(&sage_global_OP_GPU_CMD_DRAW_81, sage_number(71));
    sage_define_slot(&sage_global_OP_GPU_CMD_BIND_GP_82, sage_number(72));
    sage_define_slot(&sage_global_OP_GPU_CMD_BIND_DS_83, sage_number(73));
    sage_define_slot(&sage_global_OP_GPU_CMD_SET_VP_84, sage_number(74));
    sage_define_slot(&sage_global_OP_GPU_CMD_SET_SC_85, sage_number(75));
    sage_define_slot(&sage_global_OP_GPU_CMD_BIND_VB_86, sage_number(76));
    sage_define_slot(&sage_global_OP_GPU_CMD_BIND_IB_87, sage_number(77));
    sage_define_slot(&sage_global_OP_GPU_CMD_DRAW_IDX_88, sage_number(78));
    sage_define_slot(&sage_global_OP_GPU_SUBMIT_SYNC_89, sage_number(79));
    sage_define_slot(&sage_global_OP_GPU_ACQUIRE_IMG_90, sage_number(80));
    sage_define_slot(&sage_global_OP_GPU_PRESENT_91, sage_number(81));
    sage_define_slot(&sage_global_OP_GPU_WAIT_FENCE_92, sage_number(82));
    sage_define_slot(&sage_global_OP_GPU_RESET_FENCE_93, sage_number(83));
    sage_define_slot(&sage_global_OP_GPU_UPDATE_UNIFORM_94, sage_number(84));
    sage_define_slot(&sage_global_OP_GPU_CMD_PUSH_CONST_95, sage_number(85));
    sage_define_slot(&sage_global_OP_GPU_CMD_DISPATCH_96, sage_number(86));
    sage_define_slot(&sage_global_OP_HALT_97, sage_number(255));
    sage_define_slot(&sage_global_g_gil_98, sage_native_thread_mutex());
    sage_define_slot(&sage_global_sgvm_core_9, sage_init_native_module("sgvm_core"));
    sage_define_slot(&sage_global_OP_CONSTANT_10, sage_number(0));
    sage_define_slot(&sage_global_OP_NIL_11, sage_number(1));
    sage_define_slot(&sage_global_OP_TRUE_12, sage_number(2));
    sage_define_slot(&sage_global_OP_FALSE_13, sage_number(3));
    sage_define_slot(&sage_global_OP_POP_14, sage_number(4));
    sage_define_slot(&sage_global_OP_GET_GLOBAL_15, sage_number(5));
    sage_define_slot(&sage_global_OP_DEFINE_GLOBAL_16, sage_number(6));
    sage_define_slot(&sage_global_OP_SET_GLOBAL_17, sage_number(7));
    sage_define_slot(&sage_global_OP_DEFINE_FUNCTION_18, sage_number(8));
    sage_define_slot(&sage_global_OP_GET_PROPERTY_19, sage_number(9));
    sage_define_slot(&sage_global_OP_SET_PROPERTY_20, sage_number(10));
    sage_define_slot(&sage_global_OP_GET_INDEX_21, sage_number(11));
    sage_define_slot(&sage_global_OP_SET_INDEX_22, sage_number(12));
    sage_define_slot(&sage_global_OP_LOAD_FUNCTION_23, sage_number(13));
    sage_define_slot(&sage_global_OP_SLICE_24, sage_number(14));
    sage_define_slot(&sage_global_OP_ADD_25, sage_number(15));
    sage_define_slot(&sage_global_OP_SUB_26, sage_number(16));
    sage_define_slot(&sage_global_OP_MUL_27, sage_number(17));
    sage_define_slot(&sage_global_OP_DIV_28, sage_number(18));
    sage_define_slot(&sage_global_OP_MOD_29, sage_number(19));
    sage_define_slot(&sage_global_OP_NEGATE_30, sage_number(20));
    sage_define_slot(&sage_global_OP_EQUAL_31, sage_number(21));
    sage_define_slot(&sage_global_OP_NOT_EQUAL_32, sage_number(22));
    sage_define_slot(&sage_global_OP_GREATER_33, sage_number(23));
    sage_define_slot(&sage_global_OP_GREATER_EQUAL_34, sage_number(24));
    sage_define_slot(&sage_global_OP_LESS_35, sage_number(25));
    sage_define_slot(&sage_global_OP_LESS_EQUAL_36, sage_number(26));
    sage_define_slot(&sage_global_OP_BIT_AND_37, sage_number(27));
    sage_define_slot(&sage_global_OP_BIT_OR_38, sage_number(28));
    sage_define_slot(&sage_global_OP_BIT_XOR_39, sage_number(29));
    sage_define_slot(&sage_global_OP_BIT_NOT_40, sage_number(30));
    sage_define_slot(&sage_global_OP_SHIFT_LEFT_41, sage_number(31));
    sage_define_slot(&sage_global_OP_SHIFT_RIGHT_42, sage_number(32));
    sage_define_slot(&sage_global_OP_NOT_43, sage_number(33));
    sage_define_slot(&sage_global_OP_TRUTHY_44, sage_number(34));
    sage_define_slot(&sage_global_OP_JUMP_45, sage_number(35));
    sage_define_slot(&sage_global_OP_JUMP_IF_FALSE_46, sage_number(36));
    sage_define_slot(&sage_global_OP_CALL_47, sage_number(37));
    sage_define_slot(&sage_global_OP_CALL_METHOD_48, sage_number(38));
    sage_define_slot(&sage_global_OP_ARRAY_49, sage_number(39));
    sage_define_slot(&sage_global_OP_TUPLE_50, sage_number(40));
    sage_define_slot(&sage_global_OP_DICT_51, sage_number(41));
    sage_define_slot(&sage_global_OP_PRINT_52, sage_number(42));
    sage_define_slot(&sage_global_OP_EXEC_AST_STMT_53, sage_number(43));
    sage_define_slot(&sage_global_OP_RETURN_54, sage_number(44));
    sage_define_slot(&sage_global_OP_PUSH_ENV_55, sage_number(45));
    sage_define_slot(&sage_global_OP_POP_ENV_56, sage_number(46));
    sage_define_slot(&sage_global_OP_DUP_57, sage_number(47));
    sage_define_slot(&sage_global_OP_ARRAY_LEN_58, sage_number(48));
    sage_define_slot(&sage_global_OP_BREAK_59, sage_number(49));
    sage_define_slot(&sage_global_OP_CONTINUE_60, sage_number(50));
    sage_define_slot(&sage_global_OP_LOOP_BACK_61, sage_number(51));
    sage_define_slot(&sage_global_OP_IMPORT_62, sage_number(52));
    sage_define_slot(&sage_global_OP_CLASS_63, sage_number(53));
    sage_define_slot(&sage_global_OP_METHOD_64, sage_number(54));
    sage_define_slot(&sage_global_OP_INHERIT_65, sage_number(55));
    sage_define_slot(&sage_global_OP_SETUP_TRY_66, sage_number(56));
    sage_define_slot(&sage_global_OP_END_TRY_67, sage_number(57));
    sage_define_slot(&sage_global_OP_RAISE_68, sage_number(58));
    sage_define_slot(&sage_global_OP_GPU_POLL_EVENTS_69, sage_number(59));
    sage_define_slot(&sage_global_OP_GPU_WINDOW_SHOULD_CLOSE_70, sage_number(60));
    sage_define_slot(&sage_global_OP_GPU_GET_TIME_71, sage_number(61));
    sage_define_slot(&sage_global_OP_GPU_KEY_PRESSED_72, sage_number(62));
    sage_define_slot(&sage_global_OP_GPU_KEY_DOWN_73, sage_number(63));
    sage_define_slot(&sage_global_OP_GPU_MOUSE_POS_74, sage_number(64));
    sage_define_slot(&sage_global_OP_GPU_MOUSE_DELTA_75, sage_number(65));
    sage_define_slot(&sage_global_OP_GPU_UPDATE_INPUT_76, sage_number(66));
    sage_define_slot(&sage_global_OP_GPU_BEGIN_COMMANDS_77, sage_number(67));
    sage_define_slot(&sage_global_OP_GPU_END_COMMANDS_78, sage_number(68));
    sage_define_slot(&sage_global_OP_GPU_CMD_BEGIN_RP_79, sage_number(69));
    sage_define_slot(&sage_global_OP_GPU_CMD_END_RP_80, sage_number(70));
    sage_define_slot(&sage_global_OP_GPU_CMD_DRAW_81, sage_number(71));
    sage_define_slot(&sage_global_OP_GPU_CMD_BIND_GP_82, sage_number(72));
    sage_define_slot(&sage_global_OP_GPU_CMD_BIND_DS_83, sage_number(73));
    sage_define_slot(&sage_global_OP_GPU_CMD_SET_VP_84, sage_number(74));
    sage_define_slot(&sage_global_OP_GPU_CMD_SET_SC_85, sage_number(75));
    sage_define_slot(&sage_global_OP_GPU_CMD_BIND_VB_86, sage_number(76));
    sage_define_slot(&sage_global_OP_GPU_CMD_BIND_IB_87, sage_number(77));
    sage_define_slot(&sage_global_OP_GPU_CMD_DRAW_IDX_88, sage_number(78));
    sage_define_slot(&sage_global_OP_GPU_SUBMIT_SYNC_89, sage_number(79));
    sage_define_slot(&sage_global_OP_GPU_ACQUIRE_IMG_90, sage_number(80));
    sage_define_slot(&sage_global_OP_GPU_PRESENT_91, sage_number(81));
    sage_define_slot(&sage_global_OP_GPU_WAIT_FENCE_92, sage_number(82));
    sage_define_slot(&sage_global_OP_GPU_RESET_FENCE_93, sage_number(83));
    sage_define_slot(&sage_global_OP_GPU_UPDATE_UNIFORM_94, sage_number(84));
    sage_define_slot(&sage_global_OP_GPU_CMD_PUSH_CONST_95, sage_number(85));
    sage_define_slot(&sage_global_OP_GPU_CMD_DISPATCH_96, sage_number(86));
    sage_define_slot(&sage_global_OP_HALT_97, sage_number(255));
    (void)sage_fn_main_99();
    sage_gc_pop_frame(&sage_gc_main_frame);
    sage_gc_shutdown();
    return 0;
}
