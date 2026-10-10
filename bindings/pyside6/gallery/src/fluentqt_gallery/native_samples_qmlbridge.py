"""Atomic QmlView cards using the same QML assets as the C++ Gallery.

Source exports remain available to a base binding without importing Quick.
The standalone shared-model example is separate from these teaching cards.
"""

from pathlib import Path
from textwrap import dedent, indent

from .native_samples import native_samples, _result, _format_display_source


_IMPORTS = """import fluentqt
from fluentqt.qmlbridge import QmlSession, QmlView
from PySide6.QtCore import Qt
from PySide6.QtWidgets import QHBoxLayout, QSizePolicy, QVBoxLayout, QWidget

"""
_PANEL = """panel = QWidget(globals().get("gallery_parent"))
panel.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
column = QVBoxLayout(panel)
column.setContentsMargins(0, 0, 0, 0)
column.setSpacing(12)
column.setAlignment(Qt.AlignTop)
"""
_QML_FILES = {
    "qml-view-load": "Basic.qml",
    "qml-view-theme": "Theme.qml",
    "qml-view-context": "Context.qml",
    "qml-view-action": "Action.qml",
    "qml-view-lifecycle": "Basic.qml",
    "qml-view-error": "Basic.qml",
}


def _source(text):
    return dedent(text).strip() + "\n"


def _view_source(object_name, height=100):
    return _source(f"""
        session = QmlSession(panel, panel)
        view = session.createView(panel) if session.activate() else None
        if view is not None:
            view.setObjectName("{object_name}")
            view.setAccessibleName("QML preview")
            view.setFixedHeight({height})
            column.addWidget(view)
        else:
            column.addWidget(fluentqt.Label("QML is unavailable", panel))
    """)


_SCRIPTS = {}
_SCRIPTS["qml-view-load"] = _PANEL + _view_source("qmlBridgeLoadView") + _source("""
    if view is not None:
        view.load(qml_source("Basic"))
""")
_SCRIPTS["qml-view-theme"] = _PANEL + _view_source("qmlBridgeThemeView") + _source("""
    if view is not None:
        view.load(qml_source("Theme"))
""")
_SCRIPTS["qml-view-context"] = _PANEL + _source("""
    title_edit = fluentqt.LineEdit(panel)
    title_edit.setObjectName("qmlBridgeTitleEdit")
    title_edit.setAccessibleName("Widgets text")
    title_edit.setText("A shared title")
    column.addWidget(title_edit)
""") + _view_source("qmlBridgeContextView") + _source("""
    if view is not None and view.setContextObject("widgetInput", title_edit):
        view.load(qml_source("Context"))
""")
_SCRIPTS["qml-view-action"] = _PANEL + _source("""
    progress = fluentqt.ProgressBar(panel)
    progress.setObjectName("qmlBridgeProgressIndicator")
    progress.setAccessibleName("Widgets progress")
    progress.setRange(0, 100)
    progress.setValue(40)
    column.addWidget(progress)
""") + _view_source("qmlBridgeActionView", 180) + _source("""
    if view is not None and view.setContextObject("widgetProgress", progress):
        view.load(qml_source("Action"))
""")
_SCRIPTS["qml-view-lifecycle"] = _PANEL + _view_source("qmlBridgeLifecycleView") + _source("""
    actions = QWidget(panel)
    actions_layout = QHBoxLayout(actions)
    actions_layout.setContentsMargins(0, 0, 0, 0)
    actions_layout.setSpacing(12)
    actions_layout.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
    deactivate = fluentqt.Button("Deactivate", actions)
    deactivate.setObjectName("qmlBridgeDeactivate")
    reload = fluentqt.Button("Reload QML", actions)
    reload.setObjectName("qmlBridgeReload")
    actions_layout.addWidget(deactivate)
    actions_layout.addWidget(reload)
    column.addWidget(actions)
    status = fluentqt.Label("QML stopped", panel)
    status.setObjectName("qmlBridgeLifecycleStatus")
    column.addWidget(status)

    def update_status(state, label=status, ready=QmlView.Status.Ready):
        label.setText("QML loaded" if state == ready else "QML stopped")

    def reload_qml(*_unused):
        if view is not None and session.activate():
            view.unload()
            view.load(qml_source("Basic"))

    deactivate.clicked.connect(lambda: session.deactivate())
    reload.clicked.connect(reload_qml)
    if view is not None:
        view.statusChanged.connect(update_status)
        reload_qml()
    else:
        deactivate.setEnabled(False)
        reload.setEnabled(False)
""")
_SCRIPTS["qml-view-error"] = _PANEL + _view_source("qmlBridgeErrorView") + _source("""
    actions = QWidget(panel)
    actions_layout = QHBoxLayout(actions)
    actions_layout.setContentsMargins(0, 0, 0, 0)
    actions_layout.setSpacing(12)
    actions_layout.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
    missing = fluentqt.Button("Load missing QML", actions)
    missing.setObjectName("qmlBridgeLoadMissing")
    restore = fluentqt.Button("Restore QML", actions)
    restore.setObjectName("qmlBridgeRestore")
    actions_layout.addWidget(missing)
    actions_layout.addWidget(restore)
    column.addWidget(actions)
    status = fluentqt.Label("QML stopped", panel)
    status.setObjectName("qmlBridgeErrorStatus")
    status.setWordWrap(True)
    column.addWidget(status)

    def update_status(state, qml_view=view, label=status,
                      ready=QmlView.Status.Ready, error=QmlView.Status.Error):
        if state == ready:
            label.setText("QML loaded")
        elif state == error:
            label.setText(qml_view.lastError())
        else:
            label.setText("QML stopped")

    if view is not None:
        view.statusChanged.connect(update_status)
        missing.clicked.connect(lambda: view.load(qml_source("Missing")))
        restore.clicked.connect(lambda: view.load(qml_source("Basic")))
        view.load(qml_source("Basic"))
    else:
        missing.setEnabled(False)
        restore.setEnabled(False)
""")


