"""Keep Python-authored trailing widgets alive without changing native ownership."""

import weakref

from PySide6.QtWidgets import QWidget

from . import _fluentqt as _native

WidgetOwnership = _native.fluent.WidgetOwnership
__all__ = ["install_trailing_content_api"]


def install_trailing_content_api(native_type: type) -> None:
    native_set = native_type._setTrailingWidgetWithOwnership
    native_take = native_type.takeTrailingWidget
    native_release = native_type.releaseTrailingWidget

    def track(host):
        if getattr(host, "_fluentqt_trailing_tracking", False):
            return
        host._fluentqt_trailing_tracking = True
        reference = weakref.ref(host)

        def content_changed():
            current = reference()
            if current is not None:
                child = current.trailingWidget()
                previous = getattr(current, "_fluentqt_trailing_refs", (None, None))
                if child is not previous[0]:
                    current._fluentqt_trailing_refs = (child, None)

        host.trailingWidgetChanged.connect(content_changed)

    def setTrailingWidget(self, widget: QWidget | None, ownership: WidgetOwnership) -> bool:
        """Replace the suffix using Owned, Borrowed, or Reparented lifetime semantics."""
        if ownership not in (WidgetOwnership.Owned, WidgetOwnership.Borrowed, WidgetOwnership.Reparented):
            raise ValueError("Unknown trailing widget ownership")
        if widget is self or (widget is not None and widget.isAncestorOf(self)):
            return False
        if widget is not None and widget.thread() is not self.thread():
            return False
        if isinstance(self, _native.fluent.ComboBox) and widget is not None and widget is self.lineEdit():
            return False
        current = self.trailingWidget()
        if (widget is not None and current is not None
                and self.trailingWidgetOwnership() == WidgetOwnership.Owned
                and current.isAncestorOf(widget)):
            return False
        track(self)
        if widget is not None and widget is self.trailingWidget():
            return self.trailingWidgetOwnership() == ownership
        original_parent = None
        previous_parent = widget.parentWidget() if widget is not None else None
        if widget is not None:
            if ownership == WidgetOwnership.Reparented:
                original_parent = widget.parentWidget()
            elif widget.parent() is not None:
                widget.setParent(None)
        if not native_set(self, widget, ownership):
            if widget is not None and widget.parentWidget() is not previous_parent:
                widget.setParent(previous_parent)
            return False
        self._fluentqt_trailing_refs = (widget, original_parent)
        return True

    def takeTrailingWidget(self) -> QWidget | None:
        """Detach the custom widget and return ownership to Python."""
        child = native_take(self)
        if child is not None:
            child.setParent(None)
        self._fluentqt_trailing_refs = (None, None)
        return child

    def releaseTrailingWidget(self) -> None:
        """Apply the native content ownership policy."""
        native_release(self)
        self._fluentqt_trailing_refs = (None, None)

    native_type.setTrailingWidget = setTrailingWidget
    native_type.takeTrailingWidget = takeTrailingWidget
    native_type.releaseTrailingWidget = releaseTrailingWidget
