import io
import strings
import sys
import math

## SGVM Compiler — Pure Sage Implementation
## Translates human-readable .svm bytecode files into optimized binary .sgvm artifacts.
## Also supports generating C headers for kernel embedding.
##
## Usage: sage scripts/compile_to_sgvm.sage <input.svm> <output.sgvm> [--header <output.h> <array_name>]

proc hex_to_int(c):
    if c == "0": return 0
    if c == "1": return 1
    if c == "2": return 2
    if c == "3": return 3
    if c == "4": return 4
    if c == "5": return 5
    if c == "6": return 6
    if c == "7": return 7
    if c == "8": return 8
    if c == "9": return 9
    if c == "a" or c == "A": return 10
    if c == "b" or c == "B": return 11
    if c == "c" or c == "C": return 12
    if c == "d" or c == "D": return 13
    if c == "e" or c == "E": return 14
    if c == "f" or c == "F": return 15
    return 0

proc decode_hex(hex_str):
    let res = []
    let i = 0
    while i < len(hex_str):
        let hi = hex_to_int(hex_str[i])
        let lo = hex_to_int(hex_str[i+1])
        push(res, hi * 16 + lo)
        i = i + 2
    return res

proc pack_u16_le(v):
    return [v & 0xFF, (v >> 8) & 0xFF]

proc pack_u32_le(v):
    return [v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, (v >> 24) & 0xFF]

proc pack_f64_le(v):
    return math.pack64(v)

proc fnv1a(data_bytes):
    let h = 2166136261
    for b in data_bytes:
        h = h ^ b
        h = (h * 16777619) & 0xFFFFFFFF
    return h

proc parse_opcodes(path):
    let content = io.readfile(path)
    let lines = split(content, "\n")
    let ops = {}
    let in_enum = false
    let current_val = 0
    
    for line in lines:
        let trimmed = strip(line)
        if contains(trimmed, "typedef enum {"):
            in_enum = true
            continue
        if in_enum and contains(trimmed, "}"):
            if contains(trimmed, "BytecodeOp;"):
                break
        
        if in_enum:
            let parts = split(trimmed, "//")
            let code = parts[0]
            if len(code) == 0: continue
            
            let comma_parts = split(code, ",")
            for p in comma_parts:
                let entry = strip(p)
                if len(entry) == 0: continue
                
                if contains(entry, "="):
                    let kv = split(entry, "=")
                    let name = strip(kv[0])
                    ops[name] = current_val 
                else:
                    ops[entry] = current_val
                
                current_val = current_val + 1
    return ops

proc parse_sagebc(path):
    let content = io.readfile(path)
    let lines = split(content, "\n")
    
    if len(lines) == 0 or strip(lines[0]) != "SAGEBC1":
        print("Error: Invalid SAGEBC1 header")
        return nil
    
    let functions = []
    let chunks = []
    
    let i = 1
    while i < len(lines):
        let line = strip(lines[i])
        if startswith(line, "functions "):
            let count = tonumber(split(line, " ")[1])
            i = i + 1
            let f_idx = 0
            while f_idx < count:
                if strip(lines[i]) != "function": 
                    print("Error: Expected function at line " + str(i))
                    return nil
                i = i + 1
                let params_count = tonumber(split(strip(lines[i]), " ")[1])
                i = i + 1
                let params = []
                let p_idx = 0
                while p_idx < params_count:
                    i = i + 1 # skip param <len>
                    push(params, decode_hex(strip(lines[i])))
                    i = i + 1
                    p_idx = p_idx + 1
                
                let consts = []
                let c_line = strip(lines[i])
                let c_count = tonumber(split(c_line, " ")[1])
                i = i + 1
                let c_idx = 0
                while c_idx < c_count:
                    let cl = strip(lines[i])
                    if startswith(cl, "number "):
                        push(consts, ["num", tonumber(split(cl, " ")[1])])
                        i = i + 1
                    elif startswith(cl, "string "):
                        i = i + 1
                        push(consts, ["str", decode_hex(strip(lines[i]))])
                        i = i + 1
                    c_idx = c_idx + 1
                
                let code_line = strip(lines[i])
                let code_len = tonumber(split(code_line, " ")[1])
                i = i + 1
                let code_data = decode_hex(strip(lines[i]))
                i = i + 1
                if strip(lines[i]) != "endfunction":
                    print("Warning: Expected endfunction at line " + str(i))
                i = i + 1
                push(functions, {"params": params, "consts": consts, "code": code_data})
                f_idx = f_idx + 1
        
        elif startswith(line, "chunks "):
            let count = tonumber(split(line, " ")[1])
            i = i + 1
            let ch_idx = 0
            while ch_idx < count:
                i = i + 1 # skip chunk
                let consts = []
                let c_count = tonumber(split(strip(lines[i]), " ")[1])
                i = i + 1
                let c_idx = 0
                while c_idx < c_count:
                    let cl = strip(lines[i])
                    if startswith(cl, "number "):
                        push(consts, ["num", tonumber(split(cl, " ")[1])])
                        i = i + 1
                    elif startswith(cl, "string "):
                        i = i + 1
                        push(consts, ["str", decode_hex(strip(lines[i]))])
                        i = i + 1
                    c_idx = c_idx + 1
                
                let code_line = strip(lines[i])
                let code_len = tonumber(split(code_line, " ")[1])
                i = i + 1
                let code_data = decode_hex(strip(lines[i]))
                i = i + 1
                i = i + 1 # skip endchunk
                push(chunks, {"consts": consts, "code": code_data})
                ch_idx = ch_idx + 1
        else:
            i = i + 1
            
    return {"functions": functions, "chunks": chunks}

