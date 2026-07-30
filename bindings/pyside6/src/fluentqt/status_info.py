"""Status and information components."""

from . import _fluentqt as _native

Avatar = _native.fluent.Avatar
InfoBadge = _native.fluent.InfoBadge
_NativeInfoBar = _native.fluent.InfoBar
ProgressBar = _native.fluent.ProgressBar
ProgressRing = _native.fluent.ProgressRing
Shimmer = _native.fluent.Shimmer
_ACTION_UNSET = object()


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
]
