"""Scrolling components with explicit Python ownership contracts."""

from . import _fluentqt as _native


AnnotatedScrollBar = _native.fluent.AnnotatedScrollBar
AnnotatedScrollBarLabel = _native.fluent.AnnotatedScrollBarLabel
PipsPager = _native.fluent.PipsPager
ScrollBar = _native.fluent.ScrollBar
WidgetOwnership = _native.fluent.WidgetOwnership
_NativeScrollView = _native.fluent.ScrollView
_CONTENT_UNSET = object()


def _annotated_scroll_bar_label_key(label):
    return (label.text, label.offset, label.detailText)


def _annotated_scroll_bar_label_eq(self, other):
    if not isinstance(other, AnnotatedScrollBarLabel):
        return NotImplemented
    return (
        _annotated_scroll_bar_label_key(self)
        == _annotated_scroll_bar_label_key(other)
    )


def _annotated_scroll_bar_label_ne(self, other):
    result = _annotated_scroll_bar_label_eq(self, other)
    if result is NotImplemented:
        return NotImplemented
    return not result


# Shiboken 6.2 does not convert AnnotatedScrollBarLabel's namespace-level C++
# comparison operators into Python rich comparison, while newer generators do.
# Normalize the public package contract for native values returned by labels()
# as well as values constructed in Python. The fields are mutable, so hashing
# would be unsafe once value equality is enabled.
AnnotatedScrollBarLabel.__eq__ = _annotated_scroll_bar_label_eq
AnnotatedScrollBarLabel.__ne__ = _annotated_scroll_bar_label_ne
AnnotatedScrollBarLabel.__hash__ = None


class ScrollView(_NativeScrollView):
    """Scrollable host with explicit Python ownership methods.

    ``setContentWidget()`` and ``setWidget()`` transfer the child to the
    ScrollView. Replacing it, passing ``None``, or destroying the host deletes
    that owned child. ``takeContentWidget()`` and ``takeWidget()`` return a
    parentless object whose ownership is transferred back to Python.

    ``setBorrowedContentWidget()`` detaches the child when it leaves the host,
    while ``setReparentedContentWidget()`` restores the QWidget parent that was
    present when the child was installed.
    """

    _fluentqt_hosted_content = None
    _fluentqt_original_parent = None

    def __init__(self, *args, **kwargs):
        content = kwargs.pop("contentWidget", _CONTENT_UNSET)
        super().__init__(*args, **kwargs)
        self._fluentqt_hosted_content = None
        self._fluentqt_original_parent = None
        if content is not _CONTENT_UNSET:
            self.setContentWidget(content)

    def _set_content_widget_with_ownership(self, widget, ownership):
        effective_ownership = (
            WidgetOwnership.Owned if widget is None else ownership
        )
        current = super().contentWidget()
        if widget is current:
            if widget is None:
                self._fluentqt_hosted_content = None
                self._fluentqt_original_parent = None
                return
            if super().contentOwnership() != effective_ownership:
                raise ValueError(
                    "takeContentWidget() before changing ownership mode"
                )
            self._fluentqt_hosted_content = widget
            return

        original_parent = None
        if (
            widget is not None
            and effective_ownership == WidgetOwnership.Reparented
        ):
            original_parent = widget.parentWidget()
        elif widget is not None and widget.parent() is not None:
            # Route the detach through PySide before the native host reparents
            # the widget. This removes any previous Python parent bookkeeping
            # while keeping the wrapper itself Python-owned.
            widget.setParent(None)

        applied = super()._setContentWidgetWithOwnership(
            widget,
            effective_ownership,
        )
        if not applied:
            raise RuntimeError("ScrollView rejected the content contract")

        # Keep Python subclasses and a Reparented restore target alive without
        # adding either object to Shiboken's internal parent/reference tables.
        # Older Shiboken 6.2 builds can re-enter those tables while a Windows
        # host wrapper is invalidated.
        self._fluentqt_hosted_content = widget
        self._fluentqt_original_parent = original_parent

    def setOwnedContentWidget(self, widget):
        """Install content that is deleted when it leaves the host."""

        self._set_content_widget_with_ownership(
            widget,
            WidgetOwnership.Owned,
        )

    def setBorrowedContentWidget(self, widget):
        """Install content that becomes parentless when it leaves the host."""

        self._set_content_widget_with_ownership(
            widget,
            WidgetOwnership.Borrowed,
        )

    def setReparentedContentWidget(self, widget):
        """Install content that returns to its current QWidget parent."""

        self._set_content_widget_with_ownership(
            widget,
            WidgetOwnership.Reparented,
        )

    def setContentWidget(self, widget):
        self.setOwnedContentWidget(widget)

    def setWidget(self, widget):
        self.setOwnedContentWidget(widget)

    def takeContentWidget(self):
        widget = super().takeContentWidget()
        if widget is not None:
            # Synchronize Shiboken's QObject parent bookkeeping after every
            # mode, including a Reparented child that originally had a parent.
            widget.setParent(None)
        self._fluentqt_hosted_content = None
        self._fluentqt_original_parent = None
        return widget

    def takeWidget(self):
        return self.takeContentWidget()


__all__ = [
    "AnnotatedScrollBar",
    "AnnotatedScrollBarLabel",
    "PipsPager",
    "ScrollBar",
    "ScrollView",
    "WidgetOwnership",
]
