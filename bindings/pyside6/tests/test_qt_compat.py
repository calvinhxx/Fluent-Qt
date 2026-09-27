"""QObject destruction must preserve the application and unrelated events."""
import unittest

from PySide6.QtCore import QObject, QTimer
from PySide6.QtTest import QSignalSpy
from PySide6.QtWidgets import QApplication, QWidget
from shiboken6 import isValid

from fluentqt._qt_compat import delete_qobject


class QtLifetimeCompatTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def test_pool_deletion_never_destroys_application(self):
        class Child(QWidget):
            pass

        # Pool allocation plus CTest's malloc perturbation exposes the old
        # uninitialized Shiboken singleton flag; no private layout assumptions.
        objects = [kind() for _ in range(100) for kind in (QObject, QWidget, Child)]
        for obj in objects:
            delete_qobject(obj)
            self.assertFalse(isValid(obj))
            self.assertTrue(isValid(self.app))
            self.assertIs(QApplication.instance(), self.app)

    def test_parent_child_and_repeated_deletion(self):
        parent = QWidget()
        child = QObject(parent)
        spy = QSignalSpy(child.destroyed)
        delete_qobject(parent)
        self.assertFalse(isValid(parent))
        self.assertFalse(isValid(child))
        self.assertEqual(spy.count(), 1)
        delete_qobject(parent)
        delete_qobject(child)
        delete_qobject(None)
        self.assertEqual(spy.count(), 1)

    def test_delivery_does_not_flush_other_deletions_or_timers(self):
        target, unrelated = QObject(), QObject()
        timer = QTimer()
        timer.setSingleShot(True)
        fired = QSignalSpy(timer.timeout)
        timer.start(0)
        unrelated.deleteLater()
        try:
            delete_qobject(target)
            self.assertFalse(isValid(target))
            self.assertTrue(isValid(unrelated))
            self.assertEqual(fired.count(), 0)
        finally:
            timer.stop()
            delete_qobject(unrelated)

    def test_application_and_non_qobjects_are_rejected(self):
        with self.assertRaises(ValueError):
            delete_qobject(self.app)
        with self.assertRaises(TypeError):
            delete_qobject(object())
        self.assertTrue(isValid(self.app))

    def test_destruction_from_timer_callback_completes_before_return(self):
        target, timer = QObject(), QTimer()
        timer.setSingleShot(True)
        result = []

        def destroy_target():
            delete_qobject(target)
            result.append(isValid(target))

        timer.timeout.connect(destroy_target)
        timer.start(0)
        self.app.processEvents()
        self.assertEqual(result, [False])
        self.assertTrue(isValid(self.app))


if __name__ == "__main__":
    unittest.main()
