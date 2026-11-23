from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compile_ui import portable_string_lists


class FormCompilerTest(unittest.TestCase):
    def test_string_lists_preserve_translations_literals_and_unrelated_shifts(self):
        legacy = ('labels = (QStringList()\n    << tr("数量")\n    << "QStringList()")\n'
                  'empty = QStringList()\nshift = 2 << 3\n').encode("utf-8")
        result = portable_string_lists(legacy)
        self.assertEqual(result, ('labels = ([tr("数量"), "QStringList()"])\n'
                                  'empty = []\nshift = 2 << 3\n').encode("utf-8"))
        self.assertEqual(portable_string_lists(result), result)
