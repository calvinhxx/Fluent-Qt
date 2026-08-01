"""Same-window overlays with explicit Python dependency retention."""

import weakref

from PySide6.QtWidgets import QWidget

from . import _fluentqt as _native


_NativePopup = _native.fluent.Popup
_NativeFlyout = _native.fluent.Flyout
_NativeCoachMark = _native.fluent.CoachMark
_NativeTeachingTip = _native.fluent.TeachingTip
_NativeDialog = _native.fluent.Dialog
_NativeContentDialog = _native.fluent.ContentDialog
ContentDialogButton = _NativeContentDialog.ContentDialogButton
_CONTENT_UNSET = object()


class _PopupFacadeMeta(type(_NativePopup)):
    """Preserve Popup polymorphism across sibling Python facade classes."""

    def __instancecheck__(cls, instance):
        return isinstance(instance, _NativePopup)

    def __subclasscheck__(cls, subclass):
        return issubclass(subclass, _NativePopup)


class _DialogFacadeMeta(type(_NativeDialog)):
    """Preserve Dialog polymorphism across sibling Python facade classes."""

    def __instancecheck__(cls, instance):
        return isinstance(instance, _NativeDialog)

    def __subclasscheck__(cls, subclass):
        return issubclass(subclass, _NativeDialog)


def _disconnect_dependency(record):
    if record is None:
        return
    widget, callback = record
    try:
        widget.destroyed.disconnect(callback)
    except (RuntimeError, TypeError):
        pass


class _ObservedWidgetDependencyFacade:
    """Shared retention for caller-owned QWidget dependencies."""

    def _remember_single_dependency(self, attribute_name, widget):
        old_record = getattr(self, attribute_name, None)
        _disconnect_dependency(old_record)
        setattr(self, attribute_name, None)
        if widget is None:
            return

        dependency_key = id(widget)
        host_ref = weakref.ref(self)

        def forget_destroyed_dependency(*_args):
            host = host_ref()
            if host is None:
                return
            record = getattr(host, attribute_name, None)
            if record is not None and id(record[0]) == dependency_key:
                setattr(host, attribute_name, None)

        widget.destroyed.connect(forget_destroyed_dependency)
        setattr(
            self,
            attribute_name,
            (widget, forget_destroyed_dependency),
        )


class _PopupDependencyFacade(_ObservedWidgetDependencyFacade):
    """Retain observed QWidget wrappers without changing native ownership."""

    def _initialize_popup_dependency_facade(self):
        self._fluentqt_flyout_anchor_record = None
        self._fluentqt_position_anchor_record = None
        self._fluentqt_theme_source_record = None
        self._fluentqt_passthrough_records = {}

        host_ref = weakref.ref(self)

        def release_dependencies(*_args):
            host = host_ref()
            if host is not None:
                host._release_dependency_records()

        self._fluentqt_destroyed_callback = release_dependencies
        self.destroyed.connect(release_dependencies)

    def _release_dependency_records(self):
        for attribute_name in (
            "_fluentqt_flyout_anchor_record",
            "_fluentqt_position_anchor_record",
            "_fluentqt_theme_source_record",
        ):
            record = getattr(self, attribute_name, None)
            _disconnect_dependency(record)
            setattr(self, attribute_name, None)
        for record in self._fluentqt_passthrough_records.values():
            _disconnect_dependency(record)
        self._fluentqt_passthrough_records = {}

    def setPosition(self, relative_to, local_pos):
        """Position relative to a retained, caller-owned QWidget anchor."""

        if relative_to is None:
            raise TypeError("Popup position anchor must be a QWidget")
        if relative_to is self:
            raise ValueError("Popup cannot use itself as a position anchor")
        self._setPositionWithAnchor(relative_to, local_pos)
        self._remember_single_dependency(
            "_fluentqt_position_anchor_record",
            relative_to,
        )

    def setThemeSource(self, source):
        """Use a retained, caller-owned QWidget as the local theme source."""

        if source is self:
            raise ValueError("Popup cannot use itself as a theme source")
        self._setThemeSource(source)
        self._remember_single_dependency(
            "_fluentqt_theme_source_record",
            source,
        )

    def addLightDismissPassthrough(self, widget):
        """Register a retained QWidget that receives dismissing presses."""

        if widget is None:
            return
        if widget is self:
            raise ValueError(
                "Popup cannot use itself as a light-dismiss passthrough"
            )
        self._addLightDismissPassthrough(widget)
        dependency_key = id(widget)
        if dependency_key in self._fluentqt_passthrough_records:
            return

        host_ref = weakref.ref(self)

        def forget_destroyed_passthrough(*_args):
            host = host_ref()
            if host is not None:
                host._fluentqt_passthrough_records.pop(
                    dependency_key,
                    None,
                )

        widget.destroyed.connect(forget_destroyed_passthrough)
        self._fluentqt_passthrough_records[dependency_key] = (
            widget,
            forget_destroyed_passthrough,
        )

    def clearLightDismissPassthrough(self):
        """Clear passthrough registrations and release retained wrappers."""

        self._clearLightDismissPassthrough()
        records = self._fluentqt_passthrough_records
        self._fluentqt_passthrough_records = {}
        for record in records.values():
            _disconnect_dependency(record)


