import sys
import io
from sgvm_core import SGVMUtils

# The Enhanced .sgvm Disassembler
# Supports outputting both .svm (bytecode) and .sage (source reconstruction)

class SGVMDisassembler:
    proc init(self, path):
        self.path = path
        self.ut = SGVMUtils()
        self.data = io.readbytes(path)
        self.pos = 0
        self.func_count = 0
        self.const_count = 0
        self.chunk_total = 0
        self.consts = []
        self.chunks = []
        self.opcode_map = {
            "0": "constant", "1": "nil", "2": "true", "3": "false", "4": "pop()", 
            "5": "get_global()", "6": "define_global()", "7": "set_global()",
            "8": "define_function()", "9": "get_property()", "10": "set_property()",
            "11": "get_index()", "12": "set_index()", "13": "load_function()",
            "14": "slice()", "15": " + ", "16": " - ", "17": " * ", "18": " / ",
            "19": " % ", "20": "negate", "21": " == ", "22": " != ", "23": " > ",
            "24": " >= ", "25": " < ", "26": " <= ", "27": " & ", "28": " | ",
            "29": " ^ ", "30": "~", "31": " << ", "32": " >> ", "33": "not ",
            "34": "truthy", "35": "jump()", "36": "if not", "37": "call()",
            "38": "call_method()", "39": "array()", "40": "tuple()", "41": "dict()",
            "42": "print()", "43": "exec_ast()", "44": "return", "45": "push_env()",
            "46": "pop_env()", "47": "dup()", "48": "len()", "49": "break",
            "50": "continue", "51": "loop_back()", "52": "import()", "53": "class()",
            "54": "method()", "55": "inherit()", "56": "setup_try()", "57": "end_try()",
            "58": "raise()", "255": "halt()"
        }

    proc disassemble(self):
        if self.data == nil:
            print "ERROR: Could not read file " + str(self.path)
            return false
        
        # Skip shebang
        if len(self.data) > 0 and self.ut.my_int(self.data[0]) == 35:
            while self.pos < len(self.data) and self.ut.my_int(self.data[self.pos]) != 10:
                self.pos = self.pos + 1
            if self.pos < len(self.data):
                self.pos = self.pos + 1

        # Magic "SGVM"
        if len(self.data) - self.pos < 4: return false
        var magic = ""
        var mi = 0
        while mi < 4:
            magic = magic + chr(self.ut.my_int(self.data[self.pos + mi]))
            mi = mi + 1
        self.pos = self.pos + 4
        if magic != "SGVM":
            print "ERROR: Bad magic " + magic
            return false

        # Version (2 bytes)
        self.pos = self.pos + 2
        
        # Function count (BE16)
        self.func_count = self.ut.read_be16(self.data, self.pos)
        self.pos = self.pos + 2
        
        # Const count (BE16)
        self.const_count = self.ut.read_be16(self.data, self.pos)
        self.pos = self.pos + 2
        
        # Parse constants
        var ci = 0
        while ci < self.const_count:
            let ctype = self.ut.my_int(self.data[self.pos])
            self.pos = self.pos + 1
            if ctype == 1: # Number
                let val = self.ut.unpack_double(self.data, self.pos)
                self.pos = self.pos + 8
                push(self.consts, {"type": "number", "value": val})
            elif ctype == 3: # String
                let slen = self.ut.read_be16(self.data, self.pos)
                self.pos = self.pos + 2
                var s = ""
                var k = 0
                while k < slen:
                    s = s + chr(self.ut.my_int(self.data[self.pos + k]))
                    k = k + 1
                self.pos = self.pos + slen
                push(self.consts, {"type": "string", "value": s})
            else:
                push(self.consts, {"type": "unknown", "value": nil})
            ci = ci + 1
        
        # Chunk total (BE32)
        self.chunk_total = self.ut.read_be32(self.data, self.pos)
        self.pos = self.pos + 4
        
        # Parse chunks
        var chunk_idx = 0
        while chunk_idx < self.chunk_total:
            let chunk_len = self.ut.read_be32(self.data, self.pos)
            self.pos = self.pos + 4
            let end_pos = self.pos + chunk_len
            let instructions = []
            while self.pos < end_pos:
                let op = self.ut.my_int(self.data[self.pos])
                self.pos = self.pos + 1
                let instr = {"op": op, "operands": []}
                
                # Handle operands
                if op == 0 or op == 5 or op == 6 or op == 7 or op == 9 or op == 10 or op == 52 or op == 53 or op == 54:
                    let idx = self.ut.read_be16(self.data, self.pos)
                    self.pos = self.pos + 2
                    push(instr["operands"], idx)
                elif op == 8: # DEFINE_FUNCTION
                    let name_idx = self.ut.read_be16(self.data, self.pos)
                    let chunk_idx_ref = self.ut.read_be16(self.data, self.pos + 2)
                    self.pos = self.pos + 4
                    push(instr["operands"], name_idx)
                    push(instr["operands"], chunk_idx_ref)
                elif op == 13 or op == 35 or op == 36 or op == 39 or op == 40 or op == 41 or op == 43 or op == 51 or op == 56:
                    let val = self.ut.read_be16(self.data, self.pos)
                    self.pos = self.pos + 2
                    push(instr["operands"], val)
                elif op == 37 or op == 47: # CALL or DUP
                    let val = self.ut.my_int(self.data[self.pos])
                    self.pos = self.pos + 1
                    push(instr["operands"], val)
                elif op == 38: # CALL_METHOD
                    let name_idx = self.ut.read_be16(self.data, self.pos)
                    let argc = self.ut.my_int(self.data[self.pos + 2])
                    self.pos = self.pos + 3
                    push(instr["operands"], name_idx)
                    push(instr["operands"], argc)
                
                push(instructions, instr)
            push(self.chunks, instructions)
            chunk_idx = chunk_idx + 1
        
        return true
        
        return true

    proc generate_svm(self):
        var output = "functions " + str(self.func_count) + "\n"
        output = output + "chunks " + str(self.chunk_total - self.func_count) + "\n\n"
        
        output = output + "constants " + str(len(self.consts)) + "\n"
        for ci in range(len(self.consts)):
            let c = self.consts[ci]
            if c["type"] == "number":
                output = output + "number " + str(c["value"]) + "\n"
            elif c["type"] == "string":
                let s = c["value"]
                var hex = ""
                for i in range(len(s)):
                    hex = hex + self.byte_to_hex(ord(s[i]))
                output = output + "string " + str(len(s)) + "\n"
                output = output + hex + "\n"

        for chunk_idx in range(len(self.chunks)):
            if chunk_idx < self.func_count:
                output = output + "\nfunction\n"
            else:
                output = output + "\nchunk\n"
            
            let instructions = self.chunks[chunk_idx]
            var hex_code = ""
            for instr in instructions:
                let op = instr["op"]
                hex_code = hex_code + self.byte_to_hex(op)
                for val in instr["operands"]:
                    if op == 0 or op == 5 or op == 6 or op == 7 or op == 8 or op == 9 or op == 10 or op == 13 or op == 35 or op == 36 or op == 38 or op == 39 or op == 40 or op == 41 or op == 43 or op == 51 or op == 52 or op == 53 or op == 54 or op == 56:
                        if op == 8:
                            hex_code = hex_code + self.byte_to_hex(int(val / 256))
                            hex_code = hex_code + self.byte_to_hex(val % 256)
                        elif op == 38:
                            if val == instr["operands"][0]: 
                                hex_code = hex_code + self.byte_to_hex(int(val / 256))
                                hex_code = hex_code + self.byte_to_hex(val % 256)
                            else: 
                                hex_code = hex_code + self.byte_to_hex(val)
                        else:
                            hex_code = hex_code + self.byte_to_hex(int(val / 256))
                            hex_code = hex_code + self.byte_to_hex(val % 256)
                    elif op == 37 or op == 47:
                        hex_code = hex_code + self.byte_to_hex(val)
            
            output = output + "code " + str(len(hex_code) / 2) + "\n"
            output = output + hex_code + "\n"

        return output

    proc generate_sage(self):
        var code = "# Disassembled from .sgvm\n\n"
        for chunk_idx in range(len(self.chunks)):
            code = code + "# --- Chunk " + str(chunk_idx) + " ---\n"
            let instructions = self.chunks[chunk_idx]
            for instr in instructions:
                let op = instr["op"]
                let key = str(op)
                var line = "  "
                if dict_has(self.opcode_map, key):
                    line = line + self.opcode_map[key]
                else:
                    line = line + "unknown_" + str(op)
                
                if len(instr["operands"]) > 0:
                    line = line + " ["
                    var op_parts = []
                    for val in instr["operands"]:
                        if op == 0 or op == 5 or op == 6 or op == 7 or op == 9 or op == 10 or op == 52 or op == 53 or op == 54 or (op == 8 and val == instr["operands"][0]) or (op == 38 and val == instr["operands"][0]):
                            if val >= 0 and val < len(self.consts):
                                let c = self.consts[val]
                                if c["type"] == "string": push(op_parts, "'" + c["value"] + "'")
                                else: push(op_parts, str(c["value"]))
                            else:
                                push(op_parts, "@" + str(val))
                        else:
                            push(op_parts, str(val))
                    line = line + join(op_parts, ", ") + "]"
                
                code = code + line + "\n"
            code = code + "\n"

        return code

    proc byte_to_hex(self, val):
        let chars = "0123456789abcdef"
        let high = int(val / 16) % 16
        let low = int(val) % 16
        return chars[high] + chars[low]
