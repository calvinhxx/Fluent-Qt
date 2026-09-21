"""Spatial combinations aligned with the canonical C++ Gallery cards.

The complete Python example constructs the live preview. Key usage remains a
small integration excerpt; the Gallery binds the view to its single Settings mode.
"""

from textwrap import dedent

from .native_samples import native_samples, _result, _format_display_source, _strip_gallery_parent
from .spatial_support import SpatialPreviewBinding

_IMPORTS = """import fluentqt
from fluentqt.spatial import SpatialView
from PySide6.QtCore import QDate, QPointF, Qt, QTimer, QStringListModel
from PySide6.QtGui import QVector3D, QStandardItem, QStandardItemModel
from PySide6.QtWidgets import (
    QAbstractItemView, QGridLayout, QHBoxLayout, QVBoxLayout, QWidget,
)

"""


def _source(body):
    return dedent(body).strip() + "\n"


def _panel(height=340, view_type="SpatialView", camera=1400):
    return _source(f"""
        panel = QWidget(globals().get("gallery_parent"))
        column = QVBoxLayout(panel)
        column.setContentsMargins(0, 0, 0, 0)
        column.setSpacing(12)
        view = {view_type}(panel)
        view.setObjectName("spatialPreviewView")
        view.setFixedHeight({height})
        view.setCameraDistance({camera})
        view.setMaximumTilt(QPointF(4, 6))
        column.addWidget(view)
    """)


def _card(width, height, spacing=12):
    return _source(f"""
        card = fluentqt.Card()
        card.setFixedSize({width}, {height})
        content = QVBoxLayout(card)
        content.setContentsMargins(20, 16, 20, 16)
        content.setSpacing({spacing})
    """)


_POSE = _source("""
    item = view.addOwnedWidget(card)
    item.setSurfaceIntensity(0.75)
    item.setHoverLift(4.5)
    item.setRotation(QVector3D(5, -12, 0))
""")
_USAGE = _source("""
    from fluentqt.spatial import SpatialView
    from PySide6.QtCore import QPointF
    from PySide6.QtGui import QVector3D

    # card and column are built in Full example.
    view = SpatialView(panel)
    column.addWidget(view)
    view.setCameraDistance(1400)
    view.setMaximumTilt(QPointF(4, 6))
    item = view.addOwnedWidget(card)
    item.setRotation(QVector3D(5, -12, 0))
    item.setSurfaceIntensity(0.75)
    item.setHoverLift(4.5)
    # The existing controls keep their models and signals.
""")

_SCRIPTS = {}
_USAGE_BY_ID = {}

_SCRIPTS["spatial-view-inputs"] = _panel() + _card(320, 260) + _source("""
    title = fluentqt.Label("Level preview", card)
    title.setFluentTypography(fluentqt.FontRole.Subtitle)
    title.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
    content.addWidget(title)
    value = fluentqt.Label("Level: 40%", card)
    value.setObjectName("spatialLevelText")
    value.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
    content.addWidget(value)
    level = fluentqt.Slider(card)
    level.setObjectName("spatialLevelSlider")
    level.setAccessibleName("Preview level")
    level.setRange(0, 100)
    level.setValue(40)
    content.addWidget(level)
    meter = fluentqt.ProgressBar(card)
    meter.setObjectName("spatialLevelMeter")
    meter.setAccessibleName("Preview output")
    meter.setValue(40)
    content.addWidget(meter)
    mute = fluentqt.CheckBox("Mute preview", card)
    mute.setObjectName("spatialMuteCheckBox")
    content.addWidget(mute)

    def change_level(number):
        value.setText(f"Level: {number}%")
        meter.setValue(0 if mute.isChecked() else number)

    level.valueChanged.connect(change_level)
    mute.toggled.connect(lambda muted: meter.setValue(0 if muted else level.value()))
""") + _POSE

