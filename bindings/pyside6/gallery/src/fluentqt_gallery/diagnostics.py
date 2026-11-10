"""Gallery-owned persistence and feedback. The reusable native collector does no I/O."""
from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import json
import logging
import os
from pathlib import Path
import sys
import tempfile
import time
import re
import uuid

from PySide6 import __version__ as pyside_version
from PySide6.QtCore import (QAbstractAnimation, QByteArray, QCoreApplication, QEasingCurve, QEvent, QMimeData, QObject,
                           QPoint, QPropertyAnimation, QRect, QSize, QStandardPaths, QThread, QTimer, QUrl, QUrlQuery,
                           QVariantAnimation, Signal, Qt, QtMsgType, qFormatLogMessage, qInstallMessageHandler)
from PySide6.QtGui import QColor, QDesktopServices, QFont, QKeySequence
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget
import fluentqt
from fluentqt._qt_compat import single_shot

_save_workers = set()
_MAX_CRASH_BYTES = 16 * 1024 * 1024


def _atomic_write(path, content):
    descriptor, temporary = tempfile.mkstemp(prefix=".report-", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as file:
            file.write(content)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def process_identity():
    return {"pid": os.getpid(), "executable": str(Path(QCoreApplication.applicationFilePath()).resolve()),
            "started_ms": int(time.time() * 1000)}


def existing_crash_attachments(identities, directory):
    files, remaining = {}, _MAX_CRASH_BYTES
    try:
        candidates = sorted((p for p in Path(directory).iterdir()
                             if p.suffix in {".ips", ".crash", ".dmp"} and p.is_file() and not p.is_symlink()),
                            key=lambda p: p.stat().st_mtime, reverse=True)
    except OSError:
        return files
    for path in candidates:
        if len(files) >= 2:
            break
        try:
            stat = path.stat()
            if not 0 < stat.st_size <= remaining:
                continue
            data = None
            for identity in identities:
                pid, executable = identity.get("pid", 0), identity.get("executable", "")
                started, checkpoint = identity.get("started_ms", 0), identity.get("checkpoint_ms", 0)
                if (not pid or not executable or not started or checkpoint < started
                        or not started - 2000 <= stat.st_mtime * 1000 <= checkpoint + 120000):
                    continue
                suffix = path.suffix
                if suffix == ".dmp" and path.name.casefold() != (Path(executable).name + f".{pid}.dmp").casefold():
                    continue
                if data is None:
                    with path.open("rb") as file:
                        data = file.read(remaining + 1)
                    if not 0 < len(data) <= remaining:
                        break
                matches = suffix == ".dmp"
                if suffix == ".ips":
                    try:
                        crash = json.loads(data)
                    except ValueError:
                        crash = json.loads(data.split(b"\n", 1)[-1])
                    matches = isinstance(crash, dict) and crash.get("pid") == pid and crash.get("procPath") == executable
                elif suffix == ".crash":
                    header = data[:65536].decode("utf-8", errors="replace")
                    name = re.search(r"^(?:Path|ExecutablePath):[ \t]*(.+)$", header, re.M)
                    process = (re.search(rf"^Process:.*\[{pid}\][ \t]*$", header, re.M)
                               or re.search(rf"^[ \t]*Pid:[ \t]*{pid}[ \t]*$", header, re.M))
                    matches = name and name.group(1).strip() == executable and process
                if matches:
                    files[f"crash-{len(files) + 1}{suffix}"] = QByteArray(data)
                    remaining -= len(data)
                    break
        except (OSError, ValueError):
            continue
    return files


class _PrepareFeedback(QThread):
    completed = Signal(bool, str)

    def __init__(self, directory, report, attachments, identities):
        super().__init__()
        self.directory, self.report, self.attachments, self.identities = directory, report, attachments, identities

    def run(self):
        try:
            crashes = existing_crash_attachments(self.identities, crash_report_directory())
            self.report["crash_files_included"] = len(crashes)
            self.attachments.update(crashes)
            content = bytes(fluentqt.DiagnosticSession.createBundle(self.report, self.attachments))
            if not content:
                raise ValueError("Could not prepare the report.")
            directory = Path(self.directory) / "reports"
            directory.mkdir(mode=0o700, parents=True, exist_ok=True)
            path = directory / f"fluentqt-report-{uuid.uuid4()}.zip"
            _atomic_write(path, content)
            old = sorted((p for p in directory.glob("fluentqt-report-*.zip") if not p.is_symlink()),
                         key=lambda p: p.stat().st_mtime, reverse=True)
            for item in old[5:]:
                item.unlink()
            self.completed.emit(True, str(path))
        except (OSError, ValueError):
            self.completed.emit(False, "Could not save the report. Try again.")


_MAX_CARD_WIDTH = 244
_COMPACT_CARD_HEIGHT = 66
_COMPACT_ACTION_HEIGHT = 92
_ARTIFACT_READY_HEIGHT = 140
_ENTER_DURATION_MS = 220
_EXIT_DURATION_MS = 140
_MORPH_DURATION_MS = 160
_SPIN_DURATION_MS = 900
_REST_MARGIN = 12
_COMPACT_RAIL_MAX_WIDTH = 64


def _color_spec(color):
    return f"#{color.red():02X}{color.green():02X}{color.blue():02X}{color.alpha():02X}"


def _mix_color(surface, foreground, mix, high_contrast):
    if high_contrast:
        return surface
    return QColor.fromRgbF(
        surface.redF() * (1.0 - mix) + foreground.redF() * mix,
        surface.greenF() * (1.0 - mix) + foreground.greenF() * mix,
        surface.blueF() * (1.0 - mix) + foreground.blueF() * mix,
        1.0,
    )


def _badge_tint(surface, foreground, dark, high_contrast):
    return _mix_color(surface, foreground, 0.16 if dark else 0.11, high_contrast)


def _format_byte_size(byte_count):
    if byte_count <= 0:
        return ""
    if byte_count < 1024:
        return f"{byte_count} B"
    if byte_count < 1024 * 1024:
        return f"{max(1, (byte_count + 512) // 1024)} KB"
    return f"{byte_count / (1024 * 1024):.1f} MB"


def _paste_shortcut_text():
    native = QKeySequence(QKeySequence.StandardKey.Paste).toString(
        QKeySequence.SequenceFormat.NativeText
    )
    return native or "Ctrl+V"


class _FeedbackTipContent(fluentqt.FluentWidget):
    def __init__(self, parent):
        super().__init__(parent)
        self.preparing = False
        self.error = False
        # Reserve a 4px outer left gutter so Small Button's 8px internal horizontal padding
        # places its painted text flush at x = 12 alongside the header badge and ZIP capsule.
        self.root_layout = QVBoxLayout(self)
        self.root_layout.setContentsMargins(4, 12, 12, 12)
        self.root_layout.setSpacing(0)

        header_row = QHBoxLayout()
        header_row.setContentsMargins(8, 0, 0, 0)
        header_row.setSpacing(8)

        self.status_badge = fluentqt.Card(self)
        self.status_badge.setObjectName("galleryFeedbackStatusBadge")
        self.status_badge.setBorderVisible(False)
        self.status_badge.setFixedSize(24, 24)
        badge_layout = QHBoxLayout(self.status_badge)
        badge_layout.setContentsMargins(0, 0, 0, 0)
        self.icon = fluentqt.FontIcon(self.status_badge)
        self.icon.setObjectName("galleryFeedbackStatusIcon")
        self.icon.setIconSize(16)
        self.icon.setFixedSize(16, 16)
        badge_layout.addWidget(self.icon, 0, Qt.AlignCenter)
        header_row.addWidget(self.status_badge, 0, Qt.AlignTop)

        self.title_label = fluentqt.Label(self)
        self.title_label.setObjectName("galleryFeedbackTitle")
        self.title_label.setWordWrap(True)
        self.title_label.setMinimumHeight(24)
        header_row.addWidget(self.title_label, 1)

        self.close_button = fluentqt.Button(self)
        self.close_button.setObjectName("galleryFeedbackClose")
        self.close_button.setAccessibleName("Close report notification")
        self.close_button.setFluentStyle(fluentqt.Button.Subtle)
        self.close_button.setTextColorRole(fluentqt.Button.TextColorRole.Secondary)
        self.close_button.setFluentLayout(fluentqt.Button.IconOnly)
        self.close_button.setIconGlyph(fluentqt.Typography.Icons.Dismiss, 12)
        self.close_button.setFixedSize(24, 24)
        header_row.addWidget(self.close_button, 0, Qt.AlignTop)
        self.root_layout.addLayout(header_row)

        self.artifact_host = QWidget(self)
        artifact_outer = QVBoxLayout(self.artifact_host)
        artifact_outer.setContentsMargins(8, 6, 0, 0)
        artifact_outer.setSpacing(0)
        self.artifact_card = fluentqt.Card(self.artifact_host)
        self.artifact_card.setObjectName("galleryFeedbackArtifact")
        self.artifact_card.setBorderVisible(True)
        self.artifact_card.setFixedHeight(40)

        artifact_layout = QHBoxLayout(self.artifact_card)
        artifact_layout.setContentsMargins(8, 5, 10, 5)
        artifact_layout.setSpacing(8)

        self.artifact_icon_box = fluentqt.Card(self.artifact_card)
        self.artifact_icon_box.setObjectName("galleryFeedbackArtifactIconBox")
        self.artifact_icon_box.setBorderVisible(False)
        self.artifact_icon_box.setFixedSize(26, 26)
        artifact_icon_layout = QHBoxLayout(self.artifact_icon_box)
        artifact_icon_layout.setContentsMargins(0, 0, 0, 0)
        self.artifact_icon = fluentqt.FontIcon(self.artifact_icon_box)
        self.artifact_icon.setObjectName("galleryFeedbackArtifactIcon")
        self.artifact_icon.setGlyph("ic_fluent_folder_zip_16_regular")
        self.artifact_icon.setIconSize(16)
        self.artifact_icon.setFixedSize(16, 16)
        artifact_icon_layout.addWidget(self.artifact_icon, 0, Qt.AlignCenter)
        artifact_layout.addWidget(self.artifact_icon_box, 0, Qt.AlignVCenter)

        meta_column = QVBoxLayout()
        meta_column.setContentsMargins(0, 0, 0, 0)
        meta_column.setSpacing(1)

        name_row = QHBoxLayout()
        name_row.setContentsMargins(0, 0, 0, 0)
        name_row.setSpacing(6)
        self.artifact_name = fluentqt.Label(self.artifact_card)
        self.artifact_name.setObjectName("galleryFeedbackArtifactName")
        self.artifact_name.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        self.artifact_name.setTextElideMode(Qt.TextElideMode.ElideMiddle)
        name_row.addWidget(self.artifact_name, 1)

        self.artifact_size = fluentqt.Label(self.artifact_card)
        self.artifact_size.setObjectName("galleryFeedbackArtifactSize")
        self.artifact_size.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
        name_row.addWidget(self.artifact_size, 0, Qt.AlignRight)
        meta_column.addLayout(name_row)

        state_row = QHBoxLayout()
        state_row.setContentsMargins(0, 0, 0, 0)
        state_row.setSpacing(4)
        self.artifact_state_icon = fluentqt.FontIcon(self.artifact_card)
        self.artifact_state_icon.setObjectName("galleryFeedbackArtifactStateIcon")
        self.artifact_state_icon.setIconSize(12)
        self.artifact_state_icon.setFixedSize(12, 12)
        state_row.addWidget(self.artifact_state_icon, 0, Qt.AlignVCenter)

        self.artifact_state = fluentqt.Label(self.artifact_card)
        self.artifact_state.setObjectName("galleryFeedbackArtifactState")
        self.artifact_state.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        state_row.addWidget(self.artifact_state, 1)
        meta_column.addLayout(state_row)

        artifact_layout.addLayout(meta_column, 1)
        artifact_outer.addWidget(self.artifact_card)
        self.artifact_host.hide()
        self.root_layout.addWidget(self.artifact_host)

        self.message_layout = QHBoxLayout()
        self.message_layout.setSpacing(0)
        self.message_label = fluentqt.Label(self)
        self.message_label.setObjectName("galleryFeedbackMessage")
        self.message_label.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
        self.message_label.setWordWrap(True)
        self.message_label.setMinimumHeight(16)
        self.message_layout.addWidget(self.message_label, 1)
        self.root_layout.addLayout(self.message_layout)

        self.action_host = QWidget(self)
        self.action_layout = QHBoxLayout(self.action_host)
        self.action_layout.setSpacing(0)
        self.action = fluentqt.Button(self.action_host)
        self.action.setObjectName("galleryFeedbackAction")
        self.action.setFluentStyle(fluentqt.Button.Subtle)
        self.action.setTextColorRole(fluentqt.Button.TextColorRole.Accent)
        self.action.setFluentSize(fluentqt.Button.Small)
        self.action.setCursor(Qt.PointingHandCursor)
        self.action.setFixedHeight(24)
        self.action_layout.addWidget(self.action, 0, Qt.AlignLeft)
        self.action_host.hide()
        self.root_layout.addWidget(self.action_host)

        self.update_body_indentation()
        self.on_theme_updated()

    def update_body_indentation(self):
        if not self.artifact_host.isHidden():
            self.message_layout.setContentsMargins(8, 6, 0, 0)
            self.action_layout.setContentsMargins(0, 2, 0, 0)
        else:
            self.message_layout.setContentsMargins(40, 2, 4, 0)
            self.action_layout.setContentsMargins(32, 4, 0, 0)

    def on_theme_updated(self):
        if not hasattr(self, "action"):
            return
        colors = self.theme_tokens()["colors"]
        effective = self.effective_theme()
        dark = fluentqt.theme_uses_dark_appearance(effective)
        high_contrast = effective == fluentqt.Theme.HighContrast
        bg_layer = QColor(colors["bgLayer"])
        accent = QColor(colors["accentDefault"])
        success = QColor(colors["systemSuccess"])
        icon_color = QColor(
            colors[
                "systemCritical"
                if self.error
                else "accentDefault"
                if self.preparing
                else "systemSuccess"
            ]
        )
        badge_bg = _badge_tint(bg_layer, icon_color, dark, high_contrast)
        tint = {"bgLayer": _color_spec(badge_bg)}
        fluentqt.set_widget_theme_overrides(
            self.status_badge,
            {"light": tint, "dark": tint, "contrast": tint, "radius": {"control": 6}},
        )
        self.icon.setColor(icon_color)

        contrast_fg = QColor(Qt.white) if dark else QColor(Qt.black)
        artifact_bg = _mix_color(bg_layer, contrast_fg, 0.055 if dark else 0.035, high_contrast)
        artifact_border = (
            QColor(colors["strokeDefault"])
            if high_contrast
            else _mix_color(bg_layer, contrast_fg, 0.11 if dark else 0.08, False)
        )
        artifact_colors = {
            "bgLayer": _color_spec(artifact_bg),
            "strokeDefault": _color_spec(artifact_border),
        }
        fluentqt.set_widget_theme_overrides(
            self.artifact_card,
            {
                "light": artifact_colors,
                "dark": artifact_colors,
                "contrast": artifact_colors,
                "radius": {"control": 6},
            },
        )

        zip_tile_bg = _badge_tint(artifact_bg, accent, dark, high_contrast)
        zip_tile_colors = {"bgLayer": _color_spec(zip_tile_bg)}
        fluentqt.set_widget_theme_overrides(
            self.artifact_icon_box,
            {
                "light": zip_tile_colors,
                "dark": zip_tile_colors,
                "contrast": zip_tile_colors,
                "radius": {"control": 5},
            },
        )
        self.artifact_icon.setColor(accent)

        self.artifact_state_icon.setColor(success)
        state_text_colors = {"textPrimary": _color_spec(success)}
        fluentqt.set_widget_theme_overrides(
            self.artifact_state,
            {"light": state_text_colors, "dark": state_text_colors, "contrast": state_text_colors},
        )

        strong_base = self.theme_font(fluentqt.FontRole.BodyStrong)
        caption_font = self.theme_font(fluentqt.FontRole.Caption)
        scale = strong_base.pixelSize() / 14

        title_font = QFont(strong_base)
        title_font.setPixelSize(max(1, round(13 * scale)))
        self.title_label.setFont(title_font)

        artifact_name_font = QFont(strong_base)
        artifact_name_font.setPixelSize(max(1, caption_font.pixelSize()))
        self.artifact_name.setFont(artifact_name_font)

        artifact_meta_font = QFont(caption_font)
        artifact_meta_font.setPixelSize(max(1, round(11 * scale)))
        self.artifact_size.setFont(artifact_meta_font)

        artifact_state_font = QFont(strong_base)
        artifact_state_font.setPixelSize(max(1, round(11 * scale)))
        self.artifact_state.setFont(artifact_state_font)

        self.message_label.setFont(caption_font)

        action_font = QFont(strong_base)
        action_font.setPixelSize(max(1, caption_font.pixelSize()))
        self.action.setFont(action_font)

        self.title_label.setMinimumHeight(max(24, self.title_label.fontMetrics().height()))
        self.message_label.setMinimumHeight(max(16, self.message_label.fontMetrics().height()))
        self.action.setFixedSize(self.action.sizeHint().width(), max(24, self.action.sizeHint().height()))
        super().on_theme_updated()


class _FeedbackTip(fluentqt.TeachingTip):
    def __init__(self, parent):
        super().__init__(parent)
        self.setObjectName("galleryFeedbackTip")
        self.setAccessibleName("Problem report")
        self._target_card_size = QSize(_MAX_CARD_WIDTH, _COMPACT_CARD_HEIGHT)
        self.setCardSize(self._target_card_size)
        self.setTailVisible(False)
        self.setLightDismissEnabled(True)
        self._setFocusOnOpenEnabled(False)
        self.content = _FeedbackTipContent(self.contentHost())
        layout = QVBoxLayout(self.contentHost())
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self.content)
        self._handler = None
        self._position_pending = False

        self._spin_animation = QPropertyAnimation(self.content.icon, b"rotation", self)
        self._spin_animation.setStartValue(0.0)
        self._spin_animation.setEndValue(360.0)
        self._spin_animation.setDuration(_SPIN_DURATION_MS)
        self._spin_animation.setEasingCurve(QEasingCurve.Linear)
        self._spin_animation.setLoopCount(-1)

        self._action_fade_animation = QPropertyAnimation(self.content.action, b"contentOpacity", self)
        self._action_fade_animation.setEasingCurve(QEasingCurve.OutCubic)

        self._size_animation = QVariantAnimation(self)
        self._size_animation.setEasingCurve(QEasingCurve.OutCubic)
        self._size_animation.valueChanged.connect(self._on_size_value_changed)

        self.content.close_button.clicked.connect(lambda: self.closeWithReason(fluentqt.TeachingTip.CloseReason.CloseButton))
        self.content.action.clicked.connect(self._invoke)
        self.popupProgressChanged.connect(self._apply_progress)
        self.aboutToHide.connect(self._on_about_to_hide)
        fluentqt.motion_policy().modeChanged.connect(self._on_motion_mode_changed)
        self._progress_animation = next((animation for animation in self.findChildren(
            QPropertyAnimation, options=Qt.FindDirectChildrenOnly)
            if animation.targetObject() is self and bytes(animation.propertyName()) == b"popupProgress"), None)
        if self._progress_animation is not None:
            self._progress_animation.stateChanged.connect(self._transition_state)

    def _on_size_value_changed(self, value):
        from shiboken6 import isValid
        if isValid(self) and isinstance(value, QSize):
            self.setCardSize(value)

    def _on_about_to_hide(self):
        self._spin_animation.stop()
        self.content.icon.setRotation(0.0)
        self._size_animation.stop()
        self._action_fade_animation.stop()
        self.content.action.setContentOpacity(1.0)

    def _on_motion_mode_changed(self, *_):
        from shiboken6 import isValid
        if not isValid(self):
            return
        self._apply_progress()
        self._update_spin_state()
        if (self._size_animation.state() == QAbstractAnimation.Running
                and fluentqt.current_motion_mode() != fluentqt.MotionMode.Full):
            self._size_animation.stop()
            self.setCardSize(self._target_card_size)

    def _transition_state(self, state, _old_state):
        from shiboken6 import isValid
        if state == QAbstractAnimation.Running and isValid(self) and isValid(self._progress_animation):
            entering = self.isOpen()
            self._progress_animation.setEasingCurve(QEasingCurve.OutCubic if entering else QEasingCurve.InCubic)
            self._progress_animation.setDuration(
                fluentqt.motion_policy().resolvedDuration(
                    _ENTER_DURATION_MS if entering else _EXIT_DURATION_MS,
                    self.isAnimationEnabled(),
                )
            )

    def title(self):
        return self.content.title_label.text()

    def message(self):
        return self.content.message_label.text()

    def artifactName(self):
        return "" if self.content.artifact_host.isHidden() else self.content.artifact_name.text()

    def artifactState(self):
        return "" if self.content.artifact_host.isHidden() else self.content.artifact_state.text()

    def targetCardSize(self):
        return QSize(self._target_card_size)

    def _update_accessible_summary(self):
        summary = self.content.title_label.text()
        if not self.content.artifact_host.isHidden():
            summary += ". " + self.content.artifact_name.text()
            size_text = self.content.artifact_size.text()
            if not self.content.artifact_size.isHidden() and size_text:
                summary += f" ({size_text}, {self.content.artifact_state.text()})"
            else:
                summary += f" ({self.content.artifact_state.text()})"
        message = self.content.message_label.text()
        if message:
            summary += ". " + message
        self.setAccessibleDescription(summary)

    def configure(self, title, message, preparing, error):
        self.content.preparing, self.content.error = preparing, error
        self.content.title_label.setText(title)
        self.content.message_label.setText(message)
        self._handler = None
        self._action_fade_animation.stop()
        self.content.action.setContentOpacity(1.0)
        self.content.artifact_host.hide()
        self.content.action_host.hide()
        self.content.root_layout.setContentsMargins(4, 12, 12, 12)
        self.content.update_body_indentation()
        self._update_accessible_summary()
        self.content.icon.setGlyph("ic_fluent_arrow_clockwise_16_regular" if preparing else
            "ic_fluent_error_circle_16_regular" if error else "ic_fluent_checkmark_circle_16_regular")
        self.content.on_theme_updated()
        self._update_spin_state()
        self._sync_placement()

    def set_artifact(self, file_name, byte_count, downloaded=False):
        if not file_name:
            self.content.artifact_host.hide()
            self.content.update_body_indentation()
            self._update_accessible_summary()
            self._sync_placement()
            return
        self.content.artifact_name.setText(file_name)
        formatted_size = _format_byte_size(byte_count)
        self.content.artifact_size.setText(formatted_size)
        self.content.artifact_size.setVisible(bool(formatted_size))
        self.content.artifact_state_icon.setGlyph(
            "ic_fluent_arrow_download_16_regular"
            if downloaded
            else "ic_fluent_clipboard_checkmark_16_regular"
        )
        self.content.artifact_state.setText(
            "Download started" if downloaded else "Copied to clipboard"
        )
        self.content.artifact_host.show()
        self.content.update_body_indentation()
        self._update_accessible_summary()
        self.content.on_theme_updated()
        self._sync_placement()

    def set_action(self, caption, glyph, handler):
        was_hidden = self.content.action_host.isHidden()
        self._handler = handler
        self.content.action.setText(caption)
        self.content.action.setIconGlyph(glyph, 12)
        self.content.action.setFluentLayout(fluentqt.Button.IconAfter)
        self.content.action.setAccessibleName(caption)
        self.content.root_layout.setContentsMargins(4, 12, 12, 10)
        self.content.action_host.show()
        self.content.update_body_indentation()
        self.content.on_theme_updated()

        self._action_fade_animation.stop()
        if (was_hidden and self.isOpen() and self.isVisible() and self.popupProgress() > 0.0
                and self.isAnimationEnabled()
                and fluentqt.current_motion_mode() == fluentqt.MotionMode.Full):
            self.content.action.setContentOpacity(0.0)
            self._action_fade_animation.setDuration(
                fluentqt.motion_policy().resolvedDuration(_MORPH_DURATION_MS, self.isAnimationEnabled()))
            self._action_fade_animation.setStartValue(0.0)
            self._action_fade_animation.setEndValue(1.0)
            self._action_fade_animation.start()
        else:
            self.content.action.setContentOpacity(1.0)

        self._sync_placement()

    def _invoke(self):
        handler = self._handler
        self.closeWithReason(fluentqt.TeachingTip.CloseReason.ActionButton)
        if handler:
            handler()

    def show_for(self, anchor):
        self.setTarget(anchor)
        self._sync_placement()
        if not self.isOpen():
            self.showAt(anchor)
        self._update_spin_state()

    def _apply_target_card_size(self, target_size):
        self._target_card_size = QSize(target_size)
        current = self.cardSize()
        morph = (self.isOpen() and self.isVisible() and self.popupProgress() > 0.0
                 and self.isAnimationEnabled()
                 and fluentqt.current_motion_mode() == fluentqt.MotionMode.Full
                 and current.width() == target_size.width() and current.height() != target_size.height())
        if not morph:
            self._size_animation.stop()
            self.setCardSize(target_size)
            return
        if self._size_animation.state() == QAbstractAnimation.Running and self._size_animation.endValue() == target_size:
            return
        self._size_animation.stop()
        self._size_animation.setDuration(
            fluentqt.motion_policy().resolvedDuration(_MORPH_DURATION_MS, self.isAnimationEnabled()))
        self._size_animation.setStartValue(current)
        self._size_animation.setEndValue(target_size)
        self._size_animation.start()

    def _resolved_rest_margin(self):
        from shiboken6 import isValid
        margin = _REST_MARGIN
        anchor = self.target()
        placement = self.preferredPlacement()
        if (anchor is not None and isValid(anchor)
                and placement in (self.PreferredPlacement.Right, self.PreferredPlacement.RightBottom)):
            host = anchor.window()
            pane = anchor.parentWidget()
            if pane is not None and pane is not host and pane.width() <= _COMPACT_RAIL_MAX_WIDTH:
                pane_right = pane.mapTo(host, QPoint(pane.width() - 1, 0)).x()
                anchor_right = anchor.mapTo(host, QPoint(anchor.width() - 1, 0)).x()
                margin = max(margin, pane_right - anchor_right + 8)
        return margin

    def _apply_progress(self, *_):
        rest_margin = self._resolved_rest_margin()
        slide = self.isAnimationEnabled() and fluentqt.current_motion_mode() == fluentqt.MotionMode.Full
        if not slide:
            margin = rest_margin
        elif self.isOpen():
            margin = round((rest_margin - 6) + 6 * self.popupProgress())
        else:
            margin = round((rest_margin - 4) + 4 * self.popupProgress())
        self.setPlacementMargin(margin)

    def _update_spin_state(self):
        should_spin = (self.content.preparing and self.isOpen() and self.isVisible() and self.isAnimationEnabled()
                       and fluentqt.current_motion_mode() == fluentqt.MotionMode.Full)
        if should_spin:
            if self._spin_animation.state() != QAbstractAnimation.Running:
                self._spin_animation.start()
        else:
            self._spin_animation.stop()
            self.content.icon.setRotation(0.0)

    def _sync_placement(self):
        from shiboken6 import isValid
        anchor = self.target()
        if anchor is None or not isValid(anchor):
            return
        host = anchor.window()
        area = host.property("fluentOverlaySurfaceRect")
        if not isinstance(area, QRect) or area.isEmpty():
            area = host.rect()
        rect = QRect(anchor.mapTo(host, QPoint()), anchor.size())
        width = min(_MAX_CARD_WIDTH, max(1, area.width() - 24))
        self.contentHost().layout().activate()
        if not self.content.artifact_host.isHidden():
            min_height = 116 if self.content.action_host.isHidden() else _ARTIFACT_READY_HEIGHT
        else:
            min_height = _COMPACT_CARD_HEIGHT if self.content.action_host.isHidden() else _COMPACT_ACTION_HEIGHT
        height = max(min_height, self.contentHost().layout().totalHeightForWidth(width))
        self._apply_target_card_size(QSize(width, height))

        # Reserve the ready-state footprint while preparing, and align with the anchor edge
        # when near a pane boundary so the card does not straddle the navigation divider.
        outer_inset, align_inset = 16, 8
        placement_height = max(_ARTIFACT_READY_HEIGHT, height)
        centered_top = rect.center().y() - placement_height // 2
        side_centered_fits = (centered_top >= area.top() + outer_inset
                              and centered_top + placement_height <= area.bottom() + 1 - outer_inset)
        right_fits = area.right() - rect.right() >= width + _REST_MARGIN + outer_inset
        left_fits = rect.left() - area.left() >= width + _REST_MARGIN + outer_inset
        top_fits = rect.top() - area.top() >= placement_height + _REST_MARGIN + outer_inset
        bottom_fits = area.bottom() - rect.bottom() >= placement_height + _REST_MARGIN + outer_inset
        align_right_fits = rect.right() - width + 1 >= area.left() + align_inset
        align_left_fits = rect.left() + width - 1 <= area.right() - align_inset
        align_bottom_fits = rect.bottom() - placement_height + 1 >= area.top() + outer_inset

        if side_centered_fits and right_fits:
            placement = self.PreferredPlacement.Right
        elif side_centered_fits and left_fits:
            placement = self.PreferredPlacement.Left
        elif top_fits and align_right_fits:
            placement = self.PreferredPlacement.TopRight
        elif right_fits and align_bottom_fits:
            placement = self.PreferredPlacement.RightBottom
        elif top_fits and align_left_fits:
            placement = self.PreferredPlacement.TopLeft
        elif top_fits:
            placement = self.PreferredPlacement.Top
        elif bottom_fits and align_right_fits:
            placement = self.PreferredPlacement.BottomRight
        elif bottom_fits and align_left_fits:
            placement = self.PreferredPlacement.BottomLeft
        else:
            placement = self.PreferredPlacement.Bottom
        self.setPreferredPlacement(placement)
        self._apply_progress()

    def eventFilter(self, watched, event):
        relevant = False
        if hasattr(self, "content") and event.type() in (QEvent.Move, QEvent.Resize, QEvent.Show,
                QEvent.Hide, QEvent.LayoutRequest, QEvent.ParentChange):
            node = self.target()
            while node is not None:
                if watched is node:
                    relevant = True
                    break
                node = node.parentWidget()
        result = super().eventFilter(watched, event)
        if relevant and self.isOpen() and not self._position_pending:
            self._position_pending = True
            single_shot(0, self, self._queue_placement)
        return result

    def _queue_placement(self):
        self._position_pending = False
        if self.isOpen():
            self._sync_placement()


