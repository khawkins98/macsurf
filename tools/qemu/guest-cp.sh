#!/usr/bin/env bash
# Copy a host file INTO the guest's boot disk at a given Mac path. The VM must be
# OFF. Handles the qcow2->raw->mount->copy->repackage dance, sets Mac type/creator,
# and optionally converts LF->CR (for TEXT files CodeWarrior will read).
#
# Usage:
#   guest-cp.sh <host-file> <guest-colon-path> [TYPE CREATOR] [--cr]
#
# Examples:
#   guest-cp.sh MacSurf-import.xml "Desktop Folder:patrick:macsurf-source Folder:browser:netsurf:frontends:macos9:MacSurf-import.xml" TEXT CWIE
#   guest-cp.sh notes.txt "Desktop Folder:notes.txt" TEXT ttxt --cr
#
# The guest path is relative to the boot volume root (no leading volume name).
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/config.sh"

SRC="${1:?usage: guest-cp.sh <host-file> <guest-colon-path> [TYPE CREATOR] [--cr]}"
DEST_MAC="${2:?need guest colon path}"
TYPE="${3:-}"
CREATOR="${4:-}"
CONVERT_CR=0
for a in "$@"; do [ "$a" = "--cr" ] && CONVERT_CR=1; done
[ -f "$SRC" ] || { echo "ERROR: no such host file: $SRC" >&2; exit 1; }

pkill -9 -x qemu-system-ppc 2>/dev/null && sleep 1 || true
echo "=== detach stale mounts ==="
for v in /Volumes/untitled "/Volumes/untitled 1" /Volumes/Back40 "/Volumes/Back40 1"; do
  [ -d "$v" ] && hdiutil detach "$v" -force 2>/dev/null || true
done
sleep 1

echo "=== mount boot disk rw ==="
rm -f "$QEMU_IMAGES/_guestcp.raw"
qemu-img convert -f qcow2 -O raw "$QEMU_SYSDISK" "$QEMU_IMAGES/_guestcp.raw"
set +o pipefail
ATTACH_OUT="$(printf 'y\ny\ny\n' | hdiutil attach -noverify -imagekey diskimage-class=CRawDiskImage "$QEMU_IMAGES/_guestcp.raw" 2>&1)"
set -o pipefail
VOL="$(echo "$ATTACH_OUT" | grep -i "Apple_HFS" | sed -E 's/.*Apple_HFS[[:space:]]+//' | head -1)"
[ -n "$VOL" ] && [ -d "$VOL" ] || { echo "ERROR: mount failed"; echo "$ATTACH_OUT"; exit 1; }
echo "mounted: $VOL"

# guest colon path -> posix path under the mountpoint
DEST_POSIX="$VOL/$(echo "$DEST_MAC" | tr ':' '/')"
mkdir -p "$(dirname "$DEST_POSIX")"

echo "=== copy ==="
if [ "$CONVERT_CR" = "1" ]; then
  perl -pe 's/\r\n/\r/g; s/\n/\r/g' < "$SRC" > "$DEST_POSIX"
else
  ditto "$SRC" "$DEST_POSIX"
fi
if [ -n "$TYPE" ] && [ -n "$CREATOR" ] && [ "$TYPE" != "--cr" ]; then
  /usr/bin/SetFile -t "$TYPE" -c "$CREATOR" "$DEST_POSIX"
  echo "type/creator: $TYPE/$CREATOR"
fi
ls -la "$DEST_POSIX"

echo "=== detach + repackage ==="
hdiutil detach "$VOL" >/dev/null
qemu-img convert -f raw -O qcow2 "$QEMU_IMAGES/_guestcp.raw" "$QEMU_SYSDISK"
rm -f "$QEMU_IMAGES/_guestcp.raw"
echo "DONE: $DEST_MAC is in the guest"
