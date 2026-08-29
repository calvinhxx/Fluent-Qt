"""Standalone Gallery ports of native basic-input SampleCards."""

from __future__ import annotations

from textwrap import dedent

from .native_samples import register_source_samples


def _script(body: str, imports: str = "") -> str:
    prefix = "import fluentqt\n"
    if imports:
        prefix += imports.strip() + "\n"
    return prefix + "\n" + dedent(body).strip() + "\n"


_WIDGETS = "from PySide6.QtWidgets import QHBoxLayout, QVBoxLayout, QWidget"


register_source_samples(
    "button",
    ("Button",),
    {
        "button-styles": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QHBoxLayout(root)

                standard = fluentqt.Button("Standard", root)
                standard.setFluentStyle(fluentqt.Button.ButtonStyle.Standard)

                accent = fluentqt.Button("Accent", root)
                accent.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)

                subtle = fluentqt.Button("Subtle", root)
                subtle.setFluentStyle(fluentqt.Button.ButtonStyle.Subtle)

                layout.addWidget(standard)
                layout.addWidget(accent)
                layout.addWidget(subtle)
                """,
                _WIDGETS,
            ),
        ),
        "button-sizes": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QHBoxLayout(root)

                small = fluentqt.Button("Small", root)
                small.setFluentSize(fluentqt.Button.ButtonSize.Small)

                standard = fluentqt.Button("Standard", root)
                standard.setFluentSize(
                    fluentqt.Button.ButtonSize.StandardSize
                )

                large = fluentqt.Button("Large", root)
                large.setFluentSize(fluentqt.Button.ButtonSize.Large)

                layout.addWidget(small)
                layout.addWidget(standard)
                layout.addWidget(large)
                """,
                _WIDGETS,
            ),
        ),
        "button-icon-layouts": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QHBoxLayout(root)

                leading = fluentqt.Button("Icon before", root)
                leading.setFluentLayout(
                    fluentqt.Button.ButtonLayout.IconBefore
                )
                leading.setIconGlyph("\ue710")

                icon_only = fluentqt.Button("", root)
                icon_only.setFluentLayout(
                    fluentqt.Button.ButtonLayout.IconOnly
                )
                icon_only.setIconGlyph("\ue712")
                icon_only.setFixedSize(40, 40)

                trailing = fluentqt.Button("Next", root)
                trailing.setFluentLayout(
                    fluentqt.Button.ButtonLayout.IconAfter
                )
                trailing.setIconGlyph("\ue76c")

                layout.addWidget(leading)
                layout.addWidget(icon_only)
                layout.addWidget(trailing)
                """,
                _WIDGETS,
            ),
        ),
        "button-interaction-state": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QHBoxLayout(root)
                rest = fluentqt.Button("Rest", root)
                hover = fluentqt.Button("Hover", root)
                hover.setInteractionState(fluentqt.Button.InteractionState.Hover)
                pressed = fluentqt.Button("Pressed", root)
                pressed.setInteractionState(fluentqt.Button.InteractionState.Pressed)
                focused = fluentqt.Button("Focus", root)
                focused.setFocusVisual(True)
                disabled = fluentqt.Button("Disabled", root)
                disabled.setInteractionState(fluentqt.Button.InteractionState.Disabled)
                for button in (rest, hover, pressed, focused, disabled):
                    layout.addWidget(button)
                """,
                _WIDGETS,
            ),
        ),
        "button-critical-hover": (
            "button",
            _script(
                """
                button = fluentqt.Button(
                    "Delete", globals().get("gallery_parent")
                )
                button.setFluentLayout(fluentqt.Button.ButtonLayout.IconBefore)
                button.setIconGlyph("\ue74d")
                button.setCriticalOnHover(True)
                button.setInteractionState(fluentqt.Button.InteractionState.Hover)
                """
            ),
        ),
    },
)


