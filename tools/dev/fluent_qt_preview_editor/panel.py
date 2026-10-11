"""Fluent controls for selection, edits, and review; no scene-specific logic."""

import json
from pathlib import Path

import fluentqt
from PySide6.QtCore import QSize, Qt
from PySide6.QtGui import QColor, QKeySequence, QPalette, QPixmap, QShortcut
from PySide6.QtWidgets import QApplication, QAbstractScrollArea, QHBoxLayout, QLabel, QSizePolicy, QVBoxLayout, QWidget


def text(value, parent, *, secondary=False, role=None):
    widget = fluentqt.Label(value, parent)
    widget.setWordWrap(True)
    widget.setMinimumWidth(0)
    widget.setFluentTypography(role or fluentqt.FontRole.Body)
    widget.setTextColorRole(fluentqt.Label.TextColorRole.Secondary if secondary
                            else fluentqt.Label.TextColorRole.Primary)
    return widget


class ToolSurface(fluentqt.FluentWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAutoFillBackground(True)
        self.on_theme_updated()

    def on_theme_updated(self):
        colors = self.theme_tokens().colors
        palette = self.palette()
        palette.setColor(QPalette.ColorRole.Window, QColor(colors.bgCanvas))
        palette.setColor(QPalette.ColorRole.WindowText, QColor(colors.textPrimary))
        self.setPalette(palette)


class Snapshot(QLabel):
    """Aspect-correct image only; all visible text uses Fluent labels."""

    def __init__(self, path, canvas_size, parent):
        super().__init__(parent)
        self.original = QPixmap(path)
        self.canvas_size = canvas_size
        self.setAccessibleName("界面实际渲染截图")
        self.setAlignment(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop)
        self.setMinimumSize(160, 120)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)

    def resizeEvent(self, event):
        super().resizeEvent(event)
        scale = min(self.width() / self.canvas_size.width(), self.height() / self.canvas_size.height())
        ratio = self.devicePixelRatioF()
        target = QSize(round(self.original.width() * scale * ratio), round(self.original.height() * scale * ratio))
        pixmap = self.original.scaled(target, Qt.AspectRatioMode.KeepAspectRatio,
                                      Qt.TransformationMode.SmoothTransformation)
        pixmap.setDevicePixelRatio(ratio)
        self.setPixmap(pixmap)


class ComparisonWindow(fluentqt.Window):
    def __init__(self, comparison, parent):
        super().__init__(parent)
        self.setWindowFlag(Qt.WindowType.Window, True)
        self.setCustomWindowChromeEnabled(False)
        self.setBackdropEffect(fluentqt.BackdropEffect.Solid)
        self.setWindowTitle("修改前后 · " + comparison["title"])
        self.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose)
        self.resize(1120, 500)
        self.setMinimumSize(700, 380)
        page = ToolSurface(self)
        layout = QVBoxLayout(page)
        layout.setContentsMargins(24, 24, 24, 24)
        layout.setSpacing(16)
        layout.addWidget(text(comparison["title"], page, role=fluentqt.FontRole.Subtitle))
        layout.addWidget(text("同一次调整前后的真实界面。布局和字号按相同比例显示。", page, secondary=True))
        columns = QHBoxLayout()
        columns.setSpacing(24)
        sizes = [QPixmap(comparison[key]).size() for key in ("before_image", "after_image")]
        canvas = QSize(max(size.width() for size in sizes), max(size.height() for size in sizes))
        if canvas.isEmpty():
            raise ValueError("The comparison snapshots are missing.")
        for caption, key in (("修改前", "before_image"), ("修改后", "after_image")):
            column = QVBoxLayout()
            column.setSpacing(12)
            column.addWidget(text(caption, page, role=fluentqt.FontRole.BodyStrong))
            column.addWidget(Snapshot(comparison[key], canvas, page), 1)
            columns.addLayout(column, 1)
        layout.addLayout(columns, 1)
        footer = QHBoxLayout()
        note = text("关闭对比后，可在右侧保留或撤销。", page, secondary=True,
                    role=fluentqt.FontRole.Caption)
        footer.addWidget(note, 1)
        copy = fluentqt.Button("复制代码差异", page)
        copy.clicked.connect(lambda: QApplication.clipboard().setText(comparison["diff"]))
        footer.addWidget(copy)
        layout.addLayout(footer)
        self.setContentWidget(page)


