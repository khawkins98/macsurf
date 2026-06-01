# MacSurf off-hardware build — roadmap & driving document

**This is the driving document for the QEMU/CodeWarrior automation work.** Open
this first; it says where things stand, what's next in what order, and what's
blocked on what. The other docs are reference material (see the doc map at the
bottom). Supersedes `HANDOFF-NEXT-SESSION.md` and `MORNING-STATUS.md` (both
deleted; their content is folded in here — git history has the originals).

**Branch discipline:** all of this lives on `feat/qemu-os9-build-automation`,
pushed to the **`fork`** remote (khawkins98/macsurf). **Never push to `origin`**
(upstream mplsllc/macsurf); never open a PR against upstream.

---

## Where things stand (2026-06-01)

| Capability | Status |
|---|---|
| OS 9.2.1 boots under QEMU (Apple Silicon, TCG) | ✅ working baseline `os9-cw.qcow2` |
| Autonomous GUI control (clicks/menus/dialogs/screenshots) | ✅ verified (`vm.py`, usb-tablet INIT) |
| CodeWarrior 8 installed + launches in guest | ✅ host-side install, no serial |
| MacSurf source staged in guest at `Back40:…` paths | ✅ `stage-on-bootvol.sh` |
| Importable CW8 project XML generated from the manifest | ✅ `manifest-to-mcpxml.py` (rename map + twin detection) |
| Host-side C89 precheck gate | ✅ `tools/precheck.sh` (~15 s, 445 files) |
| **Project imported into CW8 in the guest** | ❌ **next step (M1)** |
| **First green build (Make)** | ❌ gated on M1 |
| Networking: guest → host proxy (10.0.2.2:8765) | ✅ SLIRP works (unlike SheepShaver) |

The single-thread TCG performance ceiling and what to do about it is settled —
see [docs/PERFORMANCE.md](docs/PERFORMANCE.md) (levers + measurements) and
[docs/EMULATION-FRONTIER.md](docs/EMULATION-FRONTIER.md) (research + citations).

---

## The critical path

### M1 — Import the project into CodeWarrior (NOW)

The repo's `MacSurf.mcp` is a hand-maintained XML manifest that CW8 cannot
open or import (schema mismatch — see GOTCHAS.md). The converter generates a
genuine CW8-importable XML from it.

```bash
# 1. regenerate the importable project (also reports stale entries + twins)
python3 tools/codewarrior/manifest-to-mcpxml.py
#    -> browser/netsurf/frontends/macos9/MacSurf-import.xml

# 2. run the host-side precheck (catches C89/manifest errors before guest time)
./tools/precheck.sh

# 3. stage the XML into the guest (hdiutil mount pattern from stage-on-bootvol.sh,
#    SetFile -t TEXT -c CWIE), boot headless, then in CodeWarrior:
./tools/qemu/launch.sh headless
./tools/qemu/vm.py pressdrag 48 7 108 302     # File -> Import Project (640x480 coords)
#    OPEN dialog: type-select the XML, Return
#    SAVE dialog: type a name (e.g. MacSurfP), Return — location doesn't matter,
#    all paths in the generated XML are absolute Back40:… paths
```

Import behaviors verified in earlier sessions: CW8 parses all entries; menus
need press-drag; SAVE dialogs type into the Name field; Esc cancels the dialog.
Full GUI-driving reference: [docs/INPUT.md](docs/INPUT.md).

### M2 — First green build

In CW8: **Make** (⌘M). Expect 30–60+ min under TCG for the cold 530-file build;
poll by screen motion (`vm.py settle` / screenshots), not elapsed time.

