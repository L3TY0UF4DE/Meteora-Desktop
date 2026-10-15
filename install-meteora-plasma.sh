#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Install the Meteora Plasma 6 theme into the CURRENT USER's data directories.
# --apply is intentionally explicit and backs up settings before resetting panels.
set -Eeuo pipefail
umask 077

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
data_home="${XDG_DATA_HOME:-$HOME/.local/share}"
state_home="${XDG_STATE_HOME:-$HOME/.local/state}"
config_home="${XDG_CONFIG_HOME:-$HOME/.config}"
backup_root="$state_home/meteora/backups"
theme_id="org.meteora.desktop"
mode="${1:---install-only}"

usage() {
    cat <<'HELP'
Usage:
  bash install-meteora-plasma.sh --install-only
  bash install-meteora-plasma.sh --apply
  bash install-meteora-plasma.sh --restore [backup-directory]

--install-only  Copies original Meteora theme, artwork and icon for this user.
--apply         Applies the theme, colors and fresh Plasma panel layout after
                an explicit confirmation. Backs up existing settings first.
--restore       Restores a recorded backup; requires log out/in afterward.
Requires KDE Plasma 6 for --apply. Never changes GRUB, SDDM, or root configs.
HELP
}

if (( EUID == 0 )); then
    echo "Do not run this installer with sudo or as root." >&2
    exit 1
fi
if [[ ! -f "$root/plasma/look-and-feel/$theme_id/metadata.json" ||
      ! -f "$root/assets/meteora-wallpaper.png" ]]; then
    echo "Run this script from a complete Meteora Desktop checkout." >&2
    exit 1
fi

need_plasma() {
    if [[ ":${XDG_CURRENT_DESKTOP:-}:" != *":KDE:"* && 
          "${XDG_CURRENT_DESKTOP:-}" != *"KDE"* ]]; then
        echo "This action needs a logged-in KDE Plasma session (XDG_CURRENT_DESKTOP=KDE)." >&2
        exit 1
    fi
}

install_theme() {
    local destination="$data_home/plasma/look-and-feel/$theme_id"
    install -d "$destination" "$data_home/color-schemes" "$data_home/wallpapers/Meteora/contents/images"
    install -d "$data_home/icons/hicolor/scalable/apps"
    cp -R "$root/plasma/look-and-feel/$theme_id/." "$destination/"
    install -m 0644 "$root/plasma/color-schemes/MeteoraDark.colors" "$data_home/color-schemes/MeteoraDark.colors"
    install -m 0644 "$root/assets/meteora-wallpaper.png" "$data_home/wallpapers/Meteora/contents/images/meteora-wallpaper.png"
    install -m 0644 "$root/plasma/wallpaper/metadata.json" "$data_home/wallpapers/Meteora/metadata.json"
    install -m 0644 "$root/plasma/icons/meteora.svg" "$data_home/icons/hicolor/scalable/apps/meteora.svg"
    echo "Installed Meteora Plasma assets for $USER."
}

backup_config() {
    local date_stamp backup file
    date_stamp="$(date -u +%Y%m%dT%H%M%SZ)"
    backup="$backup_root/${date_stamp}-$$"
    install -d "$backup"
    for file in plasma-org.kde.plasma.desktop-appletsrc plasmashellrc \
                kdeglobals plasmarc kwinrc kglobalshortcutsrc; do
        if [[ -f "$config_home/$file" ]]; then
            cp -p "$config_home/$file" "$backup/$file"
            printf '%s\n' "$file" >> "$backup/saved-files.txt"
        fi
    done
    printf '%s\n' "$backup" > "$state_home/meteora/last-backup.txt"
    echo "$backup"
}

restore_config() {
    need_plasma
    local folder="${1:-}"
    if [[ -z "$folder" ]]; then
        if [[ -f "$state_home/meteora/last-backup.txt" ]]; then
            IFS= read -r folder < "$state_home/meteora/last-backup.txt"
        fi
    fi
    [[ -n "$folder" && -f "$folder/saved-files.txt" ]] || {
        echo "No recorded backup found." >&2; exit 1;
    }
    local canonical_root canonical_folder
    canonical_root="$(realpath -m "$backup_root")"
    canonical_folder="$(realpath -m "$folder")"
    [[ "$canonical_folder" == "$canonical_root/"* ]] || {
        echo "Refusing to restore from outside Meteora's backup directory." >&2; exit 1;
    }
    echo "Restore KDE Plasma settings from: $canonical_folder"
    read -r -p "Type RESTORE to continue: " answer
    [[ "$answer" == "RESTORE" ]] || exit 0
    # Files will take effect after a clean logout/login, avoiding a forced crash
    # or dangerous live Plasma process kill.
    install -d "$config_home"
    while IFS= read -r file; do
        case "$file" in
            plasma-org.kde.plasma.desktop-appletsrc|plasmashellrc|kdeglobals|plasmarc|kwinrc|kglobalshortcutsrc) ;;
            *) echo "Unsafe filename in manifest." >&2; exit 1 ;;
        esac
        [[ -f "$canonical_folder/$file" ]] || { echo "Backup incomplete." >&2; exit 1; }
        cp -p "$canonical_folder/$file" "$config_home/$file"
    done < "$canonical_folder/saved-files.txt"
    echo "Files restored. Log out of Plasma now (without making further layout changes) and log back in."
}

case "$mode" in
    --help|-h) usage ;;
    --install-only)
        install_theme
        echo "Open KDE System Settings → Colors & Themes → Global Theme to apply Meteora."
        echo "Or run this script with --apply from inside KDE Plasma."
        ;;
    --apply)
        need_plasma
        command -v plasma-apply-lookandfeel >/dev/null || {
            echo "plasma-apply-lookandfeel is missing. Install plasma-workspace first." >&2; exit 1;
        }
        command -v plasma-apply-colorscheme >/dev/null || {
            echo "plasma-apply-colorscheme is missing." >&2; exit 1;
        }
        install_theme
        echo "WARNING: Applying Meteora will replace the current Plasma panels and wallpaper."
        echo "Existing configuration will be saved under $backup_root."
        read -r -p "Type METEORA to apply the desktop layout: " answer
        [[ "$answer" == "METEORA" ]] || { echo "Theme installed but layout unchanged."; exit 0; }
        backup="$(backup_config)"
        echo "Backup saved at $backup"
        plasma-apply-lookandfeel -a "$theme_id" --resetLayout
        plasma-apply-colorscheme MeteoraDark
        echo "Meteora Plasma layout applied. Log out and back in if icons or panels do not update."
        ;;
    --restore)
        restore_config "${2:-}"
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac
