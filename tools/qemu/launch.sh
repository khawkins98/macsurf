#!/usr/bin/env bash
# Launch the MacSurf OS 9 QEMU VM in one of three modes.
#
#   launch.sh install [--cdrom EXTRA.iso] [--transfer img]   GUI, boots OS 9 install ISO (-boot d)
#   launch.sh run     [--cdrom EXTRA.iso] [--transfer img]   GUI, boots installed disk (-boot c)
#   launch.sh headless                    [--transfer img]   no UI, QMP-driven, boots installed disk
#
# Flags:
#   --cdrom PATH     attach an extra CD image (e.g. the CodeWarrior toast) as the CD drive
#   --transfer PATH  attach a secondary raw HFS image as a second disk (host<->guest exchange)
#   --boot d|c       override boot device
#   --no-net         disable networking
#
# All modes expose a QMP socket ($QEMU_QMP_SOCK) and an HMP monitor socket
# ($QEMU_MON_SOCK) so scripts (and you) can drive/inspect the VM. Serial goes
# to $QEMU_SERIAL_LOG. Run via `&` or a background task for unattended use.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/config.sh"

MODE="${1:-run}"; shift || true
EXTRA_CDROM=""
TRANSFER=""
BOOT=""
WITH_NET=1
while [ $# -gt 0 ]; do
  case "$1" in
    --cdrom)    EXTRA_CDROM="$2"; shift 2 ;;
    --transfer) TRANSFER="$2"; shift 2 ;;
    --boot)     BOOT="$2"; shift 2 ;;
    --no-net)   WITH_NET=0; shift ;;
    *) echo "unknown flag: $1" >&2; exit 2 ;;
  esac
done

qemu_require "$QEMU_BIN"
[ -f "$QEMU_SYSDISK" ] || { echo "ERROR: system disk missing: $QEMU_SYSDISK (run create-disks.sh)" >&2; exit 1; }
mkdir -p "$QEMU_LOGS"
rm -f "$QEMU_QMP_SOCK" "$QEMU_MON_SOCK"

# ---- assemble args ---------------------------------------------------------
ARGS=(
  -L /opt/homebrew/share/qemu
  -M "$QEMU_MACHINE"
  -cpu "$QEMU_CPU"
  -m "$QEMU_RAM_MB"
  -smp 1
  -g "$QEMU_VIDEO"
  -drive "file=$QEMU_SYSDISK,format=qcow2,media=disk"
  -device usb-mouse
  -device usb-kbd
  -prom-env "vga-ndrv?=true"
  -qmp "unix:$QEMU_QMP_SOCK,server,nowait"
  -monitor "unix:$QEMU_MON_SOCK,server,nowait"
  -serial "file:$QEMU_SERIAL_LOG"
)

case "$MODE" in
  install)
    : "${MACSURF_OS9_ISO:=$QEMU_MEDIA/macos_921_ppc.iso}"
    [ -f "$MACSURF_OS9_ISO" ] || { echo "ERROR: OS 9 ISO missing: $MACSURF_OS9_ISO" >&2; exit 1; }
    ARGS+=( -drive "file=$MACSURF_OS9_ISO,format=raw,media=cdrom" )
    ARGS+=( -boot "${BOOT:-d}" )
    ARGS+=( -display "cocoa,swap-opt-cmd=on" )
    ARGS+=( -name "macsurf-os9 [INSTALL]" )
    ;;
  run)
    [ -n "$EXTRA_CDROM" ] && ARGS+=( -drive "file=$EXTRA_CDROM,format=raw,media=cdrom" )
    ARGS+=( -boot "${BOOT:-c}" )
    ARGS+=( -display "cocoa,swap-opt-cmd=on" )
    ARGS+=( -name "macsurf-os9 [RUN]" )
    ;;
  headless)
    [ -n "$EXTRA_CDROM" ] && ARGS+=( -drive "file=$EXTRA_CDROM,format=raw,media=cdrom" )
    ARGS+=( -boot "${BOOT:-c}" )
    ARGS+=( -display none )
    ARGS+=( -name "macsurf-os9 [HEADLESS]" )
    ;;
  *) echo "usage: launch.sh {install|run|headless} [--cdrom X] [--transfer Y]" >&2; exit 2 ;;
esac

# extra CD for install mode too (e.g. CodeWarrior toast alongside the OS install CD is not
# possible on one IDE channel; attach CW after OS install via `run --cdrom`).
if [ "$MODE" = "install" ] && [ -n "$EXTRA_CDROM" ]; then
  echo "NOTE: --cdrom ignored in install mode (OS ISO occupies the CD). Install OS first, then: launch.sh run --cdrom <toast>" >&2
fi

[ -n "$TRANSFER" ] && ARGS+=( -drive "file=$TRANSFER,format=raw,media=disk" )

if [ "$WITH_NET" = "1" ]; then
  ARGS+=( -netdev "user,id=net0,$QEMU_HOSTFWD" -device "$QEMU_NIC,netdev=net0" )
fi

echo "Launching ($MODE):"
printf '  %s\n' "${ARGS[@]}" | sed 's/^/  /'
echo "QMP:    $QEMU_QMP_SOCK"
echo "Serial: $QEMU_SERIAL_LOG"
exec "$QEMU_BIN" "${ARGS[@]}"
