"""Live preview builders used by the standalone PySide6 Gallery."""

from __future__ import annotations

from dataclasses import dataclass
import fluentqt
from PySide6.QtCore import QDate, QSize, Qt, QTime, QUrl
from PySide6.QtGui import QAction, QColor, QStandardItem, QStandardItemModel
from PySide6.QtWidgets import QHBoxLayout, QVBoxLayout, QWidget

from .catalog import ENTRY_BY_NAME, ENTRY_BY_ROUTE_ID


@dataclass
class PreviewResult:
    """A live preview and the semantically matching source snippet."""

    widget: QWidget
    source: str
    covered_types: tuple[str, ...]
    preview_source: str = ""
    route_id: str = ""
    sample_id: str = ""
    parity_level: str = "component-smoke"
    source_driven: bool = False

    def __post_init__(self) -> None:
        # ``source`` is the concise, user-facing example.  Exact Gallery
        # previews sometimes need sizeable model/delegate/drawing helpers to
        # match the C++ app pixel-for-pixel; keep that executable implementation
        # separate so those app-only details never leak into the teaching code.
        if not self.preview_source:
            self.preview_source = self.source


def _source(*lines: str) -> str:
    return "import fluentqt\n\n" + "\n".join(lines) + "\n"


def _column(parent: QWidget | None = None, minimum_height: int = 132):
    root = QWidget(parent)
    root.setMinimumWidth(420)
    root.setMinimumHeight(minimum_height)
    layout = QVBoxLayout(root)
    layout.setContentsMargins(8, 8, 8, 8)
    layout.setSpacing(10)
    return root, layout


def _hold(owner: QWidget, *values: object) -> None:
    owner._fluentqt_gallery_references = values


def _card_page(title: str, detail: str) -> fluentqt.Card:
    page = fluentqt.Card()
    layout = QVBoxLayout(page)
    layout.setContentsMargins(18, 16, 18, 16)
    heading = fluentqt.Label(title, page)
    heading.setFluentTypography(fluentqt.FontRole.BodyStrong)
    description = fluentqt.Label(detail, page)
    description.setWordWrap(True)
    layout.addWidget(heading)
    layout.addWidget(description)
    layout.addStretch()
    return page


def _simple_preview(name: str, parent: QWidget | None) -> PreviewResult | None:
    if name == "FontIcon":
        control = fluentqt.FontIcon("ic_fluent_settings_20_regular")
        control.setParent(parent)
        control.setFixedSize(56, 56)
        return PreviewResult(
            control,
            _source(
                'icon = fluentqt.FontIcon("ic_fluent_settings_20_regular")',
                "icon.setFixedSize(56, 56)",
            ),
            (name,),
        )

    text_controls = {
        "Button": ("Run action", fluentqt.Button),
        "CheckBox": ("Enable notifications", fluentqt.CheckBox),
        "HyperlinkButton": ("FluentQt repository", fluentqt.HyperlinkButton),
        "RadioButton": ("Recommended option", fluentqt.RadioButton),
        "RepeatButton": ("Click and hold", fluentqt.RepeatButton),
        "ToggleButton": ("Pinned", fluentqt.ToggleButton),
    }
    if name in text_controls:
        text, control_type = text_controls[name]
        control = control_type(text, parent)
        lines = ['control = fluentqt.{0}("{1}")'.format(name, text)]
        if name == "Button":
            control.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
            lines.append("control.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)")
        elif name == "CheckBox":
            control.setChecked(True)
            lines.append("control.setChecked(True)")
        elif name == "HyperlinkButton":
            control.setUrl(QUrl("https://github.com/calvinhxx/Fluent-Qt"))
            control.setShowUnderline(True)
            lines.extend(
                (
                    'control.setUrl(QUrl("https://github.com/calvinhxx/Fluent-Qt"))',
                    "control.setShowUnderline(True)",
                )
            )
        elif name == "RadioButton":
            control.setChecked(True)
            lines.append("control.setChecked(True)")
        elif name == "RepeatButton":
            control.setDelay(250)
            control.setInterval(80)
            lines.extend(("control.setDelay(250)", "control.setInterval(80)"))
        elif name == "ToggleButton":
            control.setChecked(True)
            lines.append("control.setChecked(True)")
        prefix = "from PySide6.QtCore import QUrl\n" if name == "HyperlinkButton" else ""
        return PreviewResult(control, prefix + _source(*lines), (name,))

    if name == "CompoundButton":
        control = fluentqt.CompoundButton(
            "Install update", "Download, install, and restart", parent
        )
        control.setMinimumWidth(300)
        return PreviewResult(
            control,
            _source(
                'control = fluentqt.CompoundButton(',
                '    "Install update",',
                '    "Download, install, and restart",',
                ")",
                "control.setMinimumWidth(300)",
            ),
            (name,),
        )

    if name == "ColorPicker":
        control = fluentqt.ColorPicker(parent)
        control.setColor(QColor("#5B5FC7"))
        control.setAlphaEnabled(True)
        return PreviewResult(
            control,
            "from PySide6.QtGui import QColor\n" + _source(
                "control = fluentqt.ColorPicker()",
                'control.setColor(QColor("#5B5FC7"))',
                "control.setAlphaEnabled(True)",
            ),
            (name,),
        )

    if name == "ComboBox":
        control = fluentqt.ComboBox(parent)
        control.addItems(["Fluent", "Material", "macOS"])
        control.setCurrentIndex(0)
        control.setMinimumWidth(220)
        return PreviewResult(
            control,
            _source(
                "control = fluentqt.ComboBox()",
                'control.addItems(["Fluent", "Material", "macOS"])',
                "control.setCurrentIndex(0)",
                "control.setMinimumWidth(220)",
            ),
            (name,),
        )

    if name == "RatingControl":
        control = fluentqt.RatingControl(parent)
        control.setValue(4.0)
        control.setCaption("4 of 5")
        return PreviewResult(
            control,
            _source(
                "control = fluentqt.RatingControl()",
                "control.setValue(4.0)",
                'control.setCaption("4 of 5")',
            ),
            (name,),
        )

    if name == "Slider":
        control = fluentqt.Slider(Qt.Horizontal, parent)
        control.setRange(0, 100)
        control.setValue(64)
        control.setMinimumWidth(280)
        return PreviewResult(
            control,
            "from PySide6.QtCore import Qt\n" + _source(
                "control = fluentqt.Slider(Qt.Horizontal)",
                "control.setRange(0, 100)",
                "control.setValue(64)",
                "control.setMinimumWidth(280)",
            ),
            (name,),
        )

    if name == "ToggleSwitch":
        control = fluentqt.ToggleSwitch(parent)
        control.setOnContent("On")
        control.setOffContent("Off")
        control.setIsOn(True)
        return PreviewResult(
            control,
            _source(
                "control = fluentqt.ToggleSwitch()",
                'control.setOnContent("On")',
                'control.setOffContent("Off")',
                "control.setIsOn(True)",
            ),
            (name,),
        )

    if name == "Label":
        control = fluentqt.Label("Fluent typography from Python", parent)
        control.setFluentTypography(fluentqt.FontRole.Title)
        return PreviewResult(
            control,
            _source(
                'control = fluentqt.Label("Fluent typography from Python")',
                "control.setFluentTypography(fluentqt.FontRole.Title)",
            ),
            (name,),
        )

    if name == "LineEdit":
        control = fluentqt.LineEdit(parent)
        control.setPlaceholderText("Type a message")
        control.setMinimumWidth(300)
        return PreviewResult(
            control,
            _source(
                "control = fluentqt.LineEdit()",
                'control.setPlaceholderText("Type a message")',
                "control.setMinimumWidth(300)",
            ),
            (name,),
        )

    if name == "PasswordBox":
        control = fluentqt.PasswordBox(parent)
        control.setHeader("Password")
        control.setPlaceholderText("Enter password")
        control.setMinimumWidth(300)
        return PreviewResult(
            control,
            _source(
                "control = fluentqt.PasswordBox()",
                'control.setHeader("Password")',
                'control.setPlaceholderText("Enter password")',
                "control.setMinimumWidth(300)",
            ),
            (name,),
        )

    if name == "NumberBox":
        control = fluentqt.NumberBox(parent)
        control.setHeader("Quantity")
        control.setRange(0.0, 10.0)
        control.setValue(3.0)
        control.setMinimumWidth(300)
        return PreviewResult(
            control,
            _source(
                "control = fluentqt.NumberBox()",
                'control.setHeader("Quantity")',
                "control.setRange(0.0, 10.0)",
                "control.setValue(3.0)",
                "control.setMinimumWidth(300)",
            ),
            (name,),
        )

    if name == "TextEdit":
        control = fluentqt.TextEdit(parent)
        control.setPlainText("The Python Gallery uses the public TextEdit API.")
        control.setReadOnly(True)
        control.setMinimumSize(420, 120)
        return PreviewResult(
            control,
            _source(
                "control = fluentqt.TextEdit()",
                'control.setPlainText("The Python Gallery uses the public TextEdit API.")',
                "control.setReadOnly(True)",
                "control.setMinimumSize(420, 120)",
            ),
            (name,),
        )

    if name == "AutoSuggestBox":
        control = fluentqt.AutoSuggestBox(parent)
        control.setHeader("Find a component")
        control.setPlaceholderText("Start typing")
        control.setSuggestions(["Button", "NavigationView", "Window"])
        control.setMinimumWidth(320)
        return PreviewResult(
            control,
            _source(
                "control = fluentqt.AutoSuggestBox()",
                'control.setHeader("Find a component")',
                'control.setPlaceholderText("Start typing")',
                'control.setSuggestions(["Button", "NavigationView", "Window"])',
                "control.setMinimumWidth(320)",
            ),
            (name,),
        )

    return None


