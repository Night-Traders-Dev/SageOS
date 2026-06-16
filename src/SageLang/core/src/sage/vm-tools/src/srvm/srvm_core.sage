# Sage RISC-V (SRVM) Core
# Instruction formats, opcode mappings, and encoding/decoding logic

# Major Opcode Groups (7-bit)
let OP_LUI     = 0b0110111 # 0x37
let OP_AUIPC   = 0b0010111 # 0x17
let OP_JAL     = 0b1101111 # 0x6F
let OP_JALR    = 0b1100111 # 0x67
let OP_BRANCH  = 0b1100011 # 0x63
let OP_LOAD    = 0b0000011 # 0x03
let OP_STORE   = 0b0100011 # 0x23
let OP_IMM     = 0b0010011 # 0x13
let OP_REG     = 0b0110011 # 0x33
let OP_LDC     = 0b1011011 # Custom-2 Opcode for Load Constant (U-type)
let OP_VMSYS   = 0b1110011 # 0x73 (Standard SYSTEM opcode repurposed)

# Funct3 for OP_BRANCH
let F3_BEQ  = 0b000
let F3_BNE  = 0b001
let F3_BLT  = 0b100
let F3_BGE  = 0b101
let F3_BLTU = 0b110
let F3_BGEU = 0b111

# Funct3 for OP_LOAD
let F3_LB  = 0b000
let F3_LH  = 0b001
let F3_LW  = 0b010
let F3_LD  = 0b011
let F3_LBU = 0b100
let F3_LHU = 0b101
let F3_LWU = 0b110

# Funct3 for OP_STORE
let F3_SB = 0b000
let F3_SH = 0b001
let F3_SW = 0b010
let F3_SD = 0b011

# Funct3 for OP_IMM
let F3_ADDI  = 0b000
let F3_SLTI  = 0b010
let F3_SLTIU = 0b011
let F3_XORI  = 0b100
let F3_ORI   = 0b110
let F3_ANDI  = 0b111
let F3_SLLI  = 0b001
let F3_SRLI  = 0b101 # also SRAI with funct7

# Funct3 for OP_REG
let F3_ADD  = 0b000 # and SUB, and MUL
let F3_SLL  = 0b001 # and MULH
let F3_SLT  = 0b010 # and MULHSU
let F3_SLTU = 0b011 # and MULHU
let F3_XOR  = 0b100 # and DIV
let F3_SRL  = 0b101 # and SRA, and DIVU
let F3_OR   = 0b110 # and REM
let F3_AND  = 0b111 # and REMU

# Custom SageVM Opcodes (using OP_VMSYS)
let F3_VM_OPS   = 0b000
let F3_GPU_OPS  = 0b001
let F3_OBJ_OPS  = 0b010

# VM Ops (funct7)
let VMO_NOP      = 0x00
let VMO_HALT     = 0x01
let VMO_PUSH_ENV = 0x02
let VMO_POP_ENV  = 0x03
let VMO_CALL     = 0x04
let VMO_SETUP_TRY = 0x05
let VMO_END_TRY   = 0x06
let VMO_RAISE    = 0x07
let VMO_IMPORT   = 0x08
let VMO_PRINT    = 0x09
let VMO_ARRAY_LEN = 0x0A
let VMO_PRINTM   = 0x0B
let VMO_EXEC_AST = 0x0C

# Object Ops (funct7)
let OBJ_GET_GLOBAL = 0x00
let OBJ_SET_GLOBAL = 0x01
let OBJ_NEW_CLASS  = 0x02
let OBJ_INHERIT    = 0x03
let OBJ_METHOD_BIND = 0x04
let OBJ_GET_PROP   = 0x05
let OBJ_SET_PROP   = 0x06
let OBJ_NEW_FUNC   = 0x07
let OBJ_ARRAY_NEW  = 0x08
let OBJ_DICT_NEW   = 0x09
let OBJ_TUPLE_NEW  = 0x0A
let OBJ_GET_INDEX  = 0x0B
let OBJ_SET_INDEX  = 0x0C
let OBJ_SLICE      = 0x0D

