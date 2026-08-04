"""First-launch CoachMark tour matching the native C++ Gallery."""

from __future__ import annotations

from dataclasses import dataclass

import fluentqt
from PySide6.QtCore import (
    QEasingCurve,
    QEvent,
    QMargins,
    QObject,
    QPoint,
    QPropertyAnimation,
    QRect,
    QRectF,
    QSize,
    Qt,
    Property,
    Signal,
)
from PySide6.QtGui import QColor, QFont, QPainter, QPainterPath
from PySide6.QtWidgets import QHBoxLayout, QVBoxLayout, QWidget

from .foundation_pages import _theme_tokens


class _IntroScrim(QWidget):
    def __init__(self, parent: QWidget) -> None:
        super().__init__(parent)
        self.setObjectName("GalleryIntroTour.Scrim")
        self.setAttribute(Qt.WA_NoSystemBackground, True)
        self.setAutoFillBackground(False)
        self.setFocusPolicy(Qt.NoFocus)
        self._progress = 0.0
        self._spotlight_enabled = False
        self._spotlight_rect = QRect()
        self._spotlight_radius = 8
        self._surface_radius = 0.0

    def _get_progress(self) -> float:
        return self._progress

    def _set_progress(self, progress: float) -> None:
        self._progress = max(0.0, min(float(progress), 1.0))
        self.update()

    progress = Property(float, _get_progress, _set_progress)

    def _get_spotlight_rect(self) -> QRect:
        return QRect(self._spotlight_rect)

    def _set_spotlight_rect(self, rect: QRect) -> None:
        self._spotlight_rect = QRect(rect)
        self.update()

    spotlightRect = Property(QRect, _get_spotlight_rect, _set_spotlight_rect)

    def set_spotlight_enabled(self, enabled: bool) -> None:
        self._spotlight_enabled = bool(enabled)
        self.update()

    def set_surface_radius(self, radius: float) -> None:
        self._surface_radius = max(0.0, float(radius))
        self.update()

    def paintEvent(self, event) -> None:
        del event
        color = QColor(0, 0, 0)
        color.setAlphaF(0.40 * self._progress)
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        surface = QPainterPath()
        if self._surface_radius > 0.0:
            surface.addRoundedRect(
                QRectF(self.rect()),
                self._surface_radius,
                self._surface_radius,
            )
        else:
            surface.addRect(QRectF(self.rect()))
        if not self._spotlight_enabled or self._spotlight_rect.isEmpty():
            painter.fillPath(surface, color)
            return
        hole = QPainterPath()
        hole.addRoundedRect(
            QRectF(self._spotlight_rect),
            self._spotlight_radius,
            self._spotlight_radius,
        )
        painter.fillPath(surface.subtracted(hole), color)

    def mousePressEvent(self, event) -> None:
        event.accept()

    def mouseReleaseEvent(self, event) -> None:
        event.accept()

    def mouseMoveEvent(self, event) -> None:
        event.accept()

    def wheelEvent(self, event) -> None:
        event.accept()


@dataclass
class TourStep:
    target: QWidget | None
    glyph: str
    title: str
    body: str
    placement: object = fluentqt.CoachMark.Placement.Auto
    centered: bool = False


