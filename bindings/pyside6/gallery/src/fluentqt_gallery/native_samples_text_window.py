"""Standalone Gallery ports for native text fields and windowing."""

from __future__ import annotations

from textwrap import dedent

from .native_samples import register_source_samples


def _script(body: str, imports: str = "") -> str:
    prefix = "import fluentqt\n"
    if imports:
        prefix += imports.strip() + "\n"
    return prefix + "\n" + dedent(body).strip() + "\n"


_WIDGETS = "from PySide6.QtWidgets import QHBoxLayout, QVBoxLayout, QWidget"

_TEXT_IMPORTS = (
    "from PySide6.QtCore import QRectF, Qt\n"
    "from PySide6.QtGui import QPainter, QPen\n"
    "from PySide6.QtWidgets import (QHBoxLayout, QSizePolicy, QVBoxLayout, QWidget)\n"
    "from fluentqt_gallery.foundation_pages import _theme_tokens\n"
    "from fluentqt_gallery.window import gallery_window_editing_command_router"
)

_TEXT_HELPER = dedent(
    """
    class TextFieldSampleSurface(QWidget):
        def __init__(self, parent=None, spacing=12):
            super().__init__(parent)
            self.setSizePolicy(
                QSizePolicy.Policy.MinimumExpanding,
                QSizePolicy.Policy.Fixed,
            )
            layout = QVBoxLayout(self)
            layout.setContentsMargins(16, 14, 16, 16)
            layout.setSpacing(spacing)
            layout.setAlignment(
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
                QRectF(self.rect()).adjusted(0.0, 0.0, -1.0, -1.0),
                8.0,
                8.0,
            )


    class ElideValueCell(QWidget):
        def __init__(self, text, mode, label_width, parent=None):
            super().__init__(parent)
            self.setFixedSize(label_width + 22, 34)
            self.setSizePolicy(
                QSizePolicy.Policy.Fixed,
                QSizePolicy.Policy.Fixed,
            )
            layout = QHBoxLayout(self)
            layout.setContentsMargins(10, 0, 10, 0)
            layout.setSpacing(0)
            label = fluentqt.Label(text, self)
            label.setFluentTypography(fluentqt.FontRole.Body)
            label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
            label.setTextElideMode(mode)
            label.setAlignment(
                Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
            )
            label.setFixedWidth(label_width)
            label.setToolTip(text)
            layout.addWidget(label, 0, Qt.AlignmentFlag.AlignVCenter)

        def paintEvent(self, event):
            del event
            colors = _theme_tokens(self)
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setPen(QPen(colors["strokeDefault"], 1.0))
            painter.setBrush(colors["controlDefault"])
            painter.drawRoundedRect(
                QRectF(self.rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                4.0,
                4.0,
            )


    def make_text_surface(spacing=12):
        surface = TextFieldSampleSurface(
            globals().get("gallery_parent"), spacing
        )
        return surface, surface.layout()


    def make_status_label(parent, text):
        label = fluentqt.Label(text, parent)
        label.setFluentTypography(fluentqt.FontRole.Body)
        label.setWordWrap(True)
        label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        return label


    def horizontal_group(parent, spacing=12):
        group = QWidget(parent)
        layout = QHBoxLayout(group)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(spacing)
        layout.setAlignment(
            Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
        )
        return group, layout


    def add_labeled_widget(parent, layout, label_text, widget):
        row, row_layout = horizontal_group(parent, 12)
        label = make_status_label(row, label_text)
        label.setMinimumWidth(
            label.fontMetrics().horizontalAdvance("Header placeholder")
        )
        widget.setParent(row)
        row_layout.addWidget(label, 0, Qt.AlignmentFlag.AlignVCenter)
        row_layout.addWidget(widget, 0, Qt.AlignmentFlag.AlignVCenter)
        row_layout.addStretch(1)
        layout.addWidget(row)


    def add_elide_row(parent, layout, caption, text, mode, width):
        row, row_layout = horizontal_group(parent, 12)
        label = make_status_label(row, caption)
        label.setFluentTypography(fluentqt.FontRole.Caption)
        label.setMinimumWidth(
            label.fontMetrics().horizontalAdvance("ElideMiddle") + 6
        )
        value = ElideValueCell(text, mode, width, row)
        row_layout.addWidget(label, 0, Qt.AlignmentFlag.AlignVCenter)
        row_layout.addWidget(value, 0, Qt.AlignmentFlag.AlignVCenter)
        row_layout.addStretch(1)
        layout.addWidget(row)
    """
).strip()


