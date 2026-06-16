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

proc hex_dump(data, width):
    let lines = []
    let ut = SGVMUtils()
    var i = 0
    while i < len(data):
        var chunk_len = width
        if i + chunk_len > len(data):
            chunk_len = len(data) - i
        
        var hex_part = ""
        var ascii_part = ""
        var j = 0
        while j < chunk_len:
            let b = ut.my_int(data[i + j])
            hex_part = hex_part + byte_to_hex(b) + " "
            if b >= 32 and b < 127:
                ascii_part = ascii_part + chr(b)
            else:
                ascii_part = ascii_part + "."
            j = j + 1
        
        while len(hex_part) < width * 3:
            hex_part = hex_part + " "
        
        let offset_str = pad_left(byte_to_hex(ut.my_int(i / 65536) % 256) + byte_to_hex(ut.my_int(i / 256) % 256) + byte_to_hex(i % 256), 6, "0")
        push(lines, "  " + offset_str + "  " + hex_part + "  " + ascii_part)
        i = i + width
    return lines

proc diff_diag(path_a, path_b):
    let content_a = io_readfile(path_a)
    let content_b = io_readfile(path_b)
    if content_a == nil or content_b == nil:
        print "ERROR: Could not read DIAG files"
        return
    
    let ut = SGVMUtils()
    let raw_a = ut.split_lines(content_a)
    let raw_b = ut.split_lines(content_b)

    let a = []
    let b = []
    
    var idx = 0
    while idx < len(raw_a):
        let line = ut.trim(raw_a[idx])
        if startswith(line, "DIAG "):
            push(a, line)
        idx = idx + 1
        
    idx = 0
    while idx < len(raw_b):
        let line = ut.trim(raw_b[idx])
        if startswith(line, "DIAG "):
            push(b, line)
        idx = idx + 1

    var max_len = len(a)
    if len(b) > max_len:
        max_len = len(b)
        
    var first_diff = -1
    var diff_count = 0

    print pad_right("Step", 6) + " " + pad_right("INTERPRETED", 60) + " " + pad_right("COMPILED", 60) + " MATCH"
    print "----------------------------------------------------------------------------------------------------------------------------------"

    var i = 0
    while i < max_len:
        var la = "<MISSING>"
        if i < len(a):
            la = a[i]
        var lb = "<MISSING>"
        if i < len(b):
            lb = b[i]
            
        var match_result = "OK"
        if la != lb:
            match_result = "DIFF"
            diff_count = diff_count + 1
            if first_diff == -1:
                first_diff = i
                
        var marker = "   "
        if match_result == "DIFF":
            marker = "***"
            
        let step_str = pad_right(marker + pad_left(str(i), 3, " "), 6)
        let la_str = pad_right(la, 60)
        let lb_str = pad_right(lb, 60)
        print step_str + " " + la_str + " " + lb_str + " " + match_result
        i = i + 1

    print ""
    if diff_count == 0:
        print "[PASS] All DIAG lines match. Compiled and interpreted outputs are identical."
    else:
        print "[FAIL] " + str(diff_count) + " differences found. First divergence at step " + str(first_diff) + "."
        var first_la = "<MISSING>"
        if first_diff < len(a):
            first_la = a[first_diff]
        var first_lb = "<MISSING>"
        if first_diff < len(b):
            first_lb = b[first_diff]
        print "       Interpreted: " + first_la
        print "       Compiled:    " + first_lb
        print ""
        print "Root cause hint: check the opcode and operand values at the divergence step."
        print "If j_after differs, the compiled binary is mis-advancing the stream pointer."
        print "If global_idx differs, the compiled const map lookup produces wrong results."

proc diff_hex(path_a, path_b):
    let data_a = io.readbytes(path_a)
    let data_b = io.readbytes(path_b)
    if data_a == nil or data_b == nil:
        print "ERROR: Could not read binary files"
        return
        
    let lines_a = hex_dump(data_a, 16)
    let lines_b = hex_dump(data_b, 16)

    print "File A: " + path_a + " (" + str(len(data_a)) + " bytes)"
    print "File B: " + path_b + " (" + str(len(data_b)) + " bytes)"
    print ""

    var max_lines = len(lines_a)
    if len(lines_b) > max_lines:
        max_lines = len(lines_b)
        
    var diff_count = 0
    var first_diff_offset = -1

    print pad_right("OFFSET", 10) + " " + pad_right("INTERPRETED", 55) + " " + pad_right("COMPILED", 55) + " MATCH"
    print "----------------------------------------------------------------------------------------------------------------------------------"
    
    let ut = SGVMUtils()
    var i = 0
    while i < max_lines:
        var la = "<SHORT>"
        if i < len(lines_a):
            la = lines_a[i]
        var lb = "<SHORT>"
        if i < len(lines_b):
            lb = lines_b[i]
            
        let is_row_match = (la == lb)
        if not is_row_match:
            diff_count = diff_count + 1
            if first_diff_offset == -1:
                first_diff_offset = i * 16
                
        var marker = "   "
        if not is_row_match:
            marker = "***"
            
        var lb_part = lb
        if len(lb) > 9:
            lb_part = ut.my_substr(lb, 9, len(lb) - 9)
            
        print marker + "  " + la + "   |   " + lb_part
        i = i + 1

    print ""
    if diff_count == 0:
        print "[PASS] Bytecode files are identical."
    else:
        var offset_hex = pad_left(byte_to_hex(ut.my_int(first_diff_offset / 256) % 256) + byte_to_hex(first_diff_offset % 256), 4, "0")
        print "[FAIL] " + str(diff_count) + " differing 16-byte rows. First difference at offset 0x" + offset_hex + " (" + str(first_diff_offset) + ")."
        if len(data_a) != len(data_b):
            print "       Size mismatch: A=" + str(len(data_a)) + ", B=" + str(len(data_b)) + " bytes."

proc main():
    let args = sys.args()
    var file_a = ""
    var file_b = ""
    var hex_mode = false
    
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
        var is_flag = false
        if a == "--hex":
            hex_mode = true
            is_flag = true
        
        if not is_flag:
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
        file_a = positional_args[0]
    if len(positional_args) > 1:
        file_b = positional_args[1]
        
    if file_a == "" or file_b == "":
        print "Usage: sage tools/diff_bytecode.sage <file_a> <file_b> [--hex]"
        return
        
    if hex_mode:
        diff_hex(file_a, file_b)
    else:
        diff_diag(file_a, file_b)

main()
