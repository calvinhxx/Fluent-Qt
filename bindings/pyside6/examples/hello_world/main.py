import sys

import fluentqt
from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication, QVBoxLayout, QWidget
from fluentqt.basicinput import Button
from fluentqt.windowing import Window


def create_window() -> Window:
    window = Window()
    window.setWindowTitle("FluentQt Hello World")
    window.resize(720, 520)

    content = QWidget()
    layout = QVBoxLayout(content)

    button = Button("Hello from FluentQt", content)
    button.setFluentStyle(Button.ButtonStyle.Accent)
    button.clicked.connect(lambda: button.setText("Hello, you!"))
    layout.addWidget(button, 0, Qt.AlignCenter)

    window.setContentWidget(content)
    return window


def main() -> int:
    fluentqt.prepare_high_dpi_application()
    app = QApplication(sys.argv)
    fluentqt.initialize_resources()
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    window = create_window()
    window.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
