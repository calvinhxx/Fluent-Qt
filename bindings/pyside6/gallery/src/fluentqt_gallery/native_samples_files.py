"""File entry, caller-owned file models, and actionable feedback."""
from textwrap import dedent
from .native_samples import register_source_samples


def _script(body):
    return (
        "import fluentqt\n"
        "from PySide6.QtCore import QFileInfo, Qt\n"
        "from PySide6.QtGui import QAction, QStandardItem, QStandardItemModel\n"
        "from PySide6.QtWidgets import QFileDialog, QHBoxLayout, QVBoxLayout, QWidget\n\n"
        + dedent(body).strip() + "\n"
    )


_MODEL = '''
panel = QWidget()
layout = QVBoxLayout(panel)
layout.setContentsMargins(0, 0, 0, 0)
layout.setSpacing(24)
model = QStandardItemModel(panel)
roles = fluentqt.FileListView.DataRole
states = fluentqt.FileListView.Status

def add_file(name, detail, state, progress):
    item = QStandardItem(name)
    item.setData(detail, roles.MetadataRole)
    item.setData(state, roles.StatusRole)
    item.setData(progress, roles.ProgressRole)
    model.appendRow(item)

add_file("brand-guidelines.pdf", "2.4 MB · Ready to use", states.Ready, 1.0)
add_file("product-shot.png", "3.6 / 5.8 MB · Uploading", states.Uploading, 0.62)
'''

_VIEW = '''
files = fluentqt.FileListView(panel)
files.setObjectName("selectedFiles")
files.setAccessibleName("Selected files")
files.setBackgroundVisible(False)
files.setBorderVisible(False)
files.setProperty("fluentPreserveParentSurface", True)
files.viewport().setProperty("fluentPreserveParentSurface", True)
files.setMinimumHeight(260)
files.setModel(model)
layout.addWidget(files)
files.removeRequested.connect(lambda index: model.removeRow(index.row()))
'''

register_source_samples("file-drop-zone", ("FileDropZone", "FileListView", "Label"), {
    "file-drop-zone-workspace": ("panel", _script(_MODEL + '''
add_file("archive.zip", "28 MB · Exceeds the 20 MB limit", states.Rejected, 0.0)
drop = fluentqt.FileDropZone(panel)
drop.setObjectName("fileDropZone")
drop.setDescription("Add documents, images or a ZIP archive.")
drop.setHintText("PDF, PNG, JPG, ZIP · Up to 20 MB per file")
layout.addWidget(drop)
heading = fluentqt.Label("Files", panel)
heading.setFluentTypography(fluentqt.FontRole.BodyStrong)
layout.addWidget(heading)
''' + _VIEW + '''
def accept_file(path):
    file = QFileInfo(path)
    if not file.isFile():
        return
    type_allowed = file.suffix().lower() in ("pdf", "png", "jpg", "jpeg", "zip")
    size_allowed = file.size() <= 20 * 1024 * 1024
    if not type_allowed or not size_allowed:
        reason = "Choose a file under 20 MB."
        if not type_allowed:
            reason = "This file type is not supported."
        drop.setErrorMessage(file.fileName() + " · " + reason)
        add_file(file.fileName(), reason, states.Rejected, 0.0)
        return
    drop.clearError()
    detail = f"{file.size() / (1024 * 1024):.1f} MB · Ready to use"
    add_file(file.fileName(), detail, states.Ready, 1.0)

def dropped(urls):
    for url in urls:
        accept_file(url.toLocalFile())

def browse():
    dialog = QFileDialog(panel, "Choose files")
    dialog.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose)
    dialog.setFileMode(QFileDialog.FileMode.ExistingFiles)
    dialog.setNameFilter("Documents (*.pdf *.png *.jpg *.jpeg *.zip)")
    dialog.filesSelected.connect(lambda paths: [accept_file(path) for path in paths])
    dialog.open()
    panel._file_dialog = dialog

drop.filesDropped.connect(dropped)
drop.browseRequested.connect(browse)
''')),
    "file-drop-zone-states": ("panel", _script('''
        panel = QWidget()
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(24)
        rejected = fluentqt.FileDropZone(panel)
        rejected.setObjectName("rejectedDropZone")
        rejected.setTitle("This file is too large")
        rejected.setBrowseText("Reset example")
        rejected.setErrorMessage("archive.zip is 28 MB. Choose a file under 20 MB.")
        def reset():
            rejected.clearError()
            rejected.setTitle("Drop files here")
        rejected.browseRequested.connect(reset)
        layout.addWidget(rejected)
        disabled = fluentqt.FileDropZone(panel)
        disabled.setObjectName("disabledDropZone")
        disabled.setTitle("File selection is unavailable")
        disabled.setDescription("You need permission to add files to this workspace.")
        disabled.setEnabled(False)
        layout.addWidget(disabled)
    ''')),
})