class _LogCapture(logging.Handler):
    def __init__(self, session):
        super().__init__()
        self.session = session
        self.previous = None
        self.handler = self._qt_message
        self.active = False

    def start(self):
        with self.lock:
            if self.active:
                return
            self.previous = qInstallMessageHandler(self.handler)
            self.active = True
        logging.getLogger().addHandler(self)

    def stop(self):
        # Drain in-flight native calls before window teardown schedules deletion.
        with self.lock:
            if not self.active:
                return
            self.active = False
        logging.getLogger().removeHandler(self)
        current = qInstallMessageHandler(self.previous)
        if current is not self.handler:
            qInstallMessageHandler(current)

    def _qt_message(self, kind, context, message):
        with self.lock:
            if self.active:
                self.session.recordMessage(kind, context.category or "default", message)
            previous = self.previous
        if previous:
            previous(kind, context, message)
        else:
            sys.stderr.write(qFormatLogMessage(kind, context, message) + "\n")

    def emit(self, record):
        with self.lock:
            if not self.active:
                return
            try:
                kind = (QtMsgType.QtCriticalMsg if record.levelno >= logging.ERROR else
                        QtMsgType.QtWarningMsg if record.levelno >= logging.WARNING else
                        QtMsgType.QtInfoMsg if record.levelno >= logging.INFO else QtMsgType.QtDebugMsg)
                self.session.recordMessage(kind, "python." + record.name, self.format(record))
            except Exception:
                self.handleError(record)


