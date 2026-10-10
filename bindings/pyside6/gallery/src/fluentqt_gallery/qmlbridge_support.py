"""Detect the opt-in bridge without importing Qt Quick into a base Gallery."""

from importlib.util import find_spec

QML_BRIDGE_AVAILABLE = find_spec("fluentqt.qmlbridge") is not None
