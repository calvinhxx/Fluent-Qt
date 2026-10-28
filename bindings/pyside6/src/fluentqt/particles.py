"""Particle backgrounds and their plugin scope; requires FLUENT_QT_BUILD_PARTICLES.

Keep unparented backdrop wrappers alive while a session tracks them. Stopping
the session pauses animation without changing animationEnabled; reactivating or
deleting it lifts the pause. Widgets and child content remain caller-owned.
"""

from . import _fluentqt as _native

ParticleBackdrop = _native.fluent.ParticleBackdrop
ParticlesPlugin = _native.fluent.ParticlesPlugin
ParticlesSession = _native.fluent.ParticlesSession

__all__ = ["ParticleBackdrop", "ParticlesPlugin", "ParticlesSession"]
