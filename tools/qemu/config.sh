#!/usr/bin/env bash
# MacSurf QEMU build-harness configuration.
# Sourced by all tools/qemu/*.sh scripts. Edit paths/tunables here.
#
# The VM workspace lives OUTSIDE the git repo so multi-GB disk images,
# ISOs, and snapshots never get committed. Only scripts + docs are tracked.

# ---- Workspace layout (outside the repo) -----------------------------------
QEMU_WS="${QEMU_WS:-$HOME/macsurf-qemu}"
QEMU_IMAGES="$QEMU_WS/images"       # system + transfer disk images
QEMU_MEDIA="$QEMU_WS/media"         # OS 9 install ISO(s), CodeWarrior media (user-supplied)
QEMU_SNAPSHOTS="$QEMU_WS/snapshots" # external snapshot artifacts / notes
QEMU_TRANSFER="$QEMU_WS/transfer"   # staging dir for host->guest file injection
QEMU_LOGS="$QEMU_WS/logs"           # serial logs, build logs pulled back from guest

# ---- Disk images -----------------------------------------------------------
QEMU_SYSDISK="$QEMU_IMAGES/os9-system.qcow2"   # OS 9 boot/system disk
QEMU_XFERDISK="$QEMU_IMAGES/transfer.hfs.img"  # raw HFS image for host<->guest exchange

# ---- VM hardware (locked from 6-dimension research; see SETUP.md) ----------
QEMU_BIN="${QEMU_BIN:-qemu-system-ppc}"
QEMU_MACHINE="${QEMU_MACHINE:-mac99,via=pmu}"  # New World G4; via=pmu enables USB
QEMU_CPU="${QEMU_CPU:-g4}"                      # g4 ok for 9.2.x (g3 only for 9.0/9.1)
QEMU_RAM_MB="${QEMU_RAM_MB:-512}"              # HARD: OS 9 won't boot <=64MB; unstable/no-audio >=1024MB. Stay 256-512.
QEMU_VIDEO="${QEMU_VIDEO:-1024x768x32}"        # OS 9 has no arbitrary-res driver; pin a known-good mode. x32/x16 reliable, x8 flaky.
# Accel: PPC-on-arm64 is ALWAYS pure TCG (no KVM/HVF; no MTTCG for ppc => -smp 1).
# split-wx=on is required for the JIT under macOS hardened runtime; tb-size enlarges
# the translation-block cache so a big CodeWarrior build doesn't thrash re-translating.
QEMU_ACCEL="${QEMU_ACCEL:-tcg,split-wx=on,tb-size=512}"

# ---- Networking (SLIRP user-mode) ------------------------------------------
# sungem = Apple GMAC, OS 9 has a built-in Open Transport driver (no install).
# SLIRP NAT: guest=10.0.2.15  gateway/HOST=10.0.2.2  DNS=10.0.2.3
QEMU_NIC="${QEMU_NIC:-sungem}"
# host->guest port forwards (for SSH/FTP into the guest later); guest->host needs none.
QEMU_HOSTFWD="${QEMU_HOSTFWD:-hostfwd=tcp::2222-:22,hostfwd=tcp::2121-:21}"
# The MacSurf TLS proxy, as the OS 9 guest must address it (host = SLIRP gateway).
MACSURF_PROXY_HOSTPORT="${MACSURF_PROXY_HOSTPORT:-10.0.2.2:8765}"

# ---- Control sockets -------------------------------------------------------
QEMU_QMP_SOCK="$QEMU_WS/qmp.sock"
QEMU_MON_SOCK="$QEMU_WS/monitor.sock"
QEMU_SERIAL_LOG="$QEMU_LOGS/serial.log"

# ---- Helpers ---------------------------------------------------------------
qemu_require() {
  command -v "$1" >/dev/null 2>&1 || { echo "ERROR: required tool '$1' not found" >&2; return 1; }
}
