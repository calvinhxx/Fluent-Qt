"""Private widget and generated-wrapper adaptations for supported PySide6 versions."""

from PySide6 import __version_info__ as _PYSIDE_VERSION_INFO
from PySide6.QtCore import Qt
from PySide6.QtGui import QAction, QGuiApplication, QIcon, QKeySequence
from PySide6.QtWidgets import QApplication
from shiboken6 import isValid
from ._qt_compat import delete_qobject

__all__ = ["application_is_saving_session", "enforce_final_type", "install_legacy_callable_add_action",
           "system_color_scheme", "create_owned_widget"]


def create_owned_widget(owner, factory, is_active, attach, parent=None, *, affinity):
    """Construct a Python-owned widget before attaching it to a borrowed scope.

    Legacy Shiboken retains native-created widget wrappers even after ownership
    transfer. Python construction avoids that reference without exposing a
    process-lifetime QThread wrapper or moving scope policy into this adapter.
    """
    def on_current_thread(obj):
        return obj is not None and isValid(obj) and affinity(obj)

    app = QApplication.instance()
    if (not isinstance(app, QApplication) or not on_current_thread(app)
            or not on_current_thread(owner)
            or (parent is not None and not on_current_thread(parent))):
        return None
    if (not is_active() or not on_current_thread(app) or not on_current_thread(owner)
            or (parent is not None and not on_current_thread(parent))):
        return None
    widget = factory(parent)
    if widget is None:
        return None

    def matches_request():
        return (on_current_thread(owner) and on_current_thread(widget)
                and (parent is None or on_current_thread(parent))
                and widget.parentWidget() is parent)

    retained = False
    try:
        # Application callbacks may change ownership during construction or attachment.
        retained = matches_request() and bool(attach(widget))
        retained = retained and matches_request()
        return widget if retained else None
    finally:
        if not retained and isValid(widget):
            delete_qobject(widget)


def system_color_scheme() -> str | None:
    """Return light/dark when the runtime exposes Qt's system color scheme."""
    scheme_type = getattr(Qt, "ColorScheme", None)
    reader = getattr(QGuiApplication.styleHints(), "colorScheme", None)
    if scheme_type is None or not callable(reader):
        return None
    scheme = reader()
    if scheme == scheme_type.Dark:
        return "dark"
    if scheme == scheme_type.Light:
        return "light"
    return None


def application_is_saving_session(app) -> bool:
    if app is None:
        return False
    is_saving_session = getattr(app, "isSavingSession", None)
    return bool(is_saving_session()) if callable(is_saving_session) else False


_PY_TPFLAGS_BASETYPE = 1 << 10


def enforce_final_type(native_type):
    """Normalize final Shiboken types carrying a legacy BASETYPE flag."""
    # Shiboken 6.2 marks every generated heap type as a Python base type,
    # including C++ final classes for which it emits no virtual wrapper shell.
    if native_type.__flags__ & _PY_TPFLAGS_BASETYPE:
        type_name = native_type.__name__

        def reject_subclass(_cls, **_kwargs):
            raise TypeError(
                "type '{0}' is final and cannot be subclassed".format(
                    type_name
                )
            )

        native_type.__init_subclass__ = classmethod(reject_subclass)
    return native_type


def install_legacy_callable_add_action(surface_type, version_info=None):
    """Restore callable QWidget.addAction overloads on Shiboken 6.2."""
    if version_info is None:
        version_info = _PYSIDE_VERSION_INFO
    if tuple(version_info[:2]) != (6, 2):
        return surface_type

    native_add_action = surface_type.addAction

    def add_action(self, *args):
        if args and callable(args[-1]):
            callback = args[-1]
            action_args = args[:-1]
            action = None
            shortcut = None
            if len(action_args) == 1 and isinstance(action_args[0], str):
                action = QAction(action_args[0], self)
            elif (
                len(action_args) == 2
                and isinstance(action_args[0], QIcon)
                and isinstance(action_args[1], str)
            ):
                action = QAction(action_args[0], action_args[1], self)
            elif len(action_args) == 2 and isinstance(action_args[0], str):
                action = QAction(action_args[0], self)
                shortcut = QKeySequence(action_args[1])
            elif (
                len(action_args) == 3
                and isinstance(action_args[0], QIcon)
                and isinstance(action_args[1], str)
            ):
                action = QAction(action_args[0], action_args[1], self)
                shortcut = QKeySequence(action_args[2])

            if action is not None:
                if shortcut is not None:
                    action.setShortcut(shortcut)
                # Keep the generated one-argument wrapper in the path so its
                # borrowed-reference synchronization still runs.
                native_add_action(self, action)
                action.triggered.connect(callback)
                return action

        return native_add_action(self, *args)

    add_action.__name__ = "addAction"
    add_action.__qualname__ = "{0}.addAction".format(surface_type.__name__)
    surface_type.addAction = add_action
    return surface_type
