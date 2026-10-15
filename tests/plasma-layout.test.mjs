// SPDX-License-Identifier: MIT
// Behavioral smoke test for the native Plasma layout JavaScript.
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import vm from "node:vm";
import test from "node:test";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const script = fs.readFileSync(path.join(root,
    "plasma/look-and-feel/org.meteora.desktop/contents/layouts/org.kde.plasma.desktop-layout.js"), "utf8");

test("layout creates branded panel, native taskbar, tray and desktop wallpaper", () => {
    const writes = [];
    const removed = [];
    const panels = [];
    const desktop = {
        wallpaperPlugin: "",
        currentConfigGroup: [],
        writeConfig(key, value) { writes.push(["wallpaper", key, value]); },
    };
    class MockWidget {
        constructor(name) {
            this.name = name;
            this.currentConfigGroup = [];
        }
        writeConfig(key, value) { writes.push([this.name, key, value]); }
    }
    class Panel {
        constructor() {
            this.added = [];
            panels.push(this);
        }
        addWidget(name) {
            const widget = new MockWidget(name);
            this.added.push(widget);
            return widget;
        }
    }
    const sandbox = {
        desktops: () => [desktop],
        userDataPath: (kind, filename) =>
            `/home/demo/.local/share/wallpapers/${filename}`,
        panelIds: [10, 11],
        panelById: (id) => ({ remove() { removed.push(id); } }),
        Panel,
        knownWidgetTypes: ["org.kde.plasma.pager"],
        applicationExists: (name) =>
            name === "org.kde.dolphin.desktop" || name === "org.kde.konsole.desktop",
    };
    vm.runInNewContext(script, sandbox, { filename: "meteora-layout.js" });

    assert.deepEqual(removed, [10, 11]);
    assert.equal(panels.length, 1);
    const panel = panels[0];
    assert.equal(panel.location, "bottom");
    assert.equal(panel.height, 46);
    assert.equal(panel.floating, true);
    assert.ok(writes.some(([type,key,val]) => type === "org.meteora.launcher" && key === "icon" && val === "meteora"));
    assert.equal(panel.added[0].name, "org.meteora.launcher");
    const widgets = panel.added.map(item => item.name);
    assert.equal(widgets[0], "org.meteora.launcher");
    for (const item of ["org.kde.plasma.icontasks", "org.kde.plasma.pager",
        "org.kde.plasma.systemtray", "org.kde.plasma.digitalclock",
        "org.kde.plasma.showdesktop"]) {
        assert.ok(widgets.includes(item), `Missing widget: ${item}`);
    }
    assert.ok(writes.some(([type,key,val]) =>
        type === "wallpaper" && key === "arrangement" && val === 1),
        "Desktop icons should form a left-hand column");
    assert.ok(writes.some(([type,key,val]) =>
        type === "org.meteora.launcher" && key === "applicationsDisplay" && val === 0),
        "App launcher should use an icon grid");
    assert.equal(desktop.wallpaperPlugin, "org.kde.image");
    assert.ok(writes.some(([type,key,val]) =>
        type === "wallpaper" && key === "Image" && val.includes("Meteora/contents/images/")));
    assert.ok(writes.some(([type,key,val]) =>
        type === "org.meteora.launcher" && key === "icon" && val === "meteora"));
    assert.ok(writes.some(([type,key,val]) =>
        type === "org.kde.plasma.icontasks" && key === "launchers" && val.includes("dolphin")));
});