register_source_samples(
    "compound-button",
    ("CompoundButton",),
    {
        "compound-button-content": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QHBoxLayout(root)
                standard = fluentqt.CompoundButton(
                    "Install update", "Download and restart the app", root
                )
                standard.setFixedWidth(220)
                accent = fluentqt.CompoundButton(
                    "Start trial", "No payment method required", root
                )
                accent.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
                accent.setFixedWidth(220)
                layout.addWidget(standard)
                layout.addWidget(accent)
                """,
                _WIDGETS,
            ),
        ),
        "compound-button-icon": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QHBoxLayout(root)
                share = fluentqt.CompoundButton(
                    "Share project", "Invite people with a link", root
                )
                share.setFluentLayout(fluentqt.Button.ButtonLayout.IconBefore)
                share.setIconGlyph(
                    fluentqt.Typography.Icons.Share,
                    fluentqt.Typography.IconSize.Standard,
                )
                share.setFocusVisual(True)
                share.setFixedWidth(240)
                disabled = fluentqt.CompoundButton(
                    "Publish", "Resolve validation errors first", root
                )
                disabled.setEnabled(False)
                disabled.setFixedWidth(240)
                layout.addWidget(share)
                layout.addWidget(disabled)
                """,
                _WIDGETS,
            ),
        ),
    },
)


register_source_samples(
    "checkbox",
    ("CheckBox",),
    {
        "checkbox-two-state": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                check_box = fluentqt.CheckBox("Accept terms", root)
                status = fluentqt.Label("State: Unchecked", root)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setMinimumWidth(
                    status.fontMetrics().horizontalAdvance("State: Unchecked")
                )
                check_box.stateChanged.connect(
                    lambda state: status.setText(
                        "State: Checked"
                        if Qt.CheckState(state) == Qt.CheckState.Checked
                        else "State: Unchecked"
                    )
                )
                layout.addWidget(check_box)
                layout.addWidget(status)
                """,
                "from PySide6.QtCore import Qt\n" + _WIDGETS,
            ),
        ),
        "checkbox-three-state": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                check_box = fluentqt.CheckBox("Enable selected items", root)
                check_box.setTristate(True)
                check_box.setCheckState(Qt.CheckState.PartiallyChecked)
                status = fluentqt.Label("State: Mixed", root)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setMinimumWidth(
                    status.fontMetrics().horizontalAdvance("State: Unchecked")
                )
                names = {
                    Qt.CheckState.Checked: "State: Checked",
                    Qt.CheckState.PartiallyChecked: "State: Mixed",
                    Qt.CheckState.Unchecked: "State: Unchecked",
                }
                check_box.stateChanged.connect(
                    lambda state: status.setText(names[Qt.CheckState(state)])
                )
                layout.addWidget(check_box)
                layout.addWidget(status)
                """,
                "from PySide6.QtCore import Qt\n" + _WIDGETS,
            ),
        ),
        "checkbox-select-all": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                select_all = fluentqt.CheckBox("Select all", root)
                select_all.setTristate(True)
                children = QWidget(root)
                children_layout = QVBoxLayout(children)
                children_layout.setContentsMargins(28, 0, 0, 0)
                children_layout.setSpacing(4)
                children_layout.setAlignment(
                    Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft
                )
                mail = fluentqt.CheckBox("Mail", children)
                calendar = fluentqt.CheckBox("Calendar", children)
                people = fluentqt.CheckBox("People", children)
                mail.setChecked(True)
                calendar.setChecked(True)

                def update_select_all(*_args):
                    checked = sum(item.isChecked() for item in (mail, calendar, people))
                    state = (
                        Qt.CheckState.Unchecked if checked == 0
                        else Qt.CheckState.Checked if checked == 3
                        else Qt.CheckState.PartiallyChecked
                    )
                    select_all.setCheckState(state)

                def apply_select_all(_checked=False):
                    checked = select_all.checkState() == Qt.CheckState.Checked
                    for item in (mail, calendar, people):
                        item.setChecked(checked)

                select_all.clicked.connect(apply_select_all)
                for item in (mail, calendar, people):
                    item.clicked.connect(update_select_all)
                    children_layout.addWidget(item)
                update_select_all()
                layout.addWidget(select_all)
                layout.addWidget(children)
                """,
                "from PySide6.QtCore import Qt\n" + _WIDGETS,
            ),
        ),
        "checkbox-metrics": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                large = fluentqt.CheckBox("Larger box", root)
                large.setBoxSize(24)
                compact = fluentqt.CheckBox("Compact spacing", root)
                compact.setBoxSize(16)
                compact.setBoxMargin(4)
                compact.setTextGap(6)
                hover_row = fluentqt.CheckBox("Hover row background", root)
                hover_row.setHoverBackgroundEnabled(True)
                hover_row.setCheckState(Qt.CheckState.Checked)
                for item in (large, compact, hover_row):
                    layout.addWidget(item)
                """,
                "from PySide6.QtCore import Qt\n" + _WIDGETS,
            ),
        ),
    },
)