register_source_samples("file-list-view", ("FileListView", "Button"), {
    "file-list-view-basic": ("panel", _script(_MODEL + '''
add_file("research-notes-for-the-autumn-product-release-and-accessibility-review.pdf",
         "Connection lost · Retry when you are ready", states.Rejected, 0.0)
model.item(2).setData(True, roles.RetryableRole)
''' + _VIEW + '''
panel.setMaximumWidth(720)
layout.setSpacing(16)
files.setObjectName("fileList")
files.setAccessibleName("Transfer examples")
files.setMinimumHeight(240)

def retry(index):
    model.setData(index, states.Uploading, roles.StatusRole)
    model.setData(index, False, roles.RetryableRole)
    model.setData(index, "Reconnected · Uploading", roles.MetadataRole)
    model.setData(index, 0.0, roles.ProgressRole)

files.retryRequested.connect(retry)
advance = fluentqt.Button("Advance example transfers", panel)
advance.setObjectName("advanceTransfer")
advance.setFluentStyle(fluentqt.Button.ButtonStyle.Subtle)

def advance_transfers():
    for row in range(model.rowCount()):
        index = model.index(row, 0)
        if index.data(roles.StatusRole) != states.Uploading:
            continue
        progress = min(1.0, float(index.data(roles.ProgressRole)) + 0.19)
        model.setData(index, progress, roles.ProgressRole)
        model.setData(index, "Ready to use" if progress >= 1 else "Uploading", roles.MetadataRole)
        if progress >= 1:
            model.setData(index, states.Ready, roles.StatusRole)

advance.clicked.connect(advance_transfers)
layout.addWidget(advance, 0, Qt.AlignmentFlag.AlignLeft)
''')),
})

register_source_samples("toast", ("Toast", "Button"), {
    "toast-feedback": ("panel", _script('''
        panel = QWidget()
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(12)
        retry = QAction("Try again", panel)
        retry.triggered.connect(lambda: fluentqt.Toast.showToast(
            panel, "Ready to retry", fluentqt.Toast.Severity.Success, 2200,
            fluentqt.Toast.Placement.TopEnd))
        review = QAction("Review file", panel)
        review.triggered.connect(lambda: fluentqt.Toast.showToast(
            panel, "Choose a file under 20 MB", fluentqt.Toast.Severity.Informational,
            3500, fluentqt.Toast.Placement.TopEnd))
        row = QWidget(panel)
        buttons = QHBoxLayout(row)
        buttons.setContentsMargins(0, 0, 0, 0)
        buttons.setSpacing(8)
        panel._feedback_toast = None
        severity = fluentqt.Toast.Severity

        def show_feedback(kind):
            if panel._feedback_toast is not None:
                panel._feedback_toast.dismiss()
            toast = fluentqt.Toast(panel)
            toast.setObjectName("feedbackToast")
            toast.setDuration(5500)
            toast.setPlacement(fluentqt.Toast.Placement.TopEnd)
            toast.setClosable(True)
            toast.setPauseOnHoverEnabled(True)
            if kind in (0, 1):
                toast.setSeverity(severity.Success if kind == 0 else severity.Informational)
                toast.setMessage("Changes saved" if kind == 0 else "Link copied")
            elif kind == 2:
                toast.setSeverity(fluentqt.Toast.Severity.Warning)
                toast.setTitle("One file needs attention")
                toast.setMessage("archive.zip exceeds the 20 MB limit. The other files can continue.")
                toast.setAction(review)
            else:
                toast.setSeverity(fluentqt.Toast.Severity.Error)
                toast.setTitle("Upload interrupted")
                toast.setMessage("Your connection was lost. Try again to resume product-shot.png.")
                toast.setAction(retry)
            def dismissed():
                if panel._feedback_toast is toast:
                    panel._feedback_toast = None
                toast.deleteLater()
            toast.dismissed.connect(dismissed)
            panel.destroyed.connect(toast.deleteLater)
            toast.present(panel)
            panel._feedback_toast = toast

        for i, label in enumerate(("Saved", "Copied", "Warning", "Error")):
            button = fluentqt.Button(label, row)
            button.setObjectName(f"feedback{i}")
            button.clicked.connect(lambda checked=False, i=i: show_feedback(i))
            buttons.addWidget(button)
        buttons.addStretch()
        layout.addWidget(row)
    ''')),
})