class DiagnosticRecorder(QObject):
    def __init__(self, owner, storage_directory=None):
        # Keep wrappers independent of the window's native child destruction order.
        # The window retains this recorder; close drains logging before deferred deletion.
        super().__init__()
        self.owner = owner
        self.session_id = str(uuid.uuid4())
        self.session = fluentqt.DiagnosticSession()
        self.capture = _LogCapture(self.session)
        self.previous = b""
        self.previous_identity = {}
        self.identity = process_identity()
        self.last_report_bytes = 0
        self.preparing = False
        self.feedback_tip = None
        self.feedback_anchor = None
        self.feedback_visible = False
        self.path = None
        self.writer = None
        self.closed = False
        self.future = None
        directory = (Path(storage_directory) if storage_directory is not None else
                     Path(QStandardPaths.writableLocation(QStandardPaths.AppLocalDataLocation)) / "diagnostics")
        self.directory = directory
        try:
            directory.mkdir(mode=0o700, parents=True, exist_ok=True)
            existing = sorted((p for p in directory.glob("session-*.json") if p.is_file() and not p.is_symlink()),
                              key=lambda p: p.stat().st_mtime, reverse=True)
            if existing and existing[0].stat().st_size <= 1024 * 1024:
                try:
                    with existing[0].open("rb") as file:
                        candidate = file.read(1024 * 1024 + 1)
                    parsed = json.loads(candidate)
                    if len(candidate) <= 1024 * 1024 and isinstance(parsed, dict) and parsed.get("tool") == "FluentQt Diagnostics":
                        self.previous_identity = parsed.pop("_local_process", {})
                        self.previous = json.dumps(parsed).encode("utf-8")
                except (OSError, ValueError):
                    pass
            for old in existing[2:]:
                old.unlink()
            self.path = directory / ("session-" + self.session_id + ".json")
            self.writer = ThreadPoolExecutor(max_workers=1, thread_name_prefix="GalleryDiagnostics")
        except (OSError, ValueError):
            pass  # A read-only location does not prevent in-memory feedback.
        self.session.start(owner, True, 0)
        self.capture.start()
        self.timer = QTimer(self)
        self.timer.setInterval(15000)
        self.timer.timeout.connect(self.persist)
        self.timer.start()
        owner.destroyed.connect(self.close)
        from PySide6.QtCore import QCoreApplication
        QCoreApplication.instance().aboutToQuit.connect(self.close)
        self.persist()

    def report(self, snapshot=True):
        report = dict(self.session.captureReport() if snapshot else self.session.report())
        report["session_id"] = self.session_id
        report["crash_capture"] = "existing-files-only"
        report["environment"] = {**report["environment"], "python_version": sys.version.split()[0],
                                 "pyside_version": pyside_version}
        if snapshot:
            settings = getattr(self.owner, "_settings", None)
            report["gallery"] = {"last_content_route": self.owner.property("galleryLastContentRoute"),
                                 "backdrop": int(settings.window_effect) if settings else None,
                                 "spatial_enabled": bool(settings.spatial_mode_enabled) if settings else None,
                                 "interface_zoom_percent": int(settings.interface_zoom_percent) if settings else None}
        return report

    def stored_report(self):
        return {**self.report(False), "_local_process": {**self.identity, "checkpoint_ms": int(time.time() * 1000)}}

    def _feedback_hidden(self):
        self.feedback_visible = False

    def _show_feedback(self, title, message, error=False):
        from shiboken6 import isValid
        anchor = self.feedback_anchor
        if not self.feedback_visible or anchor is None or not isValid(anchor) or not anchor.isVisible():
            return None
        if self.feedback_tip is None:
            self.feedback_tip = _FeedbackTip(self.owner)
            self.feedback_tip.aboutToHide.connect(self._feedback_hidden)
        self.feedback_tip.configure(title, message, self.preparing, error)
        self.feedback_tip.show_for(anchor)
        return self.feedback_tip

    def _show_prepared(self, open_failed=False):
        if self.closed:
            return
        paste_hint = f"Press {_paste_shortcut_text()} on GitHub to attach it."
        tip = self._show_feedback(
            "Couldn’t open GitHub" if open_failed else "Diagnostic ZIP ready",
            "ZIP is in clipboard. Try again." if open_failed else paste_hint,
            open_failed,
        )
        if tip:
            tip.set_artifact("fluentqt-report.zip", self.last_report_bytes, False)
            tip.set_action("Open GitHub issue", "ic_fluent_open_20_regular", self._open_issue)

    def _open_issue(self):
        if not QDesktopServices.openUrl(self.issue_target):
            single_shot(0, self, self._open_failed)

    def _open_failed(self):
        self.feedback_visible = True
        self._show_prepared(True)

    def report_problem(self, source=None):
        if self.closed:
            return
        if source is not None:
            self.feedback_anchor = source
            self.feedback_visible = True
        if self.feedback_anchor is None:
            return
        already_preparing = self.preparing
        self.preparing = True
        self._show_feedback("Preparing report ZIP…", "Packaging logs for clipboard…")
        if already_preparing:
            return
        report = self.report()
        self.issue_target = issue_url(report)
        attachments = {"previous-session.json": QByteArray(self.previous)} if self.previous else {}
        worker = _PrepareFeedback(self.directory, report, attachments,
            [{**self.identity, "checkpoint_ms": int(time.time() * 1000)}, self.previous_identity])
        _save_workers.add(worker)
        worker.completed.connect(self._feedback_ready)
        worker.finished.connect(worker.deleteLater)
        worker.finished.connect(lambda: _save_workers.discard(worker))
        worker.start()

    def _feedback_ready(self, ok, detail):
        self.preparing = False
        if self.closed:
            return
        if ok:
            try:
                self.last_report_bytes = Path(detail).stat().st_size
            except OSError:
                self.last_report_bytes = 0
            mime = QMimeData()
            url = QUrl.fromLocalFile(detail)
            mime.setUrls([url])
            mime.setData("x-special/gnome-copied-files", b"copy\n" + bytes(url.toEncoded()))
            QApplication.clipboard().setMimeData(mime)
            self._show_prepared()
            return
        tip = self._show_feedback("Couldn’t save report", "Logs are still available.", True)
        if tip:
            tip.set_action("Try again", "ic_fluent_arrow_clockwise_20_regular", self._retry)

    def _retry(self):
        single_shot(0, self, lambda: self.report_problem(self.feedback_anchor))

    def persist(self):
        if self.closed or self.writer is None or (self.future is not None and not self.future.done()):
            return
        content = json.dumps(self.stored_report(), ensure_ascii=False).encode("utf-8")
        self.future = self.writer.submit(_atomic_write, self.path, content)

    def close(self, *_):
        if self.closed:
            return
        self.timer.stop()
        self.capture.stop()
        self.session.stop()
        self.closed = True
        if self.writer is not None:
            # Queue the final checkpoint after any in-flight write; do not lose the last events.
            content = json.dumps(self.stored_report(), ensure_ascii=False).encode("utf-8")
            try:
                self.writer.submit(_atomic_write, self.path, content).result()
            except OSError:
                pass
            self.writer.shutdown(wait=True)
        self.writer = None
        self.session.deleteLater()
        self.deleteLater()


