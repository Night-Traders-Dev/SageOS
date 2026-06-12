import sys
import io

proc sys_exec(cmd):
    return sys.exec(cmd)

proc io_readfile(path):
    return io.readfile(path)

proc io_writebytes(path, bytes):
    return io.writebytes(path, bytes)

import sgvm_core
from sgvm_core import SGVMUtils
from sgvm_core import OP_CONSTANT, OP_GET_GLOBAL, OP_DEFINE_GLOBAL, OP_SET_GLOBAL
from sgvm_core import OP_DEFINE_FUNCTION, OP_GET_PROPERTY, OP_SET_PROPERTY, OP_LOAD_FUNCTION
from sgvm_core import OP_JUMP, OP_JUMP_IF_FALSE, OP_ARRAY, OP_TUPLE, OP_DICT
from sgvm_core import OP_EXEC_AST_STMT, OP_BREAK, OP_CONTINUE, OP_LOOP_BACK
from sgvm_core import OP_IMPORT, OP_CLASS, OP_METHOD, OP_SETUP_TRY
from sgvm_core import OP_CALL_METHOD, OP_CALL, OP_DUP

class SGVMCompiler:
    proc init(self):
        self.output_bytes = []
        self.global_consts = []
        self.const_map = {} 
        self.local_to_global = []
        self.local_to_global_raw = [] 
        self.chunk_params = []
        self.current_chunk = -1
        self.utils = SGVMUtils()

    proc write_byte(self, b):
        push(self.output_bytes, self.utils.my_int(b))

    proc write_string(self, s):
        var i = 0
        while i < len(s):
            self.write_byte(ord(s[i]))
            i = i + 1

    proc write_be16(self, v):
        var val = self.utils.my_int(v)
        self.write_byte(self.utils.my_int(val / 256))
        self.write_byte(val % 256)

    proc write_be32(self, v):
        var val = self.utils.my_int(v)
        self.write_byte(self.utils.my_int(val / 16777216) % 256)
        self.write_byte(self.utils.my_int(val / 65536) % 256)
        self.write_byte(self.utils.my_int(val / 256) % 256)
        self.write_byte(val % 256)

    proc write_double(self, v):
        if v == nil:
            return
        
        if v == 0.0:
            var i = 0
            while i < 8:
                self.write_byte(0)
                i = i + 1
            return
        
        var sign = 1.0
        var val = v
        if val < 0.0:
            sign = -1.0
            val = -val
        
        var exp = 0
        if val >= 1.0:
            while val >= 2.0:
                val = val / 2.0
                exp = exp + 1
        else:
            while val < 1.0:
                val = val * 2.0
                exp = exp - 1
        
        var mantissa = val - 1.0
        var e_field = exp + 1023
        var b0 = 0.0
        if sign < 0.0:
            b0 = 128.0
        b0 = b0 + self.utils.my_int(e_field / 16)
        var b1 = (e_field % 16) * 16
        var f = mantissa
        var bits = []
        var i = 0
        while i < 52:
            f = f * 2.0
            if f >= 1.0:
                push(bits, 1.0)
                f = f - 1.0
            else:
                push(bits, 0.0)
            i = i + 1
        
        var b1_low = 0.0
        var k = 0
        while k < 4:
            b1_low = b1_low * 2.0 + bits[k]
            k = k + 1
        
        self.write_byte(b0)
        self.write_byte(b1 + b1_low)
        var byte_idx = 2
        while byte_idx < 8:
            var bv = 0.0
            var bit_idx = 0
            while bit_idx < 8:
                bv = bv * 2.0 + bits[4 + (byte_idx-2)*8 + bit_idx]
                bit_idx = bit_idx + 1
            self.write_byte(bv)
            byte_idx = byte_idx + 1

    proc add_const_num(self, d):
        let key = "n" + str(d)
        if dict_has(self.const_map, key):
            return self.const_map[key]
        
        let c = {"type": 1, "num": d}
        push(self.global_consts, c)
        let idx = len(self.global_consts) - 1
        self.const_map[key] = idx
        return idx

    proc add_const_str(self, s):
        let key = "s" + s
        if dict_has(self.const_map, key):
            return self.const_map[key]
        
        let c = {"type": 3, "str": s}
        push(self.global_consts, c)
        let idx = len(self.global_consts) - 1
        self.const_map[key] = idx
        return idx

    proc first_pass(self, lines):
        let ut = self.utils
        var i = 0
        var chunk_count = 0
        var function_count = 0
        while i < len(lines):
            let line = ut.trim(lines[i])
            if startswith(line, "functions "):
                function_count = ut.parse_int_field(line, 10)
            elif startswith(line, "chunks "):
                chunk_count = ut.parse_int_field(line, 7)
            elif line == "chunk":
                self.current_chunk = self.current_chunk + 1
                let ltg = self.local_to_global
                let empty_ltg = []
                push(ltg, empty_ltg)
                
                let ltg_raw = self.local_to_global_raw
                let empty_raw = []
                push(ltg_raw, empty_raw)
                
                let params = self.chunk_params
                let empty_params = []
                push(params, empty_params)
            elif line == "function":
                self.current_chunk = self.current_chunk + 1
                let ltg = self.local_to_global
                let empty_ltg = []
                push(ltg, empty_ltg)
                
                let ltg_raw = self.local_to_global_raw
                let empty_raw = []
                push(ltg_raw, empty_raw)
                
                let params = self.chunk_params
                let empty_params = []
                push(params, empty_params)
                
                i = i + 1
                let pline = ut.trim(lines[i])
                let pcount = ut.parse_int_field(pline, 7)
                var pidx = 0
                while pidx < pcount:
                    i = i + 1
                    let param_len_line = ut.trim(lines[i])
                    let plen = ut.parse_int_field(param_len_line, 6)
                    i = i + 1
                    let hex = ut.trim(lines[i])
                    var p_name = ""
                    var k = 0
                    while k < plen:
                        let byte_val = ut.parse_hex_byte(hex, k * 2)
                        let ch = chr(byte_val)
                        p_name = p_name + ch
                        k = k + 1
                    
                    let params_arr = self.chunk_params[self.current_chunk]
                    push(params_arr, p_name)
                    pidx = pidx + 1
            elif startswith(line, "constants "):
                let count = ut.parse_int_field(line, 10)
                let ltg_chunk = self.local_to_global[self.current_chunk]
                let ltg_raw_chunk = self.local_to_global_raw[self.current_chunk]
                var j = 0
                while j < count:
                    push(ltg_chunk, 0)
                    push(ltg_raw_chunk, 0)
                    j = j + 1
                
                j = 0
                while j < count:
                    i = i + 1
                    let cl = ut.trim(lines[i])
                    if startswith(cl, "number "):
                        let cl_len = len(cl)
                        let num_sub = ut.my_substr(cl, 7, cl_len)
                        let num_trimmed = ut.trim(num_sub)
                        let num_val = tonumber(num_trimmed)
                        let num_idx = self.add_const_num(num_val)
                        ltg_chunk[j] = num_idx
                        ltg_raw_chunk[j] = num_idx
                    elif startswith(cl, "string "):
                        let slen = ut.parse_int_field(cl, 7)
                        i = i + 1
                        let hex = ut.trim(lines[i])
                        var s = ""
                        var k = 0
                        while k < slen:
                            let byte_val = ut.parse_hex_byte(hex, k * 2)
                            let ch = chr(byte_val)
                            s = s + ch
                            k = k + 1
                        
                        let raw_idx = self.add_const_str(s)
                        ltg_raw_chunk[j] = raw_idx
                        var param_idx = -1
                        var pi = 0
                        let params_list = self.chunk_params[self.current_chunk]
                        while pi < len(params_list):
                            if params_list[pi] == s:
                                param_idx = pi
                                pi = len(params_list)
                            else:
                                pi = pi + 1
                        
                        if param_idx >= 0:
                            let idx_str = str(param_idx)
                            let arg_name = "__arg" + idx_str
                            let arg_idx = self.add_const_str(arg_name)
                            ltg_chunk[j] = arg_idx
                        else:
                            ltg_chunk[j] = raw_idx
                    j = j + 1
            i = i + 1
        return [function_count, chunk_count]

    proc second_pass(self, lines, function_count, chunk_count, use_shebang):
        let ut = self.utils
        if use_shebang:
            self.write_string("#!/usr/bin/env sgvm\n")
        
        self.write_string("SGVM")
        self.write_byte(0x01)
        self.write_byte(0x00)
        self.write_be16(function_count)
        let const_count = len(self.global_consts)
        self.write_be16(const_count)
        var cidx = 0
        while cidx < len(self.global_consts):
            let c = self.global_consts[cidx]
            self.write_byte(c["type"])
            if c["type"] == 1:
                self.write_double(c["num"])
            else:
                let s_val = c["str"]
                let s_len = len(s_val)
                self.write_be16(s_len)
                self.write_string(c["str"])
            cidx = cidx + 1
        
        self.write_be32(chunk_count + function_count)
        
        self.current_chunk = -1
        var i = 0
        while i < len(lines):
            let line = ut.trim(lines[i])
            if line == "chunk" or line == "function":
                self.current_chunk = self.current_chunk + 1
            elif startswith(line, "code "):
                let clen = ut.parse_int_field(line, 5)
                self.write_be32(clen)
                i = i + 1
                let hex = ut.trim(lines[i])
                var j = 0
                while j < clen * 2:
                    let op = ut.parse_hex_byte(hex, j)
                    self.write_byte(op)
                    j = j + 2
                    if op == 0 or op == 5 or op == 6 or op == 7:
                        let v1 = ut.parse_hex_byte(hex, j)
                        let v2 = ut.parse_hex_byte(hex, j + 2)
                        let local_idx = v1 * 256 + v2
                        let ltg = self.local_to_global[self.current_chunk]
                        let val = ltg[local_idx]
                        self.write_be16(val)
                        j = j + 4
                    elif op == 8: # DEFINE_FUNCTION
                        let v1 = ut.parse_hex_byte(hex, j)
                        let v2 = ut.parse_hex_byte(hex, j + 2)
                        let local_idx = v1 * 256 + v2
                        let ltg_raw = self.local_to_global_raw[self.current_chunk]
                        let val = ltg_raw[local_idx]
                        self.write_be16(val)
                        let f1 = ut.parse_hex_byte(hex, j + 4)
                        let f2 = ut.parse_hex_byte(hex, j + 6)
                        self.write_be16(f1 * 256 + f2)
                        j = j + 8
                    elif op == 9 or op == 10 or op == 52 or op == 53 or op == 54:
                        let v1 = ut.parse_hex_byte(hex, j)
                        let v2 = ut.parse_hex_byte(hex, j + 2)
                        let local_idx = v1 * 256 + v2
                        let ltg_raw = self.local_to_global_raw[self.current_chunk]
                        let val = ltg_raw[local_idx]
                        self.write_be16(val)
                        j = j + 4
                    elif op == 13 or op == 35 or op == 36 or op == 39 or op == 40 or op == 41 or op == 43 or op == 51 or op == 56:
                        let v1 = ut.parse_hex_byte(hex, j)
                        let v2 = ut.parse_hex_byte(hex, j + 2)
                        self.write_be16(v1 * 256 + v2)
                        j = j + 4
                    elif op == 38: # CALL_METHOD
                        let v1 = ut.parse_hex_byte(hex, j)
                        let v2 = ut.parse_hex_byte(hex, j + 2)
                        let local_idx = v1 * 256 + v2
                        let ltg_raw = self.local_to_global_raw[self.current_chunk]
                        let val = ltg_raw[local_idx]
                        self.write_be16(val)
                        let call_arg = ut.parse_hex_byte(hex, j + 4)
                        self.write_byte(call_arg)
                        j = j + 6
                    elif op == 37: # CALL
                        let call_val = ut.parse_hex_byte(hex, j)
                        self.write_byte(call_val)
                        j = j + 2
            i = i + 1

    proc compile(self, input_file, output_file, use_shebang):
        let ut = self.utils
        var svm_file = input_file
        if endswith(input_file, ".sage"):
            let ext = ".svm"
            svm_file = input_file + ext
            var cmd = "sage --emit-vm "
            cmd = cmd + input_file
            cmd = cmd + " -o "
            cmd = cmd + svm_file
            sys_exec(cmd)
        
        let content = io_readfile(svm_file)
        if content == nil:
            print "Error: Could not read SVM file: " + svm_file
            return
        
        let lines = ut.split_lines(content)
        
        let counts = self.first_pass(lines)
        let function_count = counts[0]
        let chunk_count = counts[1]
        
        self.second_pass(lines, function_count, chunk_count, use_shebang)
        
        io_writebytes(output_file, self.output_bytes)
