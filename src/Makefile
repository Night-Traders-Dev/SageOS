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

# The Sage compiler to use, and the directory to put first on PATH for the steps
# that shell out to one.
#
# sagevm resolves the compiler by taking the first `sage` on PATH and does not
# check that it works. A stale or broken `sage` earlier on PATH therefore makes
# it fail with
#     "Statement requires AST fallback ... Unsupported stmt type 17"
# which reads like a problem with the Sage source and is not one at all. That
# was reproducible here: planting a failing `sage` first on PATH produced the
# exact failure from both make and a plain shell, while the real compiler
# succeeded from both.
#
# Prefer the compiler built from the SageLang submodule, so the superproject
# builds against the SageLang it has pinned. Fall back to whatever `sage` is on
# PATH, but record it and check it is executable so the failure is legible.
SAGE_COMPILER ?= $(shell test -x $(SAGELANG_DIR)/core/sage \
                     && echo $(CURDIR)/$(SAGELANG_DIR)/core/sage \
                     || command -v sage)
# Strip the trailing slash: $(dir) leaves one, and prepending it to PATH made
# sagevm try "$$PATH/sage" as a second, doubled-slash candidate
# (/usr/local/bin//sage) after the correct one. It was exec'd and then failed,
# and sagevm reported the failure as an unsupported statement in the Sage source.
SAGE_BINDIR = $(patsubst %/,%,$(dir $(SAGE_COMPILER)))

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
	@test -x "$(SAGE_COMPILER)" || { \
	    echo "error: no usable Sage compiler. Looked for '$(SAGE_COMPILER)'."; \
	    echo "       Build it with: make -C $(SAGELANG_DIR)"; \
	    echo "       or set SAGE_COMPILER=/path/to/sage."; \
	    exit 1; }
	@$(SAGE_COMPILER) --version >/dev/null 2>&1 || { \
	    echo "error: '$(SAGE_COMPILER)' is executable but does not run."; \
	    echo "       sagevm takes the first 'sage' on PATH and does not check it,"; \
	    echo "       so a broken one fails with a misleading 'AST fallback' error."; \
	    echo "       Set SAGE_COMPILER to a working compiler."; \
	    exit 1; }
	@echo "  sage compiler: $(SAGE_COMPILER)"
ifeq ($(ARCH),rv64)
	PATH="$(SAGE_BINDIR):$$PATH" $(SAGEVM) compile $(K_SAGE) $(K_SGVM) --riscv
else
	PATH="$(SAGE_BINDIR):$$PATH" $(SAGEVM) compile $(K_SAGE) $(K_SGVM)
endif

$(K_OBJ): $(K_SGVM)
	$(OBJCOPY) -I binary -O $(OBJCOPY_FORMAT) --binary-architecture $(OBJCOPY_ARCH) $< $@

$(ELF): $(SRCS) $(K_OBJ)
	$(CC) $(CFLAGS) $(SRCS) $(K_OBJ) $(LDFLAGS) -o $@

sageboot: $(SAGEVM)
	$(MAKE) -C $(SAGEBOOT_DIR) ARCH=$(ARCH) SAGE_COMPILER="$(SAGE_COMPILER)"

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

