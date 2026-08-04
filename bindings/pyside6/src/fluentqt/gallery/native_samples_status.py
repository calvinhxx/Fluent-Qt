"""Source-driven ports of native Gallery status and notification samples."""

from __future__ import annotations

from textwrap import dedent

from .native_samples import register_source_samples


def _script(body: str, imports: str = "") -> str:
    prefix = "import fluentqt\n"
    if imports:
        prefix += imports.strip() + "\n"
    return prefix + "\n" + dedent(body).strip() + "\n"


_WIDGETS = "from PySide6.QtWidgets import QHBoxLayout, QVBoxLayout, QWidget"

_STATUS_IMPORTS = (
    "from PySide6.QtCore import QRectF, Qt\n"
    "from PySide6.QtGui import QColor, QPainter, QPen\n"
    "from PySide6.QtWidgets import (QHBoxLayout, QSizePolicy, QVBoxLayout, QWidget)\n"
    "from fluentqt.gallery.foundation_pages import _theme_tokens"
)

_STATUS_HELPER = dedent(
    """
    class StatusInfoSampleSurface(QWidget):
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
            colors = _theme_tokens()
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setPen(QPen(colors["strokeCard"], 1.0))
            painter.setBrush(colors["bgCanvas"])
            painter.drawRoundedRect(
                QRectF(self.rect()).adjusted(0.0, 0.0, -1.0, -1.0),
                8.0,
                8.0,
            )


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
            self.label.setAlignment(Qt.AlignmentFlag.AlignCenter)
            self.label.setWordWrap(False)
            self.label.setTextColorRole(
                fluentqt.Label.TextColorRole.Secondary
            )
            layout.addWidget(self.label, 0, Qt.AlignmentFlag.AlignCenter)

        def setText(self, text):
            self.label.setText(text)

        def text(self):
            return self.label.text()

        def paintEvent(self, event):
            del event
            colors = _theme_tokens()
            painter = QPainter(self)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setPen(QPen(colors["strokeCard"], 1.0))
            painter.setBrush(colors["controlSecondary"])
            painter.drawRoundedRect(
                QRectF(self.rect()).adjusted(0.0, 0.0, -1.0, -1.0),
                4.0,
                4.0,
            )


    def make_status_surface(spacing=12):
        surface = StatusInfoSampleSurface(
            globals().get("gallery_parent"), spacing
        )
        return surface, surface.layout()


    def horizontal_group(parent, spacing=12):
        group = QWidget(parent)
        group_layout = QHBoxLayout(group)
        group_layout.setContentsMargins(0, 0, 0, 0)
        group_layout.setSpacing(spacing)
        group_layout.setAlignment(
            Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
        )
        return group, group_layout


    def make_status_label(parent, text):
        label = fluentqt.Label(text, parent)
        label.setFluentTypography(fluentqt.FontRole.Body)
        label.setWordWrap(True)
        label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        return label


    def make_caption_label(parent, text):
        label = fluentqt.Label(text, parent)
        label.setFluentTypography(fluentqt.FontRole.Caption)
        label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        return label


    def labeled_column(parent, label_text, content):
        cell = QWidget(parent)
        cell.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        cell_layout = QVBoxLayout(cell)
        cell_layout.setContentsMargins(0, 0, 0, 0)
        cell_layout.setSpacing(6)
        cell_layout.setAlignment(
            Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
        )
        content.setParent(cell)
        cell_layout.addWidget(content, 0, Qt.AlignmentFlag.AlignHCenter)
        label = make_caption_label(cell, label_text)
        label.setMinimumWidth(max(72, content.sizeHint().width()))
        cell_layout.addWidget(label, 0, Qt.AlignmentFlag.AlignHCenter)
        return cell


    def sample_button(parent, text):
        button = fluentqt.Button(text, parent)
        button.setFluentSize(fluentqt.Button.ButtonSize.Small)
        return button


    def make_status_pill(parent, text):
        return SampleStatusPill(text, parent)


    def make_info_bar(parent, severity, title, message, single_line=True):
        bar = fluentqt.InfoBar(parent)
        bar.setCloseButtonAccessibleName("Dismiss notification")
        bar.setPreferredWidth(520)
        bar.setSeverity(severity)
        bar.setTitle(title)
        bar.setMessage(message)
        bar.setSingleLine(single_line)
        bar.setIsOpen(True)
        return bar


    def keep_info_bar_open(bar):
        def reopen():
            bar.setIsOpen(True)
            parent = bar.parentWidget()
            if parent is not None and parent.layout() is not None:
                parent.layout().activate()
        bar.closed.connect(reopen)


    def progress_bar_row(parent, label_text, progress_bar):
        row, layout = horizontal_group(parent, 12)
        label = make_status_label(row, label_text)
        label.setFixedWidth(96)
        progress_bar.setParent(row)
        layout.addWidget(label, 0, Qt.AlignmentFlag.AlignVCenter)
        layout.addWidget(progress_bar, 0, Qt.AlignmentFlag.AlignVCenter)
        return row
    """
).strip()


