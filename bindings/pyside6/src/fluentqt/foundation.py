"""Theme and typography helpers for FluentQt bindings."""

from PySide6.QtCore import QByteArray, QMargins, QObject, Signal
from PySide6.QtWidgets import QWidget

from . import _fluentqt as _native
from ._fluentqt import (
    FontRole,
    accentColor,
    applyUserTheme,
    currentMotionMode as _nativeCurrentMotionMode,
    currentTheme,
    fontScale,
    fontForRole,
    resolvedMotionDuration as _nativeResolvedMotionDuration,
    resetThemeTokens,
    setAccentColor,
    setFontScale,
    setMotionMode as _nativeSetMotionMode,
    setTheme,
    shouldAnimateMotion as _nativeShouldAnimateMotion,
    themeRevision,
    themeUsesDarkAppearance,
)
from .design import ThemeTokens

Theme = _native.fluent.Theme
MotionMode = _native.fluent.MotionMode
MotionKind = _native.fluent.MotionKind
FontIcon = _native.fluent.FontIcon
AnchorEdge = _native.fluent.AnchorEdge
AnchorLayout = _native.fluent.AnchorLayout
AnchorSpec = _native.fluent.AnchorSpec
BindingMode = _native.fluent.BindingMode
_NativeFluentWidget = _native.fluent.FluentWidget
_NativeStateGroup = _native.fluent.StateGroup


class MotionPolicy(QObject):
    """Observable Python facade over FluentQt's process-wide motion policy."""

    Mode = MotionMode
    Kind = MotionKind
    modeChanged = Signal(object)
    _instance = None
    _initialized = False

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def __init__(self):
        if self._initialized:
            return
        super().__init__()
        self._initialized = True

    def mode(self):
        """Return the active Full, Reduced, or Disabled preference."""

        return _nativeCurrentMotionMode()

    def setMode(self, mode):
        """Set the global preference and emit once when it changes."""

        try:
            normalized = MotionMode(mode)
        except (TypeError, ValueError) as error:
            raise ValueError("Unsupported FluentQt motion mode") from error
        if normalized not in (
            MotionMode.Full,
            MotionMode.Reduced,
            MotionMode.Disabled,
        ):
            raise ValueError("Unsupported FluentQt motion mode")
        previous = self.mode()
        _nativeSetMotionMode(normalized)
        current = self.mode()
        if current != previous:
            self.modeChanged.emit(current)

    def shouldAnimate(
        self,
        local_animation_enabled=True,
        kind=MotionKind.Transition,
    ):
        """Return whether the requested transition or continuous motion may run."""

        if not isinstance(local_animation_enabled, bool):
            raise TypeError("local_animation_enabled must be bool")
        try:
            normalized_kind = MotionKind(kind)
        except (TypeError, ValueError) as error:
            raise ValueError("Unsupported FluentQt motion kind") from error
        if normalized_kind not in (
            MotionKind.Transition,
            MotionKind.Continuous,
        ):
            raise ValueError("Unsupported FluentQt motion kind")
        return _nativeShouldAnimateMotion(
            local_animation_enabled,
            normalized_kind,
        )

    def resolvedDuration(self, full_duration_ms, local_animation_enabled=True):
        """Resolve a full transition duration through the native policy."""

        if not isinstance(full_duration_ms, int) or isinstance(
            full_duration_ms, bool
        ):
            raise TypeError("full_duration_ms must be int")
        if not isinstance(local_animation_enabled, bool):
            raise TypeError("local_animation_enabled must be bool")
        return _nativeResolvedMotionDuration(
            full_duration_ms,
            local_animation_enabled,
        )


_MOTION_POLICY = MotionPolicy()


def motion_policy():
    """Return FluentQt's observable process-wide motion policy."""

    return _MOTION_POLICY


class FluentWidget(_NativeFluentWidget):
    """QWidget base for Python-authored, theme-aware Fluent components."""

    def _onThemeUpdated(self):
        """Bridge the native FluentElement hook to the Python override."""

        self.on_theme_updated()

    def on_theme_updated(self):
        """Refresh state derived from Fluent design tokens after a theme change."""

        self.update()

    def theme_tokens(self):
        """Return the complete effective Fluent design-token snapshot."""

        return ThemeTokens(self._themeTokens())

    def theme_font(self, role=FontRole.Body):
        """Return the effective Fluent font for a semantic typography role."""

        return self._themeFont(role)

    def effective_theme(self):
        """Return the Light/Dark theme inherited by this widget."""

        return self._effectiveTheme()