class GalleryIntroTour(QObject):
    finished = Signal()

    _CARD_SIZE = QSize(330, 168)

    def __init__(self, host: QWidget, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._host = host
        self._steps: list[TourStep] = []
        self._index = -1
        self._finished = False
        self._have_spot = False
        self._scrim: _IntroScrim | None = None
        self._card: fluentqt.CoachMark | None = None
        self._dim_animation: QPropertyAnimation | None = None
        self._spot_animation: QPropertyAnimation | None = None

    def set_steps(self, steps: list[TourStep]) -> None:
        self._steps = list(steps)

    def _build(self) -> None:
        if self._card is not None:
            return
        window = self._host.window()
        scrim = _IntroScrim(window)
        scrim.set_surface_radius(
            float(window.property("fluentClientSideFrameRadius") or 0.0)
        )
        self._scrim = scrim

        card = fluentqt.CoachMark(window)
        card.setObjectName("GalleryIntroTour.Card")
        card.setCardSize(self._CARD_SIZE)
        self._card = card
        content = card.contentHost()
        root = QVBoxLayout(content)
        root.setContentsMargins(18, 14, 14, 14)
        root.setSpacing(8)

        header = QHBoxLayout()
        header.setSpacing(10)
        glyph = fluentqt.Label("", content)
        glyph_font = QFont("FluentQt Icons")
        glyph_font.setPixelSize(22)
        glyph.setFont(glyph_font)
        glyph.setStyleSheet(
            "color: {0};".format(
                QColor(_theme_tokens()["textAccentPrimary"]).name()
            )
        )
        glyph.setFixedWidth(26)
        glyph.setAlignment(Qt.AlignTop | Qt.AlignHCenter)
        header.addWidget(glyph, 0, Qt.AlignTop)
        title = fluentqt.Label("", content)
        title.setFluentTypography(fluentqt.FontRole.BodyStrong)
        title.setWordWrap(True)
        header.addWidget(title, 1)
        close = fluentqt.Button("", content)
        close.setObjectName("GalleryIntroTour.CloseButton")
        close.setFluentStyle(fluentqt.Button.ButtonStyle.Subtle)
        close.setFluentLayout(fluentqt.Button.ButtonLayout.IconOnly)
        close.setFluentSize(fluentqt.Button.ButtonSize.Small)
        close.setIconGlyph("\ue8bb", 16)
        close.setFixedSize(28, 28)
        close.setFocusPolicy(Qt.NoFocus)
        close.setToolTip("Skip tour")
        header.addWidget(close, 0, Qt.AlignTop)
        root.addLayout(header)

        body = fluentqt.Label("", content)
        body.setFluentTypography(fluentqt.FontRole.Body)
        body.setWordWrap(True)
        root.addWidget(body, 1)

        footer = QHBoxLayout()
        footer.setSpacing(8)
        counter = fluentqt.Label("", content)
        counter.setFluentTypography(fluentqt.FontRole.Caption)
        footer.addWidget(counter)
        footer.addStretch(1)
        previous = fluentqt.Button("Previous", content)
        previous.setFluentStyle(fluentqt.Button.ButtonStyle.Standard)
        previous.setFocusPolicy(Qt.NoFocus)
        next_button = fluentqt.Button("Next", content)
        next_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
        next_button.setFocusPolicy(Qt.NoFocus)
        footer.addWidget(previous)
        footer.addWidget(next_button)
        root.addLayout(footer)

        self._glyph = glyph
        self._title = title
        self._body = body
        self._counter = counter
        self._previous = previous
        self._next = next_button
        self._close = close
        close.clicked.connect(self.finish_tour)
        previous.clicked.connect(
            lambda: self.go_to_step(self._index - 1)
        )
        next_button.clicked.connect(self._advance)
        window.installEventFilter(self)

        dim = QPropertyAnimation(scrim, b"progress", self)
        dim.setDuration(250)
        dim.setEasingCurve(QEasingCurve.OutCubic)
        self._dim_animation = dim
        spot = QPropertyAnimation(scrim, b"spotlightRect", self)
        spot.setDuration(250)
        spot.setEasingCurve(QEasingCurve.OutCubic)
        self._spot_animation = spot

    def _advance(self) -> None:
        if self._index + 1 >= len(self._steps):
            self.finish_tour()
        else:
            self.go_to_step(self._index + 1)

    def start(self) -> None:
        self._steps = [
            step
            for step in self._steps
            if step.centered or step.target is not None
        ]
        if not self._steps:
            self.finished.emit()
            return
        self._build()
        self._host.window().setChromeInteractive(False)
        self._sync_scrim_geometry()
        self._scrim.show()
        self._scrim.raise_()
        self._dim_animation.stop()
        self._dim_animation.setStartValue(0.0)
        self._dim_animation.setEndValue(1.0)
        self._dim_animation.start()
        self._index = 0
        self._apply_step(0, False)
        self._card.open()
        self._card.raise_()

    def _surface_rect(self) -> QRect:
        window = self._host.window()
        value = window.property("fluentOverlaySurfaceRect")
        if isinstance(value, QRect):
            surface = value.intersected(window.rect())
            if not surface.isEmpty():
                return surface
        return window.rect()

    def _sync_scrim_geometry(self) -> None:
        if self._scrim is None:
            return
        self._scrim.setGeometry(self._surface_rect())
        self._scrim.set_surface_radius(
            float(
                self._host.window().property(
                    "fluentClientSideFrameRadius"
                )
                or 0.0
            )
        )
        if self._have_spot and 0 <= self._index < len(self._steps):
            step = self._steps[self._index]
            if not step.centered and step.target is not None:
                self._scrim.spotlightRect = self._spotlight_rect(step.target)

    def _spotlight_rect(self, target: QWidget) -> QRect:
        window = self._host.window()
        surface = self._surface_rect()
        in_window = QRect(target.mapTo(window, QPoint(0, 0)), target.size())
        return (
            in_window.translated(-surface.topLeft())
            .marginsAdded(QMargins(1, 1, 1, 1))
            .intersected(QRect(QPoint(0, 0), surface.size()))
        )

    def _apply_spotlight(self, step: TourStep, animate: bool) -> None:
        if self._scrim is None or self._spot_animation is None:
            return
        self._spot_animation.stop()
        if step.centered or step.target is None:
            self._scrim.set_spotlight_enabled(False)
            self._have_spot = False
            return
        target = self._spotlight_rect(step.target)
        self._scrim.set_spotlight_enabled(True)
        if animate and self._have_spot:
            self._spot_animation.setStartValue(
                self._scrim.spotlightRect
            )
            self._spot_animation.setEndValue(target)
            self._spot_animation.start()
        else:
            self._scrim.spotlightRect = target
        self._have_spot = True

    def _apply_step(self, index: int, animate: bool) -> None:
        step = self._steps[index]
        self._glyph.setText(step.glyph)
        self._glyph.setVisible(bool(step.glyph))
        self._title.setText(step.title)
        self._body.setText(step.body)
        self._counter.setText("{0} / {1}".format(index + 1, len(self._steps)))
        self._previous.setVisible(index > 0)
        self._next.setText("Finish" if index + 1 == len(self._steps) else "Next")
        self._card.setPlacement(step.placement)
        self._card.setTarget(None if step.centered else step.target)
        self._apply_spotlight(step, animate)

    def go_to_step(self, index: int) -> None:
        if index < 0 or index >= len(self._steps):
            return
        self._index = index
        self._apply_step(index, True)
        self._card.raise_()

    def finish_tour(self) -> None:
        if self._finished:
            return
        self._finished = True
        window = self._host.window()
        window.removeEventFilter(self)
        window.setChromeInteractive(True)
        if self._card is not None:
            card = self._card
            card.closed.connect(card.deleteLater)
            card.close()
        if self._spot_animation is not None:
            self._spot_animation.stop()
        if self._scrim is not None and self._dim_animation is not None:
            scrim = self._scrim
            self._dim_animation.stop()
            self._dim_animation.setStartValue(scrim.progress)
            self._dim_animation.setEndValue(0.0)
            self._dim_animation.finished.connect(scrim.deleteLater)
            self._dim_animation.start()
        self.finished.emit()

    def eventFilter(self, watched, event) -> bool:
        if (
            self._scrim is not None
            and watched is self._host.window()
            and event.type() in (QEvent.Type.Resize, QEvent.Type.Move)
        ):
            self._sync_scrim_geometry()
            self._scrim.raise_()
            if self._card is not None and self._card.isOpen():
                self._card.raise_()
        return super().eventFilter(watched, event)


__all__ = ["GalleryIntroTour", "TourStep"]