_SCRIPTS["spatial-view-list"] = _panel() + _card(340, 290) + _source("""
    heading = QHBoxLayout()
    title = fluentqt.Label("Demo tasks", card)
    title.setFluentTypography(fluentqt.FontRole.Subtitle)
    title.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
    heading.addWidget(title, 1)
    count = fluentqt.InfoBadge(card)
    count.setObjectName("spatialTaskCount")
    count.setAccessibleName("Remaining demo tasks")
    count.setValue(3)
    heading.addWidget(count)
    content.addLayout(heading)
    model = QStringListModel(["Review colors", "Check spacing", "Try dark mode"], card)
    tasks = fluentqt.ListView(card)
    tasks.setObjectName("spatialTaskList")
    tasks.setAccessibleName("Demo tasks")
    tasks.setModel(model)
    tasks.setEditTriggers(QAbstractItemView.NoEditTriggers)
    tasks.setSelectedIndex(0)
    tasks.setPlaceholderText("All demo tasks done")
    content.addWidget(tasks, 1)
    done = fluentqt.Button("Mark selected done", card)
    done.setObjectName("spatialTaskDone")
    done.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    content.addWidget(done)

    def mark_done():
        row = tasks.selectedIndex()
        if row < 0:
            return
        model.removeRow(row)
        count.setValue(model.rowCount())
        tasks.setSelectedIndex(min(row, model.rowCount() - 1))
        done.setEnabled(model.rowCount() > 0)

    done.clicked.connect(mark_done)
""") + _POSE

_SCRIPTS["spatial-view-calendar"] = _panel(380) + _card(360, 410, 8) + _source("""
    view.setZoom(0.82)
    content.setContentsMargins(16, 12, 16, 12)
    calendar = fluentqt.CalendarView(card)
    calendar.setObjectName("spatialCalendar")
    calendar.setAccessibleName("Demo appointment date")
    calendar.setSelectedDate(QDate(2026, 9, 15))
    calendar.setFrameVisible(False)
    content.addWidget(calendar, 1)
    selected = fluentqt.Label("Selected: 2026-09-15", card)
    selected.setObjectName("spatialSelectedDate")
    selected.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
    content.addWidget(selected)
    calendar.selectedDateChanged.connect(
        lambda date: selected.setText("Selected: " + date.toString(Qt.ISODate))
    )
""") + _POSE

_SCRIPTS["spatial-view-navigation"] = _panel() + _card(340, 260) + _source("""
    title = fluentqt.Label("Studio", card)
    title.setObjectName("spatialProfileTitle")
    title.setWordWrap(True)
    title.setFluentTypography(fluentqt.FontRole.Subtitle)
    title.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
    content.addWidget(title)
    tabs = fluentqt.SelectorBar(card)
    tabs.setObjectName("spatialProfileTabs")
    tabs.addItem("Overview")
    tabs.addItem("Details")
    content.addWidget(tabs)
    pages = fluentqt.StackContentHost(card)
    pages.setObjectName("spatialProfilePages")
    pages.setTransitionAnimationEnabled(False)
    overview = fluentqt.Label("Your workspace preview", pages)
    overview.setWordWrap(True)
    details = fluentqt.Label("Card + SelectorBar + StackContentHost", pages)
    details.setWordWrap(True)
    pages.addOwnedPage(overview)
    pages.addOwnedPage(details)
    pages.setCurrentIndex(0, 0, False)
    content.addWidget(pages, 1)
    tabs.setSelectedIndex(0)
    tabs.selectedIndexChanged.connect(lambda index: pages.setCurrentIndex(index, 0, False))
    # Native text editing stays outside the projected card.
    name_label = fluentqt.Label("Workspace name", panel)
    column.addWidget(name_label)
    name = fluentqt.LineEdit(panel)
    name.setObjectName("spatialProfileName")
    name.setAccessibleName("Workspace name")
    name.setText("Studio")
    name.setMaxLength(24)
    column.addWidget(name)
    name.textChanged.connect(lambda text: title.setText(text or "Workspace"))
""") + _POSE
_USAGE_BY_ID["spatial-view-navigation"] = _USAGE + "# name stays outside view; textChanged updates title inside card.\n"

