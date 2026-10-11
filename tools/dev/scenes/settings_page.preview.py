"""A settings surface for demonstrating the shared live-preview tools."""

import fluentqt
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QPalette
from PySide6.QtWidgets import QHBoxLayout, QSizePolicy, QStackedWidget, QVBoxLayout, QWidget
from fluent_qt_preview_editor import Field, register_region


SCENE_TITLE = "设置"
CONTENT_INSET = 44
ROW_GAP = 18
SIDEBAR_WIDTH = 184

# Only these literal values are editable through the optional development tool.
PREVIEW_EDITS = {
    "workspace": {"row_padding": 18, "description_tone": "secondary"},
    "notifications": {"row_padding": 18, "heading_emphasis": True, "description_tone": "secondary"},
}

ROW_FIELDS = (
    Field("row_padding", "行内留白", "int", minimum=4, maximum=28),
    Field("description_tone", "说明文字", "choice", choices=(("primary", "正文"), ("secondary", "次要"))),
)


class Surface(fluentqt.FluentWidget):
    """Apply existing semantic colors to the two composition surfaces."""

    def __init__(self, role, parent=None):
        self._role = role
        super().__init__(parent)
        self.setAutoFillBackground(True)
        self.on_theme_updated()

    def on_theme_updated(self):
        colors = self.theme_tokens().colors
        palette = self.palette()
        palette.setColor(QPalette.ColorRole.Window, QColor(colors[self._role]))
        palette.setColor(QPalette.ColorRole.WindowText, QColor(colors.textPrimary))
        self.setPalette(palette)
        self.update()


def label(text, parent, *, role=fluentqt.FontRole.Body, secondary=False, name=""):
    widget = fluentqt.Label(text, parent)
    widget.setObjectName(name)
    widget.setFluentTypography(role)
    widget.setTextColorRole(
        fluentqt.Label.TextColorRole.Secondary if secondary
        else fluentqt.Label.TextColorRole.Primary
    )
    widget.setMinimumWidth(0)
    widget.setWordWrap(True)
    return widget


