"""The C++ Spatial Hello World mirrored through the public Python API."""
import sys
import fluentqt
from PySide6.QtCore import QPointF, Qt
from PySide6.QtGui import QVector3D
from PySide6.QtWidgets import QApplication, QVBoxLayout, QWidget
from fluentqt.basicinput import Button
from fluentqt.layout import Card
from fluentqt.spatial import SpatialSession
from fluentqt.windowing import Window


def create_window() -> Window:
    window = Window()
    window.setObjectName("helloWorldSpatialWindow")
    window.setWindowTitle("FluentQt Hello World — 3D")
    window.resize(720, 520)
    session = SpatialSession(window, window)
    activated = session.activate()
    if activated:
        session.prepareWindow(window)
    window.spatial_session = session

    card = Card()
    card.setObjectName("helloWorldSpatialCard")
    card.setFixedSize(320, 200)
    layout = QVBoxLayout(card)
    layout.setContentsMargins(32, 32, 32, 32)

    button = Button("Hello from FluentQt", card)
    button.setObjectName("helloWorldButton")
    button.setFluentStyle(Button.ButtonStyle.Accent)
    button.clicked.connect(lambda: button.setText("Hello, you!"))
    layout.addWidget(button, 0, Qt.AlignCenter)

    view = session.createView(window) if activated else None
    if view is None:
        session.deactivate()
        content = QWidget()
        content.setObjectName("helloWorldSpatialFallback")
        center = QVBoxLayout(content)
        center.setContentsMargins(0, 0, 0, 0)
        center.addWidget(card, 0, Qt.AlignCenter)
        window.setContentWidget(content)
        return window
    view.setObjectName("helloWorldSpatialView")
    item = view.addOwnedWidget(card)
    item.setRotation(QVector3D(6, -12, 0))
    item.setSurfaceIntensity(0.6)
    view.setMaximumTilt(QPointF(3, 5))
    window.setContentWidget(view)
    return window


def main() -> int:
    fluentqt.prepare_high_dpi_application()
    app = QApplication(sys.argv)
    fluentqt.initialize_resources()
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))
    window = create_window()
    window.show()
    result = app.exec()
    window.spatial_session.deactivate()
    return result


if __name__ == "__main__":
    sys.exit(main())
