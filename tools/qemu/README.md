# tools/qemu — Mac OS 9 build & test harness (QEMU on Apple Silicon)

Run Mac OS 9.2.x (PowerPC) under QEMU on a modern Mac to get an **off-hardware
build-and-test loop** for MacSurf: drive the guest GUI autonomously (keyboard +
mouse + screenshots), inject/extract files from the host, and compile the
CodeWarrior project — without the physical G3/G4 in the loop. Unlike SheepShaver,
this setup has **working networking** (the guest can reach the MacSurf proxy).

> Not a replacement for real-hardware verification — wheel/scroll/USB Overdrive
> behaviour and the 9.1↔9.2.2 font delta still need a real Mac. This is the fast
> smoke/integration tier.

## Capabilities (all verified working)

| Capability | How |
|---|---|
| Boot OS 9.2.1 headless or in a window | `launch.sh` (UTM prebuilt image baseline) |
| Screenshot the guest anytime | `vm.py screenshot out.png` (QMP, works headless) |
| Type + keyboard shortcuts | `vm.py type "text"`, `vm.py key meta_l-m` |
| **Absolute mouse clicks** | `vm.py click X Y` (needs the usb-tablet INIT — see below) |
| Pick menu items | `vm.py pressdrag Xtitle 7 Xitem Yitem` (menus need press-drag) |
| Inject/extract files (resource forks intact) | mount the qcow2's HFS+ on the host (`stage-on-bootvol.sh` pattern) |
| Ship source in with Mac CR + TEXT/CWIE types | `stage-on-bootvol.sh` / `inject-source.sh` |
| Snapshot / instant-revert VM state | `vm.py snapshot TAG` / `vm.py revert TAG` |
| CodeWarrior installed + scriptable | host-injected; drive via GUI or AppleEvents |

## Quick start

```bash
# 0. one-time host setup
brew install qemu hfsutils unar
python3 -m venv ~/macsurf-qemu/venv && ~/macsurf-qemu/venv/bin/pip install machfs

# 1. launch the working baseline (OS 9.2.1 + CodeWarrior 8 + source staged)
./tools/qemu/launch.sh run        # Cocoa window (you drive with real mouse/kbd)
./tools/qemu/launch.sh headless   # no window; drive it via vm.py

# 2. drive it
./tools/qemu/vm.py screenshot /tmp/now.png       # see the screen
./tools/qemu/vm.py key ret                        # press Return (dialog defaults)
./tools/qemu/vm.py type "Metrowerks"              # type / Finder type-select
./tools/qemu/vm.py click 362 180                  # absolute click (640x480 coords)
./tools/qemu/vm.py pressdrag 48 7 108 302         # File menu -> drag to item
./tools/qemu/vm.py quit                           # kill the VM (hard exit)
```

The first boot after a hard `quit` shows a "did not shut down properly" dialog —
dismiss with `vm.py key ret`. Boots take ~2 min under TCG; check progress with
screenshots, or `vm.py settle` to block until the screen stops changing.

## The input setup that makes clicks work (do not change casually)

OS 9 ignores QMP mouse clicks on a normal `usb-mouse`, and ignores `usb-tablet`
entirely *unless* the guest has the **kanjitalk755 "USB Tablet INIT"** installed in
`System Folder:Extensions:` **and** QEMU runs with **`-M mac99` (NOT `via=pmu`)** +
**`-device usb-tablet`**. The working baseline disk already has the INIT installed
and `config.sh` defaults to this combination. Full story: [docs/INPUT.md](docs/INPUT.md).

## Building MacSurf in the guest (current status)

The repo's `MacSurf.mcp` is a hand-maintained manifest that CW8 cannot open or
import directly (see `docs/GOTCHAS.md`). The working flow:

1. Source is staged on the boot volume at
   `Back40:Desktop Folder:patrick:macsurf-source Folder:browser:…`
   (`stage-on-bootvol.sh`; boot volume renamed "Back40").
2. **Generate an importable project**: `python3 tools/codewarrior/manifest-to-mcpxml.py`
   → `MacSurf-import.xml` (genuine CW8 schema, absolute paths — location-independent).
3. Stage it into the guest (`guest-cp.sh`), then in CodeWarrior:
   **File → Import Project** → select it → type a name + Return (save anywhere).
4. **Make** (Cmd-M). First build under TCG is slow (expect 30–60 min).

See [MORNING-STATUS.md](MORNING-STATUS.md) for exactly where this stands.

## Scripts reference

| script | purpose |
|---|---|
| `config.sh` | paths + locked VM hardware defaults (sourced by everything) |
| `launch.sh {install\|run\|headless}` | start the VM (install ISO / Cocoa window / headless+QMP) |
| `vm.py <verb>` | the driver: screenshot, key, type, move, rmove, click, press, pressdrag, settle, snapshot, revert, snapshots, powerdown, quit, status, probe-tablet |
| `qmp.py <sock> <cmd>` | thin one-shot QMP client (vm.py is the ergonomic layer) |
| `screenshot.sh [out.png]` | convenience framebuffer grab |
| `stage-on-bootvol.sh` | stage the repo source ONTO the boot volume at the Back40 paths (CR + TEXT/CWIE) |
| `inject-source.sh <dir> [img]` | build a separate HFS transfer disk from a folder (machfs) |
| `read-results.sh <img> [dest]` | extract files from an HFS image back to the host |
| `create-disks.sh` | create a blank system qcow2 (for from-ISO installs) |
| `stage-project.sh` | (superseded by stage-on-bootvol.sh; kept for the machfs approach) |
| `guest-cp.sh <file> <guest-path> [TYPE CREATOR]` | copy a single host file into the guest disk (type/creator + optional CR) |

CodeWarrior project tooling (the manifest converter, build-robot AppleScript) lives
in **[`tools/codewarrior/`](../codewarrior/)** — it works on any Mac, not just QEMU.

## Disk images (~/macsurf-qemu/images/ — never committed)

| image | what |
|---|---|
| `os9-cw.qcow2` | **working baseline**: OS 9.2.1 + CodeWarrior 8 + Tablet INIT + source staged ("Back40") |
| `os9-utm.qcow2` | clean OS 9.2.1 (has `os9_clean` snapshot) — fallback |
| `os9-system.qcow2` | abandoned from-ISO install attempt |
| `media/Mac OS 9.2.1.utm/Images/disk-0.qcow2` | pristine UTM original — untouched backup |

If `os9-cw.qcow2` is ever damaged: copy the pristine UTM disk back and re-run the
CodeWarrior injection + `stage-on-bootvol.sh` (both are scripted/documented).

## Docs index

- **[SETUP.md](SETUP.md)** — full setup from scratch: media, flags, install paths
- **[docs/INPUT.md](docs/INPUT.md)** — input/automation deep-dive: why clicks fail without
  the INIT, GUI-driving techniques, the no-mouse AppleEvents build path
- **[docs/GOTCHAS.md](docs/GOTCHAS.md)** — every trap we hit, so you don't hit it twice
- **[MORNING-STATUS.md](MORNING-STATUS.md)** — live status / handoff
