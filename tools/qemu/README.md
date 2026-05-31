# tools/qemu — Mac OS 9 build & test harness (QEMU on Apple Silicon)

Run Mac OS 9.2.x (PowerPC) under QEMU on a modern Mac to get an **off-hardware
build-and-test loop** for MacSurf: compile the CodeWarrior project and exercise
the browser's fetch path against the proxy without the physical G3/G4 in the loop
for every iteration. Unlike SheepShaver, QEMU here has **working networking**.

> Not a replacement for real-hardware verification — hardware-specific behaviour
> (wheel/scroll crashes, USB Overdrive, the 9.1↔9.2.2 font delta) still needs a
> real Mac. QEMU is the fast smoke/integration tier.

## Start here

- **[SETUP.md](SETUP.md)** — full setup guide, locked QEMU flags, the automated
  build-loop design, and gotchas. Read this first.
- **[MORNING-STATUS.md](MORNING-STATUS.md)** — current state / handoff: what's
  working, what's blocked, and the exact remaining manual steps.

## The scripts

| script | purpose |
|---|---|
| `config.sh` | paths + locked VM hardware defaults (sourced by everything) |
| `create-disks.sh` | create the OS 9 system qcow2 |
| `launch.sh {install\|run\|headless}` | start the VM (Cocoa window or headless+QMP) |
| `vm.py` | high-level driver: `screenshot/key/type/move/rmove/press/click/settle/snapshot/revert/quit` |
| `qmp.py` | thin one-shot QMP client |
| `screenshot.sh` | convenience framebuffer grab via QMP |
| `inject-source.sh` | build an HFS transfer disk from a source folder (CR + TEXT/CWIE) |
| `read-results.sh` | pull build output/logs back to the host |
| `build-robot.applescript` | in-guest CodeWarrior driver (AppleEvents; reference) |

## Current state (one-liner)

OS 9.2.1 boots; **CodeWarrior 8 is installed (host-side injection) and launches**;
keyboard + relative-mouse-motion + host HFS+ file injection all work. The open
blocker is **QMP-injected mouse clicks don't register on OS 9** — see
MORNING-STATUS.md and `docs/INPUT.md` for the investigation and workarounds.

## Workspace (outside the repo)

`~/macsurf-qemu/` holds the disk images, ISOs, snapshots, and logs — never
committed. Only these scripts + docs are tracked.

| image | what |
|---|---|
| `images/os9-cw.qcow2` | OS 9.2.1 + CodeWarrior 8 — **working baseline** (config default) |
| `images/os9-utm.qcow2` | clean OS 9.2.1 (has `os9_clean` snapshot) — fallback |
| `media/Mac OS 9.2.1.utm/Images/disk-0.qcow2` | pristine UTM original — untouched backup |
