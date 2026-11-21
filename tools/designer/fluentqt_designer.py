"""Form classes shared by generated Python, QUiLoader and Qt Designer."""
from importlib import import_module

from fluentqt_designer_catalog import WIDGETS

_ENTRIES = {entry["name"]: entry for entry in WIDGETS}


def __getattr__(name):
    entry = _ENTRIES.get(name)
    if entry is None:
        raise AttributeError(name)
    attributes = {"__module__": __name__}
    if "adapter" in entry:
        from fluentqt_designer_surfaces import CommandsButton, MessageButton, TimedMessageButton
        base = {"Commands": CommandsButton, "Message": MessageButton, "TimedMessage": TimedMessageButton}[entry["adapter"]]
        attributes["_entry"] = entry
    else:
        base = getattr(import_module(entry["python"]), entry["cpp"].split("::")[-1])
    if entry.get("textConstructor"):
        def __init__(self, parent=None):
            base.__init__(self, "", parent)
        attributes["__init__"] = __init__
    widget = type(name, (base,), attributes)
    globals()[name] = widget
    return widget


def available_widgets():
    """Skip only optional bindings absent from this installation."""
    for entry in WIDGETS:
        try:
            widget = globals().get(entry["name"]) or __getattr__(entry["name"])
        except ModuleNotFoundError as exc:
            if entry.get("extension") and exc.name == entry["python"]:
                continue
            raise
        yield entry, widget
