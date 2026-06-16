import srvm_core
from srvm_core import RVEncoder

proc gen():
    let enc = RVEncoder()
    # ADDI a0, x0, 1337 (x10 = 0 + 1337)
    let v1 = enc.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 10, 0, 1337)
    # ADDI a1, x0, 0 (x11 = 0 + 0)
    let v2 = enc.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 11, 0, 0)
    # SET_GLOBAL a1, a0 (heap[consts[x11]] = x10)
    let v3 = enc.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_OBJ_OPS, srvm_core.OBJ_SET_GLOBAL, 0, 11, 10)
    # ADDI a0, x0, 0 (clear a0)
    let v4 = enc.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 10, 0, 0)
    # GET_GLOBAL a2, a1 (x12 = heap[consts[x11]])
    let v5 = enc.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_OBJ_OPS, srvm_core.OBJ_GET_GLOBAL, 12, 11, 0)
    # PRINT a2
    let v6 = enc.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_VM_OPS, srvm_core.VMO_PRINT, 0, 12, 0)
    # HALT
    let v7 = enc.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_VM_OPS, srvm_core.VMO_HALT, 0, 0, 0)
    
    proc print_dec(name, v):
        print name + ": " + str(v & 0xFF) + ", " + str((v >> 8) & 0xFF) + ", " + str((v >> 16) & 0xFF) + ", " + str((v >> 24) & 0xFF)

    print_dec("ADDI a0, 1337", v1)
    print_dec("ADDI a1, 0", v2)
    print_dec("SET_GLOBAL a1, a0", v3)
    print_dec("ADDI a0, 0", v4)
    print_dec("GET_GLOBAL a2, a1", v5)
    print_dec("PRINT a2", v6)
    print_dec("HALT", v7)

gen()
