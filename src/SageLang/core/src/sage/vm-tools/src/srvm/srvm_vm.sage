# Sage RISC-V Virtual Machine (SRVM)
# Core Interpreter Implementation (RV64I)

import srvm_core
from srvm_core import RVInstruction

class SageVMState:
    proc init(self):
        # 32 x 64-bit registers
        self.x = []
        var i = 0
        while i < 32:
            push(self.x, 0)
            i = i + 1
            
        self.pc = 0
        self.running = true
        
        # Memory segments
        self.bytecode = []
        self.constants = []
        self.chunks = []
        self.current_chunk_idx = 0
        self.stack = [] 
        i = 0
        while i < 1000:
            push(self.stack, 0)
            i = i + 1
        
        self.heap = {} 
        self.call_stack = [] # Stack of [chunk_idx, pc, ra]
        self.try_stack = [] # Stack of [catch_pc, call_stack_depth]
        
        # Register x2 is typically stack pointer (sp)
        self.x[2] = len(self.stack)

class SRVM:
    proc init(self):
        self.state = SageVMState()
        self.trace = false

    proc run(self, bytecode):
        # Initial chunk (0)
        self.state.bytecode = bytecode
        if len(self.state.chunks) == 0:
            push(self.state.chunks, bytecode)
        
        self.state.pc = 0
        self.state.running = true
        
        # JIT State
        var hot_counts = {}
        
        while self.state.running and self.state.pc < len(self.state.bytecode):
            # Fetch
            if self.state.pc + 4 > len(self.state.bytecode):
                break
            
            # Hot-path detection hook
            let pc_key = str(self.state.current_chunk_idx) + ":" + str(self.state.pc)
            if not dict_has(hot_counts, pc_key): hot_counts[pc_key] = 0
            hot_counts[pc_key] = hot_counts[pc_key] + 1
            
            if hot_counts[pc_key] == 1000: # Threshold for JITing
                # TODO: Trigger JIT compilation and OSR
                if self.trace: print "JIT trigger at " + pc_key
            
            let b0 = self.state.bytecode[self.state.pc]
            let b1 = self.state.bytecode[self.state.pc+1]
            let b2 = self.state.bytecode[self.state.pc+2]
            let b3 = self.state.bytecode[self.state.pc+3]
            let raw_instr = int(b0) | (int(b1) << 8) | (int(b2) << 16) | (int(b3) << 24)
            
            let instr = RVInstruction(raw_instr)
            
            if self.trace:
                print "PC: " + str(self.state.pc) + " Op: " + str(instr.opcode) + " rd: " + str(instr.rd)
            
            self.execute(instr)
            
            # x0 is hardwired to zero
            self.state.x[0] = 0

    proc execute(self, instr):
        let op = instr.opcode
        
        if op == srvm_core.OP_LUI:
            self.state.x[instr.rd] = instr.imm_u
            self.state.pc = self.state.pc + 4
        elif op == srvm_core.OP_AUIPC:
            self.state.x[instr.rd] = self.state.pc + instr.imm_u
            self.state.pc = self.state.pc + 4
        elif op == srvm_core.OP_JAL:
            self.state.x[instr.rd] = self.state.pc + 4
            self.state.pc = self.state.pc + instr.imm_j
        elif op == srvm_core.OP_JALR:
            let target = (self.state.x[instr.rs1] + instr.imm_i) & ~1
            let rd_val = self.state.pc + 4
            
            # Special case for return: JALR x0, 0(x1)
            if instr.rd == 0 and instr.rs1 == 1 and instr.imm_i == 0:
                if len(self.state.call_stack) > 0:
                    let frame = pop(self.state.call_stack)
                    self.state.current_chunk_idx = frame[0]
                    self.state.bytecode = self.state.chunks[self.state.current_chunk_idx]
                    self.state.pc = frame[1]
                    self.state.x[1] = frame[2]
                    return # Skip standard PC update
                else:
                    self.state.running = false
                    return

            self.state.x[instr.rd] = rd_val
            self.state.pc = target
        elif op == srvm_core.OP_BRANCH:
            self.handle_branch(instr)
        elif op == srvm_core.OP_IMM:
            self.handle_imm(instr)
        elif op == srvm_core.OP_REG:
            self.handle_reg(instr)
        elif op == srvm_core.OP_LDC:
            self.handle_ldc(instr)
        elif op == srvm_core.OP_LOAD:
            self.handle_load(instr)
        elif op == srvm_core.OP_STORE:
            self.handle_store(instr)
        elif op == srvm_core.OP_VMSYS:
            self.handle_vmsys(instr)
        else:
            if self.trace:
                print "Unknown opcode: " + str(op)
            self.state.running = false

    proc handle_ldc(self, instr):
        let idx = (instr.imm_u >> 12) & 0xFFFFF
        if idx >= 0 and idx < len(self.state.constants):
            self.state.x[instr.rd] = self.state.constants[idx]
        else:
            if self.trace:
                print "Constant pool access violation at " + str(idx)
            self.state.running = false
        self.state.pc = self.state.pc + 4

    proc handle_load(self, instr):
        let addr = self.state.x[instr.rs1] + instr.imm_i
        if addr >= 0 and addr < len(self.state.stack):
            self.state.x[instr.rd] = self.state.stack[addr]
        else:
            if self.trace:
                print "Load access violation at " + str(addr)
            self.state.running = false
        self.state.pc = self.state.pc + 4

    proc handle_store(self, instr):
        let addr = self.state.x[instr.rs1] + instr.imm_s
        let val = self.state.x[instr.rs2]
        if addr >= 0 and addr < len(self.state.stack):
            self.state.stack[addr] = val
        else:
            if self.trace:
                print "Store access violation at " + str(addr)
            self.state.running = false
        self.state.pc = self.state.pc + 4

    proc handle_branch(self, instr):
        let rs1_val = self.state.x[instr.rs1]
        let rs2_val = self.state.x[instr.rs2]
        var take = false
        let f3 = instr.funct3
        if f3 == srvm_core.F3_BEQ: take = (rs1_val == rs2_val)
        elif f3 == srvm_core.F3_BNE: take = (rs1_val != rs2_val)
        elif f3 == srvm_core.F3_BLT: take = (rs1_val < rs2_val)
        elif f3 == srvm_core.F3_BGE: take = (rs1_val >= rs2_val)
        
        if take:
            self.state.pc = self.state.pc + instr.imm_b
        else:
            self.state.pc = self.state.pc + 4

    proc handle_imm(self, instr):
        let rs1_val = self.state.x[instr.rs1]
        let imm = instr.imm_i
        let f3 = instr.funct3
        if f3 == srvm_core.F3_ADDI:
            if imm == 0: self.state.x[instr.rd] = rs1_val
            else: self.state.x[instr.rd] = rs1_val + imm
        elif f3 == srvm_core.F3_SLTI:
            if rs1_val < imm: self.state.x[instr.rd] = 1
            else: self.state.x[instr.rd] = 0
        elif f3 == srvm_core.F3_XORI: self.state.x[instr.rd] = rs1_val ^ imm
        elif f3 == srvm_core.F3_ORI: self.state.x[instr.rd] = rs1_val | imm
        elif f3 == srvm_core.F3_ANDI: self.state.x[instr.rd] = rs1_val & imm
        elif f3 == srvm_core.F3_SLLI: self.state.x[instr.rd] = rs1_val << (imm & 0x3F)
        elif f3 == srvm_core.F3_SRLI: self.state.x[instr.rd] = rs1_val >> (imm & 0x3F)
        self.state.pc = self.state.pc + 4

    proc handle_reg(self, instr):
        let rs1_val = self.state.x[instr.rs1]
        let rs2_val = self.state.x[instr.rs2]
        let f3 = instr.funct3
        let f7 = instr.funct7

        if f7 == 0x01: # M-extension
            if f3 == srvm_core.F3_ADD: # MUL
                self.state.x[instr.rd] = rs1_val * rs2_val
            elif f3 == srvm_core.F3_XOR: # DIV
                if rs2_val != 0: self.state.x[instr.rd] = rs1_val / rs2_val
                else: self.state.x[instr.rd] = 0
            elif f3 == srvm_core.F3_OR: # REM
                if rs2_val != 0: self.state.x[instr.rd] = rs1_val % rs2_val
                else: self.state.x[instr.rd] = 0
            self.state.pc = self.state.pc + 4
            return

        if f3 == srvm_core.F3_ADD:
            if f7 == 0x00: 
                self.state.x[instr.rd] = rs1_val + rs2_val
            elif f7 == 0x20: self.state.x[instr.rd] = rs1_val - rs2_val
        elif f3 == srvm_core.F3_SLL: self.state.x[instr.rd] = rs1_val << (rs2_val & 0x3F)
        elif f3 == srvm_core.F3_SLT:
            if rs1_val < rs2_val: self.state.x[instr.rd] = 1
            else: self.state.x[instr.rd] = 0
        elif f3 == srvm_core.F3_XOR: self.state.x[instr.rd] = rs1_val ^ rs2_val
        elif f3 == srvm_core.F3_SRL:
            if f7 == 0x00: self.state.x[instr.rd] = rs1_val >> (rs2_val & 0x3F)
            elif f7 == 0x20: self.state.x[instr.rd] = rs1_val >> (rs2_val & 0x3F) # SRA
        elif f3 == srvm_core.F3_OR: self.state.x[instr.rd] = rs1_val | rs2_val
        elif f3 == srvm_core.F3_AND: self.state.x[instr.rd] = rs1_val & rs2_val
        self.state.pc = self.state.pc + 4

    proc handle_vmsys(self, instr):
        let f3 = instr.funct3
        let sub_op = instr.rs1
        
        if f3 == srvm_core.F3_VM_OPS:
            if sub_op == srvm_core.VMO_HALT:
                self.state.running = false
            elif sub_op == srvm_core.VMO_PRINT:
                print str(self.state.x[10]) # Use a0
            elif sub_op == srvm_core.VMO_PRINTM:
                print str(self.state.x[10])
            elif sub_op == srvm_core.VMO_PUSH_ENV:
                push(self.state.call_stack, self.state.heap)
                self.state.heap = {}
            elif sub_op == srvm_core.VMO_POP_ENV:
                if len(self.state.call_stack) > 0:
                    self.state.heap = pop(self.state.call_stack)
            elif sub_op == srvm_core.VMO_CALL:
                let func_obj = self.state.x[instr.rs2] 
                var target_chunk = -1
                if type(func_obj) == "number": target_chunk = int(func_obj)
                elif type(func_obj) == "dict" and dict_has(func_obj, "chunk_idx"): target_chunk = int(func_obj["chunk_idx"])
                elif type(func_obj) == "dict" and dict_has(func_obj, "__builtin__"):
                    let b_name = func_obj["__builtin__"]
                    if b_name == "str":
                        self.state.x[10] = str(self.state.x[10]) # Result in a0
                    elif b_name == "int":
                        self.state.x[10] = int(self.state.x[10])
                    self.state.pc = self.state.pc + 4
                    return
                
                if target_chunk >= 0 and target_chunk < len(self.state.chunks):
                    push(self.state.call_stack, [self.state.current_chunk_idx, self.state.pc + 4, self.state.x[1]])
                    self.state.current_chunk_idx = target_chunk
                    self.state.bytecode = self.state.chunks[target_chunk]
                    self.state.pc = 0
                    self.state.x[1] = 0
                    return
            elif sub_op == srvm_core.VMO_ARRAY_LEN:
                let obj = self.state.x[instr.rs2]
                if type(obj) == "list": self.state.x[instr.rd] = len(obj)
                elif type(obj) == "dict": self.state.x[instr.rd] = len(obj)
                else: self.state.x[instr.rd] = 0
            elif sub_op == srvm_core.VMO_SETUP_TRY:
                let catch_offset = instr.imm_i
                push(self.state.try_stack, [self.state.pc + catch_offset, len(self.state.call_stack)])
            elif sub_op == srvm_core.VMO_END_TRY:
                if len(self.state.try_stack) > 0:
                    pop(self.state.try_stack)
            elif sub_op == srvm_core.VMO_RAISE:
                let exc_obj = self.state.x[10] # a0
                if len(self.state.try_stack) > 0:
                    let handler = pop(self.state.try_stack)
                    let catch_pc = handler[0]
                    let target_call_depth = handler[1]
                    while len(self.state.call_stack) > target_call_depth:
                        pop(self.state.call_stack)
                    self.state.pc = catch_pc
                    self.state.x[10] = exc_obj
                    return
                else:
                    print "Unhandled exception: " + str(exc_obj)
                    self.state.running = false
                    return
        elif f3 == srvm_core.F3_OBJ_OPS:
            if sub_op == srvm_core.OBJ_GET_GLOBAL:
                let idx = int(self.state.x[10]) # a0
                let name = self.state.constants[idx]
                if dict_has(self.state.heap, name):
                    self.state.x[instr.rd] = self.state.heap[name]
                elif name == "str":
                    self.state.x[instr.rd] = {"__builtin__": "str"}
                elif name == "int":
                    self.state.x[instr.rd] = {"__builtin__": "int"}
                else:
                    self.state.x[instr.rd] = nil
            elif sub_op == srvm_core.OBJ_SET_GLOBAL:
                let idx = int(self.state.x[10]) # a0
                let val = self.state.x[11] # a1
                let name = self.state.constants[idx]
                self.state.heap[name] = val
            elif sub_op == srvm_core.OBJ_GET_PROP:
                let obj = self.state.x[instr.rs2]
                let name_idx = int(self.state.x[10])
                let name = self.state.constants[name_idx]
                if type(obj) == "dict": self.state.x[instr.rd] = obj[name]
                else: self.state.x[instr.rd] = nil
            elif sub_op == srvm_core.OBJ_SET_PROP:
                let obj = self.state.x[instr.rs2]
                let name_idx = int(self.state.x[10])
                let val = self.state.x[11]
                let name = self.state.constants[name_idx]
                if type(obj) == "dict": obj[name] = val
            elif sub_op == srvm_core.OBJ_NEW_FUNC:
                let chunk_idx = int(self.state.x[10])
                self.state.x[instr.rd] = {"type": "function", "chunk_idx": chunk_idx}
            elif sub_op == srvm_core.OBJ_ARRAY_NEW:
                let size = int(self.state.x[10])
                let init_val = self.state.x[11]
                var arr = []
                var i = 0
                while i < size:
                    push(arr, init_val)
                    i = i + 1
                self.state.x[instr.rd] = arr
            elif sub_op == srvm_core.OBJ_GET_INDEX:
                let obj = self.state.x[instr.rs2]
                let idx = int(self.state.x[10])
                if type(obj) == "list" and idx >= 0 and idx < len(obj):
                    self.state.x[instr.rd] = obj[idx]
                elif type(obj) == "dict":
                    self.state.x[instr.rd] = obj[idx]
                else: self.state.x[instr.rd] = nil
            elif sub_op == srvm_core.OBJ_SET_INDEX:
                let obj = self.state.x[instr.rs2]
                let idx = int(self.state.x[10])
                let val = self.state.x[11]
                if type(obj) == "list" and idx >= 0 and idx < len(obj):
                    obj[idx] = val
                elif type(obj) == "dict":
                    obj[idx] = val
        elif f3 == srvm_core.F3_GPU_OPS:
            self.handle_gpu(instr)
        
        self.state.pc = self.state.pc + 4

    proc handle_gpu(self, instr):
        let sub_op = instr.rs1
        # TODO: Implement mapping for 28 GPU opcodes
        if self.trace:
            print "GPU Op: " + str(sub_op)
        return nil

