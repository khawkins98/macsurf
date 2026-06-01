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

## The plan: make CW tell us its own schema, then convert

**Step 1 — get a genuine CW8 project export (ground truth).** In the guest:
1. Boot, launch CodeWarrior (Finder type-select: `Metrowerks` ⌘O → `Metrowerks
   CodeWarrior` ⌘O → `CodeWarrior IDE` ⌘O).
2. **File → New Project** (press-drag the File menu) → pick any Mac OS C stationery
   → save it somewhere simple (Desktop is fine).
3. Add one or two source files to it (Project menu → Add Files), set one access
   path (Edit → Target Settings → Access Paths) — enough to see every structure.
4. **File → Export Project** → save the XML.
5. Quit CW + VM; mount the disk host-side (`stage-on-bootvol.sh` shows the
   hdiutil pattern); copy the exported XML back to the host.

**Step 2 — write the converter** (`tools/qemu/manifest-to-mcpxml.py`): transform the
hand-maintained manifest (531 FILE entries + 622 FILEREFs + 17 access paths +
target settings, all meaningful content) into the genuine schema captured in
Step 1. Prefer **PATHTYPE Name** references (the classic portable form — files
resolve via access paths, missing files don't block opening) and express the
access paths in the real `SearchPath`/`PathRoot` structure ({Project}-rooted).
A background research agent's findings on the documented schema may already be
in the conversation/docs — check before starting.

**Step 3 — stage + import the converted XML** (host-mount the disk, copy it next
to the source, repackage). Then in CW: File → Import Project (press-drag) →
select it → save the generated `.mcp` (location should now barely matter with
Name refs, but macos9 next to the source is still the tidy choice).

**Step 4 — Make** (⌘M / `vm.py key meta_l-m`). The first build runs 30–60+ min
under TCG. Poll with periodic screenshots (screen motion = still compiling).
When it stops, screenshot the Errors & Warnings window and **report the error
list to the user for triage — do not fix browser code unilaterally** (CLAUDE.md
has strict rules; the user is the maintainer).

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
