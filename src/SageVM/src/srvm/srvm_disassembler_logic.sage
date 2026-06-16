import sys
import io
import srvm_core
from srvm_core import RVInstruction, SRVMUtils

# The Sage RISC-V (SRVM) Disassembler
# Decodes .sgrv binaries into readable RISC-V assembly

class SRVMDisassembler:
    proc init(self, path):
        self.path = path
        self.ut = SRVMUtils()
        self.data = io.readbytes(path)
        self.pos = 0
        self.const_count = 0
        self.chunk_total = 0
        self.consts = []
        self.chunks = []

    proc disassemble(self):
        if self.data == nil:
            print "ERROR: Could not read file " + str(self.path)
            return false
        
        # Skip shebang
        if len(self.data) > 0 and int(self.data[0]) == 35:
            while self.pos < len(self.data) and int(self.data[self.pos]) != 10:
                self.pos = self.pos + 1
            if self.pos < len(self.data):
                self.pos = self.pos + 1

        # Magic "SGRV"
        if len(self.data) - self.pos < 4: return false
        var magic = ""
        var mi = 0
        while mi < 4:
            magic = magic + chr(int(self.data[self.pos + mi]))
            mi = mi + 1
        self.pos = self.pos + 4
        if magic != "SGRV":
            print "ERROR: Bad magic " + magic
            return false

        # Version (2 bytes)
        self.pos = self.pos + 2
        
        # Const count (BE16)
        self.const_count = (int(self.data[self.pos]) << 8) | int(self.data[self.pos + 1])
        self.pos = self.pos + 2
        
        # Parse constants
        var ci = 0
        var c_str = ""
        var c_idx = 0
        while ci < self.const_count:
            let ctype = int(self.data[self.pos])
            self.pos = self.pos + 1
            if ctype == 1: # Number
                let val = self.ut.unpack_double(self.data, self.pos)
                self.pos = self.pos + 8
                push(self.consts, {"type": "number", "value": val})
            elif ctype == 3: # String
                let slen = (int(self.data[self.pos]) << 8) | int(self.data[self.pos + 1])
                self.pos = self.pos + 2
                c_str = ""
                c_idx = 0
                while c_idx < slen:
                    c_str = c_str + chr(int(self.data[self.pos + c_idx]))
                    c_idx = c_idx + 1
                self.pos = self.pos + slen
                push(self.consts, {"type": "string", "value": c_str})
            else:
                push(self.consts, {"type": "unknown", "value": nil})
            ci = ci + 1
        
        # Chunk total (BE32)
        self.chunk_total = (int(self.data[self.pos]) << 24) | (int(self.data[self.pos+1]) << 16) | (int(self.data[self.pos+2]) << 8) | int(self.data[self.pos+3])
        self.pos = self.pos + 4
        
        # Parse chunks
        var chunk_idx = 0
        while chunk_idx < self.chunk_total:
            let chunk_len = (int(self.data[self.pos]) << 24) | (int(self.data[self.pos+1]) << 16) | (int(self.data[self.pos+2]) << 8) | int(self.data[self.pos+3])
            self.pos = self.pos + 4
            let end_pos = self.pos + chunk_len
            let instructions = []
            while self.pos < end_pos:
                let val = self.ut.read_le32(self.data, self.pos)
                self.pos = self.pos + 4
                push(instructions, RVInstruction(val))
            push(self.chunks, instructions)
            chunk_idx = chunk_idx + 1
        
        return true

    proc generate_sage(self):
        print "# Disassembled from .sgrv (Sage RISC-V)"
        print ""
        var chunk_idx = 0
        while chunk_idx < len(self.chunks):
            print "# --- Chunk " + str(chunk_idx) + " ---"
            let instructions = self.chunks[chunk_idx]
            var i = 0
            while i < len(instructions):
                let instr = instructions[i]
                let pc = i * 4
                let d = self.decode_instr(instr)
                var line = "  "
                line = line + self.pad_left(str(pc), 4, " ")
                line = line + ":  "
                line = line + d
                print line
                i = i + 1
            print ""
            chunk_idx = chunk_idx + 1
        return true

    proc decode_instr(self, instr):
        let reg_names = [
            "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
            "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
            "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
            "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
        ]
        let op = instr.opcode
        var res = ""
        if op == srvm_core.OP_LUI:
            res = "lui      " + reg_names[int(instr.rd)]
            res = res + ", "
            res = res + str(instr.decode_u_imm() >> 12)
            return res
        elif op == srvm_core.OP_AUIPC:
            res = "auipc    " + reg_names[int(instr.rd)]
            res = res + ", "
            res = res + str(instr.decode_u_imm() >> 12)
            return res
        elif op == srvm_core.OP_JAL:
            res = "jal      " + reg_names[int(instr.rd)]
            res = res + ", "
            res = res + str(instr.decode_j_imm())
            return res
        elif op == srvm_core.OP_JALR:
            res = "jalr     " + reg_names[int(instr.rd)]
            res = res + ", "
            res = res + reg_names[int(instr.rs1)]
            res = res + "("
            res = res + str(instr.decode_i_imm())
            res = res + ")"
            return res
        elif op == srvm_core.OP_BRANCH:
            var bop = "beq"
            let f3 = instr.funct3
            if f3 == srvm_core.F3_BNE: bop = "bne"
            elif f3 == srvm_core.F3_BLT: bop = "blt"
            elif f3 == srvm_core.F3_BGE: bop = "bge"
            elif f3 == srvm_core.F3_BLTU: bop = "bltu"
            elif f3 == srvm_core.F3_BGEU: bop = "bgeu"
            res = self.pad_right(bop, 8) + " " + reg_names[int(instr.rs1)]
            res = res + ", "
            res = res + reg_names[int(instr.rs2)]
            res = res + ", "
            res = res + str(instr.decode_b_imm())
            return res
        elif op == srvm_core.OP_LOAD:
            var lop = "lb"
            let f3 = instr.funct3
            if f3 == srvm_core.F3_LH: lop = "lh"
            elif f3 == srvm_core.F3_LW: lop = "lw"
            elif f3 == srvm_core.F3_LD: lop = "ld"
            elif f3 == srvm_core.F3_LBU: lop = "lbu"
            elif f3 == srvm_core.F3_LHU: lop = "lhu"
            res = self.pad_right(lop, 8) + " " + reg_names[int(instr.rd)]
            res = res + ", "
            res = res + str(instr.decode_i_imm())
            res = res + "("
            res = res + reg_names[int(instr.rs1)]
            res = res + ")"
            return res
        elif op == srvm_core.OP_STORE:
            var sop = "sb"
            let f3 = instr.funct3
            if f3 == srvm_core.F3_SH: sop = "sh"
            elif f3 == srvm_core.F3_SW: sop = "sw"
            elif f3 == srvm_core.F3_SD: sop = "sd"
            res = self.pad_right(sop, 8) + " " + reg_names[int(instr.rs2)]
            res = res + ", "
            res = res + str(instr.decode_s_imm())
            res = res + "("
            res = res + reg_names[int(instr.rs1)]
            res = res + ")"
            return res
        elif op == srvm_core.OP_IMM:
            var iop = "addi"
            let f3 = instr.funct3
            if f3 == srvm_core.F3_SLTI: iop = "slti"
            elif f3 == srvm_core.F3_SLTIU: iop = "sltiu"
            elif f3 == srvm_core.F3_XORI: iop = "xori"
            elif f3 == srvm_core.F3_ORI: iop = "ori"
            elif f3 == srvm_core.F3_ANDI: iop = "andi"
            elif f3 == srvm_core.F3_SLLI: iop = "slli"
            elif f3 == srvm_core.F3_SRLI:
                if instr.funct7 == 0x20: iop = "srai"
                else: iop = "srli"
            res = self.pad_right(iop, 8) + " " + reg_names[int(instr.rd)]
            res = res + ", "
            res = res + reg_names[int(instr.rs1)]
            res = res + ", "
            res = res + str(instr.decode_i_imm())
            return res
        elif op == srvm_core.OP_REG:
            var rop = "add"
            let f3 = instr.funct3
            let f7 = instr.funct7
            if f3 == srvm_core.F3_ADD:
                if f7 == 0x20: rop = "sub"
                elif f7 == 0x01: rop = "mul"
                else: rop = "add"
            elif f3 == srvm_core.F3_SLL:
                if f7 == 0x01: rop = "mulh"
                else: rop = "sll"
            elif f3 == srvm_core.F3_SLT:
                if f7 == 0x01: rop = "mulhsu"
                else: rop = "slt"
            elif f3 == srvm_core.F3_SLTU:
                if f7 == 0x01: rop = "mulhu"
                else: rop = "sltu"
            elif f3 == srvm_core.F3_XOR:
                if f7 == 0x01: rop = "div"
                else: rop = "xor"
            elif f3 == srvm_core.F3_SRL:
                if f7 == 0x20: rop = "sra"
                elif f7 == 0x01: rop = "divu"
                else: rop = "srl"
            elif f3 == srvm_core.F3_OR:
                if f7 == 0x01: rop = "rem"
                else: rop = "or"
            elif f3 == srvm_core.F3_AND:
                if f7 == 0x01: rop = "remu"
                else: rop = "and"
            res = self.pad_right(rop, 8) + " " + reg_names[int(instr.rd)]
            res = res + ", "
            res = res + reg_names[int(instr.rs1)]
            res = res + ", "
            res = res + reg_names[int(instr.rs2)]
            return res
        elif op == srvm_core.OP_LDC:
            let rd = int(instr.rd)
            let imm = int(instr.decode_u_imm() >> 12)
            var label = str(imm)
            if imm >= 0 and imm < len(self.consts):
                let c = self.consts[imm]
                if c["type"] == "string": label = label + " ('" + c["value"] + "')"
                elif c["type"] == "number": label = label + " (" + str(c["value"]) + ")"
            res = "ldc      " + reg_names[rd]
            res = res + ", "
            res = res + label
            return res
        elif op == srvm_core.OP_VMSYS:
            let f3 = instr.funct3
            if f3 == srvm_core.F3_VM_OPS:
                var vop = "vm_nop"
                let f7 = instr.funct7
                if f7 == srvm_core.VMO_HALT: vop = "halt"
                elif f7 == srvm_core.VMO_PUSH_ENV: vop = "push_env"
                elif f7 == srvm_core.VMO_POP_ENV: vop = "pop_env"
                elif f7 == srvm_core.VMO_CALL: vop = "call"
                elif f7 == srvm_core.VMO_SETUP_TRY: vop = "setup_try"
                elif f7 == srvm_core.VMO_END_TRY: vop = "end_try"
                elif f7 == srvm_core.VMO_RAISE: vop = "raise"
                elif f7 == srvm_core.VMO_IMPORT: vop = "import"
                elif f7 == srvm_core.VMO_PRINT: vop = "print"
                elif f7 == srvm_core.VMO_ARRAY_LEN: vop = "len"
                elif f7 == srvm_core.VMO_PRINTM: vop = "printm"
                elif f7 == srvm_core.VMO_EXEC_AST: vop = "exec_ast"
                res = self.pad_right(vop, 8) + " rd=" + reg_names[int(instr.rd)]
                res = res + " rs1=" + reg_names[int(instr.rs1)]
                res = res + " rs2=" + reg_names[int(instr.rs2)]
                return res
            elif f3 == srvm_core.F3_OBJ_OPS:
                var oop = "obj_op"
                res = self.pad_right(oop, 8) + " f7=" + str(instr.funct7)
                res = res + " rd=" + reg_names[int(instr.rd)]
                return res
            elif f3 == srvm_core.F3_GPU_OPS:
                res = self.pad_right("gpu_op", 8) + " f7=" + str(instr.funct7)
                res = res + " rd=" + reg_names[int(instr.rd)]
                return res
        
        return "unknown  op=" + str(op)

    proc pad_left(self, s, width, char):
        var res = s
        while len(res) < width:
            res = char + res
        return res

    proc pad_right(self, s, width):
        var res = s
        while len(res) < width:
            res = res + " "
        return res
