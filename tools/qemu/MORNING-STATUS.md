# Morning status — QEMU OS 9 harness (overnight run, 2026-05-31)

TL;DR: **OS 9.2.1 boots under our QEMU harness, and CodeWarrior 8 is fully
installed and launches — installed entirely from the host, no GUI installer,
no serial number.** The one thing I *can't* do solo is inject mouse clicks, so
the last-mile GUI setup (≈5 min) is yours. Everything else is built and verified.

## What works (verified with screenshots)

1. **OS 9.2.1 boots** on `mac99,via=pmu` (QEMU 11, native arm64). We pivoted from
   the from-ISO install (slow under TCG — it was actually progressing, not hung,
   but not worth babysitting) to the **UTM prebuilt image** you linked. Clean desktop.
2. **CodeWarrior 8 installed via host-side injection.** I mounted the OS 9 disk's
   HFS+ volume directly on macOS (`hdiutil` + `CRawDiskImage`), `ditto`-copied the
   "Metrowerks CodeWarrior" folder + CarbonLib 1.5 + MetroNub + the MRO scripting
   addition, repackaged to qcow2, booted, and **launched the IDE** — resource forks,
   icon, `license.dat` all intact. No installer, no serial prompt.
3. **Keyboard automation is solid** — `vm.py type/key`, dialog defaults via Return,
   Finder type-to-select + Cmd-O navigation all work.
4. **Relative mouse *motion* works** (with ~1.5× OS 9 acceleration; screenshot-feedback
   calibration converges). **Host-side HFS+ file injection works** (read+write).

## The one hard limitation

**QMP-injected mouse *clicks* do not register on OS 9** (motion does). Tried:
`input-send-event` press, 250 ms hold, motion+button batched, legacy HMP
`mouse_button`. None dismissed a dialog. Your *native* clicks in the Cocoa window
worked fine during install — so clicking is available **via the Cocoa window with
your real mouse**, just not via my injection. This is the autonomy boundary.

## Your ≈5-minute morning steps (in a Cocoa window with your mouse)

```bash
cd ~/Documents/git/macsurf
./tools/qemu/launch.sh run        # boots os9-cw.qcow2 (CW installed) in a window
```
1. Open the **"untitled"** disk → **Metrowerks CodeWarrior 8.0 → Metrowerks
   CodeWarrior → CodeWarrior IDE**. Launch it.
2. On the **"Where is Mac OS X?"** first-launch dialog, click **Continue** (we build
   Classic CFM, not Mach-O). This sets a pref so it won't ask again — important so the
   scripted build path isn't blocked by it.
3. Set up the MacSurf project: get the `browser/` tree + `MacSurf.mcp` onto the disk
   and confirm the access paths (per the main CLAUDE.md). I can help wire this up
   interactively — I can host-inject the source tree onto the disk (with Mac CR +
   TEXT/CWIE via machfs/ditto); the `.mcp`/access-paths are your domain.
4. Once a **manual build is green**, ping me — from there I can likely drive rebuilds
   via keyboard alone (launch Script Editor, type a `tell application "CodeWarrior IDE"
   … Make Project … Save Error Window As` script, Cmd-R — no mouse, no applet compile).

## After that — the automated loop (no mouse needed)

- `inject-source.sh` → ship changed source onto a transfer HFS disk (CR + TEXT/CWIE).
- Script Editor + keyboard runs the build AppleScript (`build-robot.applescript` is the
  reference; for a no-clicks path I'll type it into Script Editor and Cmd-R instead of
  compiling an applet, since Save-as-applet needs a click).
- `read-results.sh` → pull `build-errors.txt` + the binary back to the host.
- Snapshot `os9_cw_ready` once the project builds, for instant revert per build.

## On "is our config suboptimal for Apple Silicon?"

No fixable suboptimality found. We're **native arm64** (no Rosetta — the one big
pitfall, avoided), with `split-wx=on` (JIT under hardened runtime) and `tb-size=512`
(code cache). There is **no PPC accelerator on any host** (no HVF/KVM for ppc-on-arm64)
— pure single-thread TCG is the ceiling; newer M-chips just run it faster per core.
The real speed wins are architectural: **snapshot-revert instead of reboot**, and
`cache=unsafe` on a disposable build overlay. Both are in the design.

## Disk lineage (in ~/macsurf-qemu/images/)

| file | what | role |
|---|---|---|
| `os9-cw.qcow2` | OS 9.2.1 + CodeWarrior 8 | **working baseline** (config default) |
| `os9-utm.qcow2` | clean OS 9.2.1 (has `os9_clean` snapshot) | fallback |
| `os9-system.qcow2` | abandoned from-ISO install | reference |
| `media/Mac OS 9.2.1.utm/Images/disk-0.qcow2` | pristine UTM original | untouched backup |

## New/changed this run
- `vm.py`: added `rel`/`rmove` + `press` (relative mouse, for the calibration work).
- `config.sh`: `QEMU_POINTER`, accel tuning, `QEMU_SYSDISK` → os9-cw.qcow2.
- Screenshots of every milestone in `~/macsurf-qemu/logs/`.
- The Mac was kept awake overnight via `caffeinate` (auto-expires).
