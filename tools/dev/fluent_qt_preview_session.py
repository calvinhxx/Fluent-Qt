"""Opt-in local command session for the Python Live Scene host."""

from __future__ import annotations

import base64
import enum
import json
import math
import os
from pathlib import Path
import uuid

import fluentqt
import shiboken6
from PySide6.QtCore import QBuffer, QCoreApplication, QIODevice, QLockFile, QObject, QPoint, QPointF, QTimer, Qt
from PySide6.QtGui import QAccessible, QKeyEvent, QKeySequence, QMouseEvent
from PySide6.QtCore import QEvent
from PySide6.QtWidgets import QApplication, QLineEdit, QWidget

from fluent_qt_preview_client import write_json


DATA_PROPERTIES = (
    "text", "plainText", "placeholderText", "checked", "checkState", "isOn",
    "value", "currentIndex", "selectedIndex", "selectedPageIndex", "enabled",
    "visible", "readOnly", "indeterminate", "isIndeterminate",
)


def alive(widget):
    return widget is not None and shiboken6.isValid(widget)


def enum_integer(value):
    """Accept both the Qt 6.2 integer enums and newer Python enum wrappers."""
    return int(getattr(value, "value", value))


def key_combination(sequence):
    combination = sequence[0]
    if hasattr(combination, "key"):
        return combination.key(), combination.keyboardModifiers()
    # PySide 6.2 exposes QKeySequence entries as combined integer flags.
    combined = int(combination)
    mask = enum_integer(Qt.KeyboardModifier.KeyboardModifierMask)
    return combined & ~mask, Qt.KeyboardModifier(combined & mask)


