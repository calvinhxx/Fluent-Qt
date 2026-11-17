from pathlib import Path
import json
import tempfile
import unittest

from check_gallery_language import CJK, cpp_literals, python_literals, validate


class GalleryLanguageTests(unittest.TestCase):
    def playground_errors(self, data, name="app/assets/playgrounds/button.json"):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / name
            path.parent.mkdir(parents=True)
            path.write_text(json.dumps(data), encoding="utf-8")
            return validate(root)

    def test_website_playground_labels_allow_explicit_chinese_translations(self):
        data = {"properties": [{"label": {"en": "Text", "zh": "文字"},
                                "description": {"en": "Action label", "zh": "操作文字"},
                                "default": "Continue"}],
                "presets": [{"label": {"en": "Primary", "zh": "主要操作"},
                             "values": {"text": "Save"}}]}
        self.assertEqual(self.playground_errors(data), [])
        self.assertTrue(self.playground_errors(data, "app/data/examples.json"))

    def test_playground_defaults_and_api_names_remain_english(self):
        for key in ("default", "property", "setter"):
            with self.subTest(key=key):
                self.assertTrue(self.playground_errors(
                    {"properties": [{"label": {"en": "Text", "zh": "文字"}, key: "中文"}]}))
        self.assertTrue(self.playground_errors(
            {"presets": [{"label": {"en": "Primary", "zh": "主要操作"}, "values": {"text": "保存"}}]}))
        self.assertTrue(self.playground_errors(
            {"properties": [{"label": {"en": "中文", "zh": "文字"}}]}))
        self.assertTrue(self.playground_errors({"unrelated": {"label": {"en": "Text", "zh": "文字"}}}))

    def test_category_labels_are_localized_in_descriptors_and_recipe_lists(self):
        data = {"categories": [{"id": "basic", "label": {"en": "Basic", "zh": "基础属性"}}]}
        self.assertEqual(self.playground_errors(data), [])
        self.assertEqual(self.playground_errors([data], "app/assets/playgrounds/components.json"), [])
        self.assertTrue(self.playground_errors({"categories": [{"id": "中文"}]}))
        for label in ({"zh": "基础属性"}, {"en": "基础", "zh": "基础属性"},
                      {"en": "Basic", "zh": "基础属性", "code": "隐藏代码"}):
            self.assertTrue(self.playground_errors({"categories": [{"label": label}]}))

    def test_effect_explanations_use_the_same_translation_contract(self):
        for key in ('effectHint', 'disabledHint'):
            data = {'properties': [{key: {'en': 'Shown when empty', 'zh': '为空时显示'}}]}
            self.assertEqual(self.playground_errors(data), [])
            self.assertTrue(self.playground_errors({'properties': [{key: {'en': '中文', 'zh': '中文'}}]}))

    def test_table_column_labels_are_metadata_but_cell_defaults_are_checked(self):
        column = {'type': 'text', 'label': {'en': 'Name', 'zh': '名称'}}
        data = {'properties': [{'columns': [column]}]}
        self.assertEqual(self.playground_errors(data), [])
        self.assertEqual(self.playground_errors([data], 'app/assets/playgrounds/components.json'), [])
        column['default'] = '中文'
        self.assertTrue(self.playground_errors(data))
        del column['default']
        column['label']['en'] = '名称'
        self.assertTrue(self.playground_errors(data))

    def test_sentinel_value_labels_are_localized_without_hiding_defaults(self):
        for key in ('valueLabels', 'optionLabels'):
            data = {'properties': [{key: {'0': {'en': 'Automatic', 'zh': '自动'}}}]}
            self.assertEqual(self.playground_errors(data), [])
            self.assertEqual(self.playground_errors([data], 'app/assets/playgrounds/components.json'), [])
        data = {'properties': [{'optionLabels': {'0': {'en': 'Automatic', 'zh': '自动'}}, 'default': '中文'}]}
        self.assertTrue(self.playground_errors(data))

    def test_playground_translation_shape_cannot_hide_arbitrary_content(self):
        for label in ({"zh": "文字"}, {"en": "", "zh": "文字"},
                      {"en": "Text", "zh": {"code": "中文"}},
                      {"en": "Text", "zh": "文字", "code": "中文"}):
            with self.subTest(label=label):
                self.assertTrue(self.playground_errors({"properties": [{"label": label}]}))

    def test_optional_calendar_locale_format_is_data_not_copy(self):
        prop = {"id": "displayFormat", "property": "displayFormat", "setter": "setDisplayFormat",
                "options": ["yyyy-MM-dd", "yyyy年M月d日"],
                "optionLabels": {"yyyy年M月d日": {"en": "Chinese date", "zh": "中文日期"}}}
        data = {"component": "calendar-date-picker", "properties": [prop]}
        self.assertEqual(self.playground_errors(data), [])
        self.assertEqual(self.playground_errors([data], "app/assets/playgrounds/components.json"), [])
        prop["default"] = "yyyy年M月d日"
        self.assertTrue(self.playground_errors(data))
        del prop["default"]
        prop["optionLabels"]["yyyy年M月d日"]["en"] = "中文日期"
        self.assertTrue(self.playground_errors(data))
        prop["optionLabels"]["yyyy年M月d日"]["en"] = "Chinese date"
        prop["options"].append("中文文案")
        self.assertTrue(self.playground_errors(data))
        prop["options"].pop()
        data["component"] = "button"
        self.assertTrue(self.playground_errors(data))
        data["component"] = "calendar-date-picker"
        prop["property"] = "text"
        self.assertTrue(self.playground_errors(data))
        prop["property"] = "displayFormat"
        data["recipe"] = {"cpp": {"setup": 'control->setText("中文");'}}
        self.assertTrue(self.playground_errors(data))

    def test_cpp_comments_are_allowed_but_raw_and_escaped_literals_are_checked(self):
        source = '// 中文注释\n/* 说明 */ auto a = "English";\nauto b = u8R"tag(团队)tag";\nauto c = "\\u4e2d";'
        self.assertEqual([line for line, value in cpp_literals(source) if CJK.search(value)], [3, 4])

    def test_python_source_comments_are_allowed_but_displayed_snippets_are_checked(self):
        source = '''"""中文模块说明"""
# 中文注释
snippet = """
    # 示例注释
    label.setText("团队")
"""
label = f"中文 {count}"
'''
        values = [value for _, value in python_literals(source) if CJK.search(value)]
        self.assertEqual(len(values), 2)
        self.assertIn('label.setText("团队")', values[0])
        self.assertEqual(values[1], "中文 ")

    def test_scan_includes_generated_and_python_copy_but_not_library_or_tests(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            files = {
                "app/sample.cpp": 'auto text = "团队";',
                "app/data/snippets.json": '{"code": "label.setText(\\\"\\u4e2d\\\")"}',
                "bindings/pyside6/gallery/src/sample.py": 'snippet = r\'label.setText("\\u6210\\u5458")\'',
                "src/components/Test.h": 'auto text = "中文";',
                "tests/components/test.py": 'query = "中文"',
            }
            for name, content in files.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content, encoding="utf-8")
            self.assertEqual(len(validate(root)), 3)

    def test_latin_names_symbols_and_user_input_handlers_remain_valid(self):
        source = 'label = "María García · Design"\ntext = editor.text()\n'
        self.assertFalse(any(CJK.search(value) for _, value in python_literals(source)))

    def test_plain_multiline_copy_is_not_mistaken_for_a_code_identifier(self):
        self.assertTrue(any(CJK.search(value) for _, value in python_literals('label = "中文\\n"')))


if __name__ == "__main__":
    unittest.main()
