import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication, QVBoxLayout, QWidget
from fluentqt.basicinput import Button
from fluentqt.windowing import Window


app = QApplication(sys.argv)
fluentqt.initialize_resources()
app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

window = Window()
window.setWindowTitle("FluentQt Hello World")
window.resize(480, 320)

content = QWidget()
layout = QVBoxLayout(content)
layout.setContentsMargins(32, 32, 32, 32)

button = Button("Hello from FluentQt", content)
button.setFluentStyle(Button.ButtonStyle.Accent)
layout.addStretch()
layout.addWidget(button, 0, Qt.AlignCenter)
layout.addStretch()

window.setContentWidget(content)
window.show()

sys.exit(app.exec())
