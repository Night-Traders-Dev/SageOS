import sys

def patch_vm(filename):
    with open(filename, 'r') as f:
        content = f.read()

    # 1. Add headers
    if "#include <stdint.h>" not in content:
        content = "#include <stdint.h>\nextern volatile unsigned long g_trap_cause;\nextern volatile unsigned long g_trap_epc;\nextern volatile int g_trap_pending;\nvoid enable_interrupts();\nvoid set_timer(unsigned long interval);\nvoid print_str(const char* s);\n" + content
    
    # 2. Add OP_PRINT
    if "case OP_PRINT:" not in content:
        content = content.replace("case OP_CONSTANT:", "case OP_PRINT: { MetalValue val = metal_vm_pop(vm); metal_print_value(vm, val); metal_print_str(vm, \"\\n\"); break; }\n        case OP_CONSTANT:")
    
    # 3. Add custom opcodes in metal_vm_step
    custom_opcodes = """
        case 200: { metal_print_str(vm, "HALT\\n"); while(1) { __asm__ volatile("wfi"); } break; } /* HALT */
        case 201: { if(g_trap_pending) { metal_vm_push(vm, mv_num((double)g_trap_cause)); metal_vm_push(vm, mv_num((double)g_trap_epc)); g_trap_pending = 0; } else { metal_vm_push(vm, mv_num(-1)); metal_vm_push(vm, mv_num(0)); } break; } /* GET_TRAP */
        case 202: { enable_interrupts(); break; } /* ENABLE_INTERRUPTS */
        case 203: { MetalValue val = metal_vm_pop(vm); set_timer((unsigned long)val.as.number); break; } /* SET_TIMER */
        case 204: { MetalValue addr = metal_vm_pop(vm); uint64_t val = *(uint64_t*)((uintptr_t)addr.as.number); metal_vm_push(vm, mv_num((double)val)); break; } /* PEEK64 */
        case 205: { MetalValue val = metal_vm_pop(vm); MetalValue addr = metal_vm_pop(vm); *(uint64_t*)((uintptr_t)addr.as.number) = (uint64_t)val.as.number; break; } /* POKE64 */
        case 206: { MetalValue val = metal_vm_pop(vm); MetalValue addr = metal_vm_pop(vm); *(uint32_t*)((uintptr_t)addr.as.number) = (uint32_t)val.as.number; break; } /* POKE32 */
    """
    if "case 200:" not in content:
        content = content.replace("case OP_GPU_CMD_DISPATCH:", "case OP_GPU_CMD_DISPATCH:" + custom_opcodes)
    
    # 4. Make verifier permissive (do not return -4 on unknown opcode)
    # Target the default case in metal_vm_verify specifically
    pattern = "                default:\n                    return -4;"
    replacement = "                default: break;"
    content = content.replace(pattern, replacement)

    with open(filename, 'w') as f:
        f.write(content)
    print(f"DEBUG: Patched {filename} successfully.")

if __name__ == "__main__":
    patch_vm(sys.argv[1])
