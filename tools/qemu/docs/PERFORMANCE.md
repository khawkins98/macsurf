# QEMU OS 9 build performance — playbook + measurements

How fast the off-hardware build loop runs, what we changed, and what's left to try.
Companion to the "Performance / CPU utilization" section in [GOTCHAS.md](GOTCHAS.md).

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
| Baseline (agent-launched, niced +5, VM on, full extensions) | _TBD_ | | | likely on E-cores |
| + P-core (Terminal launch, nice 0) | _TBD_ | | | expect 1.5–2.5× int |
| + Virtual Memory off / lean extensions | _TBD_ | | | expect ~flat on cpubench |

Build wall-clock:

| Build | time | notes |
|---|---|---|
| Cold full rebuild (530 files), prefix-fix validation | _~TBD_ | first 8.3 clean build |
| Incremental Make after touching 1 file | _TBD_ | the lever-C win |

## Deferred — possible optimizations to try later

Parked, in rough payoff order. Each needs its own before/after measurement.

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

## Confirmed non-levers (do not chase)

- `tb-size` — live `info jit` showed **TB flush count 0**, only 294 MB of 512 MB
  used. Not thrashing. Raising it does nothing here.
- `-smp >1`, MTTCG, HVF/KVM, iothreads — uniprocessor guest, no parallelism path.
- More guest RAM (>512 MB) — doesn't help a CPU-bound compile and destabilizes
  OS 9 on this mac99 config.
- In-guest RAM disk — redundant with host write caching; wastes scarce guest RAM.
- UTM — it's the same TCG engine under a GUI.
- SheepShaver migration — faster JIT per-instruction but likely interpreter-only
  on arm64, CW debugger trap-crashes, fragile networking. Spike at most.
