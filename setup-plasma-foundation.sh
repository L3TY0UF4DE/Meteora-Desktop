#!/usr/bin/env bash
# Meteora Plasma foundation: safe Arch installer.
# Reuses KDE Plasma packages rather than copying upstream code into MIT sources.
set -Eeuo pipefail
cd "$(dirname "$0")"
[[ -f /etc/arch-release ]] || { echo "Arch Linux required." >&2; exit 1; }
(( EUID != 0 )) || { echo "Run as your regular user, not root." >&2; exit 1; }
command -v pacman >/dev/null || { echo "pacman missing" >&2; exit 1; }
echo "Meteora Plasma foundation installer"
echo "Installs KDE Plasma, KWin, SDDM, and Qt6 tooling through Arch packages."
echo "Does NOT enable SDDM, change GRUB, replace your current session, or remove Weston."
read -r -p "Install KDE Plasma dependencies? [y/N] " reply
[[ "$reply" =~ ^[Yy]$ ]] || exit 0
sudo pacman -S --needed plasma-meta sddm qt6-base qt6-wayland cmake base-devel
install -d "$HOME/.local/share/wallpapers/Meteora/contents/images"
install -m 0644 assets/meteora-wallpaper.png "$HOME/.local/share/wallpapers/Meteora/contents/images/meteora-wallpaper.png"
echo
echo "KDE Plasma installed; Meteora wallpaper staged for this user."
echo "No session/login changes made. To try Plasma safely, use your existing session chooser."
echo "For a clean Arch TTY, you may optionally enable SDDM yourself:"
echo "  sudo systemctl enable --now sddm"
echo "This is a KDE-backed foundation, not yet a completed Meteora Plasma theme."
