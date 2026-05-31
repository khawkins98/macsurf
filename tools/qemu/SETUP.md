# MacSurf on QEMU — Mac OS 9 build & test harness

Run Mac OS 9.2.x PowerPC under QEMU on a modern Mac (incl. Apple Silicon) to
get an **off-hardware build-and-test loop** for MacSurf: compile the CodeWarrior
project and exercise the browser's fetch path against the proxy without needing
the physical G3/G4 in the loop for every iteration.

> **Scope note.** This is a *development accelerator*, not a replacement for
> real-hardware verification. Hardware-specific behaviour (the wheel/scroll
> crashes, USB Overdrive, the 9.1-vs-9.2.2 font AA delta) still needs a real
> G3/G4. QEMU is the smoke/integration tier — and, unlike SheepShaver, it has
> **working networking**, so the fetcher/proxy path *can* be exercised here.

---

## Architecture

```
  macOS host (Apple Silicon)                    OS 9.2.x guest (emulated G4)
  ────────────────────────────                  ───────────────────────────
  qemu-system-ppc (TCG, no accel)  ── framebuffer ──>  Cocoa window (you watch)
        │  QMP unix socket          ── screendump ───>  PNG (the agent watches)
        │                                              CodeWarrior 8 IDE
        │  -drive transfer.img (HFS) <── build out ──   + build-robot applet
        │                                              (AppleScript, no mouse)
        │  -netdev user (SLIRP NAT)                     OS 9 Open Transport
        └─ guest reaches host at 10.0.2.2 ──────────>   MacSurf proxy :8765
```

Three host-side pieces, all in `tools/qemu/`:
- **`launch.sh`** — start the VM (`install` / `run` / `headless` modes).
- **`vm.py`** — high-level driver: `screenshot / key / type / click / settle / snapshot / revert`.
- **`inject-source.sh` / `read-results.sh`** — move files in/out via a secondary HFS disk (`machfs`).
- **`build-robot.applescript`** — runs *inside* the guest; drives CodeWarrior over AppleEvents.

The VM workspace (disk images, ISOs, snapshots, logs) lives **outside the repo**
at `~/macsurf-qemu/` so multi-GB binaries never get committed. Only the scripts
and this doc are tracked.

---

## Prerequisites

```bash
brew install qemu hfsutils           # qemu-system-ppc 8.x+ ; hfsutils for raw HFS peeking
brew install unar                    # extract StuffIt (.sit) / Toast media (if needed)
python3 -m venv ~/macsurf-qemu/venv  # machfs lives here (pure-Python HFS, arm64-native)
~/macsurf-qemu/venv/bin/pip install machfs
```

**Media you must supply** (Mac OS 9 is abandonware Apple never re-released; use
your own legally-obtained copies):
- A **bootable Mac OS 9.2.x install ISO** (the "9.2.2 Universal" CD installs in one pass; a 9.2.1 CD also works).
- **CodeWarrior Pro 8** install media (a `.toast`/ISO CD image) + the **8.3 updater**.

Place them under `~/macsurf-qemu/media/`.

---

## Hardware flags (locked from research — don't guess these)

| Flag | Value | Why |
|---|---|---|
| `-M` | `mac99,via=pmu` | New World G4 board; `via=pmu` enables USB mouse/kbd |
| `-cpu` | `g4` | Fine for 9.2.x. **`g3` is required ONLY for 9.0/9.1** |
| `-m` | `512` | **HARD LIMITS: won't boot ≤64 MB; unstable / no audio ≥1024 MB.** Stay 256–512 |
| `-g` | `1024x768x32` | OS 9 has no arbitrary-res driver; pin a mode. x32/x16 reliable, x8 flaky |
| `-L` | `/opt/homebrew/share/qemu` | OpenBIOS firmware (no Apple ROM needed) |
| net | `-netdev user,id=net0 -device sungem,netdev=net0` | `sungem`=Apple GMAC, OS 9 has a built-in driver (no install) |
| input | `-device usb-mouse -device usb-kbd` | usb-mouse = **relative** (see "driving the GUI") |
| video drv | `-prom-env 'vga-ndrv?=true'` | loads the bundled VGA NDRV so OS 9 gets video |
| accel | TCG only | **No KVM/HVF for PPC-on-arm64.** Pure software emulation; `-smp 1` (MTTCG is a no-op for ppc) |

