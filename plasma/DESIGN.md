# Meteora Plasma visual specification

The original concept (provided by the project owner on 2026-10-15) is the design target, not the old Qt prototype.

## Non-negotiable
- Preserve the original textured Meteora skyline wallpaper and angular mark.
- Dark graphite/charcoal windows and navy-tinted surfaces with bright icy blue selection.
- Compact floating bottom panel: launcher at left, quick-launch/task icons, real workspace switching, tray and date at right.
- Wide categorized application launcher, searchable and keyboard accessible, using native Plasma menu infrastructure.
- Four upper-left desktop folder shortcuts, subject to the user's existing files.
- Native, working session functionality through KWin and KDE Plasma (not fake buttons).

## This implementation
- Plasma 6 Global Theme package: native Breeze Dark chrome with custom MeteoraDark palette.
- Plasma JavaScript layout: one bottom floating panel, Kickoff, icon-only tasks, pager, spacer, system tray, clock, show-desktop.
- Original user wallpaper installed separately; icon installed into hicolor theme.
- The old Qt/Weston preview remains in the repository as legacy reference only.
- Full visual parity (custom Kickoff grid, custom panel graphics, notifications previews) is **not yet claimed**.

## Validation gates
- Theme manifest must parse and declare Plasma/LookAndFeel.
- Colors must include the standard window/view/button/selection sections.
- Shell scripts must pass syntax checks and the installer must stage files without sudo.
- Plasma layout JavaScript must pass a syntax check and include functioning core widgets.
- Visual review should be performed in an actual Plasma session before claiming final polish.
