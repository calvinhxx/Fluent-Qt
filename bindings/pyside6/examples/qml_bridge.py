"""One application model shared by Fluent Widgets and the native QML bridge."""

from __future__ import annotations

import argparse
from importlib.resources import files
from pathlib import Path
import sys
import time

import fluentqt
import shiboken6
from fluentqt._qt_compat import delete_qobject
from fluentqt.qmlbridge import QmlSession, QmlView
from PySide6.QtCore import QCoreApplication, QEvent, QMetaObject, QObject, Property, QTimer, QUrl, Signal, Slot, Qt
from PySide6.QtWidgets import QApplication, QHBoxLayout, QStackedLayout, QVBoxLayout, QWidget


class PreviewModel(QObject):
    titleChanged = Signal()
    progressChanged = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._title = "A shared canvas"
        self._progress = 40

    def _get_title(self):
        return self._title

    @Slot(str)
    def setTitle(self, value):
        if self._title != value:
            self._title = value
            self.titleChanged.emit()

    def _get_progress(self):
        return self._progress

    @Slot(int)
    def setProgress(self, value):
        value = max(0, min(100, value))
        if self._progress != value:
            self._progress = value
            self.progressChanged.emit()

    title = Property(str, _get_title, setTitle, notify=titleChanged)
    progress = Property(int, _get_progress, setProgress, notify=progressChanged)


def preview_source():
    """Use the one Preview.qml shared with the C++ example and Gallery."""
    if "__file__" not in globals():
        return QUrl.fromLocalFile(str(files("fluentqt_gallery").joinpath("assets/qml_bridge/Preview.qml")))
    packaged = Path(__file__).resolve().parent / "assets" / "qml_bridge" / "Preview.qml"
    source = (packaged if packaged.is_file() else
              Path(__file__).resolve().parents[3] / "examples" / "qml_bridge" / "qml" / "Preview.qml")
    return QUrl.fromLocalFile(str(source))


