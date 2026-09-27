# SageOS

SageOS is a modern, capability-based operating system built from the ground up using **SageLang** and targeting the **SageVM** (SRVM) execution environment.

## Vision
To create a high-performance, secure, and portable OS where the majority of the system logic is expressed in Pure Sage, minimizing the reliance on C for everything except the most fundamental hardware shims.

## Targets
- **RV64** (Primary focus)
- **x86_64**
- **ARM64**

## Project Structure
- `src/arch/<arch>/`: Architecture-specific boot code, linker script and metal
  shim. **This is what the build uses** — see "Two copies of every architecture"
  below, because it is not the `arch/` you might expect.
- `src/kernel/`: Pure Sage kernel implementation.
- `src/Makefile`: the build. There is no top-level `Makefile`, so build
  commands run from `src/`.
- `arch/<arch>/`: git submodules (SageOS_arm64 / _rv64 / _x64). Unused mirrors —
  see below.
- `SageBoot/`, `SageLang/`, `SageVM/`: submodules, pinned. SageLang's pin is
  kept current so ESP32 work is visible from here.
- `docs/`: System specifications and architectural design.
- `tests/`: Comprehensive test suite for all subsystems.
- `check_arch_drift.py`: reports divergence between the two copies (below).

## SageOS does not currently build

Recorded before any of the reconciliation work, because it blocks verifying it.
Two independent problems, one fixed and one open.

### Fixed: `src/kernel/main.sage` used invalid syntax

`end` was used as a block terminator after every `if`, `while` and `proc`. That
is not SageLang -- blocks are delimited by indentation -- and the compiler
rejects it:

```
error: unknown name 'end' in compiled code
  --> kernel/main.sage:33:5
```

so `make ARCH=rv64` could not generate the kernel bytecode at all
("Failed to generate SVM from kernel/main.sage"). Every architecture was
affected and it looked like a build-system problem rather than a source one.
The terminators are removed. This is the same class of bug as the `pass` that
had been sitting in the ESP32 `boot.sage`.

### Resolved: the Sage compiler's VM emit step was rejecting aliased imports

With `main.sage` fixed, the next step fails:

```
VM compile error: Statement requires AST fallback and cannot be emitted as a
compiled VM artifact yet.
DEBUG: Unsupported stmt type 17 requires AST fallback
```

Two distinct bugs are involved.

**1. `sagevm` does not validate the compiler it finds. (Fixed.)**

`sagevm` resolves the Sage compiler by taking the first `sage` on `PATH`, and
never checks that it works. A stale or broken `sage` earlier on `PATH` therefore
produces the "AST fallback" message above, which reads like a fault in the Sage
source and is not one. This was found by planting a failing `sage` on `PATH`,
which reproduced the exact failure from both `make` and a plain shell while the
real compiler succeeded from both. In the wild it was a `sage` left in
`~/.opencode/bin` (an agent tooling directory that is on `PATH`), shadowing
`/usr/local/bin/sage`.

`src/Makefile` now pins the compiler -- preferring the one built from the
pinned `SageLang` submodule -- puts its directory first on `PATH` for both the
`sagevm` and `sageboot` steps, and fails with an explicit message if the chosen
compiler is not executable or does not run. The build now prints which compiler
it is using instead of silently using whatever it finds.

**2. `sage --emit-vm` rejected aliased imports. (Fixed, in SageLang.)**

Under `strace` the failing invocation was just the compiler exiting 1:

```
execve("/usr/local/bin/sage", ["sage","--emit-vm","kernel/main.sage",
                               "-o","kernel/main.sage.svm"]) = 0
+++ exited with 1 +++
```

`main.sage` opens with

```sage
import drivers.memory.pmm as pmm_mgr
import drivers.memory.vmm as vmm_mgr
```

so the cause is the alias form. It reproduced deterministically, with a
four-case matrix:

| source | result |
| --- | --- |
| no import | compiles |
| `import os` | compiles |
| `import os as o` | **fails** |
| `from os import path as p` | **fails** |

The error named a type number, which sent the investigation after a missing
enum case:

```
VM compile error: Statement requires AST fallback and cannot be emitted as a
compiled VM artifact yet.
DEBUG: Unsupported stmt type 17 requires AST fallback
```

`17` is `STMT_IMPORT`, and `compile_stmt` does have a `case STMT_IMPORT` for it.
The emitter was never the problem: it already writes `BC_OP_IMPORT` followed by
`BC_OP_DEFINE_GLOBAL` naming the alias, or the last dotted segment when there is
no alias. What rejected the statement was `stmt_requires_ast_fallback`, which
vetoed any import with an alias, on the belief that aliased imports "need the
richer binding logic in interpreter.c". So the emitter and the gate disagreed
about which forms were compilable, and the gate won.

