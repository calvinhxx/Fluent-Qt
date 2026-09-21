"""Minimal optional Spatial integration: a window and one live button."""
import sys
import fluentqt
from PySide6.QtGui import QVector3D
from PySide6.QtWidgets import QApplication
from fluentqt.basicinput import Button
from fluentqt.spatial import SpatialView
from fluentqt.windowing import Window


def main() -> int:
    fluentqt.prepare_high_dpi_application()
    app = QApplication(sys.argv)
    fluentqt.initialize_resources()
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))
    window = Window()
    window.setWindowTitle("FluentQt Spatial Hello World")
    window.resize(480, 320)
    view = SpatialView()
    button = Button("Hello from Spatial")
    button.setFluentStyle(Button.ButtonStyle.Accent)
    button.setFixedSize(200, 48)
    button.clicked.connect(lambda: button.setText("Clicked in 3D"))
    item = view.addOwnedWidget(button)
    item.setRotation(QVector3D(8, -12, 0))
    window.setContentWidget(view)
    window.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