- When it stops, screenshot the Errors window and **report the error list to
  Ken / the maintainer — do NOT unilaterally fix browser code.** (CLAUDE.md's
  fix-round discipline applies; the browser source is the maintainer's domain.)
- Likely first errors: access-path gaps (see the two already found in
  GOTCHAS.md "Build-config gaps"), CW component-version differences, and
  possibly the designated-initializer files listed under "Open questions" below.
- Once green: snapshot the post-build disk (`vm.py snapshot post_build`) — that
  qcow2 is the durable object cache (incremental builds from here are 5–50x
  cheaper than cold builds).

### M3 — Build-loop hardening (after first green build)

In payoff order; details in [docs/PERFORMANCE.md](docs/PERFORMANCE.md):

1. **Warm snapshot** of "CW open, project loaded" → restore in seconds instead
   of 2-min boots. Requires moving file injection to the network path so
   snapshots survive (the raw-image round-trip flattens them — GOTCHAS.md).
2. **2-VM pipeline** — one builds round N+1 while the other tests round N.
3. **`howvec` TCG-plugin run** during one build — settles whether FP emulation
   matters for compiles (almost certainly not).

### M4 — Bigger performance levers (when the loop is stable)

1. **Sharded compilation**: split into 4 CW8 Library targets, one cloned VM per
   shard, link once. ~3–3.5x on cold builds. (EMULATION-FRONTIER.md §4)
2. **SheepShaver arm64-JIT spike**: build `kanjitalk755/macemu` with the JIT
   enabled, install CW8, attempt a compile. ~5x if it works; nobody has tried
   (the maintainer only ever used SheepShaver for runtime smoke tests).

### M5 — The toolchain question (Ken's long-term interest)

Evaluate replacing CodeWarrior with **Retro68** (GCC-based PPC cross-compiler,
runs natively on macOS/Linux). This would eliminate the emulated build loop
entirely: native-speed builds, CI, no .mcp/resource-fork problems, relaxed C89
constraints. Most of this project's pain is CW8-specific, not OS 9-specific.
Open questions: PEF/CFM + CarbonLib linking fidelity, MSL→newlib runtime
differences, whether CW8-specific code paths (`#ifdef __MWERKS__`) behave.
This is a research round, not yet scoped. Gate: M1+M2 first, so there's a
known-good CW8 baseline to compare against.

---

## Open questions for the maintainer

Surfaced by the tooling on this branch; need Mac-side answers. None block M1
(the generated project uses the manifest's choices, with warnings).

1. **Designated initializers in 7 manifest files** (found by `tools/precheck.sh`):
   `layout.c` (3 sites), `scrollbar.c` (3), `textarea.c` (3), `plot_style.h:310`,
   libcss `properties.c:188`, `unit.c:430`. Per CLAUDE.md these break CW8 —
   so either the Mac copies of these files differ from the repo (drift), or
   CW8's "C99 mode" setting accepts them and CLAUDE.md's C89 rules are
   over-strict. Whichever way it resolves, something needs updating.

2. **Four manifest entries whose renamed twin differs in content** (found by
   `manifest-to-mcpxml.py`): the manifest builds the left file, but the right
   file also exists with different content — which is the real one?
   - `content.c` vs `ns_content.c` (netsurf/content)
   - `font.c` vs `html_font.c` (handlers/html)
   - `html_element.c` vs `html_html_element.c` (libdom)
   - `parser.c` vs `dom_parser.c` (libdom hubbub binding)

3. **One twin already corrected with documented evidence**: the converter maps
   `libcss/select/dispatch.c` → `s_dispatch.c` (CLAUDE.md documents dispatch.c
   as the stale snapshot; building it causes the prop_dispatch desync crash).
   Maintainer should confirm and ideally fix the manifest itself.

4. **25 stale manifest entries** (renamed files): handled transparently by the
   converter's rename map; the checked-in manifest is the maintainer's to fix.
   Plus 35 byte-identical twins (harmless today, drift risk —
   `manifest-to-mcpxml.py -v` lists them).

---

## Doc map

| Doc | What it's for |
|---|---|
| **ROADMAP.md** (this) | What's next, in what order, blocked on what |
| [README.md](README.md) | How to use the harness (launch, vm.py verbs, scripts reference) |
| [SETUP.md](SETUP.md) | How the harness was built / how to rebuild it |
| [docs/INPUT.md](docs/INPUT.md) | GUI driving: clicks, menus, dialogs, type-select |
| [docs/GOTCHAS.md](docs/GOTCHAS.md) | Every trap already hit — read before debugging anything |
| [docs/PERFORMANCE.md](docs/PERFORMANCE.md) | Speed levers, measurements, what not to chase |
| [docs/EMULATION-FRONTIER.md](docs/EMULATION-FRONTIER.md) | Research: why TCG is the ceiling, what else exists |
| [../precheck.sh](../precheck.sh) | Host-side C89 gate — run before every guest build |
| [../codewarrior/](../codewarrior/) | Manifest→CW8-XML converter, build-robot AppleScript |
| Project memory `qemu-os9-harness` | Cross-session agent context (mirrors this doc) |
