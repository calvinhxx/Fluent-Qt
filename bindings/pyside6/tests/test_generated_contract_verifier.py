"""Tests for the generated-wrapper contract verifier."""

from pathlib import Path
import subprocess
import sys
import tempfile
import textwrap
import unittest


VERIFIER = (
    Path(__file__).resolve().parents[1]
    / "tools"
    / "verify_generated_contracts.py"
)
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
ENUM_CONVERTER = (
    "static void Enum_PythonToCpp_fluent_windowing_BackdropEffect"
)


def window_source(
    limited_api_arguments=True,
    expose_result_pointer=False,
    split_override=True,
):
    limited_api = (
        'auto args = Py_BuildValue("(NN)", first, second);'
        if limited_api_arguments
        else ""
    )
    pointer_exposure = (
        "auto exposed = Shiboken::Conversions::copyToPython(converter, result);"
        if expose_result_pointer
        else ""
    )
    if split_override:
        native_event = """
        bool WindowWrapper::nativeEvent(
            const QByteArray &, void *, long long *result)
        {{
            return this->::fluent::windowing::Window::nativeEvent(
                QByteArray(), nullptr, result);
        }}

        bool WindowWrapper::sbk_o_nativeEvent(
            const char *, const char *, Gil &, const Ref &,
            const QByteArray &, void *, long long *result)
        {{
            PyObject *pyArgArray[2] = {{first, second}};
            {limited_api}
            {pointer_exposure}
            return false;
        }}
        """
    else:
        native_event = """
        bool WindowWrapper::nativeEvent(
            const QByteArray &, void *, long long *result)
        {{
            if (useFallback)
                return this->::fluent::windowing::Window::nativeEvent(
                    QByteArray(), nullptr, result);
            {limited_api}
            {pointer_exposure}
            return false;
        }}
        """
    native_event = textwrap.dedent(native_event).format(
        limited_api=limited_api,
        pointer_exposure=pointer_exposure,
    )
    return textwrap.dedent(
        """
        bool WindowWrapper::eventFilter(QObject *, QEvent *)
        {{
            auto decoy = Py_BuildValue("(NN)", first, second);
            return decoy != nullptr;
        }}

        {native_event}
        """
    ).format(
        native_event=native_event,
    )


