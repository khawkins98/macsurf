# PowerPC emulation on Apple Silicon — what's at the frontier (research, 2026-06-01)

Lateral research into whether anything beyond our current qemu-system-ppc + TCG
setup could speed up the OS 9 build/test loop. Four research threads: QEMU TCG
internals, academic work on parallelizing serial emulation, alternative
emulators/JITs, and workflow-level parallelism. Companion to
[PERFORMANCE.md](PERFORMANCE.md), which holds the measured levers; this doc holds
the *why* behind what's worth trying and what's a documented dead end.

## TL;DR

1. **Parallelizing a single guest thread across host cores is a measured dead
   end** — the research literature tops out at ~1.09x on real hardware. Our
   "single-vCPU ceiling" conclusion is correct and now has citations.
2. **The single thread itself is leaving ~5x on the table.** Production
   PPC→ARM64 JITs exist (Dolphin, MAME, SheepShaver-arm64) that embarrass TCG;
   they're just not attached to a Mac OS 9-capable emulator — except
   SheepShaver, which is a spike candidate.
3. **The work parallelizes even though the emulator can't**: native precheck
   gate (shipped — see `tools/precheck.sh`), warm snapshots, build/test
   pipelining, and sharded library-target builds across cloned VMs.
4. **PPC floating-point under TCG is slow *by design* and unfixable upstream**
   — but compilers are integer-bound, so it likely doesn't matter for builds.
   One TCG-plugin run would confirm.

---

## 1. Can a single-threaded guest be parallelized? (No — with receipts)

The question: Mac OS 9 is uniprocessor, so QEMU runs it on one TCG thread = one
host core. Can the other ~9 cores of an M5 help?

**Speculative cross-core execution** (leader-follower, slipstream, run-ahead
threads with checkpoint/rollback):

- The definitive limit study is Edinburgh's *"Limits of region-based dynamic
  binary parallelization"* (VEE 2013): even with idealized region selection
  cutting up to 54% of critical-path instructions, a realistic
  hardware-supported implementation delivers **~1.09x average speedup**.
  Speculation overhead, misspeculation rollback, and inter-core communication
  eat the theoretical gains.
  https://groups.inf.ed.ac.uk/pasta/pub_VEE_2013_LIMITS_BIN_PARAL.html
- RASP (Stanford, CGO 2011) claimed +49% on SPEC2006int — but on a *simulated*
  4-core machine assuming generic hardware speculation support that Apple
  Silicon does not expose to a userspace process.
  https://ieeexplore.ieee.org/document/5764675/
- Mac OS 9's cooperative event-loop control flow (`WaitNextEvent`, data-dependent
  branching, constant I/O and state mutation) is the *worst case* for these
  schemes even where they work.

**Background/tiered translation** (a second core optimizes hot blocks while the
vCPU thread runs — the JavaScript-engine model):

- The canonical system was **HQEMU** (CGO 2012): TCG on the execution thread +
  LLVM optimizer on helper threads, 2.4–4x on SPEC. The codebase has been
  **dead since 2018**, pinned to QEMU 2.x and an old LLVM.
  https://github.com/sailfish009/hqemu
- Console emulators (RPCS3, Xenia) do parallel *compilation* of guest code, but
  their *runtime* multicore gains come from the guests (PS3, Xbox 360) being
  genuinely multi-core. Doesn't transfer to a uniprocessor guest.
- QEMU upstream has no tiered JIT, no async translation, no persistent
  translation cache, and none are on the roadmap.

**Pipeline parallelism** (devices/IO/display on other threads): QEMU already
does this (I/O thread, display scanout). The vCPU instruction stream — the
dominant cost of a compile workload — cannot be offloaded.

**Exotica**: FPGA PPC cores exist (A2O, Microwatt) but are 64-bit POWER server
cores, not Mac-OS-9-bootable G3/G4-class cores. GPU binary translation and
distributed emulation have no credible system-emulation results.

**Verdict: closed.** Do not revisit speculative parallelization. The only
"extra cores help" mechanism with real-world precedent is compilation offload
(HQEMU-style), and Mac OS 9's small, quickly-warmed code footprint caps its
upside well below what SPEC benchmarks show.

---

## 2. The real gap: TCG is a slow PPC JIT, and fast ones exist

Three production-quality PPC→ARM64 JITs run today on Apple Silicon:

