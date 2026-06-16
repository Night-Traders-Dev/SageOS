# Sage SVM to SRVM (RISC-V) Translator
# Translates stack-based bytecode to register-based bytecode

import sgvm_core
import srvm_core
from srvm_core import RVEncoder
import srvm_profiler

class StackToRiscVTranslator:
    proc init(self, constants):
        self.encoder = RVEncoder()
        self.profiler = srvm_profiler.TypeProfiler(constants)
        self.reg_stack = []
        self.next_reg = 10 # Start from a0 (x10)
        self.output_bytes = []
        self.label_map = {} # SVM IP -> SRVM PC
        self.jump_patches = [] # (SRVM PC to patch, target SVM IP)

    proc emit_32(self, val):
        push(self.output_bytes, val & 0xFF)
        push(self.output_bytes, (val >> 8) & 0xFF)
        push(self.output_bytes, (val >> 16) & 0xFF)
        push(self.output_bytes, (val >> 24) & 0xFF)

    proc patch_32(self, pc, val):
        self.output_bytes[pc] = val & 0xFF
        self.output_bytes[pc+1] = (val >> 8) & 0xFF
        self.output_bytes[pc+2] = (val >> 16) & 0xFF
        self.output_bytes[pc+3] = (val >> 24) & 0xFF

    proc alloc_reg(self):
        let r = self.next_reg
        self.next_reg = self.next_reg + 1
        if self.next_reg > 17:
            self.next_reg = 10
        return r

    proc translate(self, svm_bytecode):
        # Speculative type analysis
        let speculative_types = self.profiler.analyze(svm_bytecode)
        
        self.output_bytes = []
        self.reg_stack = []
        self.next_reg = 10
        self.label_map = {}
        self.jump_patches = []
        
        # Pre-scan for catch labels
        var catch_labels = {}
        var j = 0
        while j < len(svm_bytecode):
            let op = int(svm_bytecode[j])
            j = j + 1
            if op == sgvm_core.OP_SETUP_TRY:
                let cip = (int(svm_bytecode[j]) << 8) | int(svm_bytecode[j+1])
                catch_labels[str(cip)] = true
                j = j + 2
            elif op == sgvm_core.OP_CONSTANT or op == sgvm_core.OP_GET_GLOBAL or op == sgvm_core.OP_SET_GLOBAL or op == sgvm_core.OP_DEFINE_GLOBAL or op == sgvm_core.OP_JUMP or op == sgvm_core.OP_JUMP_IF_FALSE or op == sgvm_core.OP_DEFINE_FUNCTION or op == sgvm_core.OP_GET_PROPERTY or op == sgvm_core.OP_SET_PROPERTY or op == sgvm_core.OP_METHOD:
                j = j + 2
            elif op == sgvm_core.OP_CALL or op == sgvm_core.OP_GET_LOCAL or op == sgvm_core.OP_SET_LOCAL:
                j = j + 1
        
        var i = 0
        while i < len(svm_bytecode):
            let ip = i
            
            # If this is a catch block start, the VM will have pushed the exception object
            if dict_has(catch_labels, str(ip)):
                self.reg_stack = [10] # Exception is in a0 (x10)
                self.next_reg = 11
            
            self.label_map[str(ip)] = len(self.output_bytes)
            let op = int(svm_bytecode[i])
            i = i + 1
            
            if op == sgvm_core.OP_CONSTANT:
                let idx = (int(svm_bytecode[i]) << 8) | int(svm_bytecode[i+1])
                i = i + 2
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_u(srvm_core.OP_LDC, rd, idx << 12))
                push(self.reg_stack, rd)
                
            elif op == sgvm_core.OP_ADD:
                let rs2 = pop(self.reg_stack)
                let rs1 = pop(self.reg_stack)
                let rd = self.alloc_reg()
                
                # Check speculative type
                # Simple lookup: use current instruction index to get speculative type
                let t = speculative_types[i-1] 
                if t == srvm_profiler.TYPE_INT:
                    # Emit integer specialized ADD
                    self.emit_32(self.encoder.encode_r(srvm_core.OP_REG, srvm_core.F3_ADD, 0, rd, rs1, rs2))
                else:
                    # Emit generic/fallback ADD
                    self.emit_32(self.encoder.encode_r(srvm_core.OP_REG, srvm_core.F3_ADD, 0, rd, rs1, rs2))
                
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_SUB:
                let rs2 = pop(self.reg_stack)
                let rs1 = pop(self.reg_stack)
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_r(srvm_core.OP_REG, srvm_core.F3_ADD, 0x20, rd, rs1, rs2))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_MUL:
                let rs2 = pop(self.reg_stack)
                let rs1 = pop(self.reg_stack)
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_r(srvm_core.OP_REG, srvm_core.F3_ADD, 0x01, rd, rs1, rs2))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_DIV:
                let rs2 = pop(self.reg_stack)
                let rs1 = pop(self.reg_stack)
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_r(srvm_core.OP_REG, srvm_core.F3_XOR, 0x01, rd, rs1, rs2))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_MOD:
                let rs2 = pop(self.reg_stack)
                let rs1 = pop(self.reg_stack)
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_r(srvm_core.OP_REG, srvm_core.F3_OR, 0x01, rd, rs1, rs2))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_BIT_AND:
                let rs2 = pop(self.reg_stack)
                let rs1 = pop(self.reg_stack)
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_r(srvm_core.OP_REG, srvm_core.F3_AND, 0, rd, rs1, rs2))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_BIT_OR:
                let rs2 = pop(self.reg_stack)
                let rs1 = pop(self.reg_stack)
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_r(srvm_core.OP_REG, srvm_core.F3_OR, 0, rd, rs1, rs2))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_BIT_XOR:
                let rs2 = pop(self.reg_stack)
                let rs1 = pop(self.reg_stack)
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_r(srvm_core.OP_REG, srvm_core.F3_XOR, 0, rd, rs1, rs2))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_BIT_NOT:
                let rs1 = pop(self.reg_stack)
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_XORI, rd, rs1, -1))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_EQUAL:
                let rs2 = pop(self.reg_stack)
                let rs1 = pop(self.reg_stack)
                let rt = self.alloc_reg()
                self.emit_32(self.encoder.encode_r(srvm_core.OP_REG, srvm_core.F3_XOR, 0, rt, rs1, rs2))
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_SLTIU, rd, rt, 1))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_LESS:
                let rs2 = pop(self.reg_stack)
                let rs1 = pop(self.reg_stack)
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_r(srvm_core.OP_REG, srvm_core.F3_SLT, 0, rd, rs1, rs2))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_GET_INDEX:
                let idx = pop(self.reg_stack)
                let obj = pop(self.reg_stack)
                let rd = self.alloc_reg()
                if idx != 10:
                    self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 10, idx, 0))
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_OBJ_OPS, 0, rd, srvm_core.OBJ_GET_INDEX, obj))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_SET_INDEX:
                let val = pop(self.reg_stack)
                let idx = pop(self.reg_stack)
                let obj = pop(self.reg_stack)
                if val != 11:
                    self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 11, val, 0))
                if idx != 10:
                    self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 10, idx, 0))
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_OBJ_OPS, 0, 0, srvm_core.OBJ_SET_INDEX, obj))

            elif op == sgvm_core.OP_ARRAY:
                let init_val = pop(self.reg_stack)
                let size = pop(self.reg_stack)
                let rd = self.alloc_reg()
                if init_val != 10:
                    self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 10, init_val, 0))
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_OBJ_OPS, 0, rd, srvm_core.OBJ_ARRAY_NEW, size))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_JUMP:
                let target_ip = (int(svm_bytecode[i]) << 8) | int(svm_bytecode[i+1])
                i = i + 2
                let patch_pc = len(self.output_bytes)
                self.emit_32(self.encoder.encode_j(srvm_core.OP_JAL, 0, 0)) 
                push(self.jump_patches, [patch_pc, target_ip])
                
            elif op == sgvm_core.OP_GET_PROPERTY:
                let name_idx = (int(svm_bytecode[i]) << 8) | int(svm_bytecode[i+1])
                i = i + 2
                let obj = pop(self.reg_stack)
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 10, 0, name_idx))
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_OBJ_OPS, 0, rd, srvm_core.OBJ_GET_PROP, obj))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_SET_PROPERTY:
                let name_idx = (int(svm_bytecode[i]) << 8) | int(svm_bytecode[i+1])
                i = i + 2
                let val = pop(self.reg_stack)
                let obj = pop(self.reg_stack)
                if val != 11:
                    self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 11, val, 0))
                self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 10, 0, name_idx))
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_OBJ_OPS, 0, 0, srvm_core.OBJ_SET_PROP, obj))

            elif op == sgvm_core.OP_POP:
                if len(self.reg_stack) > 0:
                    pop(self.reg_stack)

            elif op == sgvm_core.OP_DUP:
                if len(self.reg_stack) > 0:
                    let rs = self.reg_stack[len(self.reg_stack)-1]
                    let rd = self.alloc_reg()
                    self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, rd, rs, 0))
                    push(self.reg_stack, rd)

            elif op == sgvm_core.OP_CLASS:
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_OBJ_OPS, 0, rd, srvm_core.OBJ_NEW_CLASS, 0))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_INHERIT:
                let parent = pop(self.reg_stack)
                let child = pop(self.reg_stack)
                if parent != 10:
                    self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 10, parent, 0))
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_OBJ_OPS, 0, 0, srvm_core.OBJ_INHERIT, child))
                push(self.reg_stack, child)

            elif op == sgvm_core.OP_METHOD:
                let name_idx = (int(svm_bytecode[i]) << 8) | int(svm_bytecode[i+1])
                i = i + 2
                let func = pop(self.reg_stack)
                let klass = pop(self.reg_stack)
                if func != 11:
                    self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 11, func, 0))
                self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 10, 0, name_idx))
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_OBJ_OPS, 0, 0, srvm_core.OBJ_METHOD_BIND, klass))

            elif op == sgvm_core.OP_DEFINE_GLOBAL:
                let idx = (int(svm_bytecode[i]) << 8) | int(svm_bytecode[i+1])
                i = i + 2
                let val = pop(self.reg_stack)
                if val != 11:
                    self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 11, val, 0))
                self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 10, 0, idx))
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_OBJ_OPS, 0, 0, srvm_core.OBJ_SET_GLOBAL, 0))

            elif op == sgvm_core.OP_NIL:
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, rd, 0, 0)) 
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_TRUE:
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, rd, 0, 1))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_FALSE:
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, rd, 0, 0))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_GET_GLOBAL:
                let idx = (int(svm_bytecode[i]) << 8) | int(svm_bytecode[i+1])
                i = i + 2
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 10, 0, idx))
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_OBJ_OPS, 0, rd, srvm_core.OBJ_GET_GLOBAL, 0))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_SET_GLOBAL:
                let idx = (int(svm_bytecode[i]) << 8) | int(svm_bytecode[i+1])
                i = i + 2
                let val = pop(self.reg_stack)
                if val != 11:
                    self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 11, val, 0))
                self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 10, 0, idx))
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_OBJ_OPS, 0, 0, srvm_core.OBJ_SET_GLOBAL, 0))
            
            elif op == sgvm_core.OP_GET_LOCAL:
                let idx = int(svm_bytecode[i])
                i = i + 1
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_i(srvm_core.OP_LOAD, srvm_core.F3_LD, rd, 8, idx * 8))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_SET_LOCAL:
                let idx = int(svm_bytecode[i])
                i = i + 1
                let rs = pop(self.reg_stack)
                self.emit_32(self.encoder.encode_s(srvm_core.OP_STORE, srvm_core.F3_SD, 8, rs, idx * 8))
                
            elif op == sgvm_core.OP_SETUP_TRY:
                let catch_ip = (int(svm_bytecode[i]) << 8) | int(svm_bytecode[i+1])
                i = i + 2
                let patch_pc = len(self.output_bytes)
                self.emit_32(self.encoder.encode_i(srvm_core.OP_VMSYS, srvm_core.F3_VM_OPS, 0, srvm_core.VMO_SETUP_TRY, 0)) 
                push(self.jump_patches, [patch_pc, catch_ip])

            elif op == sgvm_core.OP_END_TRY:
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_VM_OPS, 0, 0, srvm_core.VMO_END_TRY, 0))

            elif op == sgvm_core.OP_RAISE:
                let exc_obj = pop(self.reg_stack)
                if exc_obj != 10:
                    self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 10, exc_obj, 0))
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_VM_OPS, 0, 0, srvm_core.VMO_RAISE, 0))

            elif op == sgvm_core.OP_PUSH_ENV:
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_VM_OPS, 0, 0, srvm_core.VMO_PUSH_ENV, 0))

            elif op == sgvm_core.OP_POP_ENV:
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_VM_OPS, 0, 0, srvm_core.VMO_POP_ENV, 0))

            elif op == sgvm_core.OP_JUMP_IF_FALSE:
                let target_ip = (int(svm_bytecode[i]) << 8) | int(svm_bytecode[i+1])
                i = i + 2
                let cond = pop(self.reg_stack)
                let patch_pc = len(self.output_bytes)
                self.emit_32(self.encoder.encode_b(srvm_core.OP_BRANCH, srvm_core.F3_BEQ, cond, 0, 0)) 
                push(self.jump_patches, [patch_pc, target_ip])

            elif op == sgvm_core.OP_RETURN:
                self.emit_32(self.encoder.encode_i(srvm_core.OP_JALR, 0, 0, 1, 0))

            elif op == sgvm_core.OP_DEFINE_FUNCTION:
                let chunk_idx = (int(svm_bytecode[i]) << 8) | int(svm_bytecode[i+1])
                i = i + 2
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 10, 0, chunk_idx))
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_OBJ_OPS, 0, rd, srvm_core.OBJ_NEW_FUNC, 0))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_CALL:
                let argc = int(svm_bytecode[i])
                i = i + 1
                var args = []
                var k = 0
                while k < argc:
                    push(args, pop(self.reg_stack))
                    k = k + 1
                k = argc - 1
                while k >= 0:
                    let src = args[argc - 1 - k]
                    let dest = 10 + k 
                    if src != dest:
                        self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, dest, src, 0))
                    k = k - 1
                let func_reg = pop(self.reg_stack)
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_VM_OPS, 0, 0, srvm_core.VMO_CALL, func_reg))
                let rd = self.alloc_reg()
                self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, rd, 10, 0))
                push(self.reg_stack, rd)

            elif op == sgvm_core.OP_PRINT:
                let rs = pop(self.reg_stack)
                if rs != 10:
                    self.emit_32(self.encoder.encode_i(srvm_core.OP_IMM, srvm_core.F3_ADDI, 10, rs, 0))
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_VM_OPS, 0, 0, srvm_core.VMO_PRINT, 0))
                
            elif op == sgvm_core.OP_HALT:
                self.emit_32(self.encoder.encode_r(srvm_core.OP_VMSYS, srvm_core.F3_VM_OPS, 0, 0, srvm_core.VMO_HALT, 0))
        
        var p = 0
        while p < len(self.jump_patches):
            let patch = self.jump_patches[p]
            let patch_pc = patch[0]
            let target_ip = patch[1]
            if self.label_map[str(target_ip)] != nil:
                let target_pc = self.label_map[str(target_ip)]
                let offset = target_pc - patch_pc
                let b0 = self.output_bytes[patch_pc]
                let opcode = b0 & 0x7F
                var instr = 0
                if opcode == srvm_core.OP_JAL:
                    instr = self.encoder.encode_j(srvm_core.OP_JAL, 0, offset)
                elif opcode == srvm_core.OP_BRANCH:
                    let raw = int(self.output_bytes[patch_pc]) | (int(self.output_bytes[patch_pc+1]) << 8) | (int(self.output_bytes[patch_pc+2]) << 16) | (int(self.output_bytes[patch_pc+3]) << 24)
                    let rs1 = (raw >> 15) & 0x1F
                    let rs2 = (raw >> 20) & 0x1F
                    let f3 = (raw >> 12) & 0x07
                    instr = self.encoder.encode_b(srvm_core.OP_BRANCH, f3, rs1, rs2, offset)
                elif opcode == srvm_core.OP_VMSYS:
                    let raw = int(self.output_bytes[patch_pc]) | (int(self.output_bytes[patch_pc+1]) << 8) | (int(self.output_bytes[patch_pc+2]) << 16) | (int(self.output_bytes[patch_pc+3]) << 24)
                    let rd = (raw >> 7) & 0x1F
                    let sub_op = (raw >> 15) & 0x1F
                    let f3 = (raw >> 12) & 0x07
                    instr = self.encoder.encode_i(srvm_core.OP_VMSYS, f3, rd, sub_op, offset)
                if instr != 0:
                    self.patch_32(patch_pc, instr)
            p = p + 1
            
        return self.output_bytes