_SCRIPTS["spatial-view-rating"] = _panel(380) + _card(340, 300) + _source("""
    title = fluentqt.Label("Review a preset", card)
    title.setFluentTypography(fluentqt.FontRole.Subtitle)
    title.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
    content.addWidget(title)
    balanced = fluentqt.RadioButton("Balanced", card)
    balanced.setObjectName("spatialBalancedPreset")
    vivid = fluentqt.RadioButton("Vivid", card)
    vivid.setObjectName("spatialVividPreset")
    balanced.setChecked(True)
    content.addWidget(balanced)
    content.addWidget(vivid)
    rating = fluentqt.RatingControl(card)
    rating.setObjectName("spatialPresetRating")
    rating.setAccessibleName("Preset rating")
    rating.setValue(3)
    rating.setCaption("3 / 5")
    content.addWidget(rating)
    result = fluentqt.Label("Balanced selected", card)
    result.setObjectName("spatialPresetResult")
    result.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
    content.addWidget(result)
    balanced.toggled.connect(lambda checked: result.setText("Balanced selected") if checked else None)
    vivid.toggled.connect(lambda checked: result.setText("Vivid selected") if checked else None)
    rating.valueChanged.connect(
        lambda value: rating.setCaption("Not rated" if value < 0 else f"{value:g} / 5")
    )
""") + _POSE

_SCRIPTS["spatial-view-donut"] = _panel(380) + _card(340, 300) + _source("""
    chart = fluentqt.DonutChart(card)
    chart.setObjectName("spatialAllocationChart")
    chart.setAccessibleName("Illustrative storage allocation")
    chart.setTitle("Storage allocation")
    chart.setLegendVisible(False)
    chart.setCenterText("65%")
    chart.setCenterCaption("used")
    model = fluentqt.ChartModel(card)
    model.setPoints([QPointF(0, 65), QPointF(1, 35)], ["Used", "Free"])
    chart.setModel(model)
    content.addWidget(chart, 1)
    allocation = fluentqt.Slider(card)
    allocation.setObjectName("spatialAllocationSlider")
    allocation.setAccessibleName("Used storage percentage")
    allocation.setRange(5, 95)
    allocation.setValue(65)
    content.addWidget(allocation)

    def change_allocation(value):
        model.setPoints([QPointF(0, value), QPointF(1, 100 - value)], ["Used", "Free"])
        chart.setCenterText(f"{value}%")

    allocation.valueChanged.connect(change_allocation)
""") + _POSE

_SCRIPTS["spatial-view-hybrid"] = _panel(276) + _card(360, 176) + _source("""
    heading = QHBoxLayout()
    title = fluentqt.Label("Export preview", card)
    title.setFluentTypography(fluentqt.FontRole.Subtitle)
    title.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
    heading.addWidget(title)
    heading.addStretch()
    format_label = fluentqt.Label("PNG", card)
    format_label.setObjectName("spatialExportFormat")
    format_label.setTextColorRole(fluentqt.Label.TextColorRole.Accent)
    heading.addWidget(format_label)
    content.addLayout(heading)
    file = QHBoxLayout()
    file.setSpacing(16)
    icon = fluentqt.FontIcon(fluentqt.Typography.Icons.Document, card)
    icon.setIconSize(48)
    file.addWidget(icon)
    details = QVBoxLayout()
    details.setSpacing(4)
    details.setAlignment(Qt.AlignVCenter)
    filename = fluentqt.Label("gallery-preview.png", card)
    filename.setObjectName("spatialExportFilename")
    filename.setFluentTypography(fluentqt.FontRole.BodyStrong)
    details.addWidget(filename)
    note = fluentqt.Label("Image with transparency", card)
    note.setObjectName("spatialExportNote")
    note.setWordWrap(True)
    note.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
    details.addWidget(note)
    file.addLayout(details, 1)
    content.addLayout(file, 1)
    # The dropdown remains outside the projected card.
    formats = fluentqt.ComboBox(panel)
    formats.setObjectName("spatialExportChoice")
    formats.setAccessibleName("Export format")
    formats.setFixedWidth(120)
    formats.addItems(["PNG", "JPEG", "SVG"])

    def change_format(index):
        name = formats.itemText(index)
        format_label.setText(name)
        filename.setText("gallery-preview." + name.lower())
        note.setText(["Image with transparency", "Compact photo", "Scalable vector"][index])

    formats.currentIndexChanged.connect(change_format)
    toolbar = QHBoxLayout()
    toolbar.addStretch()
    toolbar.addWidget(formats)
    column.insertLayout(0, toolbar)
""") + _POSE
_USAGE_BY_ID["spatial-view-hybrid"] = _USAGE + "# formats stays outside view; its signal updates card.\n"

