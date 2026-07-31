"""Navigation components with explicit Python ownership contracts."""

import weakref

from shiboken6 import Shiboken

from . import _fluentqt as _native


BreadcrumbItem = _native.fluent.BreadcrumbItem
_NativeBreadcrumb = _native.fluent.Breadcrumb
_NativeNavigationView = _native.fluent.NavigationView
_NativeStackContentHost = _native.fluent.StackContentHost
_native_stack_take_page = _NativeStackContentHost.takePage
Pivot = _native.fluent.Pivot
PivotItem = _native.fluent.PivotItem
SelectorBar = _native.fluent.SelectorBar
SelectorBarItem = _native.fluent.SelectorBarItem
TabView = _native.fluent.TabView
TabViewItem = _native.fluent.TabViewItem
WidgetOwnership = _native.fluent.WidgetOwnership


def _ensure_page_records(host):
    records = getattr(host, "_fluentqt_page_records", None)
    if records is None:
        records = {}
        host._fluentqt_page_records = records
    return records


def _remember_page(host, page, ownership, original_parent):
    records = _ensure_page_records(host)
    key = id(page)
    host_ref = weakref.ref(host)

    def forget_destroyed_page(*_args):
        current_host = host_ref()
        if current_host is not None:
            current_records = getattr(
                current_host,
                "_fluentqt_page_records",
                None,
            )
            if current_records is not None:
                current_records.pop(key, None)

    page.destroyed.connect(forget_destroyed_page)
    records[key] = (
        page,
        ownership,
        original_parent,
        forget_destroyed_page,
    )


def _forget_page(host, page):
    record = _ensure_page_records(host).pop(id(page), None)
    if record is None:
        return
    try:
        page.destroyed.disconnect(record[3])
    except (RuntimeError, TypeError):
        pass


def _restore_rejected_parent(page, previous_parent, ownership):
    if ownership == WidgetOwnership.Reparented:
        return
    try:
        page.setParent(previous_parent)
    except RuntimeError:
        pass


def _synchronize_released_parent(record):
    if record is None:
        return
    page, ownership, original_parent, _callback = record
    if ownership == WidgetOwnership.Owned or not Shiboken.isValid(page):
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


def _stack_install_page(host, index, page, ownership):
    records = _ensure_page_records(host)
    if page is None:
        return _NativeStackContentHost._insertPageWithOwnership(
            host,
            index,
            page,
            ownership,
        )
    if page is host or page.isAncestorOf(host):
        raise ValueError(
            "StackContentHost page cannot be the host or its ancestor"
        )
    if id(page) in records or host.indexOf(page) >= 0:
        return False

    previous_parent = page.parentWidget()
    original_parent = (
        previous_parent
        if ownership == WidgetOwnership.Reparented
        else None
    )
    if previous_parent is not None and ownership != WidgetOwnership.Reparented:
        page.setParent(None)

    _remember_page(host, page, ownership, original_parent)
    try:
        accepted = _NativeStackContentHost._insertPageWithOwnership(
            host,
            index,
            page,
            ownership,
        )
    except Exception:
        _forget_page(host, page)
        _restore_rejected_parent(page, previous_parent, ownership)
        raise
    if not accepted:
        _forget_page(host, page)
        _restore_rejected_parent(page, previous_parent, ownership)
    return accepted


def _stack_replace_page(host, index, page, ownership):
    records = _ensure_page_records(host)
    current = host.pageWidget(index)
    if current is page and page is not None:
        record = records.get(id(page))
        current_ownership = host.pageOwnershipAt(index)
        if current_ownership != ownership:
            raise ValueError("takePage() before changing ownership mode")
        if record is None:
            _remember_page(host, page, ownership, None)
        return True
    if page is not None:
        if page is host or page.isAncestorOf(host):
            raise ValueError(
                "StackContentHost page cannot be the host or its ancestor"
            )
        if id(page) in records or host.indexOf(page) >= 0:
            return False

    previous_parent = page.parentWidget() if page is not None else None
    original_parent = (
        previous_parent
        if page is not None and ownership == WidgetOwnership.Reparented
        else None
    )
    if (
        page is not None
        and previous_parent is not None
        and ownership != WidgetOwnership.Reparented
    ):
        page.setParent(None)

    old_record = records.get(id(current)) if current is not None else None
    if page is not None:
        _remember_page(host, page, ownership, original_parent)
    try:
        accepted = _NativeStackContentHost._replacePageWithOwnership(
            host,
            index,
            page,
            ownership,
        )
    except Exception:
        if page is not None:
            _forget_page(host, page)
            _restore_rejected_parent(page, previous_parent, ownership)
        raise
    if not accepted:
        if page is not None:
            _forget_page(host, page)
            _restore_rejected_parent(page, previous_parent, ownership)
        return False

    if current is not None:
        _synchronize_released_parent(old_record)
        _forget_page(host, current)
    return True


