# QEMU OS 9 harness — Known Gotchas

Every trap hit while building this harness, so nobody hits it twice. Companion to
the project-wide gotchas in the root CLAUDE.md (those cover the browser code / CW8
compiler; these cover the emulation harness).

## VM / QEMU configuration

- **OS 9 RAM band is 256–512 MB. Never ≥1 GB** (no boot/instability/no audio), never
  ≤64 MB (won't boot). `-m 512` is the locked default.
- **No PPC acceleration exists on any host.** ppc-on-arm64 is pure single-thread TCG
  (~10% native). `-smp 1`; `thread=multi` is a no-op for ppc. The real perf levers:
  `tb-size=512` (set), `split-wx=on` (required on macOS hardened runtime, set),
  snapshot-revert instead of reboot, `cache=unsafe` on disposable build overlays.
- **Guest resolution is not stable across boots** — the same disk boots at 640×480
  or 1024×768 depending on the video driver dance. Always detect the framebuffer
  size from a screenshot (`sips -g pixelWidth …`) before computing click coords.
- **Boots take ~2 min under TCG** and got slower as the disk grew. Don't poll at
  30 s and conclude it hung. The OS 9 installer's time estimates are fiction; use
  screen-motion (changing screenshots) to tell progress from a real hang. A pegged
  CPU + changing screen = working; pegged CPU + frozen screen for 5+ min = livelock.
- **A hard `quit` (QMP) leaves the volume dirty** → next boot runs Disk First Aid
  ("did not shut down properly"). Harmless; dismiss with Return. Only a clean
  in-guest Special→Shut Down avoids it.

## Input (the big one)

- **QMP mouse clicks do NOT register on OS 9's `usb-mouse`.** Motion works, buttons
  never do — OS 9's polled HID driver ignores standalone button reports, and an
  instant down→up falls between polls regardless. No dwell/batching fixes it.
- **`usb-tablet` is ignored by stock OS 9** (no driver). The fix is the
  **kanjitalk755 "USB Tablet INIT"** in `System Folder:Extensions:` — then absolute
  moves AND clicks work via QMP.
- **The INIT requires `-M mac99` WITHOUT `via=pmu`** (its README is explicit; we
  confirmed). USB keyboard still works under bare mac99.
- **The INIT must be in `Extensions`, not loose in the System Folder root** — loose
  placement silently does nothing.
- **Clicks need a dwell** ≥ ~100 ms between button-down and button-up (vm.py
  defaults to 150 ms) or the guest's poll misses the press.
- **Menus: click-then-click does NOT work.** The sticky menu times out in the gap
  between two separate driver calls and the second click falls through to whatever
  is behind it (often switching apps). Use **press-drag** (`vm.py pressdrag`):
  press the menu title, drag to the item, release — atomic in one call.
- **Nav OPEN dialogs vs SAVE dialogs behave differently.** OPEN dialogs (e.g.
  Import's file picker): typing type-selects in the list, Return opens/enters; they
  remember the last folder. SAVE dialogs: typing goes to the **Name field**, NOT
  the list; enter folders by **double-clicking** them; **Cmd-Down does not work**;
  scroll with the scrollbar arrows; **Esc cancels the entire dialog** (don't use it
  to dismiss sub-popups).
- **Dialog default button = Return.** Non-default buttons need a real click.
- **Don't click below a dialog** — stray clicks at the screen bottom hit the
  Control Strip and pop up its modules.

## Host-side disk / file exchange

- **Modern macOS has zero classic-HFS support and hdiutil won't recognize the raw
  OS 9 disk without help.** Attach with
  `hdiutil attach -noverify -imagekey diskimage-class=CRawDiskImage <raw>`.
  qcow2 must be converted to raw first (`qemu-img convert`), and back after edits.
- **Parse the actual mountpoint from hdiutil's output** — never hardcode
  `/Volumes/<name>`. The volume name collides with stale mounts and macOS appends
  " 1", " 2"… Stale mounts accumulate from failed runs; detach them first.
- **`yes | hdiutil` + `set -o pipefail` = silent script death.** `yes` takes
  SIGPIPE when hdiutil closes stdin; pipefail turns that into a failed pipeline and
  `set -e` exits. Disable pipefail around that pipeline (or use a finite `printf`).
- **`ditto` preserves resource forks + type/creator between HFS+ volumes; plain
  file writes do not.** Anything that needs forks (apps, the Tablet INIT) must be
  copied with `ditto` from a fork-bearing source (e.g. an unar-extracted file).
- **`machfs`/`MakeHFS` force-decodes TEXT-typed files as UTF-8** and dies on any
  non-UTF-8 byte in the source tree (MacSurf has some). For staging real source,
  use the HFS+ mount + `ditto` + `SetFile` + byte-safe `perl` CR conversion instead
  (that's what `stage-on-bootvol.sh` does). machfs is fine for small, known-clean
  transfer disks.
- **git strips resource forks and type/creator.** Anything checked out of git is
  data-fork-only with no Mac metadata. Set types host-side (`SetFile -t TEXT -c
  CWIE`) when staging; binary Mac files (real `.mcp`s, apps) cannot round-trip
  through git at all.
- **Never modify the qcow2 while a VM is running on it**, and never run the VM
  while the raw is mounted on the host. One owner at a time.

## CodeWarrior

- **The repo `MacSurf.mcp` is an XML project export, not a binary project.** Use
  **File → Import Project** on it. **File → Open fails** with "Resource File Error
  207008 — not a resource file" because CW binary projects live in the resource
  fork, which git strips.
- **The Import must save the regenerated `.mcp` into the macos9 source folder**
  (`…:browser:netsurf:frontends:macos9:`) so the XML's relative
  `<PATH>../../../utils/…</PATH>` refs resolve. Saving anywhere else fails at the
  first `<FILE>` ("Error importing XML … near line 367").
- **CodeWarrior 8 is drag-installable** (officially, per its Installation Notes):
  copy the "Metrowerks CodeWarrior" folder + System Folder Items (CarbonLib 1.5,
  MetroNub, MRO) — no installer, no serial (`license.dat` is bundled). This is how
  the harness installed it, entirely from the host.
- **First launch shows "Where is Mac OS X?"** — click **Continue** (non-default
  button → needs a real click). The choice persists in CW's prefs after a clean
  app quit, so it's one-time per disk image.
- **Source files must be type TEXT for CW to compile them**, and the project's
  Mac CR line-ending convention applies (stage-on-bootvol.sh handles both).

## Process / workflow

- **The interactive OS 9 installer is not "hung", it's slow.** Under TCG it can sit
  on one progress-bar position for many minutes while pegging a core. Use the
  motion-poller technique before declaring it dead. (We abandoned a working install
  this way; the UTM prebuilt image made it moot.)
- **The UTM prebuilt OS 9.2.1 image** (`utmapp/vm-downloads`, release
  `mac-os-9.2.1`) is the no-babysitting path to a working guest — its config
  matches ours (mac99/512 MB/sungem) and its qcow2 boots under raw QEMU directly.
- **Keep the pristine UTM disk untouched** as the recovery root; every derived
  image (CW install, source staging) is reproducible from it via scripts.