def _text_script(body: str, imports: str = "") -> str:
    combined_imports = _TEXT_IMPORTS
    if imports:
        combined_imports += "\n" + imports.strip()
    return _script(_TEXT_HELPER + "\n\n" + dedent(body).strip(), combined_imports)


_TITLE_BAR_IMPORTS = (
    "from PySide6.QtCore import QCoreApplication, QEvent, QPoint, QSize, QTimer, Qt\n"
    "from PySide6.QtGui import QFont, QPainter, QPen\n"
    "from PySide6.QtWidgets import (QHBoxLayout, QSizePolicy, QVBoxLayout, QWidget)\n"
    "from fluentqt_gallery.foundation_pages import _theme_tokens"
)

_TITLE_BAR_HELPER = dedent(
    """
    class SampleStatusPill(QWidget):
        def __init__(self, text, parent=None):
            super().__init__(parent)
            self.setAutoFillBackground(False)
            self.setSizePolicy(
                QSizePolicy.Policy.Fixed,
                QSizePolicy.Policy.Fixed,
            )
            self.setFixedHeight(28)
            layout = QHBoxLayout(self)
            layout.setContentsMargins(10, 3, 10, 3)
            layout.setSpacing(0)
            self.label = fluentqt.Label(text, self)
            self.label.setFluentTypography(fluentqt.FontRole.Caption)
            self.label.setTextColorRole(
                fluentqt.Label.TextColorRole.Secondary
            )
            self.label.setAlignment(Qt.AlignmentFlag.AlignCenter)
            self.label.setWordWrap(False)
            layout.addWidget(
                self.label, 0, Qt.AlignmentFlag.AlignCenter
            )

        def setText(self, text):
            self.label.setText(text)

        def paintEvent(self, event):
            del event
            colors = _theme_tokens(self)
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setPen(QPen(colors["strokeCard"], 1.0))
            painter.setBrush(colors["controlSecondary"])
            painter.drawRoundedRect(
                self.rect().adjusted(0, 0, -1, -1), 4.0, 4.0
            )


    def make_windows_caption_button(parent, glyph, tooltip, critical=False):
        button = fluentqt.Button(parent)
        fluentqt.ToolTip.attach(button, tooltip)
        button.setFluentStyle(fluentqt.Button.ButtonStyle.Subtle)
        button.setFluentLayout(fluentqt.Button.ButtonLayout.IconOnly)
        button.setFluentSize(fluentqt.Button.ButtonSize.Small)
        button.setIconGlyph(glyph)
        button.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        button.setCriticalOnHover(critical)
        return button


    class TitleBarPreview(QWidget):
        CAPTION_BUTTON_WIDTH = 46
        CAPTION_RESERVED_WIDTH = 138

        def __init__(self, width, parent=None):
            super().__init__(parent)
            self.title_bar = fluentqt.TitleBar(self)
            self.setFixedSize(
                width, fluentqt.TitleBar.defaultTitleBarHeight()
            )
            self.caption_buttons = QWidget(self.title_bar)
            caption_layout = QHBoxLayout(self.caption_buttons)
            caption_layout.setContentsMargins(0, 0, 0, 0)
            caption_layout.setSpacing(0)
            caption_layout.addWidget(
                make_windows_caption_button(
                    self.caption_buttons, "\ue921", "Minimize"
                )
            )
            caption_layout.addWidget(
                make_windows_caption_button(
                    self.caption_buttons, "\ue922", "Maximize"
                )
            )
            caption_layout.addWidget(
                make_windows_caption_button(
                    self.caption_buttons, "\ue8bb", "Close", True
                )
            )
            self.title_bar.setSystemReservedLeadingWidth(0)
            self.title_bar.setSystemReservedTrailingWidth(
                self.CAPTION_RESERVED_WIDTH
            )
            self.title_bar.titleBarHeightChanged.connect(
                self._title_bar_height_changed
            )
            self.update_chrome_geometry()
            QCoreApplication.sendEvent(
                self.title_bar,
                QEvent(QEvent.Type.WindowActivate),
            )

        def _title_bar_height_changed(self, height):
            self.setFixedHeight(height)
            self.update_chrome_geometry()

        def update_chrome_geometry(self):
            self.title_bar.setGeometry(self.rect())
            self.caption_buttons.setGeometry(
                self.width() - self.CAPTION_RESERVED_WIDTH,
                0,
                self.CAPTION_RESERVED_WIDTH,
                self.height(),
            )
            for button in self.caption_buttons.findChildren(fluentqt.Button):
                button.setFixedSize(
                    self.CAPTION_BUTTON_WIDTH, self.height()
                )
            self.caption_buttons.raise_()

        def resizeEvent(self, event):
            super().resizeEvent(event)
            self.update_chrome_geometry()


    def make_title_bar_content(parent, title_text, include_search):
        content = QWidget(parent)
        layout = QHBoxLayout(content)
        layout.setContentsMargins(8, 0, 8, 0)
        layout.setSpacing(8)
        layout.setAlignment(Qt.AlignmentFlag.AlignVCenter)
        title = fluentqt.Label(title_text, content)
        title.setFluentTypography(fluentqt.FontRole.Caption)
        title.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        title.setWordWrap(True)
        title.setMinimumWidth(96)
        title_font = QFont(title.font())
        title_font.setWeight(QFont.Weight.DemiBold)
        title.setFont(title_font)
        layout.addWidget(title, 0, Qt.AlignmentFlag.AlignVCenter)
        if include_search:
            search = fluentqt.AutoSuggestBox(content)
            search.setPlaceholderText("Search")
            search.setSuggestions(
                ["TitleBar", "Window", "Drag regions"]
            )
            search.setQueryIconVisible(False)
            search.setFontRole(fluentqt.FontRole.Caption)
            search.setSuggestionFontRole(fluentqt.FontRole.Caption)
            search.setInputHeight(28)
            search.setQueryButtonSize(16)
            search.setClearButtonSize(16)
            search.setSuggestionItemHeight(24)
            search.setFixedSize(160, 28)
            layout.addWidget(search, 0, Qt.AlignmentFlag.AlignVCenter)
        else:
            layout.addStretch(1)
        share = fluentqt.Button("Share", content)
        share.setFluentSize(fluentqt.Button.ButtonSize.Small)
        share.setFluentStyle(fluentqt.Button.ButtonStyle.Subtle)
        share.setMinimumWidth(64)
        layout.addWidget(share, 0, Qt.AlignmentFlag.AlignVCenter)
        return content


    def make_sample_label(parent, text, role=fluentqt.FontRole.Body):
        label = fluentqt.Label(text, parent)
        label.setFluentTypography(role)
        label.setWordWrap(True)
        label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        return label


    def make_command_button(parent, text, glyph):
        button = make_sample_button(
            parent, text, fluentqt.Button.ButtonStyle.Accent
        )
        button.setFluentLayout(fluentqt.Button.ButtonLayout.IconBefore)
        button.setIconGlyph(glyph)
        return button


    def make_sample_button(
        parent,
        text,
        style=fluentqt.Button.ButtonStyle.Standard,
    ):
        button = fluentqt.Button(text, parent)
        button.setFluentSize(fluentqt.Button.ButtonSize.Small)
        button.setFluentStyle(style)
        button.setMinimumWidth(84)
        return button


    def make_window_content(heading, body):
        content = QWidget()
        layout = QVBoxLayout(content)
        layout.setContentsMargins(24, 24, 24, 24)
        layout.setSpacing(12)
        layout.addWidget(
            make_sample_label(
                content, heading, fluentqt.FontRole.Subtitle
            )
        )
        layout.addWidget(make_sample_label(content, body))
        layout.addStretch(1)
        return content
    """
)


