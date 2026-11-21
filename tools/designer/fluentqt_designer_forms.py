"""Attach an editable Designer form body to its FluentQt owner."""
from PySide6.QtGui import QVector3D
from PySide6.QtWidgets import QDialogButtonBox, QVBoxLayout

from fluentqt.dialogs_flyouts import ContentDialog, Dialog
from fluentqt.windowing import Window


def add_spatial_form(view, body):
    item = view.addOwnedWidget(body)
    if item is None:
        raise ValueError("SpatialView could not add the form content")
    scale = body.property("fluentItemScale")
    if scale is not None:
        item.setScale(scale)
    item.setRotation(QVector3D(0, body.property("fluentItemRotationY") or 0, 0))
    return item


def make_form_host(body, owner=None):
    kind = body.property("fluentFormHost")
    title, size = body.windowTitle(), body.size()
    if not kind:
        return body
    if owner is body or (owner is not None and body.isAncestorOf(owner)):
        raise ValueError("Choose an owner outside the form body")
    if kind == "Window":
        host = Window(owner)
        host.setContentWidget(body)
    elif kind == "Dialog":
        host = Dialog(owner)
        QVBoxLayout(host).addWidget(body)
        buttons = body.findChild(QDialogButtonBox, "buttonBox")
        if buttons is not None:
            buttons.accepted.connect(host.accept)
            buttons.rejected.connect(host.reject)
    elif kind == "ContentDialog":
        host = ContentDialog(owner)
        host.setTitle(title)
        host.setPrimaryButtonText(body.property("fluentPrimaryText") or "")
        host.setCloseButtonText(body.property("fluentCloseText") or "")
        host.setContent(body)
    elif kind == "SpatialItem":
        from fluentqt.spatial import SpatialView
        host = SpatialView(owner)
        add_spatial_form(host, body)
    else:
        raise ValueError(f"Unknown FluentQt form host: {kind}")
    host.setWindowTitle(title)
    host.resize(size.expandedTo(host.sizeHint()))
    return host


def show_form_host(host):
    if isinstance(host, (Dialog, ContentDialog)):
        host.open()
    else:
        host.show()