def _status_script(body: str, imports: str = "") -> str:
    combined_imports = _STATUS_IMPORTS
    if imports:
        combined_imports += "\n" + imports.strip()
    return _script(_STATUS_HELPER + "\n\n" + dedent(body).strip(), combined_imports)


register_source_samples(
    "avatar",
    ("Avatar",),
    {
        "avatar-initials-sizes": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                row, row_layout = horizontal_group(root, 20)
                values = (
                    (
                        "Ada Lovelace",
                        "Small",
                        fluentqt.Avatar.AvatarSize.Small,
                    ),
                    (
                        "Grace Hopper",
                        "Medium",
                        fluentqt.Avatar.AvatarSize.Medium,
                    ),
                    (
                        "Lin Chen",
                        "Large",
                        fluentqt.Avatar.AvatarSize.Large,
                    ),
                    (
                        "Sam Rivera",
                        "Extra large",
                        fluentqt.Avatar.AvatarSize.ExtraLarge,
                    ),
                )
                for name, caption, size in values:
                    avatar = fluentqt.Avatar(name, row)
                    avatar.setAvatarSize(size)
                    row_layout.addWidget(labeled_column(row, caption, avatar))
                layout.addWidget(row, 0, Qt.AlignmentFlag.AlignLeft)
                """
            ),
        ),
        "avatar-image-presence": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                row, row_layout = horizontal_group(root, 20)
                profile = fluentqt.Avatar("Product account", row)
                profile.setAvatarSize(fluentqt.Avatar.AvatarSize.ExtraLarge)
                profile.setImage(QPixmap(str(asset_path("app-icon.png"))))
                profile.setPresence(fluentqt.Avatar.PresenceStatus.Available)
                away = fluentqt.Avatar("Alex Morgan", row)
                away.setAvatarSize(fluentqt.Avatar.AvatarSize.Large)
                away.setShape(fluentqt.Avatar.AvatarShape.Square)
                away.setPresence(fluentqt.Avatar.PresenceStatus.Away)
                busy = fluentqt.Avatar("Jordan Lee", row)
                busy.setAvatarSize(fluentqt.Avatar.AvatarSize.Large)
                busy.setPresence(fluentqt.Avatar.PresenceStatus.Busy)
                offline = fluentqt.Avatar("Taylor Reed", row)
                offline.setAvatarSize(fluentqt.Avatar.AvatarSize.Large)
                offline.setPresence(fluentqt.Avatar.PresenceStatus.Offline)
                for caption, avatar in (
                    ("Available", profile),
                    ("Away", away),
                    ("Busy", busy),
                    ("Offline", offline),
                ):
                    row_layout.addWidget(labeled_column(row, caption, avatar))
                layout.addWidget(row, 0, Qt.AlignmentFlag.AlignLeft)
                """,
                "from PySide6.QtGui import QPixmap\n"
                "from fluentqt.gallery.visual import asset_path",
            ),
        ),
    },
)


