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

- **The repo `MacSurf.mcp` is a hand-maintained XML manifest, NOT a CodeWarrior
  export.** It mimics CW's export style but doesn't conform to the real schema
  (no `<?codewarrior exportversion?>` PI, `FILEFLAGS` values `compile`/`link`
  aren't CW vocabulary, no `PATHFORMAT`, made-up flat access-path form). **File →
  Open fails** ("Resource File Error 207008" — CW binary projects live in the
  resource fork, which git strips) and **File → Import Project also fails**
  ("Error importing XML … near line 367" = the first `<FILE>` entry) because of
  the schema mismatch — NOT because of file paths or save location. Classic CW
  tolerates missing files (shows them red); a hard import error means bad format.
  Fix: capture a genuine CW8 export (make a tiny project in the guest, File →
  Export Project) and convert the manifest to that schema — done: see
  tools/codewarrior/manifest-to-mcpxml.py and ../ROADMAP.md.
- **CodeWarrior 8 is drag-installable** (officially, per its Installation Notes):
  copy the "Metrowerks CodeWarrior" folder + System Folder Items (CarbonLib 1.5,
  MetroNub, MRO) — no installer, no serial (`license.dat` is bundled). This is how
  the harness installed it, entirely from the host.
- **First launch shows "Where is Mac OS X?"** — click **Continue** (non-default
  button → needs a real click). The choice persists in CW's prefs after a clean
  app quit, so it's one-time per disk image.
- **Source files must be type TEXT for CW to compile them**, and the project's
  Mac CR line-ending convention applies (stage-on-bootvol.sh handles both).
- **The 8.x updaters are strictly sequential: 8.0 -> 8.1 -> 8.2 -> 8.3.** The 8.3
  updater refuses to run on 8.0 ("must install the Version 8.2 Update first"); 8.2
  needs 8.1. Each is a VISE installer that searches the disk and patches in place
  (so a nested CW folder is fine). When 8.3 offers to "rebuild your libraries," Skip
  it (updated prebuilt MSL libs suffice). The project targets 8.3, so build on 8.3.
- **CW 8.3 prompts "Convert Project" for projects made on an older CW** (e.g. an
  import done before the update). It's a one-time per-project conversion; or
  re-import fresh on 8.3. Useful as a version check: if it says your project is
  "older", the IDE is genuinely the newer (8.3) version.
- **`AlwaysSearchUserPaths=true` is mandatory** in the Access Paths panel, or
  angle-bracket includes (`<stat.h>`, `<parserutils/errors.h>`) never search the
  user paths and you get thousands of "file cannot be opened" cascade errors. The
  converter now emits it (from Access Paths.xml); if hand-building, check the box.

## Build-config gaps found via the harness (fresh-clone blockers)

These block a clean build from a fresh clone (the DEVELOPING.md path). Found while
driving the CW 8.3 build in QEMU:

- **`ostls_cw8_prefix.h` not in the repo** — `macsurf_prefix.h` includes it
  unconditionally, but it lives in the un-checked-in `macTLS/` tree. Vendored a copy
  into `frontends/macos9/`. Upstream issue mplsllc/macsurf#162.
- **`<hubbub/hubbub_errors.h>` cannot be opened (access-path granularity)** —
  `libdom/bindings/hubbub/parser.c` includes `<hubbub/hubbub_errors.h>`; the file
  exists only at `browser/libdom/bindings/hubbub/hubbub_errors.h` (the public
  `libhubbub/include/hubbub/` has `errors.h`, NOT `hubbub_errors.h`). For the
  `<hubbub/...>` prefix to resolve to a sibling, the access paths need the PARENT
  dir `browser:libdom:bindings:`, but the export only has
  `browser:libdom:bindings:hubbub:` (one level too deep), so it resolves to a
  doubled `bindings/hubbub/hubbub/...` that doesn't exist. Fix: add the
  `browser:libdom:bindings:` access path. Affects `parser.c` (and likely
  `dom_parser.c` in the same dir). Blocks the final link (missing object), not the
  compile of other files.

## Snapshots and disk backups

- **`guest-cp.sh` / `stage-on-bootvol.sh` FLATTEN qcow2 internal snapshots.** They
  round-trip the disk through raw (`qemu-img convert`), which drops all `savevm`
  snapshots. So a `vm.py snapshot` does NOT survive a subsequent file injection.
  For state that's expensive to recreate (e.g. the CodeWarrior 8.3 install), make a
  **durable host-side copy of the qcow2** while the VM is OFF (`cp os9-cw.qcow2
  os9-cw83-clean.qcow2`) rather than relying on a snapshot. Snapshots are fine for
  short-lived "revert within this session" points when no injection happens between.

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

## Performance / CPU utilization (single-vCPU TCG ceiling)

- **A uniprocessor PPC guest is bounded to ONE host core.** TCG runs one
  translation thread per guest vCPU; OS 9 is uniprocessor (`-smp 1` is forced,
  PPC isn't MTTCG-enabled anyway), so QEMU pegs exactly one core. On a 10-core
  machine that reads as ~10% of total in Activity Monitor. That is the ceiling,
  not idle capacity. `-smp >1`, bigger `tb-size`, iothreads, HVF/KVM are all
  dead ends here. (Verified: `info jit` showed TB flush count 0 with only 294 MB
  of the 512 MB cache used, so the translation cache is not thrashing -- leave
  `tb-size` alone.)
- **THE big lever: keep that one core on a PERFORMANCE core, not an efficiency
  core.** macOS demotes a *niced* thread to E-cores under P-core contention, and
  E-cores run this TCG load ~1.5-2.5x slower. The trap: **commands launched by an
  AI-agent / background harness inherit nice +5**, so an agent-launched QEMU runs
  niced and gets shoved onto E-cores. A QEMU launched from a normal foreground
  Terminal is nice 0 and gets the P-cores.
  - **Diagnose P vs E (needs sudo):** `sudo powermetrics --samplers cpu_power -i 1000 -n 3`
    -- the ~100% core shows up under the P-Cluster (good) or E-Cluster (bad).
    Non-sudo: Activity Monitor -> Window -> CPU History, watch which graph pegs.
  - **Check nice (no sudo):** `ps -o pid,nice,stat -p $(pgrep -f qemu-system-ppc)`
    -- `NI 5` / `STAT RN` means niced.
  - **Live fix on a running niced VM (sudo; non-disruptive, no restart):**
    `sudo renice -n 0 -p <pid>` then `sudo taskpolicy -B -p <pid>`. Only root can
    LOWER niceness, so a non-root `nice -n 0` cannot undo an inherited +5.
  - **Permanent fix: launch the build VM from a plain Terminal, not via the
    agent.** That alone gets nice 0 + P-cores. For full-speed builds, drive
    interactively or have the user launch `launch.sh run` themselves.
  - Ruled out as factors on this host: AC vs battery (was on AC), Low Power Mode
    (off), thermal throttling (a single pegged core never throttles an M5; minor
    caveat for the fanless Air on 10+ min runs -- check `pmset -g therm`).
- **Stackable secondary levers (measure with identical cold-build wall-clock):**
  - `-cpu g3`/`750` instead of `g4` may cut AltiVec-emulation overhead (a C
    compiler emits no vector ops). 5-20%, SPECULATIVE. RISK: config notes flag g4
    for 9.2.x -- test on a CLONED disk, confirm it still boots + CarbonLib loads.
  - QEMU source build with `-O3 -flto` (+optional PGO): reliable 3-8% on the TCG
    thread. Keep the Homebrew binary as fallback.
  - `-display none` (headless) for unattended builds: small free gain; QMP
    `screendump` still drives the GUI.
  - `cache=unsafe,aio=threads` on the `-drive` (DISPOSABLE disk only -- a crash
    can corrupt it): 0-25% IF the build is hitting HFS flush stalls. Keep the
    pristine/clean qcow2 untouched; run unsafe on a working copy.
- **Biggest iteration win (not raw speed): incremental builds.** Use plain Make
  (Cmd-M), NOT "Remove Object Code -> full rebuild", when you've only edited
  source. CW8's per-file dependency tracking recompiles just the changed TUs --
  5-50x less work per round on a 530-file project. Reserve the full clean rebuild
  for project-structure / prefix / access-path changes. Keep a durable
  post-clean-build qcow2 so you never pay the cold 530-file cost twice.
