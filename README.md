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

### Open: `sagevm` fails when invoked from `make`

With `main.sage` fixed, the very next step fails:

```
../SageVM/sagevm compile kernel/main.sage build/kernel.sgvm --riscv
VM compile error: Statement requires AST fallback and cannot be emitted as a
compiled VM artifact yet.
DEBUG: Unsupported stmt type 17 requires AST fallback
```

The odd part is that **the identical command succeeds in a shell, every time**.
Running it 40 times in a row: 40 successes. Running `make ARCH=rv64` from a
clean tree: 0/5. And immediately after a *failing* `make`, the same command from
the shell succeeds.

Everything that could plausibly explain it has been excluded:

| Candidate | Result |
| --- | --- |
| working directory | identical (`src/`) under both |
| binary | same file, same md5 (`78b5f19a4dcb…`) |
| arguments | captured under `make`, byte-identical |
| environment | full sorted diff is 5 vars: `ARCH`, `MAKEFLAGS`, `MAKELEVEL`, `MFLAGS`, `SHLVL`; all tested individually and combined, all pass |
| stdin | `/dev/null`, closed, and inherited all pass |
| stdout | pipe, file, `/dev/null` and a tty all behave the same once the run is in the passing state |
| shell | `sh -c` (dash, what make uses) passes |
| `build/` contents | empty, and with `metal_vm_patched.c`, both pass |
| submodules | `SageLang` and `SageVM` both clean, not modified by the build |

`make -n` and `make --trace` confirm exactly one `sagevm` invocation with exactly
the arguments shown, and no hidden sub-make. The SageVM sub-build also fails
outright and separately: `make -C ../SageVM` reports "✗ SageLang build failed",
though it leaves the existing binary in place.

The most likely explanation is uninitialised memory or a file-descriptor or
signal-handling dependency inside `sagevm`, since no input distinguishes the two
invocations. It needs a debugger on the SageVM side, not more guessing from
outside.

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

