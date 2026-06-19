import sys

with open('SageLang/core/src/c/metal_vm.c', 'r') as f:
    code = f.read()

# 1. Insert header declarations at the top
header = (
    "#include <stdint.h>\n"
    "extern volatile unsigned long g_trap_cause;\n"
    "extern volatile unsigned long g_trap_epc;\n"
    "extern volatile int g_trap_pending;\n"
    "void enable_interrupts();\n"
    "void set_timer(unsigned long interval);\n"
    "void cpu_halt();\n"
)
code = header + code

# 2. Patch metal_vm_verify switch cases for 2-byte constant indices
target_const_op = """                case OP_CONSTANT:
                case OP_GET_GLOBAL:
                case OP_DEFINE_GLOBAL:
                case OP_SET_GLOBAL: {"""
replacement_const_op = """                case OP_CONSTANT:
                case OP_GET_GLOBAL:
                case OP_DEFINE_GLOBAL:
                case OP_SET_GLOBAL:
                case OP_GET_PROPERTY:
                case OP_SET_PROPERTY:
                case OP_CLASS:
                case OP_METHOD:
                case OP_IMPORT:
                case OP_EXEC_AST_STMT: {"""

if target_const_op in code:
    code = code.replace(target_const_op, replacement_const_op)
else:
    print("Warning: Could not find constant opcodes in verification to patch")

# 3. Patch metal_vm_verify switch cases for method calls, tuples, dicts, tries
target_call_op = """                case OP_CALL: {
                    if (ip + 1 > code_length) return -1;
                    ip += 1;
                    break;
                }"""
replacement_call_op = """                case OP_CALL: {
                    if (ip + 1 > code_length) return -1;
                    ip += 1;
                    break;
                }
                case OP_CALL_METHOD: {
                    if (ip + 3 > code_length) return -1;
                    int idx = (code[ip] << 8) | code[ip + 1];
                    if (idx >= vm->const_count) {
                        metal_print_str(vm, "VERIFY ERROR call_method: op=");
                        metal_print_int(vm, op);
                        metal_print_str(vm, " idx=");
                        metal_print_int(vm, idx);
                        metal_print_str(vm, " const_count=");
                        metal_print_int(vm, vm->const_count);
                        metal_print_str(vm, "\\n");
                        return -2;
                    }
                    ip += 3;
                    break;
                }
                case OP_TUPLE:
                case OP_DICT:
                case OP_LOAD_FUNCTION: {
                    if (ip + 2 > code_length) return -1;
                    ip += 2;
                    break;
                }
                case OP_SETUP_TRY: {
                    if (ip + 2 > code_length) return -1;
                    int target = (code[ip] << 8) | code[ip + 1];
                    if (target >= code_length) return -3;
                    ip += 2;
                    break;
                }"""

if target_call_op in code:
    code = code.replace(target_call_op, replacement_call_op)
else:
    print("Warning: Could not find OP_CALL in verification to patch")

# 4. Patch metal_vm_verify switch cases for 0-byte helper opcodes (slice, inherit, end_try, raise)
target_break_op = """                case OP_BREAK:
                case OP_CONTINUE:"""
replacement_break_op = """                case OP_BREAK:
                case OP_CONTINUE:
                case OP_SLICE:
                case OP_INHERIT:
                case OP_END_TRY:
                case OP_RAISE:"""

if target_break_op in code:
    code = code.replace(target_break_op, replacement_break_op)
else:
    print("Warning: Could not find OP_BREAK/CONTINUE in verification to patch")

# 5. Patch constant index verification check to output debug info
target_verify_check = "                    if (idx >= vm->const_count) return -2;"
replacement_verify_check = """                    if (idx >= vm->const_count) {
                        metal_print_str(vm, "VERIFY ERROR const: op=");
                        metal_print_int(vm, op);
                        metal_print_str(vm, " idx=");
                        metal_print_int(vm, idx);
                        metal_print_str(vm, " const_count=");
                        metal_print_int(vm, vm->const_count);
                        metal_print_str(vm, "\\n");
                        return -2;
                    }"""

if target_verify_check in code:
    code = code.replace(target_verify_check, replacement_verify_check)
else:
    print("Warning: Could not find constant check in verification to patch")

