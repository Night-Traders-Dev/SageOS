# SageOS Build System

ARCH ?= rv64
BUILD_DIR = build
K_SAGE = kernel/main.sage
K_SGVM = $(BUILD_DIR)/kernel.sgvm
K_OBJ = $(BUILD_DIR)/kernel_sgvm.o
ELF = $(BUILD_DIR)/sageos_$(ARCH).elf

# Toolchain
SAGELANG_DIR = ../SageLang
SAGEVM_DIR = ../SageVM
SAGEVM = $(SAGEVM_DIR)/sagevm
METALVM_INC = $(SAGELANG_DIR)/core/include
METALVM_SRC = build/metal_vm_patched.c

ifeq ($(ARCH),rv64)
    METALVM_ORIG = $(SAGELANG_DIR)/core/src/c/metal_rv64_vm.c
else
    METALVM_ORIG = $(SAGELANG_DIR)/core/src/c/metal_vm.c
endif

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

SAGEBOOT_DIR = ../SageBoot
SAGEBOOT_ELF = $(SAGEBOOT_DIR)/sageboot.elf

.PHONY: all clean run sageboot

all: $(ELF)

$(METALVM_SRC): $(METALVM_ORIG) patch_metal_vm.py
	mkdir -p $(BUILD_DIR)
	# Patch MetalVM
	python3 patch_metal_vm.py $(METALVM_ORIG)

$(SAGEVM):
	$(MAKE) -C $(SAGEVM_DIR)

$(K_SGVM): $(K_SAGE) $(SAGEVM)
	mkdir -p $(BUILD_DIR)
ifeq ($(ARCH),rv64)
	$(SAGEVM) compile $(K_SAGE) $(K_SGVM) --riscv
else
	$(SAGEVM) compile $(K_SAGE) $(K_SGVM)
endif

$(K_OBJ): $(K_SGVM)
	$(OBJCOPY) -I binary -O $(OBJCOPY_FORMAT) --binary-architecture $(OBJCOPY_ARCH) $< $@

$(ELF): $(SRCS) $(K_OBJ)
	$(CC) $(CFLAGS) $(SRCS) $(K_OBJ) $(LDFLAGS) -o $@

sageboot: $(SAGEVM)
	$(MAKE) -C $(SAGEBOOT_DIR) ARCH=$(ARCH) SAGE_COMPILER=$(CURDIR)/$(SAGELANG_DIR)/core/sage

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C $(SAGEBOOT_DIR) clean

run: $(ELF) sageboot
ifeq ($(ARCH),rv64)
	timeout 10s qemu-system-riscv64 -M virt -cpu rv64 -m 128M -display none -serial stdio -kernel $(SAGEBOOT_ELF) -device loader,file=$(ELF),addr=0x81000000,force-raw=on
else ifeq ($(ARCH),x64)
	timeout 10s qemu-system-x86_64 -m 128M -display none -serial stdio -kernel $(SAGEBOOT_ELF) -device loader,file=$(ELF),addr=0x01100000,force-raw=on
else ifeq ($(ARCH),arm64)
	timeout 10s qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -display none -serial stdio -kernel $(SAGEBOOT_ELF) -device loader,file=$(ELF),addr=0x41000000,force-raw=on
endif

