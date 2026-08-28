#!/usr/bin/env python3

"""Tests for reader-facing documentation validation."""

from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from generate_navigation import generate as generate_navigation
from validate_documentation import validate


class DocumentationValidationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.project_root = Path(self.temporary_directory.name)
        (self.project_root / "docs/section").mkdir(parents=True)

        manifest = {
            "schema_version": 1,
            "home": "README.md",
            "summary": "SUMMARY.md",
            "sections": [
                {
                    "title": "Section",
                    "index": "section/README.md",
                    "groups": [
                        {
                            "title": "Guides",
                            "pages": ["section/guide.md", "../CONTRIBUTING.md"],
                        }
                    ],
                }
            ],
        }
        (self.project_root / "docs/navigation.json").write_text(
            json.dumps(manifest), encoding="utf-8"
        )
        self._write_document("docs/README.md", "Documentation")
        self._write_document("docs/section/README.md", "Section")
        self._write_document("docs/section/guide.md", "Guide")
        self._write_document("CONTRIBUTING.md", "Contributing")
        generate_navigation(self.project_root, check=False)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def _write_document(self, relative: str, title: str) -> None:
        path = self.project_root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            f"# {title}\n\n> **Status:** Current guide\n\nBody.\n",
            encoding="utf-8",
        )

    def _validate(self) -> list[str]:
        required_indexes = (
            "docs/README.md",
            "docs/SUMMARY.md",
            "docs/section/README.md",
        )
        with (
            mock.patch(
                "validate_documentation.subprocess.check_output",
                side_effect=FileNotFoundError,
            ),
            mock.patch(
                "validate_documentation.REQUIRED_INDEXES",
                required_indexes,
            ),
        ):
            return validate(self.project_root)

    def _remove_status(self, relative: str) -> None:
        path = self.project_root / relative
        text = path.read_text(encoding="utf-8")
        path.write_text(
            text.replace("> **Status:** Current guide\n\n", "", 1),
            encoding="utf-8",
        )

    def _replace_body(self, replacement: str) -> None:
        path = self.project_root / "docs/section/guide.md"
        text = path.read_text(encoding="utf-8")
        path.write_text(text.replace("Body.", replacement), encoding="utf-8")

    def test_complete_navigation_manifest_passes(self) -> None:
        self.assertEqual([], self._validate())

    def test_internal_reader_page_requires_status(self) -> None:
        self._remove_status("docs/section/guide.md")

        self.assertIn(
            "missing document status: docs/section/guide.md",
            self._validate(),
        )

    def test_external_reader_page_requires_status(self) -> None:
        self._remove_status("CONTRIBUTING.md")

        self.assertIn(
            "missing document status: CONTRIBUTING.md",
            self._validate(),
        )

    def test_cjk_paragraph_hard_wrap_is_rejected(self) -> None:
        self._replace_body("中文正文不应按列宽\n人为拆成两行。")

        self.assertTrue(
            any(
                error.startswith(
                    "hard-wrapped CJK prose: docs/section/guide.md:"
                )
                for error in self._validate()
            )
        )

    def test_cjk_list_continuation_hard_wrap_is_rejected(self) -> None:
        self._replace_body("- 中文列表项不应\n  人为拆成两行。")

        self.assertTrue(
            any(
                error.startswith(
                    "hard-wrapped CJK prose: docs/section/guide.md:"
                )
                for error in self._validate()
            )
        )

    def test_separate_cjk_list_items_are_valid(self) -> None:
        self._replace_body("- 第一项。\n- 第二项。")

        self.assertEqual([], self._validate())


if __name__ == "__main__":
    unittest.main()
