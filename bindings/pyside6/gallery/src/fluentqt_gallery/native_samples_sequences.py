"""Source-driven ports of the canonical Timeline and Stepper samples."""

from textwrap import dedent
from .native_samples import register_source_samples


def timeline_source(alignment):
    return dedent('''\
        import fluentqt
        from PySide6.QtCore import Qt
        from PySide6.QtGui import QStandardItemModel
        from PySide6.QtWidgets import QWidget, QVBoxLayout

        root = QWidget()
        layout = QVBoxLayout(root)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(12)
        layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        timeline = fluentqt.Timeline(root)
        timeline.setObjectName("timelineExample")
        timeline.setAccessibleName("Timeline example")
        timeline.setMinimumWidth(240)
        timeline.setMinimumHeight(320)
        roles = fluentqt.Timeline.DataRole
        states = fluentqt.Timeline.Status
        model = QStandardItemModel(3, 1, root)
        detail = "Optional supporting content supplied by the application."
        for i in range(3):
            index = model.index(i, 0)
            model.setData(index, f"Node {i + 1}")
            model.setData(index, detail, roles.DescriptionRole)
            model.setData(index, f"Time label {i + 1}", roles.TimestampRole)
        model.setData(model.index(1, 0), states.Active, roles.StatusRole)
        timeline.setModel(model)
        change = fluentqt.Button("Change node state", root)
        change.setObjectName("timelineChangeState")
        def change_state():
            index = model.index(1, 0)
            active = index.data(roles.StatusRole) == states.Active
            state = states.Success if active else states.Active
            model.setData(index, state, roles.StatusRole)
        change.clicked.connect(change_state)
        layout.addWidget(timeline)
        layout.addWidget(change)
    ''') + f"timeline.setNodeAlignment(fluentqt.Timeline.NodeAlignment.{alignment})\n"


def stepper_source(orientation):
    return dedent('''\
        import fluentqt
        from PySide6.QtCore import Qt
        from PySide6.QtWidgets import QWidget, QVBoxLayout

        root = QWidget()
        layout = QVBoxLayout(root)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(12)
        layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        stepper = fluentqt.Stepper(root)
        stepper.setObjectName("stepperExample")
        stepper.setAccessibleName("Stepper example")
        states = fluentqt.Stepper.State
        for i in range(4):
            item = fluentqt.StepperItem(f"Step {i + 1}", "Optional description")
            if i == 0:
                item.state = states.Completed
            if i == 3:
                item.enabled = False
            stepper.addItem(item)
        stepper.setCurrentIndex(1)
        stepper.setMinimumWidth(240)
        stepper.stepRequested.connect(stepper.setCurrentIndex)
        status = fluentqt.Label("Current step: 2", root)
        status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        def update_status(index):
            status.setText(f"Current step: {index + 1}")
        stepper.currentIndexChanged.connect(update_status)
        toggle = fluentqt.Button("Toggle step 2 error", root)
        toggle.setObjectName("stepperToggleError")
        def toggle_error():
            error = stepper.itemAt(1).state == states.Error
            stepper.setItemState(1, states.Pending if error else states.Error)
        toggle.clicked.connect(toggle_error)
        layout.addWidget(stepper)
        layout.addWidget(status)
        layout.addWidget(toggle)
    ''') + (
        f"stepper.setOrientation(Qt.Orientation.{orientation})\n"
        f"stepper.setMinimumHeight({120 if orientation == 'Horizontal' else 280})\n"
    )


register_source_samples("timeline", ("Timeline",), {
    "timeline-basic": ("root", timeline_source("Leading")),
    "timeline-alternate": ("root", timeline_source("Alternate")),
})
register_source_samples("stepper", ("Stepper", "StepperItem"), {
    "stepper-horizontal": ("root", stepper_source("Horizontal")),
    "stepper-vertical": ("root", stepper_source("Vertical")),
})