def scroll_view_source(
    release_old_child=False,
    runtime_overload=False,
    use_parent=False,
    keep_reference=False,
    adapter_use_parent=False,
    adapter_keep_reference=False,
    adapter_transfer_to_cpp=False,
    adapter_transfer_to_python=False,
    take_keep_reference=False,
    transfer_to_cpp=False,
    transfer_to_python=True,
    getter_parent=False,
    getter_transfer_to_cpp=False,
    take_parent=False,
):
    release = (
        "Shiboken::Object::releaseOwnership(oldChild);"
        if release_old_child
        else ""
    )
    ownership = (
        "auto mode = fluent::WidgetOwnership::Owned;"
        if runtime_overload
        else ""
    )
    parent = (
        "Shiboken::Object::setParent(self, pyArg);"
        if use_parent
        else ""
    )
    keep = (
        "Shiboken::Object::keepReference("
        "reinterpret_cast<SbkObject *>(self), "
        '"__fluentqt_scrollview_content", pyArg);'
        if keep_reference
        else ""
    )
    release_new = (
        "Shiboken::Object::releaseOwnership(pyArg);"
        if transfer_to_cpp
        else ""
    )
    adapter_parent = (
        "Shiboken::Object::setParent(self, pyArg);"
        if adapter_use_parent
        else ""
    )
    adapter_keep = (
        "Shiboken::Object::keepReference("
        "reinterpret_cast<SbkObject *>(self), "
        '"__fluentqt_scrollview_content", pyArg);'
        if adapter_keep_reference
        else ""
    )
    adapter_release = (
        "Shiboken::Object::releaseOwnership(pyArg);"
        if adapter_transfer_to_cpp
        else ""
    )
    adapter_acquire = (
        "Shiboken::Object::getOwnership(pyArg);"
        if adapter_transfer_to_python
        else ""
    )
    acquire_taken = (
        "Shiboken::Object::getOwnership(pyResult);"
        if transfer_to_python
        else ""
    )
    if getter_parent:
        getter_ownership = "Shiboken::Object::setParent(self, pyResult);"
    elif getter_transfer_to_cpp:
        getter_ownership = "Shiboken::Object::releaseOwnership(pyResult);"
    else:
        getter_ownership = ""
    taken_parent = (
        "Shiboken::Object::setParent(self, pyResult);"
        if take_parent
        else ""
    )
    take_keep = (
        "Shiboken::Object::keepReference("
        "reinterpret_cast<SbkObject *>(self), "
        '"__fluentqt_scrollview_content", Py_None);'
        if take_keep_reference
        else ""
    )
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_scrolling_ScrollViewFunc_setContentWidget(
            PyObject *self, PyObject *pyArg)
        {{
            {ownership}
            {release}
            cppSelf->setContentWidget(cppArg0);
            {keep}
            {release_new}
            {parent}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_scrolling_ScrollViewFunc__setContentWidgetWithOwnership(
            PyObject *self, PyObject *args)
        {{
            auto cppResult = cppSelf->setContentWidget(cppArg0, cppArg1);
            {adapter_parent}
            {adapter_keep}
            {adapter_release}
            {adapter_acquire}
            return PyBool_FromLong(cppResult);
        }}

        static PyObject *Sbk_fluent_scrolling_ScrollViewFunc_contentWidget(
            PyObject *self)
        {{
            pyResult = cppSelf->contentWidget();
            {getter_ownership}
            return pyResult;
        }}

        static PyObject *Sbk_fluent_scrolling_ScrollViewFunc_takeContentWidget(
            PyObject *self)
        {{
            pyResult = cppSelf->takeContentWidget();
            {take_keep}
            {acquire_taken}
            {taken_parent}
            return Py_None;
        }}
        """
    ).format(
        ownership=ownership,
        release=release,
        keep=keep,
        release_new=release_new,
        parent=parent,
        adapter_parent=adapter_parent,
        adapter_keep=adapter_keep,
        adapter_release=adapter_release,
        adapter_acquire=adapter_acquire,
        acquire_taken=acquire_taken,
        getter_ownership=getter_ownership,
        take_keep=take_keep,
        taken_parent=taken_parent,
    )


def list_view_source(
    retain_model=True,
    retain_selection_model=True,
    public_method=None,
):
    model_retention = (
        'Shiboken::Object::keepReference(self, "model", pyArg);'
        if retain_model
        else ""
    )
    selection_retention = (
        'Shiboken::Object::keepReference(self, "selection", pyArg);'
        if retain_selection_model
        else ""
    )
    public_function = ""
    if public_method is not None:
        public_function = textwrap.dedent(
            """
            static PyObject *
            Sbk_fluent_collections_ListViewFunc_{method}(
                PyObject *self)
            {{
                return Py_None;
            }}
            """
        ).format(method=public_method)
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_collections_ListViewFunc_setModel(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->setModel(cppArg0);
            {model_retention}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_collections_ListViewFunc_setSelectionModel(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->setSelectionModel(cppArg0);
            {selection_retention}
            return Py_None;
        }}

        {public_function}
        """
    ).format(
        model_retention=model_retention,
        selection_retention=selection_retention,
        public_function=public_function,
    )


def grid_view_source(public_method=None):
    if public_method is None:
        return ""
    return textwrap.dedent(
        """
        static PyObject *
        Sbk_fluent_collections_GridViewFunc_{method}(
            PyObject *self)
        {{
            return Py_None;
        }}
        """
    ).format(method=public_method)


def flip_view_source(
    public_method=None,
    adapter_operation="",
    getter_operation="",
    take_operation="",
    transfer_taken=True,
):
    public_function = ""
    if public_method is not None:
        public_function = textwrap.dedent(
            """
            static PyObject *
            Sbk_fluent_collections_FlipViewFunc_{method}(
                PyObject *self)
            {{
                return Py_None;
            }}
            """
        ).format(method=public_method)
    take_transfer = (
        "Shiboken::Object::getOwnership(pyResult);"
        if transfer_taken
        else ""
    )
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_collections_FlipViewFunc__addPageWithOwnership(
            PyObject *self, PyObject *args)
        {{
            cppSelf->addPage(cppArg0, cppArg1);
            {adapter_operation}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_collections_FlipViewFunc__insertPageWithOwnership(
            PyObject *self, PyObject *args)
        {{
            cppSelf->insertPage(cppArg0, cppArg1, cppArg2);
            {adapter_operation}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_collections_FlipViewFunc__releasePageWithOwnership(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->releasePage(cppArg0);
            {adapter_operation}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_collections_FlipViewFunc_pageAt(
            PyObject *self, PyObject *pyArg)
        {{
            pyResult = cppSelf->pageAt(cppArg0);
            {getter_operation}
            return pyResult;
        }}

        static PyObject *Sbk_fluent_collections_FlipViewFunc_takePage(
            PyObject *self, PyObject *pyArg)
        {{
            pyResult = cppSelf->takePage(cppArg0);
            {take_transfer}
            {take_operation}
            return pyResult;
        }}

        {public_function}
        """
    ).format(
        adapter_operation=adapter_operation,
        getter_operation=getter_operation,
        take_operation=take_operation,
        take_transfer=take_transfer,
        public_function=public_function,
    )


def split_view_source(
    public_method=None,
    adapter_operation="",
    getter_operation="",
    take_operation="",
    transfer_taken=True,
):
    public_function = ""
    if public_method is not None:
        public_function = textwrap.dedent(
            """
            static PyObject *
            Sbk_fluent_collections_SplitViewFunc_{method}(
                PyObject *self)
            {{
                return Py_None;
            }}
            """
        ).format(method=public_method)
    take_transfer = (
        "Shiboken::Object::getOwnership(pyResult);"
        if transfer_taken
        else ""
    )
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_collections_SplitViewFunc__addPaneWithOwnership(
            PyObject *self, PyObject *args)
        {{
            cppSelf->addPane(cppArg0, cppArg1, *cppArg2);
            {adapter_operation}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_collections_SplitViewFunc__insertPaneWithOwnership(
            PyObject *self, PyObject *args)
        {{
            cppSelf->insertPane(cppArg0, cppArg1, cppArg2, *cppArg3);
            {adapter_operation}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_collections_SplitViewFunc__releasePaneAtWithOwnership(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->releasePaneAt(cppArg0);
            {adapter_operation}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_collections_SplitViewFunc_paneAt(
            PyObject *self, PyObject *pyArg)
        {{
            pyResult = cppSelf->paneAt(cppArg0);
            {getter_operation}
            return pyResult;
        }}

        static PyObject *Sbk_fluent_collections_SplitViewFunc_takePaneAt(
            PyObject *self, PyObject *pyArg)
        {{
            pyResult = cppSelf->takePaneAt(cppArg0);
            {take_transfer}
            {take_operation}
            return pyResult;
        }}

        {public_function}
        """
    ).format(
        adapter_operation=adapter_operation,
        getter_operation=getter_operation,
        take_operation=take_operation,
        take_transfer=take_transfer,
        public_function=public_function,
    )


def stack_content_host_source(
    public_method=None,
    adapter_operation="",
    getter_operation="",
    take_operation="",
    transfer_taken=True,
):
    public_function = ""
    if public_method is not None:
        public_function = textwrap.dedent(
            """
            static PyObject *
            Sbk_fluent_navigation_StackContentHostFunc_{method}(
                PyObject *self)
            {{
                return Py_None;
            }}
            """
        ).format(method=public_method)
    take_transfer = (
        "Shiboken::Object::getOwnership(pyResult);"
        if transfer_taken
        else ""
    )
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_navigation_StackContentHostFunc__insertPageWithOwnership(
            PyObject *self, PyObject *args)
        {{
            cppSelf->insertPage(cppArg0, cppArg1, cppArg2);
            {adapter_operation}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_navigation_StackContentHostFunc__replacePageWithOwnership(
            PyObject *self, PyObject *args)
        {{
            cppSelf->replacePage(cppArg0, cppArg1, cppArg2);
            {adapter_operation}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_navigation_StackContentHostFunc__releasePageWithOwnership(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->releasePage(cppArg0);
            {adapter_operation}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_navigation_StackContentHostFunc__releaseAllPagesWithOwnership(
            PyObject *self)
        {{
            cppSelf->releaseAllPages();
            {adapter_operation}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_navigation_StackContentHostFunc_pageWidget(
            PyObject *self, PyObject *pyArg)
        {{
            pyResult = cppSelf->pageWidget(cppArg0);
            {getter_operation}
            return pyResult;
        }}

        static PyObject *Sbk_fluent_navigation_StackContentHostFunc_takePage(
            PyObject *self, PyObject *pyArg)
        {{
            pyResult = cppSelf->takePage(cppArg0);
            {take_transfer}
            {take_operation}
            return pyResult;
        }}

        {public_function}
        """
    ).format(
        adapter_operation=adapter_operation,
        getter_operation=getter_operation,
        take_transfer=take_transfer,
        take_operation=take_operation,
        public_function=public_function,
    )


def navigation_view_source(
    public_method=None,
    adapter_operation="",
    getter_operation="",
    take_operation="",
    transfer_taken=True,
):
    public_function = ""
    if public_method is not None:
        public_function = textwrap.dedent(
            """
            static PyObject *
            Sbk_fluent_navigation_NavigationViewFunc_{method}(
                PyObject *self)
            {{
                return Py_None;
            }}
            """
        ).format(method=public_method)
    take_transfer = (
        "Shiboken::Object::getOwnership(pyResult);"
        if transfer_taken
        else ""
    )
    adapters = []
    for name, native_call in (
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
        adapters.append(
            textwrap.dedent(
                """
                static PyObject *Sbk_fluent_navigation_NavigationViewFunc_{name}(
                    PyObject *self)
                {{
                    auto result = {native_call};
                    {adapter_operation}
                    return Py_None;
                }}
                """
            ).format(
                name=name,
                native_call=native_call,
                adapter_operation=adapter_operation,
            )
        )
    getters = []
    for name, native_call in (
        ("headerChromeWidget", "cppSelf->headerChromeWidget()"),
        ("mainChromeWidget", "cppSelf->mainChromeWidget()"),
        ("footerChromeWidget", "cppSelf->footerChromeWidget()"),
        ("contentHost", "cppSelf->contentHost()"),
    ):
        getters.append(
            textwrap.dedent(
                """
                static PyObject *Sbk_fluent_navigation_NavigationViewFunc_{name}(
                    PyObject *self)
                {{
                    pyResult = {native_call};
                    {getter_operation}
                    return pyResult;
                }}
                """
            ).format(
                name=name,
                native_call=native_call,
                getter_operation=getter_operation,
            )
        )
    takers = []
    for name in (
        "takeHeaderChromeWidget",
        "takeMainChromeWidget",
        "takeFooterChromeWidget",
    ):
        takers.append(
            textwrap.dedent(
                """
                static PyObject *Sbk_fluent_navigation_NavigationViewFunc_{name}(
                    PyObject *self)
                {{
                    pyResult = cppSelf->{name}();
                    {take_transfer}
                    {take_operation}
                    return pyResult;
                }}
                """
            ).format(
                name=name,
                take_transfer=take_transfer,
                take_operation=take_operation,
            )
        )
    return "\n".join(
        adapters + getters + takers + [public_function]
    )


def flow_view_source(
    retain_model=True,
    retain_item_delegate=True,
    public_method=None,
):
    model_retention = (
        'Shiboken::Object::keepReference(self, "model", pyArg);'
        if retain_model
        else ""
    )
    delegate_retention = (
        'Shiboken::Object::keepReference(self, "delegate", pyArg);'
        if retain_item_delegate
        else ""
    )
    public_function = ""
    if public_method is not None:
        public_function = textwrap.dedent(
            """
            static PyObject *
            Sbk_fluent_collections_FlowViewFunc_{method}(
                PyObject *self)
            {{
                return Py_None;
            }}
            """
        ).format(method=public_method)
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_collections_FlowViewFunc_setModel(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->setModel(cppArg0);
            {model_retention}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_collections_FlowViewFunc_setItemDelegate(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->setItemDelegate(cppArg0);
            {delegate_retention}
            return Py_None;
        }}

        {public_function}
        """
    ).format(
        model_retention=model_retention,
        delegate_retention=delegate_retention,
        public_function=public_function,
    )


def tree_view_source(
    retain_model=True,
    retain_selection_model=True,
    public_method=None,
):
    return list_view_source(
        retain_model=retain_model,
        retain_selection_model=retain_selection_model,
        public_method=public_method,
    ).replace("ListView", "TreeView")


def module_source(
    include_getter=True,
    include_setter=True,
    include_vertical_scroll_getter=True,
    include_horizontal_scroll_getter=True,
    scroll_getter_bookkeeping=None,
    include_flow_getter=True,
    include_flow_setter=True,
    include_flow_vertical_scroll_getter=True,
    flow_scroll_getter_bookkeeping=None,
    include_grid_getter=True,
    include_grid_setter=True,
    include_grid_vertical_scroll_getter=True,
    grid_scroll_getter_bookkeeping=None,
    include_tree_getter=True,
    include_tree_setter=True,
    include_tree_vertical_scroll_getter=True,
    include_tree_horizontal_scroll_getter=True,
    tree_scroll_getter_bookkeeping=None,
    include_breadcrumb_text_setter=True,
    include_breadcrumb_metadata_setter=True,
    breadcrumb_text_converter="QStringList",
    breadcrumb_metadata_converter=(
        "QList<fluent::navigation::BreadcrumbItem>"
    ),
):
    flow_getter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_flowViewSelectionMode(
                PyObject *self, PyObject *pyArg)
            {
                auto result = flowViewSelectionMode(cppArg0);
                return Py_None;
            }
            """
        )
        if include_flow_getter
        else ""
    )
    flow_setter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_setFlowViewSelectionMode(
                PyObject *self, PyObject *args)
            {
                setFlowViewSelectionMode(cppArg0, cppArg1);
                return Py_None;
            }
            """
        )
        if include_flow_setter
        else ""
    )
    flow_getter_operation = flow_scroll_getter_bookkeeping or ""
    flow_vertical_scroll_getter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_flowViewVerticalFluentScrollBar(
                PyObject *self, PyObject *pyArg)
            {{
                auto result = flowViewVerticalFluentScrollBar(cppArg0);
                {getter_operation}
                return Py_None;
            }}
            """
        ).format(getter_operation=flow_getter_operation)
        if include_flow_vertical_scroll_getter
        else ""
    )
    getter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_listViewSelectionMode(
                PyObject *self, PyObject *pyArg)
            {
                auto result = listViewSelectionMode(cppArg0);
                return Py_None;
            }
            """
        )
        if include_getter
        else ""
    )
    setter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_setListViewSelectionMode(
                PyObject *self, PyObject *args)
            {
                setListViewSelectionMode(cppArg0, cppArg1);
                return Py_None;
            }
            """
        )
        if include_setter
        else ""
    )
    getter_operation = scroll_getter_bookkeeping or ""
    vertical_scroll_getter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_listViewVerticalFluentScrollBar(
                PyObject *self, PyObject *pyArg)
            {{
                auto result = listViewVerticalFluentScrollBar(cppArg0);
                {getter_operation}
                return Py_None;
            }}
            """
        ).format(getter_operation=getter_operation)
        if include_vertical_scroll_getter
        else ""
    )
    horizontal_scroll_getter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_listViewHorizontalFluentScrollBar(
                PyObject *self, PyObject *pyArg)
            {{
                auto result = listViewHorizontalFluentScrollBar(cppArg0);
                {getter_operation}
                return Py_None;
            }}
            """
        ).format(getter_operation=getter_operation)
        if include_horizontal_scroll_getter
        else ""
    )
    grid_getter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_gridViewSelectionMode(
                PyObject *self, PyObject *pyArg)
            {
                auto result = gridViewSelectionMode(cppArg0);
                return Py_None;
            }
            """
        )
        if include_grid_getter
        else ""
    )
    grid_setter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_setGridViewSelectionMode(
                PyObject *self, PyObject *args)
            {
                setGridViewSelectionMode(cppArg0, cppArg1);
                return Py_None;
            }
            """
        )
        if include_grid_setter
        else ""
    )
    grid_getter_operation = grid_scroll_getter_bookkeeping or ""
    grid_vertical_scroll_getter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_gridViewVerticalFluentScrollBar(
                PyObject *self, PyObject *pyArg)
            {{
                auto result = gridViewVerticalFluentScrollBar(cppArg0);
                {getter_operation}
                return Py_None;
            }}
            """
        ).format(getter_operation=grid_getter_operation)
        if include_grid_vertical_scroll_getter
        else ""
    )
    tree_getter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_treeViewSelectionMode(
                PyObject *self, PyObject *pyArg)
            {
                auto result = treeViewSelectionMode(cppArg0);
                return Py_None;
            }
            """
        )
        if include_tree_getter
        else ""
    )
    tree_setter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_setTreeViewSelectionMode(
                PyObject *self, PyObject *args)
            {
                setTreeViewSelectionMode(cppArg0, cppArg1);
                return Py_None;
            }
            """
        )
        if include_tree_setter
        else ""
    )
    tree_getter_operation = tree_scroll_getter_bookkeeping or ""
    tree_vertical_scroll_getter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_treeViewVerticalFluentScrollBar(
                PyObject *self, PyObject *pyArg)
            {{
                auto result = treeViewVerticalFluentScrollBar(cppArg0);
                {getter_operation}
                return Py_None;
            }}
            """
        ).format(getter_operation=tree_getter_operation)
        if include_tree_vertical_scroll_getter
        else ""
    )
    tree_horizontal_scroll_getter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_treeViewHorizontalFluentScrollBar(
                PyObject *self, PyObject *pyArg)
            {{
                auto result = treeViewHorizontalFluentScrollBar(cppArg0);
                {getter_operation}
                return Py_None;
            }}
            """
        ).format(getter_operation=tree_getter_operation)
        if include_tree_horizontal_scroll_getter
        else ""
    )
    breadcrumb_text_setter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_setBreadcrumbTextItems(
                PyObject *self, PyObject *args)
            {
                BREADCRUMB_TEXT_LIST cppArg1;
                setBreadcrumbTextItems(cppArg0, cppArg1);
                return Py_None;
            }
            """
        ).replace("BREADCRUMB_TEXT_LIST", breadcrumb_text_converter)
        if include_breadcrumb_text_setter
        else ""
    )
    breadcrumb_metadata_setter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_setBreadcrumbMetadataItems(
                PyObject *self, PyObject *args)
            {
                BREADCRUMB_METADATA_LIST cppArg1;
                setBreadcrumbMetadataItems(cppArg0, cppArg1);
                return Py_None;
            }
            """
        ).replace(
            "BREADCRUMB_METADATA_LIST",
            breadcrumb_metadata_converter,
        )
        if include_breadcrumb_metadata_setter
        else ""
    )
    return (
        flow_getter
        + flow_setter
        + flow_vertical_scroll_getter
        + grid_getter
        + grid_setter
        + grid_vertical_scroll_getter
        + tree_getter
        + tree_setter
        + tree_vertical_scroll_getter
        + tree_horizontal_scroll_getter
        + getter
        + setter
        + vertical_scroll_getter
        + horizontal_scroll_getter
        + breadcrumb_text_setter
        + breadcrumb_metadata_setter
    )


