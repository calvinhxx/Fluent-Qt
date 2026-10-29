"""Tests for FluentQt wheel metadata helpers."""

import argparse
import ast
import importlib.util
import json
from email.parser import Parser
from pathlib import Path
import struct
import sys
import tempfile
import unittest


WHEEL_BUILDER_PATH = (
    Path(__file__).resolve().parents[1]
    / "tools"
    / "build_wheel.py"
)
WHEEL_BUILDER_SPEC = importlib.util.spec_from_file_location(
    "fluentqt_build_wheel",
    WHEEL_BUILDER_PATH,
)
WHEEL_BUILDER = importlib.util.module_from_spec(WHEEL_BUILDER_SPEC)
WHEEL_BUILDER_SPEC.loader.exec_module(WHEEL_BUILDER)


def pe_image(dll_names):
    """Return a minimal PE32+ image whose import directory names DLLs."""
    optional = 0x58
    optional_size = 112 + 16 * 8
    header = bytearray(0x200)
    header[0:2] = b"MZ"
    struct.pack_into("<I", header, 0x3C, 0x40)
    header[0x40:0x44] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", header, 0x44, 0x8664, 1, 0, 0, 0, optional_size, 0)
    struct.pack_into("<H", header, optional, 0x20B)
    struct.pack_into("<I", header, optional + 108, 16)
    struct.pack_into("<II", header, optional + 120, 0x1000, 0x200)
    section = optional + optional_size
    header[section:section + 8] = b".idata\0\0"
    struct.pack_into("<4I", header, section + 8, 0x200, 0x1000, 0x200, 0x200)
    imports = bytearray(0x200)
    name_offset = 20 * (len(dll_names) + 1)
    for index, name in enumerate(dll_names):
        struct.pack_into("<5I", imports, 20 * index, 0, 0, 0, 0x1000 + name_offset, 0)
        encoded = name.encode("ascii") + b"\0"
        imports[name_offset:name_offset + len(encoded)] = encoded
        name_offset += len(encoded)
    return bytes(header + imports)


