"""Executable Python counterparts of the layout samples."""

from textwrap import dedent

from .native_samples import register_source_samples


def _script(body: str) -> str:
    return (
        "import fluentqt\n"
        "from PySide6.QtCore import QMarginsF, QPointF, Qt\n"
        "from PySide6.QtWidgets import QSizePolicy, QVBoxLayout, QWidget\n\n"
        + dedent(body).strip() + "\n"
    )


def connection_form_source_files(result):
    """Keep the complete connection form copyable beside its short API excerpt."""
    full = result.preview_source.replace(
        "import fluentqt\n", "import fluentqt\n"
        "from PySide6.QtWidgets import QApplication\n"
        "application = QApplication.instance()\n"
        "if application is None:\n"
        "    fluentqt.prepare_high_dpi_application()\n"
        "    application = QApplication([])\n"
        "    fluentqt.initialize_resources()\n"
        "    application.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))\n", 1
    )
    full += '\nif __name__ == "__main__":\n    panel.show()\n    application.exec()\n'
    return [{"name": "connection_form.py", "language": "python", "code": full,
             "usage_snippet": result.source}]


register_source_samples(
    "field",
    ("Field", "FormController"),
    {
        "field-connection-form": (
            "panel",
            _script('''
                from PySide6.QtCore import QTimer, QUrl
                from PySide6.QtWidgets import QBoxLayout

                class ConnectionForm(fluentqt.Card):
                    def __init__(self, parent=None):
                        super().__init__(parent)
                        self.setObjectName("connectionForm")
                        self.setMaximumWidth(560)
                        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Maximum)
                        self.body = QVBoxLayout(self)
                        self.body.setContentsMargins(32, 32, 32, 32)
                        self.body.setSpacing(24)
                        self.body.setAlignment(Qt.AlignTop)
                        heading = QWidget(self)
                        header = QVBoxLayout(heading)
                        header.setContentsMargins(0, 0, 0, 0)
                        header.setSpacing(8)
                        header.addWidget(self.label("Connection settings", fluentqt.FontRole.Subtitle))
                        header.addWidget(self.label(
                            "Configure the endpoint used by this workspace.", fluentqt.FontRole.Body, True))
                        self.body.addWidget(heading)
                        self.form = fluentqt.FormController(self)
                        self.form.setObjectName("connectionFormController")
                        self.editors = {}
                        self.add_field("name", "Connection name",
                            "A name to identify this connection.", "Production API")
                        self.add_field("address", "Server address",
                            "Use HTTPS. Try offline.example to see a connection error.",
                            "https://api.example.com/v1")
                        self.add_field("token", "API token", "Use any 8 characters in this demo.", "", True)
                        self.status = self.label("No unsaved changes", fluentqt.FontRole.Caption, True)
                        self.status.setObjectName("connectionStatus")
                        self.status.setMinimumHeight(16)
                        self.body.addWidget(self.status)
                        actions = QWidget(self)
                        self.actions = QBoxLayout(QBoxLayout.LeftToRight, actions)
                        self.actions.setContentsMargins(0, 0, 0, 0)
                        self.actions.setSpacing(8)
                        self.test_button = fluentqt.Button("Test connection", actions)
                        self.test_button.setObjectName("connectionTest")
                        self.save_button = fluentqt.Button("Save connection", actions)
                        self.save_button.setObjectName("connectionSave")
                        self.save_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
                        self.actions.addWidget(self.test_button)
                        self.actions.addWidget(self.save_button)
                        self.body.addWidget(actions)
                        self.body.addWidget(self.label(
                            "Local demo · no credentials are sent or stored.", fluentqt.FontRole.Caption, True))
                        self.form.validationRequested.connect(self.validate_field)
                        self.form.submitRequested.connect(lambda token: self.later(
                            450, lambda: self.form.completeSubmit(token, True)))
                        self.form.stateChanged.connect(self.update_status)
                        self.form.submissionFinished.connect(lambda success: self.status.setText(
                            "Connection saved for this demo.") if success else None)
                        self.test_button.clicked.connect(self.form.validate)
                        self.save_button.clicked.connect(self.form.submit)

                    def label(self, text, role, secondary=False):
                        result = fluentqt.Label(text, self)
                        result.setFluentTypography(role)
                        result.setTextColorRole(fluentqt.Label.TextColorRole.Secondary if secondary
                                                else fluentqt.Label.TextColorRole.Primary)
                        result.setWordWrap(True)
                        return result

                    def add_field(self, key, title, helper, value, secret=False):
                        field = fluentqt.Field(self)
                        field.setObjectName("connectionField_" + key)
                        field.setLabelText(title)
                        field.setHelperText(helper)
                        editor = fluentqt.PasswordBox() if secret else fluentqt.LineEdit()
                        editor.setObjectName("connectionInput_" + key)
                        editor.setText(value)
                        editor.setMinimumHeight(fluentqt.Spacing.ControlHeight.Standard)
                        if secret:
                            editor.setPlaceholderText("Enter a demo token")
                        field.setOwnedEditor(editor)
                        self.body.addWidget(field)
                        self.editors[key] = editor
                        self.form.addField(key, field)
                        editor.textChanged.connect(lambda _text: self.form.markFieldChanged(key))
                        editor.editingFinished.connect(lambda: self.form.setFieldTouched(key))

                    def later(self, delay, callback):
                        timer = QTimer(self.form)
                        timer.setSingleShot(True)
                        def finish():
                            timer.deleteLater()
                            callback()
                        timer.timeout.connect(finish)
                        timer.start(delay)

                    def validate_field(self, key, token):
                        value = self.editors[key].text().strip()
                        if key == "name":
                            self.form.completeValidation(key, token, bool(value),
                                "" if value else "Enter a connection name.")
                        elif key == "token":
                            self.form.completeValidation(key, token, len(value) >= 8,
                                "" if len(value) >= 8 else "Enter at least 8 characters.")
                        else:
                            url = QUrl(value, QUrl.StrictMode)
                            if not url.isValid() or url.scheme() != "https" or not url.host():
                                self.form.completeValidation(key, token, False, "Enter a valid HTTPS address.")
                                return
                            reachable = "offline" not in value.lower()
                            self.later(650, lambda: self.form.completeValidation(key, token, reachable,
                                "Connection verified." if reachable else "Server unavailable. Try another address."))

                    def update_status(self):
                        busy = self.form.isValidating() or self.form.isSubmitting()
                        self.test_button.setEnabled(not busy)
                        self.save_button.setEnabled(not busy)
                        self.save_button.setText("Saving…" if self.form.isSubmitting() else "Save connection")
                        if self.form.isValidating():
                            message = "Checking connection…"
                        elif self.form.isSubmitting():
                            message = "Saving connection…"
                        elif self.form.errorMessages():
                            message = chr(10).join(self.form.errorMessages())
                        elif self.form.submissionError():
                            message = self.form.submissionError()
                        elif self.form.isValid():
                            message = "Ready to save · unsaved changes" if self.form.isDirty() else "Connection verified."
                        else:
                            message = "Unsaved changes" if self.form.isDirty() else "No unsaved changes"
                        self.status.setText(message)

                    def resizeEvent(self, event):
                        super().resizeEvent(event)
                        narrow = self.width() < 400
                        padding = 16 if narrow else 32
                        self.body.setContentsMargins(padding, padding, padding, padding)
                        self.actions.setDirection(QBoxLayout.TopToBottom if narrow else QBoxLayout.LeftToRight)

                panel = ConnectionForm()
            '''),
        ),
    },
)