def _menu_preview(name: str, parent: QWidget | None) -> PreviewResult:
    root, layout = _column(parent)
    label = name if name in {"DropDownButton", "SplitButton", "ToggleSplitButton"} else "Fluent menu"
    button_type = {
        "SplitButton": fluentqt.SplitButton,
        "ToggleSplitButton": fluentqt.ToggleSplitButton,
    }.get(name, fluentqt.DropDownButton)
    button = button_type(label, root)
    button.setMinimumWidth(220)
    menu = fluentqt.FluentMenu("Commands", button)
    first = fluentqt.FluentMenuItem("Open", menu)
    second = fluentqt.FluentMenuItem("Save", menu)
    menu.addAction(first)
    menu.addAction(second)
    button.setMenu(menu)
    layout.addWidget(button, 0, Qt.AlignLeft)
    layout.addStretch()
    _hold(root, button, menu, first, second)
    return PreviewResult(
        root,
        _source(
            'button = fluentqt.{0}("{1}")'.format(button_type.__name__, label),
            'menu = fluentqt.FluentMenu("Commands", button)',
            'open_item = fluentqt.FluentMenuItem("Open", menu)',
            'save_item = fluentqt.FluentMenuItem("Save", menu)',
            "menu.addAction(open_item)",
            "menu.addAction(save_item)",
            "button.setMenu(menu)",
        ),
        tuple(
            item
            for item in (name, "FluentMenu", "FluentMenuItem")
            if item in ENTRY_BY_NAME
        ),
    )


def _layout_preview(name: str, parent: QWidget | None) -> PreviewResult:
    if name == "Card":
        card = _card_page("Card surface", "Token-driven background, border, and radius.")
        card.setParent(parent)
        card.setMinimumSize(420, 140)
        return PreviewResult(
            card,
            "from PySide6.QtWidgets import QVBoxLayout\n" + _source(
                "card = fluentqt.Card()",
                "layout = QVBoxLayout(card)",
                'title = fluentqt.Label("Card surface", card)',
                'detail = fluentqt.Label("Token-driven background, border, and radius.", card)',
                "detail.setWordWrap(True)",
                "layout.addWidget(title)",
                "layout.addWidget(detail)",
                "layout.addStretch()",
                "card.setMinimumSize(420, 140)",
            ),
            (name,),
        )

    if name == "Divider":
        root, layout = _column(parent)
        layout.addWidget(fluentqt.Label("Above the divider", root))
        layout.addWidget(fluentqt.Divider(root))
        layout.addWidget(fluentqt.Label("Below the divider", root))
        layout.addStretch()
        return PreviewResult(
            root,
            "from PySide6.QtWidgets import QVBoxLayout, QWidget\n" + _source(
                "root = QWidget()",
                "layout = QVBoxLayout(root)",
                'above = fluentqt.Label("Above the divider", root)',
                "divider = fluentqt.Divider(root)",
                'below = fluentqt.Label("Below the divider", root)',
                "layout.addWidget(above)",
                "layout.addWidget(divider)",
                "layout.addWidget(below)",
                "layout.addStretch()",
            ),
            (name,),
        )

    def make_item(title: str, detail: str) -> fluentqt.Expander:
        item = fluentqt.Expander()
        item.setHeaderText(title)
        item.setAnimationEnabled(False)
        item.setOwnedContentWidget(_card_page(title, detail))
        return item

    if name == "Expander":
        item = make_item("Hosted Python content", "Owned content follows the Expander lifetime.")
        item.setParent(parent)
        item.setExpanded(True)
        return PreviewResult(
            item,
            "from PySide6.QtWidgets import QVBoxLayout\n" + _source(
                "content = fluentqt.Card()",
                "content_layout = QVBoxLayout(content)",
                'content_layout.addWidget(fluentqt.Label("Hosted Python content", content))',
                'detail = fluentqt.Label("Owned content follows the Expander lifetime.", content)',
                "detail.setWordWrap(True)",
                "content_layout.addWidget(detail)",
                "expander = fluentqt.Expander()",
                'expander.setHeaderText("Hosted Python content")',
                "expander.setAnimationEnabled(False)",
                "expander.setOwnedContentWidget(content)",
                "expander.setExpanded(True)",
            ),
            (name,),
        )

    accordion = fluentqt.Accordion(parent)
    first = make_item("First item", "The Accordion coordinates a native disclosure group.")
    second = make_item("Second item", "Each child keeps an explicit Python ownership policy.")
    accordion.addOwnedItem(first)
    accordion.addOwnedItem(second)
    first.setExpanded(True)
    return PreviewResult(
        accordion,
        "from PySide6.QtWidgets import QVBoxLayout\n" + _source(
            "def make_item(title, detail):",
            "    content = fluentqt.Card()",
            "    content_layout = QVBoxLayout(content)",
            "    content_layout.addWidget(fluentqt.Label(title, content))",
            "    description = fluentqt.Label(detail, content)",
            "    description.setWordWrap(True)",
            "    content_layout.addWidget(description)",
            "    item = fluentqt.Expander()",
            "    item.setHeaderText(title)",
            "    item.setAnimationEnabled(False)",
            "    item.setOwnedContentWidget(content)",
            "    return item",
            "",
            'first_expander = make_item("First item", "The Accordion coordinates a native disclosure group.")',
            'second_expander = make_item("Second item", "Each child keeps an explicit Python ownership policy.")',
            "accordion = fluentqt.Accordion()",
            "accordion.addOwnedItem(first_expander)",
            "accordion.addOwnedItem(second_expander)",
            "first_expander.setExpanded(True)",
        ),
        ("Accordion", "Expander"),
    )


