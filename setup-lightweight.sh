#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -Eeuo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
deps=false
system=false
for arg in "$@"; do
    case "$arg" in
        --install-deps) deps=true ;;
        --system) system=true ;;
        -h|--help)
            echo 'Usage: bash setup-lightweight.sh [--install-deps] [--system]'
            echo 'Default: build and install for this user in ~/.local.'
            echo '--install-deps: install a small explicit Arch package list with pacman.'
            echo '--system: install in /usr/local, including a login-session entry.'
            exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 2 ;;
    esac
done
(( EUID != 0 )) || { echo 'Run as your regular user. The script uses sudo only when needed.' >&2; exit 1; }
if "$deps"; then
    [[ -f /etc/arch-release ]] || { echo '--install-deps currently supports Arch Linux. See README for other distributions.' >&2; exit 1; }
    sudo pacman -S --needed base-devel cmake ninja pkgconf qt6-base qt6-svg glib2-devel libx11 libxi openbox adwaita-icon-theme ttf-dejavu xdg-utils dbus xorg-server xorg-xinit xorg-xprop xorg-server-xephyr xorg-xauth
fi
jobs=${METEORA_BUILD_JOBS:-2}
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo 'METEORA_BUILD_JOBS must be a positive integer.' >&2; exit 2; }
prefix="$HOME/.local"
"$system" && prefix=/usr/local
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    -DCMAKE_INSTALL_PREFIX="$prefix" -DMETEORA_INSTALL_SESSION_ENTRY="$system"
cmake --build build --parallel "$jobs"
if "$system"; then
    sudo cmake --install build
else
    cmake --install build
fi
printf '\nMeteora is built.\n'
if "$system"; then
    echo 'Log out and choose Meteora (Lightweight) in your existing login manager.'
    echo 'From a local TTY: startx /usr/local/bin/meteora-session'
else
    echo 'Try the real desktop safely in a nested window: bash demo-lightweight.sh'
    echo 'Window-only preview: ~/.local/bin/meteora-shell --preview'
fi
echo 'No Plasma session, bundled user apps, display-manager enablement or GRUB changes.'
