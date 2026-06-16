# SageOS Build System

ARCH ?= rv64
BUILD_DIR = build
K_SAGE = kernel/main.sage
K_SGVM = $(BUILD_DIR)/kernel.sgvm
K_OBJ = $(BUILD_DIR)/kernel_sgvm.o
ELF = $(BUILD_DIR)/sageos_$(ARCH).elf

# Toolchain
SAGELANG_DIR = SageLang
SGVMC = $(SAGELANG_DIR)/core/sgvmc
METALVM_INC = $(SAGELANG_DIR)/core/include
METALVM_SRC = build/metal_vm_patched.c
METALVM_ORIG = $(SAGELANG_DIR)/core/src/c/metal_vm.c

ifeq ($(ARCH),rv64)
    CROSS_COMPILE = riscv64-linux-gnu-
    CFLAGS_ARCH = -march=rv64g -mabi=lp64 -mcmodel=medany
    OBJCOPY_ARCH = riscv:rv64
    OBJCOPY_FORMAT = elf64-littleriscv
else ifeq ($(ARCH),x64)
    CROSS_COMPILE = x86_64-linux-gnu-
    CFLAGS_ARCH = -m64 -march=x86-64 -mno-red-zone -mno-mmx -mno-sse -mno-sse2
    OBJCOPY_ARCH = i386:x86-64
    OBJCOPY_FORMAT = elf64-x86-64
else ifeq ($(ARCH),arm64)
    CROSS_COMPILE = aarch64-linux-gnu-
    CFLAGS_ARCH = -march=armv8-a
    OBJCOPY_ARCH = aarch64
    OBJCOPY_FORMAT = elf64-littleaarch64
endif

CC = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy

CFLAGS = $(CFLAGS_ARCH) -ffreestanding -nostdlib -fno-pic -fno-pie -O2 -static -I$(METALVM_INC)
LDFLAGS = -T arch/$(ARCH)/linker.ld

SRCS = arch/$(ARCH)/boot.S arch/$(ARCH)/metal_shim.c $(METALVM_SRC)

.PHONY: all clean run

all: $(ELF)

$(METALVM_SRC): $(METALVM_ORIG)
	mkdir -p $(BUILD_DIR)
	# Patch MetalVM to include OP_PRINT (opcode 42)
	cat $(METALVM_ORIG) | sed '635a\        case OP_PRINT: { MetalValue val = metal_vm_pop(vm); metal_print_value(vm, val); metal_print_str(vm, "\\n"); break; }' > $(METALVM_SRC)

$(SGVMC):
	$(MAKE) -C $(SAGELANG_DIR)/core

$(K_SGVM): $(K_SAGE) $(SGVMC)
	mkdir -p $(BUILD_DIR)
	(cd $(SAGELANG_DIR)/core && ./sgvmc $(CURDIR)/$(K_SAGE) $(CURDIR)/$(K_SGVM))

$(K_OBJ): $(K_SGVM)
	$(OBJCOPY) -I binary -O $(OBJCOPY_FORMAT) --binary-architecture $(OBJCOPY_ARCH) $< $@

$(ELF): $(SRCS) $(K_OBJ)
	$(CC) $(CFLAGS) $(SRCS) $(K_OBJ) $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR)

run: $(ELF)
ifeq ($(ARCH),rv64)
	timeout 10s qemu-system-riscv64 -M virt -cpu rv64 -m 128M -display none -serial stdio -kernel $(ELF)
else ifeq ($(ARCH),x64)
	timeout 10s qemu-system-x86_64 -m 128M -display none -serial stdio -kernel $(ELF)
else ifeq ($(ARCH),arm64)
	timeout 10s qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -display none -serial stdio -kernel $(ELF)
endif
