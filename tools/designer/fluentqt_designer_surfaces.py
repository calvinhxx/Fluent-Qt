"""Form anchors for FluentQt components that open on demand."""
from importlib import import_module
from weakref import ref

from PySide6.QtCore import Property, QEvent, QPoint, QTimer, Signal, Slot
from PySide6.QtGui import QAction, QHelpEvent
from PySide6.QtWidgets import QApplication, QVBoxLayout
from shiboken6 import isValid

from fluentqt.basicinput import Button
from fluentqt.textfields import Label
from fluentqt.windowing import Window


class SurfaceButton(Button):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._surface = None
        self._host = None
        self.clicked.connect(self.openSurface)

    def surface(self):
        host = self.window()
        if self._surface is not None and isValid(self._surface):
            if self._host() is host or self._entry["surface"] == "ToolTip":
                return self._surface
            self._surface.hide()
            self._surface.deleteLater()
        entry = self._entry
        kind = entry["surface"]
        cls = getattr(import_module(entry["python"]), entry["cpp"].split("::")[-1])
        if kind == "ToolTip":
            surface = cls.attach(self, self._text)
        elif kind == "Menu":
            surface = cls(self.text(), self)
        elif kind == "SplashScreen":
            surface = cls(host.contentHost() if isinstance(host, Window) else host)
        else:
            surface = cls(self)
        self._surface = surface
        self._host = ref(host)
        self.destroyed.connect(surface.deleteLater)
        self._configure(surface)
        return surface

    @Slot()
    def openSurface(self):
        surface = self.surface()
        kind = self._entry["surface"]
        if kind == "Popup":
            surface.setPosition(self, QPoint(0, self.height()))
            surface.open()
        elif kind in ("Flyout", "TeachingTip", "CommandBarFlyout"):
            surface.showAt(self)
        elif kind == "CoachMark":
            surface.setTarget(self)
            surface.open()
        elif kind == "Toast":
            surface.present(self)
        elif kind == "SplashScreen":
            surface.show()
            surface.raise_()
            if self._timer.interval() > 0:
                self._timer.start()
        elif kind == "ToolTip":
            point = self.rect().center()
            QApplication.sendEvent(self, QHelpEvent(QEvent.Type.ToolTip, point, self.mapToGlobal(point)))
        elif kind == "Menu":
            surface.popup(self.mapToGlobal(QPoint(0, self.height())))
        else:
            surface.open()


class MessageButton(SurfaceButton):
    surfaceTextChanged = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._text = self.tr("Message")
        if self._entry["surface"] == "ToolTip":
            self.surface()

    @Property(str, notify=surfaceTextChanged)
    def surfaceText(self):
        return self._text

    @surfaceText.setter
    def surfaceText(self, text):
        if self._text != text:
            self._text = text
            if self._surface is not None and isValid(self._surface) and isValid(self._text_target):
                self._set_surface_text(text)
            self.surfaceTextChanged.emit(text)

    def _configure(self, surface):
        kind = self._entry["surface"]
        self._text_target = surface
        if kind == "Toast":
            setter = surface.setMessage
        elif kind == "ToolTip":
            setter = surface.setText
        elif kind == "SplashScreen":
            setter = surface.setTitle
        else:
            host = surface.contentHost() if kind in ("TeachingTip", "CoachMark") else surface
            label = Label(host)
            label.setWordWrap(True)
            self._text_target = label
            setter = label.setText
            if kind == "DrawerView":
                surface.setOwnedContentWidget(label)
            else:
                QVBoxLayout(host).addWidget(label)
                if host is surface:
                    surface.resize(240, 96)
        setter(self._text)
        self._set_surface_text = setter

    def setSurfaceText(self, text):
        self.surfaceText = text


class TimedMessageButton(MessageButton):
    durationChanged = Signal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._duration = 2200

    @Property(int, notify=durationChanged)
    def duration(self):
        return self._duration

    @duration.setter
    def duration(self, milliseconds):
        milliseconds = max(0, milliseconds)
        if self._duration == milliseconds:
            return
        self._duration = milliseconds
        if self._surface is not None and isValid(self._surface):
            self._apply_duration(self._surface)
        self.durationChanged.emit(milliseconds)

    def setDuration(self, milliseconds):
        self.duration = milliseconds

    def _apply_duration(self, surface):
        if self._entry["surface"] == "Toast":
            surface.setDuration(self._duration)
        else:
            self._timer.setInterval(self._duration)
            if self._duration > 0 and surface.isVisible():
                self._timer.start()
            else:
                self._timer.stop()

    def _configure(self, surface):
        super()._configure(surface)
        if self._entry["surface"] == "SplashScreen":
            self._timer = QTimer(surface)
            self._timer.setSingleShot(True)
            self._timer.timeout.connect(surface.dismiss)
        self._apply_duration(surface)


class _FormAction(QAction):
    textTriggered = Signal(str)

    def __init__(self, text, parent):
        super().__init__(text, parent)
        self.triggered.connect(self._emit_text)

    @Slot()
    def _emit_text(self):
        self.textTriggered.emit(self.text())


class CommandsButton(SurfaceButton):
    actionTriggered = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._texts = [self.tr("Action")]
        self._actions = []

    @Property("QStringList")
    def actionTexts(self):
        return list(self._texts)

    @actionTexts.setter
    def actionTexts(self, texts):
        if list(texts) != self._texts:
            self._texts = list(texts)
            if self._surface is not None and isValid(self._surface):
                self._configure(self._surface)

    def setActionTexts(self, texts):
        self.actionTexts = texts

    def _configure(self, surface):
        kind = self._entry["surface"]
        for action in self._actions:
            if isValid(action):
                if action.parent() is surface:
                    if kind == "CommandPalette":
                        surface.removeCommand(action)
                    else:
                        surface.removeAction(action)
                action.deleteLater()
        self._actions = []
        for text in self._texts:
            action = _FormAction(text, surface)
            action.textTriggered.connect(self.actionTriggered)
            if kind == "CommandPalette":
                surface.addCommand(action)
            else:
                surface.addAction(action)
            self._actions.append(action)
