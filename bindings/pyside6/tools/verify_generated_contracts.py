"""Verify safety-sensitive contracts in generated FluentQt wrappers."""

import argparse
from pathlib import Path
import re
import sys


WINDOW_WRAPPER = "fluent_windowing_window_wrapper.cpp"
WINDOWING_NAMESPACE_WRAPPER = "fluent_windowing_wrapper.cpp"
ACCORDION_WRAPPER = "fluent_layout_accordion_wrapper.cpp"
EXPANDER_WRAPPER = "fluent_layout_expander_wrapper.cpp"
INFO_BAR_WRAPPER = "fluent_status_info_infobar_wrapper.cpp"
ANNOTATED_SCROLL_BAR_WRAPPER = (
    "fluent_scrolling_annotatedscrollbar_wrapper.cpp"
)
PIPS_PAGER_WRAPPER = "fluent_scrolling_pipspager_wrapper.cpp"
SCROLL_VIEW_WRAPPER = "fluent_scrolling_scrollview_wrapper.cpp"
STACK_VIEW_WRAPPER = "fluent_collections_stackview_wrapper.cpp"
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
        "StackView navigation ownership",
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
