"""Collection controls with explicit Python navigation ownership contracts."""

import weakref

from shiboken6 import Shiboken

from . import _fluentqt as _native


WidgetOwnership = _native.fluent.WidgetOwnership
SelectionMode = _native.fluent.SelectionMode
_NativeFlipView = _native.fluent.FlipView
_NativeFlowView = _native.fluent.FlowView
_NativeGridView = _native.fluent.GridView
_NativeListView = _native.fluent.ListView
_NativeSplitView = _native.fluent.SplitView
SplitViewPaneOptions = _native.fluent.SplitViewPaneOptions
_NativeStackView = _native.fluent.StackView
_NativeTreeView = _native.fluent.TreeView


def _split_view_options_key(options):
    return (
        options.minimumSize,
        options.preferredSize,
        options.maximumSize,
        options.fill,
    )


def _split_view_options_eq(self, other):
    if not isinstance(other, SplitViewPaneOptions):
        return NotImplemented
    return _split_view_options_key(self) == _split_view_options_key(other)


def _split_view_options_ne(self, other):
    result = _split_view_options_eq(self, other)
    if result is NotImplemented:
        return NotImplemented
    return not result


SplitViewPaneOptions.__eq__ = _split_view_options_eq
SplitViewPaneOptions.__ne__ = _split_view_options_ne
SplitViewPaneOptions.__hash__ = None


class FlipView(_NativeFlipView):
    """Fluent carousel with explicit per-page ownership methods.

    ``addPage()`` and ``insertPage()`` preserve the C++ host-owned default.
    Borrowed pages become parentless when released, Reparented pages return to
    their original QWidget parent, and ``takePage()`` always transfers a
    parentless page to the caller. Python page wrappers and restoration parents
    remain alive while the page is hosted.
    """

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._fluentqt_page_records = {}

    def _remember_page(self, page, original_parent):
        key = id(page)
        host_ref = weakref.ref(self)

        def forget_destroyed_page(*_args):
            host = host_ref()
            if host is not None:
                host._fluentqt_page_records.pop(key, None)

        page.destroyed.connect(forget_destroyed_page)
        self._fluentqt_page_records[key] = (
            page,
            original_parent,
            forget_destroyed_page,
        )

    def _forget_page(self, page):
        record = self._fluentqt_page_records.pop(id(page), None)
        if record is None:
            return
        try:
            page.destroyed.disconnect(record[2])
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

    def _contains_page(self, page):
        for index in range(super().pageCount()):
            if super().pageAt(index) is page:
                return True
        return False

    def _install_page(self, page, ownership, index=None):
        if page is None:
            return False
        if page is self or page.isAncestorOf(self):
            raise ValueError("FlipView page cannot be the host or its ancestor")
        if id(page) in self._fluentqt_page_records or self._contains_page(page):
            return False

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
            # Clear PySide's previous parent bookkeeping before native QWidget
            # adoption. The facade retains the wrapper independently.
            page.setParent(None)

        self._remember_page(page, original_parent)
        try:
            if index is None:
                accepted = super()._addPageWithOwnership(page, ownership)
            else:
                accepted = super()._insertPageWithOwnership(
                    index,
                    page,
                    ownership,
                )
        except Exception:
            self._forget_page(page)
            self._restore_rejected_parent(
                page,
                previous_parent,
                ownership,
            )
            raise

        if not accepted:
            self._forget_page(page)
            self._restore_rejected_parent(
                page,
                previous_parent,
                ownership,
            )
        return accepted

    def addOwnedPage(self, page):
        """Append a page deleted when it leaves the FlipView."""

        return self._install_page(page, WidgetOwnership.Owned)

    def addBorrowedPage(self, page):
        """Append a page detached when it leaves the FlipView."""

        return self._install_page(page, WidgetOwnership.Borrowed)

    def addReparentedPage(self, page):
        """Append a page restored to its current QWidget parent."""

        return self._install_page(page, WidgetOwnership.Reparented)

    def addPage(self, page):
        return self.addOwnedPage(page)

    def insertOwnedPage(self, index, page):
        """Insert a page deleted when it leaves the FlipView."""

        return self._install_page(
            page,
            WidgetOwnership.Owned,
            index,
        )

    def insertBorrowedPage(self, index, page):
        """Insert a page detached when it leaves the FlipView."""

        return self._install_page(
            page,
            WidgetOwnership.Borrowed,
            index,
        )

    def insertReparentedPage(self, index, page):
        """Insert a page restored to its current QWidget parent."""

        return self._install_page(
            page,
            WidgetOwnership.Reparented,
            index,
        )

    def insertPage(self, index, page):
        return self.insertOwnedPage(index, page)

    def removePage(self, index):
        page = super().pageAt(index)
        removed = super()._releasePageWithOwnership(index)
        if removed and page is not None:
            self._forget_page(page)
        return removed

    def takePage(self, index):
        page = super().takePage(index)
        if page is not None:
            page.setParent(None)
            self._forget_page(page)
        return page


