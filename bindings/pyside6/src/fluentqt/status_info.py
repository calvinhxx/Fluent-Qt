"""Status and information components."""

from dataclasses import dataclass
from enum import IntEnum
import json

from PySide6.QtCore import QMargins, QRectF
from PySide6.QtWidgets import QWidget

from . import _fluentqt as _native

Avatar = _native.fluent.Avatar
InfoBadge = _native.fluent.InfoBadge
_NativeInfoBar = _native.fluent.InfoBar
ProgressBar = _native.fluent.ProgressBar
ProgressRing = _native.fluent.ProgressRing
_NativeShimmer = _native.fluent.Shimmer
_NativeToast = _native.fluent.Toast
ToolTip = _native.fluent.ToolTip
_ACTION_UNSET = object()


class _ShimmerShape(IntEnum):
    Rectangle = 0
    RoundedRect = 1
    Circle = 2
    Line = 3


@dataclass(frozen=True)
class _ShimmerElement:
    """One custom Shimmer skeleton shape in local widget coordinates."""

    shape: _ShimmerShape
    rect: QRectF
    radius: float = -1.0

    def __post_init__(self):
        object.__setattr__(self, "shape", _ShimmerShape(self.shape))
        if not isinstance(self.rect, QRectF):
            raise TypeError("Shimmer.Element rect must be a QRectF")
        object.__setattr__(self, "rect", QRectF(self.rect))
        object.__setattr__(self, "radius", float(self.radius))


class Shimmer(_NativeShimmer):
    """Skeleton placeholder with Python-authored custom elements."""

    Shape = _ShimmerShape
    Element = _ShimmerElement

    def __init__(self, *args, **kwargs):
        elements = kwargs.pop("elements", None)
        super().__init__(*args, **kwargs)
        if elements is not None:
            self.setElements(elements)

    @staticmethod
    def _element_payload(element):
        if not isinstance(element, _ShimmerElement):
            raise TypeError(
                "Shimmer elements must be fluentqt.Shimmer.Element values"
            )
        return {
            "shape": int(element.shape),
            "x": element.rect.x(),
            "y": element.rect.y(),
            "width": element.rect.width(),
            "height": element.rect.height(),
            "radius": float(element.radius),
        }

    def elements(self):
        values = _native.shimmerElementsForBinding(self)
        return [
            _ShimmerElement(
                _ShimmerShape(int(value["shape"])),
                QRectF(value["rect"]),
                float(value["radius"]),
            )
            for value in values
        ]

    def setElements(self, elements):
        values = list(elements)
        payload = json.dumps(
            [self._element_payload(element) for element in values],
            separators=(",", ":"),
        )
        if not _native.setShimmerElementsJsonForBinding(self, payload):
            raise RuntimeError("FluentQt rejected the Shimmer element payload")

    def clearElements(self):
        _native.clearShimmerElementsForBinding(self)


class _ToastFacadeMeta(type(_NativeToast)):
    """Preserve Toast identity for native managed-factory results."""

    def __instancecheck__(cls, instance):
        return isinstance(instance, _NativeToast)

    def __subclasscheck__(cls, subclass):
        return issubclass(subclass, _NativeToast)


def _toast_host(anchor):
    if not isinstance(anchor, QWidget):
        raise TypeError("Toast anchor must be a QWidget")
    host = anchor.window()
    if host is None:
        raise ValueError("Toast anchor has no top-level window")
    return host


def _toast_margins(margins):
    return QMargins(16, 16, 16, 16) if margins is None else margins


class Toast(_NativeToast, metaclass=_ToastFacadeMeta):
    """Same-window notification with explicit top-level host bookkeeping.

    Native Toast placement and local-theme inheritance still use the supplied
    anchor. The facade separately resolves its top-level window so PySide and
    Qt agree on the parent of direct and self-deleting managed toasts.
    """

    def present(self, anchor):
        """Present this toast in the anchor's top-level window."""

        host = _toast_host(anchor)
        if host is self or self.isAncestorOf(host):
            raise ValueError("Toast cannot use itself or its child as anchor")
        if self.parentWidget() is not host:
            self.setParent(host)
        return super()._present(anchor)

    @staticmethod
    def showToast(
        anchor,
        message,
        severity=_NativeToast.Severity.Informational,
        durationMs=2200,
        placement=_NativeToast.Placement.Top,
        margins=None,
    ):
        """Create a host-owned toast that deletes itself after dismissal."""

        host = _toast_host(anchor)
        return _native.showToastForBinding(
            host,
            anchor,
            message,
            severity,
            durationMs,
            placement,
            _toast_margins(margins),
        )

    @staticmethod
    def showOrUpdateToast(
        anchor,
        updateKey,
        message,
        severity=_NativeToast.Severity.Informational,
        durationMs=2200,
        placement=_NativeToast.Placement.Top,
        margins=None,
    ):
        """Create or update one managed toast in the host stack."""

        host = _toast_host(anchor)
        return _native.showOrUpdateToastForBinding(
            host,
            anchor,
            updateKey,
            message,
            severity,
            durationMs,
            placement,
            _toast_margins(margins),
        )


class InfoBar(_NativeInfoBar):
    """Inline notification with an explicitly retained action widget.

    The action is parented to the InfoBar while installed. Replacing or
    clearing it releases the previous widget as a parentless Python-owned
    object; destroying the InfoBar deletes the currently installed action.
    ``takeActionWidget()`` releases and returns the current action explicitly.
    """

    _fluentqt_action_widget = None

    def __init__(self, *args, **kwargs):
        action = kwargs.pop("actionWidget", _ACTION_UNSET)
        super().__init__(*args, **kwargs)
        self._fluentqt_action_widget = None
        if action is not _ACTION_UNSET:
            self.setActionWidget(action)

    def actionWidget(self):
        widget = super().actionWidget()
        if widget is None:
            self._fluentqt_action_widget = None
        return widget

    def setActionWidget(self, widget):
        """Install an action using InfoBar's hosted-widget contract."""

        if widget is self or (
            widget is not None and widget.isAncestorOf(self)
        ):
            raise ValueError(
                "InfoBar action cannot be the host or its ancestor"
            )

        current = super().actionWidget()
        if widget is current:
            self._fluentqt_action_widget = widget
            return

        if widget is not None and widget.parent() is not None:
            # Clear PySide's previous parent bookkeeping before native C++
            # installs the widget into the InfoBar's Qt parent chain.
            widget.setParent(None)

        super()._setActionWidget(widget)
        self._fluentqt_action_widget = widget

    def takeActionWidget(self):
        """Release and return the current action as Python-owned content."""

        widget = super().actionWidget()
        if widget is None:
            self._fluentqt_action_widget = None
            return None

        super()._setActionWidget(None)
        widget.setParent(None)
        self._fluentqt_action_widget = None
        return widget

__all__ = [
    "Avatar",
    "InfoBadge",
    "InfoBar",
    "ProgressBar",
    "ProgressRing",
    "Shimmer",
    "Toast",
    "ToolTip",
]