Fixed in SageLang `4128a9a3`, by dropping the alias from the veto. From-imports
still need the AST walker and still fail, but now by name:

```
from-imports ('from module import name') are not supported by the bytecode VM
yet; use 'import module' or 'import module as name'
```

The artifact path stays `BYTECODE_COMPILE_STRICT` on purpose. The walker is
reached through `chunk->ast_stmts[]`, which holds pointers into the *live* AST
and is never serialized, so a fallback opcode in a file would load and then do
nothing. Constructs that need the walker have to compile natively instead.

`make ARCH=rv64` is 6/6 and links a 32 KB ELF. The checked-in
`kernel/main.sage.svm` grows 436 → 1537 bytes, which is the import opcode plus
the module bindings that were previously missing.

One loose end: `sage --emit-vm` was also observed succeeding 40 runs out of 40
on this same input, which the fix above does not account for. Nothing in the
build depends on that variance now, so it is noted rather than chased.

**3. `BC_OP_IMPORT` has no execution case. (Open, in SageLang.)**

With the kernel compiling, `sagevm run` reaches

```
SageOS Booting...
  Initializing PMM...
  PMM Init OK.
```

only because `src/Makefile` resolves the driver modules at compile time. Import
a module the build cannot resolve and the opcode binds an empty module, because
`core/src/vm/vm.c` never executes `BC_OP_IMPORT` — it appears only in the
validator's opcode tables. The kernel cannot call into the drivers it imports
until that opcode actually loads a module.

## Two copies of every architecture

Every architecture exists twice, and this is deliberate-but-undocumented, so it
has been allowed to rot:

| path | kind | used by the build | state |
| --- | --- | --- | --- |
| `src/arch/<arch>/` | vendored plain files | **yes** | ahead |
| `arch/<arch>/` | git submodule | no | behind |

`src/Makefile` writes `-T arch/$(ARCH)/linker.ld` and lists
`arch/$(ARCH)/boot.S`, which resolve relative to `src/` — so the build reads the
**vendored** copy. 1533 of this repo's 1587 tracked files live under `src/arch/`.

The vendored copies are ahead, not behind. `src/arch/rv64/boot.S` carries the
trap vector, S-mode delegation and the handoff-structure argument that the
submodule pin lacks, and the `metal_shim.c` files differ by 44/104/43 lines,
with the vendored side carrying the real UART output and the `MetalRV64VM`
instantiation. Last vendored change 2026-06-19; submodule pins are 2026-06-16.

**Decision: `src/arch/` is authoritative.** The submodules are unused, so
"just build from the submodules" is not currently true, and deleting either side
before reconciling would lose work.

**The reconciliation is deliberately not done yet.** The intended end state is:
push the real arch content up to the `SageOS_*` repositories, re-pin the
submodules, point `src/Makefile` at `../arch/$(ARCH)/`, and delete `src/arch/`
-- which would also delete a stale 1464-file nested copy of SageLang 0.7.0 that
nothing references (the build takes SageLang from the `SageLang` submodule, which
is at 4.1.16). That is 1533 files removed and a genuine de-duplication.

It is blocked because SageOS cannot currently be built, so a refactor of that
size could not be verified. Deleting 1533 files -- including the only working
copy of the newer `boot.S` and `metal_shim.c` for each architecture -- on the
strength of a build that does not run would be reckless. The `src/arch/` tree
stays until the build is green.

One change must *not* be taken blindly when it does happen: the vendored
`rv64/linker.ld` moves the load address to `0x80800000`, which nothing else in
the tree references. `0x80200000` is used by SageBoot's `KERNEL_LOAD_ADDR`,
`src/kernel/main.sage` and both VMM/PMM tests, while `make run` loads the ELF at
`0x81000000` -- three different values already. The vendored linker script is
otherwise byte-identical to the submodule's, so its only change is that address,
and it should not be adopted without settling the inconsistency first.

To see the current state:

```bash
python3 check_arch_drift.py            # human-readable report
python3 check_arch_drift.py --strict   # exit 1 on any drift, for CI
```

The remaining cleanup is to push `src/arch/<arch>` up to the corresponding
SageOS_* repository and re-pin the submodule, after which `arch/` can be dropped
entirely. That is left undone deliberately: removing gitlinks to published
repositories is a structural change with consequences beyond this checkout, and
it should not happen as a side effect of a sync commit.

Also note the build instructions below say `make run`, which only works from
`src/`.

## Booting via SageBoot
SageOS integrates **SageBoot** as a submodule bootloader. When running, SageBoot is dynamically compiled for the target architecture (`ARCH`) and loaded as the primary `-kernel` payload in QEMU, while the SageOS kernel ELF is loaded into memory using QEMU's `-device loader`:

There is no top-level `Makefile`; these run from `src/`.

```bash
cd src
make clean && make run ARCH=rv64
make clean && make run ARCH=arm64
```