class SplitView(_NativeSplitView):
    """Resizable native panes with explicit per-pane ownership.

    ``addPane()`` and ``insertPane()`` preserve the C++ host-owned default.
    Borrowed panes become parentless when removed, Reparented panes return to
    their original QWidget parent, and ``takePaneAt()`` always transfers a
    parentless pane to the caller. Pane wrappers and restoration parents remain
    alive while native SplitView hosts them.
    """

    PaneOptions = SplitViewPaneOptions

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._fluentqt_pane_records = {}

    @staticmethod
    def _normalize_options(options):
        if options is None:
            return SplitViewPaneOptions()
        if not isinstance(options, SplitViewPaneOptions):
            raise TypeError(
                "SplitView pane options must be SplitViewPaneOptions or None"
            )
        return options

    def _remember_pane(self, pane, ownership, original_parent):
        key = id(pane)
        host_ref = weakref.ref(self)

        def forget_destroyed_pane(*_args):
            host = host_ref()
            if host is not None:
                host._fluentqt_pane_records.pop(key, None)

        pane.destroyed.connect(forget_destroyed_pane)
        self._fluentqt_pane_records[key] = (
            pane,
            ownership,
            original_parent,
            forget_destroyed_pane,
        )

    def _forget_pane(self, pane):
        record = self._fluentqt_pane_records.pop(id(pane), None)
        if record is None:
            return
        try:
            pane.destroyed.disconnect(record[3])
        except (RuntimeError, TypeError):
            pass

    @staticmethod
    def _restore_rejected_parent(pane, previous_parent, ownership):
        if ownership == WidgetOwnership.Reparented:
            return
        try:
            pane.setParent(previous_parent)
        except RuntimeError:
            pass

    @staticmethod
    def _synchronize_released_parent(pane, ownership, original_parent):
        if ownership == WidgetOwnership.Owned:
            return
        target_parent = (
            original_parent
            if ownership == WidgetOwnership.Reparented
            else None
        )
        try:
            pane.setParent(target_parent)
        except RuntimeError:
            pass

    def _install_pane(self, pane, ownership, options=None, index=None):
        if pane is None:
            return -1
        if pane is self or pane.isAncestorOf(self):
            raise ValueError("SplitView pane cannot be the host or its ancestor")
        if id(pane) in self._fluentqt_pane_records:
            return -1
        if super().indexOf(pane) >= 0:
            return -1

        normalized_options = self._normalize_options(options)
        previous_parent = pane.parentWidget()
        original_parent = (
            previous_parent
            if ownership == WidgetOwnership.Reparented
            else None
        )
        if (
            previous_parent is not None
            and ownership != WidgetOwnership.Reparented
        ):
            # Clear PySide's old parent bookkeeping before native QWidget
            # adoption. The facade retains the wrapper independently.
            pane.setParent(None)

        self._remember_pane(pane, ownership, original_parent)
        try:
            if index is None:
                result = super()._addPaneWithOwnership(
                    pane,
                    ownership,
                    normalized_options,
                )
            else:
                result = super()._insertPaneWithOwnership(
                    index,
                    pane,
                    ownership,
                    normalized_options,
                )
        except Exception:
            self._forget_pane(pane)
            self._restore_rejected_parent(
                pane,
                previous_parent,
                ownership,
            )
            raise

        if result < 0:
            self._forget_pane(pane)
            self._restore_rejected_parent(
                pane,
                previous_parent,
                ownership,
            )
        return result

    def addOwnedPane(self, pane, options=None):
        """Append a pane deleted when it leaves the SplitView."""

        return self._install_pane(
            pane,
            WidgetOwnership.Owned,
            options,
        )

    def addBorrowedPane(self, pane, options=None):
        """Append a pane detached when it leaves the SplitView."""

        return self._install_pane(
            pane,
            WidgetOwnership.Borrowed,
            options,
        )

    def addReparentedPane(self, pane, options=None):
        """Append a pane restored to its current QWidget parent."""

        return self._install_pane(
            pane,
            WidgetOwnership.Reparented,
            options,
        )

    def addPane(self, pane, options=None):
        return self.addOwnedPane(pane, options)

    def insertOwnedPane(self, index, pane, options=None):
        """Insert a pane deleted when it leaves the SplitView."""

        return self._install_pane(
            pane,
            WidgetOwnership.Owned,
            options,
            index,
        )

    def insertBorrowedPane(self, index, pane, options=None):
        """Insert a pane detached when it leaves the SplitView."""

        return self._install_pane(
            pane,
            WidgetOwnership.Borrowed,
            options,
            index,
        )

    def insertReparentedPane(self, index, pane, options=None):
        """Insert a pane restored to its current QWidget parent."""

        return self._install_pane(
            pane,
            WidgetOwnership.Reparented,
            options,
            index,
        )

    def insertPane(self, index, pane, options=None):
        return self.insertOwnedPane(index, pane, options)

    def removePane(self, pane):
        return self.removePaneAt(super().indexOf(pane))

    def removePaneAt(self, index):
        pane = super().paneAt(index)
        if pane is None:
            return False
        record = self._fluentqt_pane_records.get(id(pane))
        removed = super()._releasePaneAtWithOwnership(index)
        if not removed:
            return False
        if record is not None and Shiboken.isValid(pane):
            self._synchronize_released_parent(
                pane,
                record[1],
                record[2],
            )
        self._forget_pane(pane)
        return True

    def takePaneAt(self, index):
        pane = super().takePaneAt(index)
        if pane is not None:
            pane.setParent(None)
            self._forget_pane(pane)
        return pane