register_source_samples(
    "auto-suggest-box",
    ("AutoSuggestBox",),
    {
        "auto-suggest-box-suggestions": (
            "root",
            _text_script(
                """
                root, layout = make_text_surface()
                box = fluentqt.AutoSuggestBox(root)
                box.setHeader("Fruit")
                box.setPlaceholderText("Type a fruit name")
                box.setSuggestions(
                    [
                        "Apple",
                        "Apricot",
                        "Banana",
                        "Blueberry",
                        "Cherry",
                        "Grape",
                        "Orange",
                        "Strawberry",
                    ]
                )
                box.setFixedWidth(320)
                status = make_status_label(
                    root, "Text: , reason: ProgrammaticChange"
                )
                status.setMinimumWidth(
                    status.fontMetrics().horizontalAdvance(
                        "Submitted: Strawberry (Strawberry)"
                    )
                )

                def reason_name(reason):
                    name = reason.name
                    return name.decode("ascii") if isinstance(name, bytes) else str(name)

                box.textChangedWithReason.connect(
                    lambda text, reason: status.setText(
                        f"Text: {text}, reason: {reason_name(reason)}"
                    )
                )
                box.suggestionChosen.connect(
                    lambda item: status.setText(f"Chosen: {item}")
                )
                box.querySubmitted.connect(
                    lambda query, chosen: status.setText(f"Submitted: {query} ({chosen})")
                )
                layout.addWidget(box)
                layout.addWidget(status)
                """
            ),
        ),
        "auto-suggest-box-query-placement": (
            "root",
            _text_script(
                """
                root, layout = make_text_surface()
                box = fluentqt.AutoSuggestBox(root)
                box.setPlaceholderText("Filter commands")
                box.setSuggestions([
                    "Open", "Open recent", "Save", "Save as", "Settings", "Sync"
                ])
                box.setQueryButtonPlacement(
                    fluentqt.AutoSuggestBox.QueryButtonPlacement.Left
                )
                box.setQueryIconGlyph("ic_fluent_filter_20_regular")
                box.setInputHeight(28)
                box.setQueryButtonSize(20)
                box.setClearButtonSize(18)
                box.setSuggestionFontRole(fluentqt.FontRole.Caption)
                box.setSuggestionItemHeight(28)
                box.setFixedWidth(320)
                status = make_status_label(root, "Filter:")
                box.querySubmitted.connect(
                    lambda query, _chosen: status.setText(f"Filter: {query}")
                )
                layout.addWidget(box)
                layout.addWidget(status)
                """
            ),
        ),
    },
)


