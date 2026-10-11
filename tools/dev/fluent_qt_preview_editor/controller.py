"""Connect source transactions to a borrowed Live Scene host."""

from dataclasses import asdict
from copy import deepcopy
import difflib
import time

import shiboken6
from PySide6.QtCore import QAbstractAnimation, QObject, QPoint, QEventLoop, QTimer, Signal
from PySide6.QtWidgets import QApplication

from .history import EditHistory
from .registry import regions_for
from .source import SourceDocument, digest, replace_source


class PreviewEditor(QObject):
    changed = Signal()

    def __init__(self, window, history_directory):
        super().__init__(window)
        self.window = window
        self.history = EditHistory(window.source_path, history_directory)
        self.selected = ""
        self.busy = False
        self.picker = None
        self.message = "选择一个区域，开始局部调整。"
        window.reloadFinished.connect(self.rebind)

    def regions(self):
        root = self.window.scene_widget
        return {key: region for key, region in regions_for(root).items()
                if shiboken6.isValid(region.widget)
                and (root is region.widget or root.isAncestorOf(region.widget))}

    def rebind(self, success, _generation):
        if not success:
            if not self.busy:
                self.changed.emit()
            return
        if self.selected not in self.regions():
            self.selected = ""
        if not self.busy:
            self.changed.emit()

    def inspect(self):
        document = SourceDocument(self.window.source_path)
        loaded = getattr(self.window.scene_widget, "_fluentqt_live_scene_namespace", {}).get("PREVIEW_EDITS")
        result = []
        for region in self.regions().values():
            fields = []
            for field in region.fields:
                value = document.value(region.key, field.key)
                field.validate(value)
                fields.append({**asdict(field), "value": value,
                               "source": document.location(region.key, field.key)})
            position = region.widget.mapTo(self.window.scene_widget, QPoint())
            result.append({"key": region.key, "title": region.title,
                           "object_name": region.widget.objectName(), "visible": region.widget.isVisible(),
                           "rect": {"x": position.x(), "y": position.y(), "width": region.widget.width(),
                                    "height": region.widget.height()}, "fields": fields})
        current_values = {region: {key: document.value(region, key) for key in values}
                          for region, values in document.regions.items()}
        pending = self.history.pending
        conflict = bool(pending and document.revision != pending["after_revision"])
        loaded_revision = getattr(self.window.scene_widget, "_fluentqt_live_scene_revision", None)
        return {"source": str(document.path), "revision": document.revision,
                "synchronized": loaded_revision == document.revision and loaded == current_values,
                "selected": self.selected, "regions": result,
                "pending": deepcopy(pending), "conflict": conflict, "can_undo": bool(self.history.undoable and not pending),
                "message": self.message, "history_directory": str(self.history.directory)}

    def select(self, key):
        if self.history.pending:
            raise RuntimeError("Keep or revert the current edit before changing selection.")
        region = self.regions().get(key)
        if not region:
            raise ValueError("Unknown editable region. Inspect the editor again.")
        if region.reveal:
            region.reveal()
        if not region.widget.isVisible():
            raise ValueError("The selected region is not visible.")
        self.selected = key
        self.message = "已选中“" + region.title + "”。"
        self.changed.emit()
        return self.inspect()

    def capture(self, region_key):
        # Capturing the first paint after reload can record switch knobs at
        # their animation start even though their logical state is already on.
        deadline = time.monotonic() + 1.5
        while True:
            QApplication.processEvents(QEventLoop.ProcessEventsFlag.ExcludeUserInputEvents)
            animations = self.window.scene_widget.findChildren(QAbstractAnimation)
            active = [animation for animation in animations
                      if animation.state() == QAbstractAnimation.State.Running and animation.loopCount() != -1]
            if not active:
                break
            if time.monotonic() >= deadline:
                raise RuntimeError("The scene animation did not settle. Pause it and try again.")
            loop = QEventLoop()
            QTimer.singleShot(16, loop.quit)
            loop.exec(QEventLoop.ProcessEventsFlag.ExcludeUserInputEvents)
        if self.picker:
            self.picker.hide_highlight()
        try:
            region = self.regions()[region_key]
            return self.window.scene_widget.grab(), region.widget.grab()
        finally:
            if self.picker:
                self.picker.refresh()

    def save_capture(self, entry, version, pixmaps):
        folder = self.history.directory / entry["id"]
        for pixmap, suffix in zip(pixmaps, ("", "-region")):
            if pixmap.isNull() or not pixmap.save(str(folder / (version + suffix + ".png"))):
                raise RuntimeError("Could not capture the preview comparison.")

    def apply(self, changes, revision, region=None):
        if self.busy:
            raise RuntimeError("An edit is already running.")
        if region is not None and region != self.selected:
            raise ValueError("The selected region changed. Select and inspect it before editing.")
        selection = self.regions().get(self.selected)
        if not selection or not selection.widget.isVisible():
            raise ValueError("Select a visible editable region first.")
        if not self.inspect()["synchronized"]:
            raise RuntimeError("The scene has not loaded the current source. Reload before editing.")
        document = SourceDocument(self.window.source_path)
        replacement = document.patch(selection.key, changes, selection.fields, revision)
        self.busy = True
        entry = None
        try:
            before = self.capture(selection.key)
            entry = self.history.begin(document.content, replacement, region=selection.key,
                                       title=selection.title, changes=changes)
            self.save_capture(entry, "before", before)
            if not self.window.reload_scene():
                raise RuntimeError("The edit could not load: " + self.window.last_error)
            self.save_capture(entry, "after", self.capture(selection.key))
            self.message = "正在预览修改。比较后，选择保留或撤销。"
        except BaseException:
            if entry and self.history.pending:
                self.history.restore()
                self.window.reload_scene()
            raise
        finally:
            self.busy = False
            self.changed.emit()
        return self.inspect()

    def accept(self):
        self.history.accept()
        self.message = "已保留修改。仍可撤销上一次调整。"
        self.changed.emit()
        return self.inspect()

    def restore(self, *, undo=False):
        if self.busy:
            raise RuntimeError("An edit is already running.")
        previous = self.history.undoable if undo else self.history.pending
        old_status = previous["status"] if previous else None
        self.busy = True
        try:
            entry = self.history.restore(undo=undo)
            if not self.window.reload_scene():
                replace_source(self.window.source_path, self.history.contents(entry, "before"),
                               self.history.contents(entry, "after"))
                entry["status"] = old_status
                self.history.save()
                raise RuntimeError("The previous version could not load; the current edit was retained.")
            self.selected = entry["region"] if entry["region"] in self.regions() else ""
            self.message = "已恢复修改前的布局，交互状态继续保留。"
        finally:
            self.busy = False
            self.changed.emit()
        return self.inspect()

    def comparison(self):
        entry = self.history.pending or self.history.latest
        if not entry:
            raise ValueError("Apply an edit before opening a comparison.")
        before = self.history.contents(entry, "before").decode("utf-8")
        after = self.history.contents(entry, "after").decode("utf-8")
        folder = self.history.directory / entry["id"]
        return {**deepcopy(entry), "before_image": str(folder / "before-region.png"),
                "after_image": str(folder / "after-region.png"),
                "before_page": str(folder / "before.png"), "after_page": str(folder / "after.png"),
                "diff": "".join(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                                                   fromfile="before.py", tofile="after.py"))}

    def execute(self, method, parameters):
        if method == "editor_inspect":
            return self.inspect()
        if method == "editor_select":
            return self.select(parameters.get("region"))
        if method == "editor_apply":
            return self.apply(parameters.get("changes"), parameters.get("revision"), parameters.get("region"))
        if method == "editor_accept":
            return self.accept()
        if method in ("editor_revert", "editor_undo"):
            return self.restore(undo=method == "editor_undo")
        if method == "editor_compare":
            return self.comparison()
        if method == "editor_dismiss":
            self.history.dismiss()
            self.message = "已结束对比，文件内容保持当前版本。"
            self.changed.emit()
            return {"dismissed": True, "source": str(self.window.source_path)}
        raise ValueError("Unknown editor command.")
