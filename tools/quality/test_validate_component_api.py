#!/usr/bin/env python3

"""Regression tests for component API validation."""

from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from validate_component_api import validate


class ComponentApiValidationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.project_root = Path(self.temporary_directory.name)
        self.header = self.project_root / "src/components/basicinput/Widget.h"
        self.header.parent.mkdir(parents=True)
        (self.project_root / "include/FluentQt").mkdir(parents=True)
        (self.project_root / "include/FluentQt/BasicInput.h").write_text(
            "#pragma once\n", encoding="utf-8"
        )
        test_source = self.project_root / "tests/components/basicinput/TestWidget.cpp"
        test_source.parent.mkdir(parents=True)
        test_source.write_text("// focused test\n", encoding="utf-8")
        cmake_path = self.project_root / "cmake/FluentQtInstallHeaders.cmake"
        cmake_path.parent.mkdir(parents=True)
        cmake_path.write_text(
            "set(FLUENT_QT_INSTALL_HEADERS\n"
            "    include/FluentQt/BasicInput.h\n"
            "    src/components/basicinput/Widget.h\n"
            ")\n",
            encoding="utf-8",
        )
        catalog_path = self.project_root / "site/api/catalog.json"
        catalog_path.parent.mkdir(parents=True)
        catalog_path.write_text(
            json.dumps(
                {
                    "components": [
                        {
                            "id": "widget",
                            "cpp": {
                                "public_header": "<FluentQt/BasicInput.h>",
                                "installed_declaration_header": (
                                    "<FluentQt/components/basicinput/Widget.h>"
                                ),
                                "qualified_type": "fluent::basicinput::Widget",
                            },
                            "tests": [
                                {
                                    "target": "test_widget",
                                    "ctest_label": "test_widget",
                                    "source_url": (
                                        "https://github.com/example/repo/blob/main/"
                                        "tests/components/basicinput/TestWidget.cpp"
                                    ),
                                }
                            ],
                        }
                    ]
                }
            ),
            encoding="utf-8",
        )
        self.policy_path = (
            self.project_root / "docs/development/component-api-policy.json"
        )
        self.policy_path.parent.mkdir(parents=True)
        self._write_policy()
        self._write_header(
            "Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)",
            "int value() const;\nvoid setValue(int);\nvoid valueChanged();",
        )

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def _write_header(self, property_line: str, declarations: str) -> None:
        self.header.write_text(
            "#pragma once\n"
            "class Widget {\n"
            "    Q_OBJECT\n"
            f"    {property_line}\n"
            "public:\n"
            f"    {declarations}\n"
            "};\n",
            encoding="utf-8",
        )

    def _write_policy(
        self,
        write_without_notify: list[str] | None = None,
        noun_boolean_reader: list[str] | None = None,
    ) -> None:
        self.policy_path.write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "source_catalog": "site/api/catalog.json",
                    "installed_headers": "cmake/FluentQtInstallHeaders.cmake",
                    "exception_categories": {
                        "animation-channel": "Animation-only property.",
                        "legacy-compatibility": "Existing 1.x surface.",
                    },
                    "exceptions": {
                        "write_without_notify": {
                            "animation-channel": sorted(write_without_notify or []),
                            "legacy-compatibility": [],
                        },
                        "noun_boolean_reader": {
                            "legacy-compatibility": sorted(noun_boolean_reader or [])
                        },
                    },
                }
            ),
            encoding="utf-8",
        )

    def _errors(self) -> list[str]:
        return validate(self.project_root)[1]

    def test_complete_contract_passes(self) -> None:
        self.assertEqual([], self._errors())

    def test_new_write_without_notify_requires_classification(self) -> None:
        self._write_header(
            "Q_PROPERTY(qreal progress READ progress WRITE setProgress)",
            "qreal progress() const;\nvoid setProgress(qreal);",
        )
        self.assertTrue(
            any("unclassified write_without_notify" in error for error in self._errors())
        )

    def test_exact_write_without_notify_classification_passes(self) -> None:
        key = "src/components/basicinput/Widget.h#Widget.progress"
        self._write_header(
            "Q_PROPERTY(qreal progress READ progress WRITE setProgress)",
            "qreal progress() const;\nvoid setProgress(qreal);",
        )
        self._write_policy(write_without_notify=[key])
        self.assertEqual([], self._errors())

    def test_stale_exception_is_rejected(self) -> None:
        self._write_policy(
            write_without_notify=[
                "src/components/basicinput/Widget.h#Widget.removedProgress"
            ]
        )
        self.assertTrue(
            any("stale write_without_notify" in error for error in self._errors())
        )

    def test_noun_boolean_reader_requires_classification(self) -> None:
        self._write_header(
            "Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)",
            "bool enabled() const;\nvoid setEnabled(bool);\nvoid enabledChanged();",
        )
        self.assertTrue(
            any("unclassified noun_boolean_reader" in error for error in self._errors())
        )

    def test_standard_boolean_reader_passes_without_exception(self) -> None:
        self._write_header(
            "Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)",
            "bool isEnabled() const;\nvoid setEnabled(bool);\nvoid enabledChanged();",
        )
        self.assertEqual([], self._errors())

    def test_missing_property_callable_is_rejected(self) -> None:
        self._write_header(
            "Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)",
            "int value() const;\nvoid valueChanged();",
        )
        self.assertTrue(
            any("WRITE callable is not declared" in error for error in self._errors())
        )

    def test_inherited_notify_callable_is_accepted(self) -> None:
        self.header.write_text(
            "#pragma once\n"
            "class BaseWidget {\n"
            "public:\n"
            "    void valueChanged();\n"
            "};\n"
            "class Widget : public BaseWidget {\n"
            "    Q_OBJECT\n"
            "    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)\n"
            "public:\n"
            "    int value() const;\n"
            "    void setValue(int);\n"
            "};\n",
            encoding="utf-8",
        )
        self.assertEqual([], self._errors())

    def test_catalog_header_must_be_installed(self) -> None:
        cmake_path = self.project_root / "cmake/FluentQtInstallHeaders.cmake"
        cmake_path.write_text(
            "set(FLUENT_QT_INSTALL_HEADERS\n"
            "    include/FluentQt/BasicInput.h\n"
            ")\n",
            encoding="utf-8",
        )
        self.assertTrue(
            any("declaration header is not installed" in error for error in self._errors())
        )

    def test_catalog_ctest_label_must_match_target(self) -> None:
        catalog_path = self.project_root / "site/api/catalog.json"
        catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
        catalog["components"][0]["tests"][0]["ctest_label"] = "test_other"
        catalog_path.write_text(json.dumps(catalog), encoding="utf-8")
        self.assertTrue(
            any("ctest_label must match" in error for error in self._errors())
        )

    def test_exception_category_requires_description(self) -> None:
        policy = json.loads(self.policy_path.read_text(encoding="utf-8"))
        policy["exception_categories"]["legacy-compatibility"] = ""
        self.policy_path.write_text(json.dumps(policy), encoding="utf-8")
        self.assertTrue(
            any("requires a description" in error for error in self._errors())
        )

    def test_policy_requires_complete_exception_groups(self) -> None:
        policy = json.loads(self.policy_path.read_text(encoding="utf-8"))
        del policy["exceptions"]["noun_boolean_reader"]
        self.policy_path.write_text(json.dumps(policy), encoding="utf-8")
        self.assertTrue(any("missing groups" in error for error in self._errors()))

    def test_private_installed_header_is_rejected(self) -> None:
        private_header = (
            self.project_root / "src/components/basicinput/private/Widget_p.h"
        )
        private_header.parent.mkdir(parents=True)
        private_header.write_text("#pragma once\n", encoding="utf-8")
        cmake_path = self.project_root / "cmake/FluentQtInstallHeaders.cmake"
        cmake_path.write_text(
            "set(FLUENT_QT_INSTALL_HEADERS\n"
            "    include/FluentQt/BasicInput.h\n"
            "    src/components/basicinput/Widget.h\n"
            "    src/components/basicinput/private/Widget_p.h\n"
            ")\n",
            encoding="utf-8",
        )
        self.assertTrue(
            any("private component header is installed" in error for error in self._errors())
        )


if __name__ == "__main__":
    unittest.main()