class FlowView(_NativeFlowView):
    """Fluent wrapping view for caller-owned Qt models and delegates.

    Native drag reordering is supported for ``QStandardItemModel``. Other
    ``QAbstractItemModel`` implementations retain display, selection, and
    notification support without a reorder guarantee.
    """

    SelectionMode = SelectionMode

    def __init__(self, *args, **kwargs):
        selection_mode = kwargs.pop("selectionMode", None)
        super().__init__(*args, **kwargs)
        self._fluentqt_item_delegate = None
        self._fluentqt_item_delegate_destroyed = None
        if selection_mode is not None:
            self.setSelectionMode(selection_mode)

    def selectionMode(self):
        return _native.flowViewSelectionMode(self)

    def setSelectionMode(self, mode):
        _native.setFlowViewSelectionMode(self, mode)

    def verticalFluentScrollBar(self):
        return _native.flowViewVerticalFluentScrollBar(self)

    def setItemDelegate(self, delegate):
        previous = self._fluentqt_item_delegate
        callback = self._fluentqt_item_delegate_destroyed
        super().setItemDelegate(delegate)

        if previous is not None and callback is not None:
            try:
                previous.destroyed.disconnect(callback)
            except (RuntimeError, TypeError):
                pass

        self._fluentqt_item_delegate = delegate
        self._fluentqt_item_delegate_destroyed = None
        if delegate is None:
            return

        host_ref = weakref.ref(self)

        def forget_destroyed_delegate(*_args):
            host = host_ref()
            if host is not None:
                host._fluentqt_item_delegate = None
                host._fluentqt_item_delegate_destroyed = None

        delegate.destroyed.connect(forget_destroyed_delegate)
        self._fluentqt_item_delegate_destroyed = forget_destroyed_delegate

    def itemDelegate(self, *args):
        delegate = self._fluentqt_item_delegate
        if delegate is not None and not args:
            if Shiboken.isValid(delegate):
                return delegate
            self._fluentqt_item_delegate = None
            self._fluentqt_item_delegate_destroyed = None
            return None
        return super().itemDelegate(*args)


