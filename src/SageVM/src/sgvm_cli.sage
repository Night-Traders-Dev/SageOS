import sys
import io
import sgvm_runner
import sgvm_compiler
import sgvm_disassembler_logic
import sgvm_hexdump_logic
import srvm_runner
import srvm_compiler
import srvm_disassembler_logic
import srvm_hexdump_logic
from sgvm_compiler import sys_exec, io_readfile, io_writebytes

proc print_help():
    print "Usage: sagevm <command> [options]"
    print ""
    print "Commands:"
    print "  run <file.sgvm> [--debug] [--riscv]   Execute a compiled binary"
    print "  compile <input.sage> [out] [--riscv]  Compile Sage source to binary"
    print "  dis <file.sgvm> [--riscv]             Disassemble binary"
    print "  hex <file.sgvm> [--riscv]             Low-level binary hexdump"
    print "  version                               Show version information"
    print ""
    print "Use 'sagevm <command> --help' for command-specific options."

class SGVMCLI:
    proc init(self):
        # Dispatcher for SGVM tools
        return nil

    proc run(self):
        # In compiled binary, sys might be shadowed or nil in some scopes
        # Try to use it directly
        let args = sys.args()
        var cmd = ""
        if len(args) >= 2:
            cmd = args[1]
        
        # Check if called via symlink
        let binary_name = args[0]
        if endswith(binary_name, "sgvm"):
            self.handle_run(args, 1)
            return
        elif endswith(binary_name, "sgvmc"):
            self.handle_compile(args, 1)
            return

        if cmd == "":
            print_help()
            return

        if cmd == "run":
            self.handle_run(args, 2)
        elif cmd == "compile":
            self.handle_compile(args, 2)
        elif cmd == "dis":
            self.handle_dis(args, 2)
        elif cmd == "hex":
            self.handle_hex(args)
        elif cmd == "version":
            print "SageVM v0.9.7"
        elif cmd == "--help" or cmd == "-h" or cmd == "help":
            print_help()
        else:
            print "Unknown command: " + cmd
            print_help()

    proc handle_run(self, args, start_idx):
        var input_file = ""
        var debug = false
        var safe = false
        var no_ffi = false
        var riscv = false
        var i = start_idx
        while i < len(args):
            let a = args[i]
            if a == "--debug": debug = true
            elif a == "--safe": safe = true
            elif a == "--no-ffi": no_ffi = true
            elif a == "--riscv": riscv = true
            else: input_file = a
            i = i + 1
        
        if input_file == "":
            print "Usage: sagevm run <file.sgvm> [--debug] [--safe] [--no-ffi] [--riscv]"
            return
        
        # Auto-detect RISC-V header
        let data = io.readbytes(input_file)
        if data != nil and len(data) >= 4:
            if int(data[0]) == 83 and int(data[1]) == 71 and int(data[2]) == 82 and int(data[3]) == 86:
                riscv = true

        if riscv:
            let runner = srvm_runner.SRVMRunner()
            runner.run_file(input_file, debug)
        else:
            let runner = sgvm_runner.SGVMRunner()
            runner.run_file(input_file, debug, safe, not no_ffi)

    proc handle_compile(self, args, start_idx):
        var input_file = ""
        var output_file = ""
        var use_shebang = false
        var riscv = false
        var pos_idx = 0
        var iter_idx = start_idx
        while iter_idx < len(args):
            let a = args[iter_idx]
            if a == "--shebang": use_shebang = true
            elif a == "--riscv": riscv = true
            else:
                if pos_idx == 0: input_file = a
                elif pos_idx == 1: output_file = a
                pos_idx = pos_idx + 1
            iter_idx = iter_idx + 1
        
        if input_file == "":
            print "Usage: sagevm compile <input.sage> [output.sgvm] [--shebang] [--riscv]"
            return
        
        if output_file == "":
            if riscv: output_file = input_file + ".sgrv"
            else: output_file = input_file + ".sgvm"
        
        let compiler = sgvm_compiler.SGVMCompiler()
        if compiler.compile(input_file, output_file, use_shebang):
            if riscv:
                # Post-process: Translate SVM to SRVM
                let svm_data = io.readbytes(output_file)
                let rv_compiler = srvm_compiler.SGRVCompiler()
                let sgrv_data = rv_compiler.compile(svm_data)
                if sgrv_data != nil:
                    io_writebytes(output_file, sgrv_data)
                    print "✨ RISC-V translation complete."
                else:
                    print "❌ RISC-V translation failed."
            print "✨ Compilation complete: " + output_file
        else:
            print "❌ Compilation failed."

    proc handle_dis(self, args, start_idx):
        var input_file = ""
        var mode = "sage"
        var riscv = false
        var i = start_idx
        while i < len(args):
            let a = args[i]
            if a == "--svm": mode = "svm"
            elif a == "--sage": mode = "sage"
            elif a == "--riscv": riscv = true
            else: input_file = a
            i = i + 1
        
        if input_file == "":
            print "Usage: sagevm dis <file.sgvm> [--sage | --svm] [--riscv]"
            return
        
        # Auto-detect RISC-V header
        let data = io.readbytes(input_file)
        if data != nil and len(data) >= 4:
            if int(data[0]) == 83 and int(data[1]) == 71 and int(data[2]) == 82 and int(data[3]) == 86:
                riscv = true

        if riscv:
            let dis = srvm_disassembler_logic.SRVMDisassembler(input_file)
            if dis.disassemble():
                dis.generate_sage()
        else:
            let dis = sgvm_disassembler_logic.SGVMDisassembler(input_file)
            if dis.disassemble():
                if mode == "svm": print dis.generate_svm()
                else: print dis.generate_sage()

    proc handle_hex(self, args):
        var input_file = ""
        var riscv = false
        var i = 2
        while i < len(args):
            let a = args[i]
            if a == "--riscv": riscv = true
            else: input_file = a
            i = i + 1

        if input_file == "":
            print "Usage: sagevm hex <file.sgvm> [--riscv]"
            return
        
        # Auto-detect RISC-V header
        let data = io.readbytes(input_file)
        if data != nil and len(data) >= 4:
            if int(data[0]) == 83 and int(data[1]) == 71 and int(data[2]) == 82 and int(data[3]) == 86:
                riscv = true

        if riscv:
            srvm_hexdump_logic.srvm_disassemble(input_file)
        else:
            sgvm_hexdump_logic.disassemble(input_file)
