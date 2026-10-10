"""Atomic Python QMLPlus examples over the shared native reactive runtime."""

from textwrap import dedent

import fluentqt
from PySide6.QtCore import Qt
from PySide6.QtWidgets import QVBoxLayout, QWidget


_SOURCES = {
    "qmlplus-binding-one-way": '''
slider = fluentqt.Slider(Qt.Horizontal, panel)
slider.setRange(0, 100)
slider.setValue(40)
bar = fluentqt.ProgressBar(panel)
bar.setRange(0, 100)
fluentqt.bind(slider, "value", bar, "value")
''',
    "qmlplus-binding-two-way": '''
first = fluentqt.ToggleSwitch(panel)
second = fluentqt.ToggleSwitch(panel)
fluentqt.bind(first, "isOn", second, "isOn", fluentqt.BindingMode.TwoWay)
''',
    "qmlplus-binding-computed": '''
first = fluentqt.Slider(Qt.Horizontal, panel)
first.setRange(0, 100)
first.setValue(20)
second = fluentqt.Slider(Qt.Horizontal, panel)
second.setRange(0, 100)
second.setValue(30)
result = fluentqt.Label("", panel)
binding = fluentqt.bind_computed(
    result, "text", [(first, "value"), (second, "value")],
    lambda: "{} + {} = {}".format(
        first.value(), second.value(), first.value() + second.value())
)
''',
    "qmlplus-binding-pause": '''
slider = fluentqt.Slider(Qt.Horizontal, panel)
slider.setRange(0, 100)
slider.setValue(40)
bar = fluentqt.ProgressBar(panel)
pause = fluentqt.ToggleSwitch(panel)
binding = fluentqt.bind_managed(slider, "value", bar, "value")
pause.toggled.connect(
    lambda on: binding.pause() if on else binding.resume()
)
''',
    "qmlplus-state-manual": '''
label = fluentqt.Label("Default", panel)
toggle = fluentqt.ToggleSwitch(panel)
states = fluentqt.StateGroup(panel)
states.add("active", {label: {"text": "Active"}})
toggle.toggled.connect(lambda on: states.set("active" if on else ""))
''',
    "qmlplus-state-conditional": '''
label = fluentqt.Label("Default", panel)
toggle = fluentqt.ToggleSwitch(panel)
states = fluentqt.StateGroup(panel)
states.add("active", {label: {"text": "Active"}},
           when=lambda: toggle.isOn(), dependencies=[(toggle, "isOn")])
''',
    "qmlplus-state-inheritance": '''
button = fluentqt.Button("Ready", panel)
toggle = fluentqt.ToggleSwitch(panel)
states = fluentqt.StateGroup(panel)
states.add("busy", {button: {"enabled": False}})
states.add("saving", {button: {"text": "Saving..."}}, extend="busy")
toggle.toggled.connect(lambda on: states.set("saving" if on else ""))
''',
    "qmlplus-state-binding-restore": '''
slider = fluentqt.Slider(Qt.Horizontal, panel)
slider.setRange(0, 100)
slider.setValue(40)
bar = fluentqt.ProgressBar(panel)
fluentqt.bind(slider, "value", bar, "value")
toggle = fluentqt.ToggleSwitch(panel)
states = fluentqt.StateGroup(panel)
states.add("override", {bar: {"value": 100.0}})
toggle.toggled.connect(lambda on: states.set("override" if on else ""))
''',
    "qmlplus-behavior": '''
slider = fluentqt.Slider(Qt.Horizontal, panel)
slider.setRange(0, 100)
slider.setValue(40)
bar = fluentqt.ProgressBar(panel)
fluentqt.set_behavior(bar, "value", duration=300)
fluentqt.bind(slider, "value", bar, "value")
''',
    "qmlplus-transition": '''
bar = fluentqt.ProgressBar(panel)
bar.setValue(20)
toggle = fluentqt.ToggleSwitch(panel)
states = fluentqt.StateGroup(panel)
states.add("high", {bar: {"value": 80.0}})
states.add_transition(["value"], duration=300)
toggle.toggled.connect(lambda on: states.set("high" if on else ""))
''',
    "qmlplus-anchors-center": '''
surface = fluentqt.Card(panel)
surface.setMinimumHeight(104)
anchors = fluentqt.AnchorLayout(surface)
button = fluentqt.Button("Centered", surface)
anchors.addWidget(button, fluentqt.anchors(center_in=surface))
''',
    "qmlplus-anchors-state": '''
surface = fluentqt.Card(panel)
surface.setMinimumHeight(104)
anchors = fluentqt.AnchorLayout(surface)
button = fluentqt.Button("Move me", surface)
toggle = fluentqt.ToggleSwitch(panel)
left = fluentqt.anchors(left=(surface, 12), vertical_center=surface)
anchors.addWidget(button, left)
right = fluentqt.anchors(right=(surface, -12), vertical_center=surface)
states = fluentqt.StateGroup(panel)
states.add("right", {}, anchor_changes={button: right})
toggle.toggled.connect(lambda on: states.set("right" if on else ""))
''',
}