class GridView(_NativeGridView):
    """Fluent grid that retains its caller-owned Qt item delegate."""

    SelectionMode = SelectionMode

    def __init__(self, *args, **kwargs):
        selection_mode = kwargs.pop("selectionMode", None)
        super().__init__(*args, **kwargs)
        self._fluentqt_item_delegate = None
        self._fluentqt_item_delegate_destroyed = None
        if selection_mode is not None:
            self.setSelectionMode(selection_mode)

    def selectionMode(self):
        return _native.gridViewSelectionMode(self)

    def setSelectionMode(self, mode):
        _native.setGridViewSelectionMode(self, mode)

    def verticalFluentScrollBar(self):
        return _native.gridViewVerticalFluentScrollBar(self)

    def setItemDelegate(self, delegate):
        previous = self._fluentqt_item_delegate
        callback = self._fluentqt_item_delegate_destroyed
        super().setItemDelegate(delegate)

        if previous is not None and callback is not None:
            try:
                previous.destroyed.disconnect(callback)
            except (RuntimeError, TypeError):
                pass

        self._fluentqt_item_delegate = delegate
        self._fluentqt_item_delegate_destroyed = None
        if delegate is None:
            return

        host_ref = weakref.ref(self)

        def forget_destroyed_delegate(*_args):
            host = host_ref()
            if host is not None:
                host._fluentqt_item_delegate = None
                host._fluentqt_item_delegate_destroyed = None

        delegate.destroyed.connect(forget_destroyed_delegate)
        self._fluentqt_item_delegate_destroyed = forget_destroyed_delegate

    def itemDelegate(self, *args):
        delegate = self._fluentqt_item_delegate
        if delegate is not None and not args:
            if Shiboken.isValid(delegate):
                return delegate
            # Shiboken 6.2 on Windows can invalidate a native Qt delegate
            # before its Python destroyed callback clears the retained wrapper.
            self._fluentqt_item_delegate = None
            self._fluentqt_item_delegate_destroyed = None
            return None
        return super().itemDelegate(*args)