class StateGroup(_NativeStateGroup):
    """Named property bundles with default restoration and safe QObject targets."""

    state_changed = Signal(str)

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._fluentqt_state_names = set()

    def add(self, name, changes):
        """Add or replace a state from ``{QObject: {property: value}}``."""

        if not isinstance(name, str) or not name:
            raise ValueError("State name must be a non-empty string")
        if not hasattr(changes, "items"):
            raise TypeError("State changes must be a mapping")

        staged_changes = []
        for target, properties in changes.items():
            if not isinstance(target, QObject):
                raise TypeError("State targets must be QObject instances")
            if not hasattr(properties, "items"):
                raise TypeError("Each state target must map properties to values")
            for property_name, value in properties.items():
                if not isinstance(property_name, str) or not property_name:
                    raise ValueError("State property names must be non-empty strings")
                meta_object = target.metaObject()
                property_index = meta_object.indexOfProperty(property_name)
                dynamic_name = QByteArray(property_name.encode("utf-8"))
                if (
                    property_index < 0
                    and dynamic_name not in target.dynamicPropertyNames()
                ):
                    raise ValueError(
                        "State property is unknown: {0}".format(property_name)
                    )
                if (
                    property_index >= 0
                    and not meta_object.property(property_index).isWritable()
                ):
                    raise ValueError(
                        "State property is read-only: {0}".format(property_name)
                    )
                staged_changes.append((target, property_name, value))

        active = self.state() == name
        if active:
            self.set("")
        self._clearStateDefinition(name)
        for target, property_name, value in staged_changes:
            try:
                if not self._addStateChange(
                    name, target, property_name, value
                ):
                    raise ValueError(
                        "State property is unknown or read-only: {0}".format(
                            property_name
                        )
                    )
            except Exception:
                self._clearStateDefinition(name)
                self._fluentqt_state_names.discard(name)
                raise
        self._fluentqt_state_names.add(name)
        if active:
            self.set(name)
        return self

    def set(self, name=""):
        """Apply a named state, or restore defaults for an empty name."""

        if name and name not in self._fluentqt_state_names:
            raise KeyError("Unknown FluentQt state: {0}".format(name))
        previous = self.state()
        self._setState(name)
        current = self.state()
        if current != previous:
            self.state_changed.emit(current)

    def clear(self):
        """Restore all properties changed by the active state."""

        self.set("")

    def has(self, name):
        """Return whether a named state definition exists."""

        return name in self._fluentqt_state_names


def bind(
    source,
    source_property,
    target,
    target_property,
    mode=BindingMode.OneWay,
):
    """Bind one Qt property to another using their notify signals."""

    if not isinstance(source, QObject) or not isinstance(target, QObject):
        raise TypeError("Property binding endpoints must be QObject instances")
    if not isinstance(source_property, str) or not isinstance(target_property, str):
        raise TypeError("Property binding names must be strings")
    try:
        binding_mode = BindingMode(mode)
    except (TypeError, ValueError):
        raise ValueError("Unsupported FluentQt binding mode")
    if not _native.bindProperties(
        source,
        source_property,
        target,
        target_property,
        binding_mode,
    ):
        raise ValueError(
            "Unable to bind {0} to {1}; verify readable, writable, and notify "
            "properties".format(source_property, target_property)
        )


_ANCHOR_EDGES = {
    "left": AnchorEdge.Left,
    "right": AnchorEdge.Right,
    "top": AnchorEdge.Top,
    "bottom": AnchorEdge.Bottom,
    "horizontal_center": AnchorEdge.HorizontalCenter,
    "vertical_center": AnchorEdge.VerticalCenter,
}


def _anchor_relation(value, default_edge):
    if isinstance(value, QWidget):
        return value, default_edge, 0
    if not isinstance(value, (tuple, list)):
        raise TypeError("Anchor relation must be a QWidget or tuple")
    if len(value) == 2:
        target, offset = value
        edge = default_edge
    elif len(value) == 3:
        target, edge, offset = value
        edge = AnchorEdge(edge)
    else:
        raise ValueError("Anchor relation must contain target, edge?, and offset")
    if not isinstance(target, QWidget):
        raise TypeError("Anchor target must be a QWidget")
    return target, edge, int(offset)


def _fill_margins(value):
    if value is True or value is None:
        return QMargins()
    if isinstance(value, QMargins):
        return value
    if isinstance(value, int):
        return QMargins(value, value, value, value)
    if isinstance(value, (tuple, list)) and len(value) == 4:
        return QMargins(*(int(part) for part in value))
    raise TypeError("fill must be True, int, QMargins, or four margins")


