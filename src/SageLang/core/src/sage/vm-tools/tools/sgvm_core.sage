# Bytecode opcodes (Sync with bytecode.h and metal_vm.h)
let OP_CONSTANT       = 0
let OP_NIL            = 1
let OP_TRUE           = 2
let OP_FALSE          = 3
let OP_POP            = 4
let OP_GET_GLOBAL     = 5
let OP_DEFINE_GLOBAL  = 6
let OP_SET_GLOBAL     = 7
let OP_DEFINE_FUNCTION = 8
let OP_GET_PROPERTY   = 9
let OP_SET_PROPERTY   = 10
let OP_GET_INDEX      = 11
let OP_SET_INDEX      = 12
let OP_LOAD_FUNCTION  = 13
let OP_SLICE          = 14
let OP_ADD            = 15
let OP_SUB            = 16
let OP_MUL            = 17
let OP_DIV            = 18
let OP_MOD            = 19
let OP_NEGATE         = 20
let OP_EQUAL          = 21
let OP_NOT_EQUAL      = 22
let OP_GREATER        = 23
let OP_GREATER_EQUAL  = 24
let OP_LESS           = 25
let OP_LESS_EQUAL     = 26
let OP_BIT_AND        = 27
let OP_BIT_OR         = 28
let OP_BIT_XOR        = 29
let OP_BIT_NOT        = 30
let OP_SHIFT_LEFT     = 31
let OP_SHIFT_RIGHT    = 32
let OP_NOT            = 33
let OP_TRUTHY         = 34
let OP_JUMP           = 35
let OP_JUMP_IF_FALSE  = 36
let OP_CALL           = 37
let OP_CALL_METHOD    = 38
let OP_ARRAY          = 39
let OP_TUPLE          = 40
let OP_DICT           = 41
let OP_PRINT          = 42
let OP_EXEC_AST_STMT  = 43
let OP_RETURN         = 44
let OP_PUSH_ENV       = 45
let OP_POP_ENV        = 46
let OP_DUP            = 47
let OP_ARRAY_LEN      = 48
let OP_BREAK          = 49
let OP_CONTINUE       = 50
let OP_LOOP_BACK      = 51
let OP_IMPORT         = 52
let OP_CLASS          = 53
let OP_METHOD         = 54
let OP_INHERIT        = 55
let OP_SETUP_TRY      = 56
let OP_END_TRY        = 57
let OP_RAISE          = 58

# GPU hot-path opcodes (Phase 16)
let OP_GPU_POLL_EVENTS         = 59
let OP_GPU_WINDOW_SHOULD_CLOSE = 60
let OP_GPU_GET_TIME            = 61
let OP_GPU_KEY_PRESSED         = 62
let OP_GPU_KEY_DOWN            = 63
let OP_GPU_MOUSE_POS           = 64
let OP_GPU_MOUSE_DELTA         = 65
let OP_GPU_UPDATE_INPUT        = 66
let OP_GPU_BEGIN_COMMANDS      = 67
let OP_GPU_END_COMMANDS        = 68
let OP_GPU_CMD_BEGIN_RP        = 69
let OP_GPU_CMD_END_RP          = 70
let OP_GPU_CMD_DRAW            = 71
let OP_GPU_CMD_BIND_GP         = 72
let OP_GPU_CMD_BIND_DS         = 73
let OP_GPU_CMD_SET_VP          = 74
let OP_GPU_CMD_SET_SC          = 75
let OP_GPU_CMD_BIND_VB         = 76
let OP_GPU_CMD_BIND_IB         = 77
let OP_GPU_CMD_DRAW_IDX        = 78
let OP_GPU_SUBMIT_SYNC         = 79
let OP_GPU_ACQUIRE_IMG         = 80
let OP_GPU_PRESENT             = 81
let OP_GPU_WAIT_FENCE          = 82
let OP_GPU_RESET_FENCE         = 83
let OP_GPU_UPDATE_UNIFORM      = 84
let OP_GPU_CMD_PUSH_CONST      = 85
let OP_GPU_CMD_DISPATCH         = 86

let OP_MATH_PRINTM    = 87
let OP_HALT           = 255

class SGVMUtils:
    proc my_int(self, x):
        if x == nil:
            return 0
        return int(x)

    proc hex_to_byte(self, h):
        let chars = "0123456789abcdef"
        var v1 = 0
        var v2 = 0
        var c1 = h[0]
        var c2 = h[1]
        if ord(c1) >= 65 and ord(c1) <= 70:
            c1 = chr(ord(c1) + 32)
        if ord(c2) >= 65 and ord(c2) <= 70:
            c2 = chr(ord(c2) + 32)
        var i = 0
        while i < 16:
            if chars[i] == c1:
                v1 = i
            if chars[i] == c2:
                v2 = i
            i = i + 1
        return v1 * 16 + v2

    proc split_lines(self, s):
        let lines = []
        var current = ""
        let nl = chr(10)
        var i = 0
        while i < len(s):
            let char_val = s[i]
            var is_nl = false
            var is_cr = false
            if type(char_val) == "number":
                is_nl = (char_val == 10)
                is_cr = (char_val == 13)
            else:
                is_nl = (char_val == nl)
                is_cr = (char_val == chr(13))
            
            if is_nl:
                push(lines, current)
                current = ""
            else:
                if not is_cr:
                    if type(char_val) == "number":
                        current = current + chr(char_val)
                    else:
                        current = current + char_val
            i = i + 1
        if len(current) > 0:
            push(lines, current)
        return lines

    proc my_substr(self, s, start, length):
        var res = ""
        var i = 0
        while i < length:
            if start + i < len(s):
                res = res + s[start + i]
            i = i + 1
        return res

    proc parse_int_field(self, line, offset):
        let sub = self.my_substr(line, offset, len(line))
        let trimmed = self.trim(sub)
        let numval = tonumber(trimmed)
        return self.my_int(numval)

    proc parse_hex_byte(self, hex, offset):
        let sub = self.my_substr(hex, offset, 2)
        let bval = self.hex_to_byte(sub)
        return self.my_int(bval)

    proc trim(self, s):
        if len(s) == 0:
            return ""
        var start = 0
        while start < len(s):
            if ord(s[start]) <= 32:
                start = start + 1
            else:
                break
        var eidx = len(s)
        while eidx > start:
            if ord(s[eidx-1]) <= 32:
                eidx = eidx - 1
            else:
                break
        return self.my_substr(s, start, eidx - start)

    proc read_be16(self, bs, off):
        return self.my_int(bs[off]) * 256 + self.my_int(bs[off+1])

    proc read_be32(self, bs, off):
        return self.my_int(bs[off]) * 16777216 + self.my_int(bs[off+1]) * 65536 + self.my_int(bs[off+2]) * 256 + self.my_int(bs[off+3])

    proc unpack_double(self, bs, off):
        var b0 = self.my_int(bs[off])
        var b1 = self.my_int(bs[off+1])
        var b2 = self.my_int(bs[off+2])
        var b3 = self.my_int(bs[off+3])
        var b4 = self.my_int(bs[off+4])
        var b5 = self.my_int(bs[off+5])
        var b6 = self.my_int(bs[off+6])
        var b7 = self.my_int(bs[off+7])
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