proc pack_consts(consts):
    let res = pack_u16_le(len(consts))
    for c in consts:
        let type = c[0]
        let val = c[1]
        if type == "num":
            push(res, 1) # MV_NUMBER
            let packed = pack_f64_le(val)
            for b in packed: push(res, b)
        else:
            push(res, 3) # MV_STRING
            let packed_len = pack_u16_le(len(val))
            for b in packed_len: push(res, b)
            for b in val: push(res, b)
    return res

proc int_to_hex(n):
    let hex = "0123456789abcdef"
    return hex[(n >> 4) & 0x0F] + hex[n & 0x0F]

proc main():
    let args = sys.args()
    if len(args) < 4:
        print("Usage: sage scripts/compile_to_sgvm.sage <input.svm> <output.sgvm> [--header <output.h> <array_name>]")
        return 1
    
    let src_path = args[2]
    let dest_path = args[3]
    
    let header_path = nil
    let array_name = ""
    
    let a_idx = 4
    while a_idx < len(args):
        if args[a_idx] == "--header":
            header_path = args[a_idx + 1]
            array_name = args[a_idx + 2]
            a_idx = a_idx + 3
        else:
            a_idx = a_idx + 1
    
    let ops = parse_opcodes("sageos_build/sage_lang/core/src/vm/bytecode.h")
    let data = parse_sagebc(src_path)
    if data == nil: return 1
    
    let functions = data["functions"]
    let chunks = data["chunks"]
    
    let blob = [83, 71, 86, 77, 2] # "SGVM" + version 2
    
    let main_consts = []
    let main_code = []
    
    let op_const = ops["BC_OP_CONSTANT"]
    let op_get_g = ops["BC_OP_GET_GLOBAL"]
    let op_def_g = ops["BC_OP_DEFINE_GLOBAL"]
    let op_set_g = ops["BC_OP_SET_GLOBAL"]
    let op_get_p = ops["BC_OP_GET_PROPERTY"]
    let op_set_p = ops["BC_OP_SET_PROPERTY"]
    let op_exec_ast = ops["BC_OP_EXEC_AST_STMT"]
    let op_def_fn = ops["BC_OP_DEFINE_FUNCTION"]
    let op_load_fn = ops["BC_OP_LOAD_FUNCTION"]
    let op_call_m = ops["BC_OP_CALL_METHOD"]
    let op_class = ops["BC_OP_CLASS"]
    let op_method = ops["BC_OP_METHOD"]
    let op_return = ops["BC_OP_RETURN"]

    for chunk in chunks:
        let base = len(main_consts)
        for c in chunk["consts"]: push(main_consts, c)
        let code = chunk["code"]
        
        if len(code) > 0 and code[len(code)-1] == op_return:
            pop(code)
            
        let pc = 0
        while pc < len(code):
            let op = code[pc]
            if op == op_const or op == op_get_g or op == op_def_g or op == op_set_g or op == op_get_p or op == op_set_p or op == op_exec_ast or op == op_method or op == op_load_fn:
                let idx = (code[pc+1] << 8) | code[pc+2]
                let new_idx = idx + base
                code[pc+1] = (new_idx >> 8) & 0xFF
                code[pc+2] = new_idx & 0xFF
                pc = pc + 3
            elif op == op_def_fn:
                let idx = (code[pc+1] << 8) | code[pc+2]
                let new_idx = idx + base
                code[pc+1] = (new_idx >> 8) & 0xFF
                code[pc+2] = new_idx & 0xFF
                pc = pc + 5
            elif op == op_call_m:
                let idx = (code[pc+1] << 8) | code[pc+2]
                let new_idx = idx + base
                code[pc+1] = (new_idx >> 8) & 0xFF
                code[pc+2] = new_idx & 0xFF
                pc = pc + 4
            elif op == op_class:
                let idx = (code[pc+1] << 8) | code[pc+2]
                let new_idx = idx + base
                code[pc+1] = (new_idx >> 8) & 0xFF
                code[pc+2] = new_idx & 0xFF
                let has_parent = code[pc+5]
                pc = pc + 6
                if has_parent != 0:
                    let pidx = (code[pc] << 8) | code[pc+1]
                    let new_pidx = pidx + base
                    code[pc] = (new_pidx >> 8) & 0xFF
                    code[pc+1] = new_pidx & 0xFF
                    pc = pc + 2
            elif op == ops["BC_OP_JUMP"] or op == ops["BC_OP_JUMP_IF_FALSE"] or op == ops["BC_OP_ARRAY"] or op == ops["BC_OP_TUPLE"] or op == ops["BC_OP_DICT"] or op == ops["BC_OP_SETUP_TRY"]:
                pc = pc + 3
            elif op == ops["BC_OP_CALL"] or op == ops["BC_OP_DUP"] or op == ops["BC_OP_BREAK"] or op == ops["BC_OP_CONTINUE"] or op == ops["BC_OP_LOOP_BACK"]:
                pc = pc + 2
            else:
                pc = pc + 1
        
        for b in code: push(main_code, b)

    push(main_code, op_return)
    
    let pc_bytes = pack_consts(main_consts)
    for b in pc_bytes: push(blob, b)
    
    let p_code_len = pack_u32_le(len(main_code))
    for b in p_code_len: push(blob, b)
    for b in main_code: push(blob, b)
    
    let p_fn_count = pack_u16_le(len(functions))
    for b in p_fn_count: push(blob, b)
    
    for f in functions:
        let p_params_count = pack_u16_le(len(f["params"]))
        for b in p_params_count: push(blob, b)
        for p in f["params"]:
            let hash = fnv1a(p)
            let p_hash = pack_u32_le(hash)
            for b in p_hash: push(blob, b)
        
        let pf_consts = pack_consts(f["consts"])
        for b in pf_consts: push(blob, b)
        
        let pf_code_len = pack_u32_le(len(f["code"]))
        for b in pf_code_len: push(blob, b)
        for b in f["code"]: push(blob, b)
        
    io.writebytes(dest_path, blob)
    print("Compiled " + src_path + " to " + dest_path + " (" + str(len(blob)) + " bytes)")
    
    if header_path != nil:
        let h_content = "/* Auto-generated binary SGVM artifact — DO NOT EDIT */\n"
        h_content = h_content + "#pragma once\n"
        h_content = h_content + "#include <stdint.h>\n"
        h_content = h_content + "static const uint8_t " + array_name + "[] = {\n"
        
        let row_count = 0
        for b in blob:
            if row_count == 0: h_content = h_content + "    "
            h_content = h_content + "0x" + int_to_hex(b) + ", "
            row_count = row_count + 1
            if row_count == 16:
                h_content = h_content + "\n"
                row_count = 0
        
        if row_count != 0: h_content = h_content + "\n"
        h_content = h_content + "};\n"
        h_content = h_content + "static const int " + array_name + "_len = " + str(len(blob)) + ";\n"
        
        io.writefile(header_path, h_content)
        print("Generated C header: " + header_path)
        
    return 0

main()
