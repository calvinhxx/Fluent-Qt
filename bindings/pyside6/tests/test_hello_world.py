"""Behavior and layout of the minimal example shared with C++ and WebAssembly."""

import importlib.util
from pathlib import Path
import unittest

import fluentqt
from fluentqt._qt_compat import delete_qobject
from fluentqt.basicinput import Button
from PySide6.QtCore import QSize, Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication


EXAMPLE = Path(__file__).resolve().parents[1] / "examples/hello_world/main.py"
spec = importlib.util.spec_from_file_location("hello_world_example", EXAMPLE)
example = importlib.util.module_from_spec(spec)
spec.loader.exec_module(example)


class HelloWorldTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fluentqt.prepare_high_dpi_application()
        cls.app = QApplication.instance() or QApplication([])
        fluentqt.initialize_resources()
        cls.app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    def setUp(self):
        self.window = example.create_window()
        self.window.show()
        self.app.processEvents()
        self.button = self.window.findChild(Button)
        self.assertIsNotNone(self.button)

    def tearDown(self):
        self.window.close()
        delete_qobject(self.window)
        self.app.processEvents()

    def test_mouse_and_keyboard_change_the_greeting(self):
        self.assertEqual(self.button.text(), "Hello from FluentQt")
        QTest.mouseClick(self.button, Qt.LeftButton)
        self.assertEqual(self.button.text(), "Hello, you!")

        self.button.setText("Hello from FluentQt")
        self.button.setFocus()
        QTest.keyClick(self.button, Qt.Key_Space)
        self.assertEqual(self.button.text(), "Hello, you!")

    def test_native_size_and_centering_survive_resize_and_click(self):
        self.assertEqual(self.window.size(), QSize(720, 520))
        self.assertEqual(self.button.fluentStyle(), Button.ButtonStyle.Accent)
        for size in (QSize(720, 520), QSize(480, 520), QSize(960, 640)):
            with self.subTest(size=size):
                self.window.resize(size)
                for text in ("Hello from FluentQt", "Hello, you!"):
                    self.button.setText(text)
                    self.app.processEvents()
                    content = self.button.parentWidget()
                    offset = self.button.geometry().center() - content.rect().center()
                    self.assertLessEqual(abs(offset.x()), 1)
                    self.assertLessEqual(abs(offset.y()), 1)
                    self.assertTrue(content.rect().contains(self.button.geometry()))


if __name__ == "__main__":
    unittest.main()