def fluent_namespace_source(
    selection_converter_count=1,
    unstable_collections_converter=False,
):
    converter = textwrap.dedent(
        """
        static void generatedSelectionModeConverter(PyObject *, void *) {}
        void registerSelectionModeConverter()
        {
            Shiboken::Conversions::registerConverterName(
                converter, "fluent::binding::SelectionMode");
        }
        """
    )
    source = converter * selection_converter_count
    if unstable_collections_converter:
        source += (
            'registerConverterName(converter, '
            '"fluent::collections::SelectionMode");\n'
        )
    return source


def expander_source(include_header_button=False, **kwargs):
    source = scroll_view_source(**kwargs)
    source = source.replace(
        "fluent_scrolling_ScrollView",
        "fluent_layout_Expander",
    ).replace("ScrollView::", "Expander::")
    if include_header_button:
        source += textwrap.dedent(
            """
            static PyObject *Sbk_fluent_layout_ExpanderFunc_headerButton(
                PyObject *self)
            {
                return Py_None;
            }
            """
        )
    return source


def stack_view_source(
    public_bypass=None,
    adapter_bookkeeping=None,
    getter_bookkeeping=None,
):
    public_function = ""
    if public_bypass is not None:
        public_function = textwrap.dedent(
            """
            static PyObject *Sbk_fluent_collections_StackViewFunc_{0}(
                PyObject *self)
            {{
                return Py_None;
            }}
            """
        ).format(public_bypass)

    adapter_name = None
    adapter_operation = ""
    if adapter_bookkeeping is not None:
        adapter_name, adapter_operation = adapter_bookkeeping
    getter_name = None
    getter_operation = ""
    if getter_bookkeeping is not None:
        getter_name, getter_operation = getter_bookkeeping

    adapters = []
    for name, native_call in (
        ("_pushItemWithOwnership", "cppSelf->push(cppArg0, cppArg1)"),
        ("_pushItemsWithOwnership", "cppSelf->push(cppArg0, cppArg1)"),
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
    ):
        operation = adapter_operation if adapter_name == name else ""
        adapters.append(
            textwrap.dedent(
                """
                static PyObject *Sbk_fluent_collections_StackViewFunc_{name}(
                    PyObject *self)
                {{
                    auto result = {native_call};
                    {operation}
                    return PyBool_FromLong(result);
                }}
                """
            ).format(
                name=name,
                native_call=native_call,
                operation=operation,
            )
        )

    getters = []
    for name, native_call in (
        ("currentItem", "cppSelf->currentItem()"),
        ("initialItem", "cppSelf->initialItem()"),
        ("itemAt", "cppSelf->itemAt(cppArg0)"),
    ):
        operation = getter_operation if getter_name == name else ""
        getters.append(
            textwrap.dedent(
                """
                static PyObject *Sbk_fluent_collections_StackViewFunc_{name}(
                    PyObject *self)
                {{
                    pyResult = {native_call};
                    {operation}
                    return pyResult;
                }}
                """
            ).format(
                name=name,
                native_call=native_call,
                operation=operation,
            )
        )
    return "\n".join(adapters + getters + [public_function])