# 6. Patch GPU/custom opcodes in verification
op_array_len_target = "                case OP_ARRAY_LEN:"
op_array_len_replacement = """                case OP_GPU_POLL_EVENTS:
                case OP_GPU_WINDOW_SHOULD_CLOSE:
                case OP_GPU_GET_TIME:
                case OP_GPU_KEY_PRESSED:
                case OP_GPU_KEY_DOWN:
                case OP_GPU_MOUSE_POS:
                case OP_GPU_MOUSE_DELTA:
                case OP_GPU_UPDATE_INPUT:
                case OP_GPU_BEGIN_COMMANDS:
                case OP_GPU_END_COMMANDS:
                case OP_GPU_CMD_BEGIN_RP:
                case OP_GPU_CMD_END_RP:
                case OP_GPU_CMD_DRAW:
                case OP_GPU_CMD_BIND_GP:
                case OP_GPU_CMD_BIND_DS:
                case OP_GPU_CMD_SET_VP:
                case OP_GPU_CMD_SET_SC:
                case OP_GPU_CMD_BIND_VB:
                case OP_GPU_CMD_BIND_IB:
                case OP_GPU_CMD_DRAW_IDX:
                case OP_GPU_SUBMIT_SYNC:
                case OP_GPU_ACQUIRE_IMG:
                case OP_GPU_PRESENT:
                case OP_GPU_WAIT_FENCE:
                case OP_GPU_RESET_FENCE:
                case OP_GPU_UPDATE_UNIFORM:
                case OP_GPU_CMD_PUSH_CONST:
                case OP_GPU_CMD_DISPATCH:
                case 200:
                case 201:
                case 202:
                case 203:
                case 204:
                case 205:
                case 206:
                    break;
                case OP_ARRAY_LEN:"""

if op_array_len_target in code:
    code = code.replace(op_array_len_target, op_array_len_replacement)
else:
    print("Warning: Could not find OP_ARRAY_LEN in verification to patch")

# 7. Patch OP_PRINT case in metal_vm_step
op_truthy_target = """        case OP_TRUTHY: {
            MetalValue a = metal_vm_pop(vm);
            metal_vm_push(vm, mv_bool(metal_truthy(a)));
            break;
        }"""
op_truthy_replacement = """        case OP_TRUTHY: {
            MetalValue a = metal_vm_pop(vm);
            metal_vm_push(vm, mv_bool(metal_truthy(a)));
            break;
        }

        case OP_PRINT: {
            MetalValue val = metal_vm_pop(vm);
            metal_print_value(vm, val);
            metal_print_str(vm, "\\n");
            break;
        }"""

if op_truthy_target in code:
    code = code.replace(op_truthy_target, op_truthy_replacement)
else:
    code = code.replace("case OP_TRUTHY:", "case OP_PRINT: { MetalValue val = metal_vm_pop(vm); metal_print_value(vm, val); metal_print_str(vm, \"\\n\"); break; }\n        case OP_TRUTHY:")
    print("Patched OP_PRINT via fallback")

# 8. Patch custom opcodes (200-206) in metal_vm_step
dispatch_target = """        case OP_GPU_CMD_DISPATCH:
            // GPU opcodes require host implementation (sgpu_*)
            break;"""

dispatch_replacement = """        case OP_GPU_CMD_DISPATCH:
            // GPU opcodes require host implementation (sgpu_*)
            break;

        case 200: { metal_print_str(vm, "HALT\\n"); while(1) { cpu_halt(); } break; } /* HALT */
        case 201: { if(g_trap_pending) { metal_vm_push(vm, mv_num((double)g_trap_cause)); metal_vm_push(vm, mv_num((double)g_trap_epc)); g_trap_pending = 0; } else { metal_vm_push(vm, mv_num(-1)); metal_vm_push(vm, mv_num(0)); } break; } /* GET_TRAP */
        case 202: { enable_interrupts(); break; } /* ENABLE_INTERRUPTS */
        case 203: { MetalValue val = metal_vm_pop(vm); set_timer((unsigned long)val.as.number); break; } /* SET_TIMER */
        case 204: { MetalValue addr = metal_vm_pop(vm); uint64_t val = *(uint64_t*)((uintptr_t)addr.as.number); metal_vm_push(vm, mv_num((double)val)); break; } /* PEEK64 */
        case 205: { MetalValue val = metal_vm_pop(vm); MetalValue addr = metal_vm_pop(vm); *(uint64_t*)((uintptr_t)addr.as.number) = (uint64_t)val.as.number; break; } /* POKE64 */
        case 206: { metal_print_str(vm, "POKE32\\n"); MetalValue val = metal_vm_pop(vm); MetalValue addr = metal_vm_pop(vm); *(uint32_t*)((uintptr_t)addr.as.number) = (uint32_t)val.as.number; break; } /* POKE32 */"""

if dispatch_target in code:
    code = code.replace(dispatch_target, dispatch_replacement)
else:
    print("Warning: Could not find OP_GPU_CMD_DISPATCH to patch")

with open('build/metal_vm_patched.c', 'w') as f:
    f.write(code)

print("Successfully patched metal_vm.c into build/metal_vm_patched.c")
