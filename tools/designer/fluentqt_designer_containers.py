"""Expose a Fluent single-content host as a page container in Qt Designer."""
from PySide6.QtDesigner import QDesignerCustomWidgetInterface, QExtensionFactory, QPyDesignerContainerExtension
from PySide6.QtGui import QIcon

from fluentqt_designer_catalog import DIRECTORY, PAGE_METHODS, widget_xml


# Qt Designer borrows Python-created plugin interfaces for the lifetime of the editor.
_plugins = []


def register_container(collection, entry, widget):
    plugin = ContainerPlugin(entry, widget)
    _plugins.append(plugin)
    collection.addCustomWidget(plugin)


class PageExtension(QPyDesignerContainerExtension):
    def __init__(self, host, parent):
        super().__init__(parent)
        self.host = host
        self.field = PAGE_METHODS[host.metaObject().className()] == "setEditor"

    def widget(self, index):
        if index == 0:
            return self.host.editor() if self.field else self.host.contentWidget()
        return None

    def count(self):
        return int(self.widget(0) is not None)

    def currentIndex(self):
        return self.count() - 1

    def canAddWidget(self):
        return self.count() == 0

    def canRemove(self, index):
        return index == 0 and self.count() == 1

    def addWidget(self, widget):
        if self.canAddWidget():
            self.host.setFormPage(widget)

    def insertWidget(self, index, widget):
        if index == 0:
            self.addWidget(widget)

    def remove(self, index):
        if self.canRemove(index):
            (self.host.takeEditor if self.field else self.host.takeContentWidget)()

    def setCurrentIndex(self, index):
        pass


class AccordionExtension(QPyDesignerContainerExtension):
    def __init__(self, host, parent):
        super().__init__(parent)
        self.host = host
        self.current = 0

    def widget(self, index): return self.host.formSectionPage(index)
    def count(self): return self.host.count()
    def currentIndex(self): return min(self.current, self.count() - 1)
    def canAddWidget(self): return True
    def canRemove(self, index): return 0 <= index < self.count()

    def setCurrentIndex(self, index):
        if 0 <= index < self.count(): self.current = index

    def addWidget(self, page): self.host.addFormSection(page)

    def insertWidget(self, index, page):
        self.host.insertFormSection(index, page)

    def remove(self, index):
        if self.canRemove(index): self.host.removeFormSection(index)


class PageFactory(QExtensionFactory):
    def createExtension(self, obj, iid, parent):
        if iid == "org.qt-project.Qt.Designer.Container" and obj.metaObject().className() == "FluentAccordion":
            return AccordionExtension(obj, parent)
        if iid == "org.qt-project.Qt.Designer.Container" and obj.metaObject().className() in PAGE_METHODS:
            return PageExtension(obj, parent)
        return None


class ContainerPlugin(QDesignerCustomWidgetInterface):
    def __init__(self, entry, widget):
        super().__init__()
        self.entry = entry
        self.widget = widget
        self.ready = False

    def initialize(self, core):
        manager = core.extensionManager()
        if not manager.findChild(PageFactory, "fluentDesignerPages"):
            factory = PageFactory(manager)
            factory.setObjectName("fluentDesignerPages")
            manager.registerExtensions(factory, "org.qt-project.Qt.Designer.Container")
        self.ready = True

    def createWidget(self, parent): return self.widget(parent)
    def isInitialized(self): return self.ready
    def isContainer(self): return True
    def domXml(self): return widget_xml(self.entry)
    def group(self): return "FluentQt / " + self.entry["group"]
    def includeFile(self): return "fluentqt_designer.h"
    def name(self): return self.entry["name"]
    def icon(self): return QIcon(str(DIRECTORY / "icons" / (self.name() + ".png")))
    def toolTip(self): return self.name()
    def whatsThis(self): return self.name()
