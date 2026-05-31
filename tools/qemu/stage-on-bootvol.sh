#!/usr/bin/env bash
# Stage the MacSurf source ONTO the boot volume (os9-cw.qcow2), renaming it to
# "Back40" and placing the tree at the exact path the project expects:
#   Back40:Desktop Folder:patrick:macsurf-source Folder:browser:...
# This makes BOTH the .mcp's relative file paths AND Access Paths.xml's absolute
# "Back40:..." include paths resolve unchanged. Source files get type TEXT/creator
# CWIE + LF->CR (byte-safe). The project XML (MacSurf.mcp) + Access Paths.xml are
# tagged TEXT so CodeWarrior's Import dialogs can see them.
#
# Avoids machfs (which force-decodes TEXT files as UTF-8 and chokes on non-UTF-8
# source). Uses the host HFS+ mount + ditto + SetFile + perl instead.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/config.sh"
REPO="$(cd "$HERE/../.." && pwd)"

pkill -9 -x qemu-system-ppc 2>/dev/null || true; sleep 1
echo "=== detach any stale OS9 mounts first (hygiene) ==="
for v in /Volumes/untitled "/Volumes/untitled 1" /Volumes/Back40 "/Volumes/Back40 1"; do
  [ -d "$v" ] && hdiutil detach "$v" -force 2>/dev/null || true
done
sleep 1
echo "=== mount boot volume rw ==="
rm -f "$QEMU_IMAGES/os9-cw.raw"
qemu-img convert -f qcow2 -O raw "$QEMU_IMAGES/os9-cw.qcow2" "$QEMU_IMAGES/os9-cw.raw"
# attach and PARSE the actual mountpoint (the volume is "untitled" and macOS
# appends " 1" etc. on name collisions, so never hardcode the path).
# NB: `yes |` gets SIGPIPE when hdiutil closes stdin; under `set -o pipefail`
# that aborts the script, so disable pipefail just for this pipeline.
set +o pipefail
ATTACH_OUT="$(printf 'y\ny\ny\n' | hdiutil attach -noverify -imagekey diskimage-class=CRawDiskImage "$QEMU_IMAGES/os9-cw.raw" 2>&1)"
set -o pipefail
VOL="$(echo "$ATTACH_OUT" | grep -i "Apple_HFS" | sed -E 's/.*Apple_HFS[[:space:]]+//' | head -1)"
[ -n "$VOL" ] && [ -d "$VOL/System Folder" ] || { echo "ERROR: HFS volume not mounted"; echo "$ATTACH_OUT"; exit 1; }
echo "mounted at: $VOL"

echo "=== rename volume -> Back40 ==="
DEV="$(echo "$ATTACH_OUT" | grep -i "Apple_HFS" | awk '{print $1}' | head -1)"
diskutil rename "$DEV" Back40 >/dev/null 2>&1 || diskutil rename "$VOL" Back40 >/dev/null 2>&1 || true
sleep 1
VOL="/Volumes/Back40"
[ -d "$VOL/System Folder" ] || { echo "ERROR: rename to Back40 failed"; exit 1; }

DEST="$VOL/Desktop Folder/patrick/macsurf-source Folder"
echo "=== ditto source tree -> $DEST/browser ==="
mkdir -p "$DEST"
rm -rf "$DEST/browser"
ditto "$REPO/browser" "$DEST/browser"
cp "$REPO/Access Paths.xml" "$DEST/Access Paths.xml"

echo "=== LF->CR + type TEXT/CWIE on source files (byte-safe) ==="
# convert line endings (CRLF or LF -> CR) and set type/creator
find "$DEST/browser" -type f \( -name '*.c' -o -name '*.h' -o -name '*.r' -o -name '*.exp' \
     -o -name '*.pch' -o -name '*.x' \) -print0 \
  | while IFS= read -r -d '' f; do
      perl -i -pe 's/\r\n/\r/g; s/\n/\r/g' "$f"
      /usr/bin/SetFile -t TEXT -c CWIE "$f"
    done

echo "=== tag the XML files TEXT so CW Import can see them ==="
/usr/bin/SetFile -t TEXT -c CWIE "$DEST/browser/netsurf/frontends/macos9/MacSurf.mcp" 2>/dev/null || true
/usr/bin/SetFile -t TEXT -c CWIE "$DEST/Access Paths.xml" 2>/dev/null || true

echo "=== detach + repackage ==="
hdiutil detach "$VOL" >/dev/null 2>&1
qemu-img convert -f raw -O qcow2 "$QEMU_IMAGES/os9-cw.raw" "$QEMU_IMAGES/os9-cw.qcow2"
rm -f "$QEMU_IMAGES/os9-cw.raw"
echo "DONE. Boot volume is now 'Back40' with source at Desktop Folder:patrick:macsurf-source Folder:browser:"
qemu-img info "$QEMU_IMAGES/os9-cw.qcow2" | grep -i "disk size"