---

## Step-by-step

### 1. Create the system disk
```bash
qemu-img create -f qcow2 ~/macsurf-qemu/images/os9-system.qcow2 4G
```

### 2. Install OS 9 (interactive, one-time)
```bash
tools/qemu/launch.sh install      # boots the ISO (-boot d) in a Cocoa window
```
In the guest:
1. **Utilities → Drive Setup** → select the uninitialized disk → **Initialize** as
   **Mac OS Extended (HFS+)**, one partition. (A blank `qemu-img` disk does NOT
   appear until initialized.)
2. **Mac OS Install** → Select Destination = the new volume → **Start**. Accept
   defaults. (Several minutes — TCG, no accel.)
3. **Special → Shut Down.**

### 3. Install CodeWarrior 8 + 8.3 update (interactive, one-time)
```bash
tools/qemu/launch.sh run --cdrom "~/macsurf-qemu/media/CodeWarrior 8 Pro.toast"
```
Install CW8 from the mounted CD, then apply the 8.3 updater (inject it via a
transfer disk — see step 5 — or mount it as a CD). Open `MacSurf.mcp` once, set
the access paths (see the project's CLAUDE.md "Access Paths"), and confirm a
manual build works before automating.

### 4. Configure networking (so the guest reaches the proxy)
In the OS 9 **TCP/IP** control panel: *Connect via* = **Ethernet (built-in)**,
*Configure* = **Using DHCP Server** (SLIRP serves 10.0.2.15 / gw 10.0.2.2 / DNS
10.0.2.3). If DHCP DNS doesn't take, set Manually: IP `10.0.2.15`, mask
`255.255.255.0`, router `10.0.2.2`, DNS `10.0.2.3`.

**The MacSurf proxy on the host is reachable from the guest at `10.0.2.2:8765`**
(SLIRP gateway = host). Point MacSurf's proxy preference there. Ensure the proxy
binds `0.0.0.0`, not just `127.0.0.1`.

### 5. Snapshot the baseline
Once OS 9 + CW8 + access paths + the build-robot applet (in Startup Items) are
set up and a manual build is green:
```bash
tools/qemu/vm.py snapshot os9_cw_ready     # qcow2 internal snapshot
```
Every automated build reverts to this snapshot first.

---

## The automated build loop

```bash
# 1. stage the changed source tree into a transfer HFS disk (LF->CR + TEXT/CWIE applied)
tools/qemu/inject-source.sh /path/to/macsurf/browser ~/macsurf-qemu/images/transfer.img TRANSFER
#    ...and drop a BUILD_REQUEST trigger file onto it (the robot polls for this)

# 2. boot headless from the baseline snapshot with the transfer disk attached
tools/qemu/launch.sh headless --transfer ~/macsurf-qemu/images/transfer.img &
tools/qemu/vm.py revert os9_cw_ready

# 3. the in-guest build-robot applet sees the trigger, runs:
#       open MacSurf.mcp -> Set Current Target -> Remove Object Code
#       -> Make Project -> Save Error Window As -> write status + BUILD_DONE
#    (all AppleEvents — no mouse)

# 4. host waits for BUILD_DONE, quits the VM, reads results back out
tools/qemu/vm.py quit
tools/qemu/read-results.sh ~/macsurf-qemu/images/transfer.img ~/macsurf-qemu/results
#    -> build-errors.txt, build-status.txt (OK n / FAIL 5 ...), and the linked binary
```

---

## Driving the GUI (the relative-mouse caveat)

- **Keyboard is fully scriptable** — `vm.py key meta_l-b` (Cmd-B / Make), `vm.py type "…"`.
- **Mouse is the catch.** `usb-mouse` is a *relative* device, so `vm.py click X Y`
  (absolute) only lands if the VM uses an **absolute** device (`usb-tablet`).
  OS 9's HID support for `usb-tablet` is historically flaky — run a probe boot
  with `-device usb-tablet` and `vm.py probe-tablet` to see if THIS guest tracks
  it. If yes, the agent can drive the whole GUI. If no, do one-time setup clicks
  by hand; the recurring build loop needs no mouse regardless.

---

## Scripts reference

| Script | Purpose |
|---|---|
| `config.sh` | Paths + locked VM hardware defaults (sourced by the others) |
| `launch.sh {install,run,headless}` | Start the VM in the right mode |
| `vm.py <verb>` | High-level driver: screenshot/key/type/click/settle/snapshot/revert/quit |
| `qmp.py <sock> <cmd>` | Thin one-shot QMP client (vm.py is the ergonomic layer on top) |
| `screenshot.sh [out.png]` | Convenience framebuffer grab |
| `inject-source.sh <dir> [img] [vol]` | Build an HFS transfer disk (LF→CR, TEXT/CWIE) |
| `read-results.sh <img> [dest]` | Extract build output/logs back to the host |
| `build-robot.applescript` | In-guest CodeWarrior driver (compile as stay-open applet) |

---

## Known gotchas / hard limits

- **RAM band is 256–512 MB. Never 1 GB+** (no audio ≥1024, instability past ~768; won't boot ≤64).
- **No PPC acceleration on any host** — pure TCG, ~10% of native. `-smp 1`; `thread=multi` is a no-op for ppc. A full 480-file CW build will be slow: a "kick it and walk away" loop, not interactive.
- **Modern macOS has zero classic-HFS support** (removed in Catalina). Don't try to `hdiutil`/Finder-mount the OS 9 volume. Use `machfs`/`hfsutils` on raw images instead — that's why `inject-source.sh`/`read-results.sh` exist.
- **9p/virtfs does NOT work** for an OS 9 guest (no 9p client). File exchange is via a secondary HFS disk image only.
- **No native OS 9 command-line compiler exists.** Metrowerks CLI tools (`mwccppc` etc.) are Windows/Solaris/OS X only. On OS 9 the *only* headless build path is AppleScript driving the GUI IDE — hence the build-robot applet.
- **Read the transfer image only when the VM is shut down** (or the disk detached), or you'll get a stale view.
- **Prefer `vm.py quit` over `system_powerdown`** — OS 9 has no ACPI, so the power-button event may not cleanly shut down. Have the guest flush/write its sentinel *before* you read.
- **Don't mix pc-bios across QEMU builds** (e.g. stock vs the screamer-audio fork) — keep each build's matching `-L` dir.

---

## Status

Validated on this host (macOS 26.x, Apple Silicon, QEMU 11.0.1):
- ✅ OS 9.2.1 boots to a usable desktop (UTM prebuilt image; from-ISO also boots, just slow under TCG)
- ✅ Headless + Cocoa display, QMP control, framebuffer screenshots
- ✅ HFS transfer round-trip: LF→CR + `TEXT/CWIE` confirmed on-volume (machfs)
- ✅ **CodeWarrior 8 installed entirely host-side** (mount HFS+, ditto IDE + CarbonLib 1.5 + MetroNub + MRO) — IDE launches
- ✅ Keyboard automation; relative-mouse motion; keyboard confirmed under `-M mac99` (no via=pmu)
- 🔬 usb-tablet + `kanjitalk755` INIT for absolute clicks — under test (INIT in Extensions; `-M mac99 -device usb-tablet`)
- ⏳ MacSurf project wired (XML import / matching access paths) → first green build → `os9_cw_ready` snapshot
- ⏳ end-to-end automated build loop (keyboard + AppleScript; see docs/INPUT.md)

See **docs/INPUT.md** for the full input/automation strategy and **MORNING-STATUS.md** for the live handoff.
```
