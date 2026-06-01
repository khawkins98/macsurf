# QEMU OS 9 build performance — playbook + measurements

How fast the off-hardware build loop runs, what we changed, and what's left to try.
Companion to the "Performance / CPU utilization" section in [GOTCHAS.md](GOTCHAS.md).
Research backing the levers and dead ends below (citations, alternatives
assessed): [EMULATION-FRONTIER.md](EMULATION-FRONTIER.md).

## The ceiling (read this first)

Mac OS 9 is a **uniprocessor** OS, so QEMU emulates it on a **single TCG thread =
one host core**. On the M5 (4 P + 6 E cores) that reads as ~10% of total CPU in
Activity Monitor. That is the wall, not idle capacity. Nothing spreads a
uniprocessor guest across cores (`-smp >1`, MTTCG, HVF/KVM are all dead ends).
Every real win is either (a) putting that one core on a fast **P-core**, or
(b) doing **less work** (fewer files, less background overhead).

## Measurement instruments

Two, because they measure different things. Always measure before AND after a
change, one change at a time, runs done serially (never two at once — they split
the one core and corrupt both numbers).

1. **`cpubench`** ([../bench/cpubench.c](../bench/cpubench.c)) — raw single-core
   CPU/FPU/memory throughput. Build it as a CodeWarrior SIOUX console app (New
   Project → Std C Console PPC, drop the file in, Make, Run). Headline number is
   "M int-iters/s". **This is the instrument for the P-core lever.** It also gives
   the "comparative megahertz" figure: run the SAME binary on a real G3/G4 and
   divide.
2. **Build wall-clock** — time a real CodeWarrior build end to end (trigger →
   completion). **This is the instrument for the incremental-build and
   extensions/VM-off levers**, which barely touch a tight CPU loop but change how
   much work a real build does. Use a host stopwatch around the Make, or a
   guest-written timestamp pulled back via read-results.sh.

Which instrument sees which lever:

