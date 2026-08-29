"""Standalone Gallery ports for native menus and navigation."""

from __future__ import annotations

from textwrap import dedent

from .native_samples import register_source_samples


def _script(body: str, imports: str = "") -> str:
    prefix = "import fluentqt\n"
    if imports:
        prefix += imports.strip() + "\n"
    return prefix + "\n" + dedent(body).strip() + "\n"


class _SourceHelper(str):
    """Dedent a sample body appended to a reusable source helper."""

    def __add__(self, other: str) -> str:
        return super().__add__(dedent(other))


_WIDGETS = (
    "from PySide6.QtCore import Qt\n"
    "from PySide6.QtWidgets import QHBoxLayout, QVBoxLayout, QWidget"
)

_MENUS_IMPORTS = (
    "from PySide6.QtCore import Qt\n"
    "from PySide6.QtGui import QAction, QActionGroup, QKeySequence, QPainter, QPen\n"
    "from PySide6.QtWidgets import QHBoxLayout, QSizePolicy, QVBoxLayout, QWidget\n"
    "from fluentqt_gallery.foundation_pages import _theme_tokens"
)

_MENUS_HELPER = _SourceHelper(dedent(
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
                self.rect().adjusted(0, 0, -1, -1),
                8.0,
                8.0,
            )


    def make_status_label(parent, text):
        label = fluentqt.Label(text, parent)
        label.setFluentTypography(fluentqt.FontRole.Body)
        label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        label.setWordWrap(True)
        return label


    def display_action_text(text):
        return text.split("\\t", 1)[0].replace("&", "")


    def add_status_action(menu, status, text, shortcut=None):
        action = menu.addAction(text)
        if shortcut is not None:
            action.setShortcut(shortcut)
        action.triggered.connect(
            lambda checked=False, value=text: status.setText(
                f"Clicked: {display_action_text(value)}"
            )
        )
        return action
    """
))

_COMMAND_FLYOUT_IMPORTS = (
    "from PySide6.QtCore import QRect, QRectF, QTimer, Qt\n"
    "from PySide6.QtGui import QAction, QColor, QFont, QIcon, QLinearGradient, QPainter, QPainterPath, QPen\n"
    "from PySide6.QtWidgets import QHBoxLayout, QSizePolicy, QVBoxLayout, QWidget\n"
    "from fluentqt_gallery.visual import gallery_font_icon_pixmap\n"
    "from fluentqt_gallery.window import gallery_window_editing_command_router"
)

_COMMAND_FLYOUT_HELPER = _SourceHelper(dedent(
    """
    class SampleSurface(fluentqt.FluentWidget):
        def __init__(self, parent=None, spacing=12):
            super().__init__(parent)
            self._action_glyphs = []
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
            tokens = self.theme_tokens()
            colors = tokens["colors"]
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setPen(QPen(colors["strokeCard"], 1.0))
            painter.setBrush(colors["bgCanvas"])
            radius = float(tokens["radius"]["overlay"])
            painter.drawRoundedRect(
                self.rect().adjusted(0, 0, -1, -1), radius, radius
            )

        def set_action_glyph(self, action, glyph):
            for index, (stored_action, _stored_glyph) in enumerate(
                self._action_glyphs
            ):
                if stored_action is action:
                    self._action_glyphs[index] = (action, glyph)
                    self._update_action_icon(action, glyph)
                    return
            self._action_glyphs.append((action, glyph))
            self._update_action_icon(action, glyph)

        def _update_action_icon(self, action, glyph):
            colors = self.theme_tokens()["colors"]
            icon = QIcon()
            for size in (16, 20, 24):
                normal = gallery_font_icon_pixmap(
                    glyph, size, colors["textPrimary"]
                )
                disabled = gallery_font_icon_pixmap(
                    glyph, size, colors["textDisabled"]
                )
                for mode, state in (
                    (QIcon.Mode.Normal, QIcon.State.Off),
                    (QIcon.Mode.Active, QIcon.State.Off),
                    (QIcon.Mode.Selected, QIcon.State.Off),
                    (QIcon.Mode.Normal, QIcon.State.On),
                    (QIcon.Mode.Active, QIcon.State.On),
                ):
                    icon.addPixmap(normal, mode, state)
                icon.addPixmap(
                    disabled, QIcon.Mode.Disabled, QIcon.State.Off
                )
                icon.addPixmap(
                    disabled, QIcon.Mode.Disabled, QIcon.State.On
                )
            action.setIcon(icon)

        def on_theme_updated(self):
            for action, glyph in self._action_glyphs:
                self._update_action_icon(action, glyph)
            super().on_theme_updated()


    class CommandPreviewPanel(fluentqt.FluentWidget):
        def __init__(self, parent=None):
            super().__init__(parent)
            self.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

        def paintEvent(self, event):
            del event
            tokens = self.theme_tokens()
            colors = tokens["colors"]
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setPen(QPen(colors["strokeCard"], 1.0))
            painter.setBrush(colors["bgLayer"])
            radius = float(tokens["radius"]["overlay"])
            painter.drawRoundedRect(
                self.rect().adjusted(0, 0, -1, -1), radius, radius
            )


    class ContextMediaTile(fluentqt.FluentWidget):
        def __init__(self, parent=None):
            super().__init__(parent)
            self._invoke_handler = None
            self._hovered = False
            self._keyboard_focus_visible = False
            self.setObjectName("Gallery.CommandBarFlyout.ContextTile")
            self.setFixedSize(560, 184)
            self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)
            self.setCursor(Qt.CursorShape.PointingHandCursor)
            self.setAccessibleName("Northern ridge photo")
            self.setAccessibleDescription(
                "Click for quick commands without moving focus, or right-click "
                "for the expanded context menu."
            )

        def set_invoke_handler(self, handler):
            self._invoke_handler = handler

        def _invoke(self, position, standard):
            if self._invoke_handler is not None:
                self._invoke_handler(position, standard)

        def paintEvent(self, event):
            del event
            tokens = self.theme_tokens()
            colors = tokens["colors"]
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
            painter.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform)

            background = (
                colors["subtleSecondary"] if self._hovered else colors["bgLayer"]
            )
            outline = (
                colors["accentDefault"]
                if self.hasFocus() and self._keyboard_focus_visible
                else colors["strokeCard"]
            )
            painter.setPen(QPen(outline, 1.0))
            painter.setBrush(background)
            radius = float(tokens["radius"]["overlay"])
            painter.drawRoundedRect(
                self.rect().adjusted(0, 0, -1, -1), radius, radius
            )

            photo = QRectF(12.0, 12.0, 276.0, 160.0)
            gradient = QLinearGradient(photo.topLeft(), photo.bottomRight())
            gradient.setColorAt(0.0, QColor("#355C7D"))
            gradient.setColorAt(1.0, QColor("#A7C6D9"))
            photo_path = QPainterPath()
            photo_path.addRoundedRect(photo, 8.0, 8.0)
            painter.fillPath(photo_path, gradient)
            photo_font = QFont()
            photo_font.setPixelSize(15)
            photo_font.setWeight(QFont.Weight.DemiBold)
            painter.setFont(photo_font)
            painter.setPen(QColor(255, 255, 255, 230))
            painter.drawText(
                photo.adjusted(16.0, 12.0, -16.0, -12.0),
                Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignBottom,
                "Northern ridge",
            )

            text_left = 312
            text_width = self.width() - text_left - 20
            painter.setPen(colors["textPrimary"])
            painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.BodyStrong))
            painter.drawText(
                QRect(text_left, 28, text_width, 24),
                Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter,
                "Northern ridge",
            )
            painter.setPen(colors["textSecondary"])
            painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.Caption))
            painter.drawText(
                QRect(text_left, 54, text_width, 20),
                Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter,
                "Photo · 4.8 MB",
            )
            painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))
            painter.drawText(
                QRect(text_left, 94, text_width, 48),
                Qt.AlignmentFlag.AlignLeft
                | Qt.AlignmentFlag.AlignTop
                | Qt.TextFlag.TextWordWrap,
                "Click: quick commands\\nRight-click: full context menu",
            )

        def enterEvent(self, event):
            super().enterEvent(event)
            self._hovered = True
            self.update()

        def leaveEvent(self, event):
            super().leaveEvent(event)
            self._hovered = False
            self.update()

        def mouseReleaseEvent(self, event):
            if (
                event.button() == Qt.MouseButton.LeftButton
                and self.rect().contains(event.position().toPoint())
            ):
                self._keyboard_focus_visible = False
                self.setFocus(Qt.FocusReason.MouseFocusReason)
                self._invoke(self.rect().center(), False)
                event.accept()
                return
            super().mouseReleaseEvent(event)

        def contextMenuEvent(self, event):
            keyboard = event.reason() == event.Reason.Keyboard
            self.setFocus(
                Qt.FocusReason.OtherFocusReason
                if keyboard
                else Qt.FocusReason.MouseFocusReason
            )
            self._keyboard_focus_visible = keyboard
            self.update()
            self._invoke(event.pos(), True)
            event.accept()

        def keyPressEvent(self, event):
            if event.key() in (
                Qt.Key.Key_Return,
                Qt.Key.Key_Enter,
                Qt.Key.Key_Space,
            ):
                self._keyboard_focus_visible = True
                self.update()
                self._invoke(self.rect().center(), True)
                event.accept()
                return
            super().keyPressEvent(event)

        def focusInEvent(self, event):
            self._keyboard_focus_visible = (
                event.reason() != Qt.FocusReason.MouseFocusReason
            )
            super().focusInEvent(event)
            self.update()

        def focusOutEvent(self, event):
            super().focusOutEvent(event)
            self.update()


    def make_hint_label(parent, text):
        label = fluentqt.Label(text, parent)
        label.setFluentTypography(fluentqt.FontRole.Caption)
        label.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
        label.setWordWrap(True)
        return label


    def make_sample_button(parent, text):
        button = fluentqt.Button(text, parent)
        button.setFluentSize(fluentqt.Button.ButtonSize.Small)
        button.setMinimumWidth(76)
        return button


    def make_preview_label(parent, text, role, color_role):
        label = fluentqt.Label(text, parent)
        label.setFluentTypography(role)
        label.setTextColorRole(color_role)
        return label


    def horizontal_group(parent, spacing):
        group = QWidget(parent)
        layout = QHBoxLayout(group)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(spacing)
        layout.setAlignment(
            Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
        )
        return group


    def vertical_group(parent, spacing):
        group = QWidget(parent)
        layout = QVBoxLayout(group)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(spacing)
        layout.setAlignment(
            Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
        )
        return group
    """
))


register_source_samples(
    "menu",
    ("FluentMenu",),
    {
        "menu-command-shortcuts": (
            "root",
            _script(_MENUS_HELPER + """
                root = SampleSurface(globals().get("gallery_parent"))
                status = make_status_label(root, "Clicked: (none)")
                button = fluentqt.DropDownButton("File", root)
                button.setMinimumWidth(120)

                menu = fluentqt.FluentMenu("", button)
                add_status_action(
                    menu, status, "New", QKeySequence.StandardKey.New
                )
                add_status_action(
                    menu, status, "Open...", QKeySequence.StandardKey.Open
                )
                add_status_action(
                    menu, status, "Save", QKeySequence.StandardKey.Save
                )
                menu.addSeparator()
                publish = menu.addAction("Publish")
                publish.setEnabled(False)
                add_status_action(menu, status, "Close")
                button.setMenu(menu)

                root.content_layout.addWidget(
                    button, 0, Qt.AlignmentFlag.AlignLeft
                )
                root.content_layout.addWidget(status)
                """, _MENUS_IMPORTS),
        ),
        "menu-cascading-selection": (
            "root",
            _script(
                _MENUS_HELPER + """
                root = SampleSurface(globals().get("gallery_parent"))
                status = make_status_label(root, "View: Comfortable list")
                button = fluentqt.DropDownButton("View options", root)
                button.setMinimumWidth(150)
                menu = fluentqt.FluentMenu("", button)
                sort_menu = fluentqt.FluentMenu("Sort by", menu)
                add_status_action(sort_menu, status, "Name")
                add_status_action(sort_menu, status, "Date modified")
                add_status_action(sort_menu, status, "Type")
                menu.addMenu(sort_menu)
                menu.addSeparator()
                view_group = QActionGroup(menu)
                view_group.setExclusive(True)

                def add_view_mode(text, checked=False):
                    action = menu.addAction(text)
                    action.setCheckable(True)
                    action.setChecked(checked)
                    view_group.addAction(action)
                    action.triggered.connect(
                        lambda _value=False, name=text: status.setText(
                            f"View: {name}"
                        )
                    )
                    return action

                add_view_mode("Compact list")
                add_view_mode("Comfortable list", True)
                add_view_mode("Large icons")
                hidden = menu.addAction("Show hidden files")
                hidden.setCheckable(True)
                hidden.triggered.connect(
                    lambda: status.setText(
                        "Hidden files: shown"
                        if hidden.isChecked()
                        else "Hidden files: hidden"
                    )
                )
                button.setMenu(menu)
                root.content_layout.addWidget(
                    button, 0, Qt.AlignmentFlag.AlignLeft
                )
                root.content_layout.addWidget(status)
                """,
                _MENUS_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "menu-bar",
    ("FluentMenuBar", "FluentMenu"),
    {
        "menu-bar-hosted-surface": (
            "root",
            _script(_MENUS_HELPER + """
                root = SampleSurface(globals().get("gallery_parent"))
                status = make_status_label(root, "Clicked: (none)")
                menu_bar = fluentqt.FluentMenuBar(root)
                menu_bar.setSizePolicy(
                    QSizePolicy.Policy.MinimumExpanding,
                    QSizePolicy.Policy.Fixed,
                )
                menu_bar.setMinimumWidth(340)
                menu_bar.setBackgroundVisible(False)

                file_menu = fluentqt.FluentMenu("File", menu_bar)
                add_status_action(file_menu, status, "New")
                add_status_action(file_menu, status, "Open...")
                add_status_action(file_menu, status, "Save")
                menu_bar.addMenu(file_menu)

                edit_menu = fluentqt.FluentMenu("Edit", menu_bar)
                add_status_action(
                    edit_menu, status, "Undo", QKeySequence.StandardKey.Undo
                )
                add_status_action(
                    edit_menu, status, "Cut", QKeySequence.StandardKey.Cut
                )
                add_status_action(
                    edit_menu, status, "Copy", QKeySequence.StandardKey.Copy
                )
                add_status_action(
                    edit_menu, status, "Paste", QKeySequence.StandardKey.Paste
                )
                menu_bar.addMenu(edit_menu)

                help_menu = fluentqt.FluentMenu("Help", menu_bar)
                add_status_action(help_menu, status, "About")
                menu_bar.addMenu(help_menu)

                root.content_layout.addWidget(menu_bar)
                root.content_layout.addWidget(status)
                """, _MENUS_IMPORTS),
        ),
        "menu-bar-access-keys": (
            "root",
            _script(
                _MENUS_HELPER + """
                root = SampleSurface(globals().get("gallery_parent"))
                status = make_status_label(root, "Command: (none)")
                row = QWidget(root)
                row_layout = QHBoxLayout(row)
                row_layout.setContentsMargins(0, 0, 0, 0)
                row_layout.setSpacing(8)
                row_layout.setAlignment(
                    Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                )
                focus = fluentqt.Button("Focus", row)
                focus.setFluentSize(fluentqt.Button.ButtonSize.Small)
                focus.setMinimumWidth(76)
                menu_bar = fluentqt.FluentMenuBar(row)
                menu_bar.setSizePolicy(
                    QSizePolicy.Policy.MinimumExpanding,
                    QSizePolicy.Policy.Fixed,
                )
                menu_bar.setMinimumWidth(390)
                file_menu = fluentqt.FluentMenu("&File", menu_bar)
                file_menu.menuAction().setProperty("accessKey", "F")
                add_status_action(
                    file_menu, status, "New", QKeySequence.StandardKey.New
                )
                add_status_action(
                    file_menu, status, "Save", QKeySequence.StandardKey.Save
                )
                menu_bar.addMenu(file_menu)

                view_menu = fluentqt.FluentMenu("&View", menu_bar)
                view_menu.menuAction().setProperty("accessKey", "V")
                add_status_action(view_menu, status, "Zoom in")
                add_status_action(view_menu, status, "Zoom out")
                menu_bar.addMenu(view_menu)

                run = QAction("Run", menu_bar)
                run.triggered.connect(lambda: status.setText("Command: Run"))
                menu_bar.addAction(run)
                deploy = QAction("Deploy", menu_bar)
                deploy.setEnabled(False)
                menu_bar.addAction(deploy)

                def focus_menu_bar():
                    menu_bar.setFocus(Qt.FocusReason.OtherFocusReason)
                    status.setText("Command: MenuBar focused")

                focus.clicked.connect(focus_menu_bar)
                row_layout.addWidget(focus, 0, Qt.AlignmentFlag.AlignVCenter)
                row_layout.addWidget(
                    menu_bar, 0, Qt.AlignmentFlag.AlignVCenter
                )
                root.content_layout.addWidget(row)
                root.content_layout.addWidget(status)
                """,
                _MENUS_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "command-bar",
    ("CommandBar",),
    {
        "command-bar-responsive-overflow": (
            "root",
            _script(_COMMAND_FLYOUT_HELPER + """
                root = SampleSurface(globals().get("gallery_parent"))
                status = make_hint_label(
                    root,
                    "Full width · all primary commands are visible",
                )

                panel = CommandPreviewPanel(root)
                panel.setFixedSize(560, 132)
                panel_layout = QVBoxLayout(panel)
                panel_layout.setContentsMargins(12, 10, 12, 12)
                panel_layout.setSpacing(8)

                bar_host = QWidget(panel)
                bar_host.setObjectName("Gallery.CommandBar.Host")
                bar_host.setFixedSize(536, 40)
                bar_layout = QHBoxLayout(bar_host)
                bar_layout.setContentsMargins(0, 0, 0, 0)
                bar_layout.setSpacing(0)

                bar = fluentqt.CommandBar(bar_host)
                bar.setObjectName("Gallery.CommandBar.Responsive")
                bar.setAccessibleName("Document commands")
                bar.setLabelPosition(fluentqt.CommandBar.LabelPosition.Right)
                bar.setBackgroundVisible(False)
                bar_layout.addWidget(bar)

                def make_action(text, glyph):
                    action = QAction(text, root)
                    root.set_action_glyph(action, glyph)
                    action.triggered.connect(
                        lambda checked=False, value=text: status.setText(
                            f"Command: {value}"
                        )
                    )
                    return action

                add_action = make_action(
                    "Add", fluentqt.Typography.Icons.Add
                )
                edit_action = make_action(
                    "Edit", fluentqt.Typography.Icons.Edit
                )
                edit_action.setPriority(QAction.Priority.HighPriority)
                share_action = make_action(
                    "Share", fluentqt.Typography.Icons.Share
                )
                separator = QAction(root)
                separator.setSeparator(True)
                sync_action = make_action(
                    "Sync", fluentqt.Typography.Icons.Sync
                )
                sync_action.setPriority(QAction.Priority.LowPriority)
                pin_action = make_action(
                    "Pin", fluentqt.Typography.Icons.Pin
                )
                pin_action.setCheckable(True)
                settings_action = make_action(
                    "Settings", fluentqt.Typography.Icons.Settings
                )
                help_action = make_action(
                    "Help", fluentqt.Typography.Icons.Info
                )

                for action in (
                    add_action,
                    edit_action,
                    share_action,
                    separator,
                    sync_action,
                    pin_action,
                ):
                    bar.addPrimaryAction(action)
                bar.addSecondaryAction(settings_action)
                bar.addSecondaryAction(help_action)

                document_row = horizontal_group(panel, 10)
                document_icon = fluentqt.FontIcon(
                    fluentqt.Typography.Icons.Document, document_row
                )
                document_icon.setIconSize(28)
                document_text = vertical_group(document_row, 1)
                document_title = make_preview_label(
                    document_text,
                    "Quarterly report",
                    fluentqt.FontRole.BodyStrong,
                    fluentqt.Label.TextColorRole.Primary,
                )
                document_meta = make_preview_label(
                    document_text,
                    "Edited just now · shared with 4 people",
                    fluentqt.FontRole.Caption,
                    fluentqt.Label.TextColorRole.Secondary,
                )
                document_text.layout().addWidget(document_title)
                document_text.layout().addWidget(document_meta)
                document_row.layout().addWidget(document_icon)
                document_row.layout().addWidget(document_text)
                panel_layout.addWidget(bar_host)
                panel_layout.addWidget(document_row)

                controls = horizontal_group(root, 8)
                compact = make_sample_button(controls, "Compact view")
                compact.setMinimumWidth(112)
                labels = make_sample_button(controls, "Labels: Right")
                labels.setMinimumWidth(112)
                background = make_sample_button(controls, "Show background")
                background.setMinimumWidth(128)
                controls.layout().addWidget(compact)
                controls.layout().addWidget(labels)
                controls.layout().addWidget(background)

                def toggle_compact():
                    compact_mode = bar_host.width() > 300
                    bar_host.setFixedWidth(288 if compact_mode else 536)
                    compact.setText("Full view" if compact_mode else "Compact view")
                    status.setText(
                        "Compact width · lower-priority commands moved to More"
                        if compact_mode
                        else "Full width · all primary commands are visible"
                    )

                def toggle_labels():
                    show_labels = (
                        bar.labelPosition()
                        == fluentqt.CommandBar.LabelPosition.Collapsed
                    )
                    bar.setLabelPosition(
                        fluentqt.CommandBar.LabelPosition.Right
                        if show_labels
                        else fluentqt.CommandBar.LabelPosition.Collapsed
                    )
                    labels.setText(
                        "Labels: Right" if show_labels else "Labels: Icons"
                    )
                    status.setText(
                        "Labels on the right · text remains available beside icons"
                        if show_labels
                        else "Icons only · accessible names still come from QAction text"
                    )

                def toggle_background():
                    visible = not bar.backgroundVisible()
                    bar.setBackgroundVisible(visible)
                    background.setText(
                        "Hide background" if visible else "Show background"
                    )
                    status.setText(
                        "Background on · use the self-contained command surface"
                        if visible
                        else "Background off · blend the commands into their host"
                    )

                compact.clicked.connect(toggle_compact)
                labels.clicked.connect(toggle_labels)
                background.clicked.connect(toggle_background)

                root.content_layout.addWidget(panel)
                root.content_layout.addWidget(controls)
                root.content_layout.addWidget(status)
                """, _COMMAND_FLYOUT_IMPORTS),
        ),
        "command-bar-editing-router": (
            "root",
            _script(
                _COMMAND_FLYOUT_HELPER + """
                root = SampleSurface(globals().get("gallery_parent"))
                status = make_hint_label(
                    root,
                    "Edit the note to enable Undo · select text to enable Cut and Copy",
                )
                panel = CommandPreviewPanel(root)
                panel.setFixedSize(560, 154)
                panel_layout = QVBoxLayout(panel)
                panel_layout.setContentsMargins(12, 10, 12, 12)
                panel_layout.setSpacing(8)

                heading = horizontal_group(panel, 8)
                heading_icon = fluentqt.FontIcon(
                    fluentqt.Typography.Icons.Edit, heading
                )
                heading_icon.setIconSize(20)
                heading_label = make_preview_label(
                    heading,
                    "Quick note",
                    fluentqt.FontRole.BodyStrong,
                    fluentqt.Label.TextColorRole.Primary,
                )
                heading.layout().addWidget(heading_icon)
                heading.layout().addWidget(heading_label)

                editor = fluentqt.LineEdit(panel)
                editor.setObjectName("Gallery.CommandBar.EditingTarget")
                editor.setText("Review the release notes before Friday")
                editor.setFixedWidth(536)
                router = gallery_window_editing_command_router(root)
                bar = fluentqt.CommandBar(panel)
                bar.setObjectName("Gallery.CommandBar.EditingRouter")
                bar.setAccessibleName("Editing commands")
                bar.setLabelPosition(fluentqt.CommandBar.LabelPosition.Right)
                bar.setBackgroundVisible(False)
                bar.setFixedWidth(536)
                Command = fluentqt.EditingCommandRouter.Command
                bar.addPrimaryAction(router.action(Command.Undo))
                bar.addPrimaryAction(router.action(Command.Redo))
                separator = QAction(root)
                separator.setSeparator(True)
                bar.addPrimaryAction(separator)
                for command in (Command.Cut, Command.Copy, Command.Paste):
                    bar.addPrimaryAction(router.action(command))
                bar.addSecondaryAction(router.action(Command.Delete))
                bar.addSecondaryAction(router.action(Command.SelectAll))
                for command, glyph in (
                    (Command.Undo, fluentqt.Typography.Icons.Undo),
                    (Command.Redo, fluentqt.Typography.Icons.Redo),
                    (Command.Cut, fluentqt.Typography.Icons.Cut),
                    (Command.Copy, fluentqt.Typography.Icons.Copy),
                    (Command.Paste, fluentqt.Typography.Icons.Paste),
                    (Command.Delete, fluentqt.Typography.Icons.Delete),
                    (Command.SelectAll, fluentqt.Typography.Icons.SelectAll),
                ):
                    root.set_action_glyph(router.action(command), glyph)

                panel_layout.addWidget(heading)
                panel_layout.addWidget(editor)
                panel_layout.addWidget(bar)

                controls = horizontal_group(root, 8)
                select_text = make_sample_button(controls, "Select text")
                clear_selection = make_sample_button(
                    controls, "Clear selection"
                )
                read_only = make_sample_button(controls, "Read-only: Off")
                read_only.setCheckable(True)
                controls.layout().addWidget(select_text)
                controls.layout().addWidget(clear_selection)
                controls.layout().addWidget(read_only)

                def availability_text():
                    state = lambda enabled: "on" if enabled else "off"
                    return (
                        f"Cut: {state(router.canExecute(Command.Cut))} · "
                        f"Copy: {state(router.canExecute(Command.Copy))} · "
                        f"Paste: {state(router.canExecute(Command.Paste))} · "
                        f"Read-only: {state(editor.isReadOnly())}"
                    )

                def update_availability(*_args):
                    status.setText(availability_text())

                router.activeTargetChanged.connect(update_availability)
                router.commandCapabilityChanged.connect(update_availability)

                def defer_editor_update(callback):
                    timer = QTimer(editor)
                    timer.setSingleShot(True)
                    timer.timeout.connect(callback)
                    timer.start(0)

                def select_all():
                    def apply():
                        editor.setFocus(Qt.FocusReason.OtherFocusReason)
                        editor.selectAll()
                        router.refresh()
                        update_availability()
                    defer_editor_update(apply)

                def deselect():
                    def apply():
                        editor.setFocus(Qt.FocusReason.OtherFocusReason)
                        editor.deselect()
                        router.refresh()
                        update_availability()
                    defer_editor_update(apply)

                def set_read_only(value):
                    read_only.setText(
                        "Read-only: On" if value else "Read-only: Off"
                    )
                    def apply():
                        editor.setFocus(Qt.FocusReason.OtherFocusReason)
                        editor.setReadOnly(value)
                        editor.selectAll()
                        router.refresh()
                        update_availability()
                    defer_editor_update(apply)

                select_text.clicked.connect(select_all)
                clear_selection.clicked.connect(deselect)
                read_only.toggled.connect(set_read_only)
                root.content_layout.addWidget(panel)
                root.content_layout.addWidget(controls)
                root.content_layout.addWidget(status)
                """,
                _COMMAND_FLYOUT_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "command-bar-flyout",
    ("CommandBarFlyout",),
    {
        "command-bar-flyout-show-modes": (
            "root",
            _script(
                _COMMAND_FLYOUT_HELPER
                + """
                root = SampleSurface(globals().get("gallery_parent"))
                layout = root.content_layout
                status = make_hint_label(
                    root,
                    "Click: Transient · right-click or keyboard: Standard",
                )
                photo = ContextMediaTile(root)
                flyout = fluentqt.CommandBarFlyout(root)
                flyout.setObjectName("Gallery.CommandBarFlyout")
                actions = {}
                for text in ("Share", "Save", "Delete"):
                    action = QAction(text, root)
                    action.triggered.connect(
                        lambda checked=False, item=action: status.setText(
                            "Command: {0}".format(item.text())
                        )
                    )
                    flyout.addPrimaryAction(action)
                    actions[text] = action
                for text in ("Resize", "Move"):
                    action = QAction(text, root)
                    action.triggered.connect(
                        lambda checked=False, item=action: status.setText(
                            "Command: {0}".format(item.text())
                        )
                    )
                    flyout.addSecondaryAction(action)
                    actions[text] = action
                for text, glyph in (
                    ("Share", fluentqt.Typography.Icons.Share),
                    ("Save", fluentqt.Typography.Icons.Save),
                    ("Delete", fluentqt.Typography.Icons.Delete),
                    ("Resize", fluentqt.Typography.Icons.FullScreen),
                    ("Move", fluentqt.Typography.Icons.Forward),
                ):
                    root.set_action_glyph(actions[text], glyph)

                def invoke(position, standard):
                    flyout.setAlwaysExpanded(False)
                    if standard:
                        flyout.showAtPoint(
                            photo,
                            position,
                            fluentqt.CommandBarFlyout.ShowMode.Standard,
                        )
                        status.setText("Standard · expanded and keyboard focused")
                    else:
                        flyout.showAt(
                            photo,
                            fluentqt.CommandBarFlyout.ShowMode.Transient,
                        )
                        status.setText("Transient · collapsed and focus preserved")

                photo.set_invoke_handler(invoke)
                layout.addWidget(photo)
                layout.addWidget(status)
                """,
                _COMMAND_FLYOUT_IMPORTS,
            ),
        ),
        "command-bar-flyout-always-expanded": (
            "root",
            _script(
                _COMMAND_FLYOUT_HELPER + """
                root = SampleSurface(globals().get("gallery_parent"))
                status = make_hint_label(
                    root,
                    "Always expanded · primary and secondary commands open together",
                )
                panel = CommandPreviewPanel(root)
                panel.setFixedSize(560, 88)
                panel_layout = QHBoxLayout(panel)
                panel_layout.setContentsMargins(16, 12, 16, 12)
                panel_layout.setSpacing(12)

                file_icon = fluentqt.FontIcon(
                    fluentqt.Typography.Icons.Document, panel
                )
                file_icon.setIconSize(28)
                file_text = vertical_group(panel, 1)
                file_title = make_preview_label(
                    file_text,
                    "Release-notes.md",
                    fluentqt.FontRole.BodyStrong,
                    fluentqt.Label.TextColorRole.Primary,
                )
                file_meta = make_preview_label(
                    file_text,
                    "Markdown · 18 KB",
                    fluentqt.FontRole.Caption,
                    fluentqt.Label.TextColorRole.Secondary,
                )
                file_text.layout().addWidget(file_title)
                file_text.layout().addWidget(file_meta)
                open_button = make_sample_button(panel, "Open actions")
                open_button.setObjectName(
                    "Gallery.CommandBarFlyout.OpenAlwaysExpanded"
                )
                panel_layout.addWidget(file_icon)
                panel_layout.addWidget(file_text, 1)
                panel_layout.addWidget(open_button)

                flyout = fluentqt.CommandBarFlyout(root)
                flyout.setObjectName(
                    "Gallery.CommandBarFlyout.AlwaysExpanded"
                )
                flyout.setAlwaysExpanded(True)
                copy_link = QAction("Copy link", root)
                favorite = QAction("Favorite", root)
                favorite.setCheckable(True)
                rename = QAction("Rename", root)
                properties = QAction("Properties", root)
                flyout.addPrimaryAction(copy_link)
                flyout.addPrimaryAction(favorite)
                flyout.addSecondaryAction(rename)
                flyout.addSecondaryAction(properties)
                for action, glyph in (
                    (copy_link, fluentqt.Typography.Icons.Link),
                    (favorite, fluentqt.Typography.Icons.FavoriteStar),
                    (rename, fluentqt.Typography.Icons.Edit),
                    (properties, fluentqt.Typography.Icons.Info),
                ):
                    root.set_action_glyph(action, glyph)

                controls = horizontal_group(root, 8)
                expanded = make_sample_button(
                    controls, "Always expanded: On"
                )
                expanded.setCheckable(True)
                expanded.setChecked(True)
                controls.layout().addWidget(expanded)

                def set_expanded(checked):
                    flyout.setAlwaysExpanded(checked)
                    expanded.setText(
                        "Always expanded: On"
                        if checked
                        else "Always expanded: Off"
                    )
                    status.setText(
                        "Always expanded · primary and secondary commands open together"
                        if checked
                        else "Collapsed · secondary commands are available behind More"
                    )

                def open_actions():
                    flyout.showAt(
                        open_button,
                        fluentqt.CommandBarFlyout.ShowMode.Transient,
                    )
                    status.setText(
                        "Transient + AlwaysExpanded · focus stays on Open actions"
                        if flyout.isAlwaysExpanded()
                        else "Transient · focus stays on Open actions; use More for secondary commands"
                    )

                expanded.toggled.connect(set_expanded)
                open_button.clicked.connect(open_actions)
                for action in (copy_link, favorite, rename, properties):
                    action.triggered.connect(
                        lambda _checked=False, item=action: status.setText(
                            f"Command: {item.text().split(chr(9), 1)[0].replace('&', '')}"
                        )
                    )

                root.content_layout.addWidget(panel)
                root.content_layout.addWidget(controls)
                root.content_layout.addWidget(status)
                """,
                _COMMAND_FLYOUT_IMPORTS,
            ),
        ),
    },
)


def _page_source_helper() -> str:
    return dedent(
        """
        def make_page(title, body=""):
            page = QWidget()
            page_layout = QVBoxLayout(page)
            page_layout.setContentsMargins(16, 14, 16, 14)
            page_layout.setSpacing(6)
            page_layout.setAlignment(
                Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
            )
            heading = fluentqt.Label(title, page)
            heading.setFluentTypography(fluentqt.FontRole.BodyStrong)
            heading.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
            summary = fluentqt.Label(body, page)
            summary.setFluentTypography(fluentqt.FontRole.Caption)
            summary.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
            summary.setWordWrap(True)
            page_layout.addWidget(heading)
            page_layout.addWidget(summary)
            return page


        def make_status_label(parent, text):
            label = fluentqt.Label(text, parent)
            label.setFluentTypography(fluentqt.FontRole.Body)
            label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
            label.setWordWrap(True)
            return label
        """
    )


register_source_samples(
    "breadcrumb",
    ("Breadcrumb", "BreadcrumbItem"),
    {
        "breadcrumb-size": (
            "root",
            _script(
                """
                root = QWidget(globals().get("gallery_parent"))
                layout = QVBoxLayout(root)
                layout.setContentsMargins(0, 0, 0, 0)
                layout.setSpacing(10)
                layout.setAlignment(
                    Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                )
                path = ["Home", "Documents", "Images"]

                standard_row = QWidget(root)
                standard_layout = QHBoxLayout(standard_row)
                standard_layout.setContentsMargins(0, 0, 0, 0)
                standard_layout.setSpacing(10)
                standard_layout.setAlignment(
                    Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                )
                standard_label = fluentqt.Label("Standard", standard_row)
                standard_label.setFluentTypography(fluentqt.FontRole.Caption)
                standard_label.setTextColorRole(
                    fluentqt.Label.TextColorRole.Primary
                )
                standard_label.setFixedWidth(72)
                standard = fluentqt.Breadcrumb(standard_row)
                standard.setItems(path)
                standard.setBreadcrumbSize(fluentqt.Breadcrumb.BreadcrumbSize.Standard)
                standard.setFixedSize(360, 20)
                standard_layout.addWidget(standard_label)
                standard_layout.addWidget(standard)

                large_row = QWidget(root)
                large_layout = QHBoxLayout(large_row)
                large_layout.setContentsMargins(0, 0, 0, 0)
                large_layout.setSpacing(10)
                large_layout.setAlignment(
                    Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                )
                large_label = fluentqt.Label("Large", large_row)
                large_label.setFluentTypography(fluentqt.FontRole.Caption)
                large_label.setTextColorRole(
                    fluentqt.Label.TextColorRole.Primary
                )
                large_label.setFixedWidth(72)
                large = fluentqt.Breadcrumb(large_row)
                large.setItems(path)
                large.setBreadcrumbSize(fluentqt.Breadcrumb.BreadcrumbSize.Large)
                large.setFixedSize(430, 40)
                large_layout.addWidget(large_label)
                large_layout.addWidget(large)

                layout.addWidget(standard_row)
                layout.addWidget(large_row)
                """,
                _WIDGETS,
            ),
        ),
        "breadcrumb-overflow-mode": (
            "root",
            _script(
                """
                root = QWidget(globals().get("gallery_parent"))
                layout = QVBoxLayout(root)
                layout.setContentsMargins(0, 0, 0, 0)
                layout.setSpacing(10)
                layout.setAlignment(
                    Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                )
                path = ["Home", "Projects", "Fluent", "Controls", "Breadcrumb"]
                values = (
                    ("None", fluentqt.Breadcrumb.OverflowMode.None_, 520),
                    ("Beginning", fluentqt.Breadcrumb.OverflowMode.Beginning, 260),
                    ("Middle", fluentqt.Breadcrumb.OverflowMode.Middle, 260),
                )
                status = fluentqt.Label("Hidden indexes: none", root)
                status.setFluentTypography(fluentqt.FontRole.Caption)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setWordWrap(True)

                def hidden_indexes_text(indexes):
                    return ", ".join(str(index) for index in indexes) or "none"

                for label_text, mode, width in values:
                    row = QWidget(root)
                    row_layout = QHBoxLayout(row)
                    row_layout.setContentsMargins(0, 0, 0, 0)
                    row_layout.setSpacing(10)
                    row_layout.setAlignment(
                        Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                    )
                    label = fluentqt.Label(label_text, row)
                    label.setFluentTypography(fluentqt.FontRole.Caption)
                    label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                    label.setFixedWidth(72)
                    breadcrumb = fluentqt.Breadcrumb(row)
                    breadcrumb.setItems(path)
                    breadcrumb.setOverflowMode(mode)
                    breadcrumb.setFixedSize(width, 20)
                    breadcrumb.overflowActivated.connect(
                        lambda indexes, name=label_text: status.setText(
                            f"{name} hidden indexes: {hidden_indexes_text(indexes)}"
                        )
                    )
                    row_layout.addWidget(label)
                    row_layout.addWidget(breadcrumb)
                    layout.addWidget(row)
                layout.addWidget(status)
                """,
                _WIDGETS,
            ),
        ),
        "breadcrumb-item-state": (
            "root",
            _script(
                """
                root = QWidget(globals().get("gallery_parent"))
                layout = QVBoxLayout(root)
                layout.setContentsMargins(0, 0, 0, 0)
                layout.setSpacing(8)
                layout.setAlignment(
                    Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                )
                items = [
                    fluentqt.BreadcrumbItem("Home", "home"),
                    fluentqt.BreadcrumbItem("Archive", "archive", False, "Archive folder"),
                    fluentqt.BreadcrumbItem("Reports", "reports"),
                    fluentqt.BreadcrumbItem("Current", "current"),
                ]
                breadcrumb = fluentqt.Breadcrumb(root)
                breadcrumb.setItems(items)
                breadcrumb.setFixedSize(520, 20)
                status = fluentqt.Label(
                    "Archive is disabled; activated item data appears here.", root
                )
                status.setFluentTypography(fluentqt.FontRole.Caption)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setWordWrap(True)
                breadcrumb.itemActivated.connect(
                    lambda index, item: status.setText(
                        f"Activated {item.text}, data: {item.data}, index: {index}"
                    )
                )
                layout.addWidget(breadcrumb)
                layout.addWidget(status)
                """
                ,
                _WIDGETS,
            ),
        ),
        "breadcrumb-auto-truncate": (
            "root",
            _script(
                """
                root = QWidget(globals().get("gallery_parent"))
                layout = QVBoxLayout(root)
                layout.setContentsMargins(0, 0, 0, 0)
                layout.setSpacing(10)
                layout.setAlignment(
                    Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                )
                full_path = ["This PC", "Local Disk", "Users", "Public", "Pictures"]
                breadcrumb = fluentqt.Breadcrumb(root)
                breadcrumb.setItems(full_path)
                breadcrumb.setAutoTruncateOnItemClick(True)
                breadcrumb.setFixedSize(520, 20)

                controls = QWidget(root)
                controls_layout = QHBoxLayout(controls)
                controls_layout.setContentsMargins(0, 0, 0, 0)
                controls_layout.setSpacing(8)
                controls_layout.setAlignment(
                    Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                )
                reset = fluentqt.Button("Reset", controls)
                reset.setFluentSize(fluentqt.Button.ButtonSize.Small)
                controls_layout.addWidget(reset)
                status = fluentqt.Label("Items: 5", root)
                status.setFluentTypography(fluentqt.FontRole.Caption)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setWordWrap(True)

                def update_items_status():
                    count = breadcrumb.itemCount()
                    if count == 0:
                        status.setText("Items: 0")
                    else:
                        status.setText(
                            f"Items: {count}, current: {breadcrumb.itemAt(count - 1).text}"
                        )

                breadcrumb.itemActivated.connect(
                    lambda index, item: status.setText(f"Activated {item.text} at index {index}")
                )
                breadcrumb.itemsChanged.connect(update_items_status)
                reset.clicked.connect(
                    lambda: (breadcrumb.setItems(full_path), status.setText("Items: 5"))
                )
                layout.addWidget(breadcrumb)
                layout.addWidget(controls)
                layout.addWidget(status)
                """,
                _WIDGETS,
            ),
        ),
        "breadcrumb-activation": (
            "root",
            _script(
                _page_source_helper()
                + dedent(
                    """
                    root = QWidget(globals().get("gallery_parent"))
                    layout = QVBoxLayout(root)
                    layout.setContentsMargins(0, 0, 0, 0)
                    layout.setSpacing(10)
                    layout.setAlignment(
                        Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                    )
                    full_path = ["Home", "Projects", "Fluent", "Controls", "Breadcrumb"]
                    breadcrumb = fluentqt.Breadcrumb(root)
                    breadcrumb.setItems(full_path)
                    breadcrumb.setOverflowMode(fluentqt.Breadcrumb.OverflowMode.None_)
                    breadcrumb.setFixedSize(540, 20)
                    host = fluentqt.StackContentHost(root)
                    host.setFixedSize(540, 126)
                    host.setTransitionAnimationEnabled(True)
                    for index, title in enumerate(full_path):
                        host.addOwnedPage(
                            make_page(
                                title,
                                f"StackContentHost page {index + 1} selected from a full breadcrumb trail.",
                            )
                        )
                    host.setCurrentIndex(len(full_path) - 1, 0, False)
                    status = make_status_label(root, "Current: Breadcrumb")
                    status.setFluentTypography(fluentqt.FontRole.Caption)

                    def jump_to(index):
                        bounded = max(0, min(index, len(full_path) - 1))
                        direction = 1 if bounded >= host.currentIndex() else -1
                        host.setCurrentIndex(bounded, direction, True)
                        status.setText(f"Current: {full_path[bounded]}")

                    breadcrumb.itemActivated.connect(
                        lambda index, _item: jump_to(index)
                    )
                    layout.addWidget(breadcrumb)
                    layout.addWidget(host)
                    layout.addWidget(status)
                    """
                ),
                _WIDGETS,
            ),
        ),
    },
)


_NAV_HELPER = dedent(
    """
    def make_navigation():
        nav = fluentqt.NavigationView()
        nav.setExpandedPaneWidth(180)
        header = fluentqt.Label("Gallery", nav)
        main = QWidget(nav)
        main_layout = QVBoxLayout(main)
        buttons = []
        for index, title in enumerate(("Home", "Controls", "Settings")):
            button = fluentqt.Button(title, main)
            main_layout.addWidget(button)
            buttons.append(button)
        footer = fluentqt.Label("FluentQt", nav)
        nav.setOwnedHeaderChromeWidget(header)
        nav.setOwnedMainChromeWidget(main)
        nav.setOwnedFooterChromeWidget(footer)
        host = nav.contentHost()
        for title in ("Home", "Controls", "Settings"):
            host.addOwnedPage(make_page(title))
        for index, button in enumerate(buttons):
            button.clicked.connect(
                lambda _checked=False, value=index: host.setCurrentIndex(value, 0, True)
            )
        return nav
    """
)

_NAV_EXACT_IMPORTS = (
    "from PySide6.QtCore import QRect, QRectF, QSize, Qt\n"
    "from PySide6.QtGui import QFont, QFontMetrics, QPainter, QPen\n"
    "from PySide6.QtWidgets import (QBoxLayout, QHBoxLayout, QSizePolicy, "
    "QVBoxLayout, QWidget)\n"
    "from fluentqt_gallery.foundation_pages import _theme_tokens"
)

_NAV_EXACT_HELPER = dedent(
    """
    class NavigationDemoRow(QWidget):
        def __init__(self, icon_glyph, text, parent=None):
            super().__init__(parent)
            self.icon_glyph = icon_glyph
            self.text = text
            self.selected = False
            self.compact = False
            self.hovered = False
            self.pressed = False
            self.on_activated = None
            self.setMouseTracking(True)
            self.setSizePolicy(
                QSizePolicy.Policy.Expanding,
                QSizePolicy.Policy.Fixed,
            )
            self.setFixedHeight(40)

        def sizeHint(self):
            if self.compact:
                return QSize(48, 40)
            metrics = QFontMetrics(fluentqt.font_for_role(fluentqt.FontRole.Body))
            return QSize(max(88, metrics.horizontalAdvance(self.text) + 68), 40)

        def set_selected(self, selected):
            if self.selected != selected:
                self.selected = selected
                self.update()

        def set_compact(self, compact):
            if self.compact != compact:
                self.compact = compact
                self.updateGeometry()
                self.update()

        def paintEvent(self, event):
            del event
            colors = _theme_tokens(self)
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
            item_rect = QRectF(self.rect().adjusted(4, 2, -4, -2))
            fill = None
            if self.pressed:
                fill = colors["subtleTertiary"]
            elif self.selected or self.hovered:
                fill = colors["subtleSecondary"]
            if fill is not None and fill.alpha() > 0:
                painter.setPen(Qt.PenStyle.NoPen)
                painter.setBrush(fill)
                painter.drawRoundedRect(item_rect, 4.0, 4.0)
            if self.selected and not self.compact:
                indicator = QRectF(
                    item_rect.left() + 4,
                    item_rect.center().y() - 7,
                    3,
                    14,
                )
                painter.setBrush(colors["accentDefault"])
                painter.drawRoundedRect(indicator, 1.5, 1.5)

            icon_x = (self.width() - 20) // 2 if self.compact else 22
            icon_font = QFont("FluentQt Icons")
            icon_font.setPixelSize(16)
            painter.setFont(icon_font)
            painter.setPen(
                colors["textPrimary"]
                if self.selected
                else colors["textSecondary"]
            )
            painter.drawText(
                QRect(icon_x, 0, 20, self.height()),
                Qt.AlignmentFlag.AlignCenter,
                self.icon_glyph,
            )
            if not self.compact:
                painter.setFont(
                    fluentqt.font_for_role(
                        fluentqt.FontRole.BodyStrong
                        if self.selected
                        else fluentqt.FontRole.Body
                    )
                )
                painter.setPen(colors["textPrimary"])
                painter.drawText(
                    QRect(52, 0, max(0, self.width() - 64), self.height()),
                    Qt.AlignmentFlag.AlignVCenter
                    | Qt.AlignmentFlag.AlignLeft
                    | Qt.TextFlag.TextSingleLine,
                    self.text,
                )

        def enterEvent(self, event):
            super().enterEvent(event)
            self.hovered = True
            self.update()

        def leaveEvent(self, event):
            super().leaveEvent(event)
            self.hovered = False
            self.pressed = False
            self.update()

        def mousePressEvent(self, event):
            if event.button() == Qt.MouseButton.LeftButton:
                self.pressed = True
                self.update()
                event.accept()
                return
            super().mousePressEvent(event)

        def mouseReleaseEvent(self, event):
            activate = self.pressed and self.rect().contains(
                event.position().toPoint()
            )
            self.pressed = False
            self.update()
            if activate and self.on_activated is not None:
                self.on_activated()
                event.accept()
                return
            super().mouseReleaseEvent(event)


    class NavigationDemoSection(QWidget):
        def __init__(self, entries, parent=None):
            super().__init__(parent)
            self.entries = entries
            self.rows = []
            self.selected_index = -1
            self.preferred_vertical_height = 0
            self.orientation = Qt.Orientation.Vertical
            self.compact = False
            self.on_activated = None
            self.setSizePolicy(
                QSizePolicy.Policy.Expanding,
                QSizePolicy.Policy.Preferred,
            )
            self.content_layout = QBoxLayout(
                QBoxLayout.Direction.TopToBottom,
                self,
            )
            self.content_layout.setContentsMargins(8, 8, 8, 8)
            self.content_layout.setSpacing(4)
            for index, (icon, text, page_index) in enumerate(entries):
                row = NavigationDemoRow(icon, text, self)
                row.on_activated = (
                    lambda value=index, page=page_index: self.activate(value, page)
                )
                self.rows.append(row)
                self.content_layout.addWidget(row)
            self.content_layout.addStretch(1)

        def sizeHint(self):
            if self.orientation == Qt.Orientation.Horizontal:
                width = 16
                for row in self.rows:
                    width += row.sizeHint().width() + self.content_layout.spacing()
                return QSize(width, 48)
            height = self.preferred_vertical_height
            if height <= 0:
                height = 16 + len(self.rows) * 44
            return QSize(220, height)

        def minimumSizeHint(self):
            if self.orientation == Qt.Orientation.Horizontal:
                return QSize(0, 44)
            return QSize(48, 16 + len(self.rows) * 40)

        def set_orientation(self, orientation):
            self.orientation = orientation
            self.content_layout.setDirection(
                QBoxLayout.Direction.TopToBottom
                if orientation == Qt.Orientation.Vertical
                else QBoxLayout.Direction.LeftToRight
            )
            vertical = orientation == Qt.Orientation.Vertical
            self.content_layout.setContentsMargins(
                8, 8 if vertical else 4, 8, 8 if vertical else 4
            )
            for row in self.rows:
                row.setFixedHeight(40 if vertical else 36)
                row.set_compact(self.compact)
            self.updateGeometry()

        def set_compact(self, compact):
            self.compact = compact
            for row in self.rows:
                row.set_compact(compact)
            self.updateGeometry()

        def set_preferred_vertical_height(self, height):
            self.preferred_vertical_height = height
            self.updateGeometry()

        def set_selected_index(self, index):
            if self.selected_index == index:
                return
            self.selected_index = index
            for row_index, row in enumerate(self.rows):
                row.set_selected(row_index == index)

        def clear_selection(self):
            self.set_selected_index(-1)

        def activate(self, row_index, page_index):
            self.set_selected_index(row_index)
            if self.on_activated is not None:
                self.on_activated(page_index)


    class NavigationSampleSurface(QWidget):
        def __init__(self, parent=None, spacing=8):
            super().__init__(parent)
            self.setSizePolicy(
                QSizePolicy.Policy.Preferred,
                QSizePolicy.Policy.Preferred,
            )
            self.content_layout = QVBoxLayout(self)
            self.content_layout.setContentsMargins(16, 16, 16, 16)
            self.content_layout.setSpacing(spacing)

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


    class NavigationDashboardPage(QWidget):
        def __init__(self, spec, parent=None):
            super().__init__(parent)
            self.spec = spec
            self.setAutoFillBackground(False)

        def paintEvent(self, event):
            del event
            colors = _theme_tokens(self)
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
            side_pad = 22
            left_pad = side_pad
            content_width = max(0, self.width() - left_pad - side_pad)
            painter.fillRect(self.rect(), colors["bgLayer"])

            painter.setPen(colors["textPrimary"])
            painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.Subtitle))
            painter.drawText(
                QRect(left_pad, 12, content_width, 28),
                Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter,
                self.spec[0],
            )
            painter.setPen(colors["textSecondary"])
            painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.Caption))
            painter.drawText(
                QRect(left_pad, 42, content_width, 30),
                Qt.AlignmentFlag.AlignLeft
                | Qt.AlignmentFlag.AlignTop
                | Qt.TextFlag.TextWordWrap,
                self.spec[1],
            )

            gap = 12
            card_top = 82
            card_height = 56
            card_width = max(96, (content_width - gap) // 2)
            self.draw_card(
                painter,
                QRect(left_pad, card_top, card_width, card_height),
                self.spec[2],
                self.spec[3],
                colors["accentDefault"],
            )
            self.draw_card(
                painter,
                QRect(
                    left_pad + card_width + gap,
                    card_top,
                    card_width,
                    card_height,
                ),
                self.spec[4],
                self.spec[5],
                colors["systemInfo"],
            )

            panel_top = card_top + card_height + 14
            panel_height = max(72, self.height() - panel_top - 14)
            panel = QRect(left_pad, panel_top, content_width, panel_height)
            painter.setPen(colors["strokeCard"])
            painter.setBrush(colors["bgCanvas"])
            painter.drawRoundedRect(panel, 8.0, 8.0)
            painter.setPen(colors["textPrimary"])
            painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.BodyStrong))
            painter.drawText(
                panel.adjusted(16, 10, -16, -panel.height() + 34),
                Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter,
                "Recent activity",
            )
            painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.Caption))
            y = panel.top() + 38
            for row_text in self.spec[6]:
                row_rect = QRect(panel.left() + 14, y, panel.width() - 28, 36)
                painter.setPen(colors["strokeDivider"])
                painter.drawLine(row_rect.topLeft(), row_rect.topRight())
                painter.setPen(colors["textSecondary"])
                painter.drawText(
                    row_rect.adjusted(2, 0, -2, 0),
                    Qt.AlignmentFlag.AlignVCenter
                    | Qt.AlignmentFlag.AlignLeft
                    | Qt.TextFlag.TextWordWrap,
                    row_text,
                )
                y += 40
                if y > panel.bottom() - 16:
                    break

        def draw_card(self, painter, rect, title, value, accent):
            colors = _theme_tokens(self)
            painter.setPen(colors["strokeCard"])
            painter.setBrush(colors["bgCanvas"])
            painter.drawRoundedRect(rect, 4.0, 4.0)
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(accent)
            painter.drawRoundedRect(
                QRect(rect.left() + 12, rect.top() + 12, 3, 30), 1.5, 1.5
            )
            painter.setPen(colors["textSecondary"])
            painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.Caption))
            painter.drawText(
                rect.adjusted(26, 8, -12, -rect.height() + 30),
                Qt.AlignmentFlag.AlignLeft
                | Qt.AlignmentFlag.AlignVCenter
                | Qt.TextFlag.TextSingleLine,
                title,
            )
            painter.setPen(colors["textPrimary"])
            painter.setFont(fluentqt.font_for_role(fluentqt.FontRole.BodyStrong))
            painter.drawText(
                rect.adjusted(26, 28, -12, -8),
                Qt.AlignmentFlag.AlignLeft
                | Qt.AlignmentFlag.AlignVCenter
                | Qt.TextFlag.TextSingleLine,
                value,
            )


    NAVIGATION_PAGE_SPECS = (
        (
            "Home",
            "Navigation rows route to pages owned by StackContentHost.",
            "Page", "0", "Scope", "Home",
            (
                "Home row activated page index 0",
                "NavigationView keeps shell geometry separate",
                "Selection is maintained by the app chrome",
            ),
        ),
        (
            "Search",
            "Header chrome can navigate to a hosted search page.",
            "Page", "1", "Results", "128",
            (
                "Search row activated page index 1",
                "Header and main rows share the same host",
                "The active page stays inside contentHost()",
            ),
        ),
        (
            "Documents",
            "Document workspace content is supplied by the application layer.",
            "Page", "2", "Pinned", "8 files",
            (
                "Documents row activated page index 2",
                "The content transition follows the shell mode",
                "Pane width does not change page ownership",
            ),
        ),
        (
            "Downloads",
            "Downloads keeps working across expanded, compact, and minimal pane modes.",
            "Page", "3", "Queue", "6 items",
            (
                "Downloads row activated page index 3",
                "Compact rows keep icons centered",
                "Minimal overlay leaves the host page readable",
            ),
        ),
        (
            "Help",
            "Footer chrome participates in the same page routing as the main section.",
            "Page", "4", "Articles", "36",
            (
                "Help row activated page index 4",
                "Footer chrome is laid out separately",
                "StackContentHost owns the page widget",
            ),
        ),
        (
            "Settings",
            "Settings is an ordinary hosted page selected from app-owned chrome.",
            "Page", "5", "Updates", "Current",
            (
                "Settings row activated page index 5",
                "Routing is shared across NavigationView samples",
                "NavigationView does not own a menu model",
            ),
        ),
    )


    def populate_navigation_pages(host):
        for index, spec in enumerate(NAVIGATION_PAGE_SPECS):
            host.insertPage(index, NavigationDashboardPage(spec, host))
        host.setCurrentIndex(0, 0, False)


    def make_caption_status(parent, text):
        label = fluentqt.Label(text, parent)
        label.setFluentTypography(fluentqt.FontRole.Caption)
        label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        label.setWordWrap(True)
        return label


    def configure_navigation_preview(nav, height, object_name):
        nav.setObjectName(object_name)
        nav.setMinimumWidth(440)
        nav.setMaximumWidth(620)
        nav.setFixedHeight(height)
        nav.setSizePolicy(
            QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed
        )
    """
)


register_source_samples(
    "navigation-view",
    ("NavigationView", "StackContentHost"),
    {
        "navigation-view-chrome-slots": (
            "root",
            _script(
                _NAV_EXACT_HELPER
                + dedent(
                    """
                    root = QWidget(globals().get("gallery_parent"))
                    root_layout = QVBoxLayout(root)
                    root_layout.setContentsMargins(0, 0, 0, 0)
                    root_layout.setSpacing(8)
                    root_layout.setAlignment(
                        Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                    )
                    surface = NavigationSampleSurface(root, 8)

                    nav = fluentqt.NavigationView(surface)
                    configure_navigation_preview(
                        nav, 340, "navigationViewChromeSlotsPreview"
                    )
                    nav.setDisplayMode(fluentqt.NavigationView.DisplayMode.Left)
                    nav.setExpandedPaneWidth(180)

                    header_section = NavigationDemoSection(
                        (("\ue72b", "Back", 0), ("\ue721", "Search", 1)),
                        nav,
                    )
                    header_section.set_preferred_vertical_height(88)
                    main_section = NavigationDemoSection(
                        (
                            ("\ue80f", "Home", 0),
                            ("\ue8a5", "Documents", 2),
                            ("\ue896", "Downloads", 3),
                        ),
                        nav,
                    )
                    main_section.set_selected_index(0)
                    footer_section = NavigationDemoSection(
                        (("\ue946", "Help", 4), ("\ue713", "Settings", 5)),
                        nav,
                    )
                    footer_section.set_preferred_vertical_height(88)

                    nav.setHeaderChromeWidget(header_section)
                    nav.setMainChromeWidget(main_section)
                    nav.setFooterChromeWidget(footer_section)
                    populate_navigation_pages(nav.contentHost())

                    status = make_caption_status(surface, "Current page: Home")

                    def route_to_page(source, page_index):
                        host = nav.contentHost()
                        if page_index < 0 or page_index >= host.count():
                            return
                        for section in (
                            header_section,
                            main_section,
                            footer_section,
                        ):
                            if section is not source:
                                section.clear_selection()
                        direction = 1 if page_index >= host.currentIndex() else -1
                        host.setCurrentIndex(page_index, direction, True)
                        status.setText(
                            f"Current page: {NAVIGATION_PAGE_SPECS[page_index][0]}"
                        )

                    header_section.on_activated = (
                        lambda index: route_to_page(header_section, index)
                    )
                    main_section.on_activated = (
                        lambda index: route_to_page(main_section, index)
                    )
                    footer_section.on_activated = (
                        lambda index: route_to_page(footer_section, index)
                    )

                    surface.content_layout.addWidget(nav)
                    surface.content_layout.addWidget(status)
                    root_layout.addWidget(surface)
                    """
                ),
                _NAV_EXACT_IMPORTS,
            ),
        ),
        "navigation-view-display-modes": (
            "root",
            _script(
                _NAV_EXACT_HELPER
                + dedent(
                    """
                    root = QWidget(globals().get("gallery_parent"))
                    root_layout = QVBoxLayout(root)
                    root_layout.setContentsMargins(0, 0, 0, 0)
                    root_layout.setSpacing(8)
                    root_layout.setAlignment(
                        Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                    )
                    surface = NavigationSampleSurface(root, 8)

                    controls = QWidget(surface)
                    controls_layout = QHBoxLayout(controls)
                    controls_layout.setContentsMargins(0, 0, 0, 0)
                    controls_layout.setSpacing(6)
                    controls_layout.setAlignment(
                        Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                    )
                    button_specs = (
                        ("Left", "\uea37", fluentqt.NavigationView.DisplayMode.Left),
                        ("Compact", "\ue700", fluentqt.NavigationView.DisplayMode.LeftCompact),
                        ("Minimal", "\ue700", fluentqt.NavigationView.DisplayMode.LeftMinimal),
                        ("Top", "\ue71d", fluentqt.NavigationView.DisplayMode.Top),
                    )
                    mode_buttons = []
                    for text, glyph, mode in button_specs:
                        button = fluentqt.Button(text, controls)
                        button.setFluentSize(fluentqt.Button.ButtonSize.Small)
                        button.setFluentLayout(
                            fluentqt.Button.ButtonLayout.IconBefore
                        )
                        button.setIconGlyph(glyph, 16)
                        controls_layout.addWidget(button)
                        mode_buttons.append((button, mode))

                    nav = fluentqt.NavigationView(surface)
                    configure_navigation_preview(
                        nav, 340, "navigationViewDisplayModesPreview"
                    )
                    nav.setAnimationEnabled(True)
                    nav.setExpandedPaneWidth(180)
                    nav.setCompactPaneWidth(52)
                    nav.setTopBarHeight(48)

                    header_section = NavigationDemoSection(
                        (("\ue72b", "Back", 0), ("\ue721", "Search", 1)),
                        nav,
                    )
                    header_section.set_preferred_vertical_height(88)
                    main_section = NavigationDemoSection(
                        (
                            ("\ue80f", "Home", 0),
                            ("\ue8a5", "Documents", 2),
                            ("\ue896", "Downloads", 3),
                        ),
                        nav,
                    )
                    main_section.set_selected_index(0)
                    footer_section = NavigationDemoSection(
                        (("\ue946", "Help", 4), ("\ue713", "Settings", 5)),
                        nav,
                    )
                    footer_section.set_preferred_vertical_height(88)
                    nav.setHeaderChromeWidget(header_section)
                    nav.setMainChromeWidget(main_section)
                    nav.setFooterChromeWidget(footer_section)
                    populate_navigation_pages(nav.contentHost())

                    mode_status = make_caption_status(
                        surface, "Display mode: Left"
                    )
                    route_status = make_caption_status(
                        surface, "Current page: Home"
                    )

                    def route_to_page(source, page_index):
                        host = nav.contentHost()
                        if page_index < 0 or page_index >= host.count():
                            return
                        for section in (
                            header_section,
                            main_section,
                            footer_section,
                        ):
                            if section is not source:
                                section.clear_selection()
                        direction = 1 if page_index >= host.currentIndex() else -1
                        host.setCurrentIndex(page_index, direction, True)
                        route_status.setText(
                            f"Current page: {NAVIGATION_PAGE_SPECS[page_index][0]}"
                        )

                    header_section.on_activated = (
                        lambda index: route_to_page(header_section, index)
                    )
                    main_section.on_activated = (
                        lambda index: route_to_page(main_section, index)
                    )
                    footer_section.on_activated = (
                        lambda index: route_to_page(footer_section, index)
                    )

                    def apply_mode(mode):
                        top = mode == fluentqt.NavigationView.DisplayMode.Top
                        compact_chrome = (
                            mode != fluentqt.NavigationView.DisplayMode.Left
                        )
                        orientation = (
                            Qt.Orientation.Horizontal
                            if top
                            else Qt.Orientation.Vertical
                        )
                        for section in (
                            header_section,
                            main_section,
                            footer_section,
                        ):
                            section.set_orientation(orientation)
                            section.set_compact(compact_chrome)
                        nav.contentHost().setTransitionEffect(
                            fluentqt.StackContentHost.TransitionEffect.SlideFromBottom
                            if top else fluentqt.StackContentHost.TransitionEffect.SlideFromLeft
                        )
                        nav.setPaneOpen(mode == fluentqt.NavigationView.DisplayMode.Left or top)
                        nav.setDisplayMode(mode)
                        for button, button_mode in mode_buttons:
                            button.setFluentStyle(
                                fluentqt.Button.ButtonStyle.Accent
                                if button_mode == mode
                                else fluentqt.Button.ButtonStyle.Standard
                            )
                        mode_names = {
                            fluentqt.NavigationView.DisplayMode.Left: "Left",
                            fluentqt.NavigationView.DisplayMode.LeftCompact: "LeftCompact",
                            fluentqt.NavigationView.DisplayMode.LeftMinimal: "LeftMinimal",
                            fluentqt.NavigationView.DisplayMode.Top: "Top",
                        }
                        mode_status.setText(
                            f"Display mode: {mode_names.get(mode, 'Auto')}"
                        )

                    for button, mode in mode_buttons:
                        button.clicked.connect(
                            lambda _checked=False, value=mode: apply_mode(value)
                        )
                    apply_mode(fluentqt.NavigationView.DisplayMode.Left)

                    surface.content_layout.addWidget(controls)
                    surface.content_layout.addWidget(nav)
                    surface.content_layout.addWidget(mode_status)
                    surface.content_layout.addWidget(route_status)
                    root_layout.addWidget(surface)
                    """
                ),
                _NAV_EXACT_IMPORTS,
            ),
        ),
        "navigation-view-content-host": (
            "root",
            _script(
                _NAV_EXACT_HELPER
                + dedent(
                    """
                    root = QWidget(globals().get("gallery_parent"))
                    root_layout = QVBoxLayout(root)
                    root_layout.setContentsMargins(0, 0, 0, 0)
                    root_layout.setSpacing(8)
                    root_layout.setAlignment(
                        Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                    )
                    surface = NavigationSampleSurface(root, 8)
                    nav = fluentqt.NavigationView(surface)
                    configure_navigation_preview(
                        nav, 320, "navigationViewContentHostPreview"
                    )
                    nav.setAnimationEnabled(True)
                    nav.setDisplayMode(fluentqt.NavigationView.DisplayMode.Left)
                    nav.setExpandedPaneWidth(180)

                    main_section = NavigationDemoSection(
                        (
                            ("\ue80f", "Home", 0),
                            ("\ue8a5", "Documents", 2),
                            ("\ue713", "Settings", 5),
                        ),
                        nav,
                    )
                    main_section.set_selected_index(0)
                    nav.setMainChromeWidget(main_section)
                    host = nav.contentHost()
                    populate_navigation_pages(host)
                    host.setTransitionEffect(
                        fluentqt.StackContentHost.TransitionEffect.SlideFromLeft
                    )
                    status = make_caption_status(surface, "Current page: Home")

                    def route_to_page(page_index):
                        if page_index < 0 or page_index >= host.count():
                            return
                        direction = 1 if page_index >= host.currentIndex() else -1
                        host.setCurrentIndex(page_index, direction, True)
                        status.setText(
                            f"Current page: {NAVIGATION_PAGE_SPECS[page_index][0]}"
                        )

                    main_section.on_activated = route_to_page
                    surface.content_layout.addWidget(nav)
                    surface.content_layout.addWidget(status)
                    root_layout.addWidget(surface)
                    """
                ),
                _NAV_EXACT_IMPORTS,
            ),
        ),
    },
)


register_source_samples(
    "pivot",
    ("Pivot", "PivotItem"),
    {
        "pivot-basic": (
            "root",
            _script(
                _page_source_helper()
                + dedent(
                    """
                    root = QWidget()
                    layout = QVBoxLayout(root)
                    pivot = fluentqt.Pivot(root)
                    pivot.setFixedSize(540, 44)
                    for item in (
                        fluentqt.PivotItem("All", "\ue715"),
                        fluentqt.PivotItem("Unread", "\ue71c"),
                        fluentqt.PivotItem("Flagged", "\ue7c1"),
                        fluentqt.PivotItem("Locked", "\ue72e", False),
                        fluentqt.PivotItem("Mentions", "\ue77b"),
                    ):
                        pivot.addItem(item)
                    pivot.setSelectedIndex(0)
                    host = fluentqt.StackContentHost(root)
                    host.setFixedSize(540, 128)
                    for index in range(pivot.itemCount()):
                        item = pivot.itemAt(index)
                        host.addOwnedPage(
                            make_page(
                                item.header,
                                "This disabled pivot item remains visible but cannot be selected."
                                if index == 3
                                else "CurrentChanged drives this StackContentHost page.",
                            )
                        )
                    host.setCurrentIndex(0, 0, False)
                    status = make_status_label(root, "Showing: All")

                    def select_page(index):
                        if 0 <= index < host.count():
                            direction = 1 if index >= host.currentIndex() else -1
                            host.setCurrentIndex(index, direction, True)
                        status.setText(
                            "Showing: {0}".format(pivot.itemAt(index).header)
                        )

                    pivot.currentChanged.connect(select_page)
                    layout.addWidget(pivot)
                    layout.addWidget(host)
                    layout.addWidget(status)
                    """
                ),
                _WIDGETS,
            ),
        ),
        "pivot-item-state": (
            "root",
            _script(
                _page_source_helper()
                + dedent(
                    """
                    root = QWidget(globals().get("gallery_parent"))
                    layout = QVBoxLayout(root)
                    pivot = fluentqt.Pivot(root)
                    pivot.setFixedSize(540, 44)
                    pivot.addItem(
                        fluentqt.PivotItem(
                            "Inbox", "\ue715", True, "inbox", "Inbox view"
                        )
                    )
                    pivot.addItem(
                        fluentqt.PivotItem(
                            "Flagged", "\ue7c1", True, "flagged", "Flagged mail"
                        )
                    )
                    pivot.addItem(
                        fluentqt.PivotItem(
                            "Locked", "\ue72e", False, "locked", "Locked view"
                        )
                    )
                    pivot.setSelectedIndex(0)

                    controls = QWidget(root)
                    controls_layout = QHBoxLayout(controls)
                    controls_layout.setContentsMargins(0, 0, 0, 0)
                    controls_layout.setSpacing(8)
                    controls_layout.setAlignment(
                        Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                    )
                    rename = fluentqt.Button("Rename Flagged", controls)
                    unlock = fluentqt.Button("Unlock Locked", controls)
                    for button in (rename, unlock):
                        button.setFluentSize(fluentqt.Button.ButtonSize.Small)
                        controls_layout.addWidget(button)

                    status = make_status_label(
                        root, "Selected Inbox, data: inbox"
                    )
                    status.setFluentTypography(fluentqt.FontRole.Caption)

                    def update_status(prefix=""):
                        index = pivot.selectedIndex()
                        if index < 0:
                            return
                        item = pivot.itemAt(index)
                        state = f"Selected {item.header}, data: {item.data}"
                        status.setText(f"{prefix}; {state}" if prefix else state)

                    def rename_flagged():
                        pivot.setItemHeader(1, "Priority")
                        pivot.setItemIconGlyph(1, "\uedb1")
                        pivot.setItemData(1, "priority")
                        rename.setEnabled(False)
                        update_status("Updated item 2")

                    def unlock_locked():
                        pivot.setItemEnabled(2, True)
                        unlock.setEnabled(False)
                        update_status("Locked is now enabled")

                    rename.clicked.connect(rename_flagged)
                    unlock.clicked.connect(unlock_locked)
                    pivot.currentChanged.connect(lambda _index: update_status())
                    layout.addWidget(pivot)
                    layout.addWidget(controls)
                    layout.addWidget(status)
                    """
                ),
                _WIDGETS,
            ),
        ),
        "pivot-overflow-behavior": (
            "root",
            _script(
                _page_source_helper()
                + dedent(
                    """
                    root = QWidget(globals().get("gallery_parent"))
                    layout = QVBoxLayout(root)
                    items = (
                        ("All", "\ue715"),
                        ("Unread", "\ue71c"),
                        ("Flagged", "\ue7c1"),
                        ("Mentions", "\ue77b"),
                        ("Archive", "\ue8b7"),
                        ("Long category", "\ue838"),
                        ("Settings", "\ue713"),
                        ("History", "\ue81c"),
                    )
                    status = make_status_label(
                        root,
                        "MoreButton groups hidden headers behind the ... button.",
                    )
                    status.setFluentTypography(fluentqt.FontRole.Caption)

                    def add_overflow_row(label_text, behavior):
                        row = QWidget(root)
                        row_layout = QHBoxLayout(row)
                        row_layout.setContentsMargins(0, 0, 0, 0)
                        row_layout.setSpacing(10)
                        row_layout.setAlignment(
                            Qt.AlignmentFlag.AlignLeft
                            | Qt.AlignmentFlag.AlignVCenter
                        )
                        label = fluentqt.Label(label_text, row)
                        label.setFluentTypography(fluentqt.FontRole.Caption)
                        label.setTextColorRole(
                            fluentqt.Label.TextColorRole.Primary
                        )
                        label.setFixedWidth(96)
                        pivot = fluentqt.Pivot(row)
                        pivot.setFixedSize(420, 44)
                        pivot.setOverflowBehavior(behavior)
                        for header, glyph in items:
                            pivot.addItem(fluentqt.PivotItem(header, glyph))
                        pivot.setSelectedIndex(0)
                        if behavior == fluentqt.Pivot.OverflowBehavior.MoreButton:
                            pivot.overflowActivated.connect(
                                lambda indexes: status.setText(
                                    "MoreButton contains: "
                                    + ", ".join(
                                        pivot.itemAt(index).header
                                        for index in indexes
                                    )
                                )
                            )
                        row_layout.addWidget(label)
                        row_layout.addWidget(pivot)
                        layout.addWidget(row)

                    add_overflow_row(
                        "ScrollButtons",
                        fluentqt.Pivot.OverflowBehavior.ScrollButtons,
                    )
                    add_overflow_row(
                        "MoreButton",
                        fluentqt.Pivot.OverflowBehavior.MoreButton,
                    )
                    layout.addWidget(status)
                    """
                ),
                _WIDGETS,
            ),
        ),
    },
)


register_source_samples(
    "selector-bar",
    ("SelectorBar", "SelectorBarItem"),
    {
        "selector-bar-basic": (
            "root",
            _script(
                _page_source_helper()
                + dedent(
                    """
                    root = QWidget()
                    layout = QVBoxLayout(root)
                    selector = fluentqt.SelectorBar(root)
                    selector.setFixedSize(540, 44)
                    values = (
                        ("Inbox", "\ue715", "inbox"),
                        ("Calendar", "\ue787", "calendar"),
                        ("Settings", "\ue713", "settings"),
                    )
                    for text, icon, key in values:
                        selector.addItem(
                            fluentqt.SelectorBarItem(text, icon, True, True, key)
                        )
                    selector.setSelectedIndex(0)
                    host = fluentqt.StackContentHost(root)
                    host.setFixedSize(540, 128)
                    for text, _icon, key in values:
                        host.addOwnedPage(
                            make_page(
                                text,
                                'selectionChanged carries data key "{0}".'.format(key),
                            )
                        )
                    host.setCurrentIndex(0, 0, False)
                    status = make_status_label(root, "View: Inbox, data: inbox")

                    def select_page(index):
                        if 0 <= index < host.count():
                            direction = 1 if index >= host.currentIndex() else -1
                            host.setCurrentIndex(index, direction, True)

                    def update_status(index, item):
                        del index
                        status.setText(
                            "View: {0}, data: {1}".format(item.text, item.data)
                        )

                    selector.currentChanged.connect(select_page)
                    selector.selectionChanged.connect(update_status)
                    layout.addWidget(selector)
                    layout.addWidget(host)
                    layout.addWidget(status)
                    """
                ),
                _WIDGETS,
            ),
        ),
        "selector-bar-item-state": (
            "root",
            _script(
                _page_source_helper()
                + dedent(
                    """
                    root = QWidget(globals().get("gallery_parent"))
                    layout = QVBoxLayout(root)
                    selector = fluentqt.SelectorBar(root)
                    selector.setFixedSize(540, 44)
                    selector.addItem(
                        fluentqt.SelectorBarItem(
                            "Overview", "\ue80f", True, True, "overview"
                        )
                    )
                    selector.addItem(
                        fluentqt.SelectorBarItem(
                            "Sample code", "\ue8a5", True, False, "code"
                        )
                    )
                    selector.addItem(
                        fluentqt.SelectorBarItem(
                            "Disabled", "\ue72e", False, True, "disabled"
                        )
                    )
                    selector.addItem(
                        fluentqt.SelectorBarItem(
                            "Settings", "\ue713", True, True, "settings"
                        )
                    )
                    selector.setItemSelected(0, True)

                    controls = QWidget(root)
                    controls_layout = QHBoxLayout(controls)
                    controls_layout.setContentsMargins(0, 0, 0, 0)
                    controls_layout.setSpacing(8)
                    controls_layout.setAlignment(
                        Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                    )
                    show_code = fluentqt.Button("Show code", controls)
                    show_code.setFluentSize(fluentqt.Button.ButtonSize.Small)
                    controls_layout.addWidget(show_code)
                    status = make_status_label(
                        root,
                        "Sample code is hidden; Disabled is visible but not selectable.",
                    )
                    status.setFluentTypography(fluentqt.FontRole.Caption)
                    code_visible = [False]

                    def toggle_code():
                        code_visible[0] = not code_visible[0]
                        selector.setItemVisible(1, code_visible[0])
                        show_code.setText(
                            "Hide code" if code_visible[0] else "Show code"
                        )
                        status.setText(
                            "Sample code is visible and selectable."
                            if code_visible[0]
                            else "Sample code is hidden; Disabled is visible but not selectable."
                        )

                    show_code.clicked.connect(toggle_code)
                    selector.selectionChanged.connect(
                        lambda _index, item: status.setText(
                            f"Selected {item.text}, data: {item.data}"
                        )
                    )
                    layout.addWidget(selector)
                    layout.addWidget(controls)
                    layout.addWidget(status)
                    """
                ),
                _WIDGETS,
            ),
        ),
        "selector-bar-overflow-behavior": (
            "root",
            _script(
                _page_source_helper()
                + dedent(
                    """
                    root = QWidget(globals().get("gallery_parent"))
                    layout = QVBoxLayout(root)
                    items = tuple(
                        (
                            f"Category {index + 1}",
                            "\ue838" if index % 2 == 0 else "\ue8a5",
                        )
                        for index in range(6)
                    )
                    status = make_status_label(
                        root,
                        "Click the MoreButton row's ... to list hidden items.",
                    )
                    status.setFluentTypography(fluentqt.FontRole.Caption)

                    def add_overflow_row(label_text, behavior):
                        row = QWidget(root)
                        row_layout = QHBoxLayout(row)
                        row_layout.setContentsMargins(0, 0, 0, 0)
                        row_layout.setSpacing(10)
                        row_layout.setAlignment(
                            Qt.AlignmentFlag.AlignLeft
                            | Qt.AlignmentFlag.AlignVCenter
                        )
                        label = fluentqt.Label(label_text, row)
                        label.setFluentTypography(fluentqt.FontRole.Caption)
                        label.setTextColorRole(
                            fluentqt.Label.TextColorRole.Primary
                        )
                        label.setFixedWidth(96)
                        selector = fluentqt.SelectorBar(row)
                        selector.setFixedSize(360, 44)
                        selector.setOverflowBehavior(behavior)
                        for text, glyph in items:
                            selector.addItem(
                                fluentqt.SelectorBarItem(text, glyph)
                            )
                        selector.setSelectedIndex(0)
                        if behavior == fluentqt.SelectorBar.OverflowBehavior.MoreButton:
                            selector.overflowActivated.connect(
                                lambda indexes: status.setText(
                                    "MoreButton contains: "
                                    + ", ".join(
                                        selector.itemAt(index).text
                                        for index in indexes
                                    )
                                )
                            )
                        row_layout.addWidget(label)
                        row_layout.addWidget(selector)
                        layout.addWidget(row)

                    add_overflow_row(
                        "ScrollButtons",
                        fluentqt.SelectorBar.OverflowBehavior.ScrollButtons,
                    )
                    add_overflow_row(
                        "MoreButton",
                        fluentqt.SelectorBar.OverflowBehavior.MoreButton,
                    )
                    layout.addWidget(status)
                    """
                ),
                _WIDGETS,
            ),
        ),
    },
)


def _tab_items_source() -> str:
    return dedent(
        """
        def add_standard_tabs(tabs):
            tabs.addTab(fluentqt.TabViewItem("Home", "\ue80f"))
            tabs.addTab(fluentqt.TabViewItem("Long document", "\ue8a5"))
            tabs.addTab(fluentqt.TabViewItem("Activity", "\ue787"))
            tabs.setTabsClosable(False)
            tabs.setAddTabButtonVisible(False)
        """
    )


register_source_samples(
    "tab-view",
    ("TabView", "TabViewItem"),
    {
        "tab-view-hosted-pages": (
            "root",
            _script(
                _page_source_helper()
                + dedent(
                    """
                    root = QWidget()
                    layout = QVBoxLayout(root)

                    class NavigationSampleSurface(QWidget):
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

                    surface = NavigationSampleSurface(root)
                    surface.setObjectName("tabViewHostedPagesSurface")
                    surface.setMinimumWidth(360)
                    surface.setMaximumWidth(560)
                    surface.setFixedHeight(186)
                    surface.setSizePolicy(
                        QSizePolicy.Policy.Expanding,
                        QSizePolicy.Policy.Fixed,
                    )
                    surface_layout = QVBoxLayout(surface)
                    surface_layout.setContentsMargins(0, 0, 0, 0)
                    surface_layout.setSpacing(0)

                    tabs = fluentqt.TabView(surface)
                    tabs.setObjectName("tabViewHostedPagesTabs")
                    tabs.setFixedHeight(40)
                    tabs.setSizePolicy(
                        QSizePolicy.Policy.Expanding,
                        QSizePolicy.Policy.Fixed,
                    )
                    tabs.setTabWidthMode(fluentqt.TabView.TabWidthMode.SizeToContent)
                    tabs.setTabReorderEnabled(True)
                    tabs.setTabsClosable(False)
                    tabs.setAddTabButtonVisible(False)
                    initial_tabs = (
                        fluentqt.TabViewItem("Home", "\ue80f"),
                        fluentqt.TabViewItem("Details", "\ue8a5"),
                        fluentqt.TabViewItem("Activity", "\ue787"),
                    )
                    host = fluentqt.StackContentHost(surface)
                    host.setObjectName("tabViewHostedPagesHost")
                    host.setFixedHeight(146)
                    host.setSizePolicy(
                        QSizePolicy.Policy.Expanding,
                        QSizePolicy.Policy.Fixed,
                    )
                    for tab in initial_tabs:
                        tabs.addTab(tab)
                        host.addOwnedPage(
                            make_page(
                                tab.text,
                                "{0} content hosted by the selected tab.".format(
                                    tab.text
                                ),
                            )
                        )
                    tabs.setSelectedIndex(0)
                    host.setCurrentIndex(0, 0, False)
                    status = make_status_label(root, "Selected tab: Home")

                    def select_tab(index):
                        if 0 <= index < host.count():
                            host.setCurrentIndex(index, 0, True)
                            status.setText(
                                "Selected tab: {0}".format(tabs.tabAt(index).text)
                            )

                    def move_tab(source, destination):
                        host.movePage(source, destination)
                        current = tabs.selectedIndex()
                        if 0 <= current < host.count():
                            host.setCurrentIndex(current, 0, False)
                        status.setText(
                            "Moved tab {0} to {1}".format(source + 1, destination + 1)
                        )

                    tabs.currentChanged.connect(select_tab)
                    tabs.tabMoved.connect(move_tab)
                    surface_layout.addWidget(tabs)
                    surface_layout.addWidget(host)
                    layout.addWidget(surface)
                    layout.addWidget(status)
                    """
                ),
                "from PySide6.QtGui import QPainter, QPen\n"
                "from PySide6.QtWidgets import QSizePolicy\n"
                "from fluentqt_gallery.foundation_pages import _theme_tokens\n"
                + _WIDGETS,
            ),
        ),
        "tab-view-add-close": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                tabs = fluentqt.TabView(root)
                tabs.setFixedSize(560, 40)
                tabs.setTabWidthMode(fluentqt.TabView.TabWidthMode.SizeToContent)
                tabs.setCloseButtonOverlayMode(fluentqt.TabView.CloseButtonOverlayMode.Always)
                tabs.setAddTabButtonVisible(True)
                tabs.setTabsClosable(True)
                tabs.addTab(fluentqt.TabViewItem("Home", "\ue80f", False))
                tabs.addTab(fluentqt.TabViewItem("Draft", "\ue8a5"))
                tabs.addTab(fluentqt.TabViewItem("Review", "\ue70f"))
                tabs.addTab(fluentqt.TabViewItem("Disabled", "\ue72e", True, False))
                tabs.setSelectedIndex(0)
                status = fluentqt.Label(
                    "Home is pinned; Disabled is not selectable.", root
                )
                status.setFluentTypography(fluentqt.FontRole.Caption)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                document_number = [0]

                def add_tab():
                    document_number[0] += 1
                    title = f"Document {document_number[0]}"
                    index = tabs.addTab(fluentqt.TabViewItem(title, "\ue8a5"))
                    tabs.setSelectedIndex(index)
                    status.setText(f"Added {title}")

                def close_tab(index):
                    title = tabs.tabAt(index).text
                    if tabs.closeTab(index):
                        status.setText(f"Closed {title}")
                    else:
                        status.setText(f"{title} cannot be closed")

                tabs.addTabRequested.connect(add_tab)
                tabs.tabCloseRequested.connect(close_tab)
                tabs.currentChanged.connect(
                    lambda index: status.setText(f"Selected {tabs.tabAt(index).text}")
                    if index >= 0 else None
                )
                layout.addWidget(tabs)
                layout.addWidget(status)
                """
                ,
                _WIDGETS,
            ),
        ),
        "tab-view-keyboard-accelerators": (
            "root",
            _script(
                _page_source_helper()
                + dedent(
                    """
                    root = QWidget(globals().get("gallery_parent"))
                    layout = QVBoxLayout(root)
                    controls = QWidget(root)
                    controls_layout = QHBoxLayout(controls)
                    controls_layout.setContentsMargins(0, 0, 0, 0)
                    controls_layout.setSpacing(8)
                    controls_layout.setAlignment(
                        Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                    )
                    enable = fluentqt.Button("Enable", controls)
                    disable = fluentqt.Button("Disable", controls)
                    for button in (enable, disable):
                        button.setFluentSize(fluentqt.Button.ButtonSize.Small)
                        controls_layout.addWidget(button)

                    tabs = fluentqt.TabView(root)
                    tabs.setFixedSize(560, 40)
                    tabs.setTabWidthMode(
                        fluentqt.TabView.TabWidthMode.SizeToContent
                    )
                    tabs.setCloseButtonOverlayMode(
                        fluentqt.TabView.CloseButtonOverlayMode.Always
                    )
                    tabs.setAddTabButtonVisible(True)
                    tabs.setTabsClosable(True)
                    tabs.setKeyboardAcceleratorsEnabled(True)
                    for title in ("Shortcut A", "Shortcut B", "Shortcut C"):
                        tabs.addTab(fluentqt.TabViewItem(title, "\ue8a5"))
                    tabs.setSelectedIndex(0)
                    status = make_status_label(
                        root,
                        "Accelerators enabled, tabs: 3, selected: Shortcut A",
                    )
                    status.setFluentTypography(fluentqt.FontRole.Caption)
                    shortcut_number = [0]

                    def update_status():
                        index = tabs.selectedIndex()
                        selected = tabs.tabAt(index).text if index >= 0 else "none"
                        enabled = tabs.keyboardAcceleratorsEnabled()
                        enable.setFluentStyle(
                            fluentqt.Button.ButtonStyle.Standard
                            if enabled
                            else fluentqt.Button.ButtonStyle.Accent
                        )
                        disable.setFluentStyle(
                            fluentqt.Button.ButtonStyle.Accent
                            if enabled
                            else fluentqt.Button.ButtonStyle.Standard
                        )
                        state = "enabled" if enabled else "disabled"
                        status.setText(
                            f"Accelerators {state}, tabs: {tabs.tabCount()}, selected: {selected}"
                        )

                    def add_tab():
                        shortcut_number[0] += 1
                        title = f"Added {shortcut_number[0]}"
                        index = tabs.addTab(
                            fluentqt.TabViewItem(title, "\ue8a5")
                        )
                        tabs.setSelectedIndex(index)
                        update_status()

                    def close_tab(index):
                        tabs.closeTab(index)
                        update_status()

                    def set_accelerators(enabled):
                        tabs.setKeyboardAcceleratorsEnabled(enabled)
                        update_status()

                    tabs.addTabRequested.connect(add_tab)
                    tabs.tabCloseRequested.connect(close_tab)
                    tabs.currentChanged.connect(lambda _index: update_status())
                    enable.clicked.connect(lambda: set_accelerators(True))
                    disable.clicked.connect(lambda: set_accelerators(False))
                    update_status()
                    layout.addWidget(controls)
                    layout.addWidget(tabs)
                    layout.addWidget(status)
                    """
                ),
                _WIDGETS,
            ),
        ),
        "tab-view-width-modes": (
            "root",
            _script(
                _tab_items_source()
                + dedent(
                    """
                    root = QWidget()
                    layout = QVBoxLayout(root)
                    for label_text, mode, selected_index in (
                        ("Equal", fluentqt.TabView.TabWidthMode.Equal, 0),
                        (
                            "SizeToContent",
                            fluentqt.TabView.TabWidthMode.SizeToContent,
                            0,
                        ),
                        ("Compact", fluentqt.TabView.TabWidthMode.Compact, 1),
                    ):
                        row = QWidget(root)
                        row_layout = QHBoxLayout(row)
                        row_layout.setContentsMargins(0, 0, 0, 0)
                        row_layout.setSpacing(10)
                        row_layout.setAlignment(
                            Qt.AlignmentFlag.AlignLeft
                            | Qt.AlignmentFlag.AlignVCenter
                        )
                        label = fluentqt.Label(label_text, row)
                        label.setFluentTypography(fluentqt.FontRole.Caption)
                        label.setTextColorRole(
                            fluentqt.Label.TextColorRole.Primary
                        )
                        label.setFixedWidth(104)
                        tabs = fluentqt.TabView(row)
                        tabs.setFixedSize(430, 40)
                        add_standard_tabs(tabs)
                        tabs.setTabWidthMode(mode)
                        tabs.setSelectedIndex(selected_index)
                        row_layout.addWidget(label)
                        row_layout.addWidget(tabs)
                        layout.addWidget(row)
                    """
                ),
                _WIDGETS,
            ),
        ),
        "tab-view-close-button-modes": (
            "root",
            _script(
                """
                root = QWidget(globals().get("gallery_parent"))
                layout = QVBoxLayout(root)
                for label_text, mode in (
                    ("Auto", fluentqt.TabView.CloseButtonOverlayMode.Auto),
                    ("OnHover", fluentqt.TabView.CloseButtonOverlayMode.OnHover),
                    ("Always", fluentqt.TabView.CloseButtonOverlayMode.Always),
                ):
                    row = QWidget(root)
                    row_layout = QHBoxLayout(row)
                    row_layout.setContentsMargins(0, 0, 0, 0)
                    row_layout.setSpacing(10)
                    row_layout.setAlignment(
                        Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                    )
                    label = fluentqt.Label(label_text, row)
                    label.setFluentTypography(fluentqt.FontRole.Caption)
                    label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                    label.setFixedWidth(104)
                    tabs = fluentqt.TabView(row)
                    tabs.setFixedSize(430, 40)
                    tabs.setTabWidthMode(
                        fluentqt.TabView.TabWidthMode.SizeToContent
                    )
                    tabs.setCloseButtonOverlayMode(mode)
                    tabs.setAddTabButtonVisible(False)
                    tabs.addTab(fluentqt.TabViewItem("Primary", "\uea3a"))
                    tabs.addTab(fluentqt.TabViewItem("Reference", "\ue8a5"))
                    tabs.addTab(
                        fluentqt.TabViewItem("Pinned", "\ue718", False)
                    )
                    row_layout.addWidget(label)
                    row_layout.addWidget(tabs)
                    layout.addWidget(row)
                """,
                _WIDGETS,
            ),
        ),
        "tab-view-overflow-reorder": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                tabs = fluentqt.TabView(root)
                tabs.setTabWidthMode(fluentqt.TabView.TabWidthMode.SizeToContent)
                tabs.setTabReorderEnabled(True)
                tabs.setTabsClosable(False)
                tabs.setAddTabButtonVisible(False)
                tabs.setFixedSize(360, 40)
                for index in range(1, 9):
                    tabs.addTab(
                        fluentqt.TabViewItem(
                            f"Document {index} with longer title",
                            "\ue8a5",
                        )
                    )
                tabs.setSelectedIndex(5)
                status = fluentqt.Label(
                    "Selected: Document 6 with longer title", root
                )
                status.setFluentTypography(fluentqt.FontRole.Caption)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                tabs.currentChanged.connect(
                    lambda index: status.setText(f"Selected: {tabs.tabAt(index).text}")
                    if index >= 0 else None
                )
                tabs.tabMoved.connect(
                    lambda old, new: status.setText(f"Moved tab {old + 1} to {new + 1}")
                )
                layout.addWidget(tabs)
                layout.addWidget(status)
                """
                ,
                _WIDGETS,
            ),
        ),
    },
)
