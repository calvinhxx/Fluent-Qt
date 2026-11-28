from pathlib import Path
import tempfile
import unittest

from vcpkg_port_cache import dependency_archives


class PortCacheTest(unittest.TestCase):
    def test_cache_follows_dependency_abis_across_port_updates(self):
        with tempfile.TemporaryDirectory() as directory:
            plan = Path(directory) / "packages.config"

            def archives(port_version, qt_abi):
                plan.write_text(f'''<packages>
                  <package id="qtbase_x64-linux" version="6.10.2-vcpkg{qt_abi}"/>
                  <package id="fluent-qt_x64-linux" version="{port_version}"/>
                </packages>''')
                return dependency_archives(plan, "x64-linux")

            old = archives("1.12.3-vcpkg" + "a" * 64, "b" * 64)
            self.assertEqual(old, [f".vcpkg-binary-cache/bb/{'b' * 64}.zip"])
            self.assertEqual(archives("1.12.4-vcpkg" + "c" * 64, "b" * 64), old)
            self.assertNotEqual(archives("1.12.4-vcpkg" + "c" * 64, "d" * 64), old)
            with self.assertRaisesRegex(ValueError, "Missing vcpkg ABI"):
                archives("1.12.4", "unknown")
