import sys
import io
import sgvm_compiler

proc main():
    let args = sys.args()
    var input_file = ""
    var output_file = ""
    var use_shebang = false

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
        if a == "--shebang":
            use_shebang = true
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
    if len(positional_args) > 1:
        output_file = positional_args[1]

    if input_file == "" or output_file == "":
        print "Usage: sgvmc <input.sage|.svm> <output.sgvm> [--shebang]"
        return
    
    let compiler = sgvm_compiler.SGVMCompiler()
    compiler.compile(input_file, output_file, use_shebang)
    print "Compilation complete."

main()