register_source_samples(
    "label",
    ("Label",),
    {
        "label-typography": (
            "root",
            _text_script(
                """
                root, layout = make_text_surface(8)
                roles = (
                    (fluentqt.FontRole.Caption, "Caption"),
                    (fluentqt.FontRole.Body, "Body"),
                    (fluentqt.FontRole.BodyStrong, "Body strong"),
                    (fluentqt.FontRole.Subtitle, "Subtitle"),
                    (fluentqt.FontRole.Title, "Title"),
                )
                for role, text in roles:
                    label = fluentqt.Label(text, root)
                    label.setFluentTypography(role)
                    label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                    layout.addWidget(label)
                """
            ),
        ),
        "label-elide": (
            "root",
            _text_script(
                """
                root, layout = make_text_surface(10)
                add_elide_row(
                    root,
                    layout,
                    "ElideRight",
                    "Quarterly release summary for the planning review",
                    Qt.TextElideMode.ElideRight,
                    190,
                )
                add_elide_row(
                    root,
                    layout,
                    "ElideMiddle",
                    "src/components/textfields/examples/LabelSample.cpp",
                    Qt.TextElideMode.ElideMiddle,
                    260,
                )
                add_elide_row(
                    root,
                    layout,
                    "ElideLeft",
                    "fluent-qt-release-textfields-label-preview-bundle",
                    Qt.TextElideMode.ElideLeft,
                    240,
                )
                """
            ),
        ),
    },
)


