#!/usr/bin/env python3

"""Tests for reader-facing documentation validation."""

from __future__ import annotations

import json
from pathlib import Path, PureWindowsPath
import tempfile
import unittest
from unittest import mock

from generate_navigation import generate as generate_navigation
from validate_documentation import (
    cjk_hard_wrap_lines,
    english_document_cjk_lines,
    relative_posix_path,
    validate,
)


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

    def _validate(self, git_output: str | None = None) -> list[str]:
        required_indexes = (
            "docs/README.md",
            "docs/SUMMARY.md",
            "docs/section/README.md",
        )
        with (
            mock.patch(
                "validate_documentation.subprocess.check_output",
                **(
                    {"return_value": git_output}
                    if git_output is not None
                    else {"side_effect": FileNotFoundError}
                ),
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

    def test_unstaged_deletion_with_updated_navigation_passes(self) -> None:
        files = [
            path.relative_to(self.project_root).as_posix()
            for path in self.project_root.rglob("*.md")
        ]
        (self.project_root / "docs/section/guide.md").unlink()
        manifest_path = self.project_root / "docs/navigation.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["sections"][0]["groups"][0]["pages"].remove("section/guide.md")
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        generate_navigation(self.project_root, check=False)

        self.assertEqual([], self._validate(git_output="\n".join(files)))

    def test_deleted_document_with_remaining_references_is_reported(self) -> None:
        files = [
            path.relative_to(self.project_root).as_posix()
            for path in self.project_root.rglob("*.md")
        ]
        (self.project_root / "docs/section/guide.md").unlink()

        errors = self._validate(git_output="\n".join(files))
        self.assertIn(
            "navigation.json points to a missing document: docs/section/guide.md",
            errors,
        )
        self.assertIn(
            "broken local link: docs/SUMMARY.md -> section/guide.md",
            errors,
        )

    def test_windows_paths_use_navigation_manifest_separators(self) -> None:
        project_root = PureWindowsPath("C:/workspace/FluentQt")
        document = project_root / "docs/section/guide.md"

        self.assertEqual(
            "docs/section/guide.md",
            relative_posix_path(document, project_root),
        )

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

    def test_separate_cjk_list_items_do_not_hard_wrap(self) -> None:
        self.assertEqual([], cjk_hard_wrap_lines("- 第一项。\n- 第二项。"))

    def test_chinese_prose_in_english_guide_reports_source_line(self) -> None:
        self._replace_body("中文说明。")
        relative = "docs/section/guide.md"
        lines = (self.project_root / relative).read_text(encoding="utf-8").splitlines()

        self.assertIn(
            f"CJK prose in English document: {relative}:{lines.index('中文说明。') + 1} "
            "(use English or a .zh-CN.md translation)",
            self._validate(),
        )

    def test_translation_and_its_generated_navigation_pass(self) -> None:
        self._write_document("docs/section/guide.zh-CN.md", "中文说明")
        manifest_path = self.project_root / "docs/navigation.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["sections"][0]["groups"][0]["pages"].append("section/guide.zh-CN.md")
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        generate_navigation(self.project_root, check=False)

        self.assertEqual([], self._validate())

    def test_generated_summary_still_rejects_manual_edits(self) -> None:
        path = self.project_root / "docs/SUMMARY.md"
        path.write_text(path.read_text(encoding="utf-8") + "\n中文说明。\n", encoding="utf-8")

        self.assertIn("stale documentation navigation: docs/SUMMARY.md", self._validate())


class EnglishDocumentLanguageTest(unittest.TestCase):
    def test_visible_chinese_text_is_checked(self) -> None:
        for text in (
            "中文说明。",
            "## 中文标题",
            "> 中文引用",
            "- 中文列表",
            "- English item\n\n    中文列表续行",
            "| Heading | 中文单元格 |",
            "[中文链接](https://example.com)",
            "[中文链接][reference]",
            "![中文替代文本](image.png)",
            '<span title="中文提示">English</span>',
            "<p>中文正文</p>",
            '[English](https://example.com "中文提示")',
        ):
            with self.subTest(text=text):
                self.assertTrue(english_document_cjk_lines(text, "docs/guide.md"))

    def test_only_root_readme_and_explicit_translation_are_exempt(self) -> None:
        for relative in ("README.md", "README.zh-CN.md", "docs/guide.zh-CN.md"):
            with self.subTest(relative=relative):
                self.assertEqual([], english_document_cjk_lines("中文正文", relative))
        for relative in ("docs/README.md", "docs/guide_CN.md", "docs/guide.zh-CN.notes.md"):
            with self.subTest(relative=relative):
                self.assertEqual([1], english_document_cjk_lines("中文正文", relative))

    def test_code_comments_and_link_destinations_are_exempt(self) -> None:
        text = "\n".join(
            (
                "Use `zh_CN: 中文` or `` `中文` `` in a code example.",
                "```cpp",
                '// zh_CN: 中文注释。',
                'const auto text = "中文示例";',
                "```",
                "<!-- 中文维护注释\ncontinued -->",
                "[English](<中文文件.md>)",
                "[English][中文引用标识]",
                "[中文引用标识]: 中文文件.md",
                "[English](https://example.com/中文)",
                '<a href="中文文件.md">English</a>',
                "https://example.com/中文",
                "中文正文。",
            )
        )

        self.assertEqual([len(text.splitlines())], english_document_cjk_lines(text, "docs/guide.md"))

    def test_fence_ends_only_with_matching_marker_and_length(self) -> None:
        for opening, shorter, wrong in (("````", "```", "~~~"), ("~~~~", "~~~", "```")):
            with self.subTest(opening=opening):
                text = f"{opening}text\n{shorter}\n{wrong}\n中文示例\n{opening}\n中文正文"
                self.assertEqual([6], english_document_cjk_lines(text, "docs/guide.md"))

    def test_generated_navigation_does_not_exempt_surrounding_prose(self) -> None:
        text = (
            "<!-- docs-nav:top:start -->\n[中文说明](guide.zh-CN.md)\n"
            "<!-- docs-nav:top:end -->\n中文正文。\n"
            "<!-- docs-nav:bottom:start -->\n[中文说明](guide.zh-CN.md)\n"
            "<!-- docs-nav:bottom:end -->"
        )

        self.assertEqual([4], english_document_cjk_lines(text, "docs/guide.md"))

    def test_glossary_exempts_only_its_translation_column(self) -> None:
        text = (
            "中文正文。\n\n## Glossary\n\n"
            "| English | Chinese | Notes |\n|---|---|---|\n"
            "| component | 组件 | Reusable widget. |\n"
            "| control | 控件 | 中文说明。 |\n"
            "| 中文术语 | 术语 | English note. |\n\n"
            "中文段落。\n\n## Review\n\n"
            "| English | Chinese | Notes |\n|---|---|---|\n"
            "| component | 组件 | Reusable widget. |"
        )

        self.assertEqual(
            [1, 8, 9, 11, 17],
            english_document_cjk_lines(text, "docs/development/comment-style.md"),
        )
        self.assertIn(7, english_document_cjk_lines(text, "docs/guide.md"))


if __name__ == "__main__":
    unittest.main()
