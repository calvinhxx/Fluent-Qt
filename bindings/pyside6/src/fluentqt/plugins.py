"""Optional linked extensions backed by FluentQt's native plugin manager.

Plugins, hosts, models and delivered widgets keep caller ownership. Keep a
Python reference to a parentless plugin while it should remain registered.
"""

from PySide6.QtCore import QVersionNumber
from PySide6.QtWidgets import QWidget
from shiboken6 import isValid

from . import _fluentqt as _native


class PluginMetadata(_native.fluent.PluginMetadata):
    def __init__(self, id="", name="", version=None, dependencies=()):
        super().__init__()
        self.id = id
        self.name = name
        self.version = QVersionNumber() if version is None else version
        self.dependencies = list(dependencies)


class PluginContext(_native.fluent.PluginContext):
    def activePlugin(self, id):
        return _native.pluginContextActivePluginForBinding(self, id)


class _PluginCallbacks:
    def metadata(self):
        raise NotImplementedError("A plugin must provide PluginMetadata")

    def onActivate(self, context):
        return True

    def onDeactivate(self):
        pass

    def lastError(self):
        return self._callbackError()

    def _callback_failed(self, exception):
        if isValid(self):
            self._setCallbackError(str(exception) or type(exception).__name__)

    def _metadataForBinding(self):
        try:
            descriptor = self.metadata()
            if not isinstance(descriptor, _native.fluent.PluginMetadata):
                raise TypeError("metadata() must return PluginMetadata")
            return descriptor
        except BaseException as exception:
            self._callback_failed(exception)
            return _native.fluent.PluginMetadata()

    def _activateForBinding(self, context):
        try:
            result = self.onActivate(PluginContext(context))
            if not isinstance(result, bool):
                raise TypeError("onActivate() must return bool")
            return result
        except BaseException as exception:
            self._callback_failed(exception)
            return False

    def _deactivateForBinding(self):
        try:
            self.onDeactivate()
        except BaseException as exception:
            # The native hook is noexcept. Preserve diagnostics on the plugin
            # instead of letting an exception escape through its destructor.
            self._callback_failed(exception)


class Plugin(_PluginCallbacks, _native.fluent._Plugin):
    """Subclass metadata and optional synchronous activation/cleanup hooks."""


class WidgetPlugin(_PluginCallbacks, _native.fluent._WidgetPlugin):
    """Subclass widgetKeys and createWidget to provide named QWidget factories."""

    def widgetKeys(self):
        raise NotImplementedError("A widget plugin must provide widget keys")

    def createWidget(self, key, parent=None):
        raise NotImplementedError("A widget plugin must provide createWidget")

    def _widgetKeysForBinding(self):
        try:
            keys = list(self.widgetKeys())
            if any(not isinstance(key, str) for key in keys):
                raise TypeError("widgetKeys() must return strings")
            return keys
        except BaseException as exception:
            self._callback_failed(exception)
            return []

    def _createWidgetForBinding(self, key, parent):
        try:
            widget = self.createWidget(key, parent)
            if widget is not None and (not isinstance(widget, QWidget) or not isValid(widget)):
                raise TypeError("createWidget() must return a live QWidget or None")
            return widget
        except BaseException as exception:
            self._callback_failed(exception)
            return None


class PluginManager(_native.fluent._PluginManager):
    def metadata(self, id):
        descriptor = super().metadata(id)
        return PluginMetadata(descriptor.id, descriptor.name, descriptor.version,
                              descriptor.dependencies)

    def context(self):
        return PluginContext(super().context())

    def createWidget(self, pluginId, key, parent=None):
        widget = super().createWidget(pluginId, key, parent)
        spatial_type = getattr(_native.fluent, "SpatialView", None)
        if widget is not None and spatial_type is not None and isinstance(widget, spatial_type):
            from .spatial import _prepare_spatial_view
            _prepare_spatial_view(widget)
        return widget


__all__ = ["Plugin", "WidgetPlugin", "PluginMetadata", "PluginContext", "PluginManager"]
