"""Runtime identity for the standalone FluentQt PySide6 Gallery.

The Python and native C++ Galleries intentionally keep the same visible
window title for parity review, but they are separate applications.  Distinct
runtime identity prevents their single-instance locks, settings, and desktop
grouping from colliding while both implementations are open.
"""

APPLICATION_ID = "com.fluentqt.gallery.pyside6"
APPLICATION_NAME = "Fluent-Qt Gallery (Python)"
ORGANIZATION_NAME = "Fluent-Qt"


__all__ = [
    "APPLICATION_ID",
    "APPLICATION_NAME",
    "ORGANIZATION_NAME",
]