register_source_samples(
    "color-picker",
    ("ColorPicker",),
    {
        "color-picker-rgba": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                initial = QColor(0, 120, 212, 180)
                picker = fluentqt.ColorPicker(root)
                picker.setColor(initial)
                picker.setMinimumSize(420, 480)

                status_row = QWidget(root)
                status_layout = QHBoxLayout(status_row)
                status_layout.setContentsMargins(0, 0, 0, 0)
                status_layout.setSpacing(10)
                status_layout.setAlignment(
                    Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter
                )
                swatch = QWidget(status_row)
                swatch.setFixedSize(64, 40)
                status = fluentqt.Label(status_row)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)

                def update_status(color):
                    value = color.name(QColor.NameFormat.HexArgb)
                    swatch.setStyleSheet(
                        f"background-color: {value};"
                        " border: 1px solid rgba(0, 0, 0, 48);"
                        " border-radius: 6px;"
                    )
                    status.setText(f"Color: {value.upper()}")

                update_status(initial)
                picker.colorChanged.connect(update_status)
                status_layout.addWidget(swatch)
                status_layout.addWidget(status)
                layout.addWidget(picker)
                layout.addWidget(status_row)
                """,
                "from PySide6.QtGui import QColor\n" + _WIDGETS,
            ),
        ),
        "color-picker-opaque": (
            "picker",
            _script(
                """
                picker = fluentqt.ColorPicker(globals().get("gallery_parent"))
                picker.setAlphaEnabled(False)
                picker.setColor(QColor(16, 124, 16))
                picker.setMinimumSize(420, 420)
                """,
                "from PySide6.QtGui import QColor",
            ),
        ),
    },
)


register_source_samples(
    "combobox",
    ("ComboBox",),
    {
        "combobox-selection": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                combo_box = fluentqt.ComboBox(root)
                combo_box.addItems(["Blue", "Green", "Red", "Yellow"])
                combo_box.setCurrentIndex(0)
                combo_box.setFixedWidth(200)
                status = fluentqt.Label("Selected: Blue", root)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setMinimumWidth(
                    status.fontMetrics().horizontalAdvance("Selected: Yellow")
                )
                combo_box.currentIndexChanged.connect(
                    lambda _index: status.setText(
                        f"Selected: {combo_box.currentText()}"
                    )
                )
                layout.addWidget(combo_box)
                layout.addWidget(status)
                """,
                _WIDGETS,
            ),
        ),
        "combobox-editable": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                combo_box = fluentqt.ComboBox(root)
                combo_box.setAccessibleName("Editable size value")
                combo_box.addItems(["8", "9", "10", "11", "12", "14", "16"])
                combo_box.setEditable(True)
                combo_box.lineEdit().setAccessibleName("Editable size value")
                combo_box.setInsertPolicy(fluentqt.ComboBox.InsertPolicy.NoInsert)
                combo_box.setCurrentIndex(4)
                combo_box.setFixedWidth(200)
                status = fluentqt.Label(root)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setFixedWidth(200)
                status.setTextElideMode(Qt.TextElideMode.ElideRight)

                def update_status(text):
                    match = (
                        Qt.MatchFlag.MatchFixedString
                        | Qt.MatchFlag.MatchCaseSensitive
                    )
                    kind = (
                        "Suggested"
                        if combo_box.findText(text, match) >= 0
                        else "Custom"
                    )
                    status.setText(f"{kind} value: {text or '(empty)'}")

                combo_box.editTextChanged.connect(update_status)
                update_status(combo_box.currentText())
                layout.addWidget(combo_box)
                layout.addWidget(status)
                """,
                "from PySide6.QtCore import Qt\n" + _WIDGETS,
            ),
        ),
        "combobox-many-items": (
            "combo_box",
            _script(
                """
                combo_box = fluentqt.ComboBox(globals().get("gallery_parent"))
                combo_box.addItems([f"Item {index}" for index in range(1, 21)])
                combo_box.setCurrentIndex(5)
                combo_box.setFixedWidth(200)
                """
            ),
        ),
        "combobox-appearance": (
            "combo_box",
            _script(
                """
                combo_box = fluentqt.ComboBox(globals().get("gallery_parent"))
                combo_box.addItems(["Compact", "Comfortable", "Spacious"])
                combo_box.setCurrentIndex(1)
                combo_box.setFixedWidth(180)
                combo_box.setFontRole(fluentqt.FontRole.Caption)
                combo_box.setContentPaddingH(12)
                combo_box.setChevronGlyph("\ue70d")
                combo_box.setPopupOffset(8)
                """
            ),
        ),
    },
)


register_source_samples(
    "multi-select-combobox",
    ("MultiSelectComboBox",),
    {
        "multi-select-combobox-selection": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                model = QStringListModel(
                    ["Design", "Engineering", "Research", "Support"], root
                )
                box = fluentqt.MultiSelectComboBox(root)
                box.setModel(model)
                box.setPlaceholderText("Choose teams")
                box.setAccessibleName("Teams")
                box.setSelectedRows([0, 2])
                box.setFixedWidth(280)

                status = fluentqt.Label(root)
                status.setFixedWidth(280)
                status.setWordWrap(True)

                def update_status(*_args):
                    labels = [
                        index.data() for index in box.selectedIndexes()
                    ]
                    status.setText(f"Selected: {', '.join(labels)}")

                box.selectionChanged.connect(update_status)
                update_status()
                layout.addWidget(box)
                layout.addWidget(status)
                """,
                "from PySide6.QtCore import QStringListModel\n" + _WIDGETS,
            ),
        ),
        "multi-select-combobox-search": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                model = QStringListModel(
                    [
                        "Amsterdam", "Athens", "Berlin", "Boston",
                        "Lisbon", "London", "Paris", "Prague",
                    ],
                    root,
                )
                box = fluentqt.MultiSelectComboBox(root)
                box.setModel(model)
                box.setSearchEnabled(True)
                box.setSearchPlaceholderText("Filter cities")
                box.setAccessibleName("Cities")
                box.setSelectedRows([0, 2])
                box.setFixedWidth(280)

                status = fluentqt.Label("2 selected", root)
                box.selectedCountChanged.connect(
                    lambda count, status=status: status.setText(
                        f"{count} selected"
                    )
                )
                layout.addWidget(box)
                layout.addWidget(status)
                """,
                "from PySide6.QtCore import QStringListModel\n" + _WIDGETS,
            ),
        ),
        "multi-select-combobox-model": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                model = QStringListModel(
                    ["New York", "Paris", "Tokyo"], root
                )
                box = fluentqt.MultiSelectComboBox(root)
                box.setModel(model)
                box.setAccessibleName("Locations")
                box.setSelectedRows([1])
                box.setFixedWidth(280)
                add = fluentqt.Button("Add Berlin", root)

                def add_berlin():
                    model.setStringList(model.stringList() + ["Berlin"])
                    box.setSelectedRows([1, model.rowCount() - 1])
                    add.setEnabled(False)

                add.clicked.connect(add_berlin)
                layout.addWidget(box)
                layout.addWidget(add)
                """,
                "from PySide6.QtCore import QStringListModel\n" + _WIDGETS,
            ),
        ),
    },
)


