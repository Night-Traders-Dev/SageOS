import io
import strings

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
            # Handle comments
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
                    let val_str = strip(kv[1])
                    # Basic number parsing (only decimal/hex)
                    # For now just assume they are sequential if no =
                    # or handle simple = values
                    ops[name] = current_val # TODO: better val parsing
                else:
                    ops[entry] = current_val
                
                current_val = current_val + 1
    return ops

let ops = parse_opcodes("sageos_build/sage_lang/core/src/vm/bytecode.h")
print("Found " + str(len(dict_keys(ops))) + " opcodes")
print("BC_OP_CONSTANT: " + str(ops["BC_OP_CONSTANT"]))
print("BC_OP_RETURN: " + str(ops["BC_OP_RETURN"]))
