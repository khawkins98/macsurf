#!/usr/bin/env bash
# Create the OS 9 system disk (qcow2). Idempotent — won't clobber an existing one.
# The transfer/shared HFS disk is built on demand by inject-source.sh, not here.
# Usage: create-disks.sh [size]   (default 4G)
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/config.sh"
qemu_require qemu-img
mkdir -p "$QEMU_IMAGES"
SIZE="${1:-4G}"
if [ -f "$QEMU_SYSDISK" ]; then
  echo "system disk already exists (leaving as-is): $QEMU_SYSDISK"
else
  qemu-img create -f qcow2 "$QEMU_SYSDISK" "$SIZE"
  echo "created $QEMU_SYSDISK ($SIZE)"
fi
qemu-img info "$QEMU_SYSDISK" | grep -iE "file format|virtual size|disk size"