register_source_samples(
    "info-badge",
    ("InfoBadge",),
    {
        "info-badge-display-modes": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                row, row_layout = horizontal_group(root, 28)
                dot_badge = fluentqt.InfoBadge(row)
                icon_badge = fluentqt.InfoBadge(row)
                icon_badge.setIconGlyph("\ue715")
                value_badge = fluentqt.InfoBadge(row)
                value_badge.setValue(7)
                explicit_dot = fluentqt.InfoBadge(row)
                explicit_dot.setDisplayMode(
                    fluentqt.InfoBadge.InfoBadgeDisplayMode.Dot
                )
                for caption, badge in (
                    ("Auto dot", dot_badge),
                    ("Auto icon", icon_badge),
                    ("Auto value", value_badge),
                    ("Explicit dot", explicit_dot),
                ):
                    row_layout.addWidget(labeled_column(row, caption, badge))
                layout.addWidget(row)
                """
            ),
        ),
        "info-badge-status-colors": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                row, row_layout = horizontal_group(root, 22)
                statuses = (
                    ("Info", fluentqt.InfoBadge.InfoBadgeStatus.Informational),
                    ("Attention", fluentqt.InfoBadge.InfoBadgeStatus.Attention),
                    ("Caution", fluentqt.InfoBadge.InfoBadgeStatus.Caution),
                    ("Success", fluentqt.InfoBadge.InfoBadgeStatus.Success),
                    ("Critical", fluentqt.InfoBadge.InfoBadgeStatus.Critical),
                )
                for caption, badge_status in statuses:
                    badge = fluentqt.InfoBadge(row)
                    badge.setDisplayMode(
                        fluentqt.InfoBadge.InfoBadgeDisplayMode.Value
                    )
                    badge.setStatus(badge_status)
                    badge.setValue(5)
                    row_layout.addWidget(labeled_column(row, caption, badge))
                layout.addWidget(row)
                """
            ),
        ),
        "info-badge-custom-metrics": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                host = QWidget(root)
                host.setFixedSize(170, 70)
                inbox = fluentqt.Button("Inbox", host)
                inbox.setGeometry(0, 18, 146, 34)
                badge = fluentqt.InfoBadge(host)
                badge.setDisplayMode(fluentqt.InfoBadge.InfoBadgeDisplayMode.Icon)
                badge.setIconGlyph("\ue715")
                badge.setCustomBackgroundColor(QColor("#C42B1C"))
                badge.setCustomTextColor(Qt.GlobalColor.white)
                badge.setBadgeHeight(18)
                badge.setIconGlyphSize(12)
                badge.resize(badge.sizeHint())
                badge.move(132, 8)
                layout.addWidget(host, 0, Qt.AlignmentFlag.AlignLeft)
                """,
            ),
        ),
        "info-badge-accessibility": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                inbox = fluentqt.Button("Inbox", root)
                inbox.setAccessibleName("Inbox")
                inbox.setFixedSize(164, 42)
                badge = fluentqt.InfoBadge(inbox)
                badge.setObjectName("galleryInfoBadgeAccessibleValue")
                badge.setDisplayMode(fluentqt.InfoBadge.InfoBadgeDisplayMode.Value)
                badge.setAccessibleName("Unread messages")
                badge.setValue(3)
                badge.resize(badge.sizeHint())
                badge.move(inbox.width() - badge.width() - 6, 4)
                row, row_layout = horizontal_group(root, 8)
                increment = sample_button(row, "Increment")
                increment.setObjectName("galleryInfoBadgeAccessibleIncrement")
                toggle = sample_button(row, "Toggle badge")
                toggle.setObjectName("galleryInfoBadgeAccessibleToggle")
                status = make_status_label(root, "Unread value: 3")
                status.setObjectName("galleryInfoBadgeAccessibleStatus")

                def increment_value():
                    badge.setValue(badge.value() + 1)
                    status.setText(f"Unread value: {badge.value()}")

                def toggle_badge():
                    show_badge = badge.isHidden()
                    badge.setVisible(show_badge)
                    status.setText("Badge visible" if show_badge else "Badge hidden")

                increment.clicked.connect(increment_value)
                toggle.clicked.connect(toggle_badge)
                row_layout.addWidget(increment)
                row_layout.addWidget(toggle)
                layout.addWidget(inbox, 0, Qt.AlignmentFlag.AlignLeft)
                layout.addWidget(row)
                layout.addWidget(status)
                """
            ),
        ),
    },
)