_SCRIPTS["spatial-view-cards"] = _panel() + _source("""
    view.setZoom(0.78)
    settings = fluentqt.Card()
    settings.setFixedSize(270, 180)
    controls = QVBoxLayout(settings)
    controls.setContentsMargins(20, 16, 20, 16)
    heading = fluentqt.Label("Notifications", settings)
    heading.setFluentTypography(fluentqt.FontRole.Subtitle)
    controls.addWidget(heading)
    alerts = fluentqt.ToggleSwitch(settings)
    alerts.setObjectName("spatialAlertsToggle")
    alerts.setAccessibleName("Desktop alerts")
    alerts.setOnContent("Desktop alerts on")
    alerts.setOffContent("Desktop alerts off")
    alerts.setIsOn(True)
    controls.addWidget(alerts)
    note = fluentqt.Label("Try the switch in either mode.", settings)
    note.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
    note.setWordWrap(True)
    controls.addWidget(note)
    back = view.addOwnedWidget(settings)
    back.setSurfaceIntensity(0.75)
    back.setHoverLift(4.5)
    back.setPosition(QVector3D(-110, -64, -80))
    back.setRotation(QVector3D(6, 12, 0))
    summary = fluentqt.Card()
    summary.setFixedSize(270, 180)
    summary_layout = QVBoxLayout(summary)
    summary_layout.setContentsMargins(20, 16, 20, 16)
    chart = fluentqt.Sparkline(summary)
    chart.setObjectName("spatialDemoChart")
    chart.setAccessibleName("Illustrative activity trend")
    chart.setMinimumHeight(72)
    model = fluentqt.ChartModel(summary)
    model.setPoints([QPointF(x, y) for x, y in enumerate([12, 24, 18, 38, 32, 54])])
    chart.setModel(model)
    caption = fluentqt.Label("Activity trend · demo data", summary)
    summary_layout.addWidget(caption)
    summary_layout.addWidget(chart)
    front = view.addOwnedWidget(summary)
    front.setSurfaceIntensity(0.75)
    front.setHoverLift(4.5)
    front.setPosition(QVector3D(110, 64, 100))
    front.setRotation(QVector3D(-6, -14, 0))
""")
_USAGE_BY_ID["spatial-view-cards"] = _source("""
    from PySide6.QtGui import QVector3D

    # view, settings and summary are built in Full example.
    back = view.addOwnedWidget(settings)
    front = view.addOwnedWidget(summary)
    back.setPosition(QVector3D(-110, -64, -80))
    front.setPosition(QVector3D(110, 64, 100))
    back.setRotation(QVector3D(6, 12, 0))
    front.setRotation(QVector3D(-6, -14, 0))
    back.setSurfaceIntensity(0.75)
    front.setSurfaceIntensity(0.75)
""")