| Project | Guest CPU | Relevance |
|---|---|---|
| **Dolphin** (GameCube/Wii) | PPC 750 — same family as the G3 | `JitArm64` at feature parity with x86-64 JIT; near-native speed; proves a 750-class core JITs to ARM64 fast. Bonded to console hardware semantics — not adaptable to Mac emulation without a rewrite. |
| **MAME 0.274** (Feb 2025) | Various PPC | Shipped a native ARM64 dynarec backend ("M2 Pro flies"). Same story: proves the tech, bonded to arcade drivers. |
| **SheepShaver arm64 JIT** | PPC for classic Mac OS | **The only one attached to a Mac OS-capable emulator.** MacBench 5.0 vs PowerMac G3/300 baseline: **470% with arm64 JIT**, 271% x86 JIT under Rosetta, 96% arm64 interpreter. Our QEMU TCG setup is roughly interpreter-class (~250–400 MHz G3 integer-equivalent), so a working arm64 JIT would be **~5x**. |

**SheepShaver caveats** (why this is a spike, not a switch):

- The arm64 JIT has historically been flaky and is **not enabled in stock
  release builds** of the maintained fork (`kanjitalk755/macemu`, universal
  builds since v2.5, Jan 2025). A JIT-enabled build/fork is required.
- SheepShaver lacks full MMU emulation → MacsBug and debuggers don't work. Note:
  **the upstream MacSurf maintainer has never compiled in SheepShaver** — their
  docs use it strictly as a runtime smoke-test tier. The "CW debugger
  trap-crashes" concern in PERFORMANCE.md is our own unverified note, not their
  experience. Whether the CW8 IDE *builds* (no debugger) work in SheepShaver is
  an open question nobody has answered.
- Networking is weak (no working OT ethernet without manual config) — fine for
  a compile loop, matters for fetch testing.
- Benchmark numbers: https://www.emaculation.com/forum/viewtopic.php?t=10933

**Other options assessed and rejected:**

- **DingusPPC** — actively developed, can run OS 9, but pure interpreter (no
  JIT, none planned). Comparable-or-slower than our QEMU setup. Track for
  accuracy/debugging, not speed.
- **PearPC** — abandoned (2011), x86-only JIT. Dead.
- **Rosetta-1/QuickTransit-style AOT for PPC** — proprietary, defunct, nothing
  survives in usable form.
- **Executor 2000 / HLE ("Wine for classic Mac OS")** — right concept, wrong
  era: 68k-only Toolbox reimplementation. No PPC/Carbon HLE exists; building
  one is a multi-year project.