def tab_view_source(
    include_item_overload=True,
    include_value_conversion=True,
):
    item_overload = (
        "cppSelf->addTab(*cppArg0);"
        if include_item_overload
        else ""
    )
    value_conversion = (
        "copyToPython(TabViewItem, &result);"
        if include_value_conversion
        else ""
    )
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_navigation_TabViewFunc_addTab(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->addTab(cppArg0);
            {item_overload}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_navigation_TabViewFunc_tabAt(
            PyObject *self, PyObject *pyArg)
        {{
            auto result = const_cast<const TabViewWrapper *>(
                cppSelf)->tabAt(cppArg0);
            {value_conversion}
            return Py_None;
        }}
        """
    ).format(
        item_overload=item_overload,
        value_conversion=value_conversion,
    )


def tab_view_item_source(
    missing_field=None,
    include_variant_conversion=True,
):
    accessors = []
    for field_name in (
        "text",
        "iconGlyph",
        "closable",
        "enabled",
        "data",
        "accessibleName",
    ):
        if field_name == missing_field:
            continue
        accessors.append(
            textwrap.dedent(
                """
                static PyObject *
                Sbk_fluent_navigation_TabViewItem_get_{field_name}(
                    PyObject *self, void *)
                {{
                    {getter_conversion}
                    return Py_None;
                }}

                static int
                Sbk_fluent_navigation_TabViewItem_set_{field_name}(
                    PyObject *self, PyObject *value, void *)
                {{
                    {setter_conversion}
                    return 0;
                }}
                """
            ).format(
                field_name=field_name,
                getter_conversion=(
                    "copyToPython(converter, &cppSelf->data);"
                    if field_name == "data" and include_variant_conversion
                    else ""
                ),
                setter_conversion=(
                    "pythonToCpp(value, &cppSelf->data);"
                    if field_name == "data" and include_variant_conversion
                    else ""
                ),
            )
        )
    return "\n".join(accessors)


def breadcrumb_source(
    include_item_overload=True,
    include_value_conversion=True,
    include_public_setter=False,
):
    item_overload = (
        "cppSelf->appendItem(*cppArg0);"
        if include_item_overload
        else ""
    )
    value_conversion = (
        "copyToPython(BreadcrumbItem, &result);"
        if include_value_conversion
        else ""
    )
    public_setter = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluent_navigation_BreadcrumbFunc_setItems(
                PyObject *self, PyObject *pyArg)
            {
                cppSelf->setItems(cppArg0);
                return Py_None;
            }
            """
        )
        if include_public_setter
        else ""
    )
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_navigation_BreadcrumbFunc_appendItem(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->appendItem(cppArg0);
            {item_overload}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_navigation_BreadcrumbFunc_itemAt(
            PyObject *self, PyObject *pyArg)
        {{
            auto result = cppSelf->itemAt(cppArg0);
            {value_conversion}
            return Py_None;
        }}

        {public_setter}
        """
    ).format(
        item_overload=item_overload,
        value_conversion=value_conversion,
        public_setter=public_setter,
    )


def breadcrumb_item_source(
    missing_field=None,
    include_variant_conversion=True,
):
    accessors = []
    for field_name in ("text", "data", "enabled", "accessibleName"):
        if field_name == missing_field:
            continue
        accessors.append(
            textwrap.dedent(
                """
                static PyObject *
                Sbk_fluent_navigation_BreadcrumbItem_get_{field_name}(
                    PyObject *self, void *)
                {{
                    {getter_conversion}
                    return Py_None;
                }}

                static int
                Sbk_fluent_navigation_BreadcrumbItem_set_{field_name}(
                    PyObject *self, PyObject *value, void *)
                {{
                    {setter_conversion}
                    return 0;
                }}
                """
            ).format(
                field_name=field_name,
                getter_conversion=(
                    "copyToPython(converter, &cppSelf->data);"
                    if field_name == "data" and include_variant_conversion
                    else ""
                ),
                setter_conversion=(
                    "pythonToCpp(value, &cppSelf->data);"
                    if field_name == "data" and include_variant_conversion
                    else ""
                ),
            )
        )
    return "\n".join(accessors)


def navigation_metadata_widget_source(
    widget_name,
    item_name,
    include_add_item_overload=True,
    include_insert_item_overload=True,
    include_item_conversion=True,
    include_items_conversion=True,
):
    add_item_overload = (
        "cppSelf->addItem(*cppArg0);"
        if include_add_item_overload
        else ""
    )
    insert_item_overload = (
        "cppSelf->insertItem(cppArg0, *cppArg1);"
        if include_insert_item_overload
        else ""
    )
    item_conversion = (
        "copyToPython({0}, &result);".format(item_name)
        if include_item_conversion
        else ""
    )
    items_conversion = (
        "copyToPython(QList<{0}>, &result);".format(item_name)
        if include_items_conversion
        else ""
    )
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_navigation_{widget_name}Func_addItem(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->addItem(cppArg0);
            {add_item_overload}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_navigation_{widget_name}Func_insertItem(
            PyObject *self, PyObject *args)
        {{
            cppSelf->insertItem(cppArg0, cppArg1);
            {insert_item_overload}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_navigation_{widget_name}Func_itemAt(
            PyObject *self, PyObject *pyArg)
        {{
            auto result = cppSelf->itemAt(cppArg0);
            {item_conversion}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_navigation_{widget_name}Func_items(
            PyObject *self)
        {{
            auto result = cppSelf->items();
            {items_conversion}
            return Py_None;
        }}
        """
    ).format(
        widget_name=widget_name,
        add_item_overload=add_item_overload,
        insert_item_overload=insert_item_overload,
        item_conversion=item_conversion,
        items_conversion=items_conversion,
    )


def navigation_metadata_item_source(
    item_name,
    fields,
    missing_field=None,
    include_variant_conversion=True,
):
    accessors = []
    for field_name in fields:
        if field_name == missing_field:
            continue
        accessors.append(
            textwrap.dedent(
                """
                static PyObject *
                Sbk_fluent_navigation_{item_name}_get_{field_name}(
                    PyObject *self, void *)
                {{
                    {getter_conversion}
                    return Py_None;
                }}

                static int
                Sbk_fluent_navigation_{item_name}_set_{field_name}(
                    PyObject *self, PyObject *value, void *)
                {{
                    {setter_conversion}
                    return 0;
                }}
                """
            ).format(
                item_name=item_name,
                field_name=field_name,
                getter_conversion=(
                    "copyToPython(converter, &cppSelf->data);"
                    if field_name == "data" and include_variant_conversion
                    else ""
                ),
                setter_conversion=(
                    "pythonToCpp(value, &cppSelf->data);"
                    if field_name == "data" and include_variant_conversion
                    else ""
                ),
            )
        )
    return "\n".join(accessors)


def pips_pager_source(internal_property=None):
    source = "// Generated PipsPager public wrapper\n"
    if internal_property is not None:
        source += '"{0}::"\n'.format(internal_property)
    return source


def annotated_scroll_bar_source(
    link_bookkeeping=None,
    getter_bookkeeping=None,
    provider_api=None,
):
    link_operation = link_bookkeeping or ""
    getter_operation = getter_bookkeeping or ""
    provider = ""
    if provider_api is not None:
        provider = (
            "static PyObject *"
            "Sbk_fluent_scrolling_AnnotatedScrollBarFunc_{0}("
            "PyObject *self) {{ return Py_None; }}"
        ).format(provider_api)
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_scrolling_AnnotatedScrollBarFunc_connectToScrollView(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->connectToScrollView(cppArg0);
            {link_operation}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_scrolling_AnnotatedScrollBarFunc_connectedScrollView(
            PyObject *self)
        {{
            pyResult = cppSelf->connectedScrollView();
            {getter_operation}
            return pyResult;
        }}

        {provider}
        """
    ).format(
        link_operation=link_operation,
        getter_operation=getter_operation,
        provider=provider,
    )


def accordion_source(
    public_overload=None,
    add_bookkeeping=None,
    insert_bookkeeping=None,
    getter_bookkeeping=None,
    take_bookkeeping=None,
    transfer_taken_to_python=True,
):
    public_function = ""
    if public_overload is not None:
        public_function = textwrap.dedent(
            """
            static PyObject *Sbk_fluent_layout_AccordionFunc_{0}(
                PyObject *self)
            {{
                return Py_None;
            }}
            """
        ).format(public_overload)
    ownership_transfer = (
        "Shiboken::Object::getOwnership(pyResult);"
        if transfer_taken_to_python
        else ""
    )
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_layout_AccordionFunc__addItemWithOwnership(
            PyObject *self, PyObject *args)
        {{
            auto result = cppSelf->addItem(cppArg0, cppArg1);
            {add_bookkeeping}
            return PyBool_FromLong(result);
        }}

        static PyObject *Sbk_fluent_layout_AccordionFunc__insertItemWithOwnership(
            PyObject *self, PyObject *args)
        {{
            auto result = cppSelf->insertItem(cppArg0, cppArg1, cppArg2);
            {insert_bookkeeping}
            return PyBool_FromLong(result);
        }}

        static PyObject *Sbk_fluent_layout_AccordionFunc_itemAt(
            PyObject *self, PyObject *pyArg)
        {{
            pyResult = cppSelf->itemAt(cppArg0);
            {getter_bookkeeping}
            return pyResult;
        }}

        static PyObject *Sbk_fluent_layout_AccordionFunc_takeItem(
            PyObject *self, PyObject *pyArg)
        {{
            pyResult = cppSelf->takeItem(cppArg0);
            {take_bookkeeping}
            {ownership_transfer}
            return pyResult;
        }}

        {public_function}
        """
    ).format(
        add_bookkeeping=add_bookkeeping or "",
        insert_bookkeeping=insert_bookkeeping or "",
        getter_bookkeeping=getter_bookkeeping or "",
        take_bookkeeping=take_bookkeeping or "",
        ownership_transfer=ownership_transfer,
        public_function=public_function,
    )