def _model_view_preview(name: str, parent: QWidget | None) -> PreviewResult:
    root, layout = _column(parent, 250)
    view_type = getattr(fluentqt, name)
    view = view_type(root)
    model = QStandardItemModel(view)
    if name == "TreeView":
        project = QStandardItem("FluentQt")
        project.appendRow(QStandardItem("Python Gallery"))
        project.appendRow(QStandardItem("Bindings"))
        model.appendRow(project)
        view.setModel(model)
        view.expandAll()
    else:
        for text in ("Alpha", "Bravo", "Charlie", "Delta", "Echo"):
            model.appendRow(QStandardItem(text))
        view.setModel(model)
    if hasattr(view, "setHeaderText"):
        view.setHeaderText("Python model")
    if name == "FlowView":
        view.setDefaultItemSize(QSize(120, 56))
    view.setMinimumSize(440, 220)
    layout.addWidget(view)
    _hold(root, view, model)
    lines = ["view = fluentqt.{0}()".format(name), "model = QStandardItemModel(view)"]
    if name == "TreeView":
        lines.extend(
            (
                'project = QStandardItem("FluentQt")',
                'project.appendRow(QStandardItem("Python Gallery"))',
                'project.appendRow(QStandardItem("Bindings"))',
                "model.appendRow(project)",
                "view.setModel(model)",
                "view.expandAll()",
            )
        )
    else:
        lines.extend(
            (
                'for text in ("Alpha", "Bravo", "Charlie", "Delta", "Echo"):',
                "    model.appendRow(QStandardItem(text))",
                "view.setModel(model)",
            )
        )
    if hasattr(view, "setHeaderText"):
        lines.append('view.setHeaderText("Python model")')
    if name == "FlowView":
        lines.append("view.setDefaultItemSize(QSize(120, 56))")
    lines.append("view.setMinimumSize(440, 220)")
    return PreviewResult(
        root,
        "from PySide6.QtCore import QSize\nfrom PySide6.QtGui import QStandardItem, QStandardItemModel\n"
        + _source(*lines),
        (name,),
    )


def _hosted_collection_preview(name: str, parent: QWidget | None) -> PreviewResult:
    if name == "DrawerView":
        drawer = fluentqt.DrawerView(parent)
        drawer.setMinimumSize(480, 240)
        drawer.setAnimationEnabled(False)
        drawer.setDrawerLength(220)
        drawer.setOwnedContentWidget(
            _card_page("Drawer content", "Owned Python content in an edge surface.")
        )
        drawer.setIsOpen(True)
        return PreviewResult(
            drawer,
            "from PySide6.QtWidgets import QVBoxLayout\n" + _source(
                "content = fluentqt.Card()",
                "content_layout = QVBoxLayout(content)",
                'content_layout.addWidget(fluentqt.Label("Drawer content", content))',
                'detail = fluentqt.Label("Owned Python content in an edge surface.", content)',
                "detail.setWordWrap(True)",
                "content_layout.addWidget(detail)",
                "drawer = fluentqt.DrawerView()",
                "drawer.setMinimumSize(480, 240)",
                "drawer.setAnimationEnabled(False)",
                "drawer.setDrawerLength(220)",
                "drawer.setOwnedContentWidget(content)",
                "drawer.setIsOpen(True)",
            ),
            (name,),
        )

    if name == "FlipView":
        view = fluentqt.FlipView(parent)
        view.setMinimumSize(480, 220)
        view.addOwnedPage(_card_page("Page one", "Owned by the native FlipView."))
        view.addOwnedPage(_card_page("Page two", "Use arrows or keyboard navigation."))
        view.setCurrentIndex(0)
        return PreviewResult(
            view,
            "from PySide6.QtWidgets import QVBoxLayout\n" + _source(
                "first_page = fluentqt.Card()",
                "first_layout = QVBoxLayout(first_page)",
                'first_layout.addWidget(fluentqt.Label("Page one", first_page))',
                'first_detail = fluentqt.Label("Owned by the native FlipView.", first_page)',
                "first_detail.setWordWrap(True)",
                "first_layout.addWidget(first_detail)",
                "second_page = fluentqt.Card()",
                "second_layout = QVBoxLayout(second_page)",
                'second_layout.addWidget(fluentqt.Label("Page two", second_page))',
                'second_detail = fluentqt.Label("Use arrows or keyboard navigation.", second_page)',
                "second_detail.setWordWrap(True)",
                "second_layout.addWidget(second_detail)",
                "view = fluentqt.FlipView()",
                "view.setMinimumSize(480, 220)",
                "view.addOwnedPage(first_page)",
                "view.addOwnedPage(second_page)",
                "view.setCurrentIndex(0)",
            ),
            (name,),
        )

    if name in {"SplitView", "SplitViewPaneOptions"}:
        view = fluentqt.SplitView(parent)
        view.setMinimumSize(500, 220)
        first = _card_page("Navigation", "A resizable owned pane.")
        second = _card_page("Workspace", "The fill pane consumes remaining space.")
        first_options = fluentqt.SplitViewPaneOptions(120, 160, 220)
        second_options = fluentqt.SplitViewPaneOptions(220, 340, 700, True)
        view.addOwnedPane(first, first_options)
        view.addOwnedPane(second, second_options)
        return PreviewResult(
            view,
            "from PySide6.QtWidgets import QVBoxLayout\n" + _source(
                "navigation = fluentqt.Card()",
                "navigation_layout = QVBoxLayout(navigation)",
                'navigation_layout.addWidget(fluentqt.Label("Navigation", navigation))',
                'navigation_detail = fluentqt.Label("A resizable owned pane.", navigation)',
                "navigation_detail.setWordWrap(True)",
                "navigation_layout.addWidget(navigation_detail)",
                "workspace = fluentqt.Card()",
                "workspace_layout = QVBoxLayout(workspace)",
                'workspace_layout.addWidget(fluentqt.Label("Workspace", workspace))',
                'workspace_detail = fluentqt.Label("The fill pane consumes remaining space.", workspace)',
                "workspace_detail.setWordWrap(True)",
                "workspace_layout.addWidget(workspace_detail)",
                "view = fluentqt.SplitView()",
                "view.setMinimumSize(500, 220)",
                "navigation_options = fluentqt.SplitViewPaneOptions(120, 160, 220)",
                "workspace_options = fluentqt.SplitViewPaneOptions(220, 340, 700, True)",
                "view.addOwnedPane(navigation, navigation_options)",
                "view.addOwnedPane(workspace, workspace_options)",
            ),
            ("SplitView", "SplitViewPaneOptions"),
        )

    stack = fluentqt.StackView(parent)
    stack.setMinimumSize(480, 220)
    stack.setTransitionAnimationEnabled(False)
    stack.pushOwnedItem(_card_page("Stack page", "Pushed through the fixed ownership facade."))
    return PreviewResult(
        stack,
        "from PySide6.QtWidgets import QVBoxLayout\n" + _source(
            "page = fluentqt.Card()",
            "page_layout = QVBoxLayout(page)",
            'page_layout.addWidget(fluentqt.Label("Stack page", page))',
            'detail = fluentqt.Label("Pushed through the fixed ownership facade.", page)',
            "detail.setWordWrap(True)",
            "page_layout.addWidget(detail)",
            "stack = fluentqt.StackView()",
            "stack.setMinimumSize(480, 220)",
            "stack.setTransitionAnimationEnabled(False)",
            "stack.pushOwnedItem(page)",
        ),
        ("StackView",),
    )


