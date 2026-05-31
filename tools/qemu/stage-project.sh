#!/usr/bin/env bash
# Build the "Back40" source disk that satisfies MacSurf.mcp's ABSOLUTE access paths
# (Back40:Desktop Folder:patrick:macsurf-source Folder:browser:...). Produces a raw
# HFS image (machfs) you attach as a second drive; the guest mounts it as "Back40"
# and CodeWarrior's access paths resolve against it unchanged.
#
# Source files (.c/.h/.r/...) are tagged TEXT/CWIE with LF->CR conversion (MakeHFS).
# MacSurf.mcp is tagged MMPr/CWIE (CodeWarrior project) so it opens as a project.
#
# Usage: stage-project.sh [out.img]   (default ~/macsurf-qemu/images/back40.img)
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/config.sh"
REPO="$(cd "$HERE/../.." && pwd)"
OUT="${1:-$QEMU_IMAGES/back40.img}"
VENV="$QEMU_WS/venv"
[ -x "$VENV/bin/MakeHFS" ] || { echo "machfs venv missing" >&2; exit 1; }

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
# Exact layout the .mcp expects (volume root = Back40; these folders hang off it).
DEST="$STAGE/Desktop Folder/patrick/macsurf-source Folder"
mkdir -p "$DEST"
echo "copying source tree (browser/) ..."
ditto "$REPO/browser" "$DEST/browser"
# Also place Access Paths.xml next to the source root (reference / for re-import).
cp "$REPO/Access Paths.xml" "$DEST/Access Paths.xml" 2>/dev/null || true

echo "tagging source files TEXT/CWIE (LF->CR happens in MakeHFS) ..."
find "$DEST" -type f \( -name '*.c' -o -name '*.h' -o -name '*.r' -o -name '*.exp' \
     -o -name '*.pch' -o -name '*.x' -o -name '*.txt' \) -print0 \
  | while IFS= read -r -d '' f; do printf 'TEXTCWIE' > "$f.idump"; done

echo "tagging MacSurf.mcp as MMPr/CWIE ..."
MCP="$DEST/browser/netsurf/frontends/macos9/MacSurf.mcp"
[ -f "$MCP" ] && printf 'MMPrCWIE' > "$MCP.idump" || echo "WARN: MacSurf.mcp not found at expected path"

echo "building Back40 HFS image (this can take a minute for ~thousands of files) ..."
"$VENV/bin/MakeHFS" -n "Back40" -i "$STAGE" -s 314572800 "$OUT"
echo "built $OUT (vol=Back40)"
ls -lah "$OUT"
