"""Interactive FluentQt PySide6 compatibility acceptance window.

Run this from a build tree or after installing a wheel. The default mode opens
an interactive window. ``--snapshot`` renders the same window, saves a PNG, and
exits, which is useful for headless smoke checks and visual review artifacts.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt
import fluentqt._fluentqt as native
import PySide6
import shiboken6

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QTimer, Qt, QUrl, qVersion
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (
    QApplication,
    QHBoxLayout,
    QVBoxLayout,
    QWidget,
)
from shiboken6 import Shiboken

from fluentqt.basicinput import (
    Button,
    CheckBox,
    CompoundButton,
    HyperlinkButton,
    RadioButton,
    RatingControl,
    RepeatButton,
    Slider,
    ToggleButton,
    ToggleSwitch,
)
from fluentqt.foundation import FontIcon
from fluentqt.layout import Card, Divider, Expander
from fluentqt.scrolling import PipsPager, ScrollBar
from fluentqt.status_info import (
    Avatar,
    InfoBadge,
    InfoBar,
    ProgressBar,
    ProgressRing,
    Shimmer,
)
from fluentqt.textfields import Label, LineEdit, NumberBox, PasswordBox, TextEdit
from fluentqt.windowing import Window


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt PySide6 acceptance window."
    )
    parser.add_argument(
        "--snapshot",
        type=Path,
        help="Save the rendered acceptance window as a PNG and exit.",
    )
    parser.add_argument(
        "--auto-close-ms",
        type=int,
        default=0,
        help="Automatically close the interactive window after this delay.",
    )
    return parser.parse_args()


def validate_runtime() -> dict[str, str]:
    info = fluentqt.binding_build_info()
    expected = {
        "pyside6_version": PySide6.__version__,
        "shiboken6_version": shiboken6.__version__,
        "qt_compile_version": qVersion(),
        "qt_runtime_version": qVersion(),
    }
    mismatches = {
        name: (info.get(name), value)
        for name, value in expected.items()
        if info.get(name) != value
    }
    if mismatches:
        details = ", ".join(
            f"{name}: native={actual}, runtime={expected_value}"
            for name, (actual, expected_value) in mismatches.items()
        )
        raise RuntimeError(f"FluentQt runtime mismatch: {details}")
    return info


def section_label(text: str, parent: QWidget) -> Label:
    label = Label(text, parent)
    label.setFluentTypography(fluentqt.FontRole.Subtitle)
    return label


def build_showcase(info: dict[str, str]) -> tuple[Window, list[QWidget]]:
    window = Window()
    window.setWindowTitle("FluentQt PySide6 Compatibility")
    # Windows keeps a native title bar in the grabbed top-level surface. Give
    # that platform extra top-level height so the client area's bottom margin
    # matches the Linux and macOS acceptance snapshots.
    window.resize(840, 1020 if sys.platform == "win32" else 980)

    content = QWidget()
    root = QVBoxLayout(content)
    root.setContentsMargins(32, 28, 32, 28)
    root.setSpacing(14)

    title = Label("Python compatibility acceptance", content)
    title.setFluentTypography(fluentqt.FontRole.Title)
    root.addWidget(title)

    summary = Label(
        "These are the same native FluentQt widgets used by C++; Python "
        "provides construction, signals, properties, and application logic.",
        content,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)

    runtime = Label(
        "FluentQt {fluentqt_version}  ·  PySide6 {pyside6_version}  ·  "
        "Qt {qt_runtime_version}".format(**info),
        content,
    )
    runtime.setFluentTypography(fluentqt.FontRole.BodyStrong)
    root.addWidget(runtime)
    root.addWidget(Divider(content))

    toolbar = QHBoxLayout()
    settings_icon = FontIcon("ic_fluent_settings_20_regular", content)
    settings_icon.setIconSize(20)
    theme_button = Button("Theme: Light", content)
    accent_button = Button("Accent: Blue", content)
    accent_button.setFluentStyle(Button.ButtonStyle.Accent)
    toolbar.addWidget(settings_icon)
    toolbar.addWidget(theme_button)
    toolbar.addWidget(accent_button)
    toolbar.addStretch()
    root.addLayout(toolbar)

    columns = QHBoxLayout()
    columns.setSpacing(24)
    left = QWidget(content)
    left_layout = QVBoxLayout(left)
    left_layout.setContentsMargins(0, 0, 0, 0)
    left_layout.setSpacing(12)
    right = QWidget(content)
    right_layout = QVBoxLayout(right)
    right_layout.setContentsMargins(0, 0, 0, 0)
    right_layout.setSpacing(12)

    left_layout.addWidget(section_label("Basic input", left))
    compound = CompoundButton(
        "Install update",
        "Native primary and secondary content",
        left,
    )
    left_layout.addWidget(compound)
    check_box = CheckBox("Animate shimmer", left)
    radio_button = RadioButton("Recommended option", left)
    radio_button.setChecked(True)
    rating = RatingControl(left)
    rating.setValue(3.5)
    rating.setCaption("3.5")
    toggle_button = ToggleButton("Pinned", left)
    toggle_switch = ToggleSwitch(left)
    toggle_switch.setOnContent("On")
    toggle_switch.setOffContent("Off")
    left_layout.addWidget(check_box)
    left_layout.addWidget(radio_button)
    left_layout.addWidget(rating)
    left_layout.addWidget(toggle_button)
    left_layout.addWidget(toggle_switch)

    slider = Slider(Qt.Horizontal, left)
    slider.setRange(0, 100)
    slider.setValue(60)
    left_layout.addWidget(slider)

    repeat_row = QHBoxLayout()
    repeat_button = RepeatButton("Click and hold", left)
    repeat_button.setDelay(250)
    repeat_button.setInterval(80)
    repeat_count = Label("Clicks: 0", left)
    repeat_row.addWidget(repeat_button)
    repeat_row.addWidget(repeat_count)
    repeat_row.addStretch()
    left_layout.addLayout(repeat_row)

    hyperlink = HyperlinkButton("Open FluentQt on GitHub", left)
    hyperlink.setUrl(QUrl("https://github.com/calvinhxx/Fluent-Qt"))
    hyperlink.setShowUnderline(True)
    left_layout.addWidget(hyperlink)

    left_layout.addWidget(section_label("Multiline text", left))
    text_edit = TextEdit(left)
    text_edit.setMinVisibleLines(2)
    text_edit.setMaxVisibleLines(3)
    text_edit.setPlaceholderText("Write a note")
    text_edit.setPlainText("Native multiline input\nfrom the Python binding")
    left_layout.addWidget(text_edit)
    left_layout.addStretch()

    vertical_divider = Divider(Qt.Vertical, content)
    vertical_divider.setLeadingInset(4)
    vertical_divider.setTrailingInset(4)

    right_layout.addWidget(section_label("Text and status", right))

    info_bar = InfoBar(
        title="Bindings ready",
        severity=InfoBar.InfoBarSeverity.Success,
        isClosable=False,
        parent=right,
    )
    info_bar.setPreferredWidth(340)
    info_action = Button("Details")
    info_bar.setActionWidget(info_action)
    right_layout.addWidget(info_bar)

    badge_row = QHBoxLayout()
    avatar = Avatar("Ada Lovelace", right)
    avatar.setPresence(Avatar.PresenceStatus.Available)
    badge_label = Label("Unread messages", right)
    info_badge = InfoBadge(right)
    info_badge.setValue(7)
    info_badge.setDisplayMode(
        InfoBadge.InfoBadgeDisplayMode.Value
    )
    info_badge.setStatus(InfoBadge.InfoBadgeStatus.Attention)
    badge_row.addWidget(avatar)
    badge_row.addWidget(badge_label)
    badge_row.addStretch()
    badge_row.addWidget(info_badge)
    right_layout.addLayout(badge_row)

    shimmer = Shimmer(right)
    shimmer.setAnimationEnabled(False)
    shimmer.setShimmerProgress(0.35)
    shimmer.setShimmerTemplate(
        Shimmer.ShimmerTemplate.AvatarTextRow
    )
    shimmer.setFixedHeight(56)
    right_layout.addWidget(shimmer)

    line_edit = LineEdit(right)
    line_edit.setPlaceholderText("Type a message")
    right_layout.addWidget(line_edit)

    password = PasswordBox(right)
    password.setHeader("Password")
    password.setPlaceholderText("Enter password")
    right_layout.addWidget(password)

    number = NumberBox(right)
    number.setHeader("Quantity")
    number.setRange(0.0, 10.0)
    number.setValue(1.0)
    number.setSpinButtonPlacementMode(
        NumberBox.SpinButtonPlacementMode.Inline
    )
    right_layout.addWidget(number)

    progress_bar = ProgressBar(right)
    progress_bar.setValue(60)
    progress_ring = ProgressRing(right)
    progress_ring.setIsIndeterminate(False)
    progress_ring.setIsActive(True)
    progress_ring.setBackgroundVisible(True)
    progress_ring.setValue(60)
    progress_row = QHBoxLayout()
    progress_row.addWidget(progress_bar, 1)
    progress_row.addWidget(progress_ring)
    right_layout.addLayout(progress_row)

    scroll_bar = ScrollBar(Qt.Horizontal, right)
    scroll_bar.setRange(0, 100)
    scroll_bar.setPageStep(20)
    scroll_bar.setValue(60)
    scroll_bar.setOpacity(1.0)
    right_layout.addWidget(scroll_bar)

    pager = PipsPager(right)
    pager.setNumberOfPages(7)
    pager.setMaxVisiblePips(5)
    pager.setSelectedPageIndex(4)
    pager.setSelectionAnimationEnabled(False)
    pager.setPreviousButtonVisibility(
        PipsPager.PipsPagerButtonVisibility.Visible
    )
    pager.setNextButtonVisibility(
        PipsPager.PipsPagerButtonVisibility.Visible
    )
    right_layout.addWidget(pager)

    card = Card(right)
    card_layout = QVBoxLayout(card)
    card_layout.setContentsMargins(12, 10, 12, 10)
    card_layout.addWidget(Label("Native Card surface", card))
    right_layout.addWidget(card)

    expander = Expander(right)
    expander.setHeaderText("Hosted content")
    expander.setAnimationEnabled(False)
    expander_content = QWidget()
    expander_layout = QVBoxLayout(expander_content)
    expander_layout.setContentsMargins(12, 10, 12, 12)
    expander_layout.addWidget(
        Label("Owned by the Expander facade", expander_content)
    )
    expander.setOwnedContentWidget(expander_content)
    expander.setExpanded(True)
    right_layout.addWidget(expander)
    right_layout.addStretch()

    columns.addWidget(left, 1)
    columns.addWidget(vertical_divider)
    columns.addWidget(right, 1)
    root.addLayout(columns, 1)
    root.addWidget(Divider(content))

    status = Label(
        "Self-check passed: versions match, wrappers are valid, and signals "
        "are connected.",
        content,
    )
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    root.addWidget(status)

    click_count = {"value": 0}

    def count_repeat_click() -> None:
        click_count["value"] += 1
        repeat_count.setText(f"Clicks: {click_count['value']}")

    def toggle_theme() -> None:
        dark = fluentqt.current_theme() != fluentqt.Theme.Dark
        fluentqt.set_theme(
            fluentqt.Theme.Dark if dark else fluentqt.Theme.Light
        )
        theme_button.setText("Theme: Dark" if dark else "Theme: Light")

    accents = [
        ("Blue", QColor("#0067c0")),
        ("Purple", QColor("#7f52ff")),
        ("Green", QColor("#0f7b0f")),
    ]
    accent_index = {"value": 0}

    def cycle_accent() -> None:
        accent_index["value"] = (accent_index["value"] + 1) % len(accents)
        name, color = accents[accent_index["value"]]
        fluentqt.set_accent_color(color)
        accent_button.setText(f"Accent: {name}")

    repeat_button.clicked.connect(count_repeat_click)
    theme_button.clicked.connect(toggle_theme)
    accent_button.clicked.connect(cycle_accent)
    check_box.toggled.connect(shimmer.setAnimationEnabled)
    slider.valueChanged.connect(progress_bar.setValue)
    slider.valueChanged.connect(progress_ring.setValue)
    slider.valueChanged.connect(scroll_bar.setValue)
    slider.valueChanged.connect(
        lambda value: pager.setSelectedPageIndex(value * 6 // 100)
    )

    controls = [
        theme_button,
        accent_button,
        settings_icon,
        compound,
        check_box,
        radio_button,
        rating,
        toggle_button,
        toggle_switch,
        slider,
        repeat_button,
        hyperlink,
        text_edit,
        line_edit,
        password,
        number,
        avatar,
        info_badge,
        info_bar,
        info_action,
        progress_bar,
        progress_ring,
        pager,
        scroll_bar,
        shimmer,
        card,
        expander,
        vertical_divider,
    ]
    invalid = [
        type(control).__name__
        for control in controls
        if not Shiboken.isValid(control)
    ]
    if invalid:
        raise RuntimeError(f"Invalid FluentQt wrappers: {', '.join(invalid)}")

    window.setContentWidget(content)
    return window, controls


def main() -> int:
    args = parse_args()
    info = validate_runtime()
    app = QApplication.instance() or QApplication(sys.argv)
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    window, _controls = build_showcase(info)
    window.show()

    print(f"fluentqt package: {Path(fluentqt.__file__).resolve()}")
    print(f"native extension: {Path(native.__file__).resolve()}")
    print(
        "runtime: FluentQt {0} / PySide6 {1} / Qt {2}".format(
            info["fluentqt_version"],
            info["pyside6_version"],
            info["qt_runtime_version"],
        )
    )

    if args.snapshot is not None:
        snapshot_path = args.snapshot.expanduser().resolve()

        def capture() -> None:
            snapshot_path.parent.mkdir(parents=True, exist_ok=True)
            image = window.grab()
            if image.isNull() or not image.save(str(snapshot_path), "PNG"):
                print(
                    f"Unable to save snapshot: {snapshot_path}",
                    file=sys.stderr,
                )
                app.exit(2)
                return
            print(f"snapshot: {snapshot_path}")
            app.quit()

        QTimer.singleShot(250, capture)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