register_source_samples(
    "line-edit",
    ("LineEdit",),
    {
        "line-edit-clear": (
            "root",
            _text_script(
                """
                root, layout = make_text_surface()
                line_edit = fluentqt.LineEdit(root)
                line_edit.setPlaceholderText("Enter your name")
                line_edit.setClearButtonEnabled(True)
                line_edit.setFixedWidth(300)
                status = make_status_label(root, "Text length: 0")
                line_edit.textChanged.connect(
                    lambda text: status.setText(f"Text length: {len(text)}")
                )
                layout.addWidget(line_edit)
                layout.addWidget(status)
                """
            ),
        ),
        "line-edit-editing-commands": (
            "root",
            _text_script(
                """
                root, layout = make_text_surface()
                router = gallery_window_editing_command_router(root)
                menu_bar = fluentqt.FluentMenuBar(root)
                menu_bar.setBackgroundVisible(False)
                menu_bar.setFixedWidth(360)
                edit_menu = fluentqt.FluentMenu("Edit", menu_bar)
                Command = fluentqt.EditingCommandRouter.Command
                for command in (Command.Undo, Command.Redo):
                    edit_menu.addAction(router.action(command))
                edit_menu.addSeparator()
                for command in (Command.Cut, Command.Copy, Command.Paste, Command.Delete):
                    edit_menu.addAction(router.action(command))
                edit_menu.addSeparator()
                edit_menu.addAction(router.action(Command.SelectAll))
                menu_bar.addMenu(edit_menu)
                line_edit = fluentqt.LineEdit(root)
                line_edit.setText("Edit this line")
                line_edit.setFixedWidth(360)
                text_edit = fluentqt.TextEdit(root)
                text_edit.setPlainText("The same actions follow this editor.")
                text_edit.setMinVisibleLines(2)
                text_edit.setMaxVisibleLines(2)
                text_edit.setFixedWidth(360)
                status = make_status_label(root, "No editing target")
                router.activeTargetChanged.connect(
                    lambda active: status.setText(
                        "Editing target active" if active else "No editing target"
                    )
                )
                for widget in (menu_bar, line_edit, text_edit, status):
                    layout.addWidget(widget)
                """,
            ),
        ),
        "line-edit-validator": (
            "root",
            _text_script(
                """
                root, layout = make_text_surface()
                line_edit = fluentqt.LineEdit(root)
                line_edit.setPlaceholderText("0-100")
                line_edit.setValidator(QIntValidator(0, 100, line_edit))
                line_edit.setText("42")
                line_edit.setFixedWidth(220)
                status = make_status_label(root, "Acceptable input")
                line_edit.textChanged.connect(
                    lambda _text: status.setText(
                        "Acceptable input" if line_edit.hasAcceptableInput() else "Out of range"
                    )
                )
                layout.addWidget(line_edit)
                layout.addWidget(status)
                """,
                "from PySide6.QtGui import QIntValidator",
            ),
        ),
        "line-edit-frame-metrics": (
            "root",
            _text_script(
                """
                root, layout = make_text_surface(10)
                emphasized = fluentqt.LineEdit(root)
                emphasized.setText("Border thickness demo")
                emphasized.setContentMargins(QMargins(10, 4, 10, 4))
                emphasized.setFocusedBorderWidth(3)
                emphasized.setUnfocusedBorderWidth(2)
                emphasized.setFixedWidth(300)
                inline = fluentqt.LineEdit(root)
                inline.setText("Frameless inline field")
                inline.setFrameVisible(False)
                inline.setClearButtonOffset(QPoint(12, 0))
                inline.setFixedWidth(300)
                add_labeled_widget(root, layout, "Emphasized", emphasized)
                add_labeled_widget(root, layout, "Frameless", inline)
                """,
                "from PySide6.QtCore import QMargins, QPoint",
            ),
        ),
    },
)


register_source_samples(
    "number-box",
    ("NumberBox",),
    {
        "number-box-expression": (
            "root",
            _text_script(
                """
                root, layout = make_text_surface()
                number_box = fluentqt.NumberBox(root)
                number_box.setHeader("Equation")
                number_box.setPlaceholderText("1 + 2^2")
                number_box.setAcceptsExpression(True)
                number_box.setFixedWidth(260)
                status = make_status_label(root, "Value: NaN")

                def update(value):
                    status.setText(
                        "Value: NaN" if math.isnan(value) else f"Value: {value:.2f}"
                    )

                number_box.valueChanged.connect(update)
                layout.addWidget(number_box)
                layout.addWidget(status)
                """,
                "import math",
            ),
        ),
        "number-box-spin-placement": (
            "root",
            _text_script(
                """
                root, layout = make_text_surface(10)
                for header, mode in (
                    ("Inline", fluentqt.NumberBox.SpinButtonPlacementMode.Inline),
                    ("Compact", fluentqt.NumberBox.SpinButtonPlacementMode.Compact),
                ):
                    box = fluentqt.NumberBox(root)
                    box.setHeader(header)
                    box.setRange(0, 100)
                    box.setSmallChange(5)
                    box.setLargeChange(25)
                    box.setValue(50)
                    box.setSpinButtonPlacementMode(mode)
                    box.setFixedWidth(260)
                    add_labeled_widget(root, layout, header, box)
                """
            ),
        ),
        "number-box-formatting": (
            "root",
            _text_script(
                """
                root, layout = make_text_surface()
                number_box = fluentqt.NumberBox(root)
                number_box.setHeader("Amount")
                number_box.setDisplayPrecision(2)
                number_box.setFormatStep(0.25)
                number_box.setValue(1.13)
                number_box.setFixedWidth(240)
                status = make_status_label(
                    root, f"Rounded: {number_box.value():.2f}"
                )
                number_box.valueChanged.connect(
                    lambda value: status.setText(f"Rounded: {value:.2f}")
                )
                layout.addWidget(number_box)
                layout.addWidget(status)
                """
            ),
        ),
    },
)