class Popup(
    _NativePopup,
    _PopupDependencyFacade,
    metaclass=_PopupFacadeMeta,
):
    """Native same-window popup with caller-owned QWidget dependencies.

    Position anchors, theme sources, and light-dismiss passthrough widgets are
    observed rather than owned by C++. The facade keeps their Python wrappers
    alive while they are registered and releases each reference when the
    dependency, registration, or Popup is destroyed. It never changes QWidget
    parentage or Shiboken ownership.
    """

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._initialize_popup_dependency_facade()


class Flyout(_NativeFlyout, _PopupDependencyFacade):
    """Anchor-positioned Popup with a retained, caller-owned QWidget anchor."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._initialize_popup_dependency_facade()

    @staticmethod
    def _validate_flyout_anchor(anchor, operation):
        if anchor is None and operation == "showAt":
            raise TypeError("Flyout anchor must be a QWidget")
        if anchor is not None and not isinstance(anchor, QWidget):
            raise TypeError("Flyout anchor must be a QWidget")
        return anchor

    def setAnchor(self, anchor):
        """Set or clear the retained, caller-owned placement anchor."""

        self._validate_flyout_anchor(anchor, "setAnchor")
        if anchor is self:
            raise ValueError("Flyout cannot use itself as an anchor")
        self._setAnchor(anchor)
        self._remember_single_dependency(
            "_fluentqt_flyout_anchor_record",
            anchor,
        )

    def showAt(self, anchor):
        """Retain the placement anchor and open the native Flyout."""

        self._validate_flyout_anchor(anchor, "showAt")
        if anchor is self:
            raise ValueError("Flyout cannot use itself as an anchor")
        self._remember_single_dependency(
            "_fluentqt_flyout_anchor_record",
            anchor,
        )
        try:
            self._showAt(anchor)
        except Exception:
            record = self._fluentqt_flyout_anchor_record
            if record is not None and record[0] is anchor:
                self._remember_single_dependency(
                    "_fluentqt_flyout_anchor_record",
                    None,
                )
            raise


class CoachMark(_NativeCoachMark, _ObservedWidgetDependencyFacade):
    """Same-window coach mark with a retained, caller-owned target.

    The content host is owned by the native CoachMark. Children added to that
    host follow normal Qt parent-child lifetime rules. The target is observed
    by C++ and retained only at the Python-wrapper level.
    """

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._fluentqt_coach_mark_target_record = None
        host_ref = weakref.ref(self)

        def release_target(*_args):
            host = host_ref()
            if host is not None:
                record = host._fluentqt_coach_mark_target_record
                _disconnect_dependency(record)
                host._fluentqt_coach_mark_target_record = None

        self._fluentqt_coach_mark_destroyed_callback = release_target
        self.destroyed.connect(release_target)

    @staticmethod
    def _validate_target(target, host):
        if target is not None and not isinstance(target, QWidget):
            raise TypeError("CoachMark target must be a QWidget or None")
        if target is host:
            raise ValueError("CoachMark cannot use itself as a target")

    def setTarget(self, target):
        """Set or clear the retained, caller-owned target widget."""

        self._validate_target(target, self)
        self._setTarget(target)
        self._remember_single_dependency(
            "_fluentqt_coach_mark_target_record",
            target,
        )


class TeachingTip(_NativeTeachingTip, _PopupDependencyFacade):
    """Contextual same-window tip with a retained placement target.

    Popup dependencies and the TeachingTip target remain caller-owned. The
    facade retains their Python wrappers while native C++ handles placement,
    light dismiss, close reasons, painting, and the Qt-owned content host.
    """

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._initialize_popup_dependency_facade()
        self._fluentqt_teaching_tip_target_record = None

    def _release_dependency_records(self):
        super()._release_dependency_records()
        record = getattr(
            self,
            "_fluentqt_teaching_tip_target_record",
            None,
        )
        _disconnect_dependency(record)
        self._fluentqt_teaching_tip_target_record = None

    @staticmethod
    def _validate_teaching_target(target, operation, host):
        if target is None and operation == "showAt":
            raise TypeError("TeachingTip target must be a QWidget")
        if target is not None and not isinstance(target, QWidget):
            raise TypeError("TeachingTip target must be a QWidget or None")
        if target is host:
            raise ValueError("TeachingTip cannot use itself as a target")

    def setTarget(self, target):
        """Set or clear the retained, caller-owned placement target."""

        self._validate_teaching_target(target, "setTarget", self)
        self._setTarget(target)
        self._remember_single_dependency(
            "_fluentqt_teaching_tip_target_record",
            target,
        )

    def showAt(self, target):
        """Retain the target and open the native TeachingTip."""

        self._validate_teaching_target(target, "showAt", self)
        self._remember_single_dependency(
            "_fluentqt_teaching_tip_target_record",
            target,
        )
        try:
            self._showAt(target)
        except Exception:
            record = self._fluentqt_teaching_tip_target_record
            if record is not None and record[0] is target:
                self._remember_single_dependency(
                    "_fluentqt_teaching_tip_target_record",
                    None,
                )
            raise


class _DialogDependencyFacade(_ObservedWidgetDependencyFacade):
    """Retain Dialog's observed theme source without adopting it."""

    def _initialize_dialog_dependency_facade(self):
        self._fluentqt_dialog_theme_source_record = None
        host_ref = weakref.ref(self)

        def release_dependencies(*_args):
            host = host_ref()
            if host is not None:
                host._release_dialog_dependency_records()

        self._fluentqt_dialog_destroyed_callback = release_dependencies
        self.destroyed.connect(release_dependencies)

    def _release_dialog_dependency_records(self):
        record = getattr(
            self,
            "_fluentqt_dialog_theme_source_record",
            None,
        )
        _disconnect_dependency(record)
        self._fluentqt_dialog_theme_source_record = None

    def setThemeSource(self, source):
        """Use a retained, caller-owned QWidget as the local theme source."""

        if source is not None and not isinstance(source, QWidget):
            raise TypeError("Dialog theme source must be a QWidget or None")
        if source is self:
            raise ValueError("Dialog cannot use itself as a theme source")
        self._setThemeSource(source)
        self._remember_single_dependency(
            "_fluentqt_dialog_theme_source_record",
            source,
        )


