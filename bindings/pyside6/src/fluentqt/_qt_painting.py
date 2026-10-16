"""Private paint-device metric adaptation matching TextPaintCompat.h."""

import math

from PySide6.QtCore import Qt
from PySide6.QtGui import QFont, QPaintDevice, QPaintEngine

__all__ = [
    "device_pixel_ratio_metric",
    "font_style_strategy",
    "forward_paint_array",
    "forward_paint_polygon",
    "forwarding_paint_engine_features",
    "image_device_pixel_ratio",
    "region_rects",
]


def font_style_strategy(*strategies: QFont.StyleStrategy) -> QFont.StyleStrategy:
    """Combine flags into a Qt enum on both legacy and current PySide6."""
    combined = 0
    for strategy in strategies:
        combined |= int(getattr(strategy, "value", strategy))
    return QFont.StyleStrategy(combined)


def region_rects(region):
    return region.rects() if hasattr(region, "rects") else region


def forward_paint_array(painter, method, values, count, *arguments):
    if count:
        # PySide6 6.2 exposes native pointer/count pairs only on QPaintEngine.
        # Forward them intact; QPainter's sequence overload cannot consume them.
        engine = painter.paintEngine()
        engine.syncState()
        getattr(engine, method)(values, *count, *arguments)
    else:
        getattr(painter, method)(values, *arguments)


def forward_paint_polygon(painter, points, *arguments):
    count, mode = arguments[:-1], arguments[-1]
    if count:
        forward_paint_array(painter, "drawPolygon", points, count, mode)
    elif mode == QPaintEngine.PolylineMode:
        painter.drawPolyline(points)
    elif mode == QPaintEngine.ConvexMode:
        painter.drawConvexPolygon(points)
    else:
        painter.drawPolygon(points, Qt.WindingFill if mode == QPaintEngine.WindingMode
                            else Qt.OddEvenFill)


_ENCODED_DPR_METRICS = frozenset(
    value
    for value in (
        getattr(QPaintDevice, "PdmDevicePixelRatioF_EncodedA", None),
        getattr(QPaintDevice, "PdmDevicePixelRatioF_EncodedB", None),
    )
    if value is not None
)


def _qt_round(value: float) -> int:
    # Match qRound's half-away-from-zero rule, including .5 pixel ratios.
    return math.floor(value + 0.5) if value >= 0 else math.ceil(value - 0.5)


def image_device_pixel_ratio(dpr: float) -> float:
    """Return the DPR precision used by QImage on the active runtime."""
    scale = QPaintDevice.devicePixelRatioFScale()
    scaled = math.floor(dpr * scale)
    if not _ENCODED_DPR_METRICS or scaled in (scale, 2 * scale):
        return scaled / scale
    return dpr


def device_pixel_ratio_metric(metric, dpr: float) -> int:
    if metric == QPaintDevice.PdmDevicePixelRatio:
        return _qt_round(dpr)
    if metric == QPaintDevice.PdmDevicePixelRatioScaled:
        scale = QPaintDevice.devicePixelRatioFScale()
        scaled = _qt_round(dpr * scale)
        # Qt's 1x/2x shortcut must not bypass encoded fractional precision.
        if _ENCODED_DPR_METRICS and scaled in (scale, 2 * scale) and dpr != scaled / scale:
            return scaled + (-1 if dpr < scaled / scale else 1)
        return scaled
    if metric in _ENCODED_DPR_METRICS:
        return QPaintDevice.encodeMetricF(metric, dpr)
    return 0


def forwarding_paint_engine_features(engine):
    features = QPaintEngine.PaintEngineFeatures(0)
    if engine is not None:
        # Bit 31 is not a Qt feature and overflows PySide6 6.2's signed enum converter.
        for bit in range(31):
            feature = QPaintEngine.PaintEngineFeature(1 << bit)
            if engine.hasFeature(feature):
                features |= feature
    return features