def _stack_add_owned_page(self, page):
    return _stack_install_page(
        self,
        self.count(),
        page,
        WidgetOwnership.Owned,
    )


def _stack_add_borrowed_page(self, page):
    return _stack_install_page(
        self,
        self.count(),
        page,
        WidgetOwnership.Borrowed,
    )


def _stack_add_reparented_page(self, page):
    return _stack_install_page(
        self,
        self.count(),
        page,
        WidgetOwnership.Reparented,
    )


def _stack_add_page(self, page):
    return _stack_add_owned_page(self, page)


def _stack_insert_owned_page(self, index, page):
    return _stack_install_page(self, index, page, WidgetOwnership.Owned)


def _stack_insert_borrowed_page(self, index, page):
    return _stack_install_page(self, index, page, WidgetOwnership.Borrowed)


def _stack_insert_reparented_page(self, index, page):
    return _stack_install_page(
        self,
        index,
        page,
        WidgetOwnership.Reparented,
    )


def _stack_insert_page(self, index, page):
    return _stack_insert_owned_page(self, index, page)


def _stack_replace_owned_page(self, index, page):
    return _stack_replace_page(self, index, page, WidgetOwnership.Owned)


def _stack_replace_borrowed_page(self, index, page):
    return _stack_replace_page(self, index, page, WidgetOwnership.Borrowed)


def _stack_replace_reparented_page(self, index, page):
    return _stack_replace_page(
        self,
        index,
        page,
        WidgetOwnership.Reparented,
    )


def _stack_replace_default_page(self, index, page):
    return _stack_replace_owned_page(self, index, page)


def _stack_remove_page(self, index):
    page = self.pageWidget(index)
    record = (
        _ensure_page_records(self).get(id(page))
        if page is not None
        else None
    )
    removed = _NativeStackContentHost._releasePageWithOwnership(
        self,
        index,
    )
    if removed and page is not None:
        _synchronize_released_parent(record)
        _forget_page(self, page)
    return removed


def _stack_take_page(self, index):
    page = _native_stack_take_page(self, index)
    if page is not None:
        page.setParent(None)
        _forget_page(self, page)
    return page


def _stack_clear_pages(self):
    records = list(_ensure_page_records(self).values())
    _NativeStackContentHost._releaseAllPagesWithOwnership(self)
    for record in records:
        _synchronize_released_parent(record)
        _forget_page(self, record[0])


# NavigationView constructs its StackContentHost in C++, so its returned
# wrapper is the native class rather than a Python subclass. Install the safe
# facade directly on that native type and lazily allocate retention records.
for _method_name, _method in {
    "addOwnedPage": _stack_add_owned_page,
    "addBorrowedPage": _stack_add_borrowed_page,
    "addReparentedPage": _stack_add_reparented_page,
    "addPage": _stack_add_page,
    "insertOwnedPage": _stack_insert_owned_page,
    "insertBorrowedPage": _stack_insert_borrowed_page,
    "insertReparentedPage": _stack_insert_reparented_page,
    "insertPage": _stack_insert_page,
    "replaceOwnedPage": _stack_replace_owned_page,
    "replaceBorrowedPage": _stack_replace_borrowed_page,
    "replaceReparentedPage": _stack_replace_reparented_page,
    "replacePage": _stack_replace_default_page,
    "removePage": _stack_remove_page,
    "releasePage": _stack_remove_page,
    "takePage": _stack_take_page,
    "clearPages": _stack_clear_pages,
    "releaseAllPages": _stack_clear_pages,
}.items():
    setattr(_NativeStackContentHost, _method_name, _method)

StackContentHost = _NativeStackContentHost


