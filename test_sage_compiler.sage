import io

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
    let bytes = []
    let i = 0
    while i < len(hex_str):
        let hi = hex_to_int(hex_str[i])
        let lo = hex_to_int(hex_str[i+1])
        push(bytes, hi * 16 + lo)
        i = i + 2
    return bytes

proc pack_u16_le(v):
    return [v % 256, (v / 256) % 256]

proc pack_u32_le(v):
    return [v % 256, (v / 256) % 256, (v / 65536) % 256, (v / 16777216) % 256]

let test_hex = "5347564d"
let decoded = decode_hex(test_hex)
print("Decoded hex: " + str(decoded))

let packed_u32 = pack_u32_le(0x12345678)
print("Packed u32 LE: " + str(packed_u32))

io.writebytes("test_io.bin", decoded + packed_u32)
print("Wrote test_io.bin")
