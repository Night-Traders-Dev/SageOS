# SageOS TODO - x64 Boot Finalization

## 1. VFS Bridge Loading Diagnosis (Critical)
- [ ] Investigate the `load_binary` failure in `MetalVM` (specifically the `main_code_length` deserialization error).
- [ ] Compare `vfs_bridge.bc` binary produced by the new compiler vs. standalone tools to ensure format parity (endianness, header structure).
- [ ] Verify `vfs_bridge_bytecode.h` content to ensure it matches the `.bc` file exactly.

## 2. Kernel VM Robustness
- [ ] Complete the transition to the explicit `call_stack` in `sgvm_vm.sage` to eliminate host recursion.
- [ ] Implement `OP_BREAK`, `OP_CONTINUE`, `OP_PUSH_ENV`, `OP_POP_ENV` in the kernel dispatcher.
- [ ] Verify native bridge invocation with proper `MetalValue` marshaling.

## 3. Architecture Verification
- [ ] Finalize ARM64 verification after fixing the dictionary built-in issues.
- [ ] Ensure that x64 QEMU successfully mounts the disk image with the new IDE emulation and reaches the shell.

## 4. Documentation & Maintenance
- [ ] Update documentation to reflect the new toolchain usage.
- [ ] Update `CHANGELOG.md` with final fixes from this diagnosis phase.