- **mpw-emu** (https://github.com/Treeki/mpw-emu) — runs the CodeWarrior
  *compiler binary* under user-mode PPC emulation (Unicorn) with HLE'd Toolbox
  calls. No OS boot at all; native-speed batch compilation is the structural
  ideal. **But**: self-described "ridiculous weekend project," incomplete libc,
  missing PEF linker relocations, proven only on CW Pro 1's MWCPPC compiling
  single files. For a 530-file Carbon project, this is a research spike with
  high upside and high probability of hitting a wall. Write-up:
  https://wuffs.org/blog/emulating-mac-compilers

---

## 3. QEMU-specific findings

**Why FP is catastrophically slow (0.13M transcendental iters/s):** QEMU
emulates PPC floating point in pure softfloat *by design* — PPC's FI
(fraction-inexact) bit must be tracked per-operation, so the PPC target clears
FP status before every op, which permanently disqualifies QEMU's "hardfloat"
fast path (host-FPU with fixups). This was an explicit upstream decision
(~20% gain rejected to preserve FI-bit accuracy); a Feb 2025 cleanup preserved
the behavior. There is no upstream fix coming. Transcendentals are doubly
penalized: the guest's `sin`/`cos` are software libm routines, every
instruction of which is softfloat-emulated.

**Why this probably doesn't matter:** compilers are integer/string/IO-bound.
Confirm with one run before spending any effort here (next section).

**TCG plugins — the actionable diagnostic.** QEMU ships profiling plugins that
work on macOS hosts today:

```
qemu-system-ppc ... -plugin /path/to/contrib/plugins/libhowvec.so -d plugin
```

- `howvec` — instruction-class breakdown (FP vs integer vs load/store). One
  CW8 build under this tells us definitively whether the FP ceiling matters.
- `hotblocks` — hottest translation blocks; where the compile workload actually
  spends guest cycles.
- Docs: https://www.qemu.org/docs/master/devel/tcg-plugins.html

**QEMU version note:** the carry-opcode TCG rewrite (landed in the 10.x cycle)
improves PPC multi-word integer arithmetic; we're on 11.x so we already have it.
No other TCG throughput work in 8.x→11.x is relevant to this workload.

---

## 4. Workflow-level parallelism (the path that actually works)

The emulator can't use more than one core, but the *work* can. In
payoff-per-effort order:

1. **Native precheck gate — SHIPPED, see [`tools/precheck.sh`](../../precheck.sh).**
   Runs host clang in CW8-approximating C89 mode over all 528 manifest files in
   ~15 s, parallel across host cores. Most fix rounds in project history died
   on C89/include errors that this catches before any guest cycle is spent.
   Bonus: it cross-checks the manifest against the repo (caught 25 stale file
   entries on first run) and surfaces Mac–Linux source drift (caught designated
   initializers in 7 files that allegedly build on the Mac).

2. **Warm snapshots** — `vm.py snapshot cw_warm` with CodeWarrior open and the
   project loaded; `loadvm` restore takes seconds vs. the ~2 min cold boot.
   Caveat: the current `guest-cp.sh` raw-image round-trip *flattens* internal
   snapshots (see GOTCHAS.md), so file injection for the hot loop must move to
   the network path (SLIRP hostfwd, already configured) to keep snapshots alive.

3. **Build/test pipelining** — one VM builds round N+1 while a second VM tests
   round N's binary. Pure overlap; needs only a second qcow2 clone and a second
   P-core (we have both).

4. **Sharded compilation across N cloned VMs** (the big lever, **gated on the
   first single-target build working**): CW8 natively supports **Library
   targets** (`.lib` archives) and subprojects. Split the 530 files into 4
   library targets (libcss / libdom+hubbub / parserutils+duktape / netsurf
   core+frontend), compile each in a cloned VM on its own P-core, link once in
   a fifth project. CW8 is fully headless-scriptable via AppleEvents
   (`tools/codewarrior/build-robot.applescript`, or the cmdide tool). Realistic
   gain: **~3–3.5x on cold full builds** with 4 P-core shards. Does nothing for
   incremental builds (already cheap). License note: CW8 installs with a bundled
   license.dat and no serial; running N cloned guests is a question for the
   maintainer, not a technical blocker.

5. **Host-side object cache** — *deferred, not currently practical.* CW8 stores
   object code inside the per-target project "Data" folder (an opaque database),
   not as loose `.o` files that can be hashed and swapped individually. The
   realistic equivalent today is lever C in PERFORMANCE.md: keep a
   post-clean-build qcow2 as the durable cache and clone + incremental from it.
   Per-file object caching only becomes possible if builds move to library
   targets (whose `.lib` outputs ARE discrete files) or to MPW command-line
   tools.

## Recommended order of operations

1. ~~Native precheck gate~~ — **done** (`tools/precheck.sh`).
2. Land the first single-target CW8 build in the guest (the existing
   HANDOFF-NEXT-SESSION.md work). Everything else is gated on this.
3. Warm-snapshot the post-build state; move injection to the network path.
4. Run `howvec` during one build to settle the FP question.
5. 2-VM build/test pipeline.
6. SheepShaver arm64-JIT spike: build `kanjitalk755/macemu` with JIT enabled,
   install CW8, attempt a compile. (~5x if it works; nobody has tried.)
7. Shard into library targets across 4 VMs once the monolithic build is stable.

## Sources

Parallelization limits: [VEE 2013 limit study](https://groups.inf.ed.ac.uk/pasta/pub_VEE_2013_LIMITS_BIN_PARAL.html) ·
[RASP CGO 2011](https://ieeexplore.ieee.org/document/5764675/) ·
[HQEMU](https://github.com/sailfish009/hqemu) ([paper](https://dl.acm.org/doi/10.1145/2259016.2259030)) ·
[QEMU MTTCG docs](https://www.qemu.org/docs/master/devel/multi-thread-tcg.html)

Faster PPC JITs: [Dolphin on M1](https://dolphin-emu.org/blog/2021/05/24/temptation-of-the-apple-dolphin-on-macos-m1/) ·
[MAME 0.274 ARM64 dynarec](https://mameonmacs.blogspot.com/2025/02/mame-0274-for-silicon-macs-with-dynamic.html) ·
[SheepShaver arm64 JIT benchmarks](https://www.emaculation.com/forum/viewtopic.php?t=10933) ·
[kanjitalk755/macemu](https://github.com/kanjitalk755/macemu) ·
[DingusPPC](https://github.com/dingusdev/dingusppc) ·
[mpw-emu](https://github.com/Treeki/mpw-emu) ([write-up](https://wuffs.org/blog/emulating-mac-compilers))

QEMU internals: [softfloat/hardfloat for PPC](https://github.com/qemu/qemu/blob/master/fpu/softfloat.c) ·
[hardfloat-disabled-for-PPC discussion](https://lists.gnu.org/archive/html/qemu-arm/2018-02/msg00475.html) ·
[TCG plugins](https://www.qemu.org/docs/master/devel/tcg-plugins.html) ·
[Why Rosetta 2 is fast (dougallj)](https://dougallj.wordpress.com/2022/11/09/why-is-rosetta-2-fast/)

CW8 automation: [cmdide](https://github.com/burgerbecky/cmdide) ·
[CW build tools reference (library targets)](https://www.nxp.com/docs/en/user-guide/CWPABTR.pdf)