def _date_time_preview(name: str, parent: QWidget | None) -> PreviewResult:
    control = getattr(fluentqt, name)(parent)
    lines = ["control = fluentqt.{0}()".format(name)]
    if name == "CalendarView":
        control.setSelectedDate(QDate(2026, 8, 3))
        control.setMinimumSize(420, 330)
        lines.extend(
            (
                "control.setSelectedDate(QDate(2026, 8, 3))",
                "control.setMinimumSize(420, 330)",
            )
        )
    elif name == "CalendarDatePicker":
        control.setDate(QDate(2026, 8, 3))
        control.setPlaceholderText("Choose a date")
        lines.extend(
            (
                "control.setDate(QDate(2026, 8, 3))",
                'control.setPlaceholderText("Choose a date")',
            )
        )
    elif name == "DatePicker":
        control.setSelectedDate(QDate(2026, 8, 3))
        lines.append("control.setSelectedDate(QDate(2026, 8, 3))")
    else:
        control.setSelectedTime(QTime(9, 30))
        control.setMinuteIncrement(5)
        lines.extend(
            (
                "control.setSelectedTime(QTime(9, 30))",
                "control.setMinuteIncrement(5)",
            )
        )
    control.setMinimumWidth(320)
    lines.append("control.setMinimumWidth(320)")
    return PreviewResult(
        control,
        "from PySide6.QtCore import QDate, QTime\n" + _source(*lines),
        (name,),
    )


def _overlay_preview(name: str, parent: QWidget | None) -> PreviewResult:
    root, layout = _column(parent, 170)
    trigger = fluentqt.Button("Open {0}".format(name), root)
    trigger.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    status = fluentqt.Label("Closed", root)
    layout.addWidget(trigger, 0, Qt.AlignLeft)
    layout.addWidget(status)
    layout.addStretch()

    source_lines = [
        "page = QWidget()",
        "layout = QVBoxLayout(page)",
        'open_button = fluentqt.Button("Open {0}", page)'.format(name),
        "open_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)",
        'status = fluentqt.Label("Closed", page)',
        "layout.addWidget(open_button)",
        "layout.addWidget(status)",
        "layout.addStretch()",
    ]

    if name == "ContentDialog":
        surface = fluentqt.ContentDialog(root)
        surface.setFixedSize(460, 280)
        surface.setAnimationEnabled(False)
        surface.setTitle("Save changes?")
        surface.setPrimaryButtonText("Save")
        surface.setCloseButtonText("Cancel")
        surface.setContent(_card_page("Draft", "The content widget is owned by ContentDialog."))
        trigger.clicked.connect(surface.open)
        surface_name = "dialog"
        source_lines.extend(
            (
                "content = fluentqt.Card()",
                "content_layout = QVBoxLayout(content)",
                'content_layout.addWidget(fluentqt.Label("Draft", content))',
                'description = fluentqt.Label("The content widget is owned by ContentDialog.", content)',
                "description.setWordWrap(True)",
                "content_layout.addWidget(description)",
                "dialog = fluentqt.ContentDialog(page)",
                "dialog.setFixedSize(460, 280)",
                "dialog.setAnimationEnabled(False)",
                'dialog.setTitle("Save changes?")',
                'dialog.setPrimaryButtonText("Save")',
                'dialog.setCloseButtonText("Cancel")',
                "dialog.setContent(content)",
                "open_button.clicked.connect(dialog.open)",
            )
        )
    elif name == "Dialog":
        surface = fluentqt.Dialog(root)
        surface.setFixedSize(420, 220)
        surface.setAnimationEnabled(False)
        surface_layout = QVBoxLayout(surface)
        surface_layout.addWidget(fluentqt.Label("Same-window Dialog", surface))
        trigger.clicked.connect(surface.open)
        surface_name = "dialog"
        source_lines.extend(
            (
                "dialog = fluentqt.Dialog(page)",
                "dialog.setFixedSize(420, 220)",
                "dialog.setAnimationEnabled(False)",
                "dialog_layout = QVBoxLayout(dialog)",
                'dialog_layout.addWidget(fluentqt.Label("Same-window Dialog", dialog))',
                "open_button.clicked.connect(dialog.open)",
            )
        )
    elif name == "Popup":
        surface = fluentqt.Popup(root)
        surface.setFixedSize(340, 180)
        surface.setAnimationEnabled(False)
        surface_layout = QVBoxLayout(surface)
        surface_layout.addWidget(fluentqt.Label("Light-dismiss Popup", surface))
        trigger.clicked.connect(surface.open)
        surface_name = "popup"
        source_lines.extend(
            (
                "popup = fluentqt.Popup(page)",
                "popup.setFixedSize(340, 180)",
                "popup.setAnimationEnabled(False)",
                "popup_layout = QVBoxLayout(popup)",
                'popup_layout.addWidget(fluentqt.Label("Light-dismiss Popup", popup))',
                "open_button.clicked.connect(popup.open)",
            )
        )
    elif name == "Flyout":
        surface = fluentqt.Flyout(root)
        surface.setFixedSize(340, 180)
        surface.setAnimationEnabled(False)
        surface.setAnchor(trigger)
        surface_layout = QVBoxLayout(surface)
        surface_layout.addWidget(fluentqt.Label("Anchored Flyout", surface))
        trigger.clicked.connect(lambda: surface.showAt(trigger))
        surface_name = "flyout"
        source_lines.extend(
            (
                "flyout = fluentqt.Flyout(page)",
                "flyout.setFixedSize(340, 180)",
                "flyout.setAnimationEnabled(False)",
                "flyout.setAnchor(open_button)",
                "flyout_layout = QVBoxLayout(flyout)",
                'flyout_layout.addWidget(fluentqt.Label("Anchored Flyout", flyout))',
                "open_button.clicked.connect(lambda: flyout.showAt(open_button))",
            )
        )
    elif name == "CoachMark":
        surface = fluentqt.CoachMark(root)
        surface.setTarget(trigger)
        surface.setCardSize(QSize(300, 150))
        surface.setPlacement(fluentqt.CoachMark.Placement.Top)
        coach_layout = QVBoxLayout(surface.contentHost())
        coach_layout.addWidget(fluentqt.Label("Contextual guidance", surface.contentHost()))
        trigger.clicked.connect(surface.open)
        surface_name = "coach"
        source_lines.extend(
            (
                "coach = fluentqt.CoachMark(page)",
                "coach.setTarget(open_button)",
                "coach.setCardSize(QSize(300, 150))",
                "coach.setPlacement(fluentqt.CoachMark.Placement.Top)",
                "coach_layout = QVBoxLayout(coach.contentHost())",
                'coach_layout.addWidget(fluentqt.Label("Contextual guidance", coach.contentHost()))',
                "open_button.clicked.connect(coach.open)",
            )
        )
    else:
        surface = fluentqt.TeachingTip(root)
        surface.setTarget(trigger)
        surface.setAnimationEnabled(False)
        surface.setCardSize(QSize(320, 170))
        surface.setPreferredPlacement(fluentqt.TeachingTip.PreferredPlacement.Top)
        tip_layout = QVBoxLayout(surface.contentHost())
        tip_layout.addWidget(fluentqt.Label("Teaching tip content", surface.contentHost()))
        trigger.clicked.connect(lambda: surface.showAt(trigger))
        surface_name = "tip"
        source_lines.extend(
            (
                "tip = fluentqt.TeachingTip(page)",
                "tip.setTarget(open_button)",
                "tip.setAnimationEnabled(False)",
                "tip.setCardSize(QSize(320, 170))",
                "tip.setPreferredPlacement(fluentqt.TeachingTip.PreferredPlacement.Top)",
                "tip_layout = QVBoxLayout(tip.contentHost())",
                'tip_layout.addWidget(fluentqt.Label("Teaching tip content", tip.contentHost()))',
                "open_button.clicked.connect(lambda: tip.showAt(open_button))",
            )
        )
    if hasattr(surface, "isOpenChanged"):
        surface.isOpenChanged.connect(
            lambda opened: status.setText("Open" if opened else "Closed")
        )
    source_lines.extend(
        (
            'if hasattr({0}, "isOpenChanged"):'.format(surface_name),
            "    {0}.isOpenChanged.connect(".format(surface_name),
            '        lambda opened: status.setText("Open" if opened else "Closed")',
            "    )",
        )
    )
    source = (
        "from PySide6.QtCore import QSize\n"
        "from PySide6.QtWidgets import QVBoxLayout, QWidget\n"
        + _source(*source_lines)
    )
    _hold(root, trigger, status, surface)
    return PreviewResult(root, source, (name,))