| Lever | cpubench | build wall-clock |
|---|---|---|
| P-core (Terminal launch / renice) | ✅ primary | ✅ (also) |
| Virtual Memory off / lean extensions | ~flat (CPU-bound loop doesn't page) | ✅ primary |
| Incremental vs full rebuild | n/a (it's about file count) | ✅ only |

## Chosen levers (doing now)

### 0. Host-side precheck gate before any guest build — `tools/precheck.sh`
Run the native C89 gate **before** spending a guest build cycle:
```
./tools/precheck.sh        # ~15 s, all 528 manifest files, parallel on host cores
```
Apple clang in CW8-approximating C89 mode (designated initializers, `//`
comments, for-scope declarations, mid-block declarations, VLAs all gate as
errors; constructs CW8 accepts are suppressed and documented in the script).
Most historical fix rounds died on errors this catches in seconds. It also
cross-checks the `MacSurf.mcp` manifest against the repo (stale entries,
missing files) and surfaces Mac–Linux source drift. It does NOT replace the
real CW8 build (CW8 has its own miscompiles and MSL quirks) — it eliminates
the cheap failures. Frontend Toolbox files need Universal Interfaces headers
(`UNIV_HDRS=...`) and are skipped without them; ~30 files with host-SDK header
collisions are skipped (documented in the script) and remain covered by the
maintainer's Linux-side `scripts/verify_macsurf.sh`.

### A. Launch the VM from a Terminal so it gets P-cores
An agent/harness-launched `qemu` inherits **nice +5** and macOS demotes it to the
slow E-cores under P-core contention (~1.5–2.5× slower). A Terminal-launched
`qemu` is nice 0 and gets the P-cores.
- **Permanent:** run `tools/qemu/launch.sh run` from a normal Terminal.
- **Live fix for an already-running niced VM (sudo, non-disruptive):**
  `sudo renice 0 -p <pid>` (NOTE: absolute form; `renice -n 0` is a no-op on
  macOS because `-n` means a *relative increment*). Optionally
  `sudo taskpolicy -B -p <pid>`.
- **Verify P vs E:** `sudo powermetrics --samplers cpu_power -i 1000 -n 3` — the
  ~100% core should sit in the P-Cluster at high clock. Non-sudo: Activity Monitor
  → Window → CPU History.

### B. Virtual Memory off + lean extension set
In the guest: Memory control panel → **Virtual Memory: Off** (no paging on an
emulated disk; pointless with 512 MB). Extensions Manager → a **minimal set**:
keep CarbonLib 1.5, MetroNub, MRO, the USB Tablet INIT, and Open Transport
networking; drop multimedia / file-sharing / cosmetic INITs. Reboot to apply.
Frees background cycles the cooperative scheduler would otherwise hand out, and
removes paging overhead. Small on cpubench, better seen in build wall-clock.

### C. Incremental builds (plain Make, not Remove Object Code → full rebuild)
When you've only edited source, use **Make (Cmd-M)** — CW8 recompiles just the
changed TUs + dependents (5–50× less work than 530 files). Reserve "Remove Object
Code → full rebuild" for project-structure / prefix / access-path changes. Keep a
durable post-clean-build qcow2 so the cold 530-file cost is paid once, then clone
+ incremental from it.

## Results (fill in as measured)

cpubench, "M int-iters/s" (and FP / mem if useful). One change per row.

| Condition | int M/s | FP M/s | mem MB/s | notes |
|---|---|---|---|---|
| nice 0 (P-core), Debug target (-O0) | 21.60 | 0.11 | 566 | unoptimized; run via debugger |
| nice 0 (P-core), Final target (-O2) | **91.46** | 0.13 | 652 | optimized; 4.2x int vs -O0. 50M iters in 0.55s |
| + E-core (renice +5) | _TBD_ | | | raise nice (no sudo); expect ~0.4–0.7x int |
| + Virtual Memory off / lean extensions | _TBD_ | | | expect ~flat on cpubench |

**Reading the int number:** 91.46 M iters/s, ~4-5 ops/iter ≈ ~410 M integer ops/s on the
emulator (M5 P-core, TCG). Rough feel: comparable to a ~250-400 MHz G3 for *integer*
work. **Float is the killer:** 0.13 M transcendental-iters/s (sqrt+sin+cos per iter) is
orders of magnitude below real-G3 hardware FPU — TCG translating PPC FP is the worst case.
True "comparative MHz" still needs the SAME binary run on a real G3/G4, then divide.
(All runs were launched via Cmd-R under the debugger; for a compute loop the debugger's
run-to-completion overhead is minor vs the -O0/-O2 gap, which dominates.)

Build wall-clock:

| Build | time | notes |
|---|---|---|
| Cold full rebuild (530 files), prefix-fix validation | _~TBD_ | compiles clean (0 err thru 256 files); stopped for benchmark |
| Incremental Make after touching 1 file | _TBD_ | the lever-C win |

### Speedometer 4.02 (do NOT trust under emulation)

Ran Performance Rating on the QEMU guest. Results are **unreliable** and should not be
used for a G3/G4 comparison: Speedometer misdetected the CPU as **MC68020** (it's
emulated PPC), so it appears to have run the **68K** test code path; Graphics scored
**0.00** (QuickDraw test failed under TCG); and the overall **PR = 0.00** (a zero
sub-score zeroes the geometric mean). Raw scores vs Quadra 605 = 1.0: CPU 66.4, Math
2207, Disk 4.55, Graphics 0, PR 0. The low CPU (a real G3 would be many hundreds x a
Quadra 605) confirms it's not measuring native PPC. **Use cpubench, not Speedometer,
for emulator-vs-hardware.** Speedometer would only be meaningful run on the real G3.

## Deferred — possible optimizations to try later

Parked, in rough payoff order. Each needs its own before/after measurement.
(Items 5–9 added 2026-06-01 from the [EMULATION-FRONTIER.md](EMULATION-FRONTIER.md)
research round.)

1. **`-cpu g3` / `750` instead of `g4`** — a C compiler emits no AltiVec, so
   emulating the G4 vector unit is wasted translation work. Speculative 5–20% on
   cpubench/build. RISK: our config flags g4 for 9.2.x — **must test on a CLONED
   disk** (confirm it still boots + CarbonLib loads) before adopting. Try:
   `cp os9-cw.qcow2 os9-g3test.qcow2`, launch a second instance with `-cpu g3` and
   unique QMP/monitor sockets, boot-check, run cpubench.
2. **QEMU source build with `-O3 -flto`** (+optional PGO) — reliable 3–8% on the
   single TCG thread. `../configure --target-list=ppc-softmmu
   --extra-cflags="-O3 -flto -mcpu=native" --extra-ldflags="-flto"`. Keep the
   Homebrew binary as fallback. PGO adds ~2–4% more for real extra effort.
3. **`cache=unsafe,aio=threads` on the `-drive`** — removes host fsync stalls from
   HFS's flush-happy metadata writes. 0–25% IF the build is I/O-stalling (it's
   mostly CPU-bound, so likely low). DATA SAFETY: a crash can corrupt the image —
   only on a disposable working copy, never the pristine qcow2.
4. **Headless (`-display none`)** for unattended builds — small free gain; QMP
   `screendump` still drives the GUI. Lower res/depth (800x600x16) is a smaller
   variant if you keep a visible window. Refresh runs off the vCPU thread, so the
   win is second-order.
5. **Warm snapshots for the build loop** — `vm.py snapshot cw_warm` with CW8
   open + project loaded; restore in seconds instead of the ~2 min boot.
   Requires moving file injection from the raw-image round-trip (which flattens
   snapshots, see GOTCHAS.md) to the network path (SLIRP hostfwd). ~100 s saved
   per iteration.
6. **2-VM build/test pipeline** — clone the qcow2; one VM builds round N+1
   while the other tests round N's binary. Pure latency hiding; needs two
   P-cores (Terminal-launched, both nice 0).
7. **`howvec` TCG-plugin run during one build** — settles whether the FP
   softfloat ceiling matters for compile workloads (almost certainly not —
   compilers are integer-bound — but it's a one-run check).
   `-plugin contrib/plugins/libhowvec.so -d plugin`.
8. **Sharded compilation across 4 cloned VMs** (the big lever, ~3–3.5x on cold
   full builds) — split into CW8 **Library targets** (libcss / libdom+hubbub /
   parserutils+duktape / core+frontend), one VM per shard via AppleEvents
   automation, link once in a fifth project. **Gated on the first
   single-target build working.** Details in
   [EMULATION-FRONTIER.md §4](EMULATION-FRONTIER.md).
9. **SheepShaver arm64-JIT spike** — potentially ~5x (see corrected entry under
   "Confirmed non-levers" below); unverified for CW8 builds.

## Confirmed non-levers (do not chase)

- `tb-size` — live `info jit` showed **TB flush count 0**, only 294 MB of 512 MB
  used. Not thrashing. Raising it does nothing here.
- `-smp >1`, MTTCG, HVF/KVM, iothreads — uniprocessor guest, no parallelism path.
  **Now research-backed** (2026-06-01): speculative cross-core execution of a
  serial guest tops out at ~1.09x in the published limit studies, and
  background-translation systems (HQEMU) are dead projects. Citations in
  [EMULATION-FRONTIER.md §1](EMULATION-FRONTIER.md). Closed permanently.
- PPC FP speed under TCG — softfloat **by upstream design** (PPC FI-bit
  semantics disqualify QEMU's hardfloat path permanently). No fix coming;
  likely irrelevant for compile workloads (lever 7 confirms). Details in
  EMULATION-FRONTIER.md §3.
- More guest RAM (>512 MB) — doesn't help a CPU-bound compile and destabilizes
  OS 9 on this mac99 config.
- In-guest RAM disk — redundant with host write caching; wastes scarce guest RAM.
- UTM — it's the same TCG engine under a GUI.
- ~~SheepShaver migration — faster JIT per-instruction but likely interpreter-only
  on arm64, CW debugger trap-crashes, fragile networking. Spike at most.~~
  **Corrected 2026-06-01 — moved to deferred lever 9.** The original entry
  overstated what we know: (a) a native **arm64 JIT exists** for SheepShaver
  (kanjitalk755/macemu line) benchmarking ~470% of a G3/300 vs ~96% for the
  interpreter — roughly **5x our TCG setup** — though it's flaky and not in
  stock builds; (b) the "CW debugger trap-crashes" claim was our own note, not
  the upstream maintainer's experience — **the maintainer has never compiled in
  SheepShaver** (they use it strictly for runtime smoke tests), and nobody has
  tested whether CW8 IDE *builds* (no debugger involved) work there. Known real
  limitation: no MMU emulation → MacsBug/debuggers unavailable. Still a spike,
  but a more promising one than this entry previously suggested. See
  EMULATION-FRONTIER.md §2.