class SGRVCompiler:
    proc init(self, constants):
        self.translator = StackToRiscVTranslator(constants)
        self.utils = srvm_core.SRVMUtils()

    proc compile(self, sgvm_data):
        var pos = 0
        if len(sgvm_data) < 4: return nil
        if int(sgvm_data[0]) == 35:
            while pos < len(sgvm_data) and int(sgvm_data[pos]) != 10: pos = pos + 1
            if pos < len(sgvm_data): pos = pos + 1
        
        if int(sgvm_data[pos]) != 83 or int(sgvm_data[pos+1]) != 71 or int(sgvm_data[pos+2]) != 86 or int(sgvm_data[pos+3]) != 77:
            return nil
        pos = pos + 4 + 2 # Skip magic and version
        
        let func_count = (int(sgvm_data[pos]) << 8) | int(sgvm_data[pos+1])
        pos = pos + 2
        let const_count = (int(sgvm_data[pos]) << 8) | int(sgvm_data[pos+1])
        pos = pos + 2
        
        var constants = []
        
        var output = [83, 71, 82, 86, 0, 1]
        push(output, (const_count >> 8) & 0xFF)
        push(output, const_count & 0xFF)
        
        var ci = 0
        while ci < const_count:
            let t = int(sgvm_data[pos])
            push(output, t)
            pos = pos + 1
            if t == 1: # Number
                var val_bytes = []
                var k = 0
                while k < 8:
                    push(val_bytes, int(sgvm_data[pos + k]))
                    push(output, int(sgvm_data[pos + k]))
                    k = k + 1
                pos = pos + 8
                # Simplified: Need to unpack to get number object
                push(constants, {"type": 1})
            elif t == 3: # String
                let slen = (int(sgvm_data[pos]) << 8) | int(sgvm_data[pos+1])
                push(output, (slen >> 8) & 0xFF)
                push(output, slen & 0xFF)
                pos = pos + 2
                var s = ""
                var k = 0
                while k < slen:
                    let b = int(sgvm_data[pos + k])
                    push(output, b)
                    s = s + chr(b)
                    k = k + 1
                pos = pos + slen
                push(constants, {"type": 3, "str": s})
            ci = ci + 1
            
        # Initialize translator with constants
        self.translator = StackToRiscVTranslator(constants)
            
        let num_chunks = (int(sgvm_data[pos]) << 24) | (int(sgvm_data[pos+1]) << 16) | (int(sgvm_data[pos+2]) << 8) | int(sgvm_data[pos+3])
        pos = pos + 4
        
        push(output, (num_chunks >> 24) & 0xFF)
        push(output, (num_chunks >> 16) & 0xFF)
        push(output, (num_chunks >> 8) & 0xFF)
        push(output, num_chunks & 0xFF)
        
        var chunk_idx = 0
        while chunk_idx < num_chunks:
            let clen = (int(sgvm_data[pos]) << 24) | (int(sgvm_data[pos+1]) << 16) | (int(sgvm_data[pos+2]) << 8) | int(sgvm_data[pos+3])
            pos = pos + 4
            var svm_bc = []
            var i = 0
            while i < clen:
                push(svm_bc, int(sgvm_data[pos + i]))
                i = i + 1
            pos = pos + clen
            let translated = self.translator.translate(svm_bc)
            let t_len = len(translated)
            push(output, (t_len >> 24) & 0xFF)
            push(output, (t_len >> 16) & 0xFF)
            push(output, (t_len >> 8) & 0xFF)
            push(output, t_len & 0xFF)
            i = 0
            while i < t_len:
                push(output, translated[i])
                i = i + 1
            chunk_idx = chunk_idx + 1
        return output

    proc save(self, path, data, io_mod):
        io_mod.writebytes(path, data)
