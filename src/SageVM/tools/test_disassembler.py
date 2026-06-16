import sys
from sgvm_disassembler import SGVMDisassembler

def test_parsing():
    # Target: testing/00_hello.sgvm
    disassembler = SGVMDisassembler("testing/00_hello.sgvm")
    if disassembler.disassemble():
        disassembler.build_basic_blocks()
        print(f"Blocks built: {len(disassembler.basic_blocks)}")
        for i, block in enumerate(disassembler.basic_blocks):
            print(f"Block {i}: {len(block['instructions'])} instructions")
        return True
    return False

if __name__ == "__main__":
    if test_parsing():
        print("Test Passed: Parsing and Basic Block construction successful.")
    else:
        print("Test Failed.")
        sys.exit(1)