register_source_samples(
    "password-box",
    ("PasswordBox",),
    {
        "password-box-basic": (
            "root",
            _text_script(
                """
                root, layout = make_text_surface()
                password_box = fluentqt.PasswordBox(root)
                password_box.setPassword("Fluent123")
                password_box.setFixedWidth(300)
                status = make_status_label(root, "Length: 9")
                password_box.passwordChanged.connect(
                    lambda password: status.setText(f"Length: {len(password)}")
                )
                layout.addWidget(password_box)
                layout.addWidget(status)
                """
            ),
        ),
        "password-box-header-placeholder": (
            "password_box",
            _script(
                """
                password_box = fluentqt.PasswordBox(globals().get("gallery_parent"))
                password_box.setHeader("Password")
                password_box.setPlaceholderText("Enter your password")
                password_box.setFixedWidth(300)
                """
            ),
        ),
        "password-box-reveal-modes": (
            "root",
            _text_script(
                """
                root, layout = make_text_surface(10)
                for caption, text, mode in (
                    ("Peek", "Peek mode", fluentqt.PasswordBox.PasswordRevealMode.Peek),
                    ("Hidden", "Hidden mode", fluentqt.PasswordBox.PasswordRevealMode.Hidden),
                    ("Visible", "Visible mode", fluentqt.PasswordBox.PasswordRevealMode.Visible),
                ):
                    box = fluentqt.PasswordBox(root)
                    box.setPassword(text)
                    box.setPasswordRevealMode(mode)
                    box.setFixedWidth(280)
                    add_labeled_widget(root, layout, caption, box)
                """
            ),
        ),
    },
)


register_source_samples(
    "text-edit",
    ("TextEdit",),
    {
        "text-edit-visible-lines": (
            "root",
            _text_script(
                """
                root, layout = make_text_surface()
                text_edit = fluentqt.TextEdit(root)
                text_edit.setAccessibleName("Notes")
                text_edit.setPlaceholderText("Type your notes here")
                text_edit.setMinVisibleLines(2)
                text_edit.setMaxVisibleLines(4)
                text_edit.setPlainText("First line\\nSecond line")
                text_edit.setFixedWidth(360)

                def line_count_text():
                    count = max(1, text_edit.toPlainText().count("\\n") + 1)
                    return (
                        f"Lines: {count}, visible range: "
                        f"{text_edit.minVisibleLines()}-{text_edit.maxVisibleLines()}"
                    )

                status = make_status_label(root, line_count_text())
                text_edit.textChanged.connect(
                    lambda: status.setText(line_count_text())
                )
                layout.addWidget(text_edit)
                layout.addWidget(status)
                """
            ),
        ),
        "text-edit-scrollable-content": (
            "text_edit",
            _script(
                """
                text_edit = fluentqt.TextEdit(globals().get("gallery_parent"))
                text_edit.setAccessibleName("Scrollable notes")
                text_edit.setMinVisibleLines(3)
                text_edit.setMaxVisibleLines(3)
                text_edit.setLineHeight(28)
                text_edit.setPlainText("Alpha\\nBeta\\nGamma\\nDelta\\nEpsilon\\nZeta")
                text_edit.setFixedWidth(360)
                """
            ),
        ),
        "text-edit-read-only": (
            "text_edit",
            _script(
                """
                text_edit = fluentqt.TextEdit(globals().get("gallery_parent"))
                text_edit.setAccessibleName("Review terms")
                text_edit.setPlainText("Terms reviewed and locked for approval.")
                text_edit.setReadOnly(True)
                text_edit.setFontRole(fluentqt.FontRole.BodyStrong)
                text_edit.setContentMargins(QMargins(12, 6, 12, 6))
                text_edit.setFixedWidth(360)
                """,
                "from PySide6.QtCore import QMargins",
            ),
        ),
    },
)