class EditorPanel(ToolSurface):
    def __init__(self, editor, picker, parent=None):
        super().__init__(parent)
        self.editor = editor
        self.picker = picker
        self.comparison_window = None
        self.snapshot = None
        self.draft = {}
        self.setObjectName("previewEditorPanel")
        self.setFixedWidth(288)
        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        scroll = fluentqt.ScrollView(self)
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QAbstractScrollArea.Shape.NoFrame)
        scroll.setHorizontalScrollBarVisibility(fluentqt.ScrollView.ScrollBarVisibility.Hidden)
        body = QWidget(scroll)
        layout = QVBoxLayout(body)
        layout.setContentsMargins(20, 28, 20, 24)
        layout.setSpacing(16)
        layout.addWidget(text("局部调整", self, role=fluentqt.FontRole.Subtitle))
        layout.addWidget(text("点选页面中的一组内容，调整后比较效果。", self, secondary=True))
        self.pick = fluentqt.Button("点选区域", self)
        self.pick.setObjectName("previewEditPick")
        self.pick.setCheckable(True)
        self.pick.setToolTip("点选模式会拦截所选区域的点击；Esc 取消。")
        self.pick.toggled.connect(picker.set_picking)
        picker.pickingChanged.connect(self.pick.setChecked)
        layout.addWidget(self.pick)
        self.region = fluentqt.ComboBox(self)
        self.region.setObjectName("previewEditRegion")
        self.region.setAccessibleName("选择可编辑区域")
        self.region.currentIndexChanged.connect(self.select_region)
        layout.addWidget(self.region)
        self.form = QVBoxLayout()
        self.form.setSpacing(20)
        layout.addLayout(self.form)
        self.apply_button = fluentqt.Button("预览修改", self)
        self.apply_button.setObjectName("previewEditApply")
        self.apply_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
        self.apply_button.clicked.connect(self.apply)
        layout.addWidget(self.apply_button)
        self.compare = fluentqt.Button("查看前后对比", self)
        self.compare.setObjectName("previewEditCompare")
        self.compare.clicked.connect(lambda: self.call(self.show_comparison))
        layout.addWidget(self.compare)
        actions = QHBoxLayout()
        actions.setSpacing(8)
        self.keep = fluentqt.Button("保留修改", self)
        self.keep.setObjectName("previewEditAccept")
        self.keep.clicked.connect(lambda: self.call(editor.accept))
        self.revert = fluentqt.Button("撤销试改", self)
        self.revert.setObjectName("previewEditRevert")
        self.revert.clicked.connect(lambda: self.call(editor.restore))
        actions.addWidget(self.keep, 1)
        actions.addWidget(self.revert, 1)
        layout.addLayout(actions)
        self.undo = fluentqt.Button("撤销上一次调整", self)
        self.undo.setObjectName("previewEditUndo")
        self.undo.clicked.connect(lambda: self.call(lambda: editor.restore(undo=True)))
        layout.addWidget(self.undo)
        self.dismiss = fluentqt.Button("结束对比，保留当前文件", self)
        self.dismiss.clicked.connect(lambda: self.call(lambda: editor.execute("editor_dismiss", {})))
        layout.addWidget(self.dismiss)
        self.status = text("", self, secondary=True, role=fluentqt.FontRole.Caption)
        self.status.setObjectName("previewEditStatus")
        layout.addWidget(self.status)
        layout.addStretch(1)
        self.source = text("", self, secondary=True, role=fluentqt.FontRole.Caption)
        self.source.setTextElideMode(Qt.TextElideMode.ElideMiddle)
        self.source.setWordWrap(False)
        layout.addWidget(self.source)
        copy = fluentqt.Button("复制选择信息给 AI", self)
        copy.setObjectName("previewEditCopyContext")
        copy.clicked.connect(lambda: self.call(self.copy_context))
        layout.addWidget(copy)
        scroll.setContentWidget(body)
        outer.addWidget(scroll)
        shortcut = QShortcut(QKeySequence("F8"), self)
        shortcut.activated.connect(lambda: picker.set_picking(not picker.picking))
        self._shortcut = shortcut
        editor.changed.connect(self.refresh)
        self.refresh()

    def call(self, callback):
        try:
            return callback()
        except (OSError, ValueError, RuntimeError, KeyError) as error:
            self.status.setText("未能完成调整。" + str(error))
            self.status.setAccessibleName(self.status.text())

    def select_region(self, index):
        key = self.region.itemData(index)
        if key:
            self.call(lambda: self.editor.select(key))

    def refresh(self):
        try:
            state = self.editor.inspect()
        except (OSError, ValueError, RuntimeError, KeyError) as error:
            self.status.setText("当前页面没有可用的编辑信息。" + str(error))
            self.apply_button.setEnabled(False)
            pending = bool(self.editor.history.pending)
            self.keep.setVisible(pending)
            self.revert.setVisible(pending)
            self.keep.setEnabled(False)
            self.revert.setEnabled(False)
            self.dismiss.setVisible(pending)
            return
        self.snapshot = state
        self.region.blockSignals(True)
        self.region.clear()
        self.region.addItem("选择区域…", "")
        for region in state["regions"]:
            self.region.addItem(region["title"], region["key"])
        self.region.setCurrentIndex(max(0, self.region.findData(state["selected"])))
        self.region.blockSignals(False)
        while self.form.count():
            item = self.form.takeAt(0)
            if item.widget():
                item.widget().hide()
                item.widget().deleteLater()
        self.draft = {}
        selected = next((region for region in state["regions"] if region["key"] == state["selected"]), None)
        pending = bool(state["pending"])
        if selected:
            for field in selected["fields"]:
                self.add_field(field, enabled=not pending)
            location = selected["fields"][0]["source"]
            self.source.setText(f"{Path(location['file']).name}:{location['line']}")
            self.source.setToolTip(location["file"] + ":" + str(location["line"]))
        else:
            self.source.setText("选中区域后，可查看对应的代码位置。")
        self.pick.setEnabled(not pending)
        self.region.setEnabled(not pending)
        if pending:
            self.picker.set_picking(False)
        self.keep.setVisible(pending)
        self.revert.setVisible(pending)
        self.keep.setEnabled(not state["conflict"])
        self.revert.setEnabled(not state["conflict"])
        self.dismiss.setVisible(state["conflict"])
        self.undo.setVisible(state["can_undo"])
        latest = self.editor.history.pending or self.editor.history.latest
        self.compare.setEnabled(bool(latest and (self.editor.history.directory / latest["id"] / "after-region.png").exists()))
        self.status.setText("文件已被其他编辑修改；结束对比后继续。" if state["conflict"] else state["message"])
        self.update_apply()

    def add_field(self, field, *, enabled):
        box = QWidget(self)
        box.setEnabled(enabled)
        layout = QVBoxLayout(box)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(8)
        line = QHBoxLayout()
        line.addWidget(text(field["label"], box), 1)
        value = field["value"]
        self.draft[field["key"]] = value
        if field["kind"] == "int":
            amount = text(str(value), box, secondary=True)
            line.addWidget(amount)
            control = fluentqt.Slider(Qt.Orientation.Horizontal, box)
            control.setRange(field["minimum"], field["maximum"])
            control.setSingleStep(field["step"])
            control.setPageStep(field["step"])
            control.setValue(value)
            control.valueChanged.connect(lambda number: amount.setText(str(number)))
            control.valueChanged.connect(lambda number, key=field["key"]: self.change(key, number))
            layout.addLayout(line)
            layout.addWidget(control)
        elif field["kind"] == "bool":
            control = fluentqt.ToggleSwitch(box)
            control.setIsOn(value)
            control.toggled.connect(lambda checked, key=field["key"]: self.change(key, checked))
            line.addWidget(control)
            layout.addLayout(line)
        else:
            control = fluentqt.ComboBox(box)
            for key, label in field["choices"]:
                control.addItem(label, key)
            control.setCurrentIndex(control.findData(value))
            control.currentIndexChanged.connect(lambda index, key=field["key"], combo=control:
                                                self.change(key, combo.itemData(index)))
            layout.addLayout(line)
            layout.addWidget(control)
        control.setObjectName("previewEditField_" + field["key"])
        control.setAccessibleName(field["label"])
        self.form.addWidget(box)

    def change(self, key, value):
        self.draft[key] = value
        self.update_apply()

    def update_apply(self):
        state = self.snapshot or {}
        selected = next((region for region in state.get("regions", [])
                         if region["key"] == state.get("selected")), None)
        changed = selected and any(self.draft.get(field["key"]) != field["value"] for field in selected["fields"])
        self.apply_button.setEnabled(bool(changed and state.get("synchronized") and not state.get("pending")))

    def apply(self):
        if self.snapshot:
            self.call(lambda: self.editor.apply(dict(self.draft), self.snapshot["revision"], self.snapshot["selected"]))

    def show_comparison(self):
        if self.comparison_window is not None:
            import shiboken6
            if shiboken6.isValid(self.comparison_window):
                self.comparison_window.close()
        self.comparison_window = ComparisonWindow(self.editor.comparison(), self.editor.window)
        self.comparison_window.show()

    def copy_context(self):
        state = self.editor.inspect()
        selection = next((region for region in state["regions"] if region["key"] == state["selected"]), None)
        if not selection:
            raise ValueError("请先选择一个区域。")
        QApplication.clipboard().setText(json.dumps({"region": selection, "revision": state["revision"],
                                                   "instruction": "Use preview_editor_apply with this region, revision and registered field changes."},
                                                  ensure_ascii=False, indent=2))
        self.status.setText("已复制所选区域、代码位置和可调参数。")