# GPU Ops (funct7)
let GPU_POLL_EVENTS         = 0x00
let GPU_WINDOW_SHOULD_CLOSE = 0x01
let GPU_GET_TIME            = 0x02
let GPU_KEY_PRESSED         = 0x03
let GPU_MOUSE_PRESSED       = 0x04
let GPU_MOUSE_POS           = 0x05
let GPU_SCREEN_SIZE         = 0x06
let GPU_CLEAR               = 0x07
let GPU_SET_COLOR           = 0x08
let GPU_DRAW_PIXEL          = 0x09
let GPU_DRAW_LINE           = 0x0A
let GPU_DRAW_RECT           = 0x0B
let GPU_DRAW_CIRCLE         = 0x0C
let GPU_DRAW_TRIANGLE       = 0x0D
let GPU_DRAW_TEXT           = 0x0E
let GPU_LOAD_IMAGE          = 0x0F
let GPU_DRAW_IMAGE          = 0x10
let GPU_LOAD_FONT           = 0x11
let GPU_LOAD_SOUND          = 0x12
let GPU_PLAY_SOUND          = 0x13
let GPU_STOP_SOUND          = 0x14
let GPU_LOAD_MUSIC          = 0x15
let GPU_PLAY_MUSIC          = 0x16
let GPU_STOP_MUSIC          = 0x17
let GPU_CMD_BATCH_BEGIN     = 0x18
let GPU_CMD_BATCH_END       = 0x19
let GPU_CMD_DISPATCH        = 0x1A
let GPU_BUFFER_CREATE       = 0x1B

class RVInstruction:
    proc init(self, value):
        self.raw = value
        self.opcode = value & 0x7F
        self.rd = (value >> 7) & 0x1F
        self.funct3 = (value >> 12) & 0x07
        self.rs1 = (value >> 15) & 0x1F
        self.rs2 = (value >> 20) & 0x1F
        self.funct7 = (value >> 25) & 0x7F
        
        # Immediate decodings
        self.imm_i = self.decode_i_imm()
        self.imm_s = self.decode_s_imm()
        self.imm_b = self.decode_b_imm()
        self.imm_u = self.decode_u_imm()
        self.imm_j = self.decode_j_imm()

    proc decode_i_imm(self):
        # 12-bit signed immediate
        let imm = self.raw >> 20
        return self.sign_extend(imm, 12)

    proc decode_s_imm(self):
        # imm[11:5] | rs2 | rs1 | funct3 | imm[4:0] | opcode
        let imm = ((self.raw >> 25) << 5) | ((self.raw >> 7) & 0x1F)
        return self.sign_extend(imm, 12)

    proc decode_b_imm(self):
        # imm[12] | imm[10:5] | rs2 | rs1 | funct3 | imm[4:1] | imm[11] | opcode
        let v = self.raw
        let b12 = (v >> 31) & 0x01
        let b11 = (v >> 7) & 0x01
        let b10_5 = (v >> 25) & 0x3F
        let b4_1 = (v >> 8) & 0x0F
        let imm = (b12 << 12) | (b11 << 11) | (b10_5 << 5) | (b4_1 << 1)
        return self.sign_extend(imm, 13)

    proc decode_u_imm(self):
        # imm[31:12] | rd | opcode
        return self.raw & 0xFFFFF000

    proc decode_j_imm(self):
        # imm[20] | imm[10:1] | imm[11] | imm[19:12] | rd | opcode
        let v = self.raw
        let b20 = (v >> 31) & 0x01
        let b19_12 = (v >> 12) & 0xFF
        let b11 = (v >> 20) & 0x01
        let b10_1 = (v >> 21) & 0x3FF
        let imm = (b20 << 20) | (b19_12 << 12) | (b11 << 11) | (b10_1 << 1)
        return self.sign_extend(imm, 21)

    proc sign_extend(self, val, bits):
        let sign_bit = 1 << (bits - 1)
        if (val & sign_bit) != 0:
            return val - (1 << bits)
        return val

