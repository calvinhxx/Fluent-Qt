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
ACCORDION_WRAPPER = "fluent_layout_accordion_wrapper.cpp"
EXPANDER_WRAPPER = "fluent_layout_expander_wrapper.cpp"
INFO_BAR_WRAPPER = "fluent_status_info_infobar_wrapper.cpp"
ANNOTATED_SCROLL_BAR_WRAPPER = (
    "fluent_scrolling_annotatedscrollbar_wrapper.cpp"
)
PIPS_PAGER_WRAPPER = "fluent_scrolling_pipspager_wrapper.cpp"
SCROLL_VIEW_WRAPPER = "fluent_scrolling_scrollview_wrapper.cpp"
STACK_VIEW_WRAPPER = "fluent_collections_stackview_wrapper.cpp"
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
        self.write_scroll_view()
        self.write_stack_view()
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