class ListView(_NativeListView):
    """Fluent item view that retains caller-owned Qt models and delegates."""

    SelectionMode = SelectionMode

    def __init__(self, *args, **kwargs):
        selection_mode = kwargs.pop("selectionMode", None)
        for unsupported in ("header", "footer"):
            if unsupported in kwargs:
                raise TypeError(
                    "ListView {0} QWidget hosting is not exposed to Python; "
                    "use {0}Text or compose the widget outside the view".format(
                        unsupported
                    )
                )
        if "sectionEnabled" in kwargs:
            raise TypeError(
                "ListView section grouping is not exposed to Python; "
                "compose grouped rows in the model or outside the view"
            )
        super().__init__(*args, **kwargs)
        self._fluentqt_item_delegate = None
        self._fluentqt_item_delegate_destroyed = None
        if selection_mode is not None:
            self.setSelectionMode(selection_mode)

    def selectionMode(self):
        return _native.listViewSelectionMode(self)

    def setSelectionMode(self, mode):
        _native.setListViewSelectionMode(self, mode)

    def verticalFluentScrollBar(self):
        return _native.listViewVerticalFluentScrollBar(self)

    def horizontalFluentScrollBar(self):
        return _native.listViewHorizontalFluentScrollBar(self)

    def setItemDelegate(self, delegate):
        previous = self._fluentqt_item_delegate
        callback = self._fluentqt_item_delegate_destroyed
        super().setItemDelegate(delegate)

        if previous is not None and callback is not None:
            try:
                previous.destroyed.disconnect(callback)
            except (RuntimeError, TypeError):
                pass

        self._fluentqt_item_delegate = delegate
        self._fluentqt_item_delegate_destroyed = None
        if delegate is None:
            return

        host_ref = weakref.ref(self)

        def forget_destroyed_delegate(*_args):
            host = host_ref()
            if host is not None:
                host._fluentqt_item_delegate = None
                host._fluentqt_item_delegate_destroyed = None

        delegate.destroyed.connect(forget_destroyed_delegate)
        self._fluentqt_item_delegate_destroyed = (
            forget_destroyed_delegate
        )

    def itemDelegate(self, *args):
        delegate = self._fluentqt_item_delegate
        if delegate is not None and not args:
            if Shiboken.isValid(delegate):
                return delegate
            self._fluentqt_item_delegate = None
            self._fluentqt_item_delegate_destroyed = None
            return None
        return super().itemDelegate(*args)


class TreeView(_NativeTreeView):
    """Fluent hierarchy view with caller-owned Qt models and delegates.

    Native drag reordering is supported for ``QStandardItemModel``. Other
    ``QAbstractItemModel`` implementations retain hierarchy, selection, and
    notification support without a reorder guarantee.
    """

    SelectionMode = SelectionMode

    def __init__(self, *args, **kwargs):
        selection_mode = kwargs.pop("selectionMode", None)
        super().__init__(*args, **kwargs)
        self._fluentqt_item_delegate = None
        self._fluentqt_item_delegate_destroyed = None
        if selection_mode is not None:
            self.setSelectionMode(selection_mode)

    def selectionMode(self):
        return _native.treeViewSelectionMode(self)

    def setSelectionMode(self, mode):
        _native.setTreeViewSelectionMode(self, mode)

    def verticalFluentScrollBar(self):
        return _native.treeViewVerticalFluentScrollBar(self)

    def horizontalFluentScrollBar(self):
        return _native.treeViewHorizontalFluentScrollBar(self)

    def setItemDelegate(self, delegate):
        previous = self._fluentqt_item_delegate
        callback = self._fluentqt_item_delegate_destroyed
        super().setItemDelegate(delegate)

        if previous is not None and callback is not None:
            try:
                previous.destroyed.disconnect(callback)
            except (RuntimeError, TypeError):
                pass

        self._fluentqt_item_delegate = delegate
        self._fluentqt_item_delegate_destroyed = None
        if delegate is None:
            return

        host_ref = weakref.ref(self)

        def forget_destroyed_delegate(*_args):
            host = host_ref()
            if host is not None:
                host._fluentqt_item_delegate = None
                host._fluentqt_item_delegate_destroyed = None

        delegate.destroyed.connect(forget_destroyed_delegate)
        self._fluentqt_item_delegate_destroyed = forget_destroyed_delegate

    def itemDelegate(self, *args):
        delegate = self._fluentqt_item_delegate
        if delegate is not None and not args:
            if Shiboken.isValid(delegate):
                return delegate
            self._fluentqt_item_delegate = None
            self._fluentqt_item_delegate_destroyed = None
            return None
        return super().itemDelegate(*args)


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


__all__ = [
    "FlipView",
    "FlowView",
    "GridView",
    "ListView",
    "SelectionMode",
    "SplitView",
    "SplitViewPaneOptions",
    "StackView",
    "TreeView",
    "WidgetOwnership",
]
