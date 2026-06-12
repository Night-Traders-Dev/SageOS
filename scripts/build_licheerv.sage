import io
import sys
import os.boot.build as bb
import os.boot.start as start
import os.boot.linker as linker
import scripts.toml as toml

NL = chr(10)

# Load build.toml configuration
toml_content = io.readfile("build.toml")
build_config = toml.parse_toml(toml_content)

# Target configuration: SG2002 (LicheeRV Nano)
arch = "riscv64"
board = "sg2002"
output_dir = "build/licheerv_" + board
sys.exec("mkdir -p " + output_dir)

print("Generating build for " + board + " in " + output_dir + "...")

# 1. Generate boot assembly (minimal)
# Note: Initially we rely on SBI, so boot assembly is minimal.
boot_asm = start.emit_start_riscv64("kmain", "stack_top")
# Ensure FPU is enabled
boot_asm = replace(boot_asm, "csrci mstatus, 0x8", "csrci mstatus, 0x8" + NL + "	# Enable FPU (set mstatus.FS = 11)" + NL + "	li t0, 0x6000" + NL + "	csrs mstatus, t0")
boot_asm = boot_asm + bb.generate_serial_boot_riscv64()
boot_asm = replace(boot_asm, "stack_bottom:", ".global stack_bottom" + NL + "stack_bottom:")

# 2. Generate linker script (SG2002 specific memory)
ld_config = linker.default_config()
# Kernel base as per documentation
ld_config["base_address"] = 0x80000000 
linker_script = linker.generate_script(ld_config)

# Fix linker script segments
linker_script = replace(linker_script, "*(.note)", "*(.note .note.*)")
linker_script = replace(linker_script, "*(.rodata .rodata.*)", "*(.rodata .rodata.* .srodata .srodata.*)")
linker_script = replace(linker_script, "*(.data .data.*)", "*(.data .data.* .sdata .sdata.* .got .got.*)")
linker_script = replace(linker_script, "*(.bss .bss.*)", "*(.bss .bss.* .sbss .sbss.*)")

# ... (using the same segments fix as virt_build) ...
linker_script = replace(linker_script, ".rodata ALIGN(4096) : { *(.rodata .rodata.* .srodata .srodata.*) }", ".rodata ALIGN(4096) : { *(.rodata .rodata.* .srodata .srodata.*) } :text")
linker_script = replace(linker_script, ".data ALIGN(4096) : { *(.data .data.* .sdata .sdata.* .got .got.*) }", ".data ALIGN(4096) : { *(.data .data.* .sdata .sdata.* .got .got.*) } :text")
linker_script = replace(linker_script, ".bss ALIGN(4096) : { __bss_start = .; *(.bss .bss.* .sbss .sbss.*) *(COMMON) __bss_end = .; }", ".bss ALIGN(4096) : { __bss_start = .; *(.bss .bss.* .sbss .sbss.*) *(COMMON) __bss_end = .; } :text")

io.writefile(output_dir + "/boot.S", boot_asm)
io.writefile(output_dir + "/linker.ld", linker_script)

# 3. Gather sources
c_sources = build_config["sources"]["common"]
target_config = build_config["targets"][arch]
extra_sources = target_config["extra_sources"]
i = 0
while i < len(extra_sources):
    _u = push(c_sources, extra_sources[i])
    i = i + 1
end

# Add sg2002 specific
sg2002_sources = ["arch/rv64/sg2002/boot/boot.c", "arch/rv64/sg2002/kernel/uart/uart.c", "arch/rv64/sg2002/kernel/platform_init/platform.c"]
i = 0
while i < len(sg2002_sources):
    _u = push(c_sources, sg2002_sources[i])
    i = i + 1
end

# 4. Construct build script
target_as = target_config["as"]
target_asflags = target_config["asflags"]
target_cc = target_config["cc"]
target_ld = target_config["ld"]
target_cflags = target_config["cflags"]

script = "#!/bin/sh" + NL + "set -e" + NL
script = script + "AS=\"" + target_as + "\"; ASFLAGS=\"" + target_asflags + "\"; CC=\"" + target_cc + "\"; LD=\"" + target_ld + "\"" + NL
script = script + "CFLAGS=\"" + target_cflags + "\"" + NL

script = script + "echo 'Building SageOS LicheeRV Nano (" + board + ")...'" + NL
script = script + "$AS $ASFLAGS -o " + output_dir + "/boot.o " + output_dir + "/boot.S" + NL

objects_str = output_dir + "/boot.o"
i = 0
while i < len(c_sources):
    src = c_sources[i]
    obj = output_dir + "/obj" + str(i) + ".o"
    script = script + "echo '  CC " + src + "'" + NL
    if src[len(src)-2:len(src)] == ".S":
        script = script + "$CC $CFLAGS -c -o " + obj + " " + src + NL
    else:
        script = script + "$CC $CFLAGS -include sage_libc_shim.h -O2 -c -o " + obj + " " + src + NL
    end
    objects_str = objects_str + " " + obj
    i = i + 1
end

elf_path = output_dir + "/kernel.elf"
script = script + "echo '  LD " + elf_path + "'" + NL
ld_flags = "-nostdlib -static -fno-pie -no-pie -z max-page-size=4096"
script = script + "$CC " + ld_flags + " -T " + output_dir + "/linker.ld -o " + elf_path + " " + objects_str + NL

script = script + "echo 'Build complete: " + elf_path + "'" + NL

io.writefile(output_dir + "/build.sh", script)
print("Build script for LicheeRV Nano generated at " + output_dir + "/build.sh")