register_source_samples(
    "title-bar",
    ("TitleBar",),
    {
        "title-bar-content-regions": (
            "root",
            _script(
                _TITLE_BAR_HELPER
                + dedent("""
                root = QWidget(globals().get("gallery_parent"))
                root_layout = QVBoxLayout(root)
                root_layout.setContentsMargins(0, 0, 0, 0)
                root_layout.setSpacing(8)
                root_layout.setAlignment(
                    Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                )
                preview = TitleBarPreview(620, root)
                title_bar = preview.title_bar
                title_bar.setContentWidget(
                    make_title_bar_content(
                        title_bar, "Project window", True
                    )
                )
                status = SampleStatusPill("", root)

                def update_status():
                    status.setText(
                        f"Leading {title_bar.systemReservedLeadingWidth()} px | "
                        f"Trailing {title_bar.systemReservedTrailingWidth()} px | "
                        f"{len(title_bar.dragExclusionRects())} exclusions"
                    )

                title_bar.chromeGeometryChanged.connect(update_status)
                deferred_status = QTimer(title_bar)
                deferred_status.setSingleShot(True)
                deferred_status.timeout.connect(update_status)
                deferred_status.start(0)
                root_layout.addWidget(preview)
                root_layout.addWidget(status)
                """),
                _TITLE_BAR_IMPORTS,
            ),
        ),
        "title-bar-height-exclusions": (
            "root",
            _script(
                _TITLE_BAR_HELPER
                + dedent("""
                root = QWidget(globals().get("gallery_parent"))
                root_layout = QVBoxLayout(root)
                root_layout.setContentsMargins(0, 0, 0, 0)
                root_layout.setSpacing(8)
                root_layout.setAlignment(
                    Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                )
                preview = TitleBarPreview(620, root)
                title_bar = preview.title_bar
                title_bar.setTitleBarHeight(48)
                title_bar.setContentWidget(
                    make_title_bar_content(title_bar, "Review", False)
                )

                controls = QWidget(root)
                controls_layout = QHBoxLayout(controls)
                controls_layout.setContentsMargins(0, 0, 0, 0)
                controls_layout.setSpacing(8)
                controls_layout.setAlignment(
                    Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                )
                compact = make_sample_button(controls, "Compact")
                tall = make_sample_button(
                    controls,
                    "Tall",
                    fluentqt.Button.ButtonStyle.Accent,
                )
                refresh = make_sample_button(controls, "Refresh")
                refresh.setFluentStyle(fluentqt.Button.ButtonStyle.Subtle)
                refresh.setFluentLayout(fluentqt.Button.ButtonLayout.IconBefore)
                refresh.setIconGlyph("\ue72c")
                status = SampleStatusPill("", controls)

                def update_status():
                    status.setText(
                        f"{title_bar.titleBarHeight()} px height | "
                        f"{len(title_bar.dragExclusionRects())} exclusions"
                    )
                    compact.setFluentStyle(
                        fluentqt.Button.ButtonStyle.Accent
                        if title_bar.titleBarHeight() <= 32
                        else fluentqt.Button.ButtonStyle.Standard
                    )
                    tall.setFluentStyle(
                        fluentqt.Button.ButtonStyle.Standard
                        if title_bar.titleBarHeight() <= 32
                            else fluentqt.Button.ButtonStyle.Accent
                    )
                title_bar.titleBarHeightChanged.connect(
                    lambda _height: update_status()
                )
                title_bar.chromeGeometryChanged.connect(update_status)
                compact.clicked.connect(
                    lambda: title_bar.setTitleBarHeight(32)
                )
                tall.clicked.connect(
                    lambda: title_bar.setTitleBarHeight(48)
                )
                refresh.clicked.connect(
                    title_bar.refreshChromeExclusions
                )
                controls_layout.addWidget(
                    compact, 0, Qt.AlignmentFlag.AlignVCenter
                )
                controls_layout.addWidget(
                    tall, 0, Qt.AlignmentFlag.AlignVCenter
                )
                controls_layout.addWidget(
                    refresh, 0, Qt.AlignmentFlag.AlignVCenter
                )
                controls_layout.addWidget(
                    status, 0, Qt.AlignmentFlag.AlignVCenter
                )
                deferred_status = QTimer(title_bar)
                deferred_status.setSingleShot(True)
                deferred_status.timeout.connect(update_status)
                deferred_status.start(0)
                root_layout.addWidget(preview)
                root_layout.addWidget(controls)
                """),
                _TITLE_BAR_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "window",
    ("Window", "TitleBar"),
    {
        "window-content-host": (
            "root",
            _script(
                _TITLE_BAR_HELPER
                + dedent("""
                root = QWidget(globals().get("gallery_parent"))
                root_layout = QVBoxLayout(root)
                root_layout.setContentsMargins(0, 0, 0, 0)
                root_layout.setSpacing(8)
                root_layout.setAlignment(
                    Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                )
                controls = QWidget(root)
                controls_layout = QHBoxLayout(controls)
                controls_layout.setContentsMargins(0, 0, 0, 0)
                controls_layout.setSpacing(10)
                controls_layout.setAlignment(
                    Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                )
                launch = make_command_button(
                    controls, "Open window", "\ue73f"
                )
                status = SampleStatusPill("Closed", controls)
                controls_layout.addWidget(
                    launch, 0, Qt.AlignmentFlag.AlignVCenter
                )
                controls_layout.addWidget(
                    status, 0, Qt.AlignmentFlag.AlignVCenter
                )
                root_layout.addWidget(controls)
                state = {"window": None}

                def open_window():
                    window = state["window"]
                    if window is not None:
                        window.showNormal()
                        window.requestForegroundActivation()
                        status.setText("Focused")
                        return
                    window = fluentqt.Window()
                    state["window"] = window
                    window.setAttribute(
                        Qt.WidgetAttribute.WA_DeleteOnClose, True
                    )
                    window.setWindowTitle("Fluent content window")
                    window.setCustomWindowChromeEnabled(True)
                    window.setBackdropEffect(fluentqt.BackdropEffect.Mica)
                    content = make_window_content(
                        "Hosted content",
                        "This widget is parented under Window::contentHost().",
                    )
                    content.setAutoFillBackground(False)
                    window.setContentWidget(content)

                    def window_destroyed():
                        state["window"] = None
                        status.setText("Closed")

                    window.destroyed.connect(window_destroyed)
                    size = QSize(640, 520)
                    window.resize(size)
                    center = root.mapToGlobal(root.rect().center())
                    window.move(
                        center - QPoint(size.width() // 2, size.height() // 2)
                    )
                    window.show()
                    window.requestForegroundActivation()
                    status.setText("Open")

                launch.clicked.connect(open_window)
                """),
                _TITLE_BAR_IMPORTS,
            ),
        ),
        "window-custom-titlebar": (
            "root",
            _script(
                _TITLE_BAR_HELPER
                + dedent("""
                root = QWidget(globals().get("gallery_parent"))
                root_layout = QVBoxLayout(root)
                root_layout.setContentsMargins(0, 0, 0, 0)
                root_layout.setSpacing(8)
                root_layout.setAlignment(
                    Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                )
                controls = QWidget(root)
                controls_layout = QHBoxLayout(controls)
                controls_layout.setContentsMargins(0, 0, 0, 0)
                controls_layout.setSpacing(10)
                controls_layout.setAlignment(
                    Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                )
                launch = make_command_button(
                    controls, "Open custom", "\ue73f"
                )
                status = SampleStatusPill("Closed", controls)
                controls_layout.addWidget(
                    launch, 0, Qt.AlignmentFlag.AlignVCenter
                )
                controls_layout.addWidget(
                    status, 0, Qt.AlignmentFlag.AlignVCenter
                )
                root_layout.addWidget(controls)
                state = {"window": None}

                def open_window():
                    window = state["window"]
                    if window is not None:
                        window.showNormal()
                        window.requestForegroundActivation()
                        status.setText("Focused")
                        return
                    window = fluentqt.Window()
                    state["window"] = window
                    window.setAttribute(
                        Qt.WidgetAttribute.WA_DeleteOnClose, True
                    )
                    window.setWindowTitle("Custom title bar")
                    window.setCustomWindowChromeEnabled(True)
                    window.setBackdropEffect(fluentqt.BackdropEffect.Mica)
                    window.setCaptionButtonToolTips(
                        "Minimize", "Maximize", "Close", "Restore"
                    )
                    window.setCaptionButtonAccessibleNames(
                        "Minimize", "Maximize", "Close", "Restore"
                    )
                    window.titleBar().setContentWidget(
                        make_title_bar_content(
                            window.titleBar(), "Samples", True
                        )
                    )
                    window.titleBar().refreshChromeExclusions()
                    window.setContentWidget(make_window_content(
                        "Custom title bar",
                        "Interactive title-bar children are excluded from drag hit testing.",
                    ))

                    def window_destroyed():
                        state["window"] = None
                        status.setText("Closed")

                    window.destroyed.connect(window_destroyed)
                    size = QSize(720, 520)
                    window.resize(size)
                    center = root.mapToGlobal(root.rect().center())
                    window.move(
                        center - QPoint(size.width() // 2, size.height() // 2)
                    )
                    window.show()
                    window.requestForegroundActivation()
                    status.setText("Open")

                launch.clicked.connect(open_window)
                """),
                _TITLE_BAR_IMPORTS,
            ),
        ),
    },
)
