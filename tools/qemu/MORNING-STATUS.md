# Status / handoff — QEMU OS 9 harness

Living status of the off-hardware MacSurf build/test harness. Newest first.

## Headline: full autonomous GUI control achieved ✅

The OS 9 guest can now be driven end-to-end with **no human**: keyboard, **absolute
mouse clicks**, and host-side file injection all work. CodeWarrior 8 is installed
(host-side) and launches; the first-launch "Where is Mac OS X?" dialog was clicked
through autonomously. This removes the click blocker that capped the previous run.

### How clicks were solved
- Root cause: OS 9's polled HID misses an instant down→up, and its `usb-mouse`
  driver ignores standalone button reports — so QMP clicks never registered.
- Fix (proven, community-standard): the **`kanjitalk755` USB Tablet INIT** in
  `System Folder:Extensions:` + **`-M mac99` (no via=pmu)** + **`-device usb-tablet`**.
  OS 9 then tracks absolute pointer events; QMP `input-send-event` abs-move + a
  **dwelled** btn down/up register as real clicks. `vm.py click X Y` works in
  absolute framebuffer coordinates (no calibration).
- Config defaults now: `QEMU_MACHINE=mac99`, `QEMU_POINTER=usb-tablet`. Keyboard
  (`send-key`) confirmed still working under mac99 without via=pmu.
- Full investigation + fallbacks (VNC server, KeyQuencer, host CGEvent): `docs/INPUT.md`.

## What's installed / verified
- OS 9.2.1 boots (UTM prebuilt image baseline).
- **CodeWarrior 8** installed entirely from the host (mount the qcow2's HFS+ volume,
  `ditto` the IDE folder + CarbonLib 1.5 + MetroNub + MRO + the Tablet INIT). IDE
  launches with full menus; no installer, no serial.
- Driver verbs: `vm.py screenshot/key/type/move/click/settle/snapshot/revert/quit`.

## In progress: getting MacSurf to build
- The project's access paths are **absolute** to a volume named **`Back40`**
  (`Back40:Desktop Folder:patrick:macsurf-source Folder:browser:…`). Plan: stage a
  "Back40" source disk matching that layout so the existing `.mcp` resolves unchanged.
- `stage-project.sh` builds it — but `machfs/MakeHFS` force-decodes TEXT files as
  UTF-8 and some source has non-UTF-8 bytes (0x81), so the charset-agnostic path is
  HFS+ mount + `ditto` + `SetFile` (type/creator) + a byte-safe LF→CR pass instead.
- **Open question being tested:** the git `MacSurf.mcp` is data-fork-only (176 KB, no
  resource fork). Testing whether CW8 opens it as a valid project (type set to
  `MMPr`/`CWIE`). If yes → full source staging + a Make. If CW rejects it, the build
  needs the user's authoritative `.mcp` (CLAUDE.md: "user maintains the project file
  list on the Mac side") and/or a CodeWarrior XML project export to import.

## The no-mouse build path (works regardless of clicks)
Per the research sweep: CodeWarrior is fully scriptable via AppleEvents. Run a build
AppleScript from **Script Editor** (Cmd-O the script, **Cmd-R** to run — no applet
compile); `Make` returns the error list **as data** → write it to a file with a full
HFS path (no Save dialog). Project + access paths import from **XML**. See
`docs/INPUT.md` and `build-robot.applescript`.

## If you want to watch it live
```bash
cd ~/Documents/git/macsurf
./tools/qemu/launch.sh run        # Cocoa window; boots os9-cw.qcow2 (CW + Tablet INIT)
```

## Disk lineage (~/macsurf-qemu/images/)
| image | what |
|---|---|
| `os9-cw.qcow2` | OS 9.2.1 + CodeWarrior 8 + Tablet INIT + dialog-dismissed — **working baseline** |
| `os9-utm.qcow2` | clean OS 9.2.1 (os9_clean snapshot) — fallback |
| `media/Mac OS 9.2.1.utm/Images/disk-0.qcow2` | pristine UTM original — untouched backup |

## Apple Silicon perf (asked earlier)
Native arm64 QEMU (no Rosetta); `split-wx` + `tb-size=512` set; no PPC accel exists
on arm64 (pure single-thread TCG ceiling). Real wins are architectural: snapshot-
revert instead of reboot, and `cache=unsafe` on a disposable build overlay.
