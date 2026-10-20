"""Private Qt lifetime adapters shared by bindings and Gallery."""

from typing import Callable

from PySide6.QtCore import QCoreApplication, QEvent, QMetaObject, QObject, Qt, QTimer
from shiboken6 import isValid

__all__ = ["delete_qobject", "single_shot"]


def single_shot(delay_ms: int, context: QObject, callback: Callable[[], None]) -> QTimer:
    """Schedule a context-owned callback on every supported PySide6."""
    timer = QTimer(context)
    timer.setSingleShot(True)
    timer.timeout.connect(callback)
    timer.timeout.connect(timer.deleteLater)
    timer.start(max(0, int(delay_ms)))
    return timer


def delete_qobject(obj: QObject | None) -> None:
    """Destroy an owned QObject through Qt, without Shiboken's application hook.

    Shiboken 6.2.4 can leave an ordinary wrapper's application-singleton bit
    uninitialized. Its explicit delete() can then destroy QApplication instead
    of the requested object. Use Qt's targeted DeferredDelete delivery on every
    runtime; do not patch Shiboken, inspect its private layout, or flush unrelated
    events. Call from the object's owning thread, outside its own event handler.
    Application shutdown is deliberately not part of this helper's contract.
    """
    if obj is None:
        return
    if not isinstance(obj, QObject):
        raise TypeError("delete_qobject requires a QObject")
    if not isValid(obj):
        return
    if obj is QCoreApplication.instance():
        raise ValueError("delete_qobject must not destroy the application")
    # Do not materialize Qt's process-lifetime main-thread wrapper merely to
    # validate this precondition: PySide 6.2 can finalize it after Shiboken's
    # wrapper registry. These internal callers already run on the owning thread.
    # Calling the Python deleteLater wrapper invalidates native-created objects
    # before their event is delivered. Invoke Qt's slot without that wrapper step
    # so the targeted sendPostedEvents call can still use this receiver.
    if not QMetaObject.invokeMethod(obj, "deleteLater", Qt.DirectConnection):
        raise RuntimeError("Could not schedule QObject deletion")
    QCoreApplication.sendPostedEvents(obj, QEvent.DeferredDelete)