def ensure_diagnostics(window, storage_directory=None):
    ancestor = window
    while ancestor is not None:
        if hasattr(ancestor, "_gallery_diagnostics"):
            return ancestor._gallery_diagnostics
        ancestor = ancestor.parentWidget()
    if not hasattr(window, "_gallery_diagnostics"):
        window._gallery_diagnostics = DiagnosticRecorder(window, storage_directory)
    return window._gallery_diagnostics


def issue_url(report):
    env = report["environment"]
    query = QUrlQuery()
    fields = {"template": "bug_report.yml", "version": env["fluentqt_version"],
              "environment": "{os}, {architecture}, Qt {qt_version}, Python {python_version}, PySide6 {pyside_version}".format(**env),
              "install": "PySide6 Gallery", "evidence": "Report ID: " + report["session_id"] +
              "\nPaste or attach the diagnostic ZIP here before submitting.\nReview the attachment before sharing."}
    for key, value in fields.items():
        query.addQueryItem(key, value)
    url = QUrl("https://github.com/calvinhxx/Fluent-Qt/issues/new")
    url.setQuery(query)
    return url


def crash_report_directory():
    if sys.platform == "darwin":
        return str(Path.home() / "Library/Logs/DiagnosticReports")
    if sys.platform == "win32":
        return str(Path(os.environ.get("LOCALAPPDATA", str(Path.home()))) / "CrashDumps")
    return "/var/crash"


class FeedbackButton(fluentqt.Button):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("galleryFeedbackButton")
        self.setAccessibleName("Report a problem")
        self.setAccessibleDescription("Copy a diagnostic ZIP and open a GitHub issue.")
        fluentqt.ToolTip.attach(self, "Report a problem", fluentqt.ToolTip.Above)
        self.setFluentLayout(fluentqt.Button.ButtonLayout.IconOnly)
        self.setFluentSize(fluentqt.Button.ButtonSize.Small)
        self.setFluentStyle(fluentqt.Button.ButtonStyle.Subtle)
        self.setIconGlyph("ic_fluent_bug_16_regular", fluentqt.Typography.IconSize.Compact)
        self.setFixedSize(24, 24)
        fluentqt.set_widget_theme_overrides(self, {"radius": {"control": 4}})
        self.clicked.connect(lambda: ensure_diagnostics(self).report_problem(self))
