# tools/codewarrior — CodeWarrior project tooling (works on any Mac target)

Tools for building MacSurf with CodeWarrior 8 — applicable to a real Power Mac,
the maintainer's dev machine, or an emulated Mac (see `tools/qemu/`). Nothing in
this directory depends on QEMU.

## manifest-to-mcpxml.py

The repo's `browser/netsurf/frontends/macos9/MacSurf.mcp` is a hand-maintained
**XML manifest** of the project (file list, target settings, access paths). It is
NOT directly openable/importable by CodeWarrior — its vocabulary predates anyone
actually feeding it to CW8's importer (see the root CLAUDE.md Known Gotchas).

This tool converts the manifest into a **genuine CW8-importable project XML**:

```bash
python3 tools/codewarrior/manifest-to-mcpxml.py            # writes MacSurf-import.xml
python3 tools/codewarrior/manifest-to-mcpxml.py --prefix "MyVolume:my:source:path:browser:netsurf:frontends:macos9"
```

Then on the Mac: **File → Import Project** → select `MacSurf-import.xml` → save the
generated `.mcp` anywhere → build. The generated project uses absolute MacOS paths
(default: the `Back40:…` layout from `Access Paths.xml`); pass `--prefix` to match
wherever your source tree actually lives.

It also **validates every file reference against the repo** and carries a rename
map for references that drifted after the repo's duplicate-basename rename sweeps.

`reference/python-cw7-reference.xml` is a genuine real-world CW export used as the
schema/DTD source of truth.

## build-robot.applescript

A stay-open AppleScript applet that drives CodeWarrior builds via AppleEvents
(open project → Remove Object Code → Make → save error log + sentinel). Compile it
in Script Editor on the Mac and drop it in Startup Items for unattended builds.
