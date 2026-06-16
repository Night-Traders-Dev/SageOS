import sys
import io
import srvm_core
import srvm_disassembler_logic
from srvm_core import SRVMUtils

# Sage RISC-V (SGRV) Low-level Hexdump / Inspector

proc srvm_disassemble(path):
    let data = io.readbytes(path)
    if data == nil:
        print "ERROR: Could not read file " + path
        return
    
    let ut = SRVMUtils()
    var pos = 0

    # Check for shebang
    if len(data) > 0 and int(data[0]) == 35:
        while pos < len(data) and int(data[pos]) != 10:
            pos = pos + 1
        if pos < len(data):
            pos = pos + 1

    if len(data) - pos < 4:
        print "ERROR: File too short"
        return

    var magic = ""
    var m_idx = 0
    while m_idx < 4:
        magic = magic + chr(int(data[pos + m_idx]))
        m_idx = m_idx + 1
    pos = pos + 4

    if magic != "SGRV":
        print "ERROR: bad magic " + magic
        return
    print "Magic: SGRV (Sage RISC-V)"

    let ver_maj = int(data[pos])
    let ver_min = int(data[pos+1])
    pos = pos + 2
    print "Version: " + str(ver_maj) + "." + str(ver_min)

    let const_count = (int(data[pos]) << 8) | int(data[pos+1])
    pos = pos + 2
    print "Constants: " + str(const_count)

    let consts = []
    var ci = 0
    while ci < const_count:
        if pos >= len(data):
            print "ERROR: Truncated constant pool"
            return
        let ctype = int(data[pos])
        pos = pos + 1
        if ctype == 1:
            let val = ut.unpack_double(data, pos)
            pos = pos + 8
            push(consts, {"type": "number", "value": val})
            print "  const[" + str(ci) + "] = NUM " + str(val)
        elif ctype == 3:
            let slen = (int(data[pos]) << 8) | int(data[pos+1])
            pos = pos + 2
            var s = ""
            var k = 0
            while k < slen:
                s = s + chr(int(data[pos + k]))
                k = k + 1
            pos = pos + slen
            push(consts, {"type": "string", "value": s})
            print "  const[" + str(ci) + "] = STR '" + s + "'"
        else:
            push(consts, {"type": "unknown", "value": nil})
            print "  const[" + str(ci) + "] = UNKNOWN type " + str(ctype)
        ci = ci + 1

    let chunk_total = (int(data[pos]) << 24) | (int(data[pos+1]) << 16) | (int(data[pos+2]) << 8) | int(data[pos+3])
    pos = pos + 4
    print "Chunks: " + str(chunk_total)
    print ""

    let dis = srvm_disassembler_logic.SRVMDisassembler(path)
    
    var chunk_idx = 0
    while chunk_idx < chunk_total:
        if pos + 4 > len(data):
            print "ERROR: Truncated chunk header"
            return
        let chunk_len = (int(data[pos]) << 24) | (int(data[pos+1]) << 16) | (int(data[pos+2]) << 8) | int(data[pos+3])
        pos = pos + 4
        print "--- Chunk " + str(chunk_idx) + " (" + str(chunk_len) + " bytes) ---"
        let chunk_end_pos = pos + chunk_len
        var pc = 0
        while pos < chunk_end_pos:
            let val = ut.read_le32(data, pos)
            let hex_val = srvm_byte_to_hex((val >> 24) & 0xFF) + srvm_byte_to_hex((val >> 16) & 0xFF) + srvm_byte_to_hex((val >> 8) & 0xFF) + srvm_byte_to_hex(val & 0xFF)
            
            # Setup a temporary disassembler to reuse decode_instr
            let temp_dis = srvm_disassembler_logic.SRVMDisassembler(path)
            temp_dis.consts = consts
            let instr = srvm_core.RVInstruction(val)
            let decoded = temp_dis.decode_instr(instr)
            
            print "  " + srvm_pad_left(str(pc), 4, " ") + "  " + hex_val + "  " + decoded
            pos = pos + 4
            pc = pc + 4
        print ""
        chunk_idx = chunk_idx + 1

proc srvm_byte_to_hex(val):
    let chars = "0123456789abcdef"
    let high = int(val / 16) % 16
    let low = int(val) % 16
    return chars[high] + chars[low]

proc srvm_pad_left(s, width, char):
    var res = s
    while len(res) < width:
        res = char + res
    return res
