"""Executable Python counterparts of the layout samples."""

from textwrap import dedent

from .native_samples import register_source_samples
from .native_source_files import complete_sample_source_files


def _script(body: str) -> str:
    return (
        "import fluentqt\n"
        "from PySide6.QtCore import QMarginsF, QPointF, Qt\n"
        "from PySide6.QtWidgets import QSizePolicy, QVBoxLayout, QWidget\n\n"
        + dedent(body).strip() + "\n"
    )


def connection_form_source_files(result):
    return complete_sample_source_files(result, "connection_form.py")


def file_browser_source_files(result):
    return complete_sample_source_files(result, "file_browser.py")


register_source_samples(
    "list-detail-view",
    ("ListDetailView", "ListView", "TextEdit"),
    {
        "list-detail-basic": (
            "panel",
            _script('''
                panel = QWidget()
                body = QVBoxLayout(panel)
                body.setContentsMargins(0, 0, 0, 0)
                body.setSpacing(12)
                width = fluentqt.ComboBox(panel)
                width.setObjectName("listDetailPreviewWidth")
                width.setAccessibleName("Preview width")
                width.addItems(["Wide preview", "Narrow preview"])
                body.addWidget(width, 0, Qt.AlignLeft)
                view = fluentqt.ListDetailView(panel)
                view.setObjectName("basicListDetailView")
                view.setCompactThreshold(520)
                view.setMaximumWidth(640)
                view.setFixedHeight(156)
                body.addWidget(view, 0, Qt.AlignLeft)
                list_layout = QVBoxLayout(view.listPane())
                list_layout.addWidget(fluentqt.Label("Notes", view.listPane()))
                open_button = fluentqt.Button("Open meeting note", view.listPane())
                list_layout.addWidget(open_button)
                list_layout.addStretch()
                detail_layout = QVBoxLayout(view.detailPane())
                back = fluentqt.Button("Back to notes", view.detailPane())
                back.setFluentStyle(fluentqt.Button.Subtle)
                back.setVisible(view.isCompact())
                detail_layout.addWidget(back, 0, Qt.AlignLeft)
                detail_layout.addWidget(fluentqt.Label("Meeting note", view.detailPane()))
                editor = fluentqt.LineEdit(view.detailPane())
                editor.setAccessibleName("Note draft")
                editor.setText("Discuss the next release")
                detail_layout.addWidget(editor)
                detail_layout.addStretch()
                open_button.clicked.connect(view.showDetail)
                back.clicked.connect(view.showList)
                view.compactChanged.connect(back.setVisible)
                width.currentIndexChanged.connect(
                    lambda index: view.setMaximumWidth(640 if index == 0 else 360))
            '''),
        ),
        "list-detail-file-browser": (
            "panel",
            _script(r'''
                from PySide6.QtCore import QMargins, QPersistentModelIndex, QSignalBlocker, QSize
                from PySide6.QtCore import QSortFilterProxyModel
                from PySide6.QtGui import QAction, QColor, QIcon, QKeySequence, QPixmap
                from PySide6.QtGui import QStandardItem, QStandardItemModel, QRegion
                from PySide6.QtWidgets import QHBoxLayout, QLayout


                class FileBrowserSample(fluentqt.FluentWidget):
                    Draft, Modified, Saved, Folder = range(int(Qt.UserRole), int(Qt.UserRole) + 4)

                    def __init__(self, parent=None):
                        super().__init__(parent)
                        self.model = None
                        self.current = QPersistentModelIndex()
                        self.filtering = False
                        self.new_count = 0
                        self.setObjectName("fileBrowserSample")
                        self.setMaximumWidth(960)
                        self.setMinimumHeight(644)
                        body = self.column(self)
                        body.setSpacing(fluentqt.Spacing.Medium)
                        body.setSizeConstraint(QLayout.SetNoConstraint)
                        self.surface = fluentqt.Card(self)
                        self.surface.setObjectName("fileBrowserSurface")
                        self.surface.setFixedHeight(600)
                        fluentqt.set_widget_theme_overrides(self.surface, {"radius": {"control": 8}})
                        surface = self.column(self.surface, 1)
                        surface.setSizeConstraint(QLayout.SetNoConstraint)
                        body.addWidget(self.surface)
                        toolbar = self.row(self.surface, 52, fluentqt.Spacing.Medium)
                        self.breadcrumb = QWidget(self.surface)
                        path = QHBoxLayout(self.breadcrumb)
                        path.setContentsMargins(0, 0, 0, 0)
                        path.setSpacing(fluentqt.Spacing.Medium)
                        path.addWidget(fluentqt.FontIcon("ic_fluent_folder_20_regular", self.breadcrumb))
                        path.addWidget(self.label("Workspace", secondary=True))
                        chevron = fluentqt.FontIcon("ic_fluent_chevron_right_20_regular", self.breadcrumb)
                        chevron.setIconSize(fluentqt.Typography.IconSize.Compact)
                        path.addWidget(chevron)
                        toolbar.addWidget(self.breadcrumb)
                        self.folder_title = self.label("Documents", fluentqt.FontRole.BodyStrong)
                        toolbar.addWidget(self.folder_title)
                        self.back = self.button("Files", "fileBrowserBack", fluentqt.Button.Subtle)
                        self.back.setIconGlyph(fluentqt.Typography.Icons.Back)
                        self.back.setAccessibleName("Back to files")
                        toolbar.addWidget(self.back)
                        toolbar.addStretch()
                        self.wide_search = self.search("fileBrowserSearch")
                        self.wide_search.setFixedWidth(204)
                        toolbar.addWidget(self.wide_search)
                        self.new = self.button("New file", "fileBrowserNew")
                        self.new.setIconGlyph(fluentqt.Typography.Icons.Add)
                        self.new.setAccessibleName("New file")
                        toolbar.addWidget(self.new)
                        self.more = self.button("", "fileBrowserMore", fluentqt.Button.Subtle)
                        self.more.setIconGlyph(fluentqt.Typography.Icons.More)
                        self.more.setAccessibleName("File options")
                        self.more.setToolTip(self.more.accessibleName())
                        toolbar.addWidget(self.more)
                        self.compact_save = self.button("Save", "fileBrowserCompactSave", fluentqt.Button.Accent)
                        toolbar.addWidget(self.compact_save)
                        surface.addWidget(toolbar.parentWidget())
                        surface.addWidget(fluentqt.Divider(self.surface))
                        self.view = fluentqt.ListDetailView(self.surface)
                        self.view.setObjectName("fileBrowserLayout")
                        self.view.setListPaneWidth(320)
                        surface.addWidget(self.view, 1)
                        listing = self.column(self.view.listPane())
                        search_row = self.row(self.view.listPane(), 56, fluentqt.Spacing.Medium)
                        self.search_row = search_row.parentWidget()
                        self.narrow_search = self.search("fileBrowserCompactSearch")
                        search_row.addWidget(self.narrow_search)
                        listing.addWidget(self.search_row)
                        headings = self.row(self.view.listPane(), 36, 0)
                        headings.setContentsMargins(48, 0, 12, 0)
                        headings.addWidget(self.label("Name", fluentqt.FontRole.Caption, True))
                        headings.addStretch()
                        headings.addWidget(self.label("Modified", fluentqt.FontRole.Caption, True))
                        listing.addWidget(headings.parentWidget())
                        self.files = fluentqt.ListView(self.view.listPane())
                        self.files.setObjectName("fileBrowserList")
                        self.files.setAccessibleName("Documents")
                        self.files.setBorderVisible(False)
                        self.files.setScrollChainingEnabled(False)
                        self.files.setTrailingTextRole(self.Modified)
                        self.files.setIconSize(QSize(20, 20))
                        self.files.setSpacing(0)
                        self.files.setPlaceholderText("No matching files")
                        listing.addWidget(self.files, 1)
                        listing.addWidget(fluentqt.Divider(self.view.listPane()))
                        footer = self.row(self.view.listPane(), 28, fluentqt.Spacing.Standard)
                        self.count = self.label("", fluentqt.FontRole.Caption, True)
                        footer.addWidget(self.count)
                        listing.addWidget(footer.parentWidget())
                        detail = self.column(self.view.detailPane())
                        heading = self.row(self.view.detailPane(), 64, fluentqt.Spacing.Large)
                        identity = QVBoxLayout()
                        identity.setSpacing(fluentqt.Spacing.XSmall)
                        identity.setAlignment(Qt.AlignVCenter)
                        self.title = self.label("", fluentqt.FontRole.BodyStrong)
                        self.title.setObjectName("fileBrowserTitle")
                        self.title.setMinimumWidth(0)
                        identity.addWidget(self.title)
                        state = QHBoxLayout()
                        state.setSpacing(fluentqt.Spacing.XSmall)
                        self.dirty = fluentqt.InfoBadge(self)
                        self.dirty.setDisplayMode(fluentqt.InfoBadge.InfoBadgeDisplayMode.Dot)
                        self.dirty.setStatus(fluentqt.InfoBadge.InfoBadgeStatus.Caution)
                        self.dirty.setBeaconDiameter(5)
                        self.dirty.setFixedSize(5, 5)
                        self.dirty.setAccessibleName("Unsaved changes")
                        state.addWidget(self.dirty)
                        self.status = self.label("", fluentqt.FontRole.Caption, True)
                        self.status.setObjectName("fileBrowserStatus")
                        state.addWidget(self.status)
                        state.addStretch()
                        identity.addLayout(state)
                        heading.addLayout(identity, 1)
                        self.save = self.button("Save", "fileBrowserSave", fluentqt.Button.Accent)
                        heading.addWidget(self.save)
                        detail.addWidget(heading.parentWidget())
                        detail.addWidget(fluentqt.Divider(self.view.detailPane()))
                        self.editor = fluentqt.TextEdit(self.view.detailPane())
                        self.editor.setObjectName("fileBrowserEditor")
                        self.editor.setAccessibleName("File contents")
                        self.editor.setAutoResizeEnabled(False)
                        self.editor.setFrameVisible(False)
                        self.editor.setContentMargins(QMargins(24, 24, 24, 24))
                        self.editor.setLineHeight(fluentqt.Spacing.ControlHeight.Small)
                        self.editor.setScrollChainingEnabled(False)
                        detail.addWidget(self.editor, 1)
                        detail.addWidget(fluentqt.Divider(self.view.detailPane()))
                        footer = self.row(self.view.detailPane(), 28, fluentqt.Spacing.Standard)
                        self.file_type = self.label("", fluentqt.FontRole.Caption, True)
                        self.position = self.label("", fluentqt.FontRole.Caption, True)
                        footer.addWidget(self.file_type)
                        footer.addStretch()
                        footer.addWidget(self.position)
                        detail.addWidget(footer.parentWidget())
                        narrow = self.button("Narrow preview", "fileBrowserNarrow", fluentqt.Button.Subtle)
                        narrow.setCheckable(True)
                        narrow.toggled.connect(self.preview_narrow)
                        body.addWidget(narrow, 0, Qt.AlignLeft)
                        self.document_icon = self.glyph_icon("ic_fluent_document_text_20_regular", 20)
                        self.folder_icon = self.glyph_icon("ic_fluent_folder_20_regular", 20)
                        self.model = QStandardItemModel(self)
                        names = ["Design assets", "getting-started.md", "layout-notes.md",
                                 "architecture.md", "keyboard.md", "roadmap.md", "changelog.md",
                                 "theme-tokens.json", "contributing.md", "license.txt",
                                 "release-notes.md", "readme.md"]
                        dates = ["Oct 18", "11:20", "10:42", "Yesterday", "Oct 18", "Oct 18",
                                 "Oct 17", "Oct 17", "Oct 16", "Oct 15", "Oct 15", "Oct 14"]
                        for index, (name, date) in enumerate(zip(names, dates)):
                            contents = self.layout_notes() if index == 2 else f"# {name}\n\nNotes for this workspace.\n"
                            self.add_file(name, date, contents, index == 0)
                        self.model.item(0).setData(
                            "Design assets\n\nFolder contents\n\napp-icon.svg\ncolor-tokens.json\nREADME.md", self.Draft)
                        self.model.item(2).setData(self.layout_notes().split("Next:")[0], self.Saved)
                        for index in range(1, 21):
                            self.add_file(f"meeting-notes-{index:02d}.md", "Oct 12",
                                          f"# Meeting notes {index}\n\nDecisions\n\n"
                                          + "Keep the next step small and reviewable.\n" * 40)
                        self.proxy = QSortFilterProxyModel(self)
                        self.proxy.setSourceModel(self.model)
                        self.proxy.setFilterCaseSensitivity(Qt.CaseInsensitive)
                        self.proxy.setFilterKeyColumn(0)
                        self.files.setModel(self.proxy)
                        self.files.selectionModel().currentChanged.connect(self.select_file)
                        self.files.clicked.connect(self.open_file)
                        self.files.activated.connect(self.open_file)
                        self.open_action = QAction(self.files)
                        self.open_action.setShortcuts([QKeySequence(Qt.Key_Return), QKeySequence(Qt.Key_Enter)])
                        self.open_action.setShortcutContext(Qt.WidgetShortcut)
                        self.files.addAction(self.open_action)
                        self.open_action.triggered.connect(self.open_file)
                        self.back.clicked.connect(self.show_files)
                        self.view.compactChanged.connect(self.update_chrome)
                        self.view.currentPaneChanged.connect(self.update_chrome)
                        self.wide_search.textChanged.connect(self.filter_files)
                        self.narrow_search.textChanged.connect(self.filter_files)
                        self.editor.textChanged.connect(self.update_draft)
                        self.editor.cursorPositionChanged.connect(self.update_position)
                        self.save_action = QAction(self)
                        self.save_action.setShortcut(QKeySequence.Save)
                        self.save_action.setShortcutContext(Qt.WidgetWithChildrenShortcut)
                        self.addAction(self.save_action)
                        self.save_action.triggered.connect(self.save_file)
                        self.save.clicked.connect(self.save_action.trigger)
                        self.compact_save.clicked.connect(self.save_action.trigger)
                        self.new.clicked.connect(self.new_file)
                        self.menu = fluentqt.FluentMenu("File options", self)
                        self.menu.addAction("Show current file").triggered.connect(self.reveal_current)
                        self.menu.addAction("Clear search").triggered.connect(self.wide_search.clear)
                        self.more.clicked.connect(lambda: self.menu.popup(
                            self.more.mapToGlobal(self.more.rect().bottomLeft())))
                        self.files.setCurrentIndex(self.proxy.index(2, 0))
                        self.view.showList()
                        self.on_theme_updated()
                        self.update_chrome()

                    @staticmethod
                    def column(parent, margin=0):
                        layout = QVBoxLayout(parent)
                        layout.setContentsMargins(margin, margin, margin, margin)
                        layout.setSpacing(0)
                        return layout

                    @staticmethod
                    def row(parent, height, inset):
                        widget = QWidget(parent)
                        widget.setFixedHeight(height)
                        layout = QHBoxLayout(widget)
                        layout.setContentsMargins(inset, 0, inset, 0)
                        layout.setSpacing(fluentqt.Spacing.Small)
                        return layout

                    def label(self, text, role=fluentqt.FontRole.Body, secondary=False):
                        label = fluentqt.Label(text, self)
                        label.setFluentTypography(role)
                        label.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
                        if secondary:
                            label.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
                        return label

                    def button(self, text, name, style=fluentqt.Button.Standard):
                        button = fluentqt.Button(text, self)
                        button.setObjectName(name)
                        button.setFluentStyle(style)
                        return button

                    def search(self, name):
                        field = fluentqt.LineEdit(self)
                        field.setObjectName(name)
                        field.setPlaceholderText("Search files…")
                        field.setAccessibleName("Search files")
                        icon = fluentqt.FontIcon("ic_fluent_search_20_regular", field)
                        icon.setIconSize(16)
                        icon.setFixedSize(16, 16)
                        icon.setAttribute(Qt.WA_TransparentForMouseEvents)
                        layout = QHBoxLayout(field)
                        layout.setContentsMargins(fluentqt.Spacing.Small, 0, fluentqt.Spacing.Small, 0)
                        layout.addWidget(icon, 0, Qt.AlignVCenter)
                        layout.addStretch()
                        margins = field.contentMargins()
                        margins.setLeft(32)
                        field.setContentMargins(margins)
                        return field

                    def glyph_icon(self, glyph, size):
                        icon = fluentqt.FontIcon(glyph, self)
                        icon.hide()
                        icon.setIconSize(size)
                        icon.resize(size, size)
                        dpr = self.devicePixelRatioF()
                        pixmap = QPixmap(round(size * dpr), round(size * dpr))
                        pixmap.setDevicePixelRatio(dpr)
                        pixmap.fill(Qt.transparent)
                        icon.render(pixmap, QPointF(0, 0).toPoint(), QRegion(), QWidget.DrawChildren)
                        icon.deleteLater()
                        return QIcon(pixmap)

                    def on_theme_updated(self):
                        if getattr(self, "model", None) is None:
                            return
                        colors = self.theme_tokens().colors
                        self.document_icon = self.glyph_icon("ic_fluent_document_text_20_regular", 20)
                        self.folder_icon = self.glyph_icon("ic_fluent_folder_20_regular", 20)
                        for row in range(self.model.rowCount()):
                            item = self.model.item(row)
                            item.setIcon(self.folder_icon if item.data(self.Folder) else self.document_icon)
                        secondary = QColor(colors.textSecondary)
                        search_text = (f"#{secondary.red():02x}{secondary.green():02x}"
                                       f"{secondary.blue():02x}{secondary.alpha():02x}")
                        mode = {fluentqt.Theme.Dark: "dark", fluentqt.Theme.HighContrast: "contrast"}.get(
                            self.effective_theme(), "light")
                        search_theme = {mode: {"textTertiary": search_text}}
                        fluentqt.set_widget_theme_overrides(self.wide_search, search_theme)
                        fluentqt.set_widget_theme_overrides(self.narrow_search, search_theme)
                        self.update()

                    def add_file(self, name, modified, contents, folder=False):
                        item = QStandardItem(name)
                        item.setEditable(False)
                        item.setSizeHint(QSize(0, fluentqt.Spacing.ControlHeight.Large))
                        for role, value in [(self.Draft, contents), (self.Saved, contents),
                                            (self.Modified, modified), (self.Folder, folder)]:
                            item.setData(value, role)
                        item.setData(f"{name}, modified {modified}", Qt.AccessibleTextRole)
                        item.setIcon(self.folder_icon if folder else self.document_icon)
                        self.model.appendRow(item)

                    def select_file(self, index):
                        if self.filtering or not index.isValid():
                            return
                        source = self.proxy.mapToSource(index)
                        if source == self.current:
                            return
                        self.current = QPersistentModelIndex(source)
                        self.title.setText(index.data())
                        with QSignalBlocker(self.editor):
                            self.editor.setPlainText(source.data(self.Draft))
                            self.editor.setReadOnly(bool(source.data(self.Folder)))
                        self.update_state()
                        self.update_position()
                        if not self.view.isCompact():
                            self.view.showDetail()

                    def open_file(self):
                        if self.files.currentIndex().isValid():
                            self.view.showDetail()
                            self.editor.setFocus()

                    def preview_narrow(self, checked):
                        if checked:
                            self.preview_width = self.width()
                        self.setMaximumWidth(400 if checked else 960)
                        self.resize(400 if checked else self.preview_width, self.height())

                    def show_files(self):
                        self.view.showList()
                        self.files.setFocus()

                    def filter_files(self, text):
                        with QSignalBlocker(self.wide_search), QSignalBlocker(self.narrow_search):
                            if self.wide_search.text() != text:
                                self.wide_search.setText(text)
                            if self.narrow_search.text() != text:
                                self.narrow_search.setText(text)
                        self.filtering = True
                        self.proxy.setFilterFixedString(text)
                        self.files.setCurrentIndex(self.proxy.mapFromSource(self.current))
                        self.filtering = False
                        self.update_state()

                    def update_draft(self):
                        if self.current.isValid():
                            self.model.setData(self.current, self.editor.toPlainText(), self.Draft)
                            self.update_state()
                            self.view.showDetail()

                    def save_file(self):
                        if not self.current.isValid() or self.current.data(self.Folder):
                            return
                        self.model.setData(self.current, self.current.data(self.Draft), self.Saved)
                        self.model.setData(self.current, "Just now", self.Modified)
                        self.model.setData(self.current, f"{self.current.data()}, modified Just now",
                                           Qt.AccessibleTextRole)
                        self.update_state()

                    def new_file(self):
                        self.filter_files("")
                        self.new_count += 1
                        self.add_file(f"untitled-{self.new_count}.md", "Just now", "# Untitled\n\n")
                        row = self.model.rowCount() - 1
                        self.model.item(row).setData("", self.Saved)
                        self.files.setCurrentIndex(self.proxy.mapFromSource(self.model.index(row, 0)))
                        self.open_file()

                    def reveal_current(self):
                        self.filter_files("")
                        self.files.scrollTo(self.files.currentIndex())

                    def update_state(self):
                        folder = bool(self.current.data(self.Folder))
                        dirty = not folder and self.current.data(self.Draft) != self.current.data(self.Saved)
                        self.dirty.setVisible(dirty)
                        self.status.setText("Folder · 3 items" if folder else
                                            "Unsaved changes" if dirty else "Saved in memory")
                        self.save.setEnabled(dirty)
                        self.compact_save.setEnabled(dirty)
                        name = self.current.data() or ""
                        kind = "Markdown" if name.endswith(".md") else "Plain text"
                        self.file_type.setText("Folder" if folder else f"{kind} · UTF-8")
                        selected = " · 1 selected" if self.files.currentIndex().isValid() else ""
                        self.count.setText(f"{self.proxy.rowCount()} items{selected}")

                    def update_position(self):
                        cursor = self.editor.textCursor()
                        self.position.setText(f"Ln {cursor.blockNumber() + 1}, Col {cursor.positionInBlock() + 1}")

                    def update_chrome(self):
                        compact = self.view.isCompact()
                        detail = compact and self.view.currentPane() == fluentqt.ListDetailView.Pane.Detail
                        self.breadcrumb.setVisible(not compact)
                        self.folder_title.setVisible(not detail)
                        self.back.setVisible(detail)
                        self.wide_search.setVisible(not compact)
                        self.new.setVisible(not detail)
                        self.new.setText("" if compact else "New file")
                        self.more.setVisible(compact and not detail)
                        self.compact_save.setVisible(detail)
                        self.search_row.setVisible(compact)
                        self.save.setVisible(not compact)

                    @staticmethod
                    def layout_notes():
                        return (
                            "# Layout notes\n\nKeep the browser quiet. Let the document lead.\n\n"
                            "## Wide windows\n- List on the left, content on the right.\n"
                            "- One divider. No cards inside cards.\n\n## Small windows\n"
                            "- Open a file to focus on its contents.\n"
                            "- Return to the same place in the list.\n\nNext: refine the empty state."
                        )


                panel = FileBrowserSample()
            '''),
        ),
    },
)


register_source_samples(
    "field",
    ("Field", "FormController"),
    {
        "field-connection-form": (
            "panel",
            _script('''
                from PySide6.QtCore import QMargins, QTimer, QUrl
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
                        self.form.submissionFinished.connect(lambda success:
                            fluentqt.Toast.showOrUpdateToast(
                                self, "gallery.action-feedback", "Connection saved for this demo",
                                fluentqt.Toast.Success, 2200, fluentqt.Toast.Top,
                                QMargins(16, 50, 16, 16),
                            ) if success else None)
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