# Captions, object names and layout stay in the preview host; the displayed
# statements are executed verbatim, including their binding/state setup.
_NAMES = {
    "qmlplus-binding-one-way": ("qmlplusOneWaySource", "qmlplusOneWayTarget"),
    "qmlplus-binding-two-way": ("qmlplusTwoWayFirst", "qmlplusTwoWaySecond"),
    "qmlplus-binding-computed": ("qmlplusComputedFirst", "qmlplusComputedSecond", "qmlplusComputedResult"),
    "qmlplus-binding-pause": ("qmlplusPauseSource", "qmlplusPauseTarget", "qmlplusPauseToggle"),
    "qmlplus-state-manual": ("qmlplusManualResult", "qmlplusManualToggle"),
    "qmlplus-state-conditional": ("qmlplusConditionalResult", "qmlplusConditionalToggle"),
    "qmlplus-state-inheritance": ("qmlplusInheritedTarget", "qmlplusInheritedToggle"),
    "qmlplus-state-binding-restore": ("qmlplusRestoreSource", "qmlplusRestoreTarget", "qmlplusRestoreToggle"),
    "qmlplus-behavior": ("qmlplusBehaviorSource", "qmlplusBehaviorTarget"),
    "qmlplus-transition": ("qmlplusTransitionTarget", "qmlplusTransitionToggle"),
    "qmlplus-anchors-center": ("qmlplusAnchorSurface",),
    "qmlplus-anchors-state": ("qmlplusStateAnchorSurface", "qmlplusStateAnchorToggle"),
}

_CAPTIONS = {
    "qmlplusTwoWayFirst": ("First: on", "First: off"),
    "qmlplusTwoWaySecond": ("Second: on", "Second: off"),
    "qmlplusPauseToggle": ("Paused", "Live"),
    "qmlplusManualToggle": ("Active", "Default"),
    "qmlplusConditionalToggle": ("Active", "Default"),
    "qmlplusInheritedToggle": ("Saving", "Ready"),
    "qmlplusRestoreToggle": ("Override: 100", "Follow slider"),
    "qmlplusTransitionToggle": ("High: 80", "Default: 20"),
    "qmlplusStateAnchorToggle": ("Right", "Left"),
}


def sample_ids():
    return tuple(_SOURCES)


def python_source(sample_id):
    from .native_samples import _format_display_source

    source = "import fluentqt\nfrom PySide6.QtCore import Qt\n\n" + dedent(_SOURCES[sample_id]).strip() + "\n"
    return _format_display_source(source)


def build_example(sample_id, parent=None):
    panel = QWidget(parent)
    column = QVBoxLayout(panel)
    column.setContentsMargins(0, 0, 0, 0)
    column.setSpacing(12)
    column.setAlignment(Qt.AlignTop)
    namespace = {"panel": panel, "fluentqt": fluentqt}
    exec(compile(python_source(sample_id), "<" + sample_id + ">", "exec"), namespace)
    controls = [value for value in namespace.values()
                if isinstance(value, QWidget) and value is not panel and value.parentWidget() is panel]
    for widget, name in zip(controls, _NAMES[sample_id]):
        widget.setObjectName(name)
        if isinstance(widget, fluentqt.Slider):
            widget.setMaximumWidth(220)
            widget.setAccessibleName("Source value")
        if isinstance(widget, fluentqt.ToggleSwitch):
            on, off = _CAPTIONS[name]
            widget.setOnContent(on)
            widget.setOffContent(off)
            widget.setAccessibleName(on)
            column.addWidget(widget, 0, Qt.AlignLeft)
        else:
            column.addWidget(widget)
    if sample_id.startswith("qmlplus-anchors-"):
        namespace["button"].setObjectName("qmlplusAnchorTarget" if sample_id.endswith("center")
                                             else "qmlplusStateAnchorTarget")
    panel._qmlplus_namespace = namespace
    return panel
