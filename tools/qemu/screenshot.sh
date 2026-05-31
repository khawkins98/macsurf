#!/usr/bin/env bash
# Capture the running guest's framebuffer to a PNG via QMP (works with any
# -display backend, including a live Cocoa window or fully headless).
# Usage: screenshot.sh [outfile.png]
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/config.sh"
OUT="${1:-$QEMU_LOGS/shot-$(date +%Y%m%d-%H%M%S).png}"
python3 "$HERE/qmp.py" "$QEMU_QMP_SOCK" screendump filename="$OUT" format=png >/dev/null
echo "$OUT"
