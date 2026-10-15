#!/usr/bin/env python3
"""Static packaging and isolated installer tests for Meteora Plasma 6."""
import configparser
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from xml.etree import ElementTree

ROOT = Path(__file__).resolve().parents[1]
LOOK = ROOT / "plasma/look-and-feel/org.meteora.desktop"
INSTALLER = ROOT / "install-meteora-plasma.sh"


class PlasmaPackageTest(unittest.TestCase):
    def test_theme_metadata(self):
        data = json.loads((LOOK / "metadata.json").read_text())
        self.assertEqual(data["KPackageStructure"], "Plasma/LookAndFeel")
        self.assertEqual(data["KPlugin"]["Id"], "org.meteora.desktop")
        wallpaper = json.loads((ROOT / "plasma/wallpaper/metadata.json").read_text())
        self.assertEqual(wallpaper["KPackageStructure"], "Wallpaper/Images")

    def test_palette(self):
        scheme = configparser.ConfigParser(interpolation=None)
        scheme.optionxform = str
        self.assertTrue(scheme.read(ROOT / "plasma/color-schemes/MeteoraDark.colors"))
        self.assertEqual(scheme["General"]["Name"], "Meteora Dark")
        for group in ["Window", "View", "Selection", "Button", "Tooltip", "Complementary"]:
            self.assertIn(f"Colors:{group}", scheme)
            self.assertIn("BackgroundNormal", scheme[f"Colors:{group}"])
            self.assertIn("ForegroundNormal", scheme[f"Colors:{group}"])

    def test_image_and_brand_icon(self):
        data = (ROOT / "assets/meteora-wallpaper.png").read_bytes()
        self.assertTrue(data.startswith(b"\x89PNG\r\n\x1a\n"))
        self.assertGreater(len(data), 10000)
        node = ElementTree.parse(ROOT / "plasma/icons/meteora.svg").getroot()
        self.assertTrue(node.tag.endswith("svg"))

    def test_plasma_layout(self):
        script = (LOOK / "contents/layouts/org.kde.plasma.desktop-layout.js").read_text()
        self.assertIn('new Panel', script)
        self.assertIn('panel.location = "bottom"', script)
        self.assertIn('panel.floating = true', script)
        self.assertIn('"org.kde.plasma.kickoff"', script)
        self.assertIn('"org.kde.plasma.icontasks"', script)
        self.assertIn('"org.kde.plasma.systemtray"', script)
        self.assertIn('"org.kde.plasma.digitalclock"', script)
        self.assertIn('"org.kde.plasma.pager"', script)
        self.assertIn('"org.kde.image"', script)
        self.assertNotIn('"Ctrl+Space"', script)

    def test_installer_reversible_in_fake_session(self):
        if os.geteuid() == 0:
            self.skipTest("Run installer tests as an unprivileged user")
        with tempfile.TemporaryDirectory(prefix="meteora-test-") as temp:
            home = Path(temp) / "home"
            state = home / ".local/state"
            data = home / ".local/share"
            config = home / ".config"
            for d in (home, state, data, config):
                d.mkdir(parents=True, exist_ok=True)
            (config / "kdeglobals").write_text("[General]\nName=Original\n")
            fakebin = Path(temp) / "fakebin"
            fakebin.mkdir()
            log = Path(temp) / "mock-calls"
            for name in ("plasma-apply-lookandfeel", "plasma-apply-colorscheme"):
                exe = fakebin / name
                exe.write_text('#!/bin/sh\nprintf "%s\\n" "$0 $*" >> "$METEORA_TEST_LOG"\n')
                exe.chmod(0o755)
            env = os.environ.copy()
            env.update({
                "HOME": str(home),
                "XDG_DATA_HOME": str(data),
                "XDG_STATE_HOME": str(state),
                "XDG_CONFIG_HOME": str(config),
                "XDG_CURRENT_DESKTOP": "KDE",
                "METEORA_TEST_LOG": str(log),
                "PATH": f"{fakebin}:{env['PATH']}",
            })

            def run(args, user_input=""):
                return subprocess.run(
                    ["bash", str(INSTALLER)] + args, cwd=ROOT, env=env,
                    input=user_input, text=True, capture_output=True, timeout=40,
                )

            result = run(["--install-only"])
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue((data / "plasma/look-and-feel/org.meteora.desktop/metadata.json").exists())
            self.assertTrue((data / "wallpapers/Meteora/contents/images/meteora-wallpaper.png").exists())
            self.assertTrue((data / "color-schemes/MeteoraDark.colors").exists())
            self.assertTrue((data / "applications/org.meteora.settings.desktop").exists())
            self.assertFalse(log.exists(), "Installing assets must not apply Plasma settings")

            env["XDG_CURRENT_DESKTOP"] = "GNOME"
            result = run(["--apply"], "METEORA\n")
            self.assertNotEqual(result.returncode, 0, "Refuse modifications outside KDE")
            env["XDG_CURRENT_DESKTOP"] = "KDE"
            result = run(["--apply"], "METEORA\n")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("--resetLayout", log.read_text())
            self.assertIn("MeteoraDark", log.read_text())
            backup = Path((state / "meteora/last-backup.txt").read_text().strip())
            self.assertEqual((backup / "kdeglobals").read_text(), "[General]\nName=Original\n")
            self.assertTrue((home / "Desktop/Home").is_symlink())
            self.assertTrue((home / "Desktop/Meteora Trash.desktop").exists())
            (config / "kdeglobals").write_text("modified\n")
            result = run(["--restore"], "RESTORE\n")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((config / "kdeglobals").read_text(), "[General]\nName=Original\n")


if __name__ == "__main__":
    unittest.main(verbosity=2)