register_source_samples(
    "info-bar",
    ("InfoBar",),
    {
        "info-bar-severities": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface(10)
                for severity, title, message in (
                    (
                        fluentqt.InfoBar.InfoBarSeverity.Informational,
                        "Update available",
                        "Version 3.2 is ready.",
                    ),
                    (
                        fluentqt.InfoBar.InfoBarSeverity.Success,
                        "Saved",
                        "All changes were saved.",
                    ),
                    (
                        fluentqt.InfoBar.InfoBarSeverity.Warning,
                        "Storage almost full",
                        "Clear space before syncing.",
                    ),
                    (
                        fluentqt.InfoBar.InfoBarSeverity.Error,
                        "Upload failed",
                        "The document could not be saved.",
                    ),
                ):
                    bar = make_info_bar(root, severity, title, message)
                    keep_info_bar_open(bar)
                    layout.addWidget(bar)
                """
            ),
        ),
        "info-bar-action-layout": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                retry = fluentqt.Button("Retry", root)
                info_bar = make_info_bar(
                    root,
                    fluentqt.InfoBar.InfoBarSeverity.Warning,
                    "Sync paused",
                    "Some files need attention before the next sync can finish.",
                    False,
                )
                info_bar.setActionWidget(retry)
                keep_info_bar_open(info_bar)
                layout.addWidget(info_bar)
                """
            ),
        ),
        "info-bar-open-close": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface(12)
                controls, controls_layout = horizontal_group(root, 10)
                open_button = sample_button(controls, "Show again")
                open_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
                open_button.setFluentLayout(
                    fluentqt.Button.ButtonLayout.IconBefore
                )
                open_button.setIconGlyph("\ue72c")
                open_button.setEnabled(False)
                status = make_status_pill(controls, "Visible")
                controls_layout.addWidget(
                    open_button, 0, Qt.AlignmentFlag.AlignVCenter
                )
                controls_layout.addWidget(
                    status, 0, Qt.AlignmentFlag.AlignVCenter
                )
                info_bar = make_info_bar(
                    root,
                    fluentqt.InfoBar.InfoBarSeverity.Informational,
                    "Draft saved",
                    "You can safely leave this page.",
                )
                info_bar.setIsClosable(True)

                def reopen():
                    info_bar.setIsOpen(True)
                    status.setText("Visible")
                    open_button.setEnabled(False)

                def closed():
                    status.setText("Dismissed")
                    open_button.setEnabled(True)

                open_button.clicked.connect(reopen)
                info_bar.closed.connect(closed)
                layout.addWidget(controls)
                layout.addWidget(info_bar)
                """
            ),
        ),
    },
)


register_source_samples(
    "progress-bar",
    ("ProgressBar",),
    {
        "progress-bar-determinate-value": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                progress_bar = fluentqt.ProgressBar(root)
                progress_bar.setRange(0, 100)
                progress_bar.setBarWidth(320)

                status = make_status_label(root, "Progress: 44%")
                status.setWordWrap(False)
                status.setTextElideMode(Qt.TextElideMode.ElideNone)
                status.setFixedWidth(124)

                progress_box = fluentqt.NumberBox(root)
                progress_box.setHeader("Progress")
                progress_box.setRange(0, 100)
                progress_box.setSmallChange(1)
                progress_box.setLargeChange(10)
                progress_box.setDisplayPrecision(0)
                progress_box.setSpinButtonPlacementMode(
                    fluentqt.NumberBox.SpinButtonPlacementMode.Inline
                )
                progress_box.setFixedWidth(156)

                def update(value):
                    value = value if math.isfinite(value) else 0.0
                    progress_bar.setValue(value)
                    status.setText(f"Progress: {progress_bar.progressText()}%")

                progress_box.valueChanged.connect(update)
                progress_box.setValue(44)

                row, row_layout = horizontal_group(root, 28)
                row_layout.addWidget(
                    progress_bar, 0, Qt.AlignmentFlag.AlignVCenter
                )
                row_layout.addWidget(
                    progress_box, 0, Qt.AlignmentFlag.AlignVCenter
                )
                layout.addWidget(row)
                layout.addWidget(status)
                """,
                "import math",
            ),
        ),
        "progress-bar-states": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface(10)
                running = fluentqt.ProgressBar(root)
                running.setIsIndeterminate(True)
                running.setBarWidth(300)
                paused = fluentqt.ProgressBar(root)
                paused.setValue(65)
                paused.setShowPaused(True)
                paused.setBarWidth(300)
                error = fluentqt.ProgressBar(root)
                error.setValue(65)
                error.setShowError(True)
                error.setBarWidth(300)
                for caption, bar in (
                    ("Running", running),
                    ("Paused", paused),
                    ("Error", error),
                ):
                    layout.addWidget(progress_bar_row(root, caption, bar))
                """
            ),
        ),
        "progress-bar-metrics": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface(10)
                thick = fluentqt.ProgressBar(root)
                thick.setValue(70)
                thick.setBarWidth(280)
                thick.setTrackThickness(6.0)
                no_rail = fluentqt.ProgressBar(root)
                no_rail.setValue(70)
                no_rail.setBarWidth(280)
                no_rail.setRailVisible(False)
                layout.addWidget(progress_bar_row(root, "6 px track", thick))
                layout.addWidget(progress_bar_row(root, "No rail", no_rail))
                """
            ),
        ),
    },
)


