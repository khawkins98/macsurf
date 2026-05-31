#!/usr/bin/env bash
# Build a raw HFS transfer disk image from a folder of files, using machfs MakeHFS.
# - Files with a TEXT type get automatic LF->CR conversion (the project's Mac-CR rule).
# - Source-like files are tagged TEXT/CWIE (open in CodeWarrior) via .idump sidecars.
# The resulting image is a valid HFS volume the OS 9 guest mounts directly (no Drive
# Setup init needed). Attach it with: launch.sh run --transfer <out.img>
#
# Usage: inject-source.sh <src-dir> [out.img] [volname] [size-bytes]
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/config.sh"

SRC="${1:?usage: inject-source.sh <src-dir> [out.img] [volname] [size-bytes]}"
OUT="${2:-$QEMU_XFERDISK}"
VOL="${3:-TRANSFER}"
SIZE="${4:-67108864}"   # 64 MiB default
VENV="$QEMU_WS/venv"
[ -x "$VENV/bin/MakeHFS" ] || { echo "ERROR: machfs venv missing ($VENV). Re-run setup." >&2; exit 1; }
[ -d "$SRC" ] || { echo "ERROR: src dir not found: $SRC" >&2; exit 1; }

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
cp -R "$SRC"/. "$STAGE"/

# Tag CodeWarrior source/text files TEXT/CWIE so they (a) get CR line endings and
# (b) open in the IDE. .idump sidecar = 8 bytes: 4-char type + 4-char creator.
find "$STAGE" -type f \( -name '*.c' -o -name '*.h' -o -name '*.r' -o -name '*.txt' \
     -o -name '*.exp' -o -name '*.pch' -o -name '*.applescript' \) -print0 \
  | while IFS= read -r -d '' f; do printf 'TEXTCWIE' > "$f.idump"; done

"$VENV/bin/MakeHFS" -n "$VOL" -i "$STAGE" -s "$SIZE" "$OUT"
echo "built $OUT  (vol=$VOL, ${SIZE} bytes) from $SRC"
