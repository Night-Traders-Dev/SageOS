# SageVM Diagnostic Tools

These tools support Phase 2 (bytecode serialization debugging) and ongoing development.

## diff_bytecode.sage

Compares DIAG trace output from interpreted vs compiled `sgvmc` runs, or hex-diffs two `.sgvm` binary files.

**DIAG trace diff** (primary Phase 2 workflow):
```bash
# 1. Run interpreted compiler (outputs DIAG lines to stderr)
sage src/sgvm_compiler_debug.sage testsuite/test_class.sage /tmp/interp.sgvm 2>/tmp/interp.diag

# 2. Run compiled binary (requires rebuilding from sgvm_compiler_debug.sage)
./sgvmc_debug testsuite/test_class.sage /tmp/compiled.sgvm 2>/tmp/compiled.diag

# 3. Diff the traces
sage tools/diff_bytecode.sage /tmp/interp.diag /tmp/compiled.diag
```

**Hex diff** of `.sgvm` output files:
```bash
sage tools/diff_bytecode.sage --hex /tmp/interp.sgvm /tmp/compiled.sgvm
```

### Reading the output

- `OK` rows: interpreted and compiled agree at this step
- `DIFF` rows: divergence detected — look at the `op`, `j_after`, and `global_idx` fields
- `j_after` mismatch → compiled binary is mis-advancing the stream pointer (likely an integer arithmetic issue in the compiled `parse_hex_byte` or `*256+` expression)
- `global_idx` mismatch → const map lookup differs between modes

## sgvm_disassembler.sage

Disassembles `.sgvm` bytecode files into human-readable `.sage` source code.

```bash
sage tools/sgvm_disassembler.sage <file.sgvm>
```

```bash
sage tools/sgvm_hexdump.sage hello.sgvm
sage tools/sgvm_hexdump.sage /tmp/interp.sgvm
```

Use this alongside `diff_bytecode.sage --hex` to understand what a zeroed class chunk actually looks like post-corruption.

## src/sgvm_compiler_debug.sage

Instrumented drop-in replacement for `sgvm_compiler.sage`. Emits `DIAG` lines to stderr for every opcode processed in the second pass. Replace production `sgvm_compiler.sage` with this file for debugging, then revert or rebuild.

**DIAG line format:**
```
DIAG op=<num>(<name>) j_before=<hex_stream_pos> [local_idx=<n>] [global_idx=<n>] j_after=<hex_stream_pos>
```
