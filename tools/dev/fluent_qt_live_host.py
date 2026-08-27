#!/usr/bin/env python3
"""Persistent Live Scene host for repository UI development.

The top-level window and Python process stay alive. Saving the selected scene
executes it in an isolated namespace, builds a hidden candidate widget, and
only swaps the scene subtree after construction succeeds.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import os
from pathlib import Path
import runpy
import sys
import time
import traceback
from typing import Iterable

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import (  # noqa: E402
    QEventLoop,
    QTimer,
    Qt,
    Signal,
    qVersion,
)
from PySide6.QtGui import QKeySequence, QShortcut  # noqa: E402
from PySide6.QtWidgets import (  # noqa: E402
    QAbstractScrollArea,
    QApplication,
    QHBoxLayout,
    QLineEdit,
    QPlainTextEdit,
    QSizePolicy,
    QStackedLayout,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

_STATE_PROPERTIES = (
    "checked",
    "checkState",
    "isOn",
    "currentIndex",
    "selectedIndex",
    "selectedPageIndex",
    "value",
    "color",
)


@dataclass(frozen=True)
class WidgetSnapshot:
    """Serializable-in-memory interaction state keyed by named widgets."""

    values: dict[str, dict[str, object]]
    focused_key: str | None = None

@dataclass(frozen=True)
class SceneCandidate:
    widget: QWidget
    title: str


def parse_size(value: str) -> tuple[int, int]:
    pieces = value.lower().split("x", 1)
    if len(pieces) != 2:
        raise argparse.ArgumentTypeError("size must be WIDTHxHEIGHT")
    try:
        width, height = (int(piece) for piece in pieces)
    except ValueError as error:
        raise argparse.ArgumentTypeError("size must be WIDTHxHEIGHT") from error
    if not 520 <= width <= 3840 or not 420 <= height <= 2160:
        raise argparse.ArgumentTypeError(
            "size must be within 520x420 and 3840x2160"
        )
    return width, height


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Open a persistent FluentQt window and replace only its scene "
            "subtree whenever the Python scene file is saved."
        )
    )
    parser.add_argument(
        "--scene",
        type=Path,
        required=True,
        help="Python file exporting build(parent) -> QWidget.",
    )
    parser.add_argument(
        "--theme",
        choices=("light", "dark"),
        default="light",
        help="Initial Fluent theme; it can also be toggled in the live window.",
    )
    parser.add_argument("--size", type=parse_size, default=(920, 680))
    parser.add_argument("--rtl", action="store_true")
    parser.add_argument(
        "--no-watch",
        action="store_true",
        help="Disable file polling; the Reload button remains available.",
    )
    parser.add_argument("--snapshot", type=Path)
    parser.add_argument(
        "--report",
        type=Path,
        help="Write an Inspector/reload JSON report after every attempt.",
    )
    parser.add_argument("--settle-ms", type=int, default=220)
    parser.add_argument(
        "--keep-open",
        action="store_true",
        help="Keep the window open after an automated snapshot.",
    )
    args = parser.parse_args(list(argv) if argv is not None else None)
    if not 0 <= args.settle_ms <= 10000:
        parser.error("--settle-ms must be between 0 and 10000")
    args.scene = args.scene.expanduser().resolve()
    if args.snapshot is not None:
        args.snapshot = args.snapshot.expanduser().resolve()
    if args.report is not None:
        args.report = args.report.expanduser().resolve()
    return args


def _state_key(widget: QWidget) -> str | None:
    name = widget.objectName().strip()
    if not name:
        return None
    return "{0}::{1}".format(widget.metaObject().className(), name)


def _read_property(widget: QWidget, name: str) -> tuple[bool, object | None]:
    index = widget.metaObject().indexOfProperty(name)
    if index < 0:
        return False, None
    meta_property = widget.metaObject().property(index)
    if not meta_property.isReadable() or not meta_property.isWritable():
        return False, None
    return True, widget.property(name)


def capture_widget_state(root: QWidget) -> WidgetSnapshot:
    """Capture stable interaction values from uniquely named widgets."""

    values: dict[str, dict[str, object]] = {}
    widgets = (root, *root.findChildren(QWidget))
    for widget in widgets:
        key = _state_key(widget)
        if key is None or key in values:
            continue
        properties: dict[str, object] = {}
        for name in _STATE_PROPERTIES:
            readable, value = _read_property(widget, name)
            if readable:
                properties[name] = value
        if isinstance(widget, QLineEdit):
            properties["text"] = widget.text()
            properties["cursorPosition"] = widget.cursorPosition()
        elif isinstance(widget, (QPlainTextEdit, QTextEdit)):
            properties["plainText"] = widget.toPlainText()
            properties["textCursorPosition"] = widget.textCursor().position()
        if isinstance(widget, QAbstractScrollArea):
            properties["horizontalScrollValue"] = (
                widget.horizontalScrollBar().value()
            )
            properties["verticalScrollValue"] = widget.verticalScrollBar().value()
        if properties:
            values[key] = properties

    focused = QApplication.focusWidget()
    focused_key = None
    if focused is not None and (focused is root or root.isAncestorOf(focused)):
        focused_key = _state_key(focused)
    return WidgetSnapshot(values=values, focused_key=focused_key)


def _values_equal(left: object, right: object) -> bool:
    try:
        return bool(left == right)
    except Exception:
        return False


def interaction_delta(
    current: WidgetSnapshot, baseline: WidgetSnapshot
) -> WidgetSnapshot:
    """Return only values changed through interaction since scene creation."""

    values: dict[str, dict[str, object]] = {}
    for key, properties in current.values.items():
        baseline_properties = baseline.values.get(key, {})
        changed = {
            name: value
            for name, value in properties.items()
            if name not in baseline_properties
            or not _values_equal(value, baseline_properties[name])
        }
        if changed:
            values[key] = changed
    focused_key = (
        current.focused_key
        if current.focused_key != baseline.focused_key
        else None
    )
    return WidgetSnapshot(values=values, focused_key=focused_key)


def _write_property(widget: QWidget, name: str, value: object) -> bool:
    if name == "cursorPosition" and isinstance(widget, QLineEdit):
        widget.setCursorPosition(int(value))
        return True
    if name == "plainText" and isinstance(widget, (QPlainTextEdit, QTextEdit)):
        widget.setPlainText(str(value))
        return True
    if name == "textCursorPosition" and isinstance(
        widget, (QPlainTextEdit, QTextEdit)
    ):
        cursor = widget.textCursor()
        cursor.setPosition(max(0, min(int(value), len(widget.toPlainText()))))
        widget.setTextCursor(cursor)
        return True
    if name == "horizontalScrollValue" and isinstance(
        widget, QAbstractScrollArea
    ):
        widget.horizontalScrollBar().setValue(int(value))
        return True
    if name == "verticalScrollValue" and isinstance(widget, QAbstractScrollArea):
        widget.verticalScrollBar().setValue(int(value))
        return True
    index = widget.metaObject().indexOfProperty(name)
    if index < 0:
        return False
    meta_property = widget.metaObject().property(index)
    if not meta_property.isWritable():
        return False
    return bool(widget.setProperty(name, value))


def restore_widget_state(root: QWidget, state: WidgetSnapshot) -> int:
    """Restore interaction deltas and return the number of applied fields."""

    widgets: dict[str, QWidget] = {}
    for widget in (root, *root.findChildren(QWidget)):
        key = _state_key(widget)
        if key is not None and key not in widgets:
            widgets[key] = widget

    restored = 0
    for key, properties in state.values.items():
        widget = widgets.get(key)
        if widget is None:
            continue
        was_blocked = widget.blockSignals(True)
        try:
            for name, value in properties.items():
                if _write_property(widget, name, value):
                    restored += 1
        finally:
            widget.blockSignals(was_blocked)

    focused = widgets.get(state.focused_key or "")
    if focused is not None and focused.isEnabled() and focused.isVisibleTo(root):
        focused.setFocus(Qt.FocusReason.OtherFocusReason)
        restored += 1
    return restored


def execute_scene(scene_path: Path, staging_parent: QWidget) -> SceneCandidate:
    """Execute one scene file and build a hidden QWidget candidate."""

    if not scene_path.is_file():
        raise FileNotFoundError("Scene file does not exist: {0}".format(scene_path))
    namespace = runpy.run_path(
        str(scene_path),
        run_name="__fluentqt_live_scene__",
    )
    builder = namespace.get("build")
    if not callable(builder):
        raise TypeError("Scene must export a callable build(parent)")
    candidate = builder(staging_parent)
    if not isinstance(candidate, QWidget):
        raise TypeError(
            "build(parent) must return QWidget, got {0}".format(
                type(candidate).__name__
            )
        )
    if candidate is staging_parent:
        raise TypeError("build(parent) must return a child widget, not parent")
    if candidate.parentWidget() is not staging_parent:
        candidate.setParent(staging_parent)
    candidate._fluentqt_live_scene_namespace = namespace

    title = namespace.get("SCENE_TITLE", scene_path.stem)
    if not isinstance(title, str):
        raise TypeError("SCENE_TITLE must be a string")
    return SceneCandidate(
        widget=candidate,
        title=title.strip() or scene_path.stem,
    )


def _write_json(path: Path, payload: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(".{0}.{1}.tmp".format(path.name, os.getpid()))
    try:
        temporary.write_text(
            json.dumps(payload, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


class LivePreviewWindow(fluentqt.Window):
    """Stable Fluent window that atomically swaps one developer scene."""

    reloadFinished = Signal(bool, int)

    def __init__(
        self,
        scene_path: Path,
        *,
        watch: bool = True,
        debounce_ms: int = 140,
        window_size: tuple[int, int] = (920, 680),
        rtl: bool = False,
        report_path: Path | None = None,
    ) -> None:
        super().__init__()
        self._scene_path = Path(scene_path).expanduser().resolve()
        self._watch_enabled = bool(watch)
        self._report_path = report_path
        self._scene_widget: QWidget | None = None
        self._scene_defaults = WidgetSnapshot(values={})
        self._generation = 0
        self._reload_failures = 0
        self._last_error = ""
        self._last_reload_ms = 0
        self._last_restored_fields = 0
        self._last_inspector: dict[str, object] = {}
        self._last_inspector_error = ""
        self._snapshot_path: str | None = None
        self._snapshot_written = False
        self._snapshot_error = ""
        self.setObjectName("fluentQtLivePreviewWindow")
        self.setWindowTitle("FluentQt Live Scene")
        self.resize(*window_size)
        self.setMinimumSize(520, 420)
        self.setCustomWindowChromeEnabled(False)
        self.setLayoutDirection(
            Qt.LayoutDirection.RightToLeft
            if rtl
            else Qt.LayoutDirection.LeftToRight
        )
        self.setBackdropEffect(fluentqt.BackdropEffect.Solid)
        self._build_content()

        self._reload_timer = QTimer(self)
        self._reload_timer.setSingleShot(True)
        self._reload_timer.setInterval(int(debounce_ms))
        self._reload_timer.timeout.connect(self.reload_scene)
        self._observed_signature = self._source_signature()
        self._watch_poll_timer = QTimer(self)
        self._watch_poll_timer.setInterval(max(120, int(debounce_ms)))
        self._watch_poll_timer.timeout.connect(self._poll_source_signature)
        if self._watch_enabled:
            self._watch_poll_timer.start()

        self._reload_shortcut = QShortcut(QKeySequence.Refresh, self)
        self._reload_shortcut.activated.connect(self.reload_scene)
        self._refresh_theme_button()

    @property
    def generation(self) -> int:
        return self._generation

    @property
    def scene_widget(self) -> QWidget | None:
        return self._scene_widget

    @property
    def last_error(self) -> str:
        return self._last_error

    @property
    def error_bar(self):
        return self._error_bar

    def _build_content(self) -> None:
        page = QWidget()
        page.setObjectName("livePreviewPage")
        root = QVBoxLayout(page)
        root.setContentsMargins(28, 24, 28, 24)
        root.setSpacing(12)

        header = QHBoxLayout()
        header.setSpacing(8)
        heading = fluentqt.Label("Live Scene", page)
        heading.setFluentTypography(fluentqt.FontRole.TitleLarge)
        header.addWidget(heading)
        header.addStretch()

        status = fluentqt.Label("Starting", page)
        status.setObjectName("livePreviewStatus")
        status.setFluentTypography(fluentqt.FontRole.Caption)
        status.setAccessibleName("Live scene status")
        header.addWidget(status, 0, Qt.AlignmentFlag.AlignVCenter)

        reload_button = fluentqt.Button("Reload", page)
        reload_button.setAccessibleName("Reload live scene")
        reload_button.clicked.connect(self.reload_scene)
        header.addWidget(reload_button)

        theme_button = fluentqt.Button("Use dark theme", page)
        theme_button.setAccessibleName("Toggle live scene theme")
        theme_button.clicked.connect(self._toggle_theme)
        header.addWidget(theme_button)
        root.addLayout(header)

        path_label = fluentqt.Label(str(self._scene_path), page)
        path_label.setObjectName("livePreviewScenePath")
        path_label.setFluentTypography(fluentqt.FontRole.Caption)
        path_label.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
        path_label.setTextInteractionFlags(
            Qt.TextInteractionFlag.TextSelectableByMouse
        )
        path_label.setTextElideMode(Qt.TextElideMode.ElideMiddle)
        path_label.setMinimumWidth(0)
        path_label.setToolTip(str(self._scene_path))
        root.addWidget(path_label)

        error_bar = fluentqt.InfoBar(page)
        error_bar.setObjectName("livePreviewErrorBar")
        error_bar.setSeverity(fluentqt.InfoBar.InfoBarSeverity.Error)
        error_bar.setTitle("Scene reload failed")
        error_bar.setSingleLine(False)
        error_bar.setIsClosable(False)
        error_bar.setIsOpen(False)
        root.addWidget(error_bar)

        scene_scroll = fluentqt.ScrollView(page)
        scene_scroll.setObjectName("livePreviewSceneScroll")
        scene_scroll.setWidgetResizable(True)
        scene_scroll.setHorizontalScrollBarVisibility(
            fluentqt.ScrollView.ScrollBarVisibility.Auto
        )
        scene_scroll.setVerticalScrollBarVisibility(
            fluentqt.ScrollView.ScrollBarVisibility.Auto
        )
        scene_host = QWidget()
        scene_host.setObjectName("livePreviewSceneHost")
        scene_host.setAutoFillBackground(False)
        scene_host.setSizePolicy(
            QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding
        )
        stack = QStackedLayout(scene_host)
        stack.setContentsMargins(0, 0, 0, 0)
        stacking_mode = getattr(QStackedLayout, "StackingMode", QStackedLayout)
        stack.setStackingMode(stacking_mode.StackOne)
        scene_scroll.setContentWidget(scene_host)
        root.addWidget(scene_scroll, 1)

        self.setContentWidget(page)
        self._status_label = status
        self._theme_button = theme_button
        self._error_bar = error_bar
        self._scene_host = scene_host
        self._scene_stack = stack

        placeholder = QWidget(self._scene_host)
        placeholder.setObjectName("livePreviewPlaceholder")
        stack.addWidget(placeholder)
        stack.setCurrentWidget(placeholder)
        self._scene_widget = placeholder
        self._scene_defaults = capture_widget_state(placeholder)

    def _refresh_theme_button(self) -> None:
        dark = fluentqt.current_theme() == fluentqt.Theme.Dark
        self._theme_button.setText(
            "Use light theme" if dark else "Use dark theme"
        )
        self._theme_button.setAccessibleName(
            "Switch live scene to {0} theme".format(
                "light" if dark else "dark"
            )
        )

    def _toggle_theme(self) -> None:
        next_theme = (
            fluentqt.Theme.Light
            if fluentqt.current_theme() == fluentqt.Theme.Dark
            else fluentqt.Theme.Dark
        )
        fluentqt.set_theme(next_theme)
        QApplication.processEvents(
            QEventLoop.ProcessEventsFlag.ExcludeUserInputEvents
        )
        self._refresh_theme_button()
        self._refresh_inspector()
        self._write_report()

    def _source_signature(self) -> tuple[bool, int, int, int]:
        try:
            stat = self._scene_path.stat()
        except FileNotFoundError:
            return False, 0, 0, 0
        return True, stat.st_mtime_ns, stat.st_size, stat.st_ino

    def _poll_source_signature(self) -> None:
        signature = self._source_signature()
        if signature == self._observed_signature:
            return
        self._observed_signature = signature
        self._status_label.setText("Change detected")
        self._reload_timer.start()

    def _refresh_inspector(self) -> None:
        if self._scene_widget is None:
            self._last_inspector = {}
            self._last_inspector_error = ""
            return
        try:
            self._last_inspector = fluentqt.inspect_widget(self._scene_widget)
            self._last_inspector_error = ""
        except Exception as error:
            self._last_inspector = {}
            self._last_inspector_error = "{0}: {1}".format(
                type(error).__name__, error
            )

    def reload_scene(self) -> bool:
        """Build a candidate and commit it without replacing the window."""

        self._status_label.setText("Reloading")
        self._observed_signature = self._source_signature()
        started = time.perf_counter()
        current_state = (
            capture_widget_state(self._scene_widget)
            if self._scene_widget is not None
            else WidgetSnapshot(values={})
        )
        delta = interaction_delta(current_state, self._scene_defaults)
        staging = QWidget(self._scene_host)
        staging.hide()
        try:
            candidate = execute_scene(self._scene_path, staging)
            candidate.widget.setSizePolicy(
                QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding
            )
            candidate.widget.ensurePolished()
            defaults = capture_widget_state(candidate.widget)
            restored = restore_widget_state(candidate.widget, delta)
        except BaseException as error:
            if isinstance(error, KeyboardInterrupt):
                raise
            self._last_reload_ms = int(
                round((time.perf_counter() - started) * 1000)
            )
            self._handle_reload_failure(error, traceback.format_exc())
            staging.deleteLater()
            return False

        previous = self._scene_widget
        self.setUpdatesEnabled(False)
        try:
            self._scene_stack.addWidget(candidate.widget)
            self._scene_stack.setCurrentWidget(candidate.widget)
            candidate.widget.show()
            if previous is not None:
                previous.hide()
                self._scene_stack.removeWidget(previous)
                previous.setParent(None)
                previous.deleteLater()
        finally:
            self.setUpdatesEnabled(True)
            self.update()
        staging.deleteLater()

        self._scene_widget = candidate.widget
        self._scene_defaults = defaults
        self._generation += 1
        self._last_error = ""
        self._last_reload_ms = int(round((time.perf_counter() - started) * 1000))
        self._last_restored_fields = restored
        self.setWindowTitle("{0} · FluentQt Live Scene".format(candidate.title))
        self._error_bar.setIsOpen(False)

        self._scene_stack.activate()
        if candidate.widget.layout() is not None:
            candidate.widget.layout().activate()
        QApplication.processEvents(QEventLoop.ProcessEventsFlag.ExcludeUserInputEvents)
        self._refresh_inspector()

        findings = int(
            self._last_inspector.get("summary", {}).get(
                "findings", len(self._last_inspector.get("findings", ()))
            )
        )
        self._status_label.setText(
            "Updated · {0} ms · {1} finding{2}".format(
                self._last_reload_ms,
                findings,
                "" if findings == 1 else "s",
            )
        )
        self._status_label.setToolTip(
            "Generation {0} · restored {1} interaction field{2} · "
            "window PID {3}".format(
                self._generation,
                restored,
                "" if restored == 1 else "s",
                os.getpid(),
            )
        )
        self._write_report()
        print(
            "[live-scene] generation {0} ready in {1} ms (pid {2})".format(
                self._generation, self._last_reload_ms, os.getpid()
            ),
            flush=True,
        )
        self.reloadFinished.emit(True, self._generation)
        return True

    def _handle_reload_failure(
        self, error: BaseException, formatted_traceback: str
    ) -> None:
        self._reload_failures += 1
        self._last_error = "{0}: {1}".format(type(error).__name__, error)
        self._error_bar.setTitle(
            "Scene kept at generation {0}".format(self._generation)
            if self._generation
            else "Scene could not load"
        )
        self._error_bar.setMessage(
            "{0}\nThe last usable surface is still visible.".format(
                self._last_error
            )
        )
        self._error_bar.setIsOpen(True)
        self._status_label.setText("Reload failed")
        self._status_label.setToolTip(
            "Fix the scene and save again. The window and PID {0} remain "
            "unchanged.".format(os.getpid())
        )
        self._write_report()
        print(
            "[live-scene] reload failed; keeping generation {0}\n{1}".format(
                self._generation, formatted_traceback.rstrip()
            ),
            file=sys.stderr,
            flush=True,
        )
        self.reloadFinished.emit(False, self._generation)

    def report_payload(self) -> dict[str, object]:
        scene = self._scene_widget
        return {
            "schema_version": 1,
            "tool": "FluentQt Live Scene",
            "scene": str(self._scene_path),
            "process": {
                "pid": os.getpid(),
                "platform_plugin": QApplication.platformName(),
                "qt_version": qVersion(),
                "fluentqt_version": fluentqt.__version__,
            },
            "window": {
                "width": self.width(),
                "height": self.height(),
                "layout_direction": (
                    "rtl"
                    if self.layoutDirection() == Qt.LayoutDirection.RightToLeft
                    else "ltr"
                ),
                "theme": (
                    "dark"
                    if fluentqt.current_theme() == fluentqt.Theme.Dark
                    else "light"
                ),
            },
            "reload": {
                "watching": self._watch_enabled,
                "generation": self._generation,
                "failures": self._reload_failures,
                "last_duration_ms": self._last_reload_ms,
                "restored_interaction_fields": self._last_restored_fields,
                "last_error": self._last_error or None,
            },
            "scene_widget": (
                scene.metaObject().className() if scene is not None else None
            ),
            "snapshot": {
                "path": self._snapshot_path,
                "written": self._snapshot_written,
                "error": self._snapshot_error or None,
            },
            "inspector_error": self._last_inspector_error or None,
            "inspector": self._last_inspector,
        }

    def _write_report(self) -> None:
        if self._report_path is None:
            return
        try:
            _write_json(self._report_path, self.report_payload())
        except OSError as error:
            print(
                "[live-scene] unable to write report {0}: {1}".format(
                    self._report_path, error
                ),
                file=sys.stderr,
                flush=True,
            )

    def save_snapshot(self, path: Path) -> bool:
        snapshot = path.expanduser().resolve()
        self._snapshot_path = str(snapshot)
        self._snapshot_written = False
        self._snapshot_error = ""
        try:
            snapshot.parent.mkdir(parents=True, exist_ok=True)
            if not self.grab().save(str(snapshot), "PNG"):
                raise OSError("QPixmap.save returned false")
            self._snapshot_written = True
            print("[live-scene] snapshot: {0}".format(snapshot), flush=True)
        except OSError as error:
            self._snapshot_error = str(error)
            print(
                "[live-scene] snapshot failed: {0}".format(error),
                file=sys.stderr,
                flush=True,
            )
        self._write_report()
        return self._snapshot_written


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    app = QApplication.instance() or QApplication(sys.argv)
    if not fluentqt.initialize_resources():
        print("FluentQt resources could not be initialized", file=sys.stderr)
        return 2
    fluentqt.reset_theme_tokens()
    fluentqt.set_theme(
        fluentqt.Theme.Dark if args.theme == "dark" else fluentqt.Theme.Light
    )
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))
    app.setProperty("fluentqtLivePreview", True)

    window = LivePreviewWindow(
        args.scene,
        watch=not args.no_watch,
        window_size=args.size,
        rtl=args.rtl,
        report_path=args.report,
    )
    initial_finished = False

    def finish_initial_attempt(success: bool, _generation: int) -> None:
        nonlocal initial_finished
        if initial_finished:
            return
        initial_finished = True

        def settle_artifacts() -> None:
            artifacts_ok = True
            if args.snapshot is not None:
                artifacts_ok = window.save_snapshot(args.snapshot)
            if args.snapshot is not None and not args.keep_open:
                app.exit(0 if success and artifacts_ok else 2)

        QTimer.singleShot(args.settle_ms, settle_artifacts)

    window.reloadFinished.connect(finish_initial_attempt)
    window.show()
    QTimer.singleShot(0, window.reload_scene)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
