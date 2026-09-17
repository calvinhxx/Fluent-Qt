#!/usr/bin/env python3
"""Measure native chart projection plus QWidget.grab using a built PySide6 package.

Run with the matching interpreter and PYTHONPATH=<binding-build>/python.
The JSON records its actual platform, DPR and viewport; timings are diagnostic.
"""

import argparse
import json
import math
import platform
import statistics
import time
from pathlib import Path

from PySide6.QtCore import QPointF, qVersion
from PySide6.QtWidgets import QApplication
import fluentqt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--points", type=int, default=1_000_000)
    parser.add_argument("--iterations", type=int, default=30)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.points < 100 or args.iterations < 2:
        parser.error("Use at least 100 points and two iterations")

    app = QApplication([])
    fluentqt.initialize_resources()
    points = [QPointF(i, 50 + 20 * math.sin(i * .01)) for i in range(args.points)]
    start = time.perf_counter()
    snapshot = fluentqt.ChartData.fromPoints(points)
    index_ms = (time.perf_counter() - start) * 1000
    del points
    model = fluentqt.ChartModel()
    model.setDataSnapshot(snapshot)
    del snapshot  # Do not force copy-on-write during the append probe.
    chart = fluentqt.ChartView()
    chart.resize(1000, 400)
    chart.setModel(model)
    chart.show()
    app.processEvents()

    measurements = {}
    for name in ("Line", "Area"):
        chart.setChartType(getattr(fluentqt.ChartView.ChartType, name))
        chart.grab()  # Warm the presentation before timing viewport changes.
        samples = []
        for i in range(args.iterations):
            offset = args.points * .02 * i / args.iterations
            chart.setXRange(offset, args.points * .95 + offset)
            start = time.perf_counter()
            chart.grab()
            samples.append((time.perf_counter() - start) * 1000)
        measurements[name] = {
            "median_ms": statistics.median(samples),
            "p95_ms": sorted(samples)[math.ceil(len(samples) * .95) - 1],
            "representatives": chart.renderedPointCount(),
            "samples_ms": samples,
        }

    before = chart.projectionBuildCount()
    for i in range(100):
        model.appendPoints([QPointF(args.points + i, 50)])
    chart.grab()
    report = {
        "points": args.points,
        "qt": qVersion(),
        "platform": platform.platform(),
        "platform_plugin": app.platformName(),
        "logical_size": [chart.width(), chart.height()],
        "device_pixel_ratio": chart.devicePixelRatioF(),
        "conversion_and_index_ms": index_ms,
        "maximum_representatives": chart.maximumPointCount(),
        "burst_deferred_until_frame": chart.projectionBuildCount() == before,
        "projection_and_grab": measurements,
    }
    chart.close()
    serialized = json.dumps(report, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(serialized, encoding="utf-8")
    print(serialized, end="")


if __name__ == "__main__":
    main()