class SettingsPage(Surface):
    def __init__(self, parent=None):
        super().__init__("bgLayer", parent)
        self.setObjectName("settingsPage")
        self.setMinimumSize(640, 560)
        root = QHBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        self.sidebar = Surface("bgCanvas", self)
        self.sidebar.setObjectName("settingsSidebar")
        self.sidebar.setFixedWidth(SIDEBAR_WIDTH)
        sidebar_layout = QVBoxLayout(self.sidebar)
        sidebar_layout.setContentsMargins(20, 32, 20, 24)
        sidebar_layout.setSpacing(6)
        sidebar_layout.addWidget(label("设置", self.sidebar, role=fluentqt.FontRole.Subtitle))
        sidebar_layout.addSpacing(26)
        self.nav = []
        self.nav_labels = []
        for index, (title, identity) in enumerate((
            ("常规", "navGeneral"), ("外观", "navAppearance"), ("通知", "navNotifications")
        )):
            button = fluentqt.Button("", self.sidebar)
            button.setObjectName(identity)
            button.setAccessibleName(title)
            button.setFixedHeight(40)
            button.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
            content = QHBoxLayout(button)
            content.setContentsMargins(14, 0, 14, 0)
            text = label(title, button)
            text.setAttribute(Qt.WidgetAttribute.WA_TransparentForMouseEvents)
            content.addWidget(text)
            button.clicked.connect(lambda _checked=False, i=index: self.show_page(i))
            sidebar_layout.addWidget(button)
            self.nav.append(button)
            self.nav_labels.append(text)
        sidebar_layout.addStretch()
        sidebar_layout.addWidget(label("FluentQt", self.sidebar, role=fluentqt.FontRole.Caption, secondary=True))
        root.addWidget(self.sidebar)
        edge = Surface("strokeDivider", self)
        edge.setFixedWidth(1)
        root.addWidget(edge)

        content = QWidget(self)
        self.content_layout = QVBoxLayout(content)
        self.content_layout.setContentsMargins(CONTENT_INSET, 38, CONTENT_INSET, 28)
        self.content_layout.setSpacing(0)
        self.pages = QStackedWidget(content)
        self.pages.setObjectName("settingsPages")
        self.content_layout.addWidget(self.pages, 1)
        root.addWidget(content, 1)

        self.build_general()
        self.build_appearance()
        self.build_notifications()
        self.pages.currentChanged.connect(self.sync_navigation)
        self.show_page(0)

    def page(self, title, description):
        page = QWidget(self.pages)
        layout = QVBoxLayout(page)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        layout.addWidget(label(title, page, role=fluentqt.FontRole.Title))
        layout.addSpacing(8)
        layout.addWidget(label(description, page, secondary=True))
        layout.addSpacing(36)
        self.pages.addWidget(page)
        return page, layout

    def section(self, page, layout, title, *, emphasized=True):
        layout.addWidget(label(title, page, role=fluentqt.FontRole.BodyStrong if emphasized else fluentqt.FontRole.Body))
        layout.addSpacing(8)

    def row(self, page, layout, title, description, control, identity, *, style=None):
        row = QWidget(page)
        row.setObjectName(identity + "Row")
        row.setMinimumHeight(52 if style is not None else 76)
        line = QHBoxLayout(row)
        padding = style["row_padding"] if style is not None else ROW_GAP
        line.setContentsMargins(0, padding, 0, padding)
        line.setSpacing(24)
        copy = QVBoxLayout()
        copy.setSpacing(5)
        copy.addWidget(label(title, row, name=identity + "Title"))
        copy.addWidget(label(description, row, role=fluentqt.FontRole.Caption,
                             secondary=style is None or style["description_tone"] == "secondary",
                             name=identity + "Description"))
        line.addLayout(copy, 1)
        control.setParent(row)
        control.setObjectName(identity)
        control.setAccessibleName(title)
        line.addWidget(control, 0, Qt.AlignmentFlag.AlignVCenter)
        layout.addWidget(row)
        return row

    def divider(self, page, layout):
        divider = Surface("strokeDivider", page)
        divider.setFixedHeight(1)
        layout.addWidget(divider)

    def toggle(self, parent, checked):
        toggle = fluentqt.ToggleSwitch(parent)
        toggle.setIsOn(checked)
        toggle.setFixedSize(48, 32)
        return toggle

    def build_general(self):
        page, layout = self.page("常规", "让每次打开，都从熟悉的地方开始。")
        self.section(page, layout, "工作空间")
        self.name = fluentqt.LineEdit(page)
        self.name.setText("我的工作空间")
        self.name.setPlaceholderText("给工作空间起个名字")
        self.name.setMinimumWidth(160)
        self.name.setMaximumWidth(228)
        self.name.setClearButtonEnabled(True)
        row = self.row(page, layout, "名称", "为不同的工作环境取一个好记的名字。", self.name, "workspaceName",
                       style=PREVIEW_EDITS["workspace"])
        register_region(self, row, "workspace", "工作空间名称", ROW_FIELDS,
                        reveal=lambda: self.show_page(0))
        self.divider(page, layout)
        layout.addSpacing(28)
        self.section(page, layout, "启动与恢复")
        self.restore = self.toggle(page, True)
        self.row(page, layout, "恢复上次会话", "重新打开时，回到上次离开的位置。", self.restore, "restoreSession")
        self.divider(page, layout)
        self.updates = self.toggle(page, True)
        self.row(page, layout, "检查更新", "有新版本时，在应用内提醒我。", self.updates, "checkUpdates")
        layout.addStretch(1)
        self.reset_status = label("", page, role=fluentqt.FontRole.Caption, secondary=True, name="resetStatus")
        footer = QHBoxLayout()
        footer.setContentsMargins(0, 28, 0, 0)
        reset = fluentqt.Button("恢复默认", page)
        reset.setObjectName("resetSettings")
        reset.clicked.connect(self.reset_defaults)
        footer.addWidget(reset)
        footer.addSpacing(12)
        footer.addWidget(self.reset_status)
        footer.addStretch()
        layout.addLayout(footer)

    def build_appearance(self):
        page, layout = self.page("外观", "调整显示方式，让界面适合你的使用习惯。")
        self.section(page, layout, "主题")
        dark = fluentqt.theme_uses_dark_appearance(fluentqt.current_theme())
        self.dark = self.toggle(page, dark)
        self.dark.toggled.connect(lambda enabled: fluentqt.set_theme(
            fluentqt.Theme.Dark if enabled else fluentqt.Theme.Light))
        self.row(page, layout, "深色外观", "降低屏幕亮度，适合较暗的环境。", self.dark, "darkAppearance")
        self.divider(page, layout)
        layout.addSpacing(28)
        self.section(page, layout, "阅读")
        self.font_size = fluentqt.ComboBox(page)
        self.font_size.addItems(["标准", "较大"])
        self.font_size.setFixedWidth(152)
        self.font_size.setCurrentIndex(1 if fluentqt.font_scale() > 1.05 else 0)
        self.font_size.currentIndexChanged.connect(lambda index: fluentqt.set_font_scale(1.12 if index else 1.0))
        self.row(page, layout, "文字大小", "同时调整标题、说明和控件文字。", self.font_size, "fontSize")
        layout.addStretch(1)

    def build_notifications(self):
        page, layout = self.page("通知", "只接收你需要的提醒。")
        style = PREVIEW_EDITS["notifications"]
        group = QWidget(page)
        group.setObjectName("notificationGroup")
        group_layout = QVBoxLayout(group)
        group_layout.setContentsMargins(0, 0, 0, 0)
        group_layout.setSpacing(0)
        self.section(group, group_layout, "应用内通知", emphasized=style["heading_emphasis"])
        self.notifications = self.toggle(group, True)
        self.row(group, group_layout, "允许通知", "显示操作结果和需要关注的消息。", self.notifications,
                 "allowNotifications", style=style)
        self.divider(group, group_layout)
        self.sound = self.toggle(group, True)
        self.sound_row = self.row(group, group_layout, "提示音", "收到通知时播放简短的提示音。", self.sound,
                                 "notificationSound", style=style)
        self.divider(group, group_layout)
        layout.addWidget(group)
        register_region(self, group, "notifications", "应用内通知", (
            ROW_FIELDS[0], Field("heading_emphasis", "标题强调", "bool"), ROW_FIELDS[1],
        ), reveal=lambda: self.show_page(2))
        layout.addSpacing(26)
        preview_line = QHBoxLayout()
        self.preview_button = fluentqt.Button("发送测试通知", page)
        self.preview_button.setObjectName("testNotification")
        self.preview_button.clicked.connect(self.test_notification)
        preview_line.addWidget(self.preview_button)
        preview_line.addStretch()
        layout.addLayout(preview_line)
        layout.addSpacing(16)
        self.notification_preview = fluentqt.InfoBar(page)
        self.notification_preview.setObjectName("notificationPreview")
        self.notification_preview.setSeverity(fluentqt.InfoBar.InfoBarSeverity.Success)
        self.notification_preview.setTitle("通知显示正常")
        self.notification_preview.setMessage("你会在这里收到工作空间的提醒。")
        self.notification_preview.setIsClosable(True)
        self.notification_preview.setIsOpen(False)
        layout.addWidget(self.notification_preview)
        self.notifications.toggled.connect(self.sync_notifications)
        layout.addStretch(1)

    def sync_notifications(self, enabled):
        self.sound_row.setEnabled(enabled)
        self.preview_button.setEnabled(enabled)
        if not enabled:
            self.notification_preview.setIsOpen(False)

    def test_notification(self):
        self.notification_preview.setIsOpen(True)
        if self.sound.isOn():
            from PySide6.QtWidgets import QApplication
            QApplication.beep()

    def sync_navigation(self, index):
        for item, button in enumerate(self.nav):
            button.setFluentStyle(fluentqt.Button.ButtonStyle.Standard if item == index
                                  else fluentqt.Button.ButtonStyle.Subtle)
            self.nav_labels[item].setFluentTypography(
                fluentqt.FontRole.BodyStrong if item == index else fluentqt.FontRole.Body)

    def show_page(self, index):
        self.pages.setCurrentIndex(index)
        self.sync_navigation(index)

    def reset_defaults(self):
        self.name.setText("我的工作空间")
        for toggle in (self.restore, self.updates, self.notifications, self.sound):
            toggle.setIsOn(True)
        self.dark.setIsOn(False)
        self.font_size.setCurrentIndex(0)
        self.reset_status.setText("已恢复默认设置")

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if hasattr(self, "content_layout"):
            narrow = self.width() < 780
            self.sidebar.setFixedWidth(144 if narrow else SIDEBAR_WIDTH)
            inset = 24 if narrow else CONTENT_INSET
            self.content_layout.setContentsMargins(inset, 38, inset, 28)
            self.name.setMaximumWidth(180 if narrow else 228)


def build(parent):
    return SettingsPage(parent)


def after_preview_state_restore(page):
    page.sync_navigation(page.pages.currentIndex())
    page.sync_notifications(page.notifications.isOn())