_CHROME_GETTERS = {
    "header": _NativeNavigationView.headerChromeWidget,
    "main": _NativeNavigationView.mainChromeWidget,
    "footer": _NativeNavigationView.footerChromeWidget,
}
_CHROME_OWNERSHIP_GETTERS = {
    "header": _NativeNavigationView.headerChromeWidgetOwnership,
    "main": _NativeNavigationView.mainChromeWidgetOwnership,
    "footer": _NativeNavigationView.footerChromeWidgetOwnership,
}
_CHROME_SETTERS = {
    "header": _NativeNavigationView._setHeaderChromeWidgetWithOwnership,
    "main": _NativeNavigationView._setMainChromeWidgetWithOwnership,
    "footer": _NativeNavigationView._setFooterChromeWidgetWithOwnership,
}
_CHROME_TAKERS = {
    "header": _NativeNavigationView.takeHeaderChromeWidget,
    "main": _NativeNavigationView.takeMainChromeWidget,
    "footer": _NativeNavigationView.takeFooterChromeWidget,
}
_CHROME_RELEASERS = {
    "header": _NativeNavigationView._releaseHeaderChromeWidgetWithOwnership,
    "main": _NativeNavigationView._releaseMainChromeWidgetWithOwnership,
    "footer": _NativeNavigationView._releaseFooterChromeWidgetWithOwnership,
}