def _qml_asset_path(name):
    filename = name if name.endswith(".qml") else name + ".qml"
    if filename not in {"Basic.qml", "Theme.qml", "Context.qml", "Action.qml", "Missing.qml"}:
        raise ValueError("Unknown atomic QML asset: " + filename)
    packaged = Path(__file__).with_name("assets") / "qml_bridge" / "atomic" / filename
    if packaged.parent.is_dir():
        return packaged
    for ancestor in Path(__file__).resolve().parents:
        source = ancestor / "app/view/widgets/samples/qmlbridge" / filename
        if source.parent.is_dir():
            return source
    raise FileNotFoundError("The atomic QML assets were not packaged")


def qml_source(name):
    """Return an atomic asset URL; Missing deliberately has no backing file."""
    from PySide6.QtCore import QUrl
    return QUrl.fromLocalFile(str(_qml_asset_path(name)))


_KEY_SOURCE = {
    "qml-view-load": _source("""
        from fluentqt.qmlbridge import QmlSession

        # main() calls QmlSession.prepareApplication() before QApplication.
        # panel and qml_source() are defined in Full example.
        session = QmlSession(panel, panel)
        view = session.createView(panel) if session.activate() else None
        if view is not None:
            view.setFixedHeight(100)
            view.load(qml_source("Basic"))
    """),
    "qml-view-theme": _source("""
        # view and qml_source() are defined in Full example.
        # Theme.qml binds fluentTheme colors, typography and corner radius.
        view.load(qml_source("Theme"))
    """),
    "qml-view-context": _source("""
        # panel, view and qml_source() are defined in Full example.
        title_edit = fluentqt.LineEdit(panel)
        title_edit.setText("A shared title")
        if view.setContextObject("widgetInput", title_edit):
            view.load(qml_source("Context"))
    """),
    "qml-view-action": _source("""
        # panel, view and qml_source() are defined in Full example.
        progress = fluentqt.ProgressBar(panel)
        progress.setRange(0, 100)
        progress.setValue(40)
        if view.setContextObject("widgetProgress", progress):
            view.load(qml_source("Action"))
    """),
}


def teaching_source(sample_id):
    """Return only the selected capability; Full example supplies its host."""
    if sample_id in _KEY_SOURCE:
        body = _KEY_SOURCE[sample_id]
    else:
        # Take the actual callbacks from the executable preview, including
        # teardown-safe default arguments and the explicit reload operation.
        callbacks = _SCRIPTS[sample_id].split("def update_status", 1)
        if len(callbacks) != 2:
            raise ValueError("Missing QmlBridge status callback: " + sample_id)
        body = ("from fluentqt.qmlbridge import QmlView\n\n"
                "# session, view, buttons, status and qml_source() are in Full example.\n"
                "def update_status" + callbacks[1])
    return _format_display_source(body + "\n")


def full_example_source(sample_id, *, gallery_parent=False):
    """Return the preview script or main.py for the matching QML source file."""
    if gallery_parent:
        return (_IMPORTS + "from fluentqt_gallery.native_samples_qmlbridge import qml_source\n\n"
                + _SCRIPTS[sample_id])
    qml_name = _QML_FILES[sample_id]
    source = f"# Save main.py and {qml_name} in the same directory.\n"
    source += "import sys\nfrom pathlib import Path\n"
    source += _IMPORTS + "from PySide6.QtCore import QUrl\nfrom PySide6.QtWidgets import QApplication\n\n"
    source += "def qml_source(name):\n"
    source += "    filename = name if name.endswith(\".qml\") else name + \".qml\"\n"
    source += "    return QUrl.fromLocalFile(str(Path(__file__).resolve().with_name(filename)))\n\n"
    source += "def make_panel(parent=None):\n"
    body = _SCRIPTS[sample_id].replace('globals().get("gallery_parent")', "parent")
    source += indent(body, "    ")
    source += "    panel._qml_session = session\n    return panel\n\n"
    source += _source("""
        def main():
            fluentqt.prepare_high_dpi_application()
            QmlSession.prepareApplication()
            app = QApplication(sys.argv)
            fluentqt.initialize_resources()
            app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))
            window = fluentqt.Window()
            window.setWindowTitle("QmlView")
            window.resize(640, 480)
            window.setContentWidget(make_panel(window))
            window.show()
            return app.exec()

        if __name__ == "__main__":
            sys.exit(main())
    """)
    return source


def example_source_files(sample_id):
    """Return separately copyable main.py and canonical QML file contents."""
    qml_name = _QML_FILES[sample_id]
    return [
        {
            "name": "main.py",
            "language": "python",
            "code": full_example_source(sample_id),
            "usage_snippet": teaching_source(sample_id),
        },
        {
            "name": qml_name,
            "language": "qml",
            "code": _qml_asset_path(qml_name).read_bytes().decode("utf-8"),
            "usage_snippet": "",
        },
    ]


def _register(sample_id):
    @native_samples("qml-view", sample_id)
    def build(_sample_id, parent):
        source = full_example_source(sample_id, gallery_parent=True)
        namespace = {"__name__": __name__ + "." + sample_id, "gallery_parent": parent}
        exec(compile(source, "<" + sample_id + ">", "exec"), namespace)
        panel = namespace["panel"]
        panel._fluentqt_gallery_source_namespace = namespace
        panel._qml_session = namespace["session"]
        return _result(panel, source, "QmlView", source_driven=True,
                       display_source=teaching_source(sample_id))


for _sample_id in _SCRIPTS:
    _register(_sample_id)
