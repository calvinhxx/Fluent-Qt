"""Layout components and hosted-content ownership facades."""

import weakref

from . import _fluentqt as _native


Card = _native.fluent.Card
Divider = _native.fluent.Divider
WidgetOwnership = _native.fluent.WidgetOwnership
_NativeAccordion = _native.fluent.Accordion
_NativeExpander = _native.fluent.Expander
_CONTENT_UNSET = object()


class Accordion(_NativeAccordion):
    """Expander group with explicit per-item ownership methods.

    ``addItem()`` and ``insertItem()`` preserve the C++ borrowed default.
    Owned items are deleted with the Accordion, borrowed items become
    parentless, and reparented items return to their original QWidget parent.
    The facade retains Python wrappers while items are hosted so Python
    subclass state is not lost.
    """

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._fluentqt_item_records = {}

    def _remember_item(self, item, original_parent):
        key = id(item)
        host_ref = weakref.ref(self)

        def forget_destroyed_item(*_args):
            host = host_ref()
            if host is not None:
                host._fluentqt_item_records.pop(key, None)

        item.destroyed.connect(forget_destroyed_item)
        self._fluentqt_item_records[key] = (
            item,
            original_parent,
            forget_destroyed_item,
        )

    def _forget_item(self, item):
        record = self._fluentqt_item_records.pop(id(item), None)
        if record is None:
            return
        try:
            item.destroyed.disconnect(record[2])
        except (RuntimeError, TypeError):
            # An Owned item can already be invalid by the time removeItem()
            # returns. QObject destruction has then removed the connection.
            pass

    @staticmethod
    def _restore_rejected_parent(item, previous_parent, ownership):
        if (
            previous_parent is not None
            and ownership != WidgetOwnership.Reparented
        ):
            try:
                item.setParent(previous_parent)
            except RuntimeError:
                pass

    def _install_item(self, item, ownership, index=None):
        if item is None:
            return False
        if item.isAncestorOf(self) or super().indexOf(item) >= 0:
            return False

        previous_parent = item.parentWidget()
        original_parent = (
            previous_parent
            if ownership == WidgetOwnership.Reparented
            else None
        )
        if (
            previous_parent is not None
            and ownership != WidgetOwnership.Reparented
        ):
            # Clear PySide's former parent bookkeeping before native C++
            # installs the item into the Accordion's Qt parent chain.
            item.setParent(None)

        self._remember_item(item, original_parent)
        try:
            if index is None:
                accepted = super()._addItemWithOwnership(item, ownership)
            else:
                accepted = super()._insertItemWithOwnership(
                    index,
                    item,
                    ownership,
                )
        except Exception:
            self._forget_item(item)
            self._restore_rejected_parent(
                item,
                previous_parent,
                ownership,
            )
            raise

        if not accepted:
            self._forget_item(item)
            self._restore_rejected_parent(
                item,
                previous_parent,
                ownership,
            )
        return accepted

    def addOwnedItem(self, item):
        """Append an item that is deleted with the Accordion."""

        return self._install_item(item, WidgetOwnership.Owned)

    def addBorrowedItem(self, item):
        """Append an item that becomes parentless when released."""

        return self._install_item(item, WidgetOwnership.Borrowed)

    def addReparentedItem(self, item):
        """Append an item that returns to its current QWidget parent."""

        return self._install_item(item, WidgetOwnership.Reparented)

    def addItem(self, item):
        return self.addBorrowedItem(item)

    def insertOwnedItem(self, index, item):
        """Insert an item that is deleted with the Accordion."""

        return self._install_item(
            index=index,
            item=item,
            ownership=WidgetOwnership.Owned,
        )

    def insertBorrowedItem(self, index, item):
        """Insert an item that becomes parentless when released."""

        return self._install_item(
            index=index,
            item=item,
            ownership=WidgetOwnership.Borrowed,
        )

    def insertReparentedItem(self, index, item):
        """Insert an item that returns to its current QWidget parent."""

        return self._install_item(
            index=index,
            item=item,
            ownership=WidgetOwnership.Reparented,
        )

    def insertItem(self, index, item):
        return self.insertBorrowedItem(index, item)

    def removeItem(self, index):
        item = super().itemAt(index)
        removed = super().removeItem(index)
        if removed and item is not None:
            self._forget_item(item)
        return removed

    def takeItem(self, index):
        item = super().takeItem(index)
        if item is not None:
            # Clear Shiboken's former parent bookkeeping before releasing a
            # retained Reparented restore target. Releasing that parent first
            # can otherwise leave an already-detached item marked C++-owned.
            item.setParent(None)
            self._forget_item(item)
        return item


class Expander(_NativeExpander):
    """Disclosure surface with explicit content ownership methods.

    ``setContentWidget()`` uses the C++ Expander's borrowed default.
    ``setOwnedContentWidget()`` deletes content with the host, and
    ``setReparentedContentWidget()`` restores the QWidget parent present at
    adoption. ``takeContentWidget()`` always returns parentless content.
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
            WidgetOwnership.Borrowed if widget is None else ownership
        )
        if widget is self or (
            widget is not None and widget.isAncestorOf(self)
        ):
            raise ValueError(
                "Expander content cannot be the host or its ancestor"
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
            widget.setParent(None)

        applied = super()._setContentWidgetWithOwnership(
            widget,
            effective_ownership,
        )
        if not applied:
            raise RuntimeError("Expander rejected the content contract")

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
        self.setBorrowedContentWidget(widget)

    def takeContentWidget(self):
        widget = super().takeContentWidget()
        if widget is not None:
            widget.setParent(None)
        self._fluentqt_hosted_content = None
        self._fluentqt_original_parent = None
        return widget


__all__ = ["Accordion", "Card", "Divider", "Expander"]
