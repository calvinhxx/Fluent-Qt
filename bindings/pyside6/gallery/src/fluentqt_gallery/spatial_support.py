"""Optional Spatial discovery and the Gallery's shared mode binding.

Importing the 2D Gallery must not import QtOpenGL or QtOpenGLWidgets.
"""

from importlib.util import find_spec

import fluentqt
from PySide6.QtCore import QObject, Qt, Slot
from PySide6.QtGui import QColor


SPATIAL_AVAILABLE = find_spec("fluentqt.spatial") is not None


class SpatialSupportBadge(fluentqt.FluentWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        from .settings import gallery_settings

        self._settings = gallery_settings()
        self.setAccessibleName("3D support")
        self.badge = fluentqt.InfoBadge(self)
        self.badge.setDisplayMode(fluentqt.InfoBadge.InfoBadgeDisplayMode.Icon)
        self.badge.setIconGlyph(fluentqt.Typography.Icons.Info)
        self.badge.setIconGlyphSize(fluentqt.Typography.IconSize.Standard)
        self.badge.setBadgeHeight(24)
        self.badge.setFixedSize(24, 24)
        self.badge.setAttribute(Qt.WA_TransparentForMouseEvents)
        self.setFixedSize(24, 24)
        self.badge.setCustomBackgroundColor(QColor(Qt.transparent))
        self.setAttribute(Qt.WA_NoMousePropagation)
        self._settings.spatialAvailabilityChanged.connect(self.refresh)
        self.refresh()

    def on_theme_updated(self):
        super().on_theme_updated()
        if hasattr(self, "_settings"):
            self.refresh()

    @Slot()
    def refresh(self):
        settings = self._settings
        fallback = not settings.spatial_availability_pending and not settings.spatial_available
        self.badge.setStatus(fluentqt.InfoBadge.InfoBadgeStatus.Critical if fallback
                            else fluentqt.InfoBadge.InfoBadgeStatus.Informational)
        colors = self.theme_tokens().colors
        self.badge.setCustomTextColor(colors.systemCritical if fallback else colors.textSecondary)
        text = ("3D is unavailable on this device.\n" + settings.spatial_unavailable_reason
                if fallback else "3D requires a GPU with hardware acceleration.")
        self.setAccessibleDescription(text)
        fluentqt.ToolTip.attach(self, text)


class SpatialPreviewBinding(QObject):
    """Every sample follows Settings, including lazy and hidden previews."""

    def __init__(self, view):
        super().__init__(view)
        from .settings import gallery_settings

        self.view = view
        self.settings = gallery_settings()
        self.synchronizing = False
        self.settings.spatialModeEnabledChanged.connect(self.synchronize)
        self.settings.spatialAvailabilityChanged.connect(self.synchronize)
        self.view.spatialEnabledChanged.connect(self.mode_changed)
        self.synchronize()

    @Slot()
    def synchronize(self):
        self.synchronizing = True
        self.view.setSpatialEnabled(self.settings.spatial_available
                                    and self.settings.spatial_mode_enabled)
        self.synchronizing = False

    @Slot(bool)
    def mode_changed(self, enabled):
        if not self.synchronizing and not enabled:
            self.settings.set_spatial_mode_enabled(False)
