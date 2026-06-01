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
4. Project memory `qemu-os9-harness` covers the same ground

## Current state

- **Working disk:** `~/macsurf-qemu/images/os9-cw.qcow2` = OS 9.2.1 + CodeWarrior 8
  + USB Tablet INIT + full MacSurf source staged at
  `Back40:Desktop Folder:patrick:macsurf-source Folder:browser:…` (boot volume named
  "Back40"). Pristine backup: `~/macsurf-qemu/media/Mac OS 9.2.1.utm/Images/disk-0.qcow2`.
- **Launch:** `./tools/qemu/launch.sh headless` (defaults correct: `-M mac99` + `usb-tablet`). Boot ≈2 min.
- **Driver:** `./tools/qemu/vm.py` — screenshot / key / type / click / pressdrag / settle / snapshot / revert / quit.
- All GUI capabilities verified (clicks, menus via press-drag, dialogs, Finder navigation).

## ⚠️ CORRECTED DIAGNOSIS (supersedes older docs/commits)

The repo's `browser/netsurf/frontends/macos9/MacSurf.mcp` is **NOT a CodeWarrior
export at all** — it's a **hand-maintained XML manifest** written in CW-export-
*inspired* style by previous Linux-side sessions (it has prose comments; git
history shows it edited at every phase). CW8's File → Import Project rejects it
("Error importing XML file near line 367 — unknown error" at the first `<FILE>`
entry) because of **schema/vocabulary mismatch**, NOT file-path resolution:

| Genuine CW export (`Access Paths.xml`) | Hand-written manifest (`MacSurf.mcp`) |
|---|---|
| `<?codewarrior exportversion="1.0" ideversion="5.0" ?>` PI present | missing |
| Access paths: nested `SearchPath` → `Path`/`PathFormat`/`PathRoot` settings | flat `UserSearchPath` + `{Project}` inline (made-up form) |
| `FILEFLAGS` values from CW vocabulary (`Debug`, empty) | `compile` / `link` (not CW vocabulary) |
| `PATHFORMAT` declared per path | absent (Unix-style `../` paths undeclared) |

An earlier theory blamed the **save location** (Desktop vs macos9 folder) — that
was wrong: classic CW tolerates missing/unresolvable files (shows them in red),
so a hard import error is a format problem. Ignore the save-location playbook in
older commits of this file.

## The plan (converter is BUILT — this is now a short job)

**`tools/codewarrior/manifest-to-mcpxml.py` already exists and works.** It converts the
manifest into a genuine CW8-importable project XML using the schema captured from
real CW exports (`tools/codewarrior/reference/python-cw7-reference.xml` + a research
sweep). Key properties of its output (`MacSurf-import.xml`, gitignored/regenerable):
- correct prologue (`<?codewarrior exportversion="1.0.1" ideversion="5.0" ?>`) + real DOCTYPE
- 530 files as **PATHTYPE Absolute** MacOS colon paths under the staged
  `Back40:…` layout (3 CW libraries as PATHTYPE Name); valid empty FILEFLAGS
- ALL access paths absolute too → **the imported .mcp can be saved ANYWHERE**
  (no deep save-dialog navigation — save on the Desktop, type a name, Return)
- a rename map for 25 stale manifest references (repo files renamed after the
  manifest was written); 1 deleted diagnostic dropped
- validates every reference against the repo tree (build fails loudly if stale)

**Steps:**
1. `python3 tools/codewarrior/manifest-to-mcpxml.py` → regenerates
   `browser/netsurf/frontends/macos9/MacSurf-import.xml`
2. Stage it into the guest: mount the disk (see `stage-on-bootvol.sh` for the
   hdiutil pattern), `ditto` the XML somewhere easy (e.g. the volume root or
   Desktop Folder), `SetFile -t TEXT -c CWIE` it, detach, repackage to qcow2.
3. Boot headless, launch CodeWarrior (Finder type-select), then
   **File → Import Project** via `vm.py pressdrag 48 7 108 302` (640×480 coords).
4. In the OPEN dialog: navigate to the XML (type-select works), Return.
5. In the SAVE dialog: just type a name (e.g. `MacSurfP`) — typing goes straight
   to the Name field — and press Return. Location doesn't matter (absolute paths).
6. Project window should open with 530 files. **Make** (⌘M). 30–60+ min under TCG;
   poll by screen motion. Screenshot the Errors window when it stops and report
   the error list to the user — do NOT fix browser code unilaterally.

**Known issue to mention when reporting build results:** the repo contains
stale-twin files (e.g. `font_family.c` AND `p_font_family.c` — same class as
CLAUDE.md's dispatch.c/s_dispatch.c gotcha). The manifest references one of each
pair; if it references the stale twin, that's a maintainer call to fix in the
manifest, not in the converter.

## Quick reference

```bash
./tools/qemu/launch.sh headless                    # boot (background it; ~2 min)
./tools/qemu/vm.py key ret                         # dismiss Disk First Aid after boot
./tools/qemu/vm.py screenshot /tmp/now.png         # look (always detect resolution!)
./tools/qemu/vm.py type "text"                     # type / Finder type-select
./tools/qemu/vm.py click 362 180                   # absolute click
./tools/qemu/vm.py click 290 150 --double          # double-click
./tools/qemu/vm.py pressdrag 48 7 108 302          # menu: press title, drag to item, release
./tools/qemu/vm.py settle                          # block until screen stable
./tools/qemu/vm.py quit                            # kill VM
```

Top gotchas (full list in docs/GOTCHAS.md): coordinates depend on detected
resolution (640×480 vs 1024×768 varies per boot); menus = press-drag only, never
click-then-click; SAVE dialogs type into the Name field (not the list),
double-click to enter folders, and **Esc cancels the whole dialog**; OPEN dialogs
type-select and remember the last folder; never host-mount the disk while the VM
runs; judge boot/build progress by screen motion, not elapsed time.
