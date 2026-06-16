# Sage RISC-V (SRVM) Runner
# Loads and executes .sgrv binary files

import io
import srvm_vm
import srvm_core

class SRVMRunner:
    proc init(self):
        self.vm = srvm_vm.SRVM()

    proc run_file(self, input_file, debug=false):
        print "DEBUG: SRVMRunner.run_file called for " + input_file
        var data = io.readbytes(input_file)
        if data == nil:
            print "DEBUG: data is NIL"
            print "❌ Error: Could not read file: " + input_file
            return false
        print "DEBUG: data len=" + str(len(data))
        print "DEBUG: header=" + str(int(data[0])) + " " + str(int(data[1])) + " " + str(int(data[2])) + " " + str(int(data[3]))
        
        if len(data) < 4 or int(data[0]) != 83 or int(data[1]) != 71 or int(data[2]) != 82 or int(data[3]) != 86:
            print "❌ Error: Invalid SGRV header in " + input_file
            return false
            
        self.vm.trace = debug
        var off = 6 # Magic (4) + Version (2)
        print "DEBUG: loading constants..."
        
        # Load Constants
        let const_count = (int(data[off]) << 8) | int(data[off+1])
        print "DEBUG: loader const_count=" + str(const_count)
        off = off + 2
        
        let ut = srvm_core.SRVMUtils()
        
        var j = 0
        while j < const_count:
            var t = int(data[off])
            off = off + 1
            if t == 1: # Number
                let val = ut.unpack_double(data, off)
                push(self.vm.state.constants, val) 
                off = off + 8
            elif t == 3: # String
                let slen = (int(data[off]) << 8) | int(data[off+1])
                off = off + 2
                var s = ""
                var k = 0
                while k < slen:
                    s = s + chr(int(data[off + k]))
                    k = k + 1
                push(self.vm.state.constants, s)
                off = off + slen
            j = j + 1
            
        # Load Chunks
        let num_chunks = (int(data[off]) << 24) | (int(data[off+1]) << 16) | (int(data[off+2]) << 8) | int(data[off+3])
        off = off + 4
        
        var chunk_idx = 0
        while chunk_idx < num_chunks:
            let clen = (int(data[off]) << 24) | (int(data[off+1]) << 16) | (int(data[off+2]) << 8) | int(data[off+3])
            off = off + 4
            
            var bc = []
            var i = 0
            while i < clen:
                push(bc, int(data[off + i]))
                i = i + 1
            push(self.vm.state.chunks, bc)
            off = off + clen
            chunk_idx = chunk_idx + 1
            
        # Execute all chunks (sequential top-level execution)
        chunk_idx = 0
        while chunk_idx < num_chunks:
            self.vm.run(self.vm.state.chunks[chunk_idx])
            chunk_idx = chunk_idx + 1
        return true