class RVEncoder:
    proc init(self):
        return nil

    proc encode_r(self, opcode, f3, f7, rd, rs1, rs2):
        let o = int(opcode)
        let f_3 = int(f3)
        let f_7 = int(f7)
        let r_d = int(rd)
        let r_s1 = int(rs1)
        let r_s2 = int(rs2)
        return (o & 0x7F) | ((r_d & 0x1F) << 7) | ((f_3 & 0x07) << 12) | ((r_s1 & 0x1F) << 15) | ((r_s2 & 0x1F) << 20) | ((f_7 & 0x7F) << 25)

    proc encode_i(self, opcode, f3, rd, rs1, imm):
        let o = int(opcode)
        let f = int(f3)
        let r_d = int(rd)
        let r_s1 = int(rs1)
        let i = int(imm)
        let i_imm = i & 0xFFF
        var res = o & 0x7F
        res = res | ((r_d & 0x1F) << 7)
        res = res | ((f & 0x07) << 12)
        res = res | ((r_s1 & 0x1F) << 15)
        res = res | (i_imm << 20)
        return res

    proc encode_s(self, opcode, f3, rs1, rs2, imm):
        let o = int(opcode)
        let f = int(f3)
        let r_s1 = int(rs1)
        let r_s2 = int(rs2)
        let i = int(imm) & 0xFFF
        let imm_4_0 = i & 0x1F
        let imm_11_5 = (i >> 5) & 0x7F
        return (o & 0x7F) | (imm_4_0 << 7) | ((f & 0x07) << 12) | ((r_s1 & 0x1F) << 15) | ((r_s2 & 0x1F) << 20) | (imm_11_5 << 25)

    proc encode_b(self, opcode, f3, rs1, rs2, imm):
        let o = int(opcode)
        let f = int(f3)
        let r_s1 = int(rs1)
        let r_s2 = int(rs2)
        let i = int(imm) & 0x1FFF
        let b12 = (i >> 12) & 0x01
        let b11 = (i >> 11) & 0x01
        let b10_5 = (i >> 5) & 0x3F
        let b4_1 = (i >> 1) & 0x0F
        return (o & 0x7F) | (b11 << 7) | (b4_1 << 8) | ((f & 0x07) << 12) | ((r_s1 & 0x1F) << 15) | ((r_s2 & 0x1F) << 20) | (b10_5 << 25) | (b12 << 31)

    proc encode_u(self, opcode, rd, imm):
        let o = int(opcode)
        let r_d = int(rd)
        let i = int(imm)
        return (o & 0x7F) | ((r_d & 0x1F) << 7) | (i & 0xFFFFF000)

    proc encode_j(self, opcode, rd, imm):
        let o = int(opcode)
        let r_d = int(rd)
        let i = int(imm) & 0x1FFFFF
        let b20 = (i >> 20) & 0x01
        let b19_12 = (i >> 12) & 0xFF
        let b11 = (i >> 11) & 0x01
        let b10_1 = (i >> 1) & 0x3FF
        return (o & 0x7F) | ((r_d & 0x1F) << 7) | (b19_12 << 12) | (b11 << 20) | (b10_1 << 21) | (b20 << 31)

class SRVMUtils:
    proc init(self):
        return nil

    proc read_be32(self, data, off):
        var v = int(data[off]) * 16777216
        v = v + int(data[off+1]) * 65536
        v = v + int(data[off+2]) * 256
        v = v + int(data[off+3])
        return v
    
    proc read_le32(self, data, off):
        var v = int(data[off])
        v = v + int(data[off+1]) * 256
        v = v + int(data[off+2]) * 65536
        v = v + int(data[off+3]) * 16777216
        return v

    proc unpack_double(self, bs, off):
        var b0 = int(bs[off])
        var b1 = int(bs[off+1])
        var b2 = int(bs[off+2])
        var b3 = int(bs[off+3])
        var b4 = int(bs[off+4])
        var b5 = int(bs[off+5])
        var b6 = int(bs[off+6])
        var b7 = int(bs[off+7])
        var sign = 1.0
        if int(b0 / 128) == 1:
            sign = -1.0
        var exp = (int(b0 % 128) * 16) + int(b1 / 16)
        var mantissa = 1.0
        if exp == 0:
            mantissa = 0.0
            exp = 1
        mantissa = mantissa + (int(b1 % 16) / 16.0)
        mantissa = mantissa + (b2 / 4096.0)
        mantissa = mantissa + (b3 / 1048576.0)
        mantissa = mantissa + (b4 / 268435456.0)
        mantissa = mantissa + (b5 / 68719476736.0)
        mantissa = mantissa + (b6 / 17592186044416.0)
        mantissa = mantissa + (b7 / 4503599627370496.0)
        var p2 = 1.0
        var e = exp - 1023
        if e > 0:
            var i = 0
            while i < e:
                p2 = p2 * 2.0
                i = i + 1
        elif e < 0:
            var i = 0
            while i < -e:
                p2 = p2 / 2.0
                i = i + 1
        return sign * mantissa * p2