def _command_preview(name: str, parent: QWidget | None) -> PreviewResult:
    if name in {"FluentMenu", "FluentMenuItem"}:
        return _menu_preview(name, parent)

    root, layout = _column(parent, 170)
    save = QAction("Save", root)
    share = QAction("Share", root)
    delete = QAction("Delete", root)
    if name == "FluentMenuBar":
        bar = fluentqt.FluentMenuBar(root)
        file_menu = fluentqt.FluentMenu("File", bar)
        file_menu.addAction(fluentqt.FluentMenuItem("Open", file_menu))
        file_menu.addAction(fluentqt.FluentMenuItem("Save", file_menu))
        bar.addMenu(file_menu)
        layout.addWidget(bar)
        layout.addStretch()
        _hold(root, bar, file_menu)
        return PreviewResult(
            root,
            _source(
                "bar = fluentqt.FluentMenuBar()",
                'file_menu = fluentqt.FluentMenu("File", bar)',
                'file_menu.addAction(fluentqt.FluentMenuItem("Open", file_menu))',
                'file_menu.addAction(fluentqt.FluentMenuItem("Save", file_menu))',
                "bar.addMenu(file_menu)",
            ),
            (name, "FluentMenu", "FluentMenuItem"),
        )

    if name == "CommandBar":
        bar = fluentqt.CommandBar(root)
        bar.addPrimaryAction(save)
        bar.addPrimaryAction(share)
        bar.addSecondaryAction(delete)
        layout.addWidget(bar)
        layout.addStretch()
        _hold(root, bar, save, share, delete)
        return PreviewResult(
            root,
            "from PySide6.QtGui import QAction\n" + _source(
                "bar = fluentqt.CommandBar()",
                'save = QAction("Save", bar)',
                'share = QAction("Share", bar)',
                'delete = QAction("Delete", bar)',
                "bar.addPrimaryAction(save)",
                "bar.addPrimaryAction(share)",
                "bar.addSecondaryAction(delete)",
            ),
            (name,),
        )

    anchor = fluentqt.Button("Open command flyout", root)
    flyout = fluentqt.CommandBarFlyout(root)
    flyout.setAnimationEnabled(False)
    flyout.setAlwaysExpanded(True)
    flyout.addPrimaryAction(save)
    flyout.addPrimaryAction(share)
    flyout.addSecondaryAction(delete)
    anchor.clicked.connect(lambda: flyout.showAt(anchor))
    layout.addWidget(anchor, 0, Qt.AlignLeft)
    layout.addStretch()
    _hold(root, anchor, flyout, save, share, delete)
    return PreviewResult(
        root,
        "from PySide6.QtGui import QAction\n"
        "from PySide6.QtWidgets import QVBoxLayout, QWidget\n"
        + _source(
            "page = QWidget()",
            "layout = QVBoxLayout(page)",
            'open_button = fluentqt.Button("Open command flyout", page)',
            "layout.addWidget(open_button)",
            "layout.addStretch()",
            'save = QAction("Save", page)',
            'share = QAction("Share", page)',
            'delete = QAction("Delete", page)',
            "flyout = fluentqt.CommandBarFlyout(page)",
            "flyout.setAnimationEnabled(False)",
            "flyout.setAlwaysExpanded(True)",
            "flyout.addPrimaryAction(save)",
            "flyout.addPrimaryAction(share)",
            "flyout.addSecondaryAction(delete)",
            "open_button.clicked.connect(lambda: flyout.showAt(open_button))",
        ),
        (name,),
    )


