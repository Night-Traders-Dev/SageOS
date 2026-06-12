import sys
import io
from sgvm_core import SGVMUtils

proc byte_to_hex(val):
    let chars = "0123456789abcdef"
    let high = int(val / 16) % 16
    let low = int(val) % 16
    return chars[high] + chars[low]

proc pad_left(s, width, char):
    var res = s
    while len(res) < width:
        res = char + res
    return res

proc pad_right(s, width):
    var res = s
    while len(res) < width:
        res = res + " "
    return res

proc disassemble(path):
    let data = io.readbytes(path)
    if data == nil:
        print "ERROR: Could not read file " + path
        return
    
    let ut = SGVMUtils()
    var pos = 0

    # Check for shebang
    if len(data) > 0 and ut.my_int(data[0]) == 35: # ord('#') is 35
        while pos < len(data) and ut.my_int(data[pos]) != 10: # ord('\n') is 10
            pos = pos + 1
        if pos < len(data):
            pos = pos + 1

    if len(data) - pos < 4:
        print "ERROR: File too short"
        return

    var magic = ""
    var m_idx = 0
    while m_idx < 4:
        magic = magic + chr(ut.my_int(data[pos + m_idx]))
        m_idx = m_idx + 1
    pos = pos + 4

    if magic != "SGVM":
        print "ERROR: bad magic " + magic
        return
    print "Magic: SGVM"

    let ver_maj = ut.my_int(data[pos])
    let ver_min = ut.my_int(data[pos+1])
    pos = pos + 2
    print "Version: " + str(ver_maj) + "." + str(ver_min)

    let func_count = ut.my_int(ut.read_be16(data, pos))
    pos = pos + 2
    print "Functions: " + str(func_count)

    let const_count = ut.my_int(ut.read_be16(data, pos))
    pos = pos + 2
    print "Constants: " + str(const_count)

    let consts = []
    var ci = 0
    while ci < const_count:
        if pos >= len(data):
            print "ERROR: Truncated constant pool"
            return
        let ctype = ut.my_int(data[pos])
        pos = pos + 1
        if ctype == 1:
            if pos + 8 > len(data):
                print "ERROR: Truncated double constant"
                return
            let val = ut.unpack_double(data, pos)
            pos = pos + 8
            push(consts, {"type": 1, "value": val})
            print "  const[" + str(ci) + "] = NUM " + str(val)
        elif ctype == 3:
            if pos + 2 > len(data):
                print "ERROR: Truncated string constant length"
                return
            let slen = ut.my_int(ut.read_be16(data, pos))
            pos = pos + 2
            if pos + slen > len(data):
                print "ERROR: Truncated string constant value"
                return
            var s = ""
            var k = 0
            while k < slen:
                s = s + chr(ut.my_int(data[pos + k]))
                k = k + 1
            pos = pos + slen
            push(consts, {"type": 3, "value": s})
            print "  const[" + str(ci) + "] = STR '" + s + "'"
        else:
            push(consts, {"type": ctype, "value": nil})
            print "  const[" + str(ci) + "] = UNKNOWN type " + str(ctype)
        ci = ci + 1

    let chunk_total = ut.my_int(ut.read_be32(data, pos))
    pos = pos + 4
    print "Chunks+Functions: " + str(chunk_total)
    print ""

    let op_names = {
        "0": "CONSTANT",
        "1": "NIL",
        "2": "TRUE",
        "3": "FALSE",
        "4": "POP",
        "5": "GET_GLOBAL",
        "6": "DEFINE_GLOBAL",
        "7": "SET_GLOBAL",
        "8": "DEFINE_FUNCTION",
        "9": "GET_PROPERTY",
        "10": "SET_PROPERTY",
        "11": "GET_INDEX",
        "12": "SET_INDEX",
        "13": "LOAD_FUNCTION",
        "14": "SLICE",
        "15": "ADD",
        "16": "SUB",
        "17": "MUL",
        "18": "DIV",
        "19": "MOD",
        "20": "NEGATE",
        "21": "EQUAL",
        "22": "NOT_EQUAL",
        "23": "GREATER",
        "24": "GREATER_EQUAL",
        "25": "LESS",
        "26": "LESS_EQUAL",
        "27": "BIT_AND",
        "28": "BIT_OR",
        "29": "BIT_XOR",
        "30": "BIT_NOT",
        "31": "SHIFT_LEFT",
        "32": "SHIFT_RIGHT",
        "33": "NOT",
        "34": "TRUTHY",
        "35": "JUMP",
        "36": "JUMP_IF_FALSE",
        "37": "CALL",
        "38": "CALL_METHOD",
        "39": "ARRAY",
        "40": "TUPLE",
        "41": "DICT",
        "42": "PRINT",
        "43": "EXEC_AST_STMT",
        "44": "RETURN",
        "45": "PUSH_ENV",
        "46": "POP_ENV",
        "47": "DUP",
        "48": "ARRAY_LEN",
        "49": "BREAK",
        "50": "CONTINUE",
        "51": "LOOP_BACK",
        "52": "IMPORT",
        "53": "CLASS",
        "54": "METHOD",
        "55": "INHERIT",
        "56": "SETUP_TRY",
        "57": "END_TRY",
        "58": "RAISE",
        "59": "GPU_POLL_EVENTS",
        "60": "GPU_WINDOW_SHOULD_CLOSE",
        "61": "GPU_GET_TIME",
        "62": "GPU_KEY_PRESSED",
        "63": "GPU_KEY_DOWN",
        "64": "GPU_MOUSE_POS",
        "65": "GPU_MOUSE_DELTA",
        "66": "GPU_UPDATE_INPUT",
        "67": "GPU_BEGIN_COMMANDS",
        "68": "GPU_END_COMMANDS",
        "69": "GPU_CMD_BEGIN_RP",
        "70": "GPU_CMD_END_RP",
        "71": "GPU_CMD_DRAW",
        "72": "GPU_CMD_BIND_GP",
        "73": "GPU_CMD_BIND_DS",
        "74": "GPU_CMD_SET_VP",
        "75": "GPU_CMD_SET_SC",
        "76": "GPU_CMD_BIND_VB",
        "77": "GPU_CMD_BIND_IB",
        "78": "GPU_CMD_DRAW_IDX",
        "79": "GPU_SUBMIT_SYNC",
        "80": "GPU_ACQUIRE_IMG",
        "81": "GPU_PRESENT",
        "82": "GPU_WAIT_FENCE",
        "83": "GPU_RESET_FENCE",
        "84": "GPU_UPDATE_UNIFORM",
        "85": "GPU_CMD_PUSH_CONST",
        "86": "GPU_CMD_DISPATCH",
        "255": "HALT"
    }

    let op_operands = {
        "0": "const2",
        "5": "const2",
        "6": "const2",
        "7": "const2",
        "8": "defn",
        "9": "raw2",
        "10": "raw2",
        "13": "raw2",
        "35": "raw2",
        "36": "raw2",
        "37": "1",
        "38": "callm",
        "39": "raw2",
        "40": "raw2",
        "41": "raw2",
        "43": "raw2",
        "47": "1",
        "49": "raw2",
        "50": "raw2",
        "51": "raw2",
        "52": "raw2",
        "53": "raw2",
        "54": "raw2",
        "56": "raw2"
    }

    proc const_label(idx):
        if idx >= 0 and idx < len(consts):
            let c = consts[idx]
            let ct = ut.my_int(c["type"])
            let cv = c["value"]
            if ct == 3:
                return str(idx) + "('" + str(cv) + "')"
            if ct == 1:
                return str(idx) + "(" + str(cv) + ")"
        return str(idx)

    var chunk_idx = 0
    while chunk_idx < chunk_total:
        if pos + 4 > len(data):
            print "ERROR: Truncated chunk header"
            return
        let chunk_len = ut.my_int(ut.read_be32(data, pos))
        pos = pos + 4
        print "--- Chunk " + str(chunk_idx) + " (" + str(chunk_len) + " bytes) ---"
        let chunk_end_pos = pos + chunk_len
        var ip = 0
        while pos < chunk_end_pos:
            let op = ut.my_int(data[pos])
            pos = pos + 1
            ip = ip + 1
            let key = str(op)
            var op_name = "OP_" + key
            if dict_has(op_names, key):
                op_name = op_names[key]
            
            var operand_type = nil
            if dict_has(op_operands, key):
                operand_type = op_operands[key]
            
            let ip_str = pad_left(str(ip - 1), 4, " ")
            let op_str = byte_to_hex(op)
            let name_str = pad_right(op_name, 20)

            if operand_type == nil:
                print "  " + ip_str + "  " + op_str + "  " + name_str
            elif operand_type == "const2":
                let idx = ut.my_int(ut.read_be16(data, pos))
                pos = pos + 2
                ip = ip + 2
                print "  " + ip_str + "  " + op_str + "  " + name_str + " " + const_label(idx)
            elif operand_type == "raw2":
                let val = ut.my_int(ut.read_be16(data, pos))
                pos = pos + 2
                ip = ip + 2
                var label = str(val)
                if op == 52 or op == 53 or op == 54 or op == 9 or op == 10:
                    label = const_label(val)
                print "  " + ip_str + "  " + op_str + "  " + name_str + " " + label
            elif operand_type == "1":
                let val = ut.my_int(data[pos])
                pos = pos + 1
                ip = ip + 1
                print "  " + ip_str + "  " + op_str + "  " + name_str + " " + str(val)
            elif operand_type == "defn":
                let name_idx = ut.my_int(ut.read_be16(data, pos))
                pos = pos + 2
                ip = ip + 2
                let chunk_ref = ut.my_int(ut.read_be16(data, pos))
                pos = pos + 2
                ip = ip + 2
                print "  " + ip_str + "  " + op_str + "  " + name_str + " name=" + const_label(name_idx) + " chunk=" + str(chunk_ref)
            elif operand_type == "callm":
                let name_idx = ut.my_int(ut.read_be16(data, pos))
                pos = pos + 2
                ip = ip + 2
                let argc = ut.my_int(data[pos])
                pos = pos + 1
                ip = ip + 1
                print "  " + ip_str + "  " + op_str + "  " + name_str + " name=" + const_label(name_idx) + " argc=" + str(argc)
        print ""
        chunk_idx = chunk_idx + 1

proc main():
    let args = sys.args()
    var input_file = ""
    
    var is_interpreter = false
    if args[0] == "sage":
        is_interpreter = true
    elif endswith(args[0], "/sage"):
        is_interpreter = true
    elif endswith(args[0], "\\sage"):
        is_interpreter = true
    elif endswith(args[0], "sage.exe"):
        is_interpreter = true

    var positional_args = []
    var i = 0
    while i < len(args):
        let a = args[i]
        var should_skip = false
        if i == 0:
            should_skip = true
        elif i == 1:
            if is_interpreter:
                should_skip = true
        
        if not should_skip:
            push(positional_args, a)
        i = i + 1

    if len(positional_args) > 0:
        input_file = positional_args[0]
    
    if input_file == "":
        print "Usage: sage tools/sgvm_hexdump.sage <file.sgvm>"
        return
    disassemble(input_file)

main()
