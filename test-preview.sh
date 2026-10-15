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
echo "Launching Meteora preview inside your current desktop session."
exec ./build/meteora-preview
