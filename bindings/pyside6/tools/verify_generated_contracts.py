"""Verify safety-sensitive contracts in generated FluentQt wrappers."""

import argparse
from pathlib import Path
import re
import sys


WINDOW_WRAPPER = "fluent_windowing_window_wrapper.cpp"
WINDOWING_NAMESPACE_WRAPPER = "fluent_windowing_wrapper.cpp"
MODULE_WRAPPER = "_fluentqt_module_wrapper.cpp"
FLUENT_NAMESPACE_WRAPPER = "fluent_wrapper.cpp"
ACCORDION_WRAPPER = "fluent_layout_accordion_wrapper.cpp"
EXPANDER_WRAPPER = "fluent_layout_expander_wrapper.cpp"
INFO_BAR_WRAPPER = "fluent_status_info_infobar_wrapper.cpp"
ANNOTATED_SCROLL_BAR_WRAPPER = (
    "fluent_scrolling_annotatedscrollbar_wrapper.cpp"
)
PIPS_PAGER_WRAPPER = "fluent_scrolling_pipspager_wrapper.cpp"
SCROLL_VIEW_WRAPPER = "fluent_scrolling_scrollview_wrapper.cpp"
FLIP_VIEW_WRAPPER = "fluent_collections_flipview_wrapper.cpp"
SPLIT_VIEW_WRAPPER = "fluent_collections_splitview_wrapper.cpp"
STACK_CONTENT_HOST_WRAPPER = (
    "fluent_navigation_stackcontenthost_wrapper.cpp"
)
NAVIGATION_VIEW_WRAPPER = "fluent_navigation_navigationview_wrapper.cpp"
FLOW_VIEW_WRAPPER = "fluent_collections_flowview_wrapper.cpp"
GRID_VIEW_WRAPPER = "fluent_collections_gridview_wrapper.cpp"
LIST_VIEW_WRAPPER = "fluent_collections_listview_wrapper.cpp"
STACK_VIEW_WRAPPER = "fluent_collections_stackview_wrapper.cpp"
TREE_VIEW_WRAPPER = "fluent_collections_treeview_wrapper.cpp"
BREADCRUMB_WRAPPER = "fluent_navigation_breadcrumb_wrapper.cpp"
BREADCRUMB_ITEM_WRAPPER = "fluent_navigation_breadcrumbitem_wrapper.cpp"
PIVOT_WRAPPER = "fluent_navigation_pivot_wrapper.cpp"
PIVOT_ITEM_WRAPPER = "fluent_navigation_pivotitem_wrapper.cpp"
SELECTOR_BAR_WRAPPER = "fluent_navigation_selectorbar_wrapper.cpp"
SELECTOR_BAR_ITEM_WRAPPER = (
    "fluent_navigation_selectorbaritem_wrapper.cpp"
)
TAB_VIEW_WRAPPER = "fluent_navigation_tabview_wrapper.cpp"
TAB_VIEW_ITEM_WRAPPER = "fluent_navigation_tabviewitem_wrapper.cpp"
TAB_STRIP_WRAPPER = "fluent_navigation_tabstrip_wrapper.cpp"
PROTECTED_HACK = re.compile(
    r"^\s*#\s*define\s+protected\s+public\b",
    re.MULTILINE,
)


def extract_function(source, signature):
    start = source.find(signature)
    if start < 0:
        raise RuntimeError("Generated function was not found: {0}".format(signature))

    opening_brace = source.find("{", start)
    if opening_brace < 0:
        raise RuntimeError("Generated function has no body: {0}".format(signature))

    depth = 0
    for index in range(opening_brace, len(source)):
        character = source[index]
        if character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]

    raise RuntimeError("Generated function body is incomplete: {0}".format(signature))


def require_text(source, expected, context):
    if expected not in source:
        raise RuntimeError(
            "{0} is missing generated contract: {1}".format(context, expected)
        )


def verify_navigation_metadata_widget(
    generated_dir,
    widget_name,
    item_name,
    widget_wrapper,
    item_wrapper,
    item_fields,
):
    widget_path = generated_dir / widget_wrapper
    item_path = generated_dir / item_wrapper
    if not widget_path.is_file():
        raise RuntimeError(
            "Generated {0} wrapper was not found: {1}".format(
                widget_name,
                widget_path,
            )
        )
    if not item_path.is_file():
        raise RuntimeError(
            "Generated {0} wrapper was not found: {1}".format(
                item_name,
                item_path,
            )
        )

    widget_source = widget_path.read_text(encoding="utf-8")
    function_prefix = "Sbk_fluent_navigation_{0}Func_".format(widget_name)

    adder = extract_function(
        widget_source,
        "static PyObject *{0}addItem(".format(function_prefix),
    )
    require_text(
        adder,
        "cppSelf->addItem(cppArg0)",
        "{0}::addItem QString overload".format(widget_name),
    )
    require_text(
        adder,
        "cppSelf->addItem(*cppArg0)",
        "{0}::addItem {1} overload".format(widget_name, item_name),
    )

    inserter = extract_function(
        widget_source,
        "static PyObject *{0}insertItem(".format(function_prefix),
    )
    require_text(
        inserter,
        "cppSelf->insertItem(cppArg0, cppArg1)",
        "{0}::insertItem QString overload".format(widget_name),
    )
    require_text(
        inserter,
        "cppSelf->insertItem(cppArg0, *cppArg1)",
        "{0}::insertItem {1} overload".format(widget_name, item_name),
    )

    item_getter = extract_function(
        widget_source,
        "static PyObject *{0}itemAt(".format(function_prefix),
    )
    for expected, description in (
        ("itemAt(cppArg0)", "value call"),
        (item_name, "value conversion"),
        ("copyToPython", "Python conversion"),
    ):
        require_text(
            item_getter,
            expected,
            "{0}::itemAt {1}".format(widget_name, description),
        )

    items_getter = extract_function(
        widget_source,
        "static PyObject *{0}items(".format(function_prefix),
    )
    for expected, description in (
        ("items()", "value call"),
        (item_name, "list value conversion"),
        ("copyToPython", "list Python conversion"),
    ):
        require_text(
            items_getter,
            expected,
            "{0}::items {1}".format(widget_name, description),
        )

    item_source = item_path.read_text(encoding="utf-8")
    for field_name in item_fields:
        require_text(
            item_source,
            "{0}_get_{1}(".format(item_name, field_name),
            "{0} field {1} getter".format(item_name, field_name),
        )
        require_text(
            item_source,
            "{0}_set_{1}(".format(item_name, field_name),
            "{0} field {1} setter".format(item_name, field_name),
        )

    data_getter = extract_function(
        item_source,
        "Sbk_fluent_navigation_{0}_get_data(".format(item_name),
    )
    require_text(
        data_getter,
        "copyToPython",
        "{0} QVariant data getter conversion".format(item_name),
    )
    data_setter = extract_function(
        item_source,
        "Sbk_fluent_navigation_{0}_set_data(".format(item_name),
    )
    require_text(
        data_setter,
        "pythonToCpp",
        "{0} QVariant data setter conversion".format(item_name),
    )


def verify_no_protected_hack(generated_dir):
    for generated_path in sorted(generated_dir.iterdir()):
        if generated_path.suffix not in {".cpp", ".h"}:
            continue
        source = generated_path.read_text(encoding="utf-8")
        if PROTECTED_HACK.search(source):
            raise RuntimeError(
                "Generated wrapper uses the protected-access macro hack: "
                "{0}".format(generated_path)
            )


def reject_wrapper_bookkeeping(source, context, allow_python_ownership=False):
    forbidden_operations = [
        ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::keepReference", "uses keep-reference bookkeeping"),
        ("Shiboken::Object::setParent", "uses parent bookkeeping"),
    ]
    if not allow_python_ownership:
        forbidden_operations.append(
            ("Shiboken::Object::getOwnership", "changes wrapper ownership")
        )
    for forbidden, description in forbidden_operations:
        if forbidden in source:
            raise RuntimeError("{0} {1}".format(context, description))


