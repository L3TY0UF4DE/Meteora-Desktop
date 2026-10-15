// SPDX-License-Identifier: MIT
// Meteora Desktop layout for Plasma 6.
// Uses the public Plasma scripting API; contains no copied KDE source code.
(function () {
    var wallpaperPath = userDataPath("wallpaper", "Meteora/contents/images/meteora-wallpaper.png");
    var desktopsOnSession = desktops();
    for (var i = 0; i < desktopsOnSession.length; ++i) {
        var desktop = desktopsOnSession[i];
        desktop.wallpaperPlugin = "org.kde.image";
        desktop.currentConfigGroup = ["Wallpaper", "org.kde.image", "General"];
        desktop.writeConfig("Image", "file://" + wallpaperPath);
        desktop.writeConfig("FillMode", 2); // Scaled and cropped to fill
        desktop.currentConfigGroup = ["General"];
        desktop.writeConfig("arrangement", 1); // icons top to bottom
        desktop.writeConfig("alignment", 0); // left
        desktop.writeConfig("iconSize", 2); // 48 px
        desktop.writeConfig("labelWidth", 0); // narrow
        desktop.writeConfig("sortMode", 0); // user arranged
    }

    // This script is applied only with explicit consent and a saved backup.
    // Replace existing panels to prevent stacking two taskbars.
    var existing = panelIds.slice();
    for (var j = 0; j < existing.length; ++j) {
        var oldPanel = panelById(existing[j]);
        if (oldPanel) {
            oldPanel.remove();
        }
    }

    var panel = new Panel;
    panel.location = "bottom";
    panel.height = 46;
    panel.floating = true;
    panel.lengthMode = "fill";
    panel.hiding = "none";

    var launcher = panel.addWidget("org.kde.plasma.kickoff");
    launcher.currentConfigGroup = ["General"];
    launcher.writeConfig("icon", "meteora");
    launcher.writeConfig("favoritesDisplay", 0); // icon grid
    launcher.writeConfig("applicationsDisplay", 0); // icon grid

    // KDE's standard launcher mapping supports Meta/Windows on Plasma.
    // No Ctrl+Space binding is added.
    var pinned = [];
    if (applicationExists("org.kde.dolphin.desktop")) {
        pinned.push("applications:org.kde.dolphin.desktop");
    }
    if (applicationExists("org.kde.konsole.desktop")) {
        pinned.push("applications:org.kde.konsole.desktop");
    }
    if (applicationExists("firefox.desktop")) {
        pinned.push("applications:firefox.desktop");
    }

    var tasks = panel.addWidget("org.kde.plasma.icontasks");
    if (pinned.length) {
        tasks.currentConfigGroup = ["General"];
        tasks.writeConfig("launchers", pinned.join(","));
    }

    if (knownWidgetTypes.indexOf("org.kde.plasma.pager") !== -1) {
        panel.addWidget("org.kde.plasma.pager");
    }
    panel.addWidget("org.kde.plasma.panelspacer");
    panel.addWidget("org.kde.plasma.systemtray");
    var clock = panel.addWidget("org.kde.plasma.digitalclock");
    clock.currentConfigGroup = ["Appearance"];
    clock.writeConfig("showDate", true);
    clock.writeConfig("dateFormat", "longDate");
    panel.addWidget("org.kde.plasma.showdesktop");
})();
