#!/usr/bin/env python3
"""Measure a native Spatial card under pointer motion, then at rest.

Run with the built package on PYTHONPATH and a real desktop session. Frame swaps
are renderer submissions, not a guarantee of frames displayed by the monitor.
"""

import argparse
import json
import math
from pathlib import Path
import platform
import statistics
import sys
import time

import fluentqt
from fluentqt.spatial import SpatialView
from PySide6.QtCore import QPoint, QPointF, QTimer, Qt, qVersion
from PySide6.QtGui import QMouseEvent, QPolygonF, QTransform, QVector3D
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication, QGraphicsView, QVBoxLayout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dark", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    fluentqt.prepare_high_dpi_application()
    app = QApplication([])
    fluentqt.initialize_resources()
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))
    if args.dark:
        fluentqt.set_theme(fluentqt.Theme.Dark)
    fluentqt.set_motion_mode(fluentqt.MotionMode.Full)
    view = SpatialView()
    view.setWindowTitle("Spatial Python performance check")
    view.resize(900, 560)
    card = fluentqt.Card()
    card.setFixedSize(340, 250)
    layout = QVBoxLayout(card)
    layout.setContentsMargins(24, 24, 24, 24)
    heading = fluentqt.Label("Project sync")
    toggle = fluentqt.ToggleSwitch()
    toggle.setOnContent("Automatic sync")
    toggle.setOffContent("Manual sync")
    progress = fluentqt.ProgressBar()
    progress.setValue(64)
    slider = fluentqt.Slider()
    slider.setValue(64)
    slider.valueChanged.connect(progress.setValue)
    for widget in (heading, toggle, progress, slider):
        layout.addWidget(widget)
    item = view.addOwnedWidget(card)
    item.setRotation(QVector3D(6, -10, 0))
    item.setSurfaceIntensity(.35)
    view.show()
    view.raise_()
    view.activateWindow()
    result = {"python": platform.python_version(), "qt": qVersion(),
              "platform": platform.platform(), "width": 900, "height": 560,
              "dpr": view.devicePixelRatioF(), "workload": "one native card, 4 controls"}
    phase = "warmup"
    frames = []
    start = cpu_start = 0.0
    tick = QTimer()
    tick.setInterval(16)
    tick.setTimerType(Qt.PreciseTimer)

    def frame():
        if phase in ("moving", "idle"):
            frames.append(time.perf_counter())

    def record(name):
        elapsed = time.perf_counter() - start
        intervals = [(b - a) * 1000 for a, b in zip(frames, frames[1:])]
        result[name] = {"seconds": round(elapsed, 3), "frames": len(frames),
                        "fps": round(len(frames) / elapsed, 1),
                        "cpu_percent_one_core": round(100 * (time.process_time() - cpu_start) / elapsed, 1)}
        if intervals:
            result[name]["frame_interval_median_ms"] = round(statistics.median(intervals), 2)
            result[name]["frame_interval_p95_ms"] = round(sorted(intervals)[int(.95 * (len(intervals) - 1))], 2)

    def move():
        angle = (time.perf_counter() - start) * 3
        canvas = view.findChild(QGraphicsView).viewport()
        position = QPointF(canvas.width() * (.5 + .35 * math.sin(angle)),
                           canvas.height() * (.5 + .3 * math.cos(angle)))
        event = QMouseEvent(QMouseEvent.MouseMove, position, position,
                            canvas.mapToGlobal(position.toPoint()), Qt.NoButton,
                            Qt.NoButton, Qt.NoModifier)
        QApplication.sendEvent(canvas, event)

    def moving():
        nonlocal phase, start, cpu_start
        if not view.isActiveWindow():
            result["error"] = "Activate the benchmark window before measuring pointer following"
            finish()
            return
        result["backend"] = view.activeBackend().name
        result["renderer"] = view.rendererName()
        result["fallback"] = view.fallbackReason()
        surface = view.findChild(QOpenGLWidget)
        if not surface or view.activeBackend() != SpatialView.Backend.OpenGL:
            result["error"] = "OpenGL viewport did not initialize"
            finish()
            return
        surface.frameSwapped.connect(frame)
        phase = "moving"
        start, cpu_start = time.perf_counter(), time.process_time()
        tick.timeout.connect(move)
        tick.start()
        QTimer.singleShot(3000, settle)

    def settle():
        nonlocal phase
        tick.stop()
        record("moving")
        phase = "settle"
        QTimer.singleShot(1200, idle)

    def idle():
        nonlocal phase, start, cpu_start
        frames.clear()
        phase = "idle"
        start, cpu_start = time.perf_counter(), time.process_time()
        QTimer.singleShot(1500, complete)

    def complete():
        nonlocal phase
        record("idle")
        phase = "complete"
        view.findChild(QOpenGLWidget).grabFramebuffer().save(str(args.output / "python-spatial.png"))
        source = QPolygonF([QPointF(0, 0), QPointF(card.width(), 0),
                            QPointF(card.width(), card.height()), QPointF(0, card.height())])
        quad = item.projectedPolygon()
        transform = QTransform()
        QTransform.quadToQuad(source, QPolygonF([quad[i] for i in range(4)]), transform)
        before = toggle.isOn()
        position = transform.map(QPointF(toggle.mapTo(card, QPoint(20, toggle.height() // 2)))).toPoint()
        QTest.mouseClick(view.findChild(QGraphicsView).viewport(), Qt.LeftButton, pos=position)
        result["projected_click"] = toggle.isOn() != before
        if not result["projected_click"]:
            result["error"] = "The projected switch did not receive its click"
        view.setSpatialEnabled(False)
        result["flat_preserves_value"] = slider.value() == progress.value() == 64
        view.setSpatialEnabled(True)
        result["restored_3d"] = view.isSpatialEnabled()
        finish()

    def finish():
        (args.output / "python-spatial.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result, indent=2))
        view.close()
        app.exit(1 if "error" in result else 0)

    QTimer.singleShot(1500, moving)
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
