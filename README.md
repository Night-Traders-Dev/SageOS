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