_SLIDER = _source("""
    def add_slider(parent, layout, name, label, unit, minimum, maximum, initial):
        in_grid = isinstance(layout, QGridLayout)
        group = QWidget(parent) if in_grid else parent
        column = QVBoxLayout(group) if in_grid else QVBoxLayout()
        column.setContentsMargins(0, 0, 0, 0)
        column.setSpacing(4)
        text = fluentqt.Label(f"{label}: {initial}{unit}", group)
        slider = fluentqt.Slider(group)
        slider.setObjectName(name)
        slider.setAccessibleName(label)
        slider.setRange(minimum, maximum)
        slider.setValue(initial)
        slider.valueChanged.connect(lambda value: text.setText(f"{label}: {value}{unit}"))
        column.addWidget(text)
        column.addWidget(slider)
        if isinstance(layout, QGridLayout):
            layout.addWidget(group, 0, layout.count())
        else:
            layout.addLayout(column)
        return slider
""")

_SCENE_VIEW = _source("""
    class SceneView(SpatialView):
        def arrange(self):
            if self.itemCount() == 2:
                spread = 24 if self.width() < 650 else 100
                self.items()[0].setPosition(QVector3D(-spread, -44, -160))
                self.items()[-1].setPosition(QVector3D(spread, 44, 160))

        def resizeEvent(self, event):
            super().resizeEvent(event)
            self.arrange()
""")
_SCRIPTS["spatial-view-scene"] = _SCENE_VIEW + _panel(400, "SceneView", 1200) + _SLIDER + _source("""
    column.setSpacing(16)
    view.setZoom(0.9)
    view.setMaximumTilt(QPointF(6, 12))

    def make_layer(title, caption):
        card = fluentqt.Card()
        card.setFixedSize(220, 150)
        content = QVBoxLayout(card)
        content.setContentsMargins(16, 12, 16, 12)
        content.setSpacing(6)
        heading = fluentqt.Label(title, card)
        heading.setFluentTypography(fluentqt.FontRole.Subtitle)
        content.addWidget(heading)
        chart = fluentqt.Sparkline(card)
        chart.setAccessibleName(title + " sample chart")
        model = fluentqt.ChartModel(card)
        model.setPoints([QPointF(x, y) for x, y in enumerate([12, 24, 18, 38, 32, 54])])
        chart.setModel(model)
        content.addWidget(chart, 1)
        note = fluentqt.Label(caption, card)
        note.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
        content.addWidget(note)
        card._model = model
        return card

    back = view.addOwnedWidget(make_layer("Back layer", "Z -160 · sample data"))
    front = view.addOwnedWidget(make_layer("Front layer", "Z +160 · sample data"))
    back.setRotation(QVector3D(6, 16, 0))
    front.setRotation(QVector3D(-6, -16, 0))
    back.setSurfaceIntensity(0.75)
    front.setSurfaceIntensity(0.75)
    view.arrange()
    toolbar = QHBoxLayout()
    follow = fluentqt.ToggleSwitch(panel)
    follow.setObjectName("spatialViewFollow")
    follow.setAccessibleName("Follow pointer")
    follow.setOnContent("Follow pointer")
    follow.setOffContent("Follow pointer")
    follow.setIsOn(True)
    toolbar.addWidget(follow)
    toolbar.addStretch()
    column.removeWidget(view)
    column.addLayout(toolbar)
    column.addWidget(view)
    controls = QWidget(panel)
    controls.setObjectName("spatialViewControls")
    sliders = QGridLayout(controls)
    sliders.setContentsMargins(0, 0, 0, 0)
    sliders.setHorizontalSpacing(24)
    sliders.setVerticalSpacing(12)
    sliders.setColumnStretch(0, 1)
    sliders.setColumnStretch(1, 1)
    distance = add_slider(controls, sliders, "spatialViewDistance", "Camera distance", " px", 650, 2400, 1200)
    zoom = add_slider(controls, sliders, "spatialViewZoom", "Scene zoom", "%", 50, 110, 90)
    distance.valueChanged.connect(view.setCameraDistance)
    zoom.valueChanged.connect(lambda value: view.setZoom(value / 100.0))
    column.addWidget(controls)
    follow.toggled.connect(view.setPointerTrackingEnabled)
    view.pointerTrackingEnabledChanged.connect(follow.setIsOn)

    def update_availability():
        controls.setEnabled(view.isSpatialEnabled())
        follow.setEnabled(view.isSpatialEnabled())

    view.spatialEnabledChanged.connect(update_availability)
    update_availability()
""")
_USAGE_BY_ID["spatial-view-scene"] = _source("""
    from PySide6.QtCore import QPointF

    # view contains two cards; the controls are built in Full example.
    view.setCameraDistance(1200)
    view.setZoom(0.9)
    view.setMaximumTilt(QPointF(6, 12))
    distance.valueChanged.connect(view.setCameraDistance)
    zoom.valueChanged.connect(lambda value: view.setZoom(value / 100.0))
    follow.toggled.connect(view.setPointerTrackingEnabled)
""")

