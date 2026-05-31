#!/usr/bin/env bash
# Read files back OUT of an HFS transfer/shared image (build logs, the linked
# binary, sentinels) onto the host. Uses machfs DumpHFS (preserves forks via
# .idump/.rdump sidecars). The VM must be SHUT DOWN (or the image detached)
# before reading, or you'll see a stale view.
#
# Usage: read-results.sh <img> [dest-dir]
#        read-results.sh <img> --list          # just list contents
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/config.sh"

IMG="${1:?usage: read-results.sh <img> [dest-dir|--list]}"
DEST="${2:-$QEMU_WS/results}"
VENV="$QEMU_WS/venv"
[ -f "$IMG" ] || { echo "ERROR: image not found: $IMG" >&2; exit 1; }

if [ "$DEST" = "--list" ]; then
  # hfsutils gives a quick listing without extracting.
  if command -v hmount >/dev/null 2>&1; then
    hmount "$IMG" >/dev/null && hls -l && humount >/dev/null
  else
    "$VENV/bin/DumpHFS" "$IMG" /tmp/_dumphfs_peek && find /tmp/_dumphfs_peek -maxdepth 2 && rm -rf /tmp/_dumphfs_peek
  fi
  exit 0
fi

mkdir -p "$DEST"
"$VENV/bin/DumpHFS" "$IMG" "$DEST"
echo "extracted $IMG -> $DEST"
find "$DEST" -maxdepth 2 -type f | head -40
