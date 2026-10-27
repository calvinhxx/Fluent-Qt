"""PySide fixture adapter for the headless font policy in testing-workflow.md.

This is test support, not a Gallery/package API. The C++ fixture implements
the same policy in tests/support/QtTestEnvironment.cpp.
"""

import os

from PySide6.QtGui import QFont, QFontDatabase, QFontInfo


def initialize_headless_test_fixed_font(application):
    path = os.environ.get("FLUENT_QT_TEST_MONOSPACE_FONT", "")
    if not path or application.platformName() not in ("offscreen", "minimal"):
        return
    # Resolve before loading the font: loading invalidates Qt's fallback cache.
    fixed = QFontDatabase.systemFont(QFontDatabase.FixedFont)
    already_fixed = QFontInfo(fixed).fixedPitch()
    font_id = QFontDatabase.addApplicationFont(path)
    if font_id < 0:
        raise RuntimeError(f"Cannot load test monospace font: {path}")
    families = QFontDatabase.applicationFontFamilies(font_id)
    family = next((name for name in families
                   if QFontInfo(QFont(name)).fixedPitch()), None)
    if family is None:
        raise RuntimeError(f"Test font has no usable fixed-pitch family: {path}")
    if already_fixed:
        return
    if not fixed.family():
        raise RuntimeError("The headless system FixedFont has no family")
    QFont.insertSubstitution(fixed.family(), family)
    if not QFontInfo(QFontDatabase.systemFont(QFontDatabase.FixedFont)).fixedPitch():
        raise RuntimeError(f"Cannot resolve the headless system FixedFont using: {family}")