_WORKBENCH = _source("""
    class WorkbenchView(SpatialView):
        def __init__(self, parent=None):
            super().__init__(parent)
            self.arrange_timer = QTimer(self)
            self.arrange_timer.setSingleShot(True)
            self.arrange_timer.timeout.connect(self.arrange)

        def arrange(self):
            if self.itemCount() != 2:
                return
            stacked = self.width() < 720
            self.setFixedHeight(520 if not self.isSpatialEnabled() else (560 if stacked else 360))
            item = self.items()[0]
            item.setPosition(QVector3D(0 if stacked else -164, -140 if stacked else 0, item.position().z()))
            self.items()[-1].setPosition(QVector3D(0 if stacked else 164, 110 if stacked else 0, -50))

        def resizeEvent(self, event):
            super().resizeEvent(event)
            self.arrange_timer.start(0)
""")
_SCRIPTS["spatial-item-pose"] = _WORKBENCH + _panel(360, "WorkbenchView") + _card(280, 180, 8) + _SLIDER + _source("""
    view.setZoom(0.92)
    card.setObjectName("spatialDemoCard")
    title = fluentqt.Label("Activity", card)
    title.setFluentTypography(fluentqt.FontRole.Subtitle)
    title.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
    content.addWidget(title)
    chart = fluentqt.Sparkline(card)
    chart.setAccessibleName("Activity trend, demo data")
    model = fluentqt.ChartModel(card)
    model.setPoints([QPointF(x, y) for x, y in enumerate([12, 24, 18, 38, 32, 54])])
    chart.setModel(model)
    content.addWidget(chart, 1)
    caption = fluentqt.Label("Last 6 days · demo data", card)
    caption.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
    content.addWidget(caption)
    item = view.addOwnedWidget(card)
    item.setSurfaceIntensity(0.75)
    item.setHoverLift(5)
    item.setRotation(QVector3D(0, -18, 0))
    settings = fluentqt.Card()
    settings.setObjectName("spatialParameterCard")
    settings.setFixedSize(280, 260)
    controls = QVBoxLayout(settings)
    controls.setContentsMargins(20, 16, 20, 16)
    controls.setSpacing(8)
    heading = fluentqt.Label("Card", settings)
    heading.setFluentTypography(fluentqt.FontRole.Subtitle)
    controls.addWidget(heading)
    finish = add_slider(settings, controls, "spatialItemSurfaceIntensity", "Surface", "%", 0, 100, 75)
    angle = add_slider(settings, controls, "spatialItemRotation", "Rotation", "°", -35, 35, -18)
    depth = add_slider(settings, controls, "spatialItemDepth", "Depth", " px", -120, 120, 0)
    finish.valueChanged.connect(lambda value: item.setSurfaceIntensity(value / 100.0))
    angle.valueChanged.connect(lambda value: item.setRotation(QVector3D(0, value, 0)))

    def change_depth(value):
        position = item.position()
        position.setZ(value)
        item.setPosition(position)

    depth.valueChanged.connect(change_depth)
    control_item = view.addOwnedWidget(settings)
    control_item.setSurfaceIntensity(0.75)
    control_item.setRotation(QVector3D(0, -6, 0))
    view.spatialEnabledChanged.connect(view.arrange)
    view.arrange()
""")
_USAGE_BY_ID["spatial-item-pose"] = _source("""
    from PySide6.QtGui import QVector3D

    # view, cards and sliders are built in Full example.
    item = view.addOwnedWidget(card)
    item.setRotation(QVector3D(0, -18, 0))
    item.setSurfaceIntensity(0.75)
    item.setHoverLift(5)
    control_item = view.addOwnedWidget(settings)
    control_item.setRotation(QVector3D(0, -6, 0))
    control_item.setSurfaceIntensity(0.75)
    finish.valueChanged.connect(lambda value: item.setSurfaceIntensity(value / 100.0))
    angle.valueChanged.connect(lambda value: item.setRotation(QVector3D(0, value, 0)))

    def change_depth(value):
        position = item.position()
        position.setZ(value)
        item.setPosition(position)

    depth.valueChanged.connect(change_depth)
""")

