"""Shared public-API source for Popup, Dialog and DrawerView effect examples."""
from textwrap import dedent


def overlay_effects_source(kind: str, prefix: str, opener: str) -> str:
    body_setup = (
        "overlay.setEdge(fluentqt.DrawerView.DrawerEdge.Right)\n"
        "overlay.setDrawerLength(320)\n"
        "overlay.setInteractive(False)\n"
        "body = QWidget()\n"
        "overlay.setOwnedContentWidget(body)"
        if kind == "DrawerView" else
        "overlay.setThemeSource(open_button)\n"
        "overlay.setFixedWidth(400)\n"
        "body = overlay"
    )
    source = dedent('''
        import fluentqt
        from PySide6.QtCore import Qt
        from PySide6.QtWidgets import QHBoxLayout, QVBoxLayout, QWidget

        root = QWidget(globals().get("gallery_parent"))
        layout = QVBoxLayout(root)
        layout.setAlignment(Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft)
        layout.setSpacing(12)
        open_button = fluentqt.Button("@OPENER@", root)
        open_button.setObjectName("@PREFIX@.OpenEffects")
        open_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
        layout.addWidget(open_button, 0, Qt.AlignmentFlag.AlignLeft)
        hint = fluentqt.Label(
            "Open the overlay to try this action.", root
        )
        hint.setWordWrap(True)
        hint.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
        layout.addWidget(hint)
        row = QHBoxLayout()
        counter = fluentqt.Button("Background action", root)
        counter.setObjectName("@PREFIX@.Background")
        row.addWidget(counter)
        status = fluentqt.Label("Background clicks: 0", root)
        status.setObjectName("@PREFIX@.Status")
        status.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
        row.addWidget(status)
        row.addStretch()
        layout.addLayout(row)
        count = 0
        def count_background():
            global count
            count += 1
            status.setText(f"Background clicks: {count}")
        counter.clicked.connect(count_background)

        def open_effects():
            overlay = fluentqt.@KIND@(root)
            overlay.setObjectName("@PREFIX@.Effects")
            overlay.setModal(True)
            overlay.setDim(True)
            overlay.setDimOpacity(0.12)
            overlay.setBackdropBlurRadius(8)
            overlay.setClosePolicy(fluentqt.@KIND@.CloseOnEscape)
            @BODY_SETUP@
            content = QVBoxLayout(body)
            content.setContentsMargins(28, 26, 28, 26)
            content.setSpacing(10)
            title = fluentqt.Label("Modal and backdrop effects", body)
            title.setFluentTypography(fluentqt.FontRole.BodyStrong)
            title.setWordWrap(True)
            content.addWidget(title)
            modal = fluentqt.CheckBox("Modal", body)
            modal.setObjectName("@PREFIX@.Modal")
            modal.setChecked(True)
            modal.toggled.connect(overlay.setModal)
            def explain_modal(enabled):
                hint.setText(
                    "Modal on: background blocked." if enabled else
                    "Modal off: background available."
                )
            modal.toggled.connect(explain_modal)
            explain_modal(True)
            content.addWidget(modal)
            dim_enabled = fluentqt.CheckBox("Dim background", body)
            dim_enabled.setObjectName("@PREFIX@.DimEnabled")
            dim_enabled.setChecked(True)
            dim_enabled.toggled.connect(overlay.setDim)
            content.addWidget(dim_enabled)
            outside = fluentqt.CheckBox("Close on outside click", body)
            outside.setObjectName("@PREFIX@.CloseOutside")
            def set_outside_close(enabled):
                policy = fluentqt.@KIND@.CloseOnEscape
                if enabled:
                    policy |= fluentqt.@KIND@.CloseOnPressOutside
                overlay.setClosePolicy(fluentqt.@KIND@.ClosePolicy(policy))
            outside.toggled.connect(set_outside_close)
            content.addWidget(outside)
            blur_label = fluentqt.Label("Blur: 8 px", body)
            blur = fluentqt.Slider(Qt.Orientation.Horizontal, body)
            blur.setObjectName("@PREFIX@.Blur")
            blur.setAccessibleName("Background blur radius")
            blur.setRange(0, 24)
            blur.setValue(8)
            def update_blur(value):
                overlay.setBackdropBlurRadius(value)
                blur_label.setText(f"Blur: {value} px")
            blur.valueChanged.connect(update_blur)
            content.addWidget(blur_label)
            content.addWidget(blur)
            dim_label = fluentqt.Label("Dim: 12%", body)
            dim = fluentqt.Slider(Qt.Orientation.Horizontal, body)
            dim.setObjectName("@PREFIX@.Dim")
            dim.setAccessibleName("Background dim opacity")
            dim.setRange(0, 60)
            dim.setValue(12)
            def update_dim(value):
                overlay.setDimOpacity(value / 100.0)
                dim_label.setText(f"Dim: {value}%")
            dim.valueChanged.connect(update_dim)
            content.addWidget(dim_label)
            content.addWidget(dim)
            close = fluentqt.Button("Close", body)
            close.setObjectName("@PREFIX@.Close")
            close.clicked.connect(lambda: overlay.setIsOpen(False))
            content.addWidget(close, 0, Qt.AlignmentFlag.AlignRight)
            @STRETCH@
            counter.clicked.connect(overlay.refreshBackdrop)
            overlay.closed.connect(lambda: hint.setText(
                "Open the overlay to try this action."
            ))
            overlay.closed.connect(overlay.deleteLater)
            overlay.open()
        open_button.clicked.connect(open_effects)
    ''').strip() + "\n"
    return (source.replace("@KIND@", kind).replace("@PREFIX@", prefix)
            .replace("@OPENER@", opener)
            .replace("@BODY_SETUP@", body_setup.replace("\n", "\n    "))
            .replace("@STRETCH@", "content.addStretch()" if kind == "DrawerView" else ""))


def overlay_source_files(result):
    """Complete, copyable setup behind the short backdrop API excerpt."""
    full = result.preview_source.replace(
        "import fluentqt\n", "import fluentqt\n"
        "from PySide6.QtWidgets import QApplication\n"
        "fluentqt.prepare_high_dpi_application()\n"
        "application = QApplication.instance() or QApplication([])\n", 1
    )
    full += '\nif __name__ == "__main__":\n    root.show()\n    application.exec()\n'
    return [{"name": "backdrop.py", "language": "python", "code": full,
             "usage_snippet": result.source}]