register_source_samples(
    "particle-backdrop",
    ("ParticleBackdrop",),
    {
        "particle-backdrop-basic": (
            "backdrop",
            _script("""
                backdrop = fluentqt.ParticleBackdrop()
                backdrop.setEffect(fluentqt.ParticleBackdrop.Effect.FlowingRibbons)
                backdrop.setBackgroundMode(fluentqt.ParticleBackdrop.BackgroundMode.Solid)
                backdrop.setParticleCount(240)
                backdrop.setMaximumFrameRate(30)
                backdrop.setMinimumHeight(240)
                backdrop.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
            """),
        ),
        "particle-backdrop-content": (
            "backdrop",
            _script("""
                backdrop = fluentqt.ParticleBackdrop()
                backdrop.setEffect(fluentqt.ParticleBackdrop.Effect.FloatingDots)
                backdrop.setBackgroundMode(fluentqt.ParticleBackdrop.BackgroundMode.Solid)
                backdrop.setMaximumFrameRate(30)
                backdrop.setFadeMargins(QMarginsF(260, 0, 0, 0))
                backdrop.setMinimumHeight(280)
                backdrop.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
                layout = QVBoxLayout(backdrop)
                layout.setContentsMargins(24, 24, 24, 24)
                title = fluentqt.Label("Make room for a little motion", backdrop)
                title.setFluentTypography(fluentqt.FontRole.Subtitle)
                title.setWordWrap(True)
                layout.addWidget(title)
                layout.addStretch()
                ripple = fluentqt.Button("Create a ripple", backdrop)
                ripple.setObjectName("particleRipple")
                ripple.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
                layout.addWidget(ripple, 0, Qt.AlignLeft)
                ripple.clicked.connect(lambda: backdrop.triggerRipple(
                    QPointF(backdrop.width() * .72, backdrop.height() * .42)
                ))
            """),
        ),
        "particle-backdrop-interaction": (
            "panel",
            _script("""
                panel = QWidget()
                panel.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
                layout = QVBoxLayout(panel)
                layout.setContentsMargins(0, 0, 0, 0)
                layout.setSpacing(12)
                backdrop = fluentqt.ParticleBackdrop(panel)
                backdrop.setEffect(fluentqt.ParticleBackdrop.Effect.Starfield)
                backdrop.setBackgroundMode(fluentqt.ParticleBackdrop.BackgroundMode.Solid)
                backdrop.setInteractive(True)
                backdrop.setObjectName("particleBackdrop")
                backdrop.setMinimumHeight(300)
                layout.addWidget(backdrop)
                effect = fluentqt.ComboBox(panel)
                effect.setObjectName("particleEffect")
                effect.setAccessibleName("Particle effect")
                effect.addItem("Flowing ribbons", fluentqt.ParticleBackdrop.Effect.FlowingRibbons)
                effect.addItem("Floating dots", fluentqt.ParticleBackdrop.Effect.FloatingDots)
                effect.addItem("Starfield", fluentqt.ParticleBackdrop.Effect.Starfield)
                effect.setCurrentIndex(2)
                layout.addWidget(effect)
                effect.currentIndexChanged.connect(
                    lambda _index: backdrop.setEffect(effect.currentData())
                )
                speed = fluentqt.Slider(Qt.Horizontal, panel)
                speed.setObjectName("particleSpeed")
                speed.setRange(25, 200)
                speed.setValue(100)
                speed.setAccessibleName("Particle speed")
                layout.addWidget(speed)
                speed.valueChanged.connect(lambda value: backdrop.setSpeed(value / 100.0))
                pause = fluentqt.Button("Pause motion", panel)
                pause.setObjectName("particlePause")
                layout.addWidget(pause, 0, Qt.AlignLeft)

                def toggle_motion():
                    backdrop.setAnimationEnabled(not backdrop.isAnimationEnabled())
                    pause.setText("Pause motion" if backdrop.isAnimationEnabled() else "Resume motion")

                pause.clicked.connect(toggle_motion)
            """),
        ),
    },
)
