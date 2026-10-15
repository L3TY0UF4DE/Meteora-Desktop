#!/usr/bin/env bash
# Meteora Alpha 0.1: clean Arch / UTM graphical preview
set -Eeuo pipefail
cd "$(dirname "$0")"
if [[ ! -f /etc/arch-release ]]; then echo "Arch Linux required." >&2; exit 1; fi
if [[ $EUID -eq 0 ]]; then echo "Run as a regular user, never with sudo." >&2; exit 1; fi
if [[ -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" ]]; then
  echo "A graphical session is already running. Use bash test-preview.sh." >&2; exit 1
fi
if [[ ! -t 0 ]]; then echo "Run from an interactive local TTY." >&2; exit 1; fi
echo "Meteora Alpha 0.1 test environment (temporary Weston compositor)"
echo "This is NOT a standalone Meteora desktop or permanent installation."
echo "Packages: base-devel cmake qt6-base qt6-wayland weston adwaita-cursors"
echo "No SDDM, GRUB, autostart, or full desktop environment changes."
read -r -p "Install dependencies and start the preview? [y/N] " answer
[[ "$answer" =~ ^[Yy]$ ]] || exit 0
sudo pacman -S --needed base-devel cmake qt6-base qt6-wayland weston adwaita-cursors
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
# Install both Meteora executables, desktop entry, and wallpaper for this user only.
# Never alter system packages, login manager, or bootloader.
cmake --install build --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
export XDG_DATA_DIRS="$HOME/.local/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
if [[ -z "${XDG_RUNTIME_DIR:-}" || ! -d "${XDG_RUNTIME_DIR:-/nonexistent}" || ! -w "$XDG_RUNTIME_DIR" ]]; then
  echo "No writable XDG_RUNTIME_DIR. Log in normally on a local TTY (systemd-logind)." >&2; exit 1
fi
if [[ -z "${XDG_SESSION_ID:-}" ]]; then
  echo "Warning: XDG_SESSION_ID is unset; DRM seat access may not work." >&2
fi
if [[ ! -e /dev/input/event0 ]]; then
  echo "Warning: no /dev/input/event0 found. Check UTM input device configuration." >&2
fi
tmpdir=$(mktemp -d)
socket="meteora-test-$$"
weston_pid=""
cleanup() {
  if [[ -n "$weston_pid" ]]; then kill "$weston_pid" 2>/dev/null || true; wait "$weston_pid" 2>/dev/null || true; fi
  rm -rf "$tmpdir"
}
trap cleanup EXIT
cat > "$tmpdir/weston.ini" <<'INI'
[core]
shell=desktop-shell.so
idle-time=0
xwayland=false

[shell]
locking=false
panel-position=none
cursor-theme=Adwaita
cursor-size=32
INI
echo "Starting Weston (temporary Wayland compositor)..."
weston --backend=drm --config="$tmpdir/weston.ini" --socket="$socket" --log="$tmpdir/weston.log" &
weston_pid=$!
ready=false
for ((i=0;i<100;i++)); do
  if [[ -S "$XDG_RUNTIME_DIR/$socket" ]]; then ready=true; break; fi
  if ! kill -0 "$weston_pid" 2>/dev/null; then break; fi
  sleep 0.1
done
if [[ "$ready" != true ]]; then
  echo "Weston failed to start. Log follows:" >&2
  cat "$tmpdir/weston.log" >&2 || true
  exit 1
fi
echo "Meteora Settings installed at $HOME/.local/bin/meteora-settings"
echo "Preview launched. Open Settings from the panel or application launcher."
echo "If the pointer is missing, verify UTM has a USB tablet/pointing device."
QT_QPA_PLATFORM=wayland WAYLAND_DISPLAY="$socket" ./build/meteora-preview || {
  result=$?
  echo "Preview exited with code $result. Weston log:" >&2
  cat "$tmpdir/weston.log" >&2 || true
  exit "$result"
}