def _navigation_preview(name: str, parent: QWidget | None) -> PreviewResult:
    if name in {"Breadcrumb", "BreadcrumbItem"}:
        control = fluentqt.Breadcrumb(parent)
        items = [
            fluentqt.BreadcrumbItem("Home", {"route": "home"}, True, "Home"),
            fluentqt.BreadcrumbItem("Python", {"route": "python"}, True, "Python"),
            fluentqt.BreadcrumbItem("Gallery", {"route": "gallery"}, True, "Gallery"),
        ]
        control.setItems(items)
        control.setMinimumWidth(460)
        _hold(control, items)
        return PreviewResult(
            control,
            _source(
                'home = fluentqt.BreadcrumbItem("Home", {"route": "home"}, True, "Home")',
                'python = fluentqt.BreadcrumbItem("Python", {"route": "python"}, True, "Python")',
                'gallery = fluentqt.BreadcrumbItem("Gallery", {"route": "gallery"}, True, "Gallery")',
                "control = fluentqt.Breadcrumb()",
                "control.setItems([home, python, gallery])",
            ),
            ("Breadcrumb", "BreadcrumbItem"),
        )

    if name in {"SelectorBar", "SelectorBarItem"}:
        control = fluentqt.SelectorBar(parent)
        items = [
            fluentqt.SelectorBarItem("Overview", "", True, True, {"route": "overview"}, "Overview"),
            fluentqt.SelectorBarItem("Examples", "", True, True, {"route": "examples"}, "Examples"),
            fluentqt.SelectorBarItem("API", "", True, True, {"route": "api"}, "API"),
        ]
        for item in items:
            control.addItem(item)
        control.setSelectedIndex(0)
        control.setMinimumWidth(460)
        _hold(control, items)
        return PreviewResult(
            control,
            _source(
                'overview = fluentqt.SelectorBarItem("Overview", "", True, True, {"route": "overview"}, "Overview")',
                'examples = fluentqt.SelectorBarItem("Examples", "", True, True, {"route": "examples"}, "Examples")',
                'api = fluentqt.SelectorBarItem("API", "", True, True, {"route": "api"}, "API")',
                "selector = fluentqt.SelectorBar()",
                "selector.addItem(overview)",
                "selector.addItem(examples)",
                "selector.addItem(api)",
                "selector.setSelectedIndex(0)",
                "selector.setMinimumWidth(460)",
            ),
            ("SelectorBar", "SelectorBarItem"),
        )

    if name in {"Pivot", "PivotItem"}:
        control = fluentqt.Pivot(parent)
        items = [
            fluentqt.PivotItem("All", "", True, {"filter": "all"}),
            fluentqt.PivotItem("Unread", "", True, {"filter": "unread"}),
            fluentqt.PivotItem("Flagged", "", True, {"filter": "flagged"}),
        ]
        for item in items:
            control.addItem(item)
        control.setSelectedIndex(1)
        control.setMinimumWidth(460)
        _hold(control, items)
        return PreviewResult(
            control,
            _source(
                'all_items = fluentqt.PivotItem("All", "", True, {"filter": "all"})',
                'unread = fluentqt.PivotItem("Unread", "", True, {"filter": "unread"})',
                'flagged = fluentqt.PivotItem("Flagged", "", True, {"filter": "flagged"})',
                "pivot = fluentqt.Pivot()",
                "pivot.addItem(all_items)",
                "pivot.addItem(unread)",
                "pivot.addItem(flagged)",
                "pivot.setSelectedIndex(1)",
                "pivot.setMinimumWidth(460)",
            ),
            ("Pivot", "PivotItem"),
        )

    if name in {"TabView", "TabViewItem"}:
        control = fluentqt.TabView(parent)
        items = [
            fluentqt.TabViewItem("README.md", "", True, True, {"file": "README.md"}, "README tab"),
            fluentqt.TabViewItem("gallery.py", "", True, True, {"file": "gallery.py"}, "Gallery tab"),
        ]
        for item in items:
            control.addTab(item)
        control.setSelectedIndex(1)
        control.setMinimumWidth(480)
        _hold(control, items)
        return PreviewResult(
            control,
            _source(
                'readme = fluentqt.TabViewItem("README.md", "", True, True, {"file": "README.md"}, "README tab")',
                'gallery_source = fluentqt.TabViewItem("gallery.py", "", True, True, {"file": "gallery.py"}, "Gallery tab")',
                "tabs = fluentqt.TabView()",
                "tabs.addTab(readme)",
                "tabs.addTab(gallery_source)",
                "tabs.setSelectedIndex(1)",
                "tabs.setMinimumWidth(480)",
            ),
            ("TabView", "TabViewItem"),
        )

    if name == "StackContentHost":
        host = fluentqt.StackContentHost(parent)
        host.setMinimumSize(480, 220)
        host.setTransitionAnimationEnabled(False)
        host.addOwnedPage(_card_page("Overview", "First Python-owned application page."))
        host.addOwnedPage(_card_page("Details", "Second Python-owned application page."))
        host.setCurrentIndex(1, 1, False)
        return PreviewResult(
            host,
            "from PySide6.QtWidgets import QVBoxLayout\n" + _source(
                "overview_page = fluentqt.Card()",
                "overview_layout = QVBoxLayout(overview_page)",
                'overview_layout.addWidget(fluentqt.Label("Overview", overview_page))',
                'overview_detail = fluentqt.Label("First Python-owned application page.", overview_page)',
                "overview_detail.setWordWrap(True)",
                "overview_layout.addWidget(overview_detail)",
                "details_page = fluentqt.Card()",
                "details_layout = QVBoxLayout(details_page)",
                'details_layout.addWidget(fluentqt.Label("Details", details_page))',
                'details_detail = fluentqt.Label("Second Python-owned application page.", details_page)',
                "details_detail.setWordWrap(True)",
                "details_layout.addWidget(details_detail)",
                "host = fluentqt.StackContentHost()",
                "host.setMinimumSize(480, 220)",
                "host.setTransitionAnimationEnabled(False)",
                "host.addOwnedPage(overview_page)",
                "host.addOwnedPage(details_page)",
                "host.setCurrentIndex(1, 1, False)",
            ),
            (name,),
        )

    nav = fluentqt.NavigationView(parent)
    nav.setMinimumSize(540, 300)
    nav.setAnimationEnabled(False)
    nav.setDisplayMode(fluentqt.NavigationView.DisplayMode.Left)
    nav.setExpandedPaneWidth(180)
    header = fluentqt.Label("Python Gallery", nav)
    header.setMinimumHeight(48)
    main = fluentqt.ListView(nav)
    model = QStandardItemModel(main)
    for text in ("Home", "Controls", "Settings"):
        model.appendRow(QStandardItem(text))
    main.setModel(model)
    page = _card_page("Navigation content", "NavigationView hosts caller-provided chrome and pages.")
    nav.setOwnedHeaderChromeWidget(header)
    nav.setOwnedMainChromeWidget(main)
    nav.contentHost().addOwnedPage(page)
    _hold(nav, header, main, model, page)
    return PreviewResult(
        nav,
        "from PySide6.QtGui import QStandardItem, QStandardItemModel\n"
        "from PySide6.QtWidgets import QVBoxLayout\n"
        + _source(
            "nav = fluentqt.NavigationView()",
            "nav.setMinimumSize(540, 300)",
            "nav.setAnimationEnabled(False)",
            "nav.setDisplayMode(fluentqt.NavigationView.DisplayMode.Left)",
            "nav.setExpandedPaneWidth(180)",
            'header = fluentqt.Label("Python Gallery", nav)',
            "header.setMinimumHeight(48)",
            "navigation_list = fluentqt.ListView(nav)",
            "navigation_model = QStandardItemModel(navigation_list)",
            'for text in ("Home", "Controls", "Settings"):',
            "    navigation_model.appendRow(QStandardItem(text))",
            "navigation_list.setModel(navigation_model)",
            "content_page = fluentqt.Card()",
            "content_layout = QVBoxLayout(content_page)",
            'content_layout.addWidget(fluentqt.Label("Navigation content", content_page))',
            'content_detail = fluentqt.Label("NavigationView hosts caller-provided chrome and pages.", content_page)',
            "content_detail.setWordWrap(True)",
            "content_layout.addWidget(content_detail)",
            "nav.setOwnedHeaderChromeWidget(header)",
            "nav.setOwnedMainChromeWidget(navigation_list)",
            "nav.contentHost().addOwnedPage(content_page)",
        ),
        ("NavigationView", "StackContentHost"),
    )


