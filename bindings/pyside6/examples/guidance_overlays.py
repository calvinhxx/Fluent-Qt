"""Interactive and snapshot acceptance example for guidance overlays."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QSize, QTimer, Qt
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt guidance overlay example."
    )
    parser.add_argument(
        "--snapshot",
        type=Path,
        help="Open both overlays, save the window as PNG, and exit.",
    )
    parser.add_argument(
        "--auto-close-ms",
        type=int,
        default=0,
        help="Automatically close the interactive window after this delay.",
    )
    return parser.parse_args()


def build_showcase() -> tuple[
    fluentqt.Window,
    fluentqt.CoachMark,
    fluentqt.TeachingTip,
]:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 Guidance Overlays")
    window.resize(960, 680)

    page = QWidget()
    root = QVBoxLayout(page)
    root.setContentsMargins(36, 30, 36, 30)
    root.setSpacing(14)

    title = fluentqt.Label("Guidance overlays from Python", page)
    title.setFluentTypography(fluentqt.FontRole.TitleLarge)
    root.addWidget(title)

    summary = fluentqt.Label(
        "Python supplies targets and content. Native FluentQt keeps both "
        "surfaces inside the owning Window and handles placement, tails, "
        "animation, close reasons, theme inheritance, and painting.",
        page,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(fluentqt.Divider(page))

    stage = fluentqt.Card(page)
    stage.setMinimumHeight(410)
    stage_layout = QVBoxLayout(stage)
    stage_layout.setContentsMargins(28, 24, 28, 26)
    stage_layout.setSpacing(16)

    stage_title = fluentqt.Label("Choose a guidance surface", stage)
    stage_title.setFluentTypography(fluentqt.FontRole.Subtitle)
    stage_layout.addWidget(stage_title)

    stage_text = fluentqt.Label(
        "CoachMark follows a target while it moves. TeachingTip adds "
        "preferred placement, a configurable tail, light dismiss, and a "
        "semantic closing reason.",
        stage,
    )
    stage_text.setWordWrap(True)
    stage_layout.addWidget(stage_text)
    stage_layout.addStretch()

    targets = QHBoxLayout()
    targets.setSpacing(180)
    coach_button = fluentqt.Button("Show CoachMark", stage)
    coach_button.setFixedWidth(180)
    teaching_button = fluentqt.Button("Show TeachingTip", stage)
    teaching_button.setFixedWidth(180)
    teaching_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    targets.addStretch()
    targets.addWidget(coach_button)
    targets.addWidget(teaching_button)
    targets.addStretch()
    stage_layout.addLayout(targets)
    root.addWidget(stage)

    hint = fluentqt.Label(
        "Both targets remain caller-owned. Their Python wrappers are retained "
        "only while registered with the overlay.",
        page,
    )
    hint.setAlignment(Qt.AlignCenter)
    hint.setWordWrap(True)
    root.addWidget(hint)
    window.setContentWidget(page)

    coach = fluentqt.CoachMark(page)
    coach.setCardSize(QSize(300, 154))
    coach.setPlacement(fluentqt.CoachMark.Placement.Top)
    coach_host = coach.contentHost()
    coach_layout = QVBoxLayout(coach_host)
    coach_layout.setContentsMargins(20, 16, 20, 16)
    coach_layout.setSpacing(8)
    coach_title = fluentqt.Label("Keep work moving", coach_host)
    coach_title.setFluentTypography(fluentqt.FontRole.Subtitle)
    coach_body = fluentqt.Label(
        "This native CoachMark follows its Python target and remains inside "
        "the current Window.",
        coach_host,
    )
    coach_body.setWordWrap(True)
    coach_done = fluentqt.Button("Got it", coach_host)
    coach_layout.addWidget(coach_title)
    coach_layout.addWidget(coach_body)
    coach_layout.addStretch()
    coach_layout.addWidget(coach_done, 0, Qt.AlignRight)

    teaching = fluentqt.TeachingTip(page)
    teaching.setAnimationEnabled(False)
    teaching.setExitAnimationEnabled(False)
    teaching.setCardSize(QSize(330, 174))
    teaching.setPreferredPlacement(
        fluentqt.TeachingTip.PreferredPlacement.Top
    )
    teaching.setPlacementMargin(8)
    teaching.setTailVisible(True)
    teaching.setLightDismissEnabled(True)
    teaching_host = teaching.contentHost()
    teaching_layout = QVBoxLayout(teaching_host)
    teaching_layout.setContentsMargins(20, 16, 20, 16)
    teaching_layout.setSpacing(8)
    teaching_title = fluentqt.Label("Review before publishing", teaching_host)
    teaching_title.setFluentTypography(fluentqt.FontRole.Subtitle)
    teaching_body = fluentqt.Label(
        "TeachingTip reports why it closed, so Python can distinguish an "
        "action from light dismiss or target destruction.",
        teaching_host,
    )
    teaching_body.setWordWrap(True)
    teaching_action = fluentqt.Button("Review", teaching_host)
    teaching_action.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    teaching_layout.addWidget(teaching_title)
    teaching_layout.addWidget(teaching_body)
    teaching_layout.addStretch()
    teaching_layout.addWidget(teaching_action, 0, Qt.AlignRight)

    def toggle_coach() -> None:
        if coach.isOpen():
            coach.close()
            return
        coach.setTarget(coach_button)
        coach.open()

    def toggle_teaching() -> None:
        if teaching.isOpen():
            teaching.closeWithReason(
                fluentqt.TeachingTip.CloseReason.Programmatic
            )
            return
        teaching.showAt(teaching_button)

    coach_button.clicked.connect(toggle_coach)
    teaching_button.clicked.connect(toggle_teaching)
    coach_done.clicked.connect(coach.close)
    teaching_action.clicked.connect(
        lambda: teaching.closeWithReason(
            fluentqt.TeachingTip.CloseReason.ActionButton
        )
    )

    window._coach_mark = coach
    window._teaching_tip = teaching
    window._open_guidance = lambda: (
        coach.setTarget(coach_button),
        coach.open(),
        teaching.showAt(teaching_button),
    )
    return window, coach, teaching


def main() -> int:
    args = parse_args()
    app = QApplication.instance() or QApplication(sys.argv)
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    window, coach, teaching = build_showcase()
    app.aboutToQuit.connect(coach.close)
    app.aboutToQuit.connect(teaching.close)
    window.show()

    if args.snapshot is not None:
        snapshot_path = args.snapshot.expanduser().resolve()

        def open_overlays() -> None:
            window._open_guidance()

        def capture() -> None:
            if not coach.isOpen() or not teaching.isOpen():
                print("Guidance overlays did not open", file=sys.stderr)
                app.exit(2)
                return
            snapshot_path.parent.mkdir(parents=True, exist_ok=True)
            image = window.grab()
            if image.isNull() or not image.save(str(snapshot_path), "PNG"):
                print(
                    "Unable to save snapshot: {0}".format(snapshot_path),
                    file=sys.stderr,
                )
                app.exit(2)
                return
            print("snapshot: {0}".format(snapshot_path))
            app.quit()

        QTimer.singleShot(80, open_overlays)
        QTimer.singleShot(500, capture)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