register_source_samples(
    "progress-ring",
    ("ProgressRing",),
    {
        "progress-ring-indeterminate-sizes": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                row, row_layout = horizontal_group(root, 30)
                small = fluentqt.ProgressRing(row)
                small.setRingSize(fluentqt.ProgressRing.ProgressRingSize.Small)
                small.setIsActive(True)
                medium = fluentqt.ProgressRing(row)
                medium.setRingSize(fluentqt.ProgressRing.ProgressRingSize.Medium)
                medium.setIsActive(True)
                large = fluentqt.ProgressRing(row)
                large.setRingSize(fluentqt.ProgressRing.ProgressRingSize.Large)
                large.setIsActive(True)
                for caption, ring in (
                    ("Small", small),
                    ("Medium", medium),
                    ("Large", large),
                ):
                    row_layout.addWidget(labeled_column(row, caption, ring))
                layout.addWidget(row)
                """
            ),
        ),
        "progress-ring-determinate-value": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                row, row_layout = horizontal_group(root, 28)
                ring = fluentqt.ProgressRing(row)
                ring.setIsIndeterminate(False)
                ring.setIsActive(True)
                ring.setRingSize(fluentqt.ProgressRing.ProgressRingSize.Large)
                ring.setBackgroundVisible(True)
                progress_box = fluentqt.NumberBox(row)
                progress_box.setHeader("Progress")
                progress_box.setRange(0, 100)
                progress_box.setSmallChange(1)
                progress_box.setLargeChange(10)
                progress_box.setDisplayPrecision(0)
                progress_box.setSpinButtonPlacementMode(
                    fluentqt.NumberBox.SpinButtonPlacementMode.Inline
                )
                progress_box.setFixedWidth(156)
                status = make_status_label(row, "Value: 44%")
                status.setWordWrap(False)
                status.setTextElideMode(Qt.TextElideMode.ElideNone)
                status.setFixedWidth(124)

                def update(value):
                    progress = int(value) if math.isfinite(value) else 0
                    ring.setValue(progress)
                    status.setText(f"Value: {progress}%")

                progress_box.valueChanged.connect(update)
                progress_box.setValue(44)
                row_layout.addWidget(ring, 0, Qt.AlignmentFlag.AlignVCenter)
                row_layout.addWidget(
                    progress_box, 0, Qt.AlignmentFlag.AlignVCenter
                )
                row_layout.addWidget(status, 0, Qt.AlignmentFlag.AlignVCenter)
                layout.addWidget(row)
                """,
                "import math",
            ),
        ),
        "progress-ring-status": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                row, row_layout = horizontal_group(root, 30)
                for caption, ring_status in (
                    ("Running", fluentqt.ProgressRing.ProgressRingStatus.Running),
                    ("Paused", fluentqt.ProgressRing.ProgressRingStatus.Paused),
                    ("Error", fluentqt.ProgressRing.ProgressRingStatus.Error),
                ):
                    ring = fluentqt.ProgressRing(row)
                    ring.setIsIndeterminate(False)
                    ring.setIsActive(True)
                    ring.setRingSize(fluentqt.ProgressRing.ProgressRingSize.Large)
                    ring.setValue(65)
                    ring.setStatus(ring_status)
                    ring.setBackgroundVisible(True)
                    row_layout.addWidget(labeled_column(row, caption, ring))
                layout.addWidget(row)
                """
            ),
        ),
    },
)


register_source_samples(
    "toast",
    ("Toast",),
    {
        "toast-severity": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                toast = fluentqt.Toast(root)
                toast.setObjectName("galleryToastSeveritySample")
                toast.setDuration(2200)
                values = (
                    ("Info", "Draft saved locally", fluentqt.Toast.Severity.Informational),
                    ("Success", "Changes published", fluentqt.Toast.Severity.Success),
                    ("Warning", "Connection is unstable", fluentqt.Toast.Severity.Warning),
                    ("Error", "Upload could not finish", fluentqt.Toast.Severity.Error),
                )
                row, row_layout = horizontal_group(root, 8)
                buttons = []
                for text, message, severity in values:
                    button = sample_button(row, text)
                    button.clicked.connect(
                        lambda _checked=False, anchor=button, value=message, level=severity:
                            (toast.setMessage(value), toast.setSeverity(level), toast.present(anchor))
                    )
                    row_layout.addWidget(button)
                    buttons.append(button)
                layout.addWidget(row)
                layout.addWidget(
                    make_status_label(
                        root,
                        "The same toast instance updates for each severity.",
                    )
                )
                """
            ),
        ),
        "toast-title-placement": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                row, row_layout = horizontal_group(root, 8)
                toast = fluentqt.Toast(root)
                toast.setObjectName("galleryToastPlacementSample")
                toast.setTitle("Sync complete")
                toast.setMessage("12 files are now available offline.")
                toast.setSeverity(fluentqt.Toast.Severity.Success)
                toast.setDuration(2600)
                for text, placement in (
                    ("Top start", fluentqt.Toast.Placement.TopStart),
                    ("Top", fluentqt.Toast.Placement.Top),
                    ("Top end", fluentqt.Toast.Placement.TopEnd),
                    ("Bottom start", fluentqt.Toast.Placement.BottomStart),
                    ("Bottom", fluentqt.Toast.Placement.Bottom),
                    ("Bottom end", fluentqt.Toast.Placement.BottomEnd),
                ):
                    button = sample_button(row, text)
                    button.clicked.connect(
                        lambda _checked=False, anchor=button, value=placement:
                            (toast.setPlacement(value), toast.present(anchor))
                    )
                    row_layout.addWidget(button)
                layout.addWidget(row)
                """
            ),
        ),
        "toast-stacking": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                row, row_layout = horizontal_group(root, 8)
                stack_top = sample_button(row, "Stack at top")
                stack_end = sample_button(row, "Stack at top end")
                counters = [0, 0]

                def show_top():
                    counters[0] += 1
                    fluentqt.Toast.showToast(
                        stack_top,
                        "Top notice {0}".format(counters[0]),
                        fluentqt.Toast.Severity.Informational,
                    )

                def show_end():
                    counters[1] += 1
                    fluentqt.Toast.showToast(
                        stack_end,
                        "Corner notice {0}".format(counters[1]),
                        fluentqt.Toast.Severity.Warning,
                        2200,
                        fluentqt.Toast.Placement.TopEnd,
                    )

                stack_top.clicked.connect(show_top)
                stack_end.clicked.connect(show_end)
                row_layout.addWidget(stack_top)
                row_layout.addWidget(stack_end)
                layout.addWidget(row)
                layout.addWidget(make_status_label(
                    root,
                    "Default maximumVisible is 3; older toasts dismiss first.",
                ))
                """
            ),
        ),
        "toast-action-lifecycle": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                retry = QAction("Retry", root)
                toast = fluentqt.Toast(root)
                toast.setObjectName("galleryToastLifecycleSample")
                toast.setAction(retry)
                toast.setPauseOnHoverEnabled(True)
                toast.setDuration(5000)
                show = sample_button(root, "Show actionable toast")
                show.setObjectName("galleryToastLifecycleTrigger")
                status = make_status_label(root, "Ready")
                status.setObjectName("galleryToastLifecycleStatus")

                def present():
                    toast.setMessage("Upload failed. Retry when ready.")
                    toast.setSeverity(fluentqt.Toast.Severity.Error)
                    toast.present(show)
                    status.setText("Toast open; hover pauses timeout")

                show.clicked.connect(present)
                retry.triggered.connect(lambda: status.setText("Retry requested"))
                toast.dismissedWithReason.connect(
                    lambda reason: status.setText(f"Dismissed: {reason.name}")
                )
                layout.addWidget(show, 0, Qt.AlignmentFlag.AlignLeft)
                layout.addWidget(status)
                """,
                "from PySide6.QtGui import QAction",
            ),
        ),
        "toast-update-key": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                advance = sample_button(root, "Advance upload")
                advance.setObjectName("galleryToastUpdateTrigger")
                status = make_status_label(root, "Progress: 0%")
                status.setObjectName("galleryToastUpdateStatus")
                advance.setProperty("progress", 0)

                def advance_upload():
                    current = int(advance.property("progress"))
                    progress = 25 if current >= 100 else current + 25
                    advance.setProperty("progress", progress)
                    fluentqt.Toast.showOrUpdateToast(
                        advance,
                        "upload",
                        "Upload complete" if progress == 100 else f"Uploading: {progress}%",
                        fluentqt.Toast.Severity.Success if progress == 100 else fluentqt.Toast.Severity.Informational,
                        5000,
                        fluentqt.Toast.Placement.TopEnd,
                    )
                    status.setText(f"Progress: {progress}%")

                advance.clicked.connect(advance_upload)
                layout.addWidget(advance, 0, Qt.AlignmentFlag.AlignLeft)
                layout.addWidget(status)
                """
            ),
        ),
    },
)