def _menu_script(
    button_type: str,
    text: str,
    title: str,
    actions: tuple[str, ...],
    accent: bool = False,
    minimum_width: int | None = None,
) -> str:
    lines = [
        "button = fluentqt.{0}({1!r}, globals().get('gallery_parent'))".format(
            button_type, text
        )
    ]
    if accent:
        lines.append(
            "button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)"
        )
    if minimum_width is not None:
        lines.append("button.setMinimumWidth({0})".format(minimum_width))
    lines.append("menu = fluentqt.FluentMenu({0!r}, button)".format(title))
    lines.extend("menu.addAction({0!r})".format(action) for action in actions)
    lines.append("button.setMenu(menu)")
    return "import fluentqt\n\n" + "\n".join(lines) + "\n"


register_source_samples(
    "dropdown-button",
    ("DropDownButton", "FluentMenu"),
    {
        "dropdown-button-menu": (
            "button",
            _menu_script(
                "DropDownButton",
                "Options",
                "Options",
                ("Edit profile", "Account settings", "Sign out"),
                minimum_width=140,
            ),
        ),
        "dropdown-button-accent": (
            "button",
            _menu_script(
                "DropDownButton",
                "Primary action",
                "Primary action",
                ("Confirm selection", "Review changes"),
                accent=True,
                minimum_width=168,
            ),
        ),
        "dropdown-button-chevron": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QHBoxLayout(root)
                up = fluentqt.DropDownButton("Chevron up", root)
                up.setChevronGlyph("\ue70e")
                up.setChevronSize(16)
                up.setMinimumWidth(150)
                more = fluentqt.DropDownButton("More actions", root)
                more.setChevronGlyph("\ue712")
                more.setChevronOffset(QPoint(16, 3))
                more.setMinimumWidth(160)
                layout.addWidget(up)
                layout.addWidget(more)
                """,
                "from PySide6.QtCore import QPoint\n" + _WIDGETS,
            ),
        ),
        "dropdown-button-icon-layout": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QHBoxLayout(root)
                icon_only = fluentqt.DropDownButton("", root)
                icon_only.setFluentLayout(fluentqt.Button.ButtonLayout.IconOnly)
                icon_only.setIconGlyph("\ue724")
                icon_only.setChevronOffset(QPoint(10, 0))
                icon_only.setFixedSize(58, 34)
                icon_text = fluentqt.DropDownButton("More actions", root)
                icon_text.setFluentLayout(fluentqt.Button.ButtonLayout.IconBefore)
                icon_text.setIconGlyph("\ue712")
                icon_text.setMinimumWidth(170)
                layout.addWidget(icon_only)
                layout.addWidget(icon_text)
                """,
                "from PySide6.QtCore import QPoint\n" + _WIDGETS,
            ),
        ),
    },
)