class NavigationView(_NativeNavigationView):
    """Responsive navigation shell with explicit chrome/page ownership."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._fluentqt_chrome_records = {}
        self._fluentqt_content_host = super().contentHost()
        _ensure_page_records(self._fluentqt_content_host)

    def contentHost(self):
        """Return the C++-owned page host with the Python-safe facade."""

        return self._fluentqt_content_host

    def _remember_chrome(self, slot, widget, ownership, original_parent):
        host_ref = weakref.ref(self)

        def forget_destroyed_chrome(*_args):
            host = host_ref()
            if host is not None:
                record = host._fluentqt_chrome_records.get(slot)
                if record is not None and record[0] is widget:
                    host._fluentqt_chrome_records.pop(slot, None)

        widget.destroyed.connect(forget_destroyed_chrome)
        self._fluentqt_chrome_records[slot] = (
            widget,
            ownership,
            original_parent,
            forget_destroyed_chrome,
        )

    def _forget_chrome(self, slot):
        record = self._fluentqt_chrome_records.pop(slot, None)
        if record is None:
            return
        try:
            record[0].destroyed.disconnect(record[3])
        except (RuntimeError, TypeError):
            pass

    def _install_chrome(self, slot, widget, ownership):
        current = _CHROME_GETTERS[slot](self)
        if current is widget:
            if widget is None:
                return True
            if _CHROME_OWNERSHIP_GETTERS[slot](self) != ownership:
                raise ValueError(
                    "take the chrome widget before changing ownership mode"
                )
            return True
        if widget is not None:
            if widget is self or widget.isAncestorOf(self):
                raise ValueError(
                    "NavigationView chrome cannot be the host or its ancestor"
                )
            if widget is self.contentHost():
                raise ValueError(
                    "NavigationView contentHost cannot be used as chrome"
                )
            for other_slot, getter in _CHROME_GETTERS.items():
                if other_slot != slot and getter(self) is widget:
                    return False

        previous_parent = widget.parentWidget() if widget is not None else None
        original_parent = (
            previous_parent
            if widget is not None and ownership == WidgetOwnership.Reparented
            else None
        )
        if (
            widget is not None
            and previous_parent is not None
            and ownership != WidgetOwnership.Reparented
        ):
            widget.setParent(None)

        old_record = self._fluentqt_chrome_records.get(slot)
        if widget is not None:
            self._remember_chrome(slot, widget, ownership, original_parent)
        try:
            accepted = _CHROME_SETTERS[slot](self, widget, ownership)
        except Exception:
            if widget is not None:
                self._forget_chrome(slot)
                _restore_rejected_parent(
                    widget,
                    previous_parent,
                    ownership,
                )
            raise
        if not accepted:
            if widget is not None:
                self._forget_chrome(slot)
                _restore_rejected_parent(
                    widget,
                    previous_parent,
                    ownership,
                )
            if old_record is not None:
                self._fluentqt_chrome_records[slot] = old_record
            return False

        _synchronize_released_parent(old_record)
        if old_record is not None:
            try:
                old_record[0].destroyed.disconnect(old_record[3])
            except (RuntimeError, TypeError):
                pass
        if widget is None:
            self._fluentqt_chrome_records.pop(slot, None)
        return True

    def _take_chrome(self, slot):
        widget = _CHROME_TAKERS[slot](self)
        if widget is not None:
            widget.setParent(None)
            self._forget_chrome(slot)
        return widget

    def _release_chrome(self, slot):
        record = self._fluentqt_chrome_records.get(slot)
        released = _CHROME_RELEASERS[slot](self)
        if released:
            _synchronize_released_parent(record)
            self._forget_chrome(slot)
        return released

    def setOwnedHeaderChromeWidget(self, widget):
        return self._install_chrome("header", widget, WidgetOwnership.Owned)

    def setBorrowedHeaderChromeWidget(self, widget):
        return self._install_chrome("header", widget, WidgetOwnership.Borrowed)

    def setReparentedHeaderChromeWidget(self, widget):
        return self._install_chrome(
            "header", widget, WidgetOwnership.Reparented
        )

    def setHeaderChromeWidget(self, widget):
        return self.setOwnedHeaderChromeWidget(widget)

    def takeHeaderChromeWidget(self):
        return self._take_chrome("header")

    def releaseHeaderChromeWidget(self):
        return self._release_chrome("header")

    def setOwnedMainChromeWidget(self, widget):
        return self._install_chrome("main", widget, WidgetOwnership.Owned)

    def setBorrowedMainChromeWidget(self, widget):
        return self._install_chrome("main", widget, WidgetOwnership.Borrowed)

    def setReparentedMainChromeWidget(self, widget):
        return self._install_chrome("main", widget, WidgetOwnership.Reparented)

    def setMainChromeWidget(self, widget):
        return self.setOwnedMainChromeWidget(widget)

    def takeMainChromeWidget(self):
        return self._take_chrome("main")

    def releaseMainChromeWidget(self):
        return self._release_chrome("main")

    def setOwnedFooterChromeWidget(self, widget):
        return self._install_chrome("footer", widget, WidgetOwnership.Owned)

    def setBorrowedFooterChromeWidget(self, widget):
        return self._install_chrome("footer", widget, WidgetOwnership.Borrowed)

    def setReparentedFooterChromeWidget(self, widget):
        return self._install_chrome(
            "footer", widget, WidgetOwnership.Reparented
        )

    def setFooterChromeWidget(self, widget):
        return self.setOwnedFooterChromeWidget(widget)

    def takeFooterChromeWidget(self):
        return self._take_chrome("footer")

    def releaseFooterChromeWidget(self):
        return self._release_chrome("footer")


def _install_mutable_value_semantics(value_type, fields):
    def value_key(item):
        return tuple(getattr(item, field) for field in fields)

    def value_eq(self, other):
        if not isinstance(other, value_type):
            return NotImplemented
        return value_key(self) == value_key(other)

    def value_ne(self, other):
        result = value_eq(self, other)
        if result is NotImplemented:
            return NotImplemented
        return not result

    value_type.__eq__ = value_eq
    value_type.__ne__ = value_ne
    value_type.__hash__ = None


# These metadata values are mutable and have no C++ comparison operators. Keep
# value comparisons stable across Shiboken releases without making them
# hashable.
_install_mutable_value_semantics(
    BreadcrumbItem,
    ("text", "data", "enabled", "accessibleName"),
)
_install_mutable_value_semantics(
    PivotItem,
    ("header", "iconGlyph", "enabled", "data", "accessibleName"),
)
_install_mutable_value_semantics(
    SelectorBarItem,
    (
        "text",
        "iconGlyph",
        "enabled",
        "visible",
        "selected",
        "data",
        "accessibleName",
    ),
)
_install_mutable_value_semantics(
    TabViewItem,
    ("text", "iconGlyph", "closable", "enabled", "data", "accessibleName"),
)


class Breadcrumb(_NativeBreadcrumb):
    """Native breadcrumb with deterministic Python sequence dispatch."""

    def setItems(self, items):
        if isinstance(items, (str, bytes)):
            raise TypeError(
                "Breadcrumb.setItems expects a sequence of str or "
                "BreadcrumbItem values"
            )
        normalized = list(items)
        if all(isinstance(item, str) for item in normalized):
            _native.setBreadcrumbTextItems(self, normalized)
            return
        if all(isinstance(item, BreadcrumbItem) for item in normalized):
            _native.setBreadcrumbMetadataItems(self, normalized)
            return
        raise TypeError(
            "Breadcrumb.setItems cannot mix str and BreadcrumbItem values"
        )


__all__ = [
    "Breadcrumb",
    "BreadcrumbItem",
    "NavigationView",
    "Pivot",
    "PivotItem",
    "SelectorBar",
    "SelectorBarItem",
    "StackContentHost",
    "TabView",
    "TabViewItem",
    "WidgetOwnership",
]