def verify_stack_content_host(generated_dir):
    wrapper_path = generated_dir / STACK_CONTENT_HOST_WRAPPER
    if not wrapper_path.is_file():
        raise RuntimeError(
            "Generated StackContentHost wrapper was not found: {0}".format(
                wrapper_path
            )
        )
    source = wrapper_path.read_text(encoding="utf-8")
    for public_bypass in (
        "Sbk_fluent_navigation_StackContentHostFunc_insertPage(",
        "Sbk_fluent_navigation_StackContentHostFunc_replacePage(",
        "Sbk_fluent_navigation_StackContentHostFunc_releasePage(",
        "Sbk_fluent_navigation_StackContentHostFunc_releaseAllPages(",
        "Sbk_fluent_navigation_StackContentHostFunc_clearPages(",
    ):
        if public_bypass in source:
            raise RuntimeError(
                "StackContentHost exposes a page ownership bypass: {0}".format(
                    public_bypass
                )
            )

    for adapter_name, native_call in (
        (
            "_insertPageWithOwnership",
            "cppSelf->insertPage(cppArg0, cppArg1, cppArg2)",
        ),
        (
            "_replacePageWithOwnership",
            "cppSelf->replacePage(cppArg0, cppArg1, cppArg2)",
        ),
        (
            "_releasePageWithOwnership",
            "cppSelf->releasePage(cppArg0)",
        ),
        (
            "_releaseAllPagesWithOwnership",
            "cppSelf->releaseAllPages()",
        ),
    ):
        adapter = extract_function(
            source,
            (
                "static PyObject *"
                "Sbk_fluent_navigation_StackContentHostFunc_{0}(".format(
                    adapter_name
                )
            ),
        )
        require_text(
            adapter,
            native_call,
            "StackContentHost private ownership adapter {0}".format(
                adapter_name
            ),
        )
        reject_wrapper_bookkeeping(
            adapter,
            "StackContentHost adapter {0}".format(adapter_name),
        )

    page_getter = extract_function(
        source,
        "static PyObject *Sbk_fluent_navigation_StackContentHostFunc_pageWidget(",
    )
    reject_wrapper_bookkeeping(
        page_getter,
        "StackContentHost::pageWidget",
    )

    page_taker = extract_function(
        source,
        "static PyObject *Sbk_fluent_navigation_StackContentHostFunc_takePage(",
    )
    require_text(
        page_taker,
        "cppSelf->takePage(cppArg0)",
        "StackContentHost::takePage call",
    )
    require_text(
        page_taker,
        "Shiboken::Object::getOwnership(pyResult)",
        "StackContentHost::takePage Python ownership transfer",
    )
    reject_wrapper_bookkeeping(
        page_taker,
        "StackContentHost::takePage",
        allow_python_ownership=True,
    )


def verify_navigation_view(generated_dir):
    wrapper_path = generated_dir / NAVIGATION_VIEW_WRAPPER
    if not wrapper_path.is_file():
        raise RuntimeError(
            "Generated NavigationView wrapper was not found: {0}".format(
                wrapper_path
            )
        )
    source = wrapper_path.read_text(encoding="utf-8")
    for public_bypass in (
        "Sbk_fluent_navigation_NavigationViewFunc_setHeaderChromeWidget(",
        "Sbk_fluent_navigation_NavigationViewFunc_setMainChromeWidget(",
        "Sbk_fluent_navigation_NavigationViewFunc_setFooterChromeWidget(",
        "Sbk_fluent_navigation_NavigationViewFunc_releaseHeaderChromeWidget(",
        "Sbk_fluent_navigation_NavigationViewFunc_releaseMainChromeWidget(",
        "Sbk_fluent_navigation_NavigationViewFunc_releaseFooterChromeWidget(",
    ):
        if public_bypass in source:
            raise RuntimeError(
                "NavigationView exposes a chrome ownership bypass: {0}".format(
                    public_bypass
                )
            )

    for adapter_name, native_call in (
        (
            "_setHeaderChromeWidgetWithOwnership",
            "cppSelf->setHeaderChromeWidget(cppArg0, cppArg1)",
        ),
        (
            "_setMainChromeWidgetWithOwnership",
            "cppSelf->setMainChromeWidget(cppArg0, cppArg1)",
        ),
        (
            "_setFooterChromeWidgetWithOwnership",
            "cppSelf->setFooterChromeWidget(cppArg0, cppArg1)",
        ),
        (
            "_releaseHeaderChromeWidgetWithOwnership",
            "cppSelf->releaseHeaderChromeWidget()",
        ),
        (
            "_releaseMainChromeWidgetWithOwnership",
            "cppSelf->releaseMainChromeWidget()",
        ),
        (
            "_releaseFooterChromeWidgetWithOwnership",
            "cppSelf->releaseFooterChromeWidget()",
        ),
    ):
        adapter = extract_function(
            source,
            (
                "static PyObject *"
                "Sbk_fluent_navigation_NavigationViewFunc_{0}(".format(
                    adapter_name
                )
            ),
        )
        require_text(
            adapter,
            native_call,
            "NavigationView private ownership adapter {0}".format(
                adapter_name
            ),
        )
        reject_wrapper_bookkeeping(
            adapter,
            "NavigationView adapter {0}".format(adapter_name),
        )

    for getter_name in (
        "headerChromeWidget",
        "mainChromeWidget",
        "footerChromeWidget",
        "contentHost",
    ):
        getter = extract_function(
            source,
            (
                "static PyObject *"
                "Sbk_fluent_navigation_NavigationViewFunc_{0}(".format(
                    getter_name
                )
            ),
        )
        reject_wrapper_bookkeeping(
            getter,
            "NavigationView::{0}".format(getter_name),
        )

    for taker_name in (
        "takeHeaderChromeWidget",
        "takeMainChromeWidget",
        "takeFooterChromeWidget",
    ):
        taker = extract_function(
            source,
            (
                "static PyObject *"
                "Sbk_fluent_navigation_NavigationViewFunc_{0}(".format(
                    taker_name
                )
            ),
        )
        require_text(
            taker,
            "cppSelf->{0}()".format(taker_name),
            "NavigationView::{0} call".format(taker_name),
        )
        require_text(
            taker,
            "Shiboken::Object::getOwnership(pyResult)",
            "NavigationView::{0} Python ownership transfer".format(
                taker_name
            ),
        )
        reject_wrapper_bookkeeping(
            taker,
            "NavigationView::{0}".format(taker_name),
            allow_python_ownership=True,
        )