def _scrolling_preview(name: str, parent: QWidget | None) -> PreviewResult:
    if name == "PipsPager":
        control = fluentqt.PipsPager(parent)
        control.setNumberOfPages(8)
        control.setSelectedPageIndex(2)
        control.setMaxVisiblePips(6)
        return PreviewResult(
            control,
            _source(
                "pager = fluentqt.PipsPager()",
                "pager.setNumberOfPages(8)",
                "pager.setSelectedPageIndex(2)",
                "pager.setMaxVisiblePips(6)",
            ),
            (name,),
        )

    if name == "ScrollBar":
        control = fluentqt.ScrollBar(Qt.Horizontal, parent)
        control.setRange(0, 100)
        control.setPageStep(20)
        control.setValue(42)
        control.setMinimumWidth(360)
        return PreviewResult(
            control,
            "from PySide6.QtCore import Qt\n" + _source(
                "bar = fluentqt.ScrollBar(Qt.Horizontal)",
                "bar.setRange(0, 100)",
                "bar.setPageStep(20)",
                "bar.setValue(42)",
                "bar.setMinimumWidth(360)",
            ),
            (name,),
        )

    if name == "ScrollView":
        view = fluentqt.ScrollView(parent)
        view.setMinimumSize(440, 240)
        content, content_layout = _column(None, 520)
        for index in range(1, 6):
            content_layout.addWidget(
                _card_page("Section {0}".format(index), "Scrollable Python-owned content.")
            )
        view.setOwnedContentWidget(content)
        return PreviewResult(
            view,
            "from PySide6.QtWidgets import QVBoxLayout, QWidget\n" + _source(
                "content = QWidget()",
                "content.setMinimumHeight(520)",
                "content_layout = QVBoxLayout(content)",
                'for index in range(1, 6):',
                "    card = fluentqt.Card()",
                "    card_layout = QVBoxLayout(card)",
                '    card_layout.addWidget(fluentqt.Label(f"Section {index}", card))',
                '    detail = fluentqt.Label("Scrollable Python-owned content.", card)',
                "    detail.setWordWrap(True)",
                "    card_layout.addWidget(detail)",
                "    content_layout.addWidget(card)",
                "view = fluentqt.ScrollView()",
                "view.setMinimumSize(440, 240)",
                "view.setOwnedContentWidget(content)",
            ),
            (name,),
        )

    root, layout = _column(parent, 280)
    bar = fluentqt.AnnotatedScrollBar(root)
    bar.setFixedSize(180, 250)
    labels = [
        fluentqt.AnnotatedScrollBarLabel("Overview", 0, "Start"),
        fluentqt.AnnotatedScrollBarLabel("Details", 120, "Middle"),
        fluentqt.AnnotatedScrollBarLabel("Summary", 240, "End"),
    ]
    bar.setRange(0, 300)
    bar.setPageStep(60)
    bar.setLabels(labels)
    bar.setValue(120)
    layout.addWidget(bar, 0, Qt.AlignLeft)
    _hold(root, bar, labels)
    return PreviewResult(
        root,
        _source(
            'overview = fluentqt.AnnotatedScrollBarLabel("Overview", 0, "Start")',
            'details = fluentqt.AnnotatedScrollBarLabel("Details", 120, "Middle")',
            'summary = fluentqt.AnnotatedScrollBarLabel("Summary", 240, "End")',
            "bar = fluentqt.AnnotatedScrollBar()",
            "bar.setFixedSize(180, 250)",
            "bar.setRange(0, 300)",
            "bar.setPageStep(60)",
            "bar.setLabels([overview, details, summary])",
            "bar.setValue(120)",
        ),
        ("AnnotatedScrollBar", "AnnotatedScrollBarLabel"),
    )


def _status_preview(name: str, parent: QWidget | None) -> PreviewResult:
    if name == "Avatar":
        control = fluentqt.Avatar("Ada Lovelace", parent)
        control.setPresence(fluentqt.Avatar.PresenceStatus.Available)
        return PreviewResult(
            control,
            _source(
                'avatar = fluentqt.Avatar("Ada Lovelace")',
                "avatar.setPresence(fluentqt.Avatar.PresenceStatus.Available)",
            ),
            (name,),
        )
    if name == "InfoBadge":
        control = fluentqt.InfoBadge(parent)
        control.setValue(7)
        control.setDisplayMode(fluentqt.InfoBadge.InfoBadgeDisplayMode.Value)
        control.setStatus(fluentqt.InfoBadge.InfoBadgeStatus.Attention)
        return PreviewResult(
            control,
            _source(
                "badge = fluentqt.InfoBadge()",
                "badge.setValue(7)",
                "badge.setDisplayMode(fluentqt.InfoBadge.InfoBadgeDisplayMode.Value)",
                "badge.setStatus(fluentqt.InfoBadge.InfoBadgeStatus.Attention)",
            ),
            (name,),
        )
    if name == "InfoBar":
        control = fluentqt.InfoBar(parent)
        control.setTitle("Python Gallery")
        control.setMessage("The installed binding is ready for interactive review.")
        control.setSeverity(fluentqt.InfoBar.InfoBarSeverity.Success)
        control.setIsOpen(True)
        control.setPreferredWidth(560)
        return PreviewResult(
            control,
            _source(
                "bar = fluentqt.InfoBar()",
                'bar.setTitle("Python Gallery")',
                'bar.setMessage("The installed binding is ready for interactive review.")',
                "bar.setSeverity(fluentqt.InfoBar.InfoBarSeverity.Success)",
                "bar.setIsOpen(True)",
                "bar.setPreferredWidth(560)",
            ),
            (name,),
        )
    if name == "ProgressBar":
        control = fluentqt.ProgressBar(parent)
        control.setRange(0.0, 100.0)
        control.setValue(68.0)
        control.setMinimumWidth(360)
        return PreviewResult(
            control,
            _source(
                "progress = fluentqt.ProgressBar()",
                "progress.setRange(0.0, 100.0)",
                "progress.setValue(68.0)",
                "progress.setMinimumWidth(360)",
            ),
            (name,),
        )
    if name == "ProgressRing":
        control = fluentqt.ProgressRing(parent)
        control.setIsIndeterminate(False)
        control.setIsActive(True)
        control.setBackgroundVisible(True)
        control.setValue(68)
        return PreviewResult(
            control,
            _source(
                "ring = fluentqt.ProgressRing()",
                "ring.setIsIndeterminate(False)",
                "ring.setIsActive(True)",
                "ring.setBackgroundVisible(True)",
                "ring.setValue(68)",
            ),
            (name,),
        )
    if name == "Shimmer":
        control = fluentqt.Shimmer(parent)
        control.setAnimationEnabled(False)
        control.setShimmerProgress(0.35)
        control.setShimmerTemplate(fluentqt.Shimmer.ShimmerTemplate.AvatarTextRow)
        control.setMinimumSize(420, 72)
        return PreviewResult(
            control,
            _source(
                "shimmer = fluentqt.Shimmer()",
                "shimmer.setAnimationEnabled(False)",
                "shimmer.setShimmerProgress(0.35)",
                "shimmer.setShimmerTemplate(fluentqt.Shimmer.ShimmerTemplate.AvatarTextRow)",
                "shimmer.setMinimumSize(420, 72)",
            ),
            (name,),
        )

    root, layout = _column(parent, 170)
    trigger = fluentqt.Button(
        "Show Toast" if name == "Toast" else "Hover for ToolTip", root
    )
    status = fluentqt.Label("Ready", root)
    layout.addWidget(trigger, 0, Qt.AlignLeft)
    layout.addWidget(status)
    layout.addStretch()
    if name == "Toast":
        toasts = []

        def show_toast() -> None:
            toast = fluentqt.Toast.showToast(
                trigger,
                "The Python binding is ready.",
                severity=fluentqt.Toast.Severity.Success,
                durationMs=2500,
            )
            toast.setTitle("Saved")
            toasts.append(toast)
            status.setText("Toast shown")

        trigger.clicked.connect(show_toast)
        _hold(root, trigger, status, toasts, show_toast)
        source = "from PySide6.QtWidgets import QVBoxLayout, QWidget\n" + _source(
            "page = QWidget()",
            "layout = QVBoxLayout(page)",
            'button = fluentqt.Button("Show Toast", page)',
            'status = fluentqt.Label("Ready", page)',
            "layout.addWidget(button)",
            "layout.addWidget(status)",
            "layout.addStretch()",
            "toasts = []",
            "",
            "def show_toast():",
            "    toast = fluentqt.Toast.showToast(",
            "        button,",
            '        "The Python binding is ready.",',
            "        severity=fluentqt.Toast.Severity.Success,",
            "        durationMs=2500,",
            "    )",
            '    toast.setTitle("Saved")',
            "    toasts.append(toast)",
            '    status.setText("Toast shown")',
            "",
            "button.clicked.connect(show_toast)",
        )
    else:
        tooltip = fluentqt.ToolTip.attach(
            trigger,
            "Native tooltip attached from Python.",
            fluentqt.ToolTip.Placement.Above,
        )
        tooltip.setAnimationEnabled(False)
        _hold(root, trigger, status, tooltip)
        source = "from PySide6.QtWidgets import QVBoxLayout, QWidget\n" + _source(
            "page = QWidget()",
            "layout = QVBoxLayout(page)",
            'button = fluentqt.Button("Hover for ToolTip", page)',
            'status = fluentqt.Label("Ready", page)',
            "layout.addWidget(button)",
            "layout.addWidget(status)",
            "layout.addStretch()",
            "tooltip = fluentqt.ToolTip.attach(",
            "    button,",
            '    "Native tooltip attached from Python.",',
            "    fluentqt.ToolTip.Placement.Above,",
            ")",
            "tooltip.setAnimationEnabled(False)",
        )
    return PreviewResult(root, source, (name,))


