"""Non-invasive selection of explicitly registered regions."""

import shiboken6
import fluentqt
from PySide6.QtCore import QEvent, QObject, QPoint, QRect, QTimer, Qt, Signal
from PySide6.QtGui import QPainter, QPen
from PySide6.QtWidgets import QApplication, QWidget


class SelectionOutline(fluentqt.FluentWidget):
    """A transparent development overlay, preserving the pixels being edited."""

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setPen(QPen(fluentqt.accent_color(), 1))
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.drawRect(self.rect().adjusted(0, 0, -1, -1))


class RegionPicker(QObject):
    pickingChanged = Signal(bool)

    def __init__(self, editor):
        super().__init__(editor)
        self.editor = editor
        self.picking = False
        self.swallow_release = False
        self.band = None
        self.band_root = None
        QApplication.instance().installEventFilter(self)
        editor.changed.connect(self.refresh)
        self.timer = QTimer(self)
        self.timer.setInterval(120)
        self.timer.timeout.connect(self.refresh)
        self.timer.start()

    def set_picking(self, enabled):
        self.picking = bool(enabled) and not self.editor.history.pending
        self.pickingChanged.emit(self.picking)

    def hide_highlight(self):
        if self.band is not None and shiboken6.isValid(self.band):
            self.band.hide()

    def refresh(self):
        root = self.editor.window.scene_widget
        if self.band_root is not root:
            self.hide_highlight()
            self.band_root = root
            self.band = SelectionOutline(root)
            self.band.setAttribute(Qt.WidgetAttribute.WA_TransparentForMouseEvents)
            self.band.setObjectName("previewEditSelectionOutline")
        region = self.editor.regions().get(self.editor.selected)
        if not region or not region.widget.isVisible() or self.editor.busy:
            self.hide_highlight()
            return
        self.band.setGeometry(QRect(region.widget.mapTo(root, QPoint()), region.widget.size()))
        self.band.show()
        self.band.raise_()

    def eventFilter(self, watched, event):
        if event.type() == QEvent.Type.KeyPress and self.picking and event.key() == Qt.Key.Key_Escape:
            self.set_picking(False)
            return True
        if event.type() == QEvent.Type.MouseButtonRelease and self.swallow_release:
            if event.button() == Qt.MouseButton.LeftButton:
                self.swallow_release = False
                return True
        if (not self.picking or event.type() != QEvent.Type.MouseButtonPress
                or event.button() != Qt.MouseButton.LeftButton or not isinstance(watched, QWidget)):
            return False
        regions = self.editor.regions()
        current = watched
        while current is not None:
            found = next((region for region in regions.values() if region.widget is current), None)
            if found:
                self.editor.select(found.key)
                self.swallow_release = True
                self.set_picking(False)
                return True
            if current is self.editor.window.scene_widget:
                break
            current = current.parentWidget()
        return False