class Dialog(
    _NativeDialog,
    _DialogDependencyFacade,
    metaclass=_DialogFacadeMeta,
):
    """Native same-window dialog with a retained local theme source."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._initialize_dialog_dependency_facade()


class ContentDialog(_NativeContentDialog, _DialogDependencyFacade):
    """WinUI-style same-window dialog with explicit content ownership.

    Installed content becomes a QObject child and is destroyed with the
    ContentDialog. Replacing it or passing ``None`` detaches the previous
    widget. ``takeContent()`` explicitly returns that parentless widget to
    Python while preserving its wrapper and subclass state.
    """

    ResultNone = 0
    ResultPrimary = 1
    ResultSecondary = 2

    def __init__(self, *args, **kwargs):
        content = kwargs.pop("content", _CONTENT_UNSET)
        super().__init__(*args, **kwargs)
        self._initialize_dialog_dependency_facade()
        self._fluentqt_content_record = None
        if content is not _CONTENT_UNSET:
            self.setContent(content)

    def _release_dialog_dependency_records(self):
        super()._release_dialog_dependency_records()
        record = getattr(self, "_fluentqt_content_record", None)
        _disconnect_dependency(record)
        self._fluentqt_content_record = None

    @staticmethod
    def _validate_content(widget, host):
        if widget is not None and not isinstance(widget, QWidget):
            raise TypeError("ContentDialog content must be a QWidget or None")
        if widget is host or (
            widget is not None and widget.isAncestorOf(host)
        ):
            raise ValueError(
                "ContentDialog content cannot be the dialog or its ancestor"
            )

    def setContent(self, widget):
        """Install content that is destroyed with the ContentDialog."""

        self._validate_content(widget, self)
        current = super().content()
        if widget is current:
            self._remember_single_dependency(
                "_fluentqt_content_record",
                widget,
            )
            return

        if current is not None:
            current.setParent(None)
        if widget is not None and widget.parent() is not None:
            widget.setParent(None)

        super()._setContent(widget)
        self._remember_single_dependency(
            "_fluentqt_content_record",
            widget,
        )

    def takeContent(self):
        """Detach and return the installed content to Python ownership."""

        widget = super().content()
        if widget is None:
            return None
        self.setContent(None)
        widget.setParent(None)
        return widget


__all__ = [
    "CoachMark",
    "ContentDialog",
    "ContentDialogButton",
    "Dialog",
    "Flyout",
    "Popup",
    "TeachingTip",
]
