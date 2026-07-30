"""Collection controls with explicit Python navigation ownership contracts."""

import weakref

from . import _fluentqt as _native


WidgetOwnership = _native.fluent.WidgetOwnership
_NativeStackView = _native.fluent.StackView


class StackView(_NativeStackView):
    """Navigation stack with fixed per-page ownership methods.

    Plain ``push()``, ``replace()``, and ``setInitialItem()`` retain the C++
    default and install an Owned page. Explicit Borrowed and Reparented methods
    are available for caller-owned pages. Page wrappers and original parents
    stay alive until the native transition has finished.

    Inherited ``QStackedWidget`` insertion/removal methods are intentionally
    blocked because they bypass StackView's navigation and ownership records.
    """

    def __init__(self, *args, **kwargs):
        initial_item = kwargs.pop("initialItem", None)
        if "defaultItemOwnership" in kwargs:
            raise TypeError(
                "StackView defaultItemOwnership is not a Python constructor "
                "option; use the explicit ownership methods"
            )
        super().__init__(*args, **kwargs)
        self._fluentqt_page_records = {}

        host_ref = weakref.ref(self)

        def reconcile_after_transition(*_args):
            host = host_ref()
            if host is not None:
                host._reconcile_removed_pages()

        self.transitionFinished.connect(reconcile_after_transition)
        self._fluentqt_transition_callback = reconcile_after_transition
        if initial_item is not None:
            self.setInitialOwnedItem(initial_item)

    def _remember_page(self, page, ownership, original_parent):
        key = id(page)
        host_ref = weakref.ref(self)

        def forget_destroyed_page(*_args):
            host = host_ref()
            if host is not None:
                host._fluentqt_page_records.pop(key, None)

        page.destroyed.connect(forget_destroyed_page)
        self._fluentqt_page_records[key] = (
            page,
            ownership,
            original_parent,
            forget_destroyed_page,
        )

    def _forget_page(self, page):
        record = self._fluentqt_page_records.pop(id(page), None)
        if record is None:
            return
        try:
            page.destroyed.disconnect(record[3])
        except (RuntimeError, TypeError):
            pass

    @staticmethod
    def _restore_rejected_parent(page, previous_parent, ownership):
        if ownership == WidgetOwnership.Reparented:
            return
        try:
            page.setParent(previous_parent)
        except RuntimeError:
            pass

    @staticmethod
    def _synchronize_released_parent(page, ownership, original_parent):
        if ownership == WidgetOwnership.Owned:
            return
        target_parent = (
            original_parent
            if ownership == WidgetOwnership.Reparented
            else None
        )
        try:
            page.setParent(target_parent)
        except RuntimeError:
            pass

    def _reconcile_removed_pages(self):
        for record in tuple(self._fluentqt_page_records.values()):
            page, ownership, original_parent, _callback = record
            try:
                still_hosted = self.contains(page)
            except RuntimeError:
                still_hosted = False
            if still_hosted:
                continue
            self._synchronize_released_parent(
                page,
                ownership,
                original_parent,
            )
            self._forget_page(page)

    def _validate_pages(self, pages):
        seen = set()
        for page in pages:
            if page is None:
                return False
            key = id(page)
            if key in seen or key in self._fluentqt_page_records:
                return False
            seen.add(key)
            if page is self or page.isAncestorOf(self):
                raise ValueError(
                    "StackView page cannot be the host or its ancestor"
                )
            if super().contains(page):
                return False
        return True

    def _prepare_pages(self, pages, ownership):
        prepared = []
        for page in pages:
            previous_parent = page.parentWidget()
            original_parent = (
                previous_parent
                if ownership == WidgetOwnership.Reparented
                else None
            )
            if (
                previous_parent is not None
                and ownership != WidgetOwnership.Reparented
            ):
                # Clear PySide's old parent bookkeeping before native
                # QStackedWidget adoption.
                page.setParent(None)
            self._remember_page(page, ownership, original_parent)
            prepared.append((page, previous_parent))
        return prepared

    def _rollback_pages(self, prepared, ownership):
        for page, previous_parent in prepared:
            self._forget_page(page)
            self._restore_rejected_parent(
                page,
                previous_parent,
                ownership,
            )

    def _install_single_page(self, page, ownership, operation, index=None):
        if page is None:
            if operation == "initial":
                return self.clear()
            return False
        if not self._validate_pages((page,)):
            return False

        prepared = self._prepare_pages((page,), ownership)
        try:
            if operation == "initial":
                accepted = super()._setInitialItemWithOwnership(
                    page,
                    ownership,
                )
            elif operation == "push":
                accepted = super()._pushItemWithOwnership(
                    page,
                    ownership,
                )
            elif index is None:
                accepted = super()._replaceCurrentWithOwnership(
                    page,
                    ownership,
                )
            else:
                accepted = super()._replaceAtWithOwnership(
                    index,
                    page,
                    ownership,
                )
        except Exception:
            self._rollback_pages(prepared, ownership)
            raise

        if not accepted:
            self._rollback_pages(prepared, ownership)
        self._reconcile_removed_pages()
        return accepted

    def _push_pages(self, pages, ownership):
        pages = tuple(pages)
        if not pages or not self._validate_pages(pages):
            return False

        prepared = self._prepare_pages(pages, ownership)
        try:
            accepted = super()._pushItemsWithOwnership(
                list(pages),
                ownership,
            )
        except Exception:
            self._rollback_pages(prepared, ownership)
            raise

        if not accepted:
            self._rollback_pages(prepared, ownership)
        self._reconcile_removed_pages()
        return accepted

    def setInitialOwnedItem(self, page):
        """Reset the stack to an Owned page, or clear it for ``None``."""

        return self._install_single_page(
            page,
            WidgetOwnership.Owned,
            "initial",
        )

    def setInitialBorrowedItem(self, page):
        """Reset the stack to a page detached when it leaves the host."""

        return self._install_single_page(
            page,
            WidgetOwnership.Borrowed,
            "initial",
        )

    def setInitialReparentedItem(self, page):
        """Reset the stack to a page restored to its current parent."""

        return self._install_single_page(
            page,
            WidgetOwnership.Reparented,
            "initial",
        )

    def setInitialItem(self, page):
        return self.setInitialOwnedItem(page)

    def pushOwnedItem(self, page):
        """Push an Owned page."""

        return self._install_single_page(
            page,
            WidgetOwnership.Owned,
            "push",
        )

    def pushBorrowedItem(self, page):
        """Push a page that becomes parentless after pop."""

        return self._install_single_page(
            page,
            WidgetOwnership.Borrowed,
            "push",
        )

    def pushReparentedItem(self, page):
        """Push a page restored to its current parent after pop."""

        return self._install_single_page(
            page,
            WidgetOwnership.Reparented,
            "push",
        )

    def push(self, page):
        return self.pushOwnedItem(page)

    def pushOwnedItems(self, pages):
        """Push multiple Owned pages in one native transition."""

        return self._push_pages(pages, WidgetOwnership.Owned)

    def pushBorrowedItems(self, pages):
        """Push multiple pages that become parentless when removed."""

        return self._push_pages(pages, WidgetOwnership.Borrowed)

    def pushReparentedItems(self, pages):
        """Push multiple pages restored to their current parents."""

        return self._push_pages(pages, WidgetOwnership.Reparented)

    def replaceOwnedItem(self, page):
        """Replace the current page with an Owned page."""

        return self._install_single_page(
            page,
            WidgetOwnership.Owned,
            "replace",
        )

    def replaceBorrowedItem(self, page):
        """Replace the current page with a Borrowed page."""

        return self._install_single_page(
            page,
            WidgetOwnership.Borrowed,
            "replace",
        )

    def replaceReparentedItem(self, page):
        """Replace the current page with a Reparented page."""

        return self._install_single_page(
            page,
            WidgetOwnership.Reparented,
            "replace",
        )

    def replace(self, page):
        return self.replaceOwnedItem(page)

    def replaceOwnedItemAt(self, index, page):
        """Replace a page at ``index`` with an Owned page."""

        return self._install_single_page(
            page,
            WidgetOwnership.Owned,
            "replace",
            index,
        )

    def replaceBorrowedItemAt(self, index, page):
        """Replace a page at ``index`` with a Borrowed page."""

        return self._install_single_page(
            page,
            WidgetOwnership.Borrowed,
            "replace",
            index,
        )

    def replaceReparentedItemAt(self, index, page):
        """Replace a page at ``index`` with a Reparented page."""

        return self._install_single_page(
            page,
            WidgetOwnership.Reparented,
            "replace",
            index,
        )

    def pop(self):
        accepted = super().pop()
        if not self.busy():
            self._reconcile_removed_pages()
        return accepted

    def popToRoot(self):
        accepted = super().popToRoot()
        if not self.busy():
            self._reconcile_removed_pages()
        return accepted

    def popToItem(self, page):
        accepted = super().popToItem(page)
        if not self.busy():
            self._reconcile_removed_pages()
        return accepted

    def goBack(self):
        return self.pop()

    def clear(self):
        accepted = super().clear()
        if not self.busy():
            self._reconcile_removed_pages()
        return accepted

    def setCurrentWidget(self, page):
        if page is None or not self.contains(page):
            raise ValueError("StackView current page must already be hosted")
        native_index = -1
        for index in range(super().count()):
            if super().widget(index) is page:
                native_index = index
                break
        if native_index < 0:
            raise RuntimeError("StackView lost the hosted page index")
        super().setCurrentIndex(native_index)

    def addWidget(self, _page):
        raise RuntimeError(
            "Use StackView.pushOwnedItem(), pushBorrowedItem(), or "
            "pushReparentedItem()"
        )

    def insertWidget(self, _index, _page):
        raise RuntimeError(
            "StackView insertion must use the ownership-aware push methods"
        )

    def removeWidget(self, _page):
        raise RuntimeError(
            "Use StackView.pop(), popToItem(), popToRoot(), or clear()"
        )


__all__ = ["StackView", "WidgetOwnership"]