class QmlBridgePanel(QWidget):
    stateChanged = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("qmlBridgePanel")
        column = QVBoxLayout(self)
        column.setContentsMargins(24, 24, 24, 24)
        column.setSpacing(12)
        heading = QWidget(self)
        heading_layout = QVBoxLayout(heading)
        heading_layout.setContentsMargins(0, 0, 0, 0)
        heading_layout.setSpacing(4)
        title = fluentqt.Label("One model, two views", heading)
        title.setFluentTypography(fluentqt.FontRole.Title)
        title.setWordWrap(True)
        description = fluentqt.Label("Edit the Widgets form or advance the QML preview.", heading)
        description.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
        description.setWordWrap(True)
        heading_layout.addWidget(title)
        heading_layout.addWidget(description)
        column.addWidget(heading)

        self.model = PreviewModel(self)
        self.model.setObjectName("qmlBridgeModel")
        self.session = QmlSession(self, self)
        self.session.setObjectName("qmlBridgeSession")
        self.view = None
        self._error = ""

        form = fluentqt.Card(self)
        form.setObjectName("qmlBridgeForm")
        fields = QVBoxLayout(form)
        fields.setContentsMargins(16, 16, 16, 16)
        fields.setSpacing(8)
        form_title = fluentqt.Label("Widgets form", form)
        form_title.setFluentTypography(fluentqt.FontRole.BodyStrong)
        fields.addWidget(form_title)
        title_label = fluentqt.Label("Title", form)
        fields.addWidget(title_label)
        self.title_edit = fluentqt.LineEdit(form)
        self.title_edit.setObjectName("qmlBridgeTitleEdit")
        self.title_edit.setAccessibleName("Preview title")
        self.title_edit.setText(self.model.title)
        title_label.setBuddy(self.title_edit)
        fields.addWidget(self.title_edit)
        progress_row = QHBoxLayout()
        progress_row.setSpacing(8)
        progress_row.addWidget(fluentqt.Label("Progress", form))
        progress_row.addStretch()
        self.progress_text = fluentqt.Label(form)
        self.progress_text.setObjectName("qmlBridgeProgressText")
        self.progress_text.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
        progress_row.addWidget(self.progress_text)
        fields.addLayout(progress_row)
        self.progress_slider = fluentqt.Slider(Qt.Horizontal, form)
        self.progress_slider.setObjectName("qmlBridgeProgressSlider")
        self.progress_slider.setAccessibleName("Preview progress")
        self.progress_slider.setRange(0, 100)
        self.progress_slider.setValue(self.model.progress)
        fields.addWidget(self.progress_slider)
        column.addWidget(form)

        preview = fluentqt.Card(self)
        preview.setObjectName("qmlBridgePreviewCard")
        preview.setMinimumHeight(220)
        self.preview = QStackedLayout(preview)
        self.preview.setContentsMargins(0, 0, 0, 0)
        self.placeholder = fluentqt.Label(preview)
        self.placeholder.setObjectName("qmlBridgePlaceholder")
        self.placeholder.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
        self.placeholder.setAlignment(Qt.AlignCenter)
        self.placeholder.setWordWrap(True)
        self.placeholder.setMargin(20)
        self.preview.addWidget(self.placeholder)
        column.addWidget(preview, 1)

        commands = QHBoxLayout()
        commands.setSpacing(12)
        self.deactivate_button = fluentqt.Button("Deactivate", self)
        self.deactivate_button.setObjectName("qmlBridgeDeactivate")
        reload_button = fluentqt.Button("Reload QML", self)
        reload_button.setObjectName("qmlBridgeReload")
        reload_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
        commands.addWidget(self.deactivate_button)
        commands.addWidget(reload_button)
        commands.addStretch()
        column.addLayout(commands)
        self.status_label = fluentqt.Label(self)
        self.status_label.setObjectName("qmlBridgeStatus")
        self.status_label.setWordWrap(True)
        self.status_label.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
        column.addWidget(self.status_label)

        self.title_edit.textChanged.connect(self.model.setTitle)
        self.progress_slider.valueChanged.connect(self.model.setProgress)
        self.model.titleChanged.connect(self._title_changed)
        self.model.progressChanged.connect(self._progress_changed)
        self.session.activeChanged.connect(self.update_state)
        self.deactivate_button.clicked.connect(self.deactivate_qml)
        reload_button.clicked.connect(self.reload_qml)
        session = self.session
        is_valid = shiboken6.isValid
        self.destroyed.connect(lambda *_: session.deactivate() if is_valid(session) else None)
        self.reload_qml()

    @Slot()
    def _title_changed(self):
        self.title_edit.setText(self.model.title)
        self.update_state()

    @Slot()
    def _progress_changed(self):
        self.progress_slider.setValue(self.model.progress)
        self.update_state()

    @Slot(str)
    def _qml_error(self, message):
        self._error = message
        self.update_state()

    @Slot()
    def reload_qml(self):
        self._error = ""
        if not self.session.activate():
            self._error = "The QML preview is unavailable. The Widgets form stays usable."
            self.update_state()
            return False
        if self.view is None:
            self.view = self.session.createView(self.placeholder.parentWidget())
            if self.view is not None:
                self.view.setObjectName("qmlBridgeView")
                self.view.setAccessibleName("QML model preview")
                self.preview.addWidget(self.view)
                self.view.statusChanged.connect(self.update_state)
                self.view.errorOccurred.connect(self._qml_error)
        if self.view is None:
            self._error = "The QML preview is unavailable. The Widgets form stays usable."
            self.update_state()
            return False
        self.view.unload()
        if not self.view.setContextObject("previewModel", self.model):
            self._error = "The preview model could not be attached."
            self.update_state()
            return False
        loaded = self.view.load(preview_source())
        if not loaded:
            self._error = self.view.lastError()
        self.update_state()
        return loaded

    @Slot()
    def deactivate_qml(self):
        stopped = self.session.deactivate()
        if stopped:
            self._error = ""
        self.update_state()
        return stopped

    @Slot()
    def update_state(self):
        if not shiboken6.isValid(self):
            return
        active = self.session.isActive()
        ready = self.view is not None and self.view.status() == QmlView.Status.Ready
        loading = self.view is not None and self.view.status() == QmlView.Status.Loading
        self.preview.setCurrentWidget(self.view if ready else self.placeholder)
        self.progress_text.setText(f"{self.model.progress}%")
        self.deactivate_button.setEnabled(active)
        self.placeholder.setText(self._error or (
            "Loading QML…" if loading else "QML is unloaded. Your model is still available."))
        self.status_label.setText(self._error or (
            "QML ready · model shared with Widgets" if ready else
            "Loading QML…" if loading else "QML unloaded · Widgets remain editable"))
        self.stateChanged.emit()

    def bridge_state(self):
        return {
            "active": self.session.isActive(),
            "ready": self.view is not None and self.view.status() == QmlView.Status.Ready,
            "title": self.model.title, "progress": self.model.progress,
            "widgetTitle": self.title_edit.text(), "widgetProgress": self.progress_slider.value(),
            "error": self._error,
        }

    def run_smoke_test(self):
        if self.view is None or self.view.status() != QmlView.Status.Ready:
            return False
        self.title_edit.setText("Edited in Widgets")
        self.progress_slider.setValue(65)
        root = self.view.rootObject()
        if root.property("previewTitle") != self.model.title or root.property("previewProgress") != 65:
            return False
        if not QMetaObject.invokeMethod(root, "advance", Qt.DirectConnection):
            return False
        if self.title_edit.text() != "Updated from QML" or self.progress_slider.value() != 75:
            return False
        previous_theme = fluentqt.current_theme()
        try:
            for theme in (fluentqt.Theme.Light, fluentqt.Theme.Dark):
                fluentqt.set_theme(theme)
                QApplication.processEvents()
                tokens = self.view.themeObject()
                if (root.property("color") != tokens.property("surface")
                        or bool(tokens.property("dark")) != (theme == fluentqt.Theme.Dark)):
                    return False
        finally:
            fluentqt.set_theme(previous_theme)
        if not self.deactivate_qml() or self.view.rootObject() is not None or not shiboken6.isValid(self.model):
            return False
        if self.view.status() != QmlView.Status.Unloaded or not self.view.source().isEmpty():
            return False
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        if shiboken6.isValid(root):
            return False
        self.title_edit.setText("Edited while unloaded")
        self.progress_slider.setValue(25)
        if not self.reload_qml() or self.view.status() != QmlView.Status.Ready:
            return False
        root = self.view.rootObject()
        return (root.property("previewTitle") == self.title_edit.text()
                and root.property("previewProgress") == 25
                and QMetaObject.invokeMethod(root, "advance", Qt.DirectConnection)
                and self.title_edit.text() == "Updated from QML"
                and self.progress_slider.value() == 35)