class WheelBuilderTest(unittest.TestCase):
    def test_wheel_and_smoke_inventories_cover_every_source_facade(self):
        # Do not import fluentqt: this gate must run before Qt is installed.
        binding_root = WHEEL_BUILDER_PATH.parents[1]
        facades = {path.stem for path in (binding_root / "src/fluentqt").glob("*.py")}
        manifest = json.loads((binding_root / "api-manifest.json").read_text(encoding="utf-8"))
        optional_facades = {name.removeprefix("fluentqt.") for name in manifest["optional_modules"]}
        self.assertTrue(optional_facades <= facades, "Optional module facade is missing")
        for name in optional_facades:
            self.assertNotIn(name + ".py", WHEEL_BUILDER.REQUIRED_PACKAGE_FILES)
            self.assertNotIn(name + ".pyi", WHEEL_BUILDER.REQUIRED_PACKAGE_FILES)
        facades -= optional_facades
        expected_stubs = {name + ".pyi" for name in facades} | {"_fluentqt.pyi"}
        expected_files = {name + ".py" for name in facades} | expected_stubs
        self.assertFalse(
            expected_files - WHEEL_BUILDER.REQUIRED_PACKAGE_FILES,
            "Update the core wheel inventory when adding a facade",
        )
        smoke = ast.parse((binding_root / "tests/test_wheel_smoke.py").read_text(encoding="utf-8"))
        declarations = [
            ast.literal_eval(node.value)
            for node in ast.walk(smoke)
            if isinstance(node, ast.Assign)
            and any(isinstance(target, ast.Name) and target.id == "expected_stubs" for target in node.targets)
        ]
        self.assertEqual(declarations, [expected_stubs], "Installed-wheel smoke inventory is stale")
        optional_exports = [
            ast.literal_eval(node.value)
            for node in smoke.body
            if isinstance(node, ast.Assign)
            and any(isinstance(target, ast.Name) and target.id == "OPTIONAL_NATIVE_EXPORTS"
                    for target in node.targets)
        ]
        self.assertEqual(len(optional_exports), 1)
        self.assertEqual(set(optional_exports[0]) | {"spatial"}, optional_facades,
                         "Installed-wheel smoke must cover every optional facade")

    def test_native_and_facade_stubs_are_required_in_wheel(self):
        for name in (
            "__init__.pyi",
            "_fluentqt.pyi",
            "basicinput.pyi",
            "collections.pyi",
            "design.pyi",
            "windowing.pyi",
        ):
            with self.subTest(name=name):
                self.assertIn(name, WHEEL_BUILDER.REQUIRED_PACKAGE_FILES)

    def test_scrolling_facade_is_required_in_wheel(self):
        self.assertIn(
            "scrolling.py",
            WHEEL_BUILDER.REQUIRED_PACKAGE_FILES,
        )

    def test_core_wheel_builder_rejects_missing_chart_files(self):
        for missing in ("charts.py", "charts.pyi"):
            with self.subTest(missing=missing), tempfile.TemporaryDirectory() as temporary:
                package_dir = Path(temporary)
                for name in WHEEL_BUILDER.REQUIRED_PACKAGE_FILES:
                    (package_dir / name).write_text("", encoding="utf-8")
                (package_dir / "_fluentqt.so").write_bytes(b"extension fixture")
                (package_dir / missing).unlink()
                with self.assertRaisesRegex(RuntimeError, "missing required files") as error:
                    WHEEL_BUILDER.package_files(package_dir)
                self.assertIn(missing, str(error.exception))

    def test_design_facade_and_semantic_alias_data_are_required_in_wheel(self):
        for name in ("design.py", "design.pyi", "_icon_aliases.json"):
            with self.subTest(name=name):
                self.assertIn(name, WHEEL_BUILDER.REQUIRED_PACKAGE_FILES)

    def test_python_gallery_is_excluded_from_core_wheel(self):
        self.assertFalse(
            any(
                name.startswith("gallery/")
                for name in WHEEL_BUILDER.REQUIRED_PACKAGE_FILES
            )
        )

    def test_core_wheel_builder_rejects_staged_gallery_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            package_dir = Path(temporary)
            gallery_file = package_dir / "gallery" / "__init__.py"
            gallery_file.parent.mkdir(parents=True)
            gallery_file.write_text("# must not ship\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "must not contain Gallery"):
                WHEEL_BUILDER.package_files(package_dir)

    def test_qt_62_uses_monolithic_pyside6_distribution(self):
        requirement = WHEEL_BUILDER.pyside_runtime_requirement("6.2.4")
        self.assertEqual(requirement, "PySide6 (==6.2.4)")

    def test_qt_63_and_newer_use_essentials_distribution(self):
        for version in ("6.3.0", "6.9.3", "7.0.0"):
            with self.subTest(version=version):
                requirement = WHEEL_BUILDER.pyside_runtime_requirement(version)
                self.assertEqual(
                    requirement,
                    "PySide6-Essentials (=={0})".format(version),
                )

    def test_invalid_version_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "Invalid PySide6 version"):
            WHEEL_BUILDER.pyside_runtime_requirement("invalid")

    def test_python_metadata_policies_are_explicit(self):
        for policy in (">=3.10,<3.11", ">=3.11,<3.14"):
            with self.subTest(policy=policy):
                self.assertEqual(
                    WHEEL_BUILDER.validate_requires_python(policy),
                    policy,
                )

        with self.assertRaisesRegex(RuntimeError, "Requires-Python policy"):
            WHEEL_BUILDER.validate_requires_python(">=3.10")

    def test_release_metadata_excludes_python_310(self):
        metadata = WHEEL_BUILDER.metadata_contents(
            "1.5.3",
            "6.9.3",
            "6.9.3",
            ">=3.11,<3.14",
            "# FluentQt\n\nNative PySide6 bindings for Fluent-Qt.",
            ("LICENSE", "THIRD_PARTY_NOTICES.md", "TRADEMARKS.md"),
        )

        self.assertIn("Requires-Python: >=3.11,<3.14", metadata)
        self.assertIn("Requires-Dist: PySide6-Essentials (==6.9.3)", metadata)
        self.assertIn("Requires-Dist: shiboken6 (==6.9.3)", metadata)

        parsed = Parser().parsestr(metadata)
        self.assertEqual(parsed["Metadata-Version"], "2.4")
        self.assertEqual(parsed["License-Expression"], "MIT")
        self.assertEqual(
            parsed["Description-Content-Type"],
            "text/markdown; charset=UTF-8; variant=GFM",
        )
        self.assertEqual(
            set(parsed.get_all("License-File", [])),
            {"LICENSE", "THIRD_PARTY_NOTICES.md", "TRADEMARKS.md"},
        )
        self.assertIn("# FluentQt", parsed.get_payload())
        self.assertIn(
            "https://github.com/calvinhxx/Fluent-Qt",
            "\n".join(parsed.get_all("Project-URL", [])),
        )

    def write_pyd(self, directory, dll_names):
        extension = Path(directory) / "_fluentqt.pyd"
        extension.write_bytes(pe_image(dll_names))
        return extension

    def test_windows_import_directory_is_read(self):
        with tempfile.TemporaryDirectory() as temporary:
            extension = self.write_pyd(
                temporary,
                ["Qt6Core.dll", "python3.dll", "shiboken6.abi3.dll"],
            )

            self.assertEqual(
                WHEEL_BUILDER.windows_imported_dlls(extension),
                ["Qt6Core.dll", "python3.dll", "shiboken6.abi3.dll"],
            )

    def test_stable_abi_windows_module_must_import_only_python3_dll(self):
        with tempfile.TemporaryDirectory() as temporary:
            WHEEL_BUILDER.verify_stable_abi_extension(
                self.write_pyd(temporary, ["Qt6Core.dll", "PYTHON3.DLL"])
            )
            for dll_names in (
                ["python311.dll"],
                ["python3.dll", "python313.dll"],
                ["python3_d.dll"],
                ["Qt6Core.dll"],
            ):
                with self.subTest(dll_names=dll_names):
                    with self.assertRaisesRegex(
                        RuntimeError, "must import only python3.dll"
                    ):
                        WHEEL_BUILDER.verify_stable_abi_extension(
                            self.write_pyd(temporary, dll_names)
                        )

    def test_malformed_windows_module_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            extension = Path(temporary) / "_fluentqt.pyd"
            extension.write_bytes(b"MZ" + b"\0" * 16)
            with self.assertRaisesRegex(RuntimeError, "Cannot read PE imports"):
                WHEEL_BUILDER.windows_imported_dlls(extension)

    def test_stable_abi_tag_keeps_build_interpreter_floor(self):
        with tempfile.TemporaryDirectory() as temporary:
            extension = Path(temporary) / "_fluentqt.so"
            extension.write_bytes(b"not a native image")
            python_tag = "cp{0}{1}".format(*sys.version_info[:2])

            stable = WHEEL_BUILDER.python_wheel_tag(extension, stable_abi=True)
            single = WHEEL_BUILDER.python_wheel_tag(extension)

        self.assertTrue(stable.startswith(python_tag + "-abi3-"), stable)
        self.assertTrue(
            single.startswith("{0}-{0}-".format(python_tag)),
            single,
        )

    def test_stable_abi_is_limited_to_release_policy(self):
        with tempfile.TemporaryDirectory() as temporary:
            args = argparse.Namespace(
                package_dir=temporary,
                output_dir=temporary,
                requires_python=">=3.10,<3.11",
                stable_abi=True,
            )
            with self.assertRaisesRegex(RuntimeError, "Stable-ABI wheels require"):
                WHEEL_BUILDER.build_wheel(args)

    def test_empty_pypi_description_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            description = Path(temporary) / "PYPI.md"
            description.write_text(" \n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "must not be empty"):
                WHEEL_BUILDER.read_markdown_description(description)


if __name__ == "__main__":
    unittest.main()
