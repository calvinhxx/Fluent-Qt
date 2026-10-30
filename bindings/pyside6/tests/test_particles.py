"""Particle module identity, lifecycle and Python ownership contracts."""
import gc
import unittest
from unittest.mock import patch

from PySide6.QtCore import QObject
from PySide6.QtTest import QSignalSpy
from PySide6.QtWidgets import QApplication, QWidget
from shiboken6 import isValid
import fluentqt
from fluentqt import layout
from fluentqt._qt_compat import delete_qobject
from fluentqt.particles import ParticleBackdrop, ParticlesPlugin, ParticlesSession


class ParticlesBindingTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        fluentqt.initialize_resources()
        fluentqt.set_motion_mode(fluentqt.MotionMode.Full)

    def test_particles_module_owns_the_backdrop_type(self):
        self.assertFalse(hasattr(fluentqt, "ParticleBackdrop"))
        self.assertFalse(hasattr(layout, "ParticleBackdrop"))
        self.assertEqual(ParticlesPlugin.pluginId(), "fluentqt.particles")
        for name in ("onActivate", "onDeactivate", "createWidget"):
            self.assertFalse(hasattr(ParticlesPlugin, name), name)

    def show_in(self, owner, backdrop):
        backdrop.setPauseWhenInactive(False)
        backdrop.setGeometry(0, 0, 200, 140)
        backdrop.show()

    def test_session_pauses_without_changing_preferences(self):
        owner = QWidget()
        owner.resize(240, 180)
        owner.show()
        session = ParticlesSession(owner)
        try:
            self.assertIsNone(session.createBackdrop())
            active = QSignalSpy(session.activeChanged)
            self.assertTrue(session.activate())
            self.assertTrue(session.activate())
            self.assertEqual(active.count(), 1)
            running = session.createBackdrop(owner)
            paused = ParticleBackdrop(owner)
            paused.setAnimationEnabled(False)
            self.show_in(owner, running)
            self.show_in(owner, paused)
            self.assertTrue(session.attachBackdrop(paused))
            self.assertTrue(session.attachBackdrop(paused))
            running.setEffect(ParticleBackdrop.Effect.Starfield)
            self.assertTrue(running.isAnimating())
            self.assertTrue(session.deactivate())
            self.assertFalse(running.isAnimating())
            self.assertTrue(running.isAnimationEnabled())
            self.assertIs(running.parentWidget(), owner)
            paused.setAnimationEnabled(True)
            self.assertFalse(paused.isAnimating())
            self.assertTrue(session.activate())
            self.assertTrue(running.isAnimating())
            self.assertTrue(paused.isAnimating())
            self.assertEqual(running.effect(), ParticleBackdrop.Effect.Starfield)
            self.assertTrue(session.deactivate())
            delete_qobject(session)
            self.assertTrue(isValid(running))
            self.assertTrue(running.isAnimating())
        finally:
            if isValid(session):
                delete_qobject(session)
            delete_qobject(owner)

    def test_unparented_factory_output_remains_python_owned(self):
        session = ParticlesSession()
        self.assertTrue(session.activate())
        backdrop = session.createBackdrop()
        destroyed = []
        backdrop.destroyed.connect(lambda: destroyed.append(True))
        del backdrop
        gc.collect()
        self.assertEqual(destroyed, [True])
        self.assertTrue(session.deactivate())
        delete_qobject(session)

    def test_parent_notification_can_stop_the_session_during_creation(self):
        class Host(QWidget):
            def childEvent(self, event):
                super().childEvent(event)
                session = getattr(self, "session", None)
                if event.added() and session is not None and isValid(session):
                    if self.destroy_session:
                        delete_qobject(session)
                    else:
                        session.deactivate()

        for destroy in (False, True):
            with self.subTest(destroy_session=destroy):
                host = Host()
                session = ParticlesSession(host)
                host.session, host.destroy_session = session, destroy
                try:
                    self.assertTrue(session.activate())
                    self.assertIsNone(session.createBackdrop(host))
                    self.assertEqual(host.children(), [])
                    if destroy:
                        self.assertFalse(isValid(session))
                    else:
                        self.assertFalse(session.isActive())
                finally:
                    if isValid(session):
                        delete_qobject(session)
                    delete_qobject(host)

    def test_session_factory_rejects_nested_creation_and_recovers_after_errors(self):
        class Host(QWidget):
            callback = None

            def childEvent(self, event):
                super().childEvent(event)
                if event.added() and self.callback is not None:
                    callback, self.callback = self.callback, None
                    callback()

        host, session, other = Host(), ParticlesSession(), ParticlesSession()
        nested, independent = [], []
        try:
            self.assertTrue(session.activate())
            self.assertTrue(other.activate())

            def reenter():
                nested.append(session.createBackdrop(host))
                independent.append(other.createBackdrop(host))

            host.callback = reenter
            backdrop = session.createBackdrop(host)
            self.assertIsInstance(backdrop, ParticleBackdrop)
            self.assertIs(backdrop.parentWidget(), host)
            self.assertEqual(nested, [None])
            self.assertIsInstance(independent[0], ParticleBackdrop)
            self.assertIsNotNone(session.createBackdrop(host))

            with patch("fluentqt.particles.ParticleBackdrop", side_effect=RuntimeError("creation failed")):
                with self.assertRaisesRegex(RuntimeError, "creation failed"):
                    session.createBackdrop(host)
            self.assertIsInstance(session.createBackdrop(host), ParticleBackdrop)
        finally:
            delete_qobject(session)
            delete_qobject(other)
            delete_qobject(host)

    def test_borrowed_host_can_disappear_before_explicit_cleanup(self):
        host = QObject()
        session = ParticlesSession(host)
        backdrop = ParticleBackdrop()
        self.assertTrue(session.activate())
        self.assertTrue(session.attachBackdrop(backdrop))
        delete_qobject(host)
        self.assertTrue(session.deactivate())
        self.assertFalse(session.isActive())
        self.assertTrue(backdrop.isAnimationEnabled())
        delete_qobject(session)
        delete_qobject(backdrop)


if __name__ == "__main__":
    unittest.main()