def _window_preview(name: str, parent: QWidget | None) -> PreviewResult:
    root, layout = _column(parent, 180)
    state = fluentqt.Label(
        "Window and TitleBar are reviewed in a separate native top-level surface.",
        root,
    )
    state.setWordWrap(True)
    launch = fluentqt.Button("Open native window", root)
    launch.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    layout.addWidget(state)
    layout.addWidget(launch, 0, Qt.AlignLeft)
    layout.addStretch()
    windows = []

    def open_window() -> None:
        window = fluentqt.Window()
        window.setAttribute(Qt.WA_DeleteOnClose, True)
        window.setWindowTitle("FluentQt Python Gallery window")
        window.resize(620, 380)
        window.setCustomWindowChromeEnabled(True)
        window.setChromeInteractive(True)
        title_content = fluentqt.Label("Native Window + TitleBar")
        window.titleBar().setContentWidget(title_content)
        window.setContentWidget(
            _card_page("Backdrop state", "Mica uses the platform backend or a painted fallback.")
        )
        window.setBackdropEffect(fluentqt.BackdropEffect.Mica)
        window.destroyed.connect(lambda: windows.remove(window) if window in windows else None)
        windows.append(window)
        window.show()

    launch.clicked.connect(open_window)
    _hold(root, state, launch, windows, open_window)
    return PreviewResult(
        root,
        "from PySide6.QtCore import Qt\n"
        "from PySide6.QtWidgets import QVBoxLayout\n"
        + _source(
            "window = fluentqt.Window()",
            "window.setAttribute(Qt.WA_DeleteOnClose, True)",
            'window.setWindowTitle("FluentQt Python Gallery window")',
            "window.resize(620, 380)",
            "window.setCustomWindowChromeEnabled(True)",
            "window.setChromeInteractive(True)",
            'title_content = fluentqt.Label("Native Window + TitleBar")',
            "title_bar: fluentqt.TitleBar = window.titleBar()",
            "title_bar.setContentWidget(title_content)",
            "page = fluentqt.Card()",
            "page_layout = QVBoxLayout(page)",
            'heading = fluentqt.Label("Backdrop state", page)',
            "heading.setFluentTypography(fluentqt.FontRole.BodyStrong)",
            'detail = fluentqt.Label("Mica uses the platform backend or a painted fallback.", page)',
            "detail.setWordWrap(True)",
            "page_layout.addWidget(heading)",
            "page_layout.addWidget(detail)",
            "page_layout.addStretch()",
            "window.setContentWidget(page)",
            "window.setBackdropEffect(fluentqt.BackdropEffect.Mica)",
            "window.show()",
        ),
        ("Window", "TitleBar"),
    )


def build_preview(name: str, parent: QWidget | None = None) -> PreviewResult:
    """Build a live public-API preview for a catalog component."""

    if name not in ENTRY_BY_NAME:
        raise KeyError("Unknown Gallery component: {0}".format(name))

    simple = _simple_preview(name, parent)
    if simple is not None:
        return simple
    if name in {"DropDownButton", "SplitButton", "ToggleSplitButton"}:
        return _menu_preview(name, parent)
    if name in {"Accordion", "Card", "Divider", "Expander"}:
        return _layout_preview(name, parent)
    if name in {"ListView", "GridView", "FlowView", "TreeView"}:
        return _model_view_preview(name, parent)
    if name in {"DrawerView", "FlipView", "SplitView", "SplitViewPaneOptions", "StackView"}:
        return _hosted_collection_preview(name, parent)
    if name in {"CalendarDatePicker", "CalendarView", "DatePicker", "TimePicker"}:
        return _date_time_preview(name, parent)
    if name in {"CoachMark", "ContentDialog", "Dialog", "Flyout", "Popup", "TeachingTip"}:
        return _overlay_preview(name, parent)
    if name in {"CommandBar", "CommandBarFlyout", "FluentMenu", "FluentMenuBar", "FluentMenuItem"}:
        return _command_preview(name, parent)
    if name in {
        "Breadcrumb",
        "BreadcrumbItem",
        "NavigationView",
        "Pivot",
        "PivotItem",
        "SelectorBar",
        "SelectorBarItem",
        "StackContentHost",
        "TabView",
        "TabViewItem",
    }:
        return _navigation_preview(name, parent)
    if name in {"AnnotatedScrollBar", "AnnotatedScrollBarLabel", "PipsPager", "ScrollBar", "ScrollView"}:
        return _scrolling_preview(name, parent)
    if name in {"Avatar", "InfoBadge", "InfoBar", "ProgressBar", "ProgressRing", "Shimmer", "Toast", "ToolTip"}:
        return _status_preview(name, parent)
    if name in {"TitleBar", "Window"}:
        return _window_preview(name, parent)

    control_type = getattr(fluentqt, name)
    control = control_type()
    control.setParent(parent)
    control.setMinimumSize(360, 120)
    return PreviewResult(
        control,
        _source("control = fluentqt.{0}()".format(name)),
        (name,),
    )


def build_sample(
    route_id: str,
    sample_id: str,
    parent: QWidget | None = None,
) -> PreviewResult:
    """Build one SampleCard selected by the native Gallery contract.

    Route/card identity is exact while behavioral ports are implemented.  The
    acceptance suite deliberately rejects the default ``component-smoke``
    level, so a generic component preview can never be reported as parity.
    """

    entry = ENTRY_BY_ROUTE_ID.get(route_id)
    if entry is None:
        raise KeyError("Unknown native Gallery component route: {0}".format(route_id))
    if sample_id not in {sample.id for sample in entry.samples}:
        raise KeyError(
            "Unknown native Gallery sample: {0}/{1}".format(route_id, sample_id)
        )
    # Keep the registry in a separate module so the legacy component-level
    # builders remain available as a diagnostic fallback while parity work is
    # in progress.  Acceptance explicitly rejects that fallback.
    from .native_samples import build_native_sample

    native_result = build_native_sample(route_id, sample_id, parent)
    if native_result is not None:
        return native_result
    result = build_preview(entry.name, parent)
    result.route_id = route_id
    result.sample_id = sample_id
    return result


__all__ = ["PreviewResult", "build_preview", "build_sample"]