register_source_samples(
    "hyperlink-button",
    ("HyperlinkButton",),
    {
        "hyperlink-button-url": (
            "link",
            _script(
                """
                link = fluentqt.HyperlinkButton(
                    "calvinhxx/Fluent-Qt", globals().get("gallery_parent")
                )
                link.setUrl(QUrl("https://github.com/calvinhxx/Fluent-Qt"))
                """,
                "from PySide6.QtCore import QUrl",
            ),
        ),
        "hyperlink-button-underline": (
            "link",
            _script(
                """
                link = fluentqt.HyperlinkButton(
                    "Show underline", globals().get("gallery_parent")
                )
                link.setShowUnderline(True)
                """
            ),
        ),
    },
)


register_source_samples(
    "radio-button",
    ("RadioButton",),
    {
        "radio-button-group": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                status = fluentqt.Label("Selected: Medium", root)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                low = fluentqt.RadioButton("Low", root)
                medium = fluentqt.RadioButton("Medium", root)
                high = fluentqt.RadioButton("High", root)
                medium.setChecked(True)
                for radio in (low, medium, high):
                    radio.toggled.connect(
                        lambda checked, value=radio: status.setText(
                            f"Selected: {value.text()}"
                        ) if checked else None
                    )
                for item in (low, medium, high, status):
                    layout.addWidget(item)
                """,
                _WIDGETS,
            ),
        ),
        "radio-button-metrics": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                compact = fluentqt.RadioButton("Compact", root)
                compact.setCircleSize(16)
                compact.setTextGap(6)
                compact.setChecked(True)
                large = fluentqt.RadioButton("Large", root)
                large.setCircleSize(24)
                large.setTextGap(12)
                layout.addWidget(compact)
                layout.addWidget(large)
                """,
                _WIDGETS,
            ),
        ),
        "radio-button-disabled": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                disabled_off = fluentqt.RadioButton("Disabled off", root)
                disabled_off.setEnabled(False)
                disabled_on = fluentqt.RadioButton("Disabled on", root)
                disabled_on.setChecked(True)
                disabled_on.setEnabled(False)
                layout.addWidget(disabled_off)
                layout.addWidget(disabled_on)
                """,
                _WIDGETS,
            ),
        ),
    },
)


register_source_samples(
    "rating-control",
    ("RatingControl",),
    {
        "rating-control-value": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                rating = fluentqt.RatingControl(root)
                rating.setCaption("312 ratings")
                status = fluentqt.Label("Value: Unset", root)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                rating.valueChanged.connect(
                    lambda value: (
                        rating.setCaption("Your rating"),
                        status.setText(
                            "Value: Unset"
                            if value < 0.0
                            else f"Value: {value:.1f} stars"
                        ),
                    )
                )
                layout.addWidget(rating)
                layout.addWidget(status)
                """,
                _WIDGETS,
            ),
        ),
        "rating-control-placeholder": (
            "rating",
            _script(
                """
                rating = fluentqt.RatingControl(globals().get("gallery_parent"))
                rating.setPlaceholderValue(3.5)
                rating.setCaption("Suggested rating")
                """
            ),
        ),
        "rating-control-readonly": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                read_only = fluentqt.RatingControl(root)
                read_only.setValue(4.0)
                read_only.setIsReadOnly(True)
                read_only.setCaption("Read-only")
                disabled = fluentqt.RatingControl(root)
                disabled.setValue(2.5)
                disabled.setEnabled(False)
                disabled.setCaption("Disabled")
                layout.addWidget(read_only)
                layout.addWidget(disabled)
                """,
                _WIDGETS,
            ),
        ),
        "rating-control-max-size": (
            "rating",
            _script(
                """
                rating = fluentqt.RatingControl(globals().get("gallery_parent"))
                rating.setMaxRating(10)
                rating.setStarSize(20)
                rating.setPlaceholderValue(7.0)
                rating.setCaption("10 point scale")
                """
            ),
        ),
    },
)


register_source_samples(
    "repeat-button",
    ("RepeatButton",),
    {
        "repeat-button-counter": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                button = fluentqt.RepeatButton("Click and hold", root)
                counter = fluentqt.Label("Clicks: 0", root)
                counter.setFluentTypography(fluentqt.FontRole.Body)
                counter.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                counter.setMinimumWidth(
                    counter.fontMetrics().horizontalAdvance("Clicks: 8888")
                )
                state = [0]

                def increment():
                    state[0] += 1
                    counter.setText(f"Clicks: {state[0]}")

                button.clicked.connect(increment)
                layout.addWidget(button)
                layout.addWidget(counter)
                """,
                _WIDGETS,
            ),
        ),
        "repeat-button-timing": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QHBoxLayout(root)
                normal = fluentqt.RepeatButton("Normal", root)
                normal.setDelay(300)
                normal.setInterval(100)
                fast = fluentqt.RepeatButton("Fast", root)
                fast.setDelay(150)
                fast.setInterval(30)
                fast.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
                layout.addWidget(normal)
                layout.addWidget(fast)
                """,
                _WIDGETS,
            ),
        ),
    },
)


register_source_samples(
    "slider",
    ("Slider",),
    {
        "slider-live-value": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                slider = fluentqt.Slider(Qt.Orientation.Horizontal, root)
                slider.setRange(0, 100)
                slider.setValue(32)
                slider.setFixedWidth(260)
                status = fluentqt.Label("Value: 32", root)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setMinimumWidth(status.fontMetrics().horizontalAdvance("Value: 100"))
                slider.valueChanged.connect(lambda value: status.setText(f"Value: {value}"))
                layout.addWidget(slider)
                layout.addWidget(status)
                """,
                "from PySide6.QtCore import Qt\n" + _WIDGETS,
            ),
        ),
        "slider-range-steps": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QHBoxLayout(root)
                slider = fluentqt.Slider(Qt.Orientation.Horizontal, root)
                slider.setRange(500, 1000)
                slider.setSingleStep(10)
                slider.setPageStep(50)
                slider.setValue(800)
                slider.setFixedWidth(260)
                status = fluentqt.Label("Value: 800", root)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setMinimumWidth(status.fontMetrics().horizontalAdvance("Value: 1000"))
                slider.valueChanged.connect(lambda value: status.setText(f"Value: {value}"))
                layout.addWidget(slider)
                layout.addWidget(status)
                """,
                "from PySide6.QtCore import Qt\n" + _WIDGETS,
            ),
        ),
        "slider-tick-marks": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QHBoxLayout(root)
                slider = fluentqt.Slider(Qt.Orientation.Horizontal, root)
                slider.setRange(0, 10)
                slider.setTickInterval(1)
                slider.setTickPosition(fluentqt.Slider.TickPosition.TicksBelow)
                slider.setValue(4)
                slider.setFixedWidth(260)
                status = fluentqt.Label("Value: 4", root)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                slider.valueChanged.connect(lambda value: status.setText(f"Value: {value}"))
                layout.addWidget(slider)
                layout.addWidget(status)
                """,
                "from PySide6.QtCore import Qt\n" + _WIDGETS,
            ),
        ),
        "slider-vertical": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QHBoxLayout(root)
                slider = fluentqt.Slider(Qt.Orientation.Vertical, root)
                slider.setRange(0, 100)
                slider.setValue(25)
                slider.setFixedHeight(160)
                status = fluentqt.Label("Value: 25", root)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setMinimumWidth(status.fontMetrics().horizontalAdvance("Value: 100"))
                slider.valueChanged.connect(lambda value: status.setText(f"Value: {value}"))
                layout.addWidget(slider)
                layout.addWidget(status)
                """,
                "from PySide6.QtCore import Qt\n" + _WIDGETS,
            ),
        ),
    },
)


register_source_samples(
    "split-button",
    ("SplitButton", "FluentMenu"),
    {
        "split-button-primary-menu": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                button = fluentqt.SplitButton("Choose color", root)
                menu = fluentqt.FluentMenu("Colors", button)
                for text in ("Red", "Green", "Blue"):
                    menu.addAction(text)
                button.setMenu(menu)
                status = fluentqt.Label("Status: Ready", root)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setMinimumWidth(
                    status.fontMetrics().horizontalAdvance(
                        "Status: Primary clicked"
                    )
                )
                button.clicked.connect(
                    lambda: status.setText("Status: Primary clicked")
                )
                layout.addWidget(button)
                layout.addWidget(status)
                """,
                _WIDGETS,
            ),
        ),
        "split-button-accent": (
            "button",
            _menu_script(
                "SplitButton",
                "Submit",
                "Actions",
                ("Submit and close", "Submit and notify"),
                accent=True,
            ),
        ),
        "split-button-sizes": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QHBoxLayout(root)
                for text, size in (
                    ("Small", fluentqt.Button.ButtonSize.Small),
                    ("Standard", fluentqt.Button.ButtonSize.StandardSize),
                    ("Large", fluentqt.Button.ButtonSize.Large),
                ):
                    button = fluentqt.SplitButton(text, root)
                    button.setFluentSize(size)
                    layout.addWidget(button)
                """,
                _WIDGETS,
            ),
        ),
    },
)


register_source_samples(
    "toggle-button",
    ("ToggleButton",),
    {
        "toggle-button-state": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                toggle = fluentqt.ToggleButton("Bold", root)
                status = fluentqt.Label("State: Unchecked", root)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setMinimumWidth(
                    status.fontMetrics().horizontalAdvance("State: Unchecked")
                )
                toggle.toggled.connect(
                    lambda checked: status.setText(
                        "State: Checked" if checked else "State: Unchecked"
                    )
                )
                layout.addWidget(toggle)
                layout.addWidget(status)
                """,
                _WIDGETS,
            ),
        ),
        "toggle-button-three-state": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                toggle = fluentqt.ToggleButton("Three state", root)
                toggle.setThreeState(True)
                status = fluentqt.Label("State: Unchecked", root)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setMinimumWidth(
                    status.fontMetrics().horizontalAdvance("State: Unchecked")
                )
                names = {
                    Qt.CheckState.Checked: "State: Checked",
                    Qt.CheckState.PartiallyChecked: "State: Mixed",
                    Qt.CheckState.Unchecked: "State: Unchecked",
                }
                toggle.checkStateChanged.connect(lambda state: status.setText(names[state]))
                layout.addWidget(toggle)
                layout.addWidget(status)
                """,
                "from PySide6.QtCore import Qt\n" + _WIDGETS,
            ),
        ),
        "toggle-button-disabled": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QHBoxLayout(root)
                off = fluentqt.ToggleButton("Disabled off", root)
                off.setEnabled(False)
                on = fluentqt.ToggleButton("Disabled on", root)
                on.setChecked(True)
                on.setEnabled(False)
                layout.addWidget(off)
                layout.addWidget(on)
                """,
                _WIDGETS,
            ),
        ),
    },
)


