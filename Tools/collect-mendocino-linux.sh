#!/bin/sh
# Read-only AMD GPU identification for port debugging; no firmware flash or kernel changes.
set -eu
out="${1:-mendocino-hardware-report.txt}"
{
 echo 'Mendocino Radeon 610M hardware report'
 echo 'OS/kernel:'; uname -sr
 echo 'AMD PCI display controllers:'
 if command -v lspci >/dev/null 2>&1; then lspci -nnk -d 1002: | grep -A 5 -B 1 -E 'VGA|Display|3D|1506' || true; else echo 'lspci unavailable'; fi
 echo 'DRM devices:'
 for card in /sys/class/drm/card[0-9]*; do
  [ -f "$card/device/vendor" ] || continue
  v=$(cat "$card/device/vendor")
  [ "$v" = '0x1002' ] || continue
  printf '%s: vendor=%s device=%s revision=%s\n' "$card" "$v" "$(cat "$card/device/device")" "$(cat "$card/device/revision")"
  if [ -e "$card/device/driver" ]; then printf 'driver=%s\n' "$(basename "$(readlink -f "$card/device/driver")")"; fi
 done
} > "$out"
printf 'Report saved to %s\n' "$out"
