"""Particle backgrounds and their plugin scope; requires FLUENT_QT_BUILD_PARTICLES.

Keep unparented backdrop wrappers alive while a session tracks them. Stopping
the session pauses animation without changing animationEnabled; reactivating or
deleting it lifts the pause. Widgets and child content remain caller-owned.
"""

from PySide6.QtWidgets import QWidget
from . import _fluentqt as _native
from ._qt_widgets import create_owned_widget

ParticleBackdrop = _native.fluent.ParticleBackdrop
ParticlesPlugin = _native.fluent.ParticlesPlugin


class ParticlesSession(_native.fluent.ParticlesSession):
    def createBackdrop(self, parent: QWidget | None = None) -> ParticleBackdrop | None:
        """Create a tracked backdrop with caller-owned Python lifetime."""
        # ChildAdded callbacks may request another backdrop before construction ends.
        if getattr(self, "_creating_backdrop", False):
            return None
        self._creating_backdrop = True
        try:
            return create_owned_widget(self, ParticleBackdrop, self.isActive, self.attachBackdrop, parent,
                                       affinity=_native.objectHasCurrentThreadAffinityForBinding)
        finally:
            self._creating_backdrop = False

__all__ = ["ParticleBackdrop", "ParticlesPlugin", "ParticlesSession"]
