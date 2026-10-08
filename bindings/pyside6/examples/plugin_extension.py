"""The same optional greeting extension used by the C++ and browser examples."""

import json
import sys

import fluentqt
from fluentqt.basicinput import Button
from fluentqt.plugins import PluginManager, PluginMetadata, WidgetPlugin
from fluentqt.textfields import Label
from fluentqt.windowing import Window
from PySide6.QtCore import QTimer, QVersionNumber
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget


class GreetingPlugin(WidgetPlugin):
    def __init__(self):
        super().__init__()
        self.active = False
        self.activations = 0
        self.deactivations = 0

    def metadata(self):
        return PluginMetadata("example.greeting", "Greeting extension", QVersionNumber(1, 0))

    def onActivate(self, context):
        self.active = True
        self.activations += 1
        return True

    def onDeactivate(self):
        self.active = False
        self.deactivations += 1

    def widgetKeys(self):
        return ["greeting"]

    def createWidget(self, key, parent=None):
        if not self.active or key != "greeting":
            raise RuntimeError("Greeting extension is unavailable.")
        button = Button("Hello from an extension", parent)
        button.setFluentStyle(Button.ButtonStyle.Accent)
        button.clicked.connect(lambda: button.setText("Hello, you!"))
        return button


class PluginExtensionWindow(Window):
    def __init__(self):
        super().__init__()
        self.setObjectName("pluginExtensionWindow")
        self.setWindowTitle("FluentQt Plugin Extension")
        self.resize(600, 500)
        # The manager borrows this plugin; the application retains its wrapper.
        self.plugin = GreetingPlugin()
        self.manager = PluginManager(self, self)
        if not self.manager.registerPlugin(self.plugin):
            raise RuntimeError(self.manager.lastError())
        self.greetings = []
        self.rejected_factories = 0
        content = QWidget()
        content.setObjectName("pluginContent")
        column = QVBoxLayout(content)
        column.setContentsMargins(24, 24, 24, 24)
        column.setSpacing(12)
        self.status = Label(content)
        self.status.setObjectName("pluginStatus")
        self.status.setTextColorRole(Label.TextColorRole.Secondary)
        column.addWidget(self.status)
        commands = QHBoxLayout()
        self.activation = Button(content)
        self.activation.setObjectName("pluginActivation")
        self.creation = Button("Create greeting", content)
        self.creation.setObjectName("pluginCreateGreeting")
        commands.addWidget(self.activation)
        commands.addWidget(self.creation)
        column.addLayout(commands)
        self.outputs = QWidget(content)
        self.outputs.setObjectName("pluginOutputs")
        self.output_layout = QVBoxLayout(self.outputs)
        self.output_layout.setContentsMargins(0, 0, 0, 0)
        self.output_layout.setSpacing(12)
        column.addWidget(self.outputs)
        column.addStretch()
        self.setContentWidget(content)
        self.activation.clicked.connect(self.toggle_activation)
        self.creation.clicked.connect(self.create_greeting)
        self.manager.pluginActivated.connect(self.update_state)
        self.manager.pluginDeactivated.connect(self.update_state)
        self.update_state()

    def toggle_activation(self):
        active = self.manager.isActive("example.greeting")
        return self.set_extension_active(not active)

    def set_extension_active(self, active):
        operation = self.manager.activate if active else self.manager.deactivate
        result = operation("example.greeting")
        self.update_state()
        return result

    def create_greeting(self):
        widget = self.manager.createWidget("example.greeting", "greeting", self.outputs)
        if widget is None:
            self.rejected_factories += 1
            return None
        widget.setObjectName(f"pluginGreeting{len(self.greetings) + 1}")
        self.greetings.append(widget)
        self.output_layout.addWidget(widget)
        return widget

    def update_state(self, *_args):
        active = self.manager.isActive("example.greeting")
        self.activation.setText("Deactivate extension" if active else "Activate extension")
        self.creation.setEnabled(active)
        self.status.setText("Extension active" if active else
                            "Extension inactive; created greetings stay available")

    def smoke_test(self):
        assert self.set_extension_active(True), self.manager.lastError()
        first = self.create_greeting()
        assert first is not None, self.manager.lastError()
        assert self.set_extension_active(False), self.manager.lastError()
        assert self.create_greeting() is None
        first.click()
        assert first.text() == "Hello, you!"
        assert self.set_extension_active(True), self.manager.lastError()
        second = self.create_greeting()
        assert second is not None and second is not first
        assert self.set_extension_active(False), self.manager.lastError()
        assert first.parentWidget() is self.outputs and second.parentWidget() is self.outputs
        assert self.plugin.activations == self.plugin.deactivations == 2
        assert self.rejected_factories == 1
        print(json.dumps({"active": self.manager.isActive("example.greeting"),
                          "activations": self.plugin.activations,
                          "deactivations": self.plugin.deactivations,
                          "created": len(self.greetings), "alive": len(self.greetings),
                          "rejectedFactories": self.rejected_factories}))


def main():
    fluentqt.prepare_high_dpi_application()
    app = QApplication(sys.argv)
    fluentqt.initialize_resources()
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))
    window = PluginExtensionWindow()
    window.show()
    if "--smoke-test" in sys.argv:
        def smoke():
            try:
                window.smoke_test()
            except Exception:
                import traceback
                traceback.print_exc()
                app.exit(1)
            else:
                app.exit(0)
        QTimer.singleShot(100, smoke)
    result = app.exec()
    window.manager.deactivateAll()
    return result


if __name__ == "__main__":
    sys.exit(main())
