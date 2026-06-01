# Developing MacSurf — start here

MacSurf has two parts that are built and run very differently:

| Part | Language | Built with | Runs on |
|---|---|---|---|
| **Browser** | C (NetSurf fork + macos9 frontend) | **CodeWarrior Pro 8.3** | Mac OS 9 (PowerPC) |
| **Proxy** | Go (stdlib only) | `go build` | any modern machine / VPS |

The browser is the hard part: it is a **Classic Mac OS 9 PowerPC** application. You
cannot build it with a normal modern toolchain. You need one of the build
environments below, plus the right CodeWarrior version.

> Most day-to-day code editing happens on Linux/macOS (it's just C), but **compiling
> requires CodeWarrior on a Mac OS 9 environment** (real or emulated). There is no
> working modern cross-compiler for this target today.

---

## What you need to build the browser

### 1. A Mac OS 9 build environment — pick one

- **A real Power Mac** — G3 or G4, Mac OS 9.1–9.2.2, ≥64 MB RAM (128 MB+ recommended).
  This is the only environment that gives *fully trustworthy* results (see
  "Hardware verification" below). Setup: [codewarrior-setup.md](codewarrior-setup.md).
- **QEMU emulated Mac OS 9** — runs on a modern Mac (incl. Apple Silicon) or Linux.
  Off-hardware, scriptable, and — unlike SheepShaver — has **working networking**
  so the browser can reach the proxy. Full harness + setup:
  [../tools/qemu/README.md](../tools/qemu/README.md) and
  [../tools/qemu/SETUP.md](../tools/qemu/SETUP.md).
- **SheepShaver** — quick boot/render smoke tests only; no usable networking, more
  forgiving than real hardware. Not for fetcher or hardware-specific testing.

### 2. CodeWarrior Pro 8 — **with the 8.3 update**

The project is calibrated to **CodeWarrior Pro 8.3**. The base 8.0 release can
behave differently (MSL versions, compiler fixes, headers), so build errors on
plain 8.0 may be version artifacts rather than real issues.

- Install CodeWarrior Pro 8 (base 8.0), then apply the updaters **in sequence:
  8.1, then 8.2, then 8.3**. They are strictly incremental: the 8.3 updater refuses
  to run on 8.0 ("you must install the Version 8.2 Update first"), and 8.2 needs 8.1.
  Each updater searches the disk and patches the existing install in place, so the
  CodeWarrior folder's exact location doesn't matter. When 8.3 asks to "rebuild your
  libraries," you can Skip it (the updated prebuilt MSL libs are sufficient).
- Required components: **MacOS PowerPC C/C++ Compiler**, **MacOS PowerPC Linker**,
  **MSL C Libraries**, **Universal Headers**. Skip Java/Windows/Palm tools.
- Confirm `…:MacOS Support:Universal:Interfaces:CIncludes:` exists (the Toolbox
  headers) after install.

### 3. The project file (`MacSurf.mcp`) — important

The `MacSurf.mcp` checked into git is **not a directly-openable CodeWarrior
project** — it's a hand-maintained XML *manifest* (git strips the resource fork
that a binary `.mcp` lives in, and its schema predates anyone importing it).
Double-clicking it fails ("not a resource file"). Two ways to get a real project:

- **Convert + Import (recommended for a fresh setup):**
  ```
  python3 tools/codewarrior/manifest-to-mcpxml.py     # writes MacSurf-import.xml
  ```
  then in CodeWarrior **File → Import Project** → select `MacSurf-import.xml` →
  save the generated `.mcp` → build. See [../tools/codewarrior/README.md](../tools/codewarrior/README.md).
- **Maintain a binary `.mcp` on the build Mac** (the maintainer's flow): keep your
  own `.mcp` and add/remove `.c` files in the CW8 IDE as the source tree changes.
  The git manifest is the durable record; don't expect to open it directly.

### 4. The source tree must reach the Mac with the right metadata

- All `.c`/`.h`/`.r` files need **Mac CR line endings** and CodeWarrior file type
  (`TEXT`/`CWIE`). Git and FAT32 transfers strip Mac metadata. The QEMU harness's
  `stage-on-bootvol.sh` handles CR + type/creator; for real-hardware transfer see
  [cross-dev-from-linux.md](cross-dev-from-linux.md).
- The access paths are absolute to the maintainer's layout (`Back40:Desktop
  Folder:patrick:macsurf-source Folder:browser:…`); the converter emits them from
  `Access Paths.xml`. Match that layout or pass `--prefix` to the converter.

---

## Running the browser

The browser does no HTTPS itself — **all TLS is handled by the proxy**. To actually
load pages you must run the Go proxy and point MacSurf's proxy preference at it.

- Build/deploy the proxy: [deploying-proxy.md](deploying-proxy.md).
- Under QEMU, the guest reaches a proxy on the host at **`10.0.2.2:8765`** (SLIRP).
- On real hardware, point the browser at wherever the proxy runs.

---

## Hardware verification (don't skip this)

Emulators are the fast smoke/integration tier, **not** a substitute for a real Mac.
These classes of bug only reproduce on real G3/G4 hardware and MUST be verified
there: the wheel/scroll-bar crashes, USB Overdrive interactions, and the
9.1-vs-9.2.2 font anti-aliasing delta. A green result in QEMU/SheepShaver does not
guarantee real hardware.

---

## The fix/ship workflow

1. Edit C source on Linux/macOS; syntax-check with the C89 cross-check
   (see CLAUDE.md "Linux Cross-Check").
2. Get the source onto the Mac OS 9 environment (CR endings + type/creator).
3. Build in CodeWarrior 8.3 ("Remove Object Code" before rebuilds after file changes).
4. Run + verify — on real hardware for anything hardware-specific.
5. Ship deltas, not full dumps; mention any new `.c` files to add to the project.
   Details: [cross-dev-from-linux.md](cross-dev-from-linux.md).

---

## Key constraints to know before writing code

CodeWarrior 8 is strict C89 (no `//` comments, no `inline`, no C99 designated
initializers, no `for (int i…)`, all locals at block top). The Carbon partition
must be ≥16 MB. There's a large catalogue of CW8/PPC/Carbon gotchas — read
**CLAUDE.md** (root) before non-trivial work; harness-specific gotchas are in
[../tools/qemu/docs/GOTCHAS.md](../tools/qemu/docs/GOTCHAS.md).

---

## Quick reference

| I want to… | Go to |
|---|---|
| Build on real hardware | [codewarrior-setup.md](codewarrior-setup.md) |
| Build off-hardware (QEMU) | [../tools/qemu/README.md](../tools/qemu/README.md) |
| Generate an importable project | [../tools/codewarrior/README.md](../tools/codewarrior/README.md) |
| Cross-develop from Linux | [cross-dev-from-linux.md](cross-dev-from-linux.md) |
| Run the proxy | [deploying-proxy.md](deploying-proxy.md) |
| Understand the constraints/gotchas | CLAUDE.md (root) |