# Reuse the Gallery's existing tree styling, just as the C++ sample does.
from .native_samples_collections import (
    _MODEL_IMPORTS, _TREE_MODEL_HELPER, _TREE_DELEGATE_HELPER,
)
_SCRIPTS["spatial-view-tree"] = _MODEL_IMPORTS + "\n" + str(_TREE_MODEL_HELPER) + str(_TREE_DELEGATE_HELPER) + _panel(356) + _card(360, 264, 8) + _source("""
    title = fluentqt.Label("Project files", card)
    title.setFluentTypography(fluentqt.FontRole.Subtitle)
    title.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
    content.addWidget(title)
    tree = fluentqt.TreeView(card)
    tree.setObjectName("spatialFileTree")
    tree.setAccessibleName("Project files")
    tree.setHeaderHidden(True)
    tree.setBorderVisible(False)
    tree.setBackgroundVisible(False)
    row_delegate = TreeRowDelegate(tree, 32)
    tree.setItemDelegate(row_delegate)
    model = QStandardItemModel(card)
    sources = make_tree_item("Sources", fluentqt.Typography.Icons.Folder, QColor())
    sources.appendRow(make_tree_item("main.cpp", fluentqt.Typography.Icons.Document, QColor()))
    sources.appendRow(make_tree_item("window.cpp", fluentqt.Typography.Icons.Document, QColor()))
    assets = make_tree_item("Assets", fluentqt.Typography.Icons.Folder, QColor())
    assets.appendRow(make_tree_item("logo.svg", fluentqt.Typography.Icons.Document, QColor()))
    model.appendRow(sources)
    model.appendRow(assets)
    tree.setModel(model)
    tree.expandAll()
    tree.setCurrentIndex(model.index(0, 0, model.index(0, 0)))
    content.addWidget(tree, 1)
    selected = fluentqt.Label("main.cpp", card)
    selected.setObjectName("spatialSelectedFile")
    selected.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
    content.addWidget(selected)
    tree.clicked.connect(lambda index: selected.setText(index.data()))
""") + _POSE


def full_example_source(sample_id):
    """Standalone source for the Python page and the native teaching catalog."""
    return _format_display_source(_strip_gallery_parent(_IMPORTS + _SCRIPTS[sample_id]))


def _register(sample_id, body):
    route_id = "spatial-item" if sample_id.startswith("spatial-item") else "spatial-view"

    @native_samples(route_id, sample_id)
    def build(_sample_id, parent):
        source = _IMPORTS + body
        namespace = {"gallery_parent": parent, "__name__": __name__ + "." + sample_id}
        exec(compile(source, f"<{sample_id}>", "exec"), namespace)
        panel = namespace["panel"]
        panel._fluentqt_gallery_source_namespace = namespace
        panel._spatial_binding = SpatialPreviewBinding(namespace["view"])
        return _result(
            panel, source, "SpatialView", "SpatialItem", source_driven=True,
            display_source=_USAGE_BY_ID.get(sample_id, _USAGE),
        )


for _id, _body in _SCRIPTS.items():
    _register(_id, _body)
