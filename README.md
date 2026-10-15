# Meteora Desktop — Plasma 6 Preview

Meteora is a charcoal-and-ice-blue, Linkin Park *Meteora*-inspired
desktop experience for Arch Linux. We use **upstream KDE Plasma 6 + KWin**
for the actual Wayland desktop and supply **original Meteora branding,
wallpaper, colors, Plasma-style graphics, layout and setup tooling**.

The previous self-contained Qt/Weston mock desktop remains under the
legacy preview section below; it is **not** the new installation path.

## What's working in the KDE-backed build

- Native KWin window management, virtual desktops, Alt+Tab, Wayland support.
- Plasma notifications, system tray, search/application launcher, desktop icons.
- Meteora Dark color scheme, original angular icon, wallpaper image package.
- **Meteora Plasma style** with original dark blue bordered panel and popup graphics.
- Floating bottom panel with native Kickoff, task manager, pager, clock and tray.
- User-local installer with backup and `--restore` option.
- Native System Settings shortcut, rather than the old Qt preview's wallpaper-only dialog.

We do **not** claim the visual design is pixel-perfect yet. This release
needs real Arch/UTM Plasma runtime validation, especially launcher placement
and the Wayland graphics stack.

## Arch installation

For a clean Arch system, from a clone of the `development` branch:

```bash
bash setup-plasma-foundation.sh
```

The setup asks before running pacman, stages the Meteora theme, and does
**not** enable SDDM or modify GRUB. You can enable SDDM yourself if needed
with `sudo systemctl enable --now sddm`, then choose **Plasma (Wayland)**.

After signing in to Plasma, in this repository run:

```bash
bash install-meteora-plasma.sh --apply
```

Type `METEORA` after reviewing the panel replacement warning. Existing
Plasma settings are saved to `~/.local/state/meteora/backups/` first.
Wallpaper files, user applications and existing desktop items are not removed.

To restore the saved Plasma settings and log back in:

```bash
bash install-meteora-plasma.sh --restore
```

Theme source, screenshots/design guide, and detailed setup notes are in
[plasma/README.md](plasma/README.md) and [plasma/DESIGN.md](plasma/DESIGN.md).

## Developer checks

GitHub Actions validates Bash syntax, Plasma layout JS syntax, theme/wallpaper
manifests, SVG structures and fake-session install/apply/restore behavior.
Qt prototype compilation still runs separately. These are not a substitute
for testing in Plasma on Arch Linux.

## Licensing

Original Meteora scripts, app metadata, vectors and theme colors are under
MIT. KDE Plasma/KWin remain separately installed open-source KDE packages
under their own upstream licenses, and are not copied or relicensed by Meteora.
The original Meteora wallpaper is separately licensed artwork, not MIT code.

## Legacy Qt preview


An original C++20/Qt 6 desktop *visual prototype* for Arch Linux.

## Status

**NOT A DESKTOP SESSION.** This is a normal window running inside your current desktop environment. It demonstrates the wallpaper, charcoal/blue styling, floating panel, clock and installed-application launcher. The preview now opens fullscreen without a window title bar; close it with Alt+F4 or switch to a TTY. It does not manage windows, own the Wayland display, provide desktop icons, a system tray, settings, SDDM integration, or a GRUB theme. No bare-metal install script is provided yet, intentionally.

## Test safely on Arch Linux

```bash
sudo pacman -S --needed base-devel cmake qt6-base git
git clone -b development https://github.com/L3TY0UF4DE/Meteora-Desktop.git
cd Meteora-Desktop
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
./build/meteora-preview
```

Alternatively, from this cloned repository run `bash test-preview.sh` to configure, build, and launch the preview. This does not install a desktop session or change SDDM/GRUB.

GitHub Actions builds the preview on Ubuntu for an automated compile check; this does not replace testing on Arch Linux.


Run it **from your existing desktop session**. It will not change your login manager, bootloader, or system configuration. Launcher behavior is basic and does not yet implement the full Desktop Entry Exec specification; some applications may not launch correctly.

## Clean Arch TTY preview (experimental)

On a clean Arch installation with no desktop, clone the `development` branch,
log in as a normal user on a local TTY, and run:

```bash
bash setup-tty-preview.sh
```

This asks before installing build tools, Qt, an Adwaita cursor theme, and **Weston** as a temporary
Wayland compositor for the existing preview window. Weston is **not** Meteora's
compositor. The script does not enable SDDM, install a desktop environment,
or modify GRUB. Requires working DRM/KMS graphics, an active local seat and
a valid `XDG_RUNTIME_DIR`. Virtual machines or some GPU setups may fail.
The script configures a visible cursor theme and logs compositor startup failures. On UTM, ensure the VM has a USB tablet or compatible pointing device. Close the preview with Alt+F4 (or switch TTY). If it fails, share the terminal error; do not run the script as root.

## Meteora Settings

The native `meteora-settings` app is built alongside the desktop preview.
It currently supports selecting a wallpaper and restoring the default artwork;
changes are saved per user and picked up by the preview automatically.

The TTY setup script installs **both** executables, the Settings launcher entry,
and the default wallpaper to `~/.local` after its existing confirmation prompt.
No root-level desktop installation or SDDM/GRUB changes are performed.
Open Settings using the panel button or application launcher.

For a manual user-local install after building:

```bash
cmake --install build --prefix "$HOME/.local"
```

## Preview controls

- Click **Meteora** or press **Super** (Windows key, or Command when forwarded by UTM) to toggle the application launcher.
- Type to filter installed applications; press **Enter** to launch the selected result.
- Press **Escape** or click outside the launcher to dismiss it.
- Click **Exit preview** to return to the TTY testing session.
- These shortcuts are handled by the preview while it has keyboard focus, not globally by a standalone compositor.

The preview is still a Qt client hosted by temporary Weston. It does not provide
a real task manager, independent window management, system tray, notifications,
or a production-ready desktop session. No third-party desktop source code is
copied into the Meteora shell.

## Roadmap

1. Preview: branded wallpaper, basic panel, launcher (this package).
2. Prototype a real wlroots-backed Wayland compositor and layer-shell-aware shell; input, output, window management and shutdown handling.
3. Session packaging and testable SDDM theme; guarded GitHub installer.
4. Optional GRUB theme with backups and safe recovery.
5. Performance testing against XFCE and stability hardening.

No user applications are bundled. The wallpaper is the Meteora artwork approved during design.

## Licensing

Source code is offered under MIT (see LICENSE). The included wallpaper is a project asset; it is not covered by the MIT code license. Define an explicit artwork license before public redistribution.
