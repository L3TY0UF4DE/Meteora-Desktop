#!/usr/bin/env bash
# Meteora Plasma foundation: safe Arch installer.
# Reuses KDE Plasma packages rather than copying upstream code into MIT sources.
set -Eeuo pipefail
cd "$(dirname "$0")"
[[ -f /etc/arch-release ]] || { echo "Arch Linux required." >&2; exit 1; }
(( EUID != 0 )) || { echo "Run as your regular user, not root." >&2; exit 1; }
command -v pacman >/dev/null || { echo "pacman missing" >&2; exit 1; }
echo "Meteora Plasma foundation installer"
echo "Installs KDE Plasma, KWin, SDDM, Dolphin, Konsole, System Settings and Qt6 tooling."
echo "Does NOT enable SDDM, NetworkManager, change GRUB, or replace your session."
read -r -p "Install KDE Plasma dependencies? [y/N] " reply
[[ "$reply" =~ ^[Yy]$ ]] || exit 0
sudo pacman -S --needed plasma-meta sddm dolphin konsole systemsettings plasma-nm plasma-pa bluedevil breeze-icons xdg-desktop-portal-kde qt6-base qt6-wayland cmake base-devel
bash install-meteora-plasma.sh --install-only
echo
echo "KDE Plasma installed; Meteora Global Theme, palette, icon and wallpaper staged for this user."
echo "No session/login changes made. To try Plasma safely, use your existing session chooser."
echo "For a clean Arch TTY, you may optionally enable SDDM yourself:"
echo "  sudo systemctl enable --now sddm"
echo "After login to KDE Plasma, run: bash install-meteora-plasma.sh --apply"
