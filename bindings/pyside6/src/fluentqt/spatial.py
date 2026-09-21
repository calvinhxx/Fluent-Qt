"""Optional native perspective hosts; built with FLUENT_QT_BUILD_SPATIAL=ON.

Rendering and pointer animation stay in C++. Keep widget wrappers alive while
hosted, and use explicit ownership methods when the view should own or restore
content. ``addWidget`` preserves C++'s borrowed default.
"""

import weakref

from shiboken6 import isValid
from . import _fluentqt as _native

SpatialItem = _native.fluent.SpatialItem
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
        if original is not None and ownership != _Ownership.Reparented:
            widget.setParent(None)
        try:
            item = super()._addWidgetWithOwnership(widget, ownership)
        except Exception:
            widget.setParent(original)
            raise
        if item is None:
            widget.setParent(original)
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
        widget = super().takeWidget(item)
        if widget is not None:
            widget.setParent(None)
        return widget


__all__ = ["SpatialView", "SpatialItem"]
