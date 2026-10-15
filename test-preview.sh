#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if [[ "$(uname -s)" != "Linux" ]]; then
  echo "Meteora Preview currently targets Linux." >&2
  exit 1
fi
for cmd in cmake c++; do
  command -v "$cmd" >/dev/null || { echo "Missing: $cmd" >&2; exit 1; }
done
if ! command -v pkg-config >/dev/null || ! pkg-config --exists Qt6Widgets Qt6Gui; then
  echo "Qt 6 development libraries not found." >&2
  echo "On Arch: sudo pacman -S --needed base-devel cmake qt6-base" >&2
  exit 1
fi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
  echo "No X11 or Wayland display detected. Open a terminal inside a graphical desktop session." >&2
  echo "This preview is not a standalone desktop session." >&2
  exit 1
fi
if [[ -z "${QT_QPA_PLATFORM:-}" && -n "${WAYLAND_DISPLAY:-}" ]]; then
  export QT_QPA_PLATFORM=wayland
  echo "Detected Wayland; using Qt Wayland platform plugin."
fi
echo "Launching Meteora preview inside your current desktop session."
exec ./build/meteora-preview