class PythonPreviewSession(QObject):
    """Borrow the scene and expose the shared UI command protocol."""

    def __init__(self, window, directory: Path):
        super().__init__()
        self.window = window
        self.directory = Path(directory).expanduser().resolve()
        for name in ("requests", "responses"):
            (self.directory / name).mkdir(parents=True, exist_ok=True)
        self.lock = QLockFile(str(self.directory / "host.lock"))
        if not self.lock.tryLock(0):
            raise RuntimeError("Another preview owns this session directory.")
        self.widgets = {}
        self.originals = {}
        self.session_id = ""
        self.busy = False
        self.timer = QTimer(self)
        self.timer.setInterval(80)
        self.timer.timeout.connect(self.poll)
        window.reloadFinished.connect(self.rebind)
        window.destroyed.connect(self.close)
        self.rebind(True, window.generation)
        self.timer.start()

    def rebind(self, success, _generation):
        if not success:
            if not _generation:
                write_json(self.directory / "session.json", {
                    "session_id": self.session_id, "status": "error", "runtime": "python",
                    "generation": 0, "pid": os.getpid(),
                })
            return
        self.session_id = uuid.uuid4().hex
        self.widgets.clear()
        self.originals.clear()
        write_json(self.directory / "session.json", {
            "schema_version": 1, "status": "ready" if _generation else "loading", "session_id": self.session_id,
            "generation": _generation,
            "runtime": "python", "pid": os.getpid(),
        })

    def close(self):
        self.timer.stop()
        if self.lock.isLocked():
            write_json(self.directory / "session.json", {
                "session_id": self.session_id, "status": "closed",
            })
            self.lock.unlock()

    def root(self):
        widget = self.window.scene_widget if alive(self.window) else None
        if not alive(widget):
            raise RuntimeError("The preview scene was destroyed.")
        return widget

    def refresh(self):
        root = self.root()
        widgets = [root, *root.findChildren(QWidget)]
        self.widgets = {key: widget for key, widget in self.widgets.items() if alive(widget)}
        self.originals = {widget: fields for widget, fields in self.originals.items() if alive(widget)}
        for widget in widgets:
            if widget not in self.widgets.values():
                self.widgets[self.session_id + ":" + uuid.uuid4().hex] = widget
        return widgets

    def resolve(self, selector):
        root = self.root()
        if selector == "@root":
            return root
        widget = self.widgets.get(selector)
        if alive(widget) and (widget == root or root.isAncestorOf(widget)):
            return widget
        matches = [w for w in self.refresh() if selector and w.objectName() == selector]
        if len(matches) != 1:
            raise ValueError("Unknown, stale or ambiguous target. Inspect the preview again.")
        return matches[0]

    def data(self, widget):
        fields = {}
        for name in DATA_PROPERTIES:
            if name == "text" and isinstance(widget, QLineEdit) and widget.echoMode() != QLineEdit.EchoMode.Normal:
                continue
            index = widget.metaObject().indexOfProperty(name)
            if index < 0:
                continue
            prop = widget.metaObject().property(index)
            if not prop.isReadable() or not prop.isWritable():
                continue
            value = prop.read(widget)
            if isinstance(value, enum.Enum):
                value = value.value
            if not isinstance(value, (str, int, float, bool)):
                continue
            fields[name] = {"value": value, "type": prop.typeName()}
            if prop.isEnumType():
                choices = prop.enumerator()
                fields[name]["choices"] = {choices.key(i): choices.value(i) for i in range(choices.keyCount())}
        return fields

    def inspect(self):
        widgets = self.refresh()
        ids = {w: key for key, w in self.widgets.items()}
        nodes = []
        for widget in widgets[:2000]:
            position = widget.mapTo(self.root(), QPoint())
            accessible = QAccessible.queryAccessibleInterface(widget)
            nodes.append({
                "id": ids[widget], "parent_id": ids.get(widget.parentWidget(), ""),
                "object_name": widget.objectName(), "class": widget.metaObject().className(),
                "accessible_name": accessible.text(QAccessible.Text.Name) if accessible else widget.accessibleName(),
                "accessible_role": enum_integer(accessible.role()) if accessible else 0,
                "visible": widget.isVisible(), "enabled": widget.isEnabled(),
                "rect": {"x": position.x(), "y": position.y(), "width": widget.width(), "height": widget.height()},
                "data": self.data(widget),
            })
        return {"session_id": self.session_id, "nodes": nodes, "truncated": len(widgets) > 2000,
                "inspector": fluentqt.inspect_widget(self.root())}

    def set_data(self, widget, name, value, *, remember_original=True):
        fields = self.data(widget)
        if name not in fields:
            raise ValueError("Property is not exposed as preview data.")
        old = fields[name]["value"]
        valid = (isinstance(old, bool) and isinstance(value, bool)) or (
            isinstance(old, str) and isinstance(value, str) and len(value) <= 32768
        ) or (type(old) in (int, float) and type(value) in (int, float) and math.isfinite(value))
        if not valid:
            raise ValueError("Value does not match the property's data type or size.")
        if type(old) is int and (value != int(value) or not -2147483648 <= value <= 2147483647):
            raise ValueError("Use an integer in the supported range.")
        if "choices" in fields[name] and value not in fields[name]["choices"].values():
            raise ValueError("Value is not a declared enumeration member.")
        if remember_original:
            self.originals.setdefault(widget, {}).setdefault(name, old)
        prop = widget.metaObject().property(widget.metaObject().indexOfProperty(name))
        if not prop.write(widget, value):
            raise ValueError("Qt rejected the data value.")
        return {"applied": True, "data": self.data(widget) if alive(widget) else {},
                "target_removed": not alive(widget)}

    def reset_data(self):
        originals = [(widget, dict(values), next(
            (key for key, item in self.widgets.items() if item is widget), "removed"))
            for widget, values in self.originals.items()]
        write_errors = {}

        def in_preview(widget):
            return alive(widget) and alive(self.window) and alive(self.window.scene_widget) and (
                widget is self.window.scene_widget or self.window.scene_widget.isAncestorOf(widget))

        for widget, values, target in originals:
            for name, value in values.items():
                if not in_preview(widget):
                    break
                try:
                    self.set_data(widget, name, value, remember_original=False)
                except (ValueError, RuntimeError, TypeError) as error:
                    write_errors[(target, name)] = str(error)

        # Verify together, including changes triggered by other setters.
        restored, errors = 0, []
        for widget, values, target in originals:
            current = self.data(widget) if in_preview(widget) else {}
            for name, value in values.items():
                if name in current and current[name]["value"] == value:
                    self.originals.get(widget, {}).pop(name, None)
                    restored += 1
                else:
                    errors.append({"target": target, "property": name, "message":
                        write_errors.get((target, name), "The target was removed, left the preview, or did not retain the original value.")})
            if not alive(widget) or not self.originals.get(widget):
                self.originals.pop(widget, None)
        result = {"restored": restored, "failed": len(errors),
                  "pending": sum(len(values) for values in self.originals.values()), "errors": errors}
        if errors:
            result["error"] = "Some values could not be restored. Retained values can be retried with reset_data."
        return result

    def action(self, widget, parameters):
        root = self.root()
        session_id = self.session_id

        def check_target():
            if not alive(widget) or not alive(root) or self.session_id != session_id:
                raise RuntimeError("Target or preview was removed or reloaded by the action.")
            if widget is not root and not root.isAncestorOf(widget):
                raise RuntimeError("Target left the preview during the action.")

        def settle():
            QCoreApplication.sendPostedEvents(None, QEvent.Type.LayoutRequest)
            QApplication.processEvents()
            QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
            check_target()

        if not widget.isVisible() or not widget.isEnabled():
            raise ValueError("Control must be visible and enabled.")
        action = parameters.get("action")
        if action not in ("click", "focus", "type_text", "key"):
            raise ValueError("Supported actions: click, focus, type_text, key.")
        widget.setFocus(Qt.FocusReason.OtherFocusReason)
        check_target()
        if action == "click":
            point = QPointF(widget.rect().center())
            global_point = QPointF(widget.mapToGlobal(point.toPoint()))
            for kind, buttons in ((QEvent.Type.MouseButtonPress, Qt.MouseButton.LeftButton),
                                  (QEvent.Type.MouseButtonRelease, Qt.MouseButton.NoButton)):
                QApplication.sendEvent(widget, QMouseEvent(kind, point, global_point,
                    Qt.MouseButton.LeftButton, buttons, Qt.KeyboardModifier.NoModifier))
                check_target()
                if kind == QEvent.Type.MouseButtonPress:
                    settle()
        elif action in ("type_text", "key"):
            if action == "type_text":
                text = parameters.get("text")
                if not isinstance(text, str) or not 0 < len(text) <= 32768:
                    raise ValueError("Text must contain 1 to 32768 characters.")
                keys = [(0, Qt.KeyboardModifier.NoModifier, char) for char in text]
            else:
                names = {"shift": Qt.KeyboardModifier.ShiftModifier, "control": Qt.KeyboardModifier.ControlModifier,
                         "alt": Qt.KeyboardModifier.AltModifier, "meta": Qt.KeyboardModifier.MetaModifier,
                         # Qt maps ControlModifier to Command on macOS.
                         "shortcut": Qt.KeyboardModifier.ControlModifier}
                modifiers = Qt.KeyboardModifier.NoModifier
                for name in parameters.get("modifiers", []):
                    if name not in names:
                        raise ValueError("Unknown key modifier.")
                    modifiers |= names[name]
                name = parameters.get("key", "")
                if not isinstance(name, str):
                    raise ValueError("Key must be a string.")
                aliases = {"enter": "Return", "escape": "Esc", "backspace": "Backspace", "tab": "Tab", "space": "Space"}
                sequence = QKeySequence.fromString(aliases.get(name.lower(), name))
                if sequence.isEmpty():
                    raise ValueError("Unknown key.")
                key, sequence_modifiers = key_combination(sequence)
                if key == Qt.Key.Key_unknown:
                    raise ValueError("Unknown key.")
                text = " " if key == Qt.Key.Key_Space else name if len(name) == 1 else ""
                keys = [(key, modifiers | sequence_modifiers, text)]
            for key, modifiers, text in keys:
                for kind in (QEvent.Type.KeyPress, QEvent.Type.KeyRelease):
                    check_target()
                    QApplication.sendEvent(widget, QKeyEvent(kind, key, modifiers, text))
                    check_target()
        settle()
        return {"passed": True, "target_removed": False}

    def execute(self, method, parameters=None):
        parameters = parameters or {}
        self.refresh()
        if method in ("inspect", "data"):
            return self.inspect()
        if method == "screenshot":
            buffer = QBuffer()
            buffer.open(QIODevice.OpenModeFlag.WriteOnly)
            if not self.window.grab().save(buffer, "PNG") or buffer.size() > 20 * 1024 * 1024:
                raise RuntimeError("Snapshot failed or exceeds 20 MiB.")
            return {"mime_type": "image/png", "data": base64.b64encode(bytes(buffer.data())).decode("ascii")}
        if method == "reset_data":
            return self.reset_data()
        if isinstance(method, str) and method.startswith("editor_"):
            editor = getattr(self.window, "preview_editor", None)
            if editor is None:
                raise RuntimeError("Launch a registered Python scene with --edit to use source editing.")
            return editor.execute(method, parameters)
        widget = self.resolve(parameters.get("target", ""))
        if method == "set_data":
            return self.set_data(widget, parameters.get("property"), parameters.get("value"))
        if method == "action":
            return self.action(widget, parameters)
        raise ValueError("Unknown preview command.")

    def poll(self):
        if self.busy:
            return
        self.busy = True
        request = {}
        try:
            files = sorted((self.directory / "requests").glob("*.json"))
            if not files:
                return
            path = files[0]
            try:
                with path.open("rb") as stream:
                    raw = stream.read(256 * 1024 + 1)
                path.unlink()
                if len(raw) > 256 * 1024:
                    raise ValueError("Oversized request.")
                request = json.loads(raw)
                if not isinstance(request, dict):
                    request = {}
                    raise ValueError("Request must be an object.")
                if request.get("session_id") != self.session_id:
                    raise ValueError("Session changed. Reconnect to the current preview.")
                if not isinstance(request.get("parameters"), dict):
                    raise ValueError("Request parameters must be an object.")
                result = self.execute(request.get("method"), request["parameters"])
            except (OSError, ValueError, RuntimeError, TypeError) as error:
                result = {"error": str(error)}
            write_json(self.directory / "responses" / path.name, {
                # A source-edit request can replace the scene and its node IDs.
                # Bind the reply to the request epoch; subsequent calls reconnect.
                "id": request.get("id"), "session_id": request.get("session_id", self.session_id), "result": result,
            })
        finally:
            self.busy = False