def verify_contracts(generated_dir, check_backdrop_converter):
    verify_no_protected_hack(generated_dir)

    window_path = generated_dir / WINDOW_WRAPPER
    if not window_path.is_file():
        raise RuntimeError(
            "Generated Window wrapper was not found: {0}".format(window_path)
        )

    window_source = window_path.read_text(encoding="utf-8")
    native_event = extract_function(
        window_source,
        "bool WindowWrapper::nativeEvent(",
    )
    native_event_override_signature = "bool WindowWrapper::sbk_o_nativeEvent("
    if native_event_override_signature in window_source:
        native_event_override = extract_function(
            window_source,
            native_event_override_signature,
        )
        require_text(
            native_event_override,
            "PyObject *pyArgArray[2]",
            "Window::nativeEvent override",
        )
    else:
        # Shiboken 6.2 emits the Python override inline in nativeEvent().
        # Newer generators move it into sbk_o_nativeEvent().
        native_event_override = native_event

    require_text(
        native_event,
        "return this->::fluent::windowing::Window::nativeEvent",
        "Window::nativeEvent fallback",
    )
    require_text(
        native_event_override,
        'Py_BuildValue("(NN)"',
        "Window::nativeEvent limited-API override",
    )

    pointer_exposure = re.search(
        r"copyToPython\([^\n;]*\bresult\b",
        native_event_override,
    )
    if pointer_exposure:
        raise RuntimeError(
            "Window::nativeEvent exposed its result pointer to Python: {0}".format(
                pointer_exposure.group(0)
            )
        )

    scroll_view_path = generated_dir / SCROLL_VIEW_WRAPPER
    if not scroll_view_path.is_file():
        raise RuntimeError(
            "Generated ScrollView wrapper was not found: {0}".format(
                scroll_view_path
            )
        )
    scroll_view_source = scroll_view_path.read_text(encoding="utf-8")
    content_setter = extract_function(
        scroll_view_source,
        (
            "static PyObject *"
            "Sbk_fluent_scrolling_ScrollViewFunc_setContentWidget("
        ),
    )
    require_text(
        content_setter,
        "cppSelf->setContentWidget(cppArg0)",
        "ScrollView::setContentWidget call",
    )
    if (
        "releaseOwnership(oldChild)" in content_setter
        or "oldChild" in content_setter
    ):
        raise RuntimeError(
            "ScrollView::setContentWidget releases the old owned child before "
            "C++ deletes it"
        )
    if "releaseOwnership" in content_setter:
        raise RuntimeError(
            "ScrollView::setContentWidget transfers wrapper ownership to C++"
        )
    if "keepReference" in content_setter:
        raise RuntimeError(
            "ScrollView::setContentWidget uses Shiboken keep-reference "
            "bookkeeping"
        )
    if "setParent" in content_setter:
        raise RuntimeError(
            "ScrollView::setContentWidget uses Shiboken parent bookkeeping"
        )
    if "WidgetOwnership" in content_setter:
        raise RuntimeError(
            "ScrollView::setContentWidget exposes the runtime ownership "
            "overload"
        )

    ownership_setter = extract_function(
        scroll_view_source,
        (
            "static PyObject *"
            "Sbk_fluent_scrolling_ScrollViewFunc_"
            "_setContentWidgetWithOwnership("
        ),
    )
    require_text(
        ownership_setter,
        "cppSelf->setContentWidget(cppArg0, cppArg1)",
        "ScrollView private ownership adapter call",
    )
    for forbidden, description in (
        (
            "Shiboken::Object::releaseOwnership",
            "changes Python wrapper ownership",
        ),
        (
            "Shiboken::Object::getOwnership",
            "changes Python wrapper ownership",
        ),
        (
            "Shiboken::Object::keepReference",
            "uses Shiboken keep-reference bookkeeping",
        ),
        (
            "Shiboken::Object::setParent",
            "uses Shiboken parent bookkeeping",
        ),
    ):
        if forbidden in ownership_setter:
            raise RuntimeError(
                "ScrollView private ownership adapter {0}".format(description)
            )

    content_getter = extract_function(
        scroll_view_source,
        (
            "static PyObject *"
            "Sbk_fluent_scrolling_ScrollViewFunc_contentWidget("
        ),
    )
    if "setParent" in content_getter:
        raise RuntimeError(
            "ScrollView::contentWidget uses the return-value parent heuristic"
        )
    if "releaseOwnership" in content_getter or "getOwnership" in content_getter:
        raise RuntimeError(
            "ScrollView::contentWidget changes Python wrapper ownership"
        )

    content_taker = extract_function(
        scroll_view_source,
        (
            "static PyObject *"
            "Sbk_fluent_scrolling_ScrollViewFunc_takeContentWidget("
        ),
    )
    require_text(
        content_taker,
        "cppSelf->takeContentWidget()",
        "ScrollView::takeContentWidget call",
    )
    require_text(
        content_taker,
        "Shiboken::Object::getOwnership(pyResult)",
        "ScrollView::takeContentWidget Python ownership transfer",
    )
    if "keepReference" in content_taker:
        raise RuntimeError(
            "ScrollView::takeContentWidget uses Shiboken keep-reference "
            "bookkeeping"
        )
    if "setParent" in content_taker:
        raise RuntimeError(
            "ScrollView::takeContentWidget uses Shiboken parent bookkeeping"
        )

    flip_view_path = generated_dir / FLIP_VIEW_WRAPPER
    if not flip_view_path.is_file():
        raise RuntimeError(
            "Generated FlipView wrapper was not found: {0}".format(
                flip_view_path
            )
        )
    flip_view_source = flip_view_path.read_text(encoding="utf-8")
    for public_bypass in (
        "Sbk_fluent_collections_FlipViewFunc_addPage(",
        "Sbk_fluent_collections_FlipViewFunc_insertPage(",
        "Sbk_fluent_collections_FlipViewFunc_removePage(",
        "Sbk_fluent_collections_FlipViewFunc_releasePage(",
    ):
        if public_bypass in flip_view_source:
            raise RuntimeError(
                "FlipView exposes a page ownership bypass: {0}".format(
                    public_bypass
                )
            )

    flip_view_adapters = (
        (
            "_addPageWithOwnership",
            "cppSelf->addPage(cppArg0, cppArg1)",
        ),
        (
            "_insertPageWithOwnership",
            "cppSelf->insertPage(cppArg0, cppArg1, cppArg2)",
        ),
        (
            "_releasePageWithOwnership",
            "cppSelf->releasePage(cppArg0)",
        ),
    )
    for adapter_name, native_call in flip_view_adapters:
        adapter = extract_function(
            flip_view_source,
            (
                "static PyObject *"
                "Sbk_fluent_collections_FlipViewFunc_{0}(".format(
                    adapter_name
                )
            ),
        )
        require_text(
            adapter,
            native_call,
            "FlipView private ownership adapter {0}".format(adapter_name),
        )
        for forbidden, description in (
            ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
            ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
            (
                "Shiboken::Object::keepReference",
                "uses keep-reference bookkeeping",
            ),
            ("Shiboken::Object::setParent", "uses parent bookkeeping"),
        ):
            if forbidden in adapter:
                raise RuntimeError(
                    "FlipView adapter {0} {1}".format(
                        adapter_name,
                        description,
                    )
                )

    flip_view_getter = extract_function(
        flip_view_source,
        "static PyObject *Sbk_fluent_collections_FlipViewFunc_pageAt(",
    )
    for forbidden, description in (
        ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::keepReference", "uses keep-reference bookkeeping"),
        ("Shiboken::Object::setParent", "uses parent bookkeeping"),
    ):
        if forbidden in flip_view_getter:
            raise RuntimeError("FlipView::pageAt {0}".format(description))

    flip_view_taker = extract_function(
        flip_view_source,
        "static PyObject *Sbk_fluent_collections_FlipViewFunc_takePage(",
    )
    require_text(
        flip_view_taker,
        "cppSelf->takePage(cppArg0)",
        "FlipView::takePage call",
    )
    require_text(
        flip_view_taker,
        "Shiboken::Object::getOwnership(pyResult)",
        "FlipView::takePage Python ownership transfer",
    )
    if "Shiboken::Object::keepReference" in flip_view_taker:
        raise RuntimeError(
            "FlipView::takePage uses keep-reference bookkeeping"
        )
    if "Shiboken::Object::setParent" in flip_view_taker:
        raise RuntimeError("FlipView::takePage uses parent bookkeeping")

    split_view_path = generated_dir / SPLIT_VIEW_WRAPPER
    if not split_view_path.is_file():
        raise RuntimeError(
            "Generated SplitView wrapper was not found: {0}".format(
                split_view_path
            )
        )
    split_view_source = split_view_path.read_text(encoding="utf-8")
    for public_bypass in (
        "Sbk_fluent_collections_SplitViewFunc_addPane(",
        "Sbk_fluent_collections_SplitViewFunc_insertPane(",
        "Sbk_fluent_collections_SplitViewFunc_removePane(",
        "Sbk_fluent_collections_SplitViewFunc_removePaneAt(",
        "Sbk_fluent_collections_SplitViewFunc_releasePane(",
        "Sbk_fluent_collections_SplitViewFunc_releasePaneAt(",
    ):
        if public_bypass in split_view_source:
            raise RuntimeError(
                "SplitView exposes a pane ownership bypass: {0}".format(
                    public_bypass
                )
            )

    split_view_adapters = (
        (
            "_addPaneWithOwnership",
            "cppSelf->addPane(cppArg0, cppArg1, *cppArg2)",
        ),
        (
            "_insertPaneWithOwnership",
            "cppSelf->insertPane(cppArg0, cppArg1, cppArg2, *cppArg3)",
        ),
        (
            "_releasePaneAtWithOwnership",
            "cppSelf->releasePaneAt(cppArg0)",
        ),
    )
    for adapter_name, native_call in split_view_adapters:
        adapter = extract_function(
            split_view_source,
            (
                "static PyObject *"
                "Sbk_fluent_collections_SplitViewFunc_{0}(".format(
                    adapter_name
                )
            ),
        )
        require_text(
            adapter,
            native_call,
            "SplitView private ownership adapter {0}".format(adapter_name),
        )
        for forbidden, description in (
            ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
            ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
            (
                "Shiboken::Object::keepReference",
                "uses keep-reference bookkeeping",
            ),
            ("Shiboken::Object::setParent", "uses parent bookkeeping"),
        ):
            if forbidden in adapter:
                raise RuntimeError(
                    "SplitView adapter {0} {1}".format(
                        adapter_name,
                        description,
                    )
                )

    split_view_getter = extract_function(
        split_view_source,
        "static PyObject *Sbk_fluent_collections_SplitViewFunc_paneAt(",
    )
    for forbidden, description in (
        ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::keepReference", "uses keep-reference bookkeeping"),
        ("Shiboken::Object::setParent", "uses parent bookkeeping"),
    ):
        if forbidden in split_view_getter:
            raise RuntimeError("SplitView::paneAt {0}".format(description))

    split_view_taker = extract_function(
        split_view_source,
        "static PyObject *Sbk_fluent_collections_SplitViewFunc_takePaneAt(",
    )
    require_text(
        split_view_taker,
        "cppSelf->takePaneAt(cppArg0)",
        "SplitView::takePaneAt call",
    )
    require_text(
        split_view_taker,
        "Shiboken::Object::getOwnership(pyResult)",
        "SplitView::takePaneAt Python ownership transfer",
    )
    if "Shiboken::Object::keepReference" in split_view_taker:
        raise RuntimeError(
            "SplitView::takePaneAt uses keep-reference bookkeeping"
        )
    if "Shiboken::Object::setParent" in split_view_taker:
        raise RuntimeError("SplitView::takePaneAt uses parent bookkeeping")

    verify_stack_content_host(generated_dir)
    verify_navigation_view(generated_dir)

    flow_view_path = generated_dir / FLOW_VIEW_WRAPPER
    if not flow_view_path.is_file():
        raise RuntimeError(
            "Generated FlowView wrapper was not found: {0}".format(
                flow_view_path
            )
        )
    flow_view_source = flow_view_path.read_text(encoding="utf-8")
    for setter_name, native_call in (
        ("setModel", "setModel(cppArg0)"),
        ("setItemDelegate", "setItemDelegate(cppArg0)"),
    ):
        setter = extract_function(
            flow_view_source,
            (
                "static PyObject *"
                "Sbk_fluent_collections_FlowViewFunc_{0}(".format(
                    setter_name
                )
            ),
        )
        require_text(
            setter,
            native_call,
            "FlowView::{0} native call".format(setter_name),
        )
        require_text(
            setter,
            "Shiboken::Object::keepReference",
            "FlowView::{0} caller-owned retention".format(setter_name),
        )
        for forbidden, description in (
            ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
            ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
            ("Shiboken::Object::setParent", "uses parent bookkeeping"),
        ):
            if forbidden in setter:
                raise RuntimeError(
                    "FlowView::{0} {1}".format(setter_name, description)
                )

    for forbidden_method in (
        "selectionMode",
        "setSelectionMode",
        "verticalFluentScrollBar",
    ):
        generated_signature = (
            "Sbk_fluent_collections_FlowViewFunc_{0}(".format(
                forbidden_method
            )
        )
        if generated_signature in flow_view_source:
            raise RuntimeError(
                "FlowView exposes unsupported native API {0}".format(
                    forbidden_method
                )
            )

    grid_view_path = generated_dir / GRID_VIEW_WRAPPER
    if not grid_view_path.is_file():
        raise RuntimeError(
            "Generated GridView wrapper was not found: {0}".format(
                grid_view_path
            )
        )
    grid_view_source = grid_view_path.read_text(encoding="utf-8")
    for forbidden_method in (
        "selectionMode",
        "setSelectionMode",
        "verticalFluentScrollBar",
    ):
        generated_signature = (
            "Sbk_fluent_collections_GridViewFunc_{0}(".format(
                forbidden_method
            )
        )
        if generated_signature in grid_view_source:
            raise RuntimeError(
                "GridView exposes unsupported native API {0}".format(
                    forbidden_method
                )
            )

    list_view_path = generated_dir / LIST_VIEW_WRAPPER
    if not list_view_path.is_file():
        raise RuntimeError(
            "Generated ListView wrapper was not found: {0}".format(
                list_view_path
            )
        )
    list_view_source = list_view_path.read_text(encoding="utf-8")
    for setter_name, native_call in (
        ("setModel", "setModel(cppArg0)"),
        ("setSelectionModel", "setSelectionModel(cppArg0)"),
    ):
        setter = extract_function(
            list_view_source,
            (
                "static PyObject *"
                "Sbk_fluent_collections_ListViewFunc_{0}(".format(
                    setter_name
                )
            ),
        )
        require_text(
            setter,
            native_call,
            "ListView::{0} native call".format(setter_name),
        )
        require_text(
            setter,
            "Shiboken::Object::keepReference",
            "ListView::{0} caller-owned retention".format(setter_name),
        )
        for forbidden, description in (
            ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
            ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
            ("Shiboken::Object::setParent", "uses parent bookkeeping"),
        ):
            if forbidden in setter:
                raise RuntimeError(
                    "ListView::{0} {1}".format(
                        setter_name,
                        description,
                    )
                )

    for forbidden_method in (
        "header",
        "setHeader",
        "footer",
        "setFooter",
        "sectionEnabled",
        "isSectionEnabled",
        "setSectionEnabled",
        "setSectionKeyFunction",
        "selectionMode",
        "setSelectionMode",
        "verticalFluentScrollBar",
        "horizontalFluentScrollBar",
    ):
        generated_signature = (
            "Sbk_fluent_collections_ListViewFunc_{0}(".format(
                forbidden_method
            )
        )
        if generated_signature in list_view_source:
            raise RuntimeError(
                "ListView exposes unsupported native API {0}".format(
                    forbidden_method
                )
            )

    tree_view_path = generated_dir / TREE_VIEW_WRAPPER
    if not tree_view_path.is_file():
        raise RuntimeError(
            "Generated TreeView wrapper was not found: {0}".format(
                tree_view_path
            )
        )
    tree_view_source = tree_view_path.read_text(encoding="utf-8")
    for setter_name, native_call in (
        ("setModel", "setModel(cppArg0)"),
        ("setSelectionModel", "setSelectionModel(cppArg0)"),
    ):
        setter = extract_function(
            tree_view_source,
            (
                "static PyObject *"
                "Sbk_fluent_collections_TreeViewFunc_{0}(".format(
                    setter_name
                )
            ),
        )
        require_text(
            setter,
            native_call,
            "TreeView::{0} native call".format(setter_name),
        )
        require_text(
            setter,
            "Shiboken::Object::keepReference",
            "TreeView::{0} caller-owned retention".format(setter_name),
        )
        for forbidden, description in (
            ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
            ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
            ("Shiboken::Object::setParent", "uses parent bookkeeping"),
        ):
            if forbidden in setter:
                raise RuntimeError(
                    "TreeView::{0} {1}".format(
                        setter_name,
                        description,
                    )
                )

    for forbidden_method in (
        "selectionMode",
        "setSelectionMode",
        "selectionIndicatorStyle",
        "setSelectionIndicatorStyle",
        "verticalFluentScrollBar",
        "horizontalFluentScrollBar",
    ):
        generated_signature = (
            "Sbk_fluent_collections_TreeViewFunc_{0}(".format(
                forbidden_method
            )
        )
        if generated_signature in tree_view_source:
            raise RuntimeError(
                "TreeView exposes unsupported native API {0}".format(
                    forbidden_method
                )
            )

    module_path = generated_dir / MODULE_WRAPPER
    if not module_path.is_file():
        raise RuntimeError(
            "Generated module wrapper was not found: {0}".format(module_path)
        )
    module_source = module_path.read_text(encoding="utf-8")
    for adapter_name, native_call in (
        ("flowViewSelectionMode", "flowViewSelectionMode(cppArg0)"),
        (
            "setFlowViewSelectionMode",
            "setFlowViewSelectionMode(cppArg0, cppArg1)",
        ),
    ):
        adapter_signature = (
            "static PyObject *Sbk_fluentqtModule_{0}(".format(adapter_name)
        )
        if adapter_signature not in module_source:
            raise RuntimeError(
                "FlowView selection adapter {0} is missing".format(
                    adapter_name
                )
            )
        adapter = extract_function(module_source, adapter_signature)
        require_text(
            adapter,
            native_call,
            "FlowView selection adapter {0}".format(adapter_name),
        )

    flow_scroll_adapter_name = "flowViewVerticalFluentScrollBar"
    flow_scroll_adapter_signature = (
        "static PyObject *Sbk_fluentqtModule_{0}(".format(
            flow_scroll_adapter_name
        )
    )
    if flow_scroll_adapter_signature not in module_source:
        raise RuntimeError(
            "FlowView scrollbar adapter {0} is missing".format(
                flow_scroll_adapter_name
            )
        )
    flow_scroll_adapter = extract_function(
        module_source,
        flow_scroll_adapter_signature,
    )
    require_text(
        flow_scroll_adapter,
        "{0}(cppArg0)".format(flow_scroll_adapter_name),
        "FlowView scrollbar adapter {0}".format(flow_scroll_adapter_name),
    )
    for forbidden, description in (
        ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
        (
            "Shiboken::Object::keepReference",
            "uses keep-reference bookkeeping",
        ),
        ("Shiboken::Object::setParent", "uses parent bookkeeping"),
    ):
        if forbidden in flow_scroll_adapter:
            raise RuntimeError(
                "FlowView scrollbar adapter {0} {1}".format(
                    flow_scroll_adapter_name,
                    description,
                )
            )

    for adapter_name, native_call in (
        ("gridViewSelectionMode", "gridViewSelectionMode(cppArg0)"),
        (
            "setGridViewSelectionMode",
            "setGridViewSelectionMode(cppArg0, cppArg1)",
        ),
    ):
        adapter_signature = (
            "static PyObject *Sbk_fluentqtModule_{0}(".format(adapter_name)
        )
        if adapter_signature not in module_source:
            raise RuntimeError(
                "GridView selection adapter {0} is missing".format(
                    adapter_name
                )
            )
        adapter = extract_function(module_source, adapter_signature)
        require_text(
            adapter,
            native_call,
            "GridView selection adapter {0}".format(adapter_name),
        )

    grid_scroll_adapter_name = "gridViewVerticalFluentScrollBar"
    grid_scroll_adapter_signature = (
        "static PyObject *Sbk_fluentqtModule_{0}(".format(
            grid_scroll_adapter_name
        )
    )
    if grid_scroll_adapter_signature not in module_source:
        raise RuntimeError(
            "GridView scrollbar adapter {0} is missing".format(
                grid_scroll_adapter_name
            )
        )
    grid_scroll_adapter = extract_function(
        module_source,
        grid_scroll_adapter_signature,
    )
    require_text(
        grid_scroll_adapter,
        "{0}(cppArg0)".format(grid_scroll_adapter_name),
        "GridView scrollbar adapter {0}".format(grid_scroll_adapter_name),
    )
    for forbidden, description in (
        ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
        (
            "Shiboken::Object::keepReference",
            "uses keep-reference bookkeeping",
        ),
        ("Shiboken::Object::setParent", "uses parent bookkeeping"),
    ):
        if forbidden in grid_scroll_adapter:
            raise RuntimeError(
                "GridView scrollbar adapter {0} {1}".format(
                    grid_scroll_adapter_name,
                    description,
                )
            )

    for adapter_name, native_call in (
        ("listViewSelectionMode", "listViewSelectionMode(cppArg0)"),
        (
            "setListViewSelectionMode",
            "setListViewSelectionMode(cppArg0, cppArg1)",
        ),
    ):
        adapter_signature = (
            "static PyObject *Sbk_fluentqtModule_{0}(".format(adapter_name)
        )
        if adapter_signature not in module_source:
            raise RuntimeError(
                "ListView selection adapter {0} is missing".format(
                    adapter_name
                )
            )
        adapter = extract_function(
            module_source,
            adapter_signature,
        )
        require_text(
            adapter,
            native_call,
            "ListView selection adapter {0}".format(adapter_name),
        )

    for adapter_name in (
        "listViewVerticalFluentScrollBar",
        "listViewHorizontalFluentScrollBar",
    ):
        adapter_signature = (
            "static PyObject *Sbk_fluentqtModule_{0}(".format(adapter_name)
        )
        if adapter_signature not in module_source:
            raise RuntimeError(
                "ListView scrollbar adapter {0} is missing".format(
                    adapter_name
                )
            )
        adapter = extract_function(module_source, adapter_signature)
        require_text(
            adapter,
            "{0}(cppArg0)".format(adapter_name),
            "ListView scrollbar adapter {0}".format(adapter_name),
        )
        for forbidden, description in (
            ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
            ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
            (
                "Shiboken::Object::keepReference",
                "uses keep-reference bookkeeping",
            ),
            ("Shiboken::Object::setParent", "uses parent bookkeeping"),
        ):
            if forbidden in adapter:
                raise RuntimeError(
                    "ListView scrollbar adapter {0} {1}".format(
                        adapter_name,
                        description,
                    )
                )

    for adapter_name, native_call in (
        ("treeViewSelectionMode", "treeViewSelectionMode(cppArg0)"),
        (
            "setTreeViewSelectionMode",
            "setTreeViewSelectionMode(cppArg0, cppArg1)",
        ),
    ):
        adapter_signature = (
            "static PyObject *Sbk_fluentqtModule_{0}(".format(adapter_name)
        )
        if adapter_signature not in module_source:
            raise RuntimeError(
                "TreeView selection adapter {0} is missing".format(
                    adapter_name
                )
            )
        adapter = extract_function(module_source, adapter_signature)
        require_text(
            adapter,
            native_call,
            "TreeView selection adapter {0}".format(adapter_name),
        )

    for adapter_name in (
        "treeViewVerticalFluentScrollBar",
        "treeViewHorizontalFluentScrollBar",
    ):
        adapter_signature = (
            "static PyObject *Sbk_fluentqtModule_{0}(".format(adapter_name)
        )
        if adapter_signature not in module_source:
            raise RuntimeError(
                "TreeView scrollbar adapter {0} is missing".format(
                    adapter_name
                )
            )
        adapter = extract_function(module_source, adapter_signature)
        require_text(
            adapter,
            "{0}(cppArg0)".format(adapter_name),
            "TreeView scrollbar adapter {0}".format(adapter_name),
        )
        for forbidden, description in (
            ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
            ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
            (
                "Shiboken::Object::keepReference",
                "uses keep-reference bookkeeping",
            ),
            ("Shiboken::Object::setParent", "uses parent bookkeeping"),
        ):
            if forbidden in adapter:
                raise RuntimeError(
                    "TreeView scrollbar adapter {0} {1}".format(
                        adapter_name,
                        description,
                    )
                )

    fluent_namespace_path = generated_dir / FLUENT_NAMESPACE_WRAPPER
    if not fluent_namespace_path.is_file():
        raise RuntimeError(
            "Generated fluent namespace wrapper was not found: {0}".format(
                fluent_namespace_path
            )
        )
    fluent_namespace_source = fluent_namespace_path.read_text(
        encoding="utf-8"
    )
    # Converter helper symbol names differ between Shiboken 6.2 and newer
    # generators. The C++ converter registration name is the stable contract.
    converter_count = len(
        re.findall(
            (
                r"registerConverterName\s*\([^;]*"
                r'"fluent::binding::SelectionMode"\s*\)'
            ),
            fluent_namespace_source,
            re.DOTALL,
        )
    )
    if converter_count != 1:
        raise RuntimeError(
            "Expected one binding SelectionMode converter, found {0}".format(
                converter_count
            )
        )
    if "fluent::collections::SelectionMode" in fluent_namespace_source:
        raise RuntimeError(
            "ListView generated the unstable collections SelectionMode "
            "converter"
        )

    stack_view_path = generated_dir / STACK_VIEW_WRAPPER
    if not stack_view_path.is_file():
        raise RuntimeError(
            "Generated StackView wrapper was not found: {0}".format(
                stack_view_path
            )
        )
    stack_view_source = stack_view_path.read_text(encoding="utf-8")
    for public_bypass in (
        "Sbk_fluent_collections_StackViewFunc_push(",
        "Sbk_fluent_collections_StackViewFunc_replace(",
        "Sbk_fluent_collections_StackViewFunc_setInitialItem(",
        "Sbk_fluent_collections_StackViewFunc_setCurrentWidget(",
        "Sbk_fluent_collections_StackViewFunc_adoptWidget(",
        "Sbk_fluent_collections_StackViewFunc_defaultItemOwnership(",
    ):
        if public_bypass in stack_view_source:
            raise RuntimeError(
                "StackView exposes a navigation ownership bypass: {0}".format(
                    public_bypass
                )
            )

    stack_view_adapters = (
        (
            "_pushItemWithOwnership",
            "cppSelf->push(cppArg0, cppArg1)",
        ),
        (
            "_pushItemsWithOwnership",
            "cppSelf->push(cppArg0, cppArg1)",
        ),
        (
            "_replaceAtWithOwnership",
            "cppSelf->replace(cppArg0, cppArg1, cppArg2)",
        ),
        (
            "_replaceCurrentWithOwnership",
            "cppSelf->replace(cppArg0, cppArg1)",
        ),
        (
            "_setInitialItemWithOwnership",
            "cppSelf->setInitialItem(cppArg0, cppArg1)",
        ),
    )
    for adapter_name, native_call in stack_view_adapters:
        signature = (
            "static PyObject *"
            "Sbk_fluent_collections_StackViewFunc_{0}(".format(adapter_name)
        )
        adapter = extract_function(stack_view_source, signature)
        require_text(
            adapter,
            native_call,
            "StackView private ownership adapter {0}".format(adapter_name),
        )
        for forbidden, description in (
            ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
            ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
            (
                "Shiboken::Object::keepReference",
                "uses keep-reference bookkeeping",
            ),
            ("Shiboken::Object::setParent", "uses parent bookkeeping"),
        ):
            if forbidden in adapter:
                raise RuntimeError(
                    "StackView private ownership adapter {0} {1}".format(
                        adapter_name,
                        description,
                    )
                )

    for getter_name in ("currentItem", "initialItem", "itemAt"):
        getter = extract_function(
            stack_view_source,
            (
                "static PyObject *"
                "Sbk_fluent_collections_StackViewFunc_{0}(".format(
                    getter_name
                )
            ),
        )
        for forbidden, description in (
            ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
            ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
            (
                "Shiboken::Object::keepReference",
                "uses keep-reference bookkeeping",
            ),
            ("Shiboken::Object::setParent", "uses parent bookkeeping"),
        ):
            if forbidden in getter:
                raise RuntimeError(
                    "StackView::{0} {1}".format(getter_name, description)
                )

    breadcrumb_path = generated_dir / BREADCRUMB_WRAPPER
    breadcrumb_item_path = generated_dir / BREADCRUMB_ITEM_WRAPPER
    if not breadcrumb_path.is_file():
        raise RuntimeError(
            "Generated Breadcrumb wrapper was not found: {0}".format(
                breadcrumb_path
            )
        )
    if not breadcrumb_item_path.is_file():
        raise RuntimeError(
            "Generated BreadcrumbItem wrapper was not found: {0}".format(
                breadcrumb_item_path
            )
        )

    breadcrumb_source = breadcrumb_path.read_text(encoding="utf-8")
    breadcrumb_adder = extract_function(
        breadcrumb_source,
        "static PyObject *Sbk_fluent_navigation_BreadcrumbFunc_appendItem(",
    )
    require_text(
        breadcrumb_adder,
        "cppSelf->appendItem(cppArg0)",
        "Breadcrumb::appendItem QString overload",
    )
    require_text(
        breadcrumb_adder,
        "cppSelf->appendItem(*cppArg0)",
        "Breadcrumb::appendItem BreadcrumbItem overload",
    )

    breadcrumb_getter = extract_function(
        breadcrumb_source,
        "static PyObject *Sbk_fluent_navigation_BreadcrumbFunc_itemAt(",
    )
    require_text(
        breadcrumb_getter,
        "itemAt(cppArg0)",
        "Breadcrumb::itemAt value call",
    )
    require_text(
        breadcrumb_getter,
        "BreadcrumbItem",
        "Breadcrumb::itemAt value conversion",
    )
    require_text(
        breadcrumb_getter,
        "copyToPython",
        "Breadcrumb::itemAt Python conversion",
    )

    native_setter_signature = (
        "Sbk_fluent_navigation_BreadcrumbFunc_setItems("
    )
    if native_setter_signature in breadcrumb_source:
        raise RuntimeError(
            "Breadcrumb exposes ambiguous native setItems overload"
        )

    for adapter_name, native_call, converter_types in (
        (
            "setBreadcrumbTextItems",
            "setBreadcrumbTextItems(cppArg0, cppArg1)",
            ("QStringList",),
        ),
        (
            "setBreadcrumbMetadataItems",
            "setBreadcrumbMetadataItems(cppArg0, cppArg1)",
            (
                "QList<fluent::navigation::BreadcrumbItem>",
                "QVector<fluent::navigation::BreadcrumbItem>",
            ),
        ),
    ):
        adapter_signature = (
            "static PyObject *Sbk_fluentqtModule_{0}(".format(adapter_name)
        )
        if adapter_signature not in module_source:
            raise RuntimeError(
                "Breadcrumb list adapter {0} is missing".format(
                    adapter_name
                )
            )
        adapter = extract_function(module_source, adapter_signature)
        require_text(
            adapter,
            native_call,
            "Breadcrumb list adapter {0}".format(adapter_name),
        )
        if not any(
            converter_type in adapter for converter_type in converter_types
        ):
            raise RuntimeError(
                "Breadcrumb list adapter {0} converter is missing: {1}".format(
                    adapter_name,
                    " or ".join(converter_types),
                )
            )
        for forbidden, description in (
            ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
            ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
            (
                "Shiboken::Object::keepReference",
                "uses keep-reference bookkeeping",
            ),
            ("Shiboken::Object::setParent", "uses parent bookkeeping"),
        ):
            if forbidden in adapter:
                raise RuntimeError(
                    "Breadcrumb list adapter {0} {1}".format(
                        adapter_name,
                        description,
                    )
                )

    breadcrumb_item_source = breadcrumb_item_path.read_text(encoding="utf-8")
    for field_name in ("text", "data", "enabled", "accessibleName"):
        require_text(
            breadcrumb_item_source,
            "BreadcrumbItem_get_{0}(".format(field_name),
            "BreadcrumbItem field {0} getter".format(field_name),
        )
        require_text(
            breadcrumb_item_source,
            "BreadcrumbItem_set_{0}(".format(field_name),
            "BreadcrumbItem field {0} setter".format(field_name),
        )
    breadcrumb_data_getter = extract_function(
        breadcrumb_item_source,
        "Sbk_fluent_navigation_BreadcrumbItem_get_data(",
    )
    require_text(
        breadcrumb_data_getter,
        "copyToPython",
        "BreadcrumbItem QVariant data getter conversion",
    )
    breadcrumb_data_setter = extract_function(
        breadcrumb_item_source,
        "Sbk_fluent_navigation_BreadcrumbItem_set_data(",
    )
    require_text(
        breadcrumb_data_setter,
        "pythonToCpp",
        "BreadcrumbItem QVariant data setter conversion",
    )

    verify_navigation_metadata_widget(
        generated_dir,
        "Pivot",
        "PivotItem",
        PIVOT_WRAPPER,
        PIVOT_ITEM_WRAPPER,
        ("header", "iconGlyph", "enabled", "data", "accessibleName"),
    )
    verify_navigation_metadata_widget(
        generated_dir,
        "SelectorBar",
        "SelectorBarItem",
        SELECTOR_BAR_WRAPPER,
        SELECTOR_BAR_ITEM_WRAPPER,
        (
            "text",
            "iconGlyph",
            "enabled",
            "visible",
            "selected",
            "data",
            "accessibleName",
        ),
    )

    tab_view_path = generated_dir / TAB_VIEW_WRAPPER
    tab_view_item_path = generated_dir / TAB_VIEW_ITEM_WRAPPER
    if not tab_view_path.is_file():
        raise RuntimeError(
            "Generated TabView wrapper was not found: {0}".format(
                tab_view_path
            )
        )
    if not tab_view_item_path.is_file():
        raise RuntimeError(
            "Generated TabViewItem wrapper was not found: {0}".format(
                tab_view_item_path
            )
        )
    if (generated_dir / TAB_STRIP_WRAPPER).exists():
        raise RuntimeError(
            "TabView exposes its internal TabStrip implementation"
        )

    tab_view_source = tab_view_path.read_text(encoding="utf-8")
    tab_adder = extract_function(
        tab_view_source,
        "static PyObject *Sbk_fluent_navigation_TabViewFunc_addTab(",
    )
    require_text(
        tab_adder,
        "cppSelf->addTab(cppArg0)",
        "TabView::addTab QString overload",
    )
    require_text(
        tab_adder,
        "cppSelf->addTab(*cppArg0)",
        "TabView::addTab TabViewItem overload",
    )

    tab_getter = extract_function(
        tab_view_source,
        "static PyObject *Sbk_fluent_navigation_TabViewFunc_tabAt(",
    )
    require_text(
        tab_getter,
        "tabAt(cppArg0)",
        "TabView::tabAt value call",
    )
    require_text(
        tab_getter,
        "TabViewItem",
        "TabView::tabAt value conversion",
    )
    require_text(
        tab_getter,
        "copyToPython",
        "TabView::tabAt Python conversion",
    )

    tab_view_item_source = tab_view_item_path.read_text(encoding="utf-8")
    for field_name in (
        "text",
        "iconGlyph",
        "closable",
        "enabled",
        "data",
        "accessibleName",
    ):
        require_text(
            tab_view_item_source,
            "TabViewItem_get_{0}(".format(field_name),
            "TabViewItem field {0} getter".format(field_name),
        )
        require_text(
            tab_view_item_source,
            "TabViewItem_set_{0}(".format(field_name),
            "TabViewItem field {0} setter".format(field_name),
        )
    data_getter = extract_function(
        tab_view_item_source,
        "Sbk_fluent_navigation_TabViewItem_get_data(",
    )
    require_text(
        data_getter,
        "copyToPython",
        "TabViewItem QVariant data getter conversion",
    )
    data_setter = extract_function(
        tab_view_item_source,
        "Sbk_fluent_navigation_TabViewItem_set_data(",
    )
    require_text(
        data_setter,
        "pythonToCpp",
        "TabViewItem QVariant data setter conversion",
    )

    annotated_scroll_bar_path = generated_dir / ANNOTATED_SCROLL_BAR_WRAPPER
    if not annotated_scroll_bar_path.is_file():
        raise RuntimeError(
            "Generated AnnotatedScrollBar wrapper was not found: {0}".format(
                annotated_scroll_bar_path
            )
        )
    annotated_scroll_bar_source = annotated_scroll_bar_path.read_text(
        encoding="utf-8"
    )
    for provider_method in (
        "setDetailLabelProvider",
        "clearDetailLabelProvider",
        "hasDetailLabelProvider",
    ):
        if provider_method in annotated_scroll_bar_source:
            raise RuntimeError(
                "AnnotatedScrollBar exposes unsupported provider API {0}".format(
                    provider_method
                )
            )

    scroll_link = extract_function(
        annotated_scroll_bar_source,
        (
            "static PyObject *"
            "Sbk_fluent_scrolling_AnnotatedScrollBarFunc_"
            "connectToScrollView("
        ),
    )
    require_text(
        scroll_link,
        "cppSelf->connectToScrollView(cppArg0)",
        "AnnotatedScrollBar::connectToScrollView call",
    )
    for forbidden, description in (
        ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::keepReference", "uses keep-reference bookkeeping"),
        ("Shiboken::Object::setParent", "uses parent bookkeeping"),
    ):
        if forbidden in scroll_link:
            raise RuntimeError(
                "AnnotatedScrollBar::connectToScrollView {0}".format(
                    description
                )
            )

    connected_scroll_view = extract_function(
        annotated_scroll_bar_source,
        (
            "static PyObject *"
            "Sbk_fluent_scrolling_AnnotatedScrollBarFunc_"
            "connectedScrollView("
        ),
    )
    require_text(
        connected_scroll_view,
        "connectedScrollView()",
        "AnnotatedScrollBar::connectedScrollView call",
    )
    for forbidden, description in (
        ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::keepReference", "uses keep-reference bookkeeping"),
        ("Shiboken::Object::setParent", "uses parent bookkeeping"),
    ):
        if forbidden in connected_scroll_view:
            raise RuntimeError(
                "AnnotatedScrollBar::connectedScrollView {0}".format(
                    description
                )
            )

    accordion_path = generated_dir / ACCORDION_WRAPPER
    if not accordion_path.is_file():
        raise RuntimeError(
            "Generated Accordion wrapper was not found: {0}".format(
                accordion_path
            )
        )
    accordion_source = accordion_path.read_text(encoding="utf-8")
    for public_overload in (
        "Sbk_fluent_layout_AccordionFunc_addItem(",
        "Sbk_fluent_layout_AccordionFunc_insertItem(",
    ):
        if public_overload in accordion_source:
            raise RuntimeError(
                "Accordion exposes a runtime ownership bypass: {0}".format(
                    public_overload
                )
            )

    accordion_adapters = (
        (
            "static PyObject *"
            "Sbk_fluent_layout_AccordionFunc__addItemWithOwnership(",
            "cppSelf->addItem(cppArg0, cppArg1)",
            "Accordion private add ownership adapter",
        ),
        (
            "static PyObject *"
            "Sbk_fluent_layout_AccordionFunc__insertItemWithOwnership(",
            "cppSelf->insertItem(cppArg0, cppArg1, cppArg2)",
            "Accordion private insert ownership adapter",
        ),
    )
    for signature, native_call, context in accordion_adapters:
        adapter = extract_function(accordion_source, signature)
        require_text(adapter, native_call, context)
        for forbidden, description in (
            ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
            ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
            (
                "Shiboken::Object::keepReference",
                "uses keep-reference bookkeeping",
            ),
            ("Shiboken::Object::setParent", "uses parent bookkeeping"),
        ):
            if forbidden in adapter:
                raise RuntimeError("{0} {1}".format(context, description))

    accordion_getter = extract_function(
        accordion_source,
        "static PyObject *Sbk_fluent_layout_AccordionFunc_itemAt(",
    )
    for forbidden, description in (
        ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::keepReference", "uses keep-reference bookkeeping"),
        ("Shiboken::Object::setParent", "uses parent bookkeeping"),
    ):
        if forbidden in accordion_getter:
            raise RuntimeError(
                "Accordion::itemAt {0}".format(description)
            )

    accordion_taker = extract_function(
        accordion_source,
        "static PyObject *Sbk_fluent_layout_AccordionFunc_takeItem(",
    )
    require_text(
        accordion_taker,
        "cppSelf->takeItem(cppArg0)",
        "Accordion::takeItem call",
    )
    require_text(
        accordion_taker,
        "Shiboken::Object::getOwnership(pyResult)",
        "Accordion::takeItem Python ownership transfer",
    )
    if "Shiboken::Object::keepReference" in accordion_taker:
        raise RuntimeError(
            "Accordion::takeItem uses keep-reference bookkeeping"
        )
    if "Shiboken::Object::setParent" in accordion_taker:
        raise RuntimeError("Accordion::takeItem uses parent bookkeeping")

    expander_path = generated_dir / EXPANDER_WRAPPER
    if not expander_path.is_file():
        raise RuntimeError(
            "Generated Expander wrapper was not found: {0}".format(
                expander_path
            )
        )
    expander_source = expander_path.read_text(encoding="utf-8")
    expander_setter = extract_function(
        expander_source,
        (
            "static PyObject *"
            "Sbk_fluent_layout_ExpanderFunc_setContentWidget("
        ),
    )
    require_text(
        expander_setter,
        "cppSelf->setContentWidget(cppArg0)",
        "Expander::setContentWidget call",
    )
    if "WidgetOwnership" in expander_setter:
        raise RuntimeError(
            "Expander::setContentWidget exposes the runtime ownership overload"
        )
    for forbidden, description in (
        ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::keepReference", "uses keep-reference bookkeeping"),
        ("Shiboken::Object::setParent", "uses parent bookkeeping"),
    ):
        if forbidden in expander_setter:
            raise RuntimeError(
                "Expander::setContentWidget {0}".format(description)
            )

    expander_adapter = extract_function(
        expander_source,
        (
            "static PyObject *"
            "Sbk_fluent_layout_ExpanderFunc_"
            "_setContentWidgetWithOwnership("
        ),
    )
    require_text(
        expander_adapter,
        "cppSelf->setContentWidget(cppArg0, cppArg1)",
        "Expander private ownership adapter call",
    )
    for forbidden, description in (
        ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::keepReference", "uses keep-reference bookkeeping"),
        ("Shiboken::Object::setParent", "uses parent bookkeeping"),
    ):
        if forbidden in expander_adapter:
            raise RuntimeError(
                "Expander private ownership adapter {0}".format(description)
            )

    expander_getter = extract_function(
        expander_source,
        "static PyObject *Sbk_fluent_layout_ExpanderFunc_contentWidget(",
    )
    if "Shiboken::Object::setParent" in expander_getter:
        raise RuntimeError(
            "Expander::contentWidget uses the return-value parent heuristic"
        )
    if (
        "Shiboken::Object::releaseOwnership" in expander_getter
        or "Shiboken::Object::getOwnership" in expander_getter
    ):
        raise RuntimeError(
            "Expander::contentWidget changes Python wrapper ownership"
        )

    expander_taker = extract_function(
        expander_source,
        "static PyObject *Sbk_fluent_layout_ExpanderFunc_takeContentWidget(",
    )
    require_text(
        expander_taker,
        "cppSelf->takeContentWidget()",
        "Expander::takeContentWidget call",
    )
    require_text(
        expander_taker,
        "Shiboken::Object::getOwnership(pyResult)",
        "Expander::takeContentWidget Python ownership transfer",
    )
    if "Shiboken::Object::keepReference" in expander_taker:
        raise RuntimeError(
            "Expander::takeContentWidget uses keep-reference bookkeeping"
        )
    if "Shiboken::Object::setParent" in expander_taker:
        raise RuntimeError(
            "Expander::takeContentWidget uses parent bookkeeping"
        )
    if "Sbk_fluent_layout_ExpanderFunc_headerButton(" in expander_source:
        raise RuntimeError("Expander exposes its internal header button")

    info_bar_path = generated_dir / INFO_BAR_WRAPPER
    if not info_bar_path.is_file():
        raise RuntimeError(
            "Generated InfoBar wrapper was not found: {0}".format(
                info_bar_path
            )
        )
    info_bar_source = info_bar_path.read_text(encoding="utf-8")
    action_setter = extract_function(
        info_bar_source,
        (
            "static PyObject *"
            "Sbk_fluent_status_info_InfoBarFunc__setActionWidget("
        ),
    )
    require_text(
        action_setter,
        "cppSelf->setActionWidget(cppArg0)",
        "InfoBar private action adapter call",
    )
    for forbidden, description in (
        ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::keepReference", "uses keep-reference bookkeeping"),
        ("Shiboken::Object::setParent", "uses parent bookkeeping"),
    ):
        if forbidden in action_setter:
            raise RuntimeError(
                "InfoBar private action adapter {0}".format(description)
            )

    action_getter = extract_function(
        info_bar_source,
        "static PyObject *Sbk_fluent_status_info_InfoBarFunc_actionWidget(",
    )
    for forbidden, description in (
        ("Shiboken::Object::releaseOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::getOwnership", "changes wrapper ownership"),
        ("Shiboken::Object::keepReference", "uses keep-reference bookkeeping"),
        ("Shiboken::Object::setParent", "uses parent bookkeeping"),
    ):
        if forbidden in action_getter:
            raise RuntimeError(
                "InfoBar::actionWidget {0}".format(description)
            )

    pips_pager_path = generated_dir / PIPS_PAGER_WRAPPER
    if not pips_pager_path.is_file():
        raise RuntimeError(
            "Generated PipsPager wrapper was not found: {0}".format(
                pips_pager_path
            )
        )
    pips_pager_source = pips_pager_path.read_text(encoding="utf-8")
    for internal_property in (
        "selectedVisualOffset",
        "visibleWindowOffset",
    ):
        if internal_property in pips_pager_source:
            raise RuntimeError(
                "PipsPager exposes internal animation property {0}".format(
                    internal_property
                )
            )
    verified_contracts = [
        "nativeEvent",
        "ScrollView ownership",
        "FlipView page ownership",
        "SplitView pane ownership",
        "StackContentHost page ownership",
        "NavigationView chrome ownership",
        "collection view model and delegate lifetime",
        "StackView navigation ownership",
        "Breadcrumb metadata navigation",
        "Pivot/SelectorBar metadata navigation",
        "TabView metadata navigation",
        "AnnotatedScrollBar borrowed link",
        "Accordion ownership",
        "Expander ownership",
        "InfoBar action ownership",
        "PipsPager animation privacy",
    ]
    if check_backdrop_converter:
        namespace_path = generated_dir / WINDOWING_NAMESPACE_WRAPPER
        if not namespace_path.is_file():
            raise RuntimeError(
                "Generated windowing namespace wrapper was not found: {0}".format(
                    namespace_path
                )
            )
        namespace_source = namespace_path.read_text(encoding="utf-8")
        enum_converter = (
            "static void "
            "Enum_PythonToCpp_fluent_windowing_BackdropEffect"
        )
        enum_count = namespace_source.count(enum_converter)
        if enum_count != 1:
            raise RuntimeError(
                "Expected one generated BackdropEffect converter, found {0}".format(
                    enum_count
                )
            )
        verified_contracts.append("BackdropEffect")

    print(
        "Verified generated {0} contracts in {1}".format(
            " and ".join(verified_contracts),
            generated_dir,
        )
    )


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--generated-dir", type=Path, required=True)
    parser.add_argument("--check-backdrop-converter", action="store_true")
    return parser.parse_args()


if __name__ == "__main__":
    try:
        arguments = parse_args()
        verify_contracts(
            arguments.generated_dir.resolve(),
            arguments.check_backdrop_converter,
        )
    except Exception as error:
        sys.stderr.write("Generated binding contract check failed: {0}\n".format(error))
        sys.exit(1)
