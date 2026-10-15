# Meteora Desktop — Alpha 0.1 development prototype

An original C++20/Qt 6 desktop *visual prototype* for Arch Linux.

## Status

**NOT A DESKTOP SESSION.** This is a normal window running inside your current desktop environment. It demonstrates the wallpaper, charcoal/blue styling, panel, clock and installed-application launcher. It does not manage windows, own the Wayland display, provide desktop icons, a system tray, settings, SDDM integration, or a GRUB theme. No bare-metal install script is provided yet, intentionally.

## Test safely on Arch Linux

```bash
sudo pacman -S --needed base-devel cmake qt6-base git
unzip meteora-desktop-alpha01.zip
cd meteora-desktop-alpha01
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
./build/meteora-preview
```

Run it **from your existing desktop session**. It will not change your login manager, bootloader, or system configuration. Launcher behavior is basic and does not yet implement the full Desktop Entry Exec specification; some applications may not launch correctly.

## Roadmap

1. Preview: branded wallpaper, basic panel, launcher (this package).
2. Prototype a real wlroots-backed Wayland compositor and layer-shell-aware shell; input, output, window management and shutdown handling.
3. Session packaging and testable SDDM theme; guarded GitHub installer.
4. Optional GRUB theme with backups and safe recovery.
5. Performance testing against XFCE and stability hardening.

No user applications are bundled. The wallpaper is the Meteora artwork approved during design.

## Licensing

Source code is offered under MIT (see LICENSE). The included wallpaper is a project asset; it is not covered by the MIT code license. Define an explicit artwork license before public redistribution.