def create_window():
    window = fluentqt.Window()
    window.setObjectName("qmlBridgeWindow")
    window.setWindowTitle("FluentQt QML Bridge")
    window.setMinimumWidth(360)
    window.resize(760, 680)
    window.setContentWidget(QmlBridgePanel(window))
    return window


def _capture_window(window, path):
    from PySide6.QtCore import QPoint, QRect
    from PySide6.QtGui import QPainter
    from PySide6.QtQuickWidgets import QQuickWidget

    image = window.grab()
    quick = window.findChild(QQuickWidget, "qmlQuickWidget")
    if quick is not None and quick.isVisible():
        frame = quick.grabFramebuffer()
        if frame.isNull():
            return False
        painter = QPainter(image)
        try:
            painter.drawImage(QRect(quick.mapTo(window, QPoint()), quick.size()), frame)
        finally:
            painter.end()
    return image.save(str(path))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--smoke-test", action="store_true")
    parser.add_argument("--dark", action="store_true")
    parser.add_argument("--narrow", action="store_true")
    parser.add_argument("--theme", choices=("light", "dark"), default="light")
    parser.add_argument("--size", help="Window size, for example 420x760")
    parser.add_argument("--capture", type=Path, help="Save the ready native window and exit")
    args = parser.parse_args(argv)
    fluentqt.prepare_high_dpi_application()
    QmlSession.prepareApplication()
    app = QApplication(sys.argv[:1])
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))
    if args.dark or args.theme == "dark":
        fluentqt.set_theme(fluentqt.Theme.Dark)
    window = create_window()
    if args.narrow:
        window.resize(480, 680)
    if args.size:
        try:
            width, height = (int(value) for value in args.size.lower().split("x"))
            if width <= 0 or height <= 0:
                raise ValueError
        except ValueError:
            parser.error("--size requires positive WIDTHxHEIGHT")
        window.resize(width, height)
    window.show()
    if args.smoke_test or args.capture:
        panel = window.findChild(QmlBridgePanel)
        deadline = time.monotonic() + 5

        def complete(success):
            if success and args.capture:
                args.capture.parent.mkdir(parents=True, exist_ok=True)
                success = panel.bridge_state()["ready"] and _capture_window(window, args.capture)
            app.exit(0 if success else 1)

        def finish():
            if panel.view is not None and panel.view.status() == QmlView.Status.Loading and time.monotonic() < deadline:
                QTimer.singleShot(50, finish)
                return
            success = panel.run_smoke_test() if args.smoke_test else panel.bridge_state()["ready"]
            if success and args.capture:
                QTimer.singleShot(400, lambda: complete(success))
            else:
                complete(success)

        QTimer.singleShot(200, finish)
    result = app.exec()
    delete_qobject(window)
    return result


if __name__ == "__main__":
    raise SystemExit(main())
