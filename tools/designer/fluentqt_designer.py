"""Form classes shared by generated Python, QUiLoader and Qt Designer."""
from importlib import import_module

from fluentqt_designer_catalog import PAGE_METHODS, WIDGETS, form_data_property

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
    if name in PAGE_METHODS:
        from PySide6.QtCore import Slot
        from PySide6.QtWidgets import QWidget

        @Slot(QWidget)
        def setFormPage(self, page):
            getattr(self, PAGE_METHODS[name].replace("set", "setOwned", 1))(page)
        attributes["setFormPage"] = setFormPage
    data_property = form_data_property(entry)
    if data_property:
        from PySide6.QtCore import QPointF, Property, QStringListModel

        def get_data(self):
            return getattr(self, "_form_data", [])
        def set_data(self, values):
            self._form_data = list(values)
            if data_property == "formSeries":
                from fluentqt.charts import ChartData, ChartModel
                if not hasattr(self, "_form_model"):
                    self._form_model = ChartModel(self)
                self._form_model.setName(values[0] if values else "")
                points = [QPointF(i, float(v)) for i, v in enumerate(values[1:], 1)]
                self._form_model.setDataSnapshot(ChartData.fromPoints(points))
                self.setModel(self._form_model)
            elif name == "FluentListView":
                if not hasattr(self, "_form_model"):
                    self._form_model = QStringListModel(self)
                self._form_model.setStringList(values)
                self.setModel(self._form_model)
            elif name == "FluentBreadcrumb":
                self.setItems(values)
            else:
                self.clearItems()
                for text in values:
                    self.addItem(text)
        attributes[data_property] = Property("QStringList", get_data, set_data)
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
