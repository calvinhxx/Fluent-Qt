"""Standalone Gallery ports for native dialogs and flyouts."""

from __future__ import annotations

from textwrap import dedent

from .native_samples import register_source_samples


def _script(body: str, imports: str = "") -> str:
    prefix = "import fluentqt\n"
    if imports:
        prefix += imports.strip() + "\n"
    return prefix + "\n" + dedent(body).strip() + "\n"


_WIDGETS = "from PySide6.QtWidgets import QHBoxLayout, QVBoxLayout, QWidget"

_DIALOGS_IMPORTS = (
    "from PySide6.QtCore import QPoint, QSize, Qt\n"
    "from PySide6.QtGui import QPainter, QPen\n"
    "from PySide6.QtWidgets import QHBoxLayout, QSizePolicy, QVBoxLayout, QWidget\n"
    "from fluentqt_gallery.foundation_pages import _theme_tokens"
)

_DIALOGS_HELPER = dedent(
    """
    class SampleSurface(QWidget):
        def __init__(self, parent=None, spacing=12):
            super().__init__(parent)
            self.setSizePolicy(
                QSizePolicy.Policy.MinimumExpanding,
                QSizePolicy.Policy.Fixed,
            )
            self.content_layout = QVBoxLayout(self)
            self.content_layout.setContentsMargins(16, 14, 16, 16)
            self.content_layout.setSpacing(spacing)
            self.content_layout.setAlignment(
                Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
            )

        def paintEvent(self, event):
            del event
            colors = _theme_tokens(self)
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setPen(QPen(colors["strokeCard"], 1.0))
            painter.setBrush(colors["bgCanvas"])
            painter.drawRoundedRect(
                self.rect().adjusted(0, 0, -1, -1), 8.0, 8.0
            )


    def horizontal_group(parent, spacing=12):
        group = QWidget(parent)
        layout = QHBoxLayout(group)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(spacing)
        layout.setAlignment(
            Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
        )
        return group


    def make_body_label(parent, text):
        label = fluentqt.Label(text, parent)
        label.setFluentTypography(fluentqt.FontRole.Body)
        label.setWordWrap(True)
        label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        return label


    def make_status_label(parent, text):
        label = make_body_label(parent, text)
        label.setMinimumWidth(220)
        return label


    def make_title_label(parent, text):
        label = fluentqt.Label(text, parent)
        label.setFluentTypography(fluentqt.FontRole.BodyStrong)
        label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        return label


    def sample_button(parent, text):
        button = fluentqt.Button(text, parent)
        button.setFluentSize(fluentqt.Button.ButtonSize.Small)
        button.setMinimumWidth(96)
        return button
    """
)