def anchors(
    *,
    left=None,
    right=None,
    top=None,
    bottom=None,
    horizontal_center=None,
    vertical_center=None,
    center_in=None,
    top_right=None,
    fill=False
):
    """Build an AnchorSpec using QWidget targets and optional offsets."""

    spec = AnchorSpec()
    relations = {
        "left": left,
        "right": right,
        "top": top,
        "bottom": bottom,
        "horizontal_center": horizontal_center,
        "vertical_center": vertical_center,
    }
    if center_in is not None:
        if horizontal_center is not None or vertical_center is not None:
            raise ValueError("center_in cannot be combined with center anchors")
        relations["horizontal_center"] = center_in
        relations["vertical_center"] = center_in
    if top_right is not None:
        if top is not None or right is not None:
            raise ValueError("top_right cannot be combined with top or right")
        if isinstance(top_right, QWidget):
            target, margin = top_right, 0
        elif isinstance(top_right, (tuple, list)) and len(top_right) == 2:
            target, margin = top_right
        else:
            raise TypeError("top_right must be a QWidget or (QWidget, margin)")
        if not isinstance(target, QWidget):
            raise TypeError("top_right target must be a QWidget")
        relations["top"] = (target, int(margin))
        relations["right"] = (target, -int(margin))

    active_relations = [value for value in relations.values() if value is not None]
    if fill is not False:
        if active_relations:
            raise ValueError("fill cannot be combined with edge anchors")
        spec.setFill(_fill_margins(fill))
        return spec

    for name, value in relations.items():
        if value is None:
            continue
        source_edge = _ANCHOR_EDGES[name]
        target, target_edge, offset = _anchor_relation(value, source_edge)
        spec.setAnchor(source_edge, target, target_edge, offset)
    return spec


def font_for_role(role=FontRole.Body):
    """Return the Fluent application font for a typography role."""
    return fontForRole(role)


def set_theme(theme):
    """Set the global Light, Dark, or HighContrast visual theme."""
    setTheme(theme)


def current_theme():
    """Return the active global visual theme."""
    return currentTheme()


def theme_uses_dark_appearance(theme):
    """Return whether a visual theme uses dark-backed chrome."""
    return themeUsesDarkAppearance(theme)


def set_motion_mode(mode):
    """Set the global Full, Reduced, or Disabled motion preference."""
    motion_policy().setMode(mode)


def current_motion_mode():
    """Return the active global motion preference."""
    return motion_policy().mode()


def setMotionMode(mode):
    """Set the global motion preference using the Qt-style spelling."""

    motion_policy().setMode(mode)


def currentMotionMode():
    """Return the global motion preference using the Qt-style spelling."""

    return motion_policy().mode()


def apply_user_theme():
    """Load the user-editable Fluent token overrides, if present."""
    applyUserTheme()


def set_accent_color(color):
    """Apply an in-memory accent color override."""
    setAccentColor(color)


def accent_color():
    """Return the accent color for the active visual theme."""
    return accentColor()


def reset_theme_tokens():
    """Restore built-in Fluent tokens without changing the visual mode."""
    resetThemeTokens()


def set_font_scale(scale):
    """Set the runtime Fluent typography scale."""
    setFontScale(scale)


def font_scale():
    """Return the runtime Fluent typography scale."""
    return fontScale()


def theme_revision():
    """Return the token-registry revision counter."""
    return themeRevision()


__all__ = [
    "AnchorEdge",
    "AnchorLayout",
    "AnchorSpec",
    "BindingMode",
    "FluentWidget",
    "FontIcon",
    "FontRole",
    "MotionKind",
    "MotionMode",
    "MotionPolicy",
    "StateGroup",
    "Theme",
    "accent_color",
    "accentColor",
    "anchors",
    "apply_user_theme",
    "applyUserTheme",
    "bind",
    "current_motion_mode",
    "currentMotionMode",
    "current_theme",
    "currentTheme",
    "font_scale",
    "font_for_role",
    "fontScale",
    "fontForRole",
    "motion_policy",
    "reset_theme_tokens",
    "resetThemeTokens",
    "set_accent_color",
    "set_font_scale",
    "set_motion_mode",
    "set_theme",
    "setAccentColor",
    "setFontScale",
    "setMotionMode",
    "setTheme",
    "theme_revision",
    "themeRevision",
    "theme_uses_dark_appearance",
    "themeUsesDarkAppearance",
]
