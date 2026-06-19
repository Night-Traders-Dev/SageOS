import sys

if len(sys.argv) < 2:
    src_path = 'SageLang/core/src/c/metal_vm.c'
else:
    src_path = sys.argv[1]

with open(src_path, 'r') as f:
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

if 'metal_rv64_vm' in src_path:
    # RV64I register VM patching
    # Define value helpers that are normally defined in metal_vm.c (at the end of the file)
    helpers = (
        "\nMetalValue mv_nil(void) { MetalValue v; v.type = MV_NIL; v.as.number = 0; return v; }\n"
        "MetalValue mv_num(double d) { MetalValue v; v.type = MV_NUM; v.as.number = d; return v; }\n"
        "MetalValue mv_bool(int b) { MetalValue v; v.type = MV_BOOL; v.as.boolean = b; return v; }\n"
    )
    code = code + helpers

    target_gpu_ops = """    } else if (inst.funct3 == RV_F3_GPU_OPS) {
        // Stubs for GPU operations
        if (vm->trace) {
            metal_rv64_print_str(vm, "GPU Op: ");
            metal_rv64_print_int(vm, sub_op);
            if (vm->write_char) vm->write_char('\\n');
        }
    }"""
    
    replacement_gpu_ops = """    } else if (inst.funct3 == RV_F3_GPU_OPS) {
        switch (sub_op) {
            case 0: { // HALT / GPU_POLL_EVENTS
                metal_rv64_print_str(vm, "HALT\\n");
                while(1) { cpu_halt(); }
                break;
            }
            case 1: { // GET_TRAP / GPU_CMD_BEGIN_RP
                if (g_trap_pending) {
                    vm->x[10] = mv_num((double)g_trap_cause);
                    vm->x[11] = mv_num((double)g_trap_epc);
                    g_trap_pending = 0;
                } else {
                    vm->x[10] = mv_num(-1);
                    vm->x[11] = mv_num(0);
                }
                break;
            }
            case 2: { // ENABLE_INTERRUPTS / GPU_CMD_END_RP
                enable_interrupts();
                break;
            }
            case 3: { // SET_TIMER / GPU_CMD_DRAW
                MetalValue val = vm->x[10];
                set_timer((unsigned long)val.as.number);
                break;
            }
            case 4: { // PEEK64 / GPU_KEY_PRESSED
                MetalValue addr = vm->x[10];
                uint64_t val = *(uint64_t*)((uintptr_t)addr.as.number);
                vm->x[inst.rd] = mv_num((double)val);
                break;
            }
            case 5: { // POKE64 / GPU_UPDATE_UNIFORM
                MetalValue val = vm->x[11];
                MetalValue addr = vm->x[10];
                *(uint64_t*)((uintptr_t)addr.as.number) = (uint64_t)val.as.number;
                break;
            }
            case 6: { // POKE32 / GPU_CMD_PUSH_CONST
                MetalValue val = vm->x[11];
                MetalValue addr = vm->x[10];
                *(uint32_t*)((uintptr_t)addr.as.number) = (uint32_t)val.as.number;
                break;
            }
            default:
                break;
        }
    }"""
    if target_gpu_ops in code:
        code = code.replace(target_gpu_ops, replacement_gpu_ops)
    else:
        print("Warning: Could not find GPU Op stubs to patch in metal_rv64_vm.c")

    target_method_bind = """            case RV_OBJ_SET_GLOBAL: {
                int idx = 0;
                if (vm->x[10].type == MV_NUM) idx = (int)vm->x[10].as.number;
                int name_str_idx = vm->constants[idx].as.str_idx;
                MetalValue val = vm->x[11]; // a1
                metal_rv64_dict_set(vm, vm->global_dict_idx, name_str_idx, val);
                break;
            }"""
    replacement_method_bind = """            case RV_OBJ_SET_GLOBAL: {
                int idx = 0;
                if (vm->x[10].type == MV_NUM) idx = (int)vm->x[10].as.number;
                int name_str_idx = vm->constants[idx].as.str_idx;
                MetalValue val = vm->x[11]; // a1
                metal_rv64_dict_set(vm, vm->global_dict_idx, name_str_idx, val);
                break;
            }
            case RV_OBJ_METHOD_BIND: {
                MetalValue obj = vm->x[inst.rs2];
                int name_idx = 0;
                if (vm->x[10].type == MV_NUM) name_idx = (int)vm->x[10].as.number;
                int name_str_idx = vm->constants[name_idx].as.str_idx;
                MetalValue method = mv_nil();
                int is_instance = 0;
                if (obj.type == MV_DICT) {
                    int methods_key = metal_rv64_string_intern(vm, "__methods__", 11);
                    MetalValue methods_val = metal_rv64_dict_get(vm, obj.as.dict_idx, methods_key);
                    if (methods_val.type == MV_DICT) {
                        method = metal_rv64_dict_get(vm, methods_val.as.dict_idx, name_str_idx);
                        is_instance = 0;
                    }
                    if (method.type == MV_NIL) {
                        int class_key = metal_rv64_string_intern(vm, "__class__", 9);
                        MetalValue class_val = metal_rv64_dict_get(vm, obj.as.dict_idx, class_key);
                        if (class_val.type == MV_DICT) {
                            MetalValue class_methods_val = metal_rv64_dict_get(vm, class_val.as.dict_idx, methods_key);
                            if (class_methods_val.type == MV_DICT) {
                                method = metal_rv64_dict_get(vm, class_methods_val.as.dict_idx, name_str_idx);
                                is_instance = 1;
                            }
                        }
                    }
                }
                vm->x[inst.rd] = method;
                vm->x[28] = mv_num((double)is_instance);
                break;
            }"""
    if target_method_bind in code:
        code = code.replace(target_method_bind, replacement_method_bind)
    else:
        print("Warning: Could not find RV_OBJ_SET_GLOBAL to patch")

    target_ldc = """    } else {
        vm->error = 1;
        vm->error_msg = "Constant pool access violation";
        vm->x[inst.rd] = mv_nil();
    }"""
    replacement_ldc = """    } else {
        metal_rv64_print_str(vm, "LDC Access Violation: idx=");
        metal_rv64_print_int(vm, idx);
        metal_rv64_print_str(vm, " const_count=");
        metal_rv64_print_int(vm, vm->const_count);
        metal_rv64_print_str(vm, "\\n");
        vm->error = 1;
        vm->error_msg = "Constant pool access violation";
        vm->x[inst.rd] = mv_nil();
    }"""
    if target_ldc in code:
        code = code.replace(target_ldc, replacement_ldc)
    else:
        print("Warning: Could not find target_ldc to patch")

    target_load = """    } else {
        vm->error = 1;
        vm->error_msg = "Load access violation";
        vm->x[inst.rd] = mv_nil();
    }"""
    replacement_load = """    } else {
        metal_rv64_print_str(vm, "Load Access Violation: addr=");
        metal_rv64_print_int(vm, addr);
        metal_rv64_print_str(vm, "\\n");
        vm->error = 1;
        vm->error_msg = "Load access violation";
        vm->x[inst.rd] = mv_nil();
    }"""
    if target_load in code:
        code = code.replace(target_load, replacement_load)
    else:
        print("Warning: Could not find target_load to patch")

    target_store = """    } else {
        vm->error = 1;
        vm->error_msg = "Store access violation";
    }"""
    replacement_store = """    } else {
        metal_rv64_print_str(vm, "Store Access Violation: addr=");
        metal_rv64_print_int(vm, addr);
        metal_rv64_print_str(vm, "\\n");
        vm->error = 1;
        vm->error_msg = "Store access violation";
    }"""
    if target_store in code:
        code = code.replace(target_store, replacement_store)
    else:
        print("Warning: Could not find target_store to patch")
else:
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

print(f"Successfully patched {src_path} into build/metal_vm_patched.c")
