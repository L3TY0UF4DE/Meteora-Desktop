#!/usr/bin/env bash
# Meteora Alpha 0.1: clean Arch TTY preview setup
set -euo pipefail
cd "$(dirname "$0")"
if [[ ! -f /etc/arch-release ]]; then
  echo "This script supports Arch Linux only." >&2; exit 1
fi
if [[ "${EUID}" -eq 0 ]]; then
  echo "Run as your normal user (not root). sudo is used only for pacman." >&2; exit 1
fi
if [[ -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" ]]; then
  echo "Existing graphical session detected; run bash test-preview.sh instead." >&2; exit 1
fi
if [[ ! -t 0 ]]; then
  echo "Run from a local interactive TTY, not a noninteractive shell." >&2; exit 1
fi
echo "Meteora Alpha 0.1 uses a temporary Weston compositor to display the Qt preview."
echo "This is NOT the Meteora Wayland compositor or a complete desktop."
echo "Packages: base-devel cmake qt6-base qt6-wayland weston xorg-xwayland"
echo "No display manager, GRUB, or desktop environment will be configured."
read -r -p "Install required packages using pacman? [y/N] " confirm
[[ "$confirm" =~ ^[Yy]$ ]] || exit 0
sudo pacman -S --needed base-devel cmake qt6-base qt6-wayland weston xorg-xwayland
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
if [[ -z "${XDG_RUNTIME_DIR:-}" ]]; then
  echo "XDG_RUNTIME_DIR is unset. Log in directly on a local TTY with a systemd-logind session." >&2
  exit 1
fi
if [[ ! -d "$XDG_RUNTIME_DIR" || ! -w "$XDG_RUNTIME_DIR" ]]; then
  echo "XDG_RUNTIME_DIR is not writable: $XDG_RUNTIME_DIR" >&2; exit 1
fi
echo "Starting temporary Weston compositor. Close Meteora preview to return to TTY."
socket="meteora-preview-$"
weston --backend=drm --shell=kiosk-shell.so --socket="$socket" &
weston_pid=$!
cleanup() { kill "$weston_pid" 2>/dev/null || true; wait "$weston_pid" 2>/dev/null || true; }
trap cleanup EXIT INT TERM
for _ in $(seq 1 50); do
  if [[ -S "$XDG_RUNTIME_DIR/$socket" ]]; then break; fi
  if ! kill -0 "$weston_pid" 2>/dev/null; then
    echo "Weston exited before creating a display. Check GPU/seat permissions." >&2; exit 1
  fi
  sleep 0.1
done
if [[ ! -S "$XDG_RUNTIME_DIR/$socket" ]]; then
  echo "Timed out waiting for Weston display." >&2; exit 1
fi
QT_QPA_PLATFORM=wayland WAYLAND_DISPLAY="$socket" ./build/meteora-preview
