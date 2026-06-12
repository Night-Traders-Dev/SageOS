import sys
import io
import sgvm_vm
from sgvm_core import SGVMUtils

proc main():
    var args = sys.args()
    var input_file = ""
    var debug = false

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
        if a == "--debug":
            debug = true
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
        input_file = positional_args[0]

    if input_file == "":
        print "Usage: sgvm <file.sgvm> [--debug]"
        return
    var data = io.readbytes(input_file)
    if data == nil:
        print "Error: Could not read file"
        return
    var off = 0
    let core_utils = SGVMUtils()
    if len(data) > 2 and core_utils.my_int(data[0]) == 35 and core_utils.my_int(data[1]) == 33:
        while off < len(data) and core_utils.my_int(data[off]) != 10:
            off = off + 1
        if off < len(data):
            off = off + 1
    if len(data) - off < 4 or core_utils.my_int(data[off]) != 83 or core_utils.my_int(data[off+1]) != 71 or core_utils.my_int(data[off+2]) != 86 or core_utils.my_int(data[off+3]) != 77:
        print "Error: Invalid SGVM header"
        return
    var metal_vm = sgvm_vm.MetalVM()
    metal_vm.trace = debug
    off = off + 6
    if off + 4 > len(data):
        print "Error: Truncated SGVM file header"
        return
    var function_count = core_utils.my_int(core_utils.read_be16(data, off))
    off = off + 2
    var const_count = core_utils.my_int(core_utils.read_be16(data, off))
    off = off + 2
    var j = 0
    while j < const_count:
        if off >= len(data):
            print "Error: Truncated constant pool"
            return
        var t = data[off]
        off = off + 1
        if t == 1:
            if off + 8 > len(data):
                print "Error: Truncated double constant"
                return
            push(metal_vm.constants, core_utils.unpack_double(data, off))
            off = off + 8
        elif t == 3:
            if off + 2 > len(data):
                print "Error: Truncated string constant length"
                return
            var slen = core_utils.my_int(core_utils.read_be16(data, off))
            off = off + 2
            if off + slen > len(data):
                print "Error: Truncated string constant value"
                return
            var s = ""
            var k = 0
            while k < slen:
                s = s + chr(core_utils.my_int(data[off + k]))
                k = k + 1
            push(metal_vm.constants, s)
            off = off + slen
        else:
            print "Error: Invalid constant type: " + str(t)
            return
        j = j + 1
    if debug:
        print "Constants count: " + str(len(metal_vm.constants))
        var c_idx = 0
        while c_idx < len(metal_vm.constants):
            print "Const " + str(c_idx) + ": " + str(metal_vm.constants[c_idx])
            c_idx = c_idx + 1
        print "data len: " + str(len(data)) + " off: " + str(off)
    if off + 4 > len(data):
        print "Error: Truncated chunk count"
        return
    var chunk_count = core_utils.my_int(core_utils.read_be32(data, off))
    off = off + 4
    var c = 0
    while c < chunk_count:
        if off + 4 > len(data):
            print "Error: Truncated chunk header"
            return
        var clen = core_utils.my_int(core_utils.read_be32(data, off))
        off = off + 4
        if off + clen > len(data):
            print "Error: Truncated chunk data"
            return
        var chunk_code = []
        var k = 0
        while k < clen:
            push(chunk_code, data[off + k])
            k = k + 1
        push(metal_vm.chunks, chunk_code)
        off = off + clen
        c = c + 1
    if debug:
        print "Functions count: " + str(function_count)
        print "Chunks count: " + str(len(metal_vm.chunks))
    var idx = function_count
    while idx < len(metal_vm.chunks) and not metal_vm.is_throwing:
        metal_vm.run(metal_vm.chunks[idx])
        idx = idx + 1

main()