register_source_samples(
    "content-dialog",
    ("ContentDialog",),
    {
        "content-dialog-result-buttons": (
            "root",
            _script(
                _DIALOGS_HELPER
                + dedent("""
                root = SampleSurface(globals().get("gallery_parent"))
                row = horizontal_group(root, 12)
                show = sample_button(row, "Show dialog")
                show.setMinimumWidth(118)
                status = make_status_label(row, "Result: not shown")
                row.layout().addWidget(show)
                row.layout().addWidget(status)
                root.content_layout.addWidget(row)

                def open_dialog():
                    dialog = fluentqt.ContentDialog(show.window())
                    if hasattr(dialog, "setThemeSource"):
                        dialog.setThemeSource(show)
                    dialog.setTitle("Save your work?")
                    content = make_body_label(
                        None,
                        'Unsaved changes in "Quarterly report" will be lost '
                        "unless you save them.",
                    )
                    dialog.setContent(content)
                    dialog.setPrimaryButtonText("Save")
                    dialog.setSecondaryButtonText("Don't save")
                    dialog.setCloseButtonText("Cancel")
                    dialog.setDefaultButton(
                        fluentqt.ContentDialog.ContentDialogButton.Primary
                    )
                    dialog.primaryButtonClicked.connect(
                        lambda: status.setText("Result: Save")
                    )
                    dialog.secondaryButtonClicked.connect(
                        lambda: status.setText("Result: Don't save")
                    )
                    dialog.closeButtonClicked.connect(
                        lambda: status.setText("Result: Cancel")
                    )
                    dialog.exec()
                    dialog.deleteLater()

                show.clicked.connect(open_dialog)
                """),
                _DIALOGS_IMPORTS,
            ),
        ),
        "content-dialog-custom-content": (
            "root",
            _script(
                _DIALOGS_HELPER
                + dedent("""
                root = SampleSurface(globals().get("gallery_parent"))
                row = horizontal_group(root, 12)
                show = sample_button(row, "Share draft")
                show.setMinimumWidth(118)
                status = make_status_label(row, "Draft state: private")
                row.layout().addWidget(show)
                row.layout().addWidget(status)
                root.content_layout.addWidget(row)

                def open_dialog():
                    content = QWidget()
                    content.setMinimumWidth(360)
                    content_layout = QVBoxLayout(content)
                    content_layout.setContentsMargins(0, 0, 0, 0)
                    content_layout.setSpacing(8)
                    content_layout.setAlignment(
                        Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                    )
                    body = make_body_label(
                        content,
                        "Invite reviewers and include the latest attachments "
                        "with this draft.",
                    )
                    upload = fluentqt.CheckBox(
                        "Upload attachments before sharing", content
                    )
                    upload.setChecked(True)
                    content_layout.addWidget(body)
                    content_layout.addWidget(upload)

                    dialog = fluentqt.ContentDialog(show.window())
                    if hasattr(dialog, "setThemeSource"):
                        dialog.setThemeSource(show)
                    dialog.setTitle("Share draft?")
                    dialog.setContent(content)
                    dialog.setPrimaryButtonText("Share")
                    dialog.setCloseButtonText("Not now")
                    dialog.setDefaultButton(
                        fluentqt.ContentDialog.ContentDialogButton.None_
                    )
                    dialog.primaryButtonClicked.connect(
                        lambda: status.setText(
                            "Draft shared with attachments"
                            if upload.isChecked()
                            else "Draft shared without attachments"
                        )
                    )
                    dialog.closeButtonClicked.connect(
                        lambda: status.setText("Draft state: private")
                    )
                    dialog.exec()
                    dialog.deleteLater()

                show.clicked.connect(open_dialog)
                """),
                _DIALOGS_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "dialog",
    ("Dialog",),
    {
        "dialog-owned-content": (
            "root",
            _script(
                _DIALOGS_HELPER
                + dedent("""
                root = SampleSurface(globals().get("gallery_parent"))
                row = horizontal_group(root, 12)
                show = sample_button(row, "Open dialog")
                show.setMinimumWidth(118)
                status = make_status_label(
                    row, "Project name: Northwind Analytics"
                )
                row.layout().addWidget(show)
                row.layout().addWidget(status)
                root.content_layout.addWidget(row)

                def open_dialog():
                    dialog = fluentqt.Dialog(show.window())
                    if hasattr(dialog, "setThemeSource"):
                        dialog.setThemeSource(show)
                    dialog.setMinimumSize(480, 280)
                    dialog_layout = QVBoxLayout(dialog)
                    dialog_layout.setContentsMargins(32, 28, 32, 28)
                    dialog_layout.setSpacing(14)
                    dialog_layout.addWidget(
                        make_title_label(dialog, "Rename project")
                    )
                    dialog_layout.addWidget(
                        make_body_label(
                            dialog,
                            "Choose a display name that appears in navigation "
                            "and recent projects.",
                        )
                    )
                    name_edit = fluentqt.LineEdit(dialog)
                    name_edit.setText("Northwind Analytics")
                    dialog_layout.addWidget(name_edit)
                    dialog_layout.addStretch(1)

                    button_row = QHBoxLayout()
                    button_row.setSpacing(8)
                    button_row.addStretch(1)
                    apply_button = fluentqt.Button("Apply", dialog)
                    apply_button.setFluentStyle(
                        fluentqt.Button.ButtonStyle.Accent
                    )
                    cancel_button = fluentqt.Button("Cancel", dialog)
                    apply_button.clicked.connect(lambda: dialog.done(1))
                    cancel_button.clicked.connect(lambda: dialog.done(0))
                    button_row.addWidget(apply_button)
                    button_row.addWidget(cancel_button)
                    dialog_layout.addLayout(button_row)

                    result = dialog.exec()
                    status.setText(
                        f"Project name: {name_edit.text()}"
                        if result == 1
                        else "Dialog result: Cancel"
                    )
                    dialog.deleteLater()

                show.clicked.connect(open_dialog)
                """),
                _DIALOGS_IMPORTS,
            ),
        ),
        "dialog-animation-smoke": (
            "root",
            _script(
                _DIALOGS_HELPER
                + dedent("""
                root = SampleSurface(globals().get("gallery_parent"))
                row = horizontal_group(root, 10)
                animated = sample_button(row, "Animated smoke")
                animated.setMinimumWidth(136)
                instant = sample_button(row, "Instant")
                status = make_status_label(root, "Last dialog: none")
                row.layout().addWidget(animated)
                row.layout().addWidget(instant)
                root.content_layout.addWidget(row)
                root.content_layout.addWidget(status)

                def open_dialog(trigger, animation, smoke):
                    dialog = fluentqt.Dialog(trigger.window())
                    if hasattr(dialog, "setThemeSource"):
                        dialog.setThemeSource(trigger)
                    dialog.setSmokeEnabled(smoke)
                    dialog.setAnimationEnabled(animation)
                    dialog.setMinimumSize(420, 220)
                    dialog_layout = QVBoxLayout(dialog)
                    dialog_layout.setContentsMargins(30, 28, 30, 28)
                    dialog_layout.setSpacing(12)
                    dialog_layout.addWidget(
                        make_title_label(
                            dialog,
                            "Animated dialog" if animation else "Instant dialog",
                        )
                    )
                    dialog_layout.addWidget(
                        make_body_label(
                            dialog,
                            "Smoke focuses attention by dimming the owning window."
                            if smoke
                            else "No smoke keeps the surrounding window visually available.",
                        )
                    )
                    dialog_layout.addStretch(1)
                    close_button = sample_button(dialog, "Close")
                    close_button.clicked.connect(lambda: dialog.done(0))
                    dialog_layout.addWidget(
                        close_button, 0, Qt.AlignmentFlag.AlignRight
                    )
                    dialog.exec()
                    status.setText(
                        "Last dialog: animated with smoke"
                        if animation
                        else "Last dialog: instant without smoke"
                    )
                    dialog.deleteLater()

                animated.clicked.connect(
                    lambda: open_dialog(animated, True, True)
                )
                instant.clicked.connect(
                    lambda: open_dialog(instant, False, False)
                )
                """),
                _DIALOGS_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "flyout",
    ("Flyout",),
    {
        "flyout-placement-anchors": (
            "root",
            _script(
                _DIALOGS_HELPER
                + dedent("""
                root = SampleSurface(globals().get("gallery_parent"))
                row = horizontal_group(root, 8)
                bottom = sample_button(row, "Bottom")
                right = sample_button(row, "Right")
                automatic = sample_button(row, "Auto")
                status = make_status_label(
                    root, "Opened placement: none"
                )
                row.layout().addWidget(bottom)
                row.layout().addWidget(right)
                row.layout().addWidget(automatic)
                root.content_layout.addWidget(row)
                root.content_layout.addWidget(status)

                placement_names = {
                    fluentqt.Flyout.Placement.Bottom: "Bottom",
                    fluentqt.Flyout.Placement.Right: "Right",
                    fluentqt.Flyout.Placement.Auto: "Auto",
                }

                def show_placement(anchor, placement):
                    placement_name = placement_names[placement]
                    flyout = fluentqt.Flyout(anchor.window())
                    flyout.setPlacement(placement)
                    flyout.setMinimumSize(340, 188)
                    flyout_layout = QVBoxLayout(flyout)
                    flyout_layout.setContentsMargins(24, 22, 24, 22)
                    flyout_layout.setSpacing(10)
                    flyout_layout.addWidget(
                        make_title_label(flyout, f"{placement_name} flyout")
                    )
                    flyout_layout.addWidget(
                        make_body_label(
                            flyout,
                            "Click outside, press Escape, or use Close to "
                            "dismiss this anchored surface.",
                        )
                    )
                    flyout_layout.addStretch(1)
                    close_button = sample_button(flyout, "Close")
                    close_button.clicked.connect(flyout.close)
                    flyout_layout.addWidget(
                        close_button, 0, Qt.AlignmentFlag.AlignRight
                    )
                    flyout.opened.connect(
                        lambda name=placement_name: status.setText(
                            f"Opened placement: {name}"
                        )
                    )
                    flyout.closed.connect(flyout.deleteLater)
                    flyout.showAt(anchor)

                bottom.clicked.connect(
                    lambda: show_placement(
                        bottom, fluentqt.Flyout.Placement.Bottom
                    )
                )
                right.clicked.connect(
                    lambda: show_placement(
                        right, fluentqt.Flyout.Placement.Right
                    )
                )
                automatic.clicked.connect(
                    lambda: show_placement(
                        automatic, fluentqt.Flyout.Placement.Auto
                    )
                )
                """),
                _DIALOGS_IMPORTS,
            ),
        ),
        "flyout-command-confirmation": (
            "root",
            _script(
                _DIALOGS_HELPER
                + dedent("""
                root = SampleSurface(globals().get("gallery_parent"))
                row = horizontal_group(root, 12)
                command = sample_button(row, "Empty cart")
                command.setMinimumWidth(118)
                status = make_status_label(row, "Cart: 4 items")
                row.layout().addWidget(command)
                row.layout().addWidget(status)
                root.content_layout.addWidget(row)

                def open_flyout():
                    flyout = fluentqt.Flyout(command.window())
                    flyout.setPlacement(fluentqt.Flyout.Placement.Right)
                    flyout.setMinimumSize(360, 214)
                    flyout_layout = QVBoxLayout(flyout)
                    flyout_layout.setContentsMargins(24, 22, 24, 22)
                    flyout_layout.setSpacing(10)
                    flyout_layout.addWidget(
                        make_title_label(flyout, "Empty cart?")
                    )
                    flyout_layout.addWidget(
                        make_body_label(
                            flyout,
                            "All selected items will be removed from the cart. "
                            "You can restore them from order history later.",
                        )
                    )
                    flyout_layout.addStretch(1)
                    commands = horizontal_group(flyout, 8)
                    confirm = sample_button(commands, "Empty")
                    confirm.setFluentStyle(
                        fluentqt.Button.ButtonStyle.Accent
                    )
                    cancel = sample_button(commands, "Cancel")

                    def confirm_empty():
                        status.setText("Cart: emptied")
                        flyout.close()

                    confirm.clicked.connect(confirm_empty)
                    cancel.clicked.connect(flyout.close)
                    commands.layout().addWidget(confirm)
                    commands.layout().addWidget(cancel)
                    flyout_layout.addWidget(
                        commands, 0, Qt.AlignmentFlag.AlignRight
                    )
                    flyout.closed.connect(flyout.deleteLater)
                    flyout.showAt(command)

                command.clicked.connect(open_flyout)
                """),
                _DIALOGS_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "popup",
    ("Popup",),
    {
        "popup-position-light-dismiss": (
            "root",
            _script(
                _DIALOGS_HELPER
                + dedent("""
                root = SampleSurface(globals().get("gallery_parent"))
                controls = horizontal_group(root, 12)
                anchor = sample_button(controls, "Show popup")
                anchor.setMinimumWidth(118)
                light_dismiss = fluentqt.ToggleSwitch(controls)
                light_dismiss.setOnContent("Light dismiss")
                light_dismiss.setOffContent("Sticky")
                light_dismiss.setIsOn(True)
                status = make_status_label(root, "Popup state: closed")
                controls.layout().addWidget(anchor)
                controls.layout().addWidget(light_dismiss)
                root.content_layout.addWidget(controls)
                root.content_layout.addWidget(status)

                def open_popup():
                    popup = fluentqt.Popup(anchor.window())
                    if hasattr(popup, "setThemeSource"):
                        popup.setThemeSource(anchor)
                    popup.setMinimumSize(360, 186)
                    policy = (
                        fluentqt.Popup.CloseOnPressOutside | fluentqt.Popup.CloseOnEscape
                        if light_dismiss.isOn()
                        else fluentqt.Popup.NoAutoClose
                    )
                    popup.setClosePolicy(fluentqt.Popup.ClosePolicy(policy))
                    popup_layout = QVBoxLayout(popup)
                    popup_layout.setContentsMargins(24, 22, 24, 22)
                    popup_layout.setSpacing(10)
                    popup_layout.addWidget(
                        make_title_label(popup, "Simple popup")
                    )
                    popup_layout.addWidget(
                        make_body_label(
                            popup,
                            "Click outside or press Escape to close this popup."
                            if light_dismiss.isOn()
                            else "This sticky popup closes only from its own command.",
                        )
                    )
                    popup_layout.addStretch(1)
                    close_button = sample_button(popup, "Close")
                    close_button.clicked.connect(popup.close)
                    popup_layout.addWidget(
                        close_button, 0, Qt.AlignmentFlag.AlignRight
                    )
                    popup.setPosition(anchor, QPoint(0, anchor.height() + 8))
                    popup.opened.connect(
                        lambda: status.setText(
                            "Popup state: open, light dismiss on"
                            if light_dismiss.isOn()
                            else "Popup state: open, sticky"
                        )
                    )
                    popup.closed.connect(
                        lambda: status.setText("Popup state: closed")
                    )
                    popup.closed.connect(popup.deleteLater)
                    popup.open()

                anchor.clicked.connect(open_popup)
                """),
                _DIALOGS_IMPORTS,
            ),
        ),
        "popup-modal-dim": (
            "root",
            _script(
                _DIALOGS_HELPER
                + dedent("""
                root = SampleSurface(globals().get("gallery_parent"))
                row = horizontal_group(root, 12)
                show = sample_button(row, "Review access")
                show.setMinimumWidth(128)
                status = make_status_label(row, "Access review: pending")
                row.layout().addWidget(show)
                row.layout().addWidget(status)
                root.content_layout.addWidget(row)

                def open_popup():
                    popup = fluentqt.Popup(show.window())
                    if hasattr(popup, "setThemeSource"):
                        popup.setThemeSource(show)
                    popup.setModal(True)
                    popup.setDim(True)
                    popup.setClosePolicy(fluentqt.Popup.CloseOnEscape)
                    popup.setMinimumSize(400, 220)
                    popup_layout = QVBoxLayout(popup)
                    popup_layout.setContentsMargins(28, 26, 28, 26)
                    popup_layout.setSpacing(12)
                    popup_layout.addWidget(
                        make_title_label(popup, "Grant editor access?")
                    )
                    popup_layout.addWidget(
                        make_body_label(
                            popup,
                            "The recipient will be able to update shared "
                            "project files immediately.",
                        )
                    )
                    popup_layout.addStretch(1)
                    commands = horizontal_group(popup, 8)
                    grant = sample_button(commands, "Grant")
                    grant.setFluentStyle(
                        fluentqt.Button.ButtonStyle.Accent
                    )
                    cancel = sample_button(commands, "Cancel")

                    def finish(message):
                        status.setText(message)
                        popup.close()

                    grant.clicked.connect(
                        lambda: finish("Access review: granted")
                    )
                    cancel.clicked.connect(
                        lambda: finish("Access review: canceled")
                    )
                    commands.layout().addWidget(grant)
                    commands.layout().addWidget(cancel)
                    popup_layout.addWidget(
                        commands, 0, Qt.AlignmentFlag.AlignRight
                    )
                    popup.closed.connect(popup.deleteLater)
                    popup.open()

                show.clicked.connect(open_popup)
                """),
                _DIALOGS_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "teaching-tip",
    ("TeachingTip",),
    {
        "teaching-tip-targeted-action": (
            "root",
            _script(
                _DIALOGS_HELPER
                + dedent("""
                root = SampleSurface(globals().get("gallery_parent"))
                row = horizontal_group(root, 12)
                target = sample_button(row, "Sync settings")
                target.setMinimumWidth(128)
                status = make_status_label(row, "Tip state: closed")
                row.layout().addWidget(target)
                row.layout().addWidget(status)
                root.content_layout.addWidget(row)

                close_reason_names = {
                    fluentqt.TeachingTip.CloseReason.ActionButton: "action button",
                    fluentqt.TeachingTip.CloseReason.CloseButton: "close button",
                    fluentqt.TeachingTip.CloseReason.LightDismiss: "light dismiss",
                    fluentqt.TeachingTip.CloseReason.TargetDestroyed: "target destroyed",
                    fluentqt.TeachingTip.CloseReason.Programmatic: "programmatic",
                }

                def show_tip():
                    tip = fluentqt.TeachingTip(target.window())
                    tip.setPreferredPlacement(
                        fluentqt.TeachingTip.PreferredPlacement.Right
                    )
                    tip.setLightDismissEnabled(True)
                    tip.setCardSize(QSize(320, 154))
                    host = tip.contentHost()
                    host_layout = QVBoxLayout(host)
                    host_layout.setContentsMargins(14, 12, 14, 12)
                    host_layout.setSpacing(8)
                    title_row = QHBoxLayout()
                    title_row.setSpacing(8)
                    title_row.addWidget(
                        make_title_label(host, "Sync is automatic")
                    )
                    title_row.addStretch(1)
                    close_button = fluentqt.Button("", host)
                    close_button.setFluentLayout(
                        fluentqt.Button.ButtonLayout.IconOnly
                    )
                    close_button.setFluentStyle(
                        fluentqt.Button.ButtonStyle.Subtle
                    )
                    close_button.setIconGlyph("\ue8bb")
                    close_button.setFixedSize(30, 30)
                    close_button.clicked.connect(
                        lambda: tip.closeWithReason(
                            fluentqt.TeachingTip.CloseReason.CloseButton
                        )
                    )
                    title_row.addWidget(close_button)
                    host_layout.addLayout(title_row)
                    host_layout.addWidget(
                        make_body_label(
                            host,
                            "Changes are saved to the workspace as you edit.",
                        )
                    )
                    host_layout.addStretch(1)
                    action_button = sample_button(host, "Got it")
                    action_button.setFluentStyle(
                        fluentqt.Button.ButtonStyle.Accent
                    )
                    action_button.clicked.connect(
                        lambda: tip.closeWithReason(
                            fluentqt.TeachingTip.CloseReason.ActionButton
                        )
                    )
                    host_layout.addWidget(
                        action_button, 0, Qt.AlignmentFlag.AlignRight
                    )
                    tip.closing.connect(
                        lambda reason: status.setText(
                            f"Closed by {close_reason_names.get(reason, 'programmatic')}"
                        )
                    )
                    tip.opened.connect(
                        lambda: status.setText("Tip state: open")
                    )
                    tip.closed.connect(tip.deleteLater)
                    tip.showAt(target)

                target.clicked.connect(show_tip)
                """),
                _DIALOGS_IMPORTS,
            ),
        ),
        "teaching-tip-placement-tail": (
            "root",
            _script(
                _DIALOGS_HELPER
                + dedent("""
                root = SampleSurface(globals().get("gallery_parent"))
                controls = horizontal_group(root, 8)
                top = sample_button(controls, "Top")
                right_top = sample_button(controls, "RightTop")
                automatic = sample_button(controls, "Auto")
                tail = fluentqt.ToggleSwitch(controls)
                tail.setIsOn(True)
                tail.setOnContent("Tail")
                tail.setOffContent("No tail")
                for widget in (top, right_top, automatic, tail):
                    controls.layout().addWidget(widget)
                status = make_status_label(root, "Placement: none")
                root.content_layout.addWidget(controls)
                root.content_layout.addWidget(status)

                placement_names = {
                    fluentqt.TeachingTip.PreferredPlacement.Top: "Top",
                    fluentqt.TeachingTip.PreferredPlacement.RightTop: "RightTop",
                    fluentqt.TeachingTip.PreferredPlacement.Auto: "Auto",
                }
                close_reason_names = {
                    fluentqt.TeachingTip.CloseReason.ActionButton: "action button",
                    fluentqt.TeachingTip.CloseReason.CloseButton: "close button",
                    fluentqt.TeachingTip.CloseReason.LightDismiss: "light dismiss",
                    fluentqt.TeachingTip.CloseReason.TargetDestroyed: "target destroyed",
                    fluentqt.TeachingTip.CloseReason.Programmatic: "programmatic",
                }

                def show_tip(anchor, placement):
                    name = placement_names[placement]
                    tip = fluentqt.TeachingTip(anchor.window())
                    tip.setPreferredPlacement(placement)
                    tip.setTailVisible(tail.isOn())
                    tip.setLightDismissEnabled(True)
                    tip.setCardSize(QSize(300, 136))
                    host = tip.contentHost()
                    host_layout = QVBoxLayout(host)
                    host_layout.setContentsMargins(14, 12, 14, 12)
                    host_layout.setSpacing(8)
                    title_row = QHBoxLayout()
                    title_row.setSpacing(8)
                    title_row.addWidget(
                        make_title_label(host, f"{name} placement")
                    )
                    title_row.addStretch(1)
                    close_button = fluentqt.Button("", host)
                    close_button.setFluentLayout(
                        fluentqt.Button.ButtonLayout.IconOnly
                    )
                    close_button.setFluentStyle(
                        fluentqt.Button.ButtonStyle.Subtle
                    )
                    close_button.setIconGlyph("\ue8bb")
                    close_button.setFixedSize(30, 30)
                    close_button.clicked.connect(
                        lambda: tip.closeWithReason(
                            fluentqt.TeachingTip.CloseReason.CloseButton
                        )
                    )
                    title_row.addWidget(close_button)
                    host_layout.addLayout(title_row)
                    host_layout.addWidget(
                        make_body_label(
                            host,
                            "The tail points back to the control that opened the tip."
                            if tail.isOn()
                            else "Hide the tail when the surrounding layout already makes context clear.",
                        )
                    )
                    host_layout.addStretch(1)
                    action = sample_button(host, "Got it")
                    action.setFluentStyle(
                        fluentqt.Button.ButtonStyle.Accent
                    )
                    action.clicked.connect(
                        lambda: tip.closeWithReason(
                            fluentqt.TeachingTip.CloseReason.ActionButton
                        )
                    )
                    host_layout.addWidget(
                        action, 0, Qt.AlignmentFlag.AlignRight
                    )
                    tip.closing.connect(
                        lambda reason: status.setText(
                            f"Closed by {close_reason_names.get(reason, 'programmatic')}"
                        )
                    )
                    tip.opened.connect(
                        lambda: status.setText(
                            f"Placement: {name}, tail {'on' if tail.isOn() else 'off'}"
                        )
                    )
                    tip.closed.connect(tip.deleteLater)
                    tip.showAt(anchor)

                top.clicked.connect(
                    lambda: show_tip(
                        top, fluentqt.TeachingTip.PreferredPlacement.Top
                    )
                )
                right_top.clicked.connect(
                    lambda: show_tip(
                        right_top,
                        fluentqt.TeachingTip.PreferredPlacement.RightTop,
                    )
                )
                automatic.clicked.connect(
                    lambda: show_tip(
                        automatic,
                        fluentqt.TeachingTip.PreferredPlacement.Auto,
                    )
                )
                """),
                _DIALOGS_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "coach-mark",
    ("CoachMark",),
    {
        "coach-mark-targeted-glide": (
            "root",
            _script(
                _DIALOGS_HELPER
                + dedent("""
                root = SampleSurface(globals().get("gallery_parent"))
                controls = horizontal_group(root, 8)
                bottom = sample_button(controls, "Bottom")
                right = sample_button(controls, "Right")
                top = sample_button(controls, "Top")
                controls.layout().addWidget(bottom)
                controls.layout().addWidget(right)
                controls.layout().addWidget(top)
                status = make_status_label(root, "Coach mark: closed")
                root.content_layout.addWidget(controls)
                root.content_layout.addWidget(status)
                state = {"coach": None, "title": None}

                def show_coach(target, placement, name):
                    coach = state["coach"]
                    if coach is None:
                        coach = fluentqt.CoachMark(target.window())
                        coach.setCardSize(QSize(320, 150))
                        host = coach.contentHost()
                        host_layout = QVBoxLayout(host)
                        host_layout.setContentsMargins(18, 14, 14, 14)
                        host_layout.setSpacing(8)
                        title_row = QHBoxLayout()
                        title_row.setSpacing(8)
                        title = make_title_label(host, "Coach mark")
                        title_row.addWidget(title)
                        title_row.addStretch(1)
                        close_button = fluentqt.Button("", host)
                        close_button.setFluentLayout(
                            fluentqt.Button.ButtonLayout.IconOnly
                        )
                        close_button.setFluentStyle(
                            fluentqt.Button.ButtonStyle.Subtle
                        )
                        close_button.setIconGlyph("\ue8bb")
                        close_button.setFixedSize(30, 30)
                        close_button.clicked.connect(coach.close)
                        title_row.addWidget(close_button)
                        host_layout.addLayout(title_row)
                        host_layout.addWidget(
                            make_body_label(
                                host,
                                "The tail points back at the control that opened "
                                "this coach mark. Pick another placement to watch "
                                "it glide.",
                            )
                        )
                        host_layout.addStretch(1)
                        got_it = sample_button(host, "Got it")
                        got_it.setFluentStyle(
                            fluentqt.Button.ButtonStyle.Accent
                        )
                        got_it.clicked.connect(coach.close)
                        host_layout.addWidget(
                            got_it, 0, Qt.AlignmentFlag.AlignRight
                        )
                        coach.closed.connect(
                            lambda: status.setText("Coach mark: closed")
                        )
                        state["coach"] = coach
                        state["title"] = title
                    state["title"].setText(f"{name} placement")
                    coach.setPlacement(placement)
                    coach.setTarget(target)
                    status.setText("Coach mark: {0}".format(name))
                    coach.open()

                bottom.clicked.connect(
                    lambda: show_coach(
                        bottom, fluentqt.CoachMark.Placement.Bottom, "Bottom"
                    )
                )
                right.clicked.connect(
                    lambda: show_coach(
                        right, fluentqt.CoachMark.Placement.Right, "Right"
                    )
                )
                top.clicked.connect(
                    lambda: show_coach(
                        top, fluentqt.CoachMark.Placement.Top, "Top"
                    )
                )
                """),
                _DIALOGS_IMPORTS,
            ),
        ),
    },
)
