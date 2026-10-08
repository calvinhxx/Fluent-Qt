"""Optional native perspective hosts; built with FLUENT_QT_BUILD_SPATIAL=ON.

Rendering and pointer animation stay in C++. Keep widget wrappers alive while
hosted, and use explicit ownership methods when the view should own or restore
content. ``addWidget`` preserves C++'s borrowed default.
Use ``SpatialSession`` for a local extension scope or register ``SpatialPlugin``
with ``fluentqt.plugins.PluginManager``. Neither manager owns returned views.
"""

import weakref
from types import MethodType

from shiboken6 import isValid
from PySide6.QtWidgets import QWidget
from . import _fluentqt as _native
from ._qt_compat import delete_qobject

SpatialItem = _native.fluent.SpatialItem
SpatialRuntime = _native.fluent.SpatialRuntime
ParticleLayer = _native.fluent.ParticleLayer
SpatialPlugin = _native.fluent.SpatialPlugin
_Ownership = _native.fluent.WidgetOwnership


class SpatialView(_native.fluent.SpatialView):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._spatial_records = {}

    def _add_widget(self, widget, ownership):
        if widget is None:
            return None
        if widget is self or widget.isAncestorOf(self):
            raise ValueError("Spatial content cannot be its host or an ancestor")
        for item, record in self._spatial_records.values():
            if record[0] is widget:
                return item
        original = widget.parentWidget()
        restore = original if ownership == _Ownership.Reparented else None
        item = _native.fluent.SpatialView._addWidgetWithOwnership(self, widget, ownership)
        if item is None:
            return None
        # Mutable restore target lets takeWidget override Reparented semantics.
        record = [widget, restore]
        key = id(item)
        owner = weakref.ref(self)

        def released(*_args):
            if isValid(widget):
                parent = record[1]
                widget.setParent(parent if parent is not None and isValid(parent) else None)
            host = owner()
            if host is not None:
                host._spatial_records.pop(key, None)

        item.destroyed.connect(released)
        self._spatial_records[key] = (item, record)
        return item

    def addWidget(self, widget):
        """Borrow content; releasing it leaves it parentless and alive."""
        return self.addBorrowedWidget(widget)

    def addBorrowedWidget(self, widget):
        return self._add_widget(widget, _Ownership.Borrowed)

    def addOwnedWidget(self, widget):
        """Transfer content to the view; releasing its item destroys it."""
        return self._add_widget(widget, _Ownership.Owned)

    def addReparentedWidget(self, widget):
        """Restore content to its original QWidget parent on release."""
        return self._add_widget(widget, _Ownership.Reparented)

    def takeWidget(self, item):
        stored = self._spatial_records.get(id(item))
        if stored is not None:
            stored[1][1] = None
        widget = _native.fluent.SpatialView.takeWidget(self, item)
        if widget is not None:
            widget.setParent(None)
        return widget


def _prepare_spatial_view(view):
    """Add the same ownership helpers to a C++ factory-created native view."""
    if not hasattr(view, "_spatial_records"):
        view._spatial_records = {}
        for name in ("_add_widget", "addWidget", "addBorrowedWidget", "addOwnedWidget",
                     "addReparentedWidget", "takeWidget"):
            setattr(view, name, MethodType(getattr(SpatialView, name), view))
    return view


class SpatialSession(_native.fluent.SpatialSession):
    def createView(self, parent: QWidget | None = None) -> SpatialView | None:
        """Create a tracked view with Python widget ownership bookkeeping."""
        if not self.isActive():
            return None
        view = SpatialView(parent)
        if not isValid(self) or not isValid(view) or not self.attachView(view):
            if isValid(view):
                delete_qobject(view)
            return None
        return view


__all__ = ["SpatialPlugin", "SpatialSession", "SpatialView", "SpatialItem", "ParticleLayer", "SpatialRuntime"]
