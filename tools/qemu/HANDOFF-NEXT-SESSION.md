# Continue: MacSurf QEMU OS 9 harness — finish the first build

## Context

A previous session built a QEMU PowerPC + Mac OS 9 harness that can autonomously
drive the OS 9 GUI (keyboard + absolute mouse clicks + screenshots via QMP), with
CodeWarrior 8 installed and the MacSurf source staged in the guest. Everything is
committed on branch **`feat/qemu-os9-build-automation`**, pushed to the **`fork`**
remote (khawkins98/macsurf). **Never push to `origin`** (upstream mplsllc/macsurf);
never open a PR against upstream.

## Read these before touching anything

1. `tools/qemu/README.md` — how to use everything
2. `tools/qemu/docs/GOTCHAS.md` — every trap already hit (don't re-hit them)
3. `tools/qemu/docs/INPUT.md` — GUI-driving techniques (press-drag menus, dialog behaviors)
4. `tools/qemu/MORNING-STATUS.md` — status / what's verified
5. Project memory `qemu-os9-harness` covers the same ground

## Current state

- **Working disk:** `~/macsurf-qemu/images/os9-cw.qcow2` = OS 9.2.1 + CodeWarrior 8
  + USB Tablet INIT + full MacSurf source staged at
  `Back40:Desktop Folder:patrick:macsurf-source Folder:browser:…` (boot volume is
  named "Back40"). Pristine backup: `~/macsurf-qemu/media/Mac OS 9.2.1.utm/Images/disk-0.qcow2`.
- **Launch:** `./tools/qemu/launch.sh headless` (config defaults are correct:
  `-M mac99` + `usb-tablet`). Boot ≈2 min.
- **Driver:** `./tools/qemu/vm.py` — screenshot / key / type / click / pressdrag /
  settle / snapshot / revert / quit.
- **VERIFIED:** CodeWarrior imports the project XML (File → Import Project parses
  all 531 files); it fails only when the regenerated `.mcp` is saved in the wrong
  place.

## The goal

Drive the import to completion → project loads → **Make** → capture the first
build's error list and report it for triage with the user.

## CRITICAL findings from the end of last session (NOT yet in the docs)

The `MacSurf.mcp` XML (`browser/netsurf/frontends/macos9/MacSurf.mcp`) structure:

- 528 `<FILE>` entries: `<PATHTYPE>Project</PATHTYPE>` + relative `<PATH>`
  (`../../../utils/utils.c` style). Plus 2 Compiler, 1 System path types.
- 622 `<FILEREF>` one-liners in LINKORDER/GROUPLIST:
  `<FILEREF><PATH>shims/mac_iconv.c</PATH></FILEREF>` (also relative).
- 17 access-path settings: `<NAME>UserSearchPath</NAME>` with VALUES like
  `{Project}` and `{Project}/include` — **the include search paths are ALSO
  project-relative.**
- The XML contains hand-written comments → it is a **hand-maintained project
  definition**, not a raw CW export. No `<PATHFORMAT>` elements anywhere.

**Implication:** the imported `.mcp` MUST be saved in the macos9 source folder
(`Back40:Desktop Folder:patrick:macsurf-source Folder:browser:netsurf:frontends:macos9:`)
for BOTH the file list AND the `{Project}`-relative include paths to resolve.
Two approaches:

- **(a) Navigate the import's save dialog to macos9** — fully mapped, do this first.
- **(b) Host-side rewrite of ALL relative refs** (528 FILE + 622 FILEREF + 17
  UserSearchPath) to absolute `Back40:` paths so the `.mcp` can be saved anywhere —
  more XML surgery; only fall back to this if (a) keeps failing. Note: rewriting
  only the FILE paths is NOT enough; the `{Project}` access paths break too.

## Exact steps for approach (a)

All coordinates below assume a **640×480** framebuffer — ALWAYS detect the actual
resolution first (`sips -g pixelWidth <screenshot>`) and scale if it's 1024×768.

1. `./tools/qemu/launch.sh headless` → wait ~2 min → `vm.py key ret` (dismiss the
   Disk First Aid dialog) → screenshot to confirm desktop + resolution.
2. **Launch CW** (Finder type-select navigation): the Back40 disk window is usually
   already open showing "Metrowerks CodeWarrior 8.0". Type `Metrowerks` + Cmd-O →
   type `Metrowerks CodeWarrior` + Cmd-O → type `CodeWarrior IDE` + Cmd-O → wait
   ~25 s. (If a "Where is Mac OS X?" dialog appears, click Continue at ~362,180.)
3. **File → Import Project:** `vm.py pressdrag 48 7 108 302` (press File, drag to
   Import Project, release). Menus ONLY work via press-drag.
4. **The OPEN dialog** remembers macos9 and type-selects: type `MacSurf.mcp` →
   `key ret`. (If it isn't in macos9, navigate by typing folder names + Return.)
5. **The SAVE dialog** ("Create New Project…") opens at Desktop. Navigate by
   **double-clicking** folders (`vm.py click X Y --double`):
   patrick → macsurf-source Folder → browser → netsurf → frontends → macos9.
   Folder rows start at y≈183, ~20 px apart; scroll with single clicks on the
   scrollbar down-arrow (~510, 296) when the target is below the visible 6 rows.
   Screenshot between every step — coordinates drift as lists change.
6. **Set the name:** click the Name field at **(~285, 349)** — NOT lower (y≈460
   hits the Control Strip) — then Cmd-A, type `MacSurfP`.
   **NEVER press Esc inside this dialog — Esc cancels the entire import.**
7. **Save:** click the Save button (~478, 384), or press Return if the Name field
   has focus and no folder is selected in the list (a selected folder turns the
   button into "Open").
8. The **project window** should open with 531 files. Screenshot to verify. If an
   "Error importing XML … near line 367" appears, the save location was wrong
   (that line = the first `<FILE>` entry failing to resolve).
9. **Make:** `vm.py key meta_l-m` (Cmd-M). The build runs **30–60+ min** under TCG.
   Poll with a background screenshot every few minutes (screen-motion = still
   compiling). Snapshot first if you want a revert point: `vm.py snapshot pre_build`.
10. When it stops, screenshot the **Errors & Warnings** window and report the error
    list to the user for triage. Do not try to fix build errors unilaterally —
    the user is the project maintainer and CLAUDE.md has strict rules about the
    browser code.

## Quick reference

```bash
./tools/qemu/launch.sh headless                    # boot (background it)
./tools/qemu/vm.py screenshot /tmp/now.png         # look
./tools/qemu/vm.py key ret                         # Return / default button
./tools/qemu/vm.py type "text"                     # type / Finder type-select
./tools/qemu/vm.py click 362 180                   # absolute click
./tools/qemu/vm.py click 290 150 --double          # double-click
./tools/qemu/vm.py pressdrag 48 7 108 302          # menu: press title, drag to item
./tools/qemu/vm.py settle                          # block until screen stable
./tools/qemu/vm.py quit                            # kill VM (next boot shows Disk First Aid)
```

Top gotchas: coordinates depend on detected resolution; menus = press-drag only;
SAVE dialogs type into the Name field (not the list) and Esc cancels them; never
host-mount the disk while the VM runs; boots are slow — judge by screen motion,
not by time.