register_source_samples(
    "toggle-split-button",
    ("ToggleSplitButton", "FluentMenu"),
    {
        "toggle-split-button-menu": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                button = fluentqt.ToggleSplitButton("List options", root)
                button.setFluentLayout(fluentqt.Button.ButtonLayout.IconBefore)
                button.setIconGlyph("\uea37")
                button.setMinimumWidth(160)
                menu = fluentqt.FluentMenu("Styles", button)
                for text in ("None", "Bulleted", "Numbered"):
                    menu.addAction(text)
                button.setMenu(menu)
                status = fluentqt.Label("State: Unchecked", root)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setMinimumWidth(
                    status.fontMetrics().horizontalAdvance("State: Unchecked")
                )
                button.toggled.connect(
                    lambda checked: status.setText(
                        "State: Checked" if checked else "State: Unchecked"
                    )
                )
                layout.addWidget(button)
                layout.addWidget(status)
                """,
                _WIDGETS,
            ),
        ),
        "toggle-split-button-icon-only": (
            "button",
            _script(
                """
                button = fluentqt.ToggleSplitButton(
                    "", globals().get("gallery_parent")
                )
                button.setFluentLayout(fluentqt.Button.ButtonLayout.IconOnly)
                button.setIconGlyph("\ue713")
                button.setFixedSize(64, 34)
                """
            ),
        ),
    },
)


register_source_samples(
    "toggle-switch",
    ("ToggleSwitch",),
    {
        "toggle-switch-state": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QVBoxLayout(root)
                toggle = fluentqt.ToggleSwitch(root)
                toggle.setOnContent("On")
                toggle.setOffContent("Off")
                status = fluentqt.Label("State: Off", root)
                status.setFluentTypography(fluentqt.FontRole.Body)
                status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                status.setMinimumWidth(
                    status.fontMetrics().horizontalAdvance("State: Off")
                )
                toggle.toggled.connect(
                    lambda on: status.setText("State: On" if on else "State: Off")
                )
                layout.addWidget(toggle)
                layout.addWidget(status)
                """,
                _WIDGETS,
            ),
        ),
        "toggle-switch-content": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QHBoxLayout(root)
                label = fluentqt.Label("Wi-Fi", root)
                label.setFluentTypography(fluentqt.FontRole.Body)
                label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
                toggle = fluentqt.ToggleSwitch(root)
                toggle.setOnContent("Connected")
                toggle.setOffContent("Disconnected")
                toggle.setIsOn(True)
                layout.addWidget(label)
                layout.addWidget(toggle)
                """,
                _WIDGETS,
            ),
        ),
        "toggle-switch-disabled": (
            "root",
            _script(
                """
                root = QWidget()
                layout = QHBoxLayout(root)
                off = fluentqt.ToggleSwitch(root)
                off.setOffContent("Off")
                off.setEnabled(False)
                on = fluentqt.ToggleSwitch(root)
                on.setOnContent("On")
                on.setIsOn(True)
                on.setEnabled(False)
                layout.addWidget(off)
                layout.addWidget(on)
                """,
                _WIDGETS,
            ),
        ),
    },
)
