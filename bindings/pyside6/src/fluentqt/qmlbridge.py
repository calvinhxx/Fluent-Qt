"""Optional native QML scenes embedded in FluentQt Widgets.

Call ``QmlSession.prepareApplication()`` before creating the first Quick
surface, then activate a session and create or attach a view. Context objects
are borrowed: retain unparented Python models or give them a Qt owner.
Deactivation unloads scenes and preserves caller-owned views and models;
reactivation requires an explicit ``load()``.

``QmlTextureLayer`` lets an application OpenGL compositor render the same
scene into a GPU texture while retaining its host and model ownership.
It requires the compositor's GUI-thread context and does not provide
Direct3D/Metal texture import or Qt Quick 3D rendering.
"""

# Register the QObject/QQuickItem converters only for explicit bridge imports.
# The ordinary fluentqt entry point does not import Quick or create an engine.
from PySide6 import QtQml as _QtQml
from PySide6 import QtQuick as _QtQuick

from . import _fluentqt as _native

QmlPlugin = _native.fluent.QmlPlugin
QmlSession = _native.fluent.QmlSession
QmlTheme = _native.fluent.QmlTheme
QmlTextureLayer = _native.fluent.QmlTextureLayer
QmlView = _native.fluent.QmlView

__all__ = ["QmlPlugin", "QmlSession", "QmlTheme", "QmlTextureLayer", "QmlView"]