def info_bar_source(
    setter_parent=False,
    setter_keep_reference=False,
    setter_transfer_to_cpp=False,
    setter_transfer_to_python=False,
    getter_parent=False,
    getter_transfer_to_cpp=False,
):
    setter_operations = []
    if setter_parent:
        setter_operations.append(
            "Shiboken::Object::setParent(self, pyArg);"
        )
    if setter_keep_reference:
        setter_operations.append(
            "Shiboken::Object::keepReference("
            "reinterpret_cast<SbkObject *>(self), "
            '"__fluentqt_infobar_action", pyArg);'
        )
    if setter_transfer_to_cpp:
        setter_operations.append(
            "Shiboken::Object::releaseOwnership(pyArg);"
        )
    if setter_transfer_to_python:
        setter_operations.append(
            "Shiboken::Object::getOwnership(pyArg);"
        )

    getter_operation = ""
    if getter_parent:
        getter_operation = (
            "Shiboken::Object::setParent(self, pyResult);"
        )
    elif getter_transfer_to_cpp:
        getter_operation = (
            "Shiboken::Object::releaseOwnership(pyResult);"
        )

    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_status_info_InfoBarFunc__setActionWidget(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->setActionWidget(cppArg0);
            {setter_operations}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_status_info_InfoBarFunc_actionWidget(
            PyObject *self)
        {{
            pyResult = cppSelf->actionWidget();
            {getter_operation}
            return pyResult;
        }}
        """
    ).format(
        setter_operations="\n".join(setter_operations),
        getter_operation=getter_operation,
    )


class GeneratedContractVerifierTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.generated_dir = Path(self.temporary_directory.name)
        self.write_window()
        self.write_namespace(1)
        self.write_module()
        self.write_fluent_namespace()
        self.write_scroll_view()
        self.write_flip_view()
        self.write_split_view()
        self.write_stack_content_host()
        self.write_navigation_view()
        self.write_flow_view()
        self.write_grid_view()
        self.write_list_view()
        self.write_tree_view()
        self.write_stack_view()
        self.write_breadcrumb()
        self.write_breadcrumb_item()
        self.write_pivot()
        self.write_pivot_item()
        self.write_selector_bar()
        self.write_selector_bar_item()
        self.write_tab_view()
        self.write_tab_view_item()
        self.write_annotated_scroll_bar()
        self.write_accordion()
        self.write_expander()
        self.write_info_bar()
        self.write_pips_pager()

    def tearDown(self):
        self.temporary_directory.cleanup()

    def write_window(
        self,
        limited_api_arguments=True,
        expose_result_pointer=False,
        split_override=True,
    ):
        (self.generated_dir / WINDOW_WRAPPER).write_text(
            window_source(
                limited_api_arguments=limited_api_arguments,
                expose_result_pointer=expose_result_pointer,
                split_override=split_override,
            ),
            encoding="utf-8",
        )

    def write_namespace(self, converter_count):
        (self.generated_dir / WINDOWING_NAMESPACE_WRAPPER).write_text(
            "\n".join(ENUM_CONVERTER for _ in range(converter_count)),
            encoding="utf-8",
        )

    def write_scroll_view(
        self,
        release_old_child=False,
        runtime_overload=False,
        use_parent=False,
        keep_reference=False,
        adapter_use_parent=False,
        adapter_keep_reference=False,
        adapter_transfer_to_cpp=False,
        adapter_transfer_to_python=False,
        take_keep_reference=False,
        transfer_to_cpp=False,
        transfer_to_python=True,
        getter_parent=False,
        getter_transfer_to_cpp=False,
        take_parent=False,
    ):
        (self.generated_dir / SCROLL_VIEW_WRAPPER).write_text(
            scroll_view_source(
                release_old_child=release_old_child,
                runtime_overload=runtime_overload,
                use_parent=use_parent,
                keep_reference=keep_reference,
                adapter_use_parent=adapter_use_parent,
                adapter_keep_reference=adapter_keep_reference,
                adapter_transfer_to_cpp=adapter_transfer_to_cpp,
                adapter_transfer_to_python=adapter_transfer_to_python,
                take_keep_reference=take_keep_reference,
                transfer_to_cpp=transfer_to_cpp,
                transfer_to_python=transfer_to_python,
                getter_parent=getter_parent,
                getter_transfer_to_cpp=getter_transfer_to_cpp,
                take_parent=take_parent,
            ),
            encoding="utf-8",
        )

    def write_list_view(self, **kwargs):
        (self.generated_dir / LIST_VIEW_WRAPPER).write_text(
            list_view_source(**kwargs),
            encoding="utf-8",
        )

    def write_flip_view(self, **kwargs):
        (self.generated_dir / FLIP_VIEW_WRAPPER).write_text(
            flip_view_source(**kwargs),
            encoding="utf-8",
        )

    def write_split_view(self, **kwargs):
        (self.generated_dir / SPLIT_VIEW_WRAPPER).write_text(
            split_view_source(**kwargs),
            encoding="utf-8",
        )

    def write_stack_content_host(self, **kwargs):
        (self.generated_dir / STACK_CONTENT_HOST_WRAPPER).write_text(
            stack_content_host_source(**kwargs),
            encoding="utf-8",
        )

    def write_navigation_view(self, **kwargs):
        (self.generated_dir / NAVIGATION_VIEW_WRAPPER).write_text(
            navigation_view_source(**kwargs),
            encoding="utf-8",
        )

    def write_flow_view(self, **kwargs):
        (self.generated_dir / FLOW_VIEW_WRAPPER).write_text(
            flow_view_source(**kwargs),
            encoding="utf-8",
        )

    def write_grid_view(self, **kwargs):
        (self.generated_dir / GRID_VIEW_WRAPPER).write_text(
            grid_view_source(**kwargs),
            encoding="utf-8",
        )

    def write_tree_view(self, **kwargs):
        (self.generated_dir / TREE_VIEW_WRAPPER).write_text(
            tree_view_source(**kwargs),
            encoding="utf-8",
        )

    def write_module(self, **kwargs):
        (self.generated_dir / MODULE_WRAPPER).write_text(
            module_source(**kwargs),
            encoding="utf-8",
        )

    def write_fluent_namespace(
        self,
        selection_converter_count=1,
        unstable_collections_converter=False,
    ):
        (self.generated_dir / FLUENT_NAMESPACE_WRAPPER).write_text(
            fluent_namespace_source(
                selection_converter_count,
                unstable_collections_converter,
            ),
            encoding="utf-8",
        )

    def write_expander(self, include_header_button=False, **kwargs):
        (self.generated_dir / EXPANDER_WRAPPER).write_text(
            expander_source(
                include_header_button=include_header_button,
                **kwargs,
            ),
            encoding="utf-8",
        )

    def write_stack_view(self, **kwargs):
        (self.generated_dir / STACK_VIEW_WRAPPER).write_text(
            stack_view_source(**kwargs),
            encoding="utf-8",
        )

    def write_breadcrumb(self, **kwargs):
        (self.generated_dir / BREADCRUMB_WRAPPER).write_text(
            breadcrumb_source(**kwargs),
            encoding="utf-8",
        )

    def write_breadcrumb_item(self, **kwargs):
        (self.generated_dir / BREADCRUMB_ITEM_WRAPPER).write_text(
            breadcrumb_item_source(**kwargs),
            encoding="utf-8",
        )

    def write_pivot(self, **kwargs):
        (self.generated_dir / PIVOT_WRAPPER).write_text(
            navigation_metadata_widget_source(
                "Pivot",
                "PivotItem",
                **kwargs,
            ),
            encoding="utf-8",
        )

    def write_pivot_item(self, **kwargs):
        (self.generated_dir / PIVOT_ITEM_WRAPPER).write_text(
            navigation_metadata_item_source(
                "PivotItem",
                ("header", "iconGlyph", "enabled", "data", "accessibleName"),
                **kwargs,
            ),
            encoding="utf-8",
        )

    def write_selector_bar(self, **kwargs):
        (self.generated_dir / SELECTOR_BAR_WRAPPER).write_text(
            navigation_metadata_widget_source(
                "SelectorBar",
                "SelectorBarItem",
                **kwargs,
            ),
            encoding="utf-8",
        )

    def write_selector_bar_item(self, **kwargs):
        (self.generated_dir / SELECTOR_BAR_ITEM_WRAPPER).write_text(
            navigation_metadata_item_source(
                "SelectorBarItem",
                (
                    "text",
                    "iconGlyph",
                    "enabled",
                    "visible",
                    "selected",
                    "data",
                    "accessibleName",
                ),
                **kwargs,
            ),
            encoding="utf-8",
        )

    def write_tab_view(self, **kwargs):
        (self.generated_dir / TAB_VIEW_WRAPPER).write_text(
            tab_view_source(**kwargs),
            encoding="utf-8",
        )

    def write_tab_view_item(self, **kwargs):
        (self.generated_dir / TAB_VIEW_ITEM_WRAPPER).write_text(
            tab_view_item_source(**kwargs),
            encoding="utf-8",
        )

    def write_accordion(self, **kwargs):
        (self.generated_dir / ACCORDION_WRAPPER).write_text(
            accordion_source(**kwargs),
            encoding="utf-8",
        )

    def write_annotated_scroll_bar(self, **kwargs):
        (self.generated_dir / ANNOTATED_SCROLL_BAR_WRAPPER).write_text(
            annotated_scroll_bar_source(**kwargs),
            encoding="utf-8",
        )

    def write_pips_pager(self, internal_property=None):
        (self.generated_dir / PIPS_PAGER_WRAPPER).write_text(
            pips_pager_source(internal_property),
            encoding="utf-8",
        )

    def write_info_bar(self, **kwargs):
        (self.generated_dir / INFO_BAR_WRAPPER).write_text(
            info_bar_source(**kwargs),
            encoding="utf-8",
        )

    def run_verifier(self, *extra_arguments):
        return subprocess.run(
            [
                sys.executable,
                str(VERIFIER),
                "--generated-dir",
                str(self.generated_dir),
                *extra_arguments,
            ],
            check=False,
            capture_output=True,
            text=True,
        )

    def test_safe_native_event_and_single_converter_pass(self):
        result = self.run_verifier("--check-backdrop-converter")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_shiboken_62_inline_native_event_passes(self):
        self.write_window(split_override=False)
        result = self.run_verifier()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_event_filter_decoy_does_not_satisfy_native_event(self):
        self.write_window(limited_api_arguments=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("limited-API override is missing", result.stderr)

    def test_shiboken_62_event_filter_decoy_does_not_satisfy_native_event(self):
        self.write_window(
            limited_api_arguments=False,
            split_override=False,
        )
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("limited-API override is missing", result.stderr)

    def test_result_pointer_exposure_is_rejected(self):
        self.write_window(expose_result_pointer=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("exposed its result pointer", result.stderr)

    def test_shiboken_62_inline_result_pointer_exposure_is_rejected(self):
        self.write_window(
            expose_result_pointer=True,
            split_override=False,
        )
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("exposed its result pointer", result.stderr)

    def test_duplicate_converter_is_rejected(self):
        self.write_namespace(2)
        result = self.run_verifier("--check-backdrop-converter")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("found 2", result.stderr)

    def test_flip_view_runtime_ownership_bypasses_are_rejected(self):
        for method_name in (
            "addPage",
            "insertPage",
            "removePage",
            "releasePage",
        ):
            with self.subTest(method_name=method_name):
                self.write_flip_view(public_method=method_name)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("page ownership bypass", result.stderr)

    def test_flip_view_adapter_wrapper_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyArg);",
            "Shiboken::Object::getOwnership(pyArg);",
            "Shiboken::Object::keepReference(self, \"page\", pyArg);",
            "Shiboken::Object::setParent(self, pyArg);",
        ):
            with self.subTest(operation=operation):
                self.write_flip_view(adapter_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("FlipView adapter", result.stderr)

    def test_flip_view_page_getter_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
            "Shiboken::Object::keepReference(self, \"page\", pyResult);",
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_flip_view(getter_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("FlipView::pageAt", result.stderr)

    def test_flip_view_take_requires_python_ownership(self):
        self.write_flip_view(transfer_taken=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Python ownership transfer is missing", result.stderr)

    def test_flip_view_take_wrapper_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::keepReference(self, \"page\", pyResult);",
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_flip_view(take_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("FlipView::takePage", result.stderr)

    def test_split_view_runtime_ownership_bypasses_are_rejected(self):
        for method_name in (
            "addPane",
            "insertPane",
            "removePane",
            "removePaneAt",
            "releasePane",
            "releasePaneAt",
        ):
            with self.subTest(method_name=method_name):
                self.write_split_view(public_method=method_name)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("pane ownership bypass", result.stderr)

    def test_split_view_adapter_wrapper_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyArg);",
            "Shiboken::Object::getOwnership(pyArg);",
            "Shiboken::Object::keepReference(self, \"pane\", pyArg);",
            "Shiboken::Object::setParent(self, pyArg);",
        ):
            with self.subTest(operation=operation):
                self.write_split_view(adapter_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("SplitView adapter", result.stderr)

    def test_split_view_pane_getter_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
            "Shiboken::Object::keepReference(self, \"pane\", pyResult);",
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_split_view(getter_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("SplitView::paneAt", result.stderr)

    def test_split_view_take_requires_python_ownership(self):
        self.write_split_view(transfer_taken=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Python ownership transfer is missing", result.stderr)

    def test_split_view_take_wrapper_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::keepReference(self, \"pane\", pyResult);",
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_split_view(take_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("SplitView::takePaneAt", result.stderr)

    def test_stack_content_host_public_ownership_bypasses_are_rejected(self):
        for method_name in (
            "insertPage",
            "replacePage",
            "releasePage",
            "releaseAllPages",
            "clearPages",
        ):
            with self.subTest(method_name=method_name):
                self.write_stack_content_host(public_method=method_name)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("page ownership bypass", result.stderr)

    def test_stack_content_host_adapter_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyArg);",
            "Shiboken::Object::getOwnership(pyArg);",
            'Shiboken::Object::keepReference(self, "page", pyArg);',
            "Shiboken::Object::setParent(self, pyArg);",
        ):
            with self.subTest(operation=operation):
                self.write_stack_content_host(
                    adapter_operation=operation
                )
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("StackContentHost adapter", result.stderr)

    def test_stack_content_host_getter_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
            'Shiboken::Object::keepReference(self, "page", pyResult);',
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_stack_content_host(
                    getter_operation=operation
                )
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("StackContentHost::pageWidget", result.stderr)

    def test_stack_content_host_take_contract_is_enforced(self):
        self.write_stack_content_host(transfer_taken=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Python ownership transfer is missing", result.stderr)

        for operation in (
            'Shiboken::Object::keepReference(self, "page", pyResult);',
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_stack_content_host(take_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("StackContentHost::takePage", result.stderr)

    def test_navigation_view_public_ownership_bypasses_are_rejected(self):
        for method_name in (
            "setHeaderChromeWidget",
            "setMainChromeWidget",
            "setFooterChromeWidget",
            "releaseHeaderChromeWidget",
            "releaseMainChromeWidget",
            "releaseFooterChromeWidget",
        ):
            with self.subTest(method_name=method_name):
                self.write_navigation_view(public_method=method_name)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("chrome ownership bypass", result.stderr)

    def test_navigation_view_adapter_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyArg);",
            "Shiboken::Object::getOwnership(pyArg);",
            'Shiboken::Object::keepReference(self, "chrome", pyArg);',
            "Shiboken::Object::setParent(self, pyArg);",
        ):
            with self.subTest(operation=operation):
                self.write_navigation_view(adapter_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("NavigationView adapter", result.stderr)

    def test_navigation_view_getter_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
            'Shiboken::Object::keepReference(self, "chrome", pyResult);',
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_navigation_view(getter_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("NavigationView::", result.stderr)

    def test_navigation_view_take_contract_is_enforced(self):
        self.write_navigation_view(transfer_taken=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Python ownership transfer is missing", result.stderr)

        for operation in (
            'Shiboken::Object::keepReference(self, "chrome", pyResult);',
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_navigation_view(take_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("NavigationView::take", result.stderr)

    def test_scroll_view_old_child_release_is_rejected(self):
        self.write_scroll_view(release_old_child=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("releases the old owned child", result.stderr)

    def test_scroll_view_runtime_ownership_overload_is_rejected(self):
        self.write_scroll_view(runtime_overload=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("runtime ownership overload", result.stderr)

    def test_scroll_view_parent_bookkeeping_is_rejected(self):
        self.write_scroll_view(use_parent=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("parent bookkeeping", result.stderr)

    def test_scroll_view_setter_keep_reference_is_rejected(self):
        self.write_scroll_view(keep_reference=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("keep-reference bookkeeping", result.stderr)

    def test_scroll_view_adapter_parent_bookkeeping_is_rejected(self):
        self.write_scroll_view(adapter_use_parent=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("private ownership adapter uses Shiboken parent", result.stderr)

    def test_scroll_view_adapter_keep_reference_is_rejected(self):
        self.write_scroll_view(adapter_keep_reference=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "private ownership adapter uses Shiboken keep-reference",
            result.stderr,
        )

    def test_scroll_view_adapter_cpp_ownership_is_rejected(self):
        self.write_scroll_view(adapter_transfer_to_cpp=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "private ownership adapter changes Python wrapper ownership",
            result.stderr,
        )

    def test_scroll_view_adapter_python_ownership_is_rejected(self):
        self.write_scroll_view(adapter_transfer_to_python=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "private ownership adapter changes Python wrapper ownership",
            result.stderr,
        )

    def test_scroll_view_taker_keep_reference_is_rejected(self):
        self.write_scroll_view(take_keep_reference=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("keep-reference bookkeeping", result.stderr)

    def test_scroll_view_cpp_wrapper_ownership_is_rejected(self):
        self.write_scroll_view(transfer_to_cpp=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("transfers wrapper ownership to C++", result.stderr)

    def test_scroll_view_missing_python_ownership_is_rejected(self):
        self.write_scroll_view(transfer_to_python=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Python ownership transfer is missing", result.stderr)

    def test_scroll_view_getter_parent_heuristic_is_rejected(self):
        self.write_scroll_view(getter_parent=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("return-value parent heuristic", result.stderr)

    def test_scroll_view_getter_cpp_wrapper_ownership_is_rejected(self):
        self.write_scroll_view(getter_transfer_to_cpp=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("changes Python wrapper ownership", result.stderr)

    def test_scroll_view_take_parent_bookkeeping_is_rejected(self):
        self.write_scroll_view(take_parent=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("parent bookkeeping", result.stderr)

    def test_list_view_model_retention_is_required(self):
        self.write_list_view(retain_model=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "setModel caller-owned retention is missing",
            result.stderr,
        )

    def test_flow_view_model_retention_is_required(self):
        self.write_flow_view(retain_model=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "FlowView::setModel caller-owned retention is missing",
            result.stderr,
        )

    def test_flow_view_delegate_retention_is_required(self):
        self.write_flow_view(retain_item_delegate=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "FlowView::setItemDelegate caller-owned retention is missing",
            result.stderr,
        )

    def test_flow_view_unsupported_native_surface_is_rejected(self):
        for method in (
            "selectionMode",
            "setSelectionMode",
            "verticalFluentScrollBar",
        ):
            with self.subTest(method=method):
                self.write_flow_view(public_method=method)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "FlowView exposes unsupported native API {0}".format(
                        method
                    ),
                    result.stderr,
                )

    def test_flow_view_internal_scrollbar_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
            'Shiboken::Object::keepReference(self, "bar", pyResult);',
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_module(
                    flow_scroll_getter_bookkeeping=operation
                )
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "FlowView scrollbar adapter "
                    "flowViewVerticalFluentScrollBar",
                    result.stderr,
                )

    def test_flow_view_adapters_are_required(self):
        for adapter in (
            "include_flow_getter",
            "include_flow_setter",
            "include_flow_vertical_scroll_getter",
        ):
            with self.subTest(adapter=adapter):
                self.write_module(**{adapter: False})
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("FlowView", result.stderr)

    def test_list_view_selection_model_retention_is_required(self):
        self.write_list_view(retain_selection_model=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "setSelectionModel caller-owned retention is missing",
            result.stderr,
        )

    def test_grid_view_unsupported_native_surface_is_rejected(self):
        for method in (
            "selectionMode",
            "setSelectionMode",
            "verticalFluentScrollBar",
        ):
            with self.subTest(method=method):
                self.write_grid_view(public_method=method)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "GridView exposes unsupported native API {0}".format(
                        method
                    ),
                    result.stderr,
                )

    def test_grid_view_internal_scrollbar_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
            'Shiboken::Object::keepReference(self, "bar", pyResult);',
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_module(
                    grid_scroll_getter_bookkeeping=operation
                )
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "GridView scrollbar adapter "
                    "gridViewVerticalFluentScrollBar",
                    result.stderr,
                )

    def test_grid_view_adapters_are_required(self):
        for adapter in (
            "include_grid_getter",
            "include_grid_setter",
            "include_grid_vertical_scroll_getter",
        ):
            with self.subTest(adapter=adapter):
                self.write_module(**{adapter: False})
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("GridView", result.stderr)

    def test_tree_view_model_retention_is_required(self):
        self.write_tree_view(retain_model=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "TreeView::setModel caller-owned retention is missing",
            result.stderr,
        )

    def test_tree_view_selection_model_retention_is_required(self):
        self.write_tree_view(retain_selection_model=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "TreeView::setSelectionModel caller-owned retention is missing",
            result.stderr,
        )

    def test_tree_view_unsupported_native_surface_is_rejected(self):
        for method in (
            "selectionMode",
            "setSelectionMode",
            "selectionIndicatorStyle",
            "setSelectionIndicatorStyle",
            "verticalFluentScrollBar",
            "horizontalFluentScrollBar",
        ):
            with self.subTest(method=method):
                self.write_tree_view(public_method=method)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "TreeView exposes unsupported native API {0}".format(
                        method
                    ),
                    result.stderr,
                )

    def test_tree_view_internal_scrollbar_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
            'Shiboken::Object::keepReference(self, "bar", pyResult);',
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_module(
                    tree_scroll_getter_bookkeeping=operation
                )
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "TreeView scrollbar adapter "
                    "treeViewVerticalFluentScrollBar",
                    result.stderr,
                )

    def test_tree_view_adapters_are_required(self):
        for adapter in (
            "include_tree_getter",
            "include_tree_setter",
            "include_tree_vertical_scroll_getter",
            "include_tree_horizontal_scroll_getter",
        ):
            with self.subTest(adapter=adapter):
                self.write_module(**{adapter: False})
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("TreeView", result.stderr)

    def test_list_view_unsupported_native_surface_is_rejected(self):
        for method in (
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
            with self.subTest(method=method):
                self.write_list_view(public_method=method)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "unsupported native API {0}".format(method),
                    result.stderr,
                )

    def test_list_view_internal_scrollbar_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
            'Shiboken::Object::keepReference(self, "bar", pyResult);',
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_module(
                    scroll_getter_bookkeeping=operation
                )
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "ListView scrollbar adapter "
                    "listViewVerticalFluentScrollBar",
                    result.stderr,
                )

    def test_list_view_scrollbar_adapters_are_required(self):
        for adapter in (
            "include_vertical_scroll_getter",
            "include_horizontal_scroll_getter",
        ):
            with self.subTest(adapter=adapter):
                self.write_module(**{adapter: False})
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "ListView scrollbar adapter",
                    result.stderr,
                )

    def test_list_view_selection_adapters_are_required(self):
        for adapter in ("include_getter", "include_setter"):
            with self.subTest(adapter=adapter):
                self.write_module(**{adapter: False})
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("ListView selection adapter", result.stderr)

    def test_list_view_duplicate_selection_converter_is_rejected(self):
        self.write_fluent_namespace(selection_converter_count=2)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Expected one binding SelectionMode converter, found 2",
            result.stderr,
        )

    def test_list_view_missing_selection_converter_is_rejected(self):
        self.write_fluent_namespace(selection_converter_count=0)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Expected one binding SelectionMode converter, found 0",
            result.stderr,
        )

    def test_list_view_native_selection_converter_is_rejected(self):
        self.write_fluent_namespace(unstable_collections_converter=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "unstable collections SelectionMode converter",
            result.stderr,
        )

    def test_annotated_scroll_bar_link_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyArg);",
            "Shiboken::Object::getOwnership(pyArg);",
            "Shiboken::Object::keepReference(self, \"view\", pyArg);",
            "Shiboken::Object::setParent(self, pyArg);",
        ):
            with self.subTest(operation=operation):
                self.write_annotated_scroll_bar(
                    link_bookkeeping=operation
                )
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "AnnotatedScrollBar::connectToScrollView",
                    result.stderr,
                )

    def test_annotated_scroll_bar_getter_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
            "Shiboken::Object::keepReference(self, \"view\", pyResult);",
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_annotated_scroll_bar(
                    getter_bookkeeping=operation
                )
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "AnnotatedScrollBar::connectedScrollView",
                    result.stderr,
                )

    def test_annotated_scroll_bar_provider_surface_is_rejected(self):
        for method in (
            "setDetailLabelProvider",
            "clearDetailLabelProvider",
            "hasDetailLabelProvider",
        ):
            with self.subTest(method=method):
                self.write_annotated_scroll_bar(provider_api=method)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("unsupported provider API", result.stderr)

    def test_accordion_public_ownership_bypasses_are_rejected(self):
        for method in ("addItem", "insertItem"):
            with self.subTest(method=method):
                self.write_accordion(public_overload=method)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("runtime ownership bypass", result.stderr)

    def test_stack_view_public_navigation_bypasses_are_rejected(self):
        for method in (
            "push",
            "replace",
            "setInitialItem",
            "setCurrentWidget",
            "adoptWidget",
            "defaultItemOwnership",
        ):
            with self.subTest(method=method):
                self.write_stack_view(public_bypass=method)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "navigation ownership bypass",
                    result.stderr,
                )

    def test_stack_view_adapter_bookkeeping_is_rejected(self):
        operations = (
            "Shiboken::Object::releaseOwnership(pyArg);",
            "Shiboken::Object::getOwnership(pyArg);",
            'Shiboken::Object::keepReference(self, "page", pyArg);',
            "Shiboken::Object::setParent(self, pyArg);",
        )
        for adapter in (
            "_pushItemWithOwnership",
            "_pushItemsWithOwnership",
            "_replaceAtWithOwnership",
            "_replaceCurrentWithOwnership",
            "_setInitialItemWithOwnership",
        ):
            for operation in operations:
                with self.subTest(adapter=adapter, operation=operation):
                    self.write_stack_view(
                        adapter_bookkeeping=(adapter, operation)
                    )
                    result = self.run_verifier()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(
                        "StackView private ownership adapter",
                        result.stderr,
                    )

    def test_stack_view_getter_bookkeeping_is_rejected(self):
        for getter in ("currentItem", "initialItem", "itemAt"):
            for operation in (
                "Shiboken::Object::releaseOwnership(pyResult);",
                "Shiboken::Object::getOwnership(pyResult);",
                'Shiboken::Object::keepReference(self, "page", pyResult);',
                "Shiboken::Object::setParent(self, pyResult);",
            ):
                with self.subTest(getter=getter, operation=operation):
                    self.write_stack_view(
                        getter_bookkeeping=(getter, operation)
                    )
                    result = self.run_verifier()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("StackView::", result.stderr)

    def test_breadcrumb_missing_item_overload_is_rejected(self):
        self.write_breadcrumb(include_item_overload=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("BreadcrumbItem overload is missing", result.stderr)

    def test_breadcrumb_wrong_value_conversion_is_rejected(self):
        self.write_breadcrumb(include_value_conversion=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("value conversion is missing", result.stderr)

    def test_breadcrumb_ambiguous_native_setter_is_rejected(self):
        self.write_breadcrumb(include_public_setter=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("ambiguous native setItems overload", result.stderr)

    def test_breadcrumb_missing_list_adapter_is_rejected(self):
        for option, expected in (
            (
                "include_breadcrumb_text_setter",
                "setBreadcrumbTextItems is missing",
            ),
            (
                "include_breadcrumb_metadata_setter",
                "setBreadcrumbMetadataItems is missing",
            ),
        ):
            with self.subTest(option=option):
                self.write_module(**{option: False})
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(expected, result.stderr)

    def test_breadcrumb_wrong_list_converter_is_rejected(self):
        for option in (
            "breadcrumb_text_converter",
            "breadcrumb_metadata_converter",
        ):
            with self.subTest(option=option):
                self.write_module(**{option: "UnsupportedList"})
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("converter is missing", result.stderr)

    def test_breadcrumb_item_missing_field_is_rejected(self):
        self.write_breadcrumb_item(missing_field="enabled")
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("field enabled getter is missing", result.stderr)

    def test_breadcrumb_item_missing_variant_conversion_is_rejected(self):
        self.write_breadcrumb_item(include_variant_conversion=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "QVariant data getter conversion is missing",
            result.stderr,
        )

    def test_metadata_navigation_missing_item_overloads_are_rejected(self):
        cases = (
            ("Pivot", "PivotItem", self.write_pivot),
            ("SelectorBar", "SelectorBarItem", self.write_selector_bar),
        )
        for widget_name, item_name, writer in cases:
            for option, method_name in (
                ("include_add_item_overload", "addItem"),
                ("include_insert_item_overload", "insertItem"),
            ):
                with self.subTest(widget=widget_name, option=option):
                    writer(**{option: False})
                    result = self.run_verifier()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(
                        "{0}::{1} {2} overload".format(
                            widget_name,
                            method_name,
                            item_name,
                        ),
                        result.stderr,
                    )
                    writer()

    def test_metadata_navigation_wrong_value_conversions_are_rejected(self):
        cases = (
            ("Pivot", self.write_pivot),
            ("SelectorBar", self.write_selector_bar),
        )
        for widget_name, writer in cases:
            for option, method_name in (
                ("include_item_conversion", "itemAt"),
                ("include_items_conversion", "items"),
            ):
                with self.subTest(widget=widget_name, option=option):
                    writer(**{option: False})
                    result = self.run_verifier()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(
                        "{0}::{1}".format(widget_name, method_name),
                        result.stderr,
                    )
                    writer()

    def test_metadata_navigation_missing_fields_are_rejected(self):
        cases = (
            ("PivotItem", self.write_pivot_item, "enabled"),
            (
                "SelectorBarItem",
                self.write_selector_bar_item,
                "visible",
            ),
        )
        for item_name, writer, field_name in cases:
            with self.subTest(item=item_name):
                writer(missing_field=field_name)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "{0} field {1} getter".format(item_name, field_name),
                    result.stderr,
                )
                writer()

    def test_metadata_navigation_variant_conversions_are_required(self):
        cases = (
            ("PivotItem", self.write_pivot_item),
            ("SelectorBarItem", self.write_selector_bar_item),
        )
        for item_name, writer in cases:
            with self.subTest(item=item_name):
                writer(include_variant_conversion=False)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "{0} QVariant data getter conversion".format(item_name),
                    result.stderr,
                )
                writer()

    def test_tab_view_missing_item_overload_is_rejected(self):
        self.write_tab_view(include_item_overload=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("TabViewItem overload is missing", result.stderr)

    def test_tab_view_wrong_value_conversion_is_rejected(self):
        self.write_tab_view(include_value_conversion=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("value conversion is missing", result.stderr)

    def test_tab_view_item_missing_field_is_rejected(self):
        self.write_tab_view_item(missing_field="data")
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("field data getter is missing", result.stderr)

    def test_tab_view_item_missing_variant_conversion_is_rejected(self):
        self.write_tab_view_item(include_variant_conversion=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "QVariant data getter conversion is missing",
            result.stderr,
        )

    def test_internal_tab_strip_wrapper_is_rejected(self):
        (self.generated_dir / TAB_STRIP_WRAPPER).write_text(
            "// Internal implementation must stay private.\n",
            encoding="utf-8",
        )
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("internal TabStrip", result.stderr)

    def test_accordion_adapter_bookkeeping_is_rejected(self):
        operations = (
            "Shiboken::Object::releaseOwnership(pyArg);",
            "Shiboken::Object::getOwnership(pyArg);",
            "Shiboken::Object::keepReference(self, \"item\", pyArg);",
            "Shiboken::Object::setParent(self, pyArg);",
        )
        for adapter in ("add_bookkeeping", "insert_bookkeeping"):
            for operation in operations:
                with self.subTest(adapter=adapter, operation=operation):
                    self.write_accordion(**{adapter: operation})
                    result = self.run_verifier()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("Accordion private", result.stderr)

    def test_accordion_item_getter_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
            "Shiboken::Object::keepReference(self, \"item\", pyResult);",
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_accordion(getter_bookkeeping=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Accordion::itemAt", result.stderr)

    def test_accordion_take_requires_python_ownership(self):
        self.write_accordion(transfer_taken_to_python=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Accordion::takeItem Python ownership transfer is missing",
            result.stderr,
        )

    def test_accordion_take_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::keepReference(self, \"item\", pyResult);",
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_accordion(take_bookkeeping=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Accordion::takeItem", result.stderr)

    def test_expander_runtime_ownership_overload_is_rejected(self):
        self.write_expander(runtime_overload=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Expander::setContentWidget exposes", result.stderr)

    def test_expander_parent_bookkeeping_is_rejected(self):
        self.write_expander(use_parent=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Expander::setContentWidget uses parent bookkeeping",
            result.stderr,
        )

    def test_expander_adapter_ownership_change_is_rejected(self):
        self.write_expander(adapter_transfer_to_cpp=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Expander private ownership adapter changes wrapper ownership",
            result.stderr,
        )

    def test_expander_missing_take_ownership_is_rejected(self):
        self.write_expander(transfer_to_python=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Expander::takeContentWidget Python ownership transfer is missing",
            result.stderr,
        )

    def test_expander_internal_header_button_is_rejected(self):
        self.write_expander(include_header_button=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("internal header button", result.stderr)

    def test_info_bar_action_parent_bookkeeping_is_rejected(self):
        self.write_info_bar(setter_parent=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "InfoBar private action adapter uses parent bookkeeping",
            result.stderr,
        )

    def test_info_bar_action_keep_reference_is_rejected(self):
        self.write_info_bar(setter_keep_reference=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "InfoBar private action adapter uses keep-reference bookkeeping",
            result.stderr,
        )

    def test_info_bar_action_ownership_change_is_rejected(self):
        for option in (
            "setter_transfer_to_cpp",
            "setter_transfer_to_python",
        ):
            with self.subTest(option=option):
                self.write_info_bar(**{option: True})
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "InfoBar private action adapter changes wrapper ownership",
                    result.stderr,
                )

    def test_info_bar_action_getter_bookkeeping_is_rejected(self):
        for option in (
            "getter_parent",
            "getter_transfer_to_cpp",
        ):
            with self.subTest(option=option):
                self.write_info_bar(**{option: True})
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("InfoBar::actionWidget", result.stderr)

    def test_pips_pager_internal_animation_properties_are_rejected(self):
        for internal_property in (
            "selectedVisualOffset",
            "visibleWindowOffset",
        ):
            with self.subTest(internal_property=internal_property):
                self.write_pips_pager(internal_property)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "internal animation property {0}".format(
                        internal_property
                    ),
                    result.stderr,
                )

    def test_native_only_mode_does_not_require_namespace_wrapper(self):
        (self.generated_dir / WINDOWING_NAMESPACE_WRAPPER).unlink()
        result = self.run_verifier()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_protected_access_macro_hack_is_rejected(self):
        (self.generated_dir / "fluent_dummy_wrapper.h").write_text(
            "#  define protected public\n",
            encoding="utf-8",
        )
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("protected-access macro hack", result.stderr)


if __name__ == "__main__":
    unittest.main()
