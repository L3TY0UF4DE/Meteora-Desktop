# Meteora for KDE Plasma 6

This profile turns installed KDE Plasma into a working Meteora-styled desktop using upstream KDE software. It does not fork KDE code. Plasma/KWin supply the compositor, window management, workspaces, system tray, window switching, notifications and power/session infrastructure.

## Layout mapped to original concept
- Original Meteora wallpaper (staged as a Plasma wallpaper image package).
- Meteora Dark custom Qt/KDE application color scheme.
- Native floating 46 px bottom panel, Kickoff launcher with Meteora icon.
- Real icons-only task switcher, app shortcuts if installed, pager, system tray, clock and show-desktop.
- Home/Documents/Downloads and Trash shortcuts on the desktop, without overwriting existing items.
- Native KDE System Settings accessible as "Meteora Settings."

## Install on Arch

From clean Arch TTY:
```bash
bash setup-plasma-foundation.sh
```

This asks before installing KDE packages. It does not enable your login manager. To enable SDDM on a fresh system, consciously run `sudo systemctl enable --now sddm` and choose **Plasma (Wayland)** from its session chooser.

Once in a logged-in Plasma session, from this repository:
```bash
bash install-meteora-plasma.sh --apply
```

The script asks you to type **METEORA**. It first makes a backup under `~/.local/state/meteora/backups/` before replacing the panel layout and theme. It does not delete your wallpaper files or installed applications.

To stage the appearance without applying it:
```bash
bash install-meteora-plasma.sh --install-only
```

To restore the backed-up configuration inside Plasma:
```bash
bash install-meteora-plasma.sh --restore
```

Logout/login after restore. Note: theme assets and added desktop shortcuts remain available; the restore operation targets the saved Plasma settings, not user files.

## Validation

`.github/workflows/plasma-package.yml` runs syntax and packaging tests on GitHub Actions, with an isolated fake Plasma command environment to exercise installation, backup and restore without touching the real user. These tests **do not guarantee actual Plasma rendering**. The final visual match must be checked in a real Plasma session.

## Licensing

Meteora's original packaging, script, icon and color scheme are MIT licensed. KDE software is used under its own licenses and installed via distro packages, not copied into Meteora's MIT source. Original wallpaper is separately licensed project artwork.