register_source_samples(
    "tooltip",
    ("ToolTip",),
    {
        "tooltip-hover": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                button = fluentqt.Button("Archive", root)
                tooltip = fluentqt.ToolTip.attach(
                    button,
                    "Move the selected message to Archive",
                    fluentqt.ToolTip.Placement.Above,
                )
                layout.addWidget(button, 0, Qt.AlignmentFlag.AlignLeft)
                """
            ),
        ),
        "tooltip-margins-font": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                button = fluentqt.Button("Custom tip", root)
                tooltip = fluentqt.ToolTip.attach(
                    button, "Larger content margins", fluentqt.ToolTip.Placement.Right
                )
                tooltip.setMargins(QMargins(20, 10, 20, 10))
                font = QFont()
                font.setPixelSize(15)
                font.setItalic(True)
                tooltip.setFont(font)
                layout.addWidget(button, 0, Qt.AlignmentFlag.AlignLeft)
                """,
                "from PySide6.QtCore import QMargins\nfrom PySide6.QtGui import QFont",
            ),
        ),
        "tooltip-animation": (
            "root",
            _status_script(
                """
                root, layout = make_status_surface()
                button = fluentqt.Button("Instant tip", root)
                tooltip = fluentqt.ToolTip.attach(
                    button, "Animation disabled", fluentqt.ToolTip.Placement.Above
                )
                tooltip.setAnimationEnabled(False)
                layout.addWidget(button, 0, Qt.AlignmentFlag.AlignLeft)
                """
            ),
        ),
    },
)
