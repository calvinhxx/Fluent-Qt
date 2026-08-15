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
TITLE_BAR_WRAPPER = "fluent_windowing_titlebar_wrapper.cpp"
WINDOWING_NAMESPACE_WRAPPER = "fluent_windowing_wrapper.cpp"
MODULE_WRAPPER = "_fluentqt_module_wrapper.cpp"
FLUENT_NAMESPACE_WRAPPER = "fluent_wrapper.cpp"
ACCORDION_WRAPPER = "fluent_layout_accordion_wrapper.cpp"
EXPANDER_WRAPPER = "fluent_layout_expander_wrapper.cpp"
FIELD_WRAPPER = "fluent_layout_field_wrapper.cpp"
INFO_BAR_WRAPPER = "fluent_status_info_infobar_wrapper.cpp"
TOAST_WRAPPER = "fluent_status_info_toast_wrapper.cpp"
TOOLTIP_WRAPPER = "fluent_status_info_tooltip_wrapper.cpp"
ANNOTATED_SCROLL_BAR_WRAPPER = (
    "fluent_scrolling_annotatedscrollbar_wrapper.cpp"
)
PIPS_PAGER_WRAPPER = "fluent_scrolling_pipspager_wrapper.cpp"
COMBO_BOX_WRAPPER = "fluent_basicinput_combobox_wrapper.cpp"
AUTO_SUGGEST_BOX_WRAPPER = (
    "fluent_textfields_autosuggestbox_wrapper.cpp"
)
LINE_EDIT_WRAPPER = "fluent_textfields_lineedit_wrapper.cpp"
CALENDAR_DATE_PICKER_WRAPPER = (
    "fluent_date_time_calendardatepicker_wrapper.cpp"
)
DATE_PICKER_WRAPPER = "fluent_date_time_datepicker_wrapper.cpp"
TIME_PICKER_WRAPPER = "fluent_date_time_timepicker_wrapper.cpp"
DROP_DOWN_BUTTON_WRAPPER = (
    "fluent_basicinput_dropdownbutton_wrapper.cpp"
)
SPLIT_BUTTON_WRAPPER = "fluent_basicinput_splitbutton_wrapper.cpp"
FLUENT_MENU_WRAPPER = (
    "fluent_menus_toolbars_fluentmenu_wrapper.cpp"
)
FLUENT_MENU_ITEM_WRAPPER = (
    "fluent_menus_toolbars_fluentmenuitem_wrapper.cpp"
)
COMMAND_BAR_WRAPPER = "fluent_menus_toolbars_commandbar_wrapper.cpp"
COMMAND_BAR_FLYOUT_WRAPPER = (
    "fluent_menus_toolbars_commandbarflyout_wrapper.cpp"
)
FLUENT_MENU_BAR_WRAPPER = (
    "fluent_menus_toolbars_fluentmenubar_wrapper.cpp"
)
SCROLL_VIEW_WRAPPER = "fluent_scrolling_scrollview_wrapper.cpp"
DRAWER_VIEW_WRAPPER = "fluent_collections_drawerview_wrapper.cpp"
CONTENT_DIALOG_WRAPPER = (
    "fluent_dialogs_flyouts_contentdialog_wrapper.cpp"
)
DIALOG_WRAPPER = "fluent_dialogs_flyouts_dialog_wrapper.cpp"
POPUP_WRAPPER = "fluent_dialogs_flyouts_popup_wrapper.cpp"
FLYOUT_WRAPPER = "fluent_dialogs_flyouts_flyout_wrapper.cpp"
COACH_MARK_WRAPPER = "fluent_dialogs_flyouts_coachmark_wrapper.cpp"
TEACHING_TIP_WRAPPER = "fluent_dialogs_flyouts_teachingtip_wrapper.cpp"
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
COMBO_BOX_DELEGATE_WRAPPER = (
    "fluent_basicinput_comboboxitemdelegate_wrapper.cpp"
)
DATE_TIME_INTERNAL_WRAPPERS = (
    "fluent_date_time_calendardatepickerpopup_wrapper.cpp",
    "fluent_date_time_datepickerflyout_wrapper.cpp",
    "fluent_date_time_timepickerflyout_wrapper.cpp",
)
AUTO_SUGGEST_INTERNAL_WRAPPERS = (
    "fluent_textfields_suggestionlistpopup_wrapper.cpp",
    "fluent_textfields_autosuggestitemdelegate_wrapper.cpp",
)
ENUM_CONVERTER = (
    "static void Enum_PythonToCpp_fluent_windowing_BackdropEffect"
)


def window_source(
    limited_api_arguments=True,
    expose_result_pointer=False,
    split_override=True,
    title_bar_parent=True,
    expose_theme_hook=False,
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
    title_bar_bookkeeping = (
        "Shiboken::Object::setParent(self, pyResult);"
        if title_bar_parent
        else ""
    )
    theme_hook = ""
    if expose_theme_hook:
        theme_hook = textwrap.dedent(
            """
            static PyObject *Sbk_fluent_windowing_WindowFunc_onThemeUpdated(
                PyObject *self)
            {
                return Py_None;
            }
            """
        )
    return textwrap.dedent(
        """
        bool WindowWrapper::eventFilter(QObject *, QEvent *)
        {{
            auto decoy = Py_BuildValue("(NN)", first, second);
            return decoy != nullptr;
        }}

        {native_event}

        static PyObject *Sbk_fluent_windowing_WindowFunc_titleBar(
            PyObject *self)
        {{
            pyResult = cppSelf->titleBar();
            {title_bar_bookkeeping}
            return pyResult;
        }}

        {theme_hook}
        """
    ).format(
        native_event=native_event,
        title_bar_bookkeeping=title_bar_bookkeeping,
        theme_hook=theme_hook,
    )


def title_bar_source(
    release_old_child=True,
    parent_new_child=True,
    getter_parent=True,
    expose_theme_hook=False,
):
    release = ""
    if release_old_child:
        release = textwrap.dedent(
            """
            QWidget *oldChild = cppSelf->contentWidget();
            Shiboken::Object::setParent(nullptr, pyChild);
            Shiboken::Object::releaseOwnership(pyChild);
            """
        )
    new_parent = (
        "Shiboken::Object::setParent(self, pyArg);"
        if parent_new_child
        else ""
    )
    getter_bookkeeping = (
        "Shiboken::Object::setParent(self, pyResult);"
        if getter_parent
        else ""
    )
    theme_hook = ""
    if expose_theme_hook:
        theme_hook = textwrap.dedent(
            """
            static PyObject *Sbk_fluent_windowing_TitleBarFunc_onThemeUpdated(
                PyObject *self)
            {
                return Py_None;
            }
            """
        )
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_windowing_TitleBarFunc_setContentWidget(
            PyObject *self, PyObject *pyArg)
        {{
            {release}
            {new_parent}
            cppSelf->setContentWidget(cppArg0);
            return Py_None;
        }}

        static PyObject *Sbk_fluent_windowing_TitleBarFunc_contentWidget(
            PyObject *self)
        {{
            pyResult = cppSelf->contentWidget();
            {getter_bookkeeping}
            return pyResult;
        }}

        {theme_hook}
        """
    ).format(
        release=release,
        new_parent=new_parent,
        getter_bookkeeping=getter_bookkeeping,
        theme_hook=theme_hook,
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


def drawer_view_source(
    public_setter=False,
    adapter_operation="",
    getter_operation="",
    transfer_taken=True,
    take_operation="",
):
    public_method = ""
    if public_setter:
        public_method = """
        static PyObject *Sbk_fluent_collections_DrawerViewFunc_setContentWidget(
            PyObject *self, PyObject *pyArg)
        {
            cppSelf->setContentWidget(cppArg0);
            return Py_None;
        }
        """
    transfer = (
        "Shiboken::Object::getOwnership(pyResult);"
        if transfer_taken
        else ""
    )
    return textwrap.dedent(
        """
        {public_method}

        static PyObject *Sbk_fluent_collections_DrawerViewFunc__setContentWidgetWithOwnership(
            PyObject *self, PyObject *args)
        {{
            bool cppResult = cppSelf->setContentWidget(cppArg0, cppArg1);
            {adapter_operation}
            return PyBool_FromLong(cppResult);
        }}

        static PyObject *Sbk_fluent_collections_DrawerViewFunc_contentWidget(
            PyObject *self)
        {{
            pyResult = cppSelf->contentWidget();
            {getter_operation}
            return pyResult;
        }}

        static PyObject *Sbk_fluent_collections_DrawerViewFunc_takeContentWidget(
            PyObject *self)
        {{
            pyResult = cppSelf->takeContentWidget();
            {transfer}
            {take_operation}
            return pyResult;
        }}
        """
    ).format(
        public_method=textwrap.dedent(public_method),
        adapter_operation=adapter_operation,
        getter_operation=getter_operation,
        transfer=transfer,
        take_operation=take_operation,
    )


def popup_source(
    public_bypass=None,
    protected_hook=None,
    adapter_name=None,
    adapter_operation="",
):
    public_function = ""
    if public_bypass is not None:
        public_function = textwrap.dedent(
            """
            static PyObject *Sbk_fluent_dialogs_flyouts_PopupFunc_{name}(
                PyObject *self)
            {{
                return Py_None;
            }}
            """
        ).format(name=public_bypass)

    protected_function = ""
    if protected_hook is not None:
        protected_function = textwrap.dedent(
            """
            static PyObject *Sbk_fluent_dialogs_flyouts_PopupFunc_{name}(
                PyObject *self)
            {{
                return Py_None;
            }}
            """
        ).format(name=protected_hook)

    adapters = []
    for name, native_call in (
        (
            "_setPositionWithAnchor",
            "cppSelf->setPosition(cppArg0, *cppArg1);",
        ),
        ("_setThemeSource", "cppSelf->setThemeSource(cppArg0);"),
        (
            "_addLightDismissPassthrough",
            "cppSelf->addLightDismissPassthrough(cppArg0);",
        ),
        (
            "_clearLightDismissPassthrough",
            "cppSelf->clearLightDismissPassthrough();",
        ),
    ):
        operation = adapter_operation if adapter_name == name else ""
        adapters.append(
            textwrap.dedent(
                """
                static PyObject *Sbk_fluent_dialogs_flyouts_PopupFunc_{name}(
                    PyObject *self)
                {{
                    {native_call}
                    {operation}
                    return Py_None;
                }}
                """
            ).format(
                name=name,
                native_call=native_call,
                operation=operation,
            )
        )
    return "\n".join(
        adapters + [public_function, protected_function]
    )


def flyout_source(
    public_bypass=None,
    protected_hook=None,
    adapter_name=None,
    adapter_operation="",
    getter_operation="",
    include_anchor_call=True,
):
    public_function = ""
    if public_bypass is not None:
        public_function = textwrap.dedent(
            """
            static PyObject *Sbk_fluent_dialogs_flyouts_FlyoutFunc_{name}(
                PyObject *self)
            {{
                return Py_None;
            }}
            """
        ).format(name=public_bypass)

    protected_function = ""
    if protected_hook is not None:
        protected_function = textwrap.dedent(
            """
            static PyObject *Sbk_fluent_dialogs_flyouts_FlyoutFunc_{name}(
                PyObject *self)
            {{
                return Py_None;
            }}
            """
        ).format(name=protected_hook)

    adapters = []
    for name, native_call in (
        ("_setAnchor", "cppSelf->setAnchor(cppArg0);"),
        ("_showAt", "cppSelf->showAt(cppArg0);"),
    ):
        operation = adapter_operation if adapter_name == name else ""
        adapters.append(
            textwrap.dedent(
                """
                static PyObject *Sbk_fluent_dialogs_flyouts_FlyoutFunc_{name}(
                    PyObject *self)
                {{
                    {native_call}
                    {operation}
                    return Py_None;
                }}
                """
            ).format(
                name=name,
                native_call=native_call,
                operation=operation,
            )
        )

    anchor_call = (
        "pyResult = const_cast<const FlyoutWrapper *>(cppSelf)->anchor();"
        if include_anchor_call
        else "pyResult = Py_None;"
    )
    anchor_getter = textwrap.dedent(
        """
        static PyObject *Sbk_fluent_dialogs_flyouts_FlyoutFunc_anchor(
            PyObject *self)
        {{
            {anchor_call}
            {getter_operation}
            return pyResult;
        }}
        """
    ).format(
        anchor_call=anchor_call,
        getter_operation=getter_operation,
    )

    return "\n".join(
        adapters + [anchor_getter, public_function, protected_function]
    )


def observed_target_overlay_source(
    class_name,
    adapter_calls,
    public_bypass=None,
    protected_hook=None,
    missing_adapter_call=None,
    adapter_name=None,
    adapter_operation="",
    missing_getter_call=None,
    getter_name=None,
    getter_operation="",
):
    function_prefix = "Sbk_fluent_dialogs_flyouts_{0}Func_".format(
        class_name
    )
    public_function = ""
    if public_bypass is not None:
        public_function = textwrap.dedent(
            """
            static PyObject *{prefix}{name}(PyObject *self)
            {{
                return Py_None;
            }}
            """
        ).format(prefix=function_prefix, name=public_bypass)

    protected_function = ""
    if protected_hook is not None:
        protected_function = textwrap.dedent(
            """
            static PyObject *{prefix}{name}(PyObject *self)
            {{
                return Py_None;
            }}

            void {class_name}Wrapper::{name}()
            {{
            }}
            """
        ).format(
            prefix=function_prefix,
            class_name=class_name,
            name=protected_hook,
        )

    adapters = []
    for name, native_call in adapter_calls:
        operation = adapter_operation if adapter_name == name else ""
        call = "" if missing_adapter_call == name else native_call
        adapters.append(
            textwrap.dedent(
                """
                static PyObject *{prefix}{name}(PyObject *self)
                {{
                    {call};
                    {operation}
                    return Py_None;
                }}
                """
            ).format(
                prefix=function_prefix,
                name=name,
                call=call,
                operation=operation,
            )
        )

    getters = []
    for name in ("target", "contentHost"):
        operation = getter_operation if getter_name == name else ""
        call = (
            "pyResult = cppSelf->{0}()".format(name)
            if missing_getter_call != name
            else "pyResult = Py_None"
        )
        getters.append(
            textwrap.dedent(
                """
                static PyObject *{prefix}{name}(PyObject *self)
                {{
                    {call};
                    {operation}
                    return pyResult;
                }}
                """
            ).format(
                prefix=function_prefix,
                name=name,
                call=call,
                operation=operation,
            )
        )

    return "\n".join(
        adapters + getters + [public_function, protected_function]
    )


def coach_mark_source(**kwargs):
    return observed_target_overlay_source(
        "CoachMark",
        (("_setTarget", "cppSelf->setTarget(cppArg0)"),),
        **kwargs,
    )


def teaching_tip_source(**kwargs):
    return observed_target_overlay_source(
        "TeachingTip",
        (
            ("_setTarget", "cppSelf->setTarget(cppArg0)"),
            ("_showAt", "cppSelf->showAt(cppArg0)"),
        ),
        **kwargs,
    )


def dialog_source(
    public_bypass=False,
    protected_hook=None,
    adapter_operation="",
    include_adapter_call=True,
):
    public_function = ""
    if public_bypass:
        public_function = textwrap.dedent(
            """
            static PyObject *
            Sbk_fluent_dialogs_flyouts_DialogFunc_setThemeSource(
                PyObject *self)
            {
                return Py_None;
            }
            """
        )

    protected_function = ""
    if protected_hook is not None:
        protected_function = textwrap.dedent(
            """
            static PyObject *
            Sbk_fluent_dialogs_flyouts_DialogFunc_{name}(
                PyObject *self)
            {{
                return Py_None;
            }}
            """
        ).format(name=protected_hook)

    adapter_call = (
        "cppSelf->setThemeSource(cppArg0);"
        if include_adapter_call
        else ""
    )
    adapter = textwrap.dedent(
        """
        static PyObject *Sbk_fluent_dialogs_flyouts_DialogFunc__setThemeSource(
            PyObject *self)
        {{
            {adapter_call}
            {adapter_operation}
            return Py_None;
        }}
        """
    ).format(
        adapter_call=adapter_call,
        adapter_operation=adapter_operation,
    )
    return "\n".join((adapter, public_function, protected_function))


def content_dialog_source(
    public_bypass=False,
    expose_theme_hook=False,
    unsafe_static_fields=False,
    adapter_operation="",
    getter_operation="",
    include_adapter_call=True,
    include_getter_call=True,
):
    public_function = ""
    if public_bypass:
        public_function = textwrap.dedent(
            """
            static PyObject *
            Sbk_fluent_dialogs_flyouts_ContentDialogFunc_setContent(
                PyObject *self)
            {
                return Py_None;
            }
            """
        )
    theme_hook = ""
    if expose_theme_hook:
        theme_hook = textwrap.dedent(
            """
            static PyObject *
            Sbk_fluent_dialogs_flyouts_ContentDialogFunc_onThemeUpdated(
                PyObject *self)
            {
                return Py_None;
            }
            """
        )
    static_fields = (
        "init_fluent_dialogs_flyouts_ContentDialogStaticFields(module);"
        if unsafe_static_fields
        else ""
    )
    adapter_call = (
        "cppSelf->setContent(cppArg0);" if include_adapter_call else ""
    )
    getter_call = (
        "pyResult = const_cast<const ContentDialogWrapper *>(cppSelf)->content();"
        if include_getter_call
        else "pyResult = Py_None;"
    )
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_dialogs_flyouts_ContentDialogFunc__setContent(
            PyObject *self)
        {{
            {adapter_call}
            {adapter_operation}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_dialogs_flyouts_ContentDialogFunc_content(
            PyObject *self)
        {{
            {getter_call}
            {getter_operation}
            return pyResult;
        }}

        {public_function}
        {theme_hook}
        {static_fields}
        """
    ).format(
        adapter_call=adapter_call,
        adapter_operation=adapter_operation,
        getter_call=getter_call,
        getter_operation=getter_operation,
        public_function=public_function,
        theme_hook=theme_hook,
        static_fields=static_fields,
    )


def combo_box_source(
    expose_internal=None,
    parent_editor=True,
    retain_editor=False,
    retain_model=True,
    model_getter_operation="",
    include_native_fallback=True,
):
    internal_api = ""
    if expose_internal is not None:
        internal_api = textwrap.dedent(
            """
            static PyObject *Sbk_fluent_basicinput_ComboBoxFunc_{name}(
                PyObject *self)
            {{
                return Py_None;
            }}
            """
        ).format(name=expose_internal)
    parent_operation = (
        "Shiboken::Object::setParent(self, pyArg);"
        if parent_editor
        else ""
    )
    retention = (
        'Shiboken::Object::keepReference(self, "editor", pyArg);'
        if retain_editor
        else ""
    )
    model_retention = (
        'Shiboken::Object::keepReference(self, "model", pyArg);'
        if retain_model
        else ""
    )
    show_fallback = (
        "return this->::fluent::basicinput::ComboBox::showPopup();"
        if include_native_fallback
        else "return;"
    )
    hide_fallback = (
        "return this->::fluent::basicinput::ComboBox::hidePopup();"
        if include_native_fallback
        else "return;"
    )
    return textwrap.dedent(
        """
        void ComboBoxWrapper::showPopup()
        {{
            {show_fallback}
        }}

        void ComboBoxWrapper::hidePopup()
        {{
            {hide_fallback}
        }}

        static PyObject *Sbk_fluent_basicinput_ComboBoxFunc_setLineEdit(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->setLineEdit(cppArg0);
            {parent_operation}
            {retention}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_basicinput_ComboBoxFunc_fluentLineEdit(
            PyObject *self)
        {{
            pyResult = cppSelf->fluentLineEdit();
            Shiboken::Object::setParent(self, pyResult);
            return pyResult;
        }}

        static PyObject *Sbk_fluent_basicinput_ComboBoxFunc_setModel(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->setModel(cppArg0);
            {model_retention}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_basicinput_ComboBoxFunc_model(
            PyObject *self)
        {{
            pyResult = cppSelf->model();
            {model_getter_operation}
            return pyResult;
        }}

        {internal_api}
        """
    ).format(
        show_fallback=show_fallback,
        hide_fallback=hide_fallback,
        parent_operation=parent_operation,
        retention=retention,
        model_retention=model_retention,
        model_getter_operation=model_getter_operation,
        internal_api=internal_api,
    )


def auto_suggest_box_source(
    expose_theme_hook=False,
    include_native_fallback=True,
    include_setter_conversion=True,
    include_getter_conversion=True,
):
    theme_hook = ""
    if expose_theme_hook:
        theme_hook = textwrap.dedent(
            """
            static PyObject *
            Sbk_fluent_textfields_AutoSuggestBoxFunc_onThemeUpdated(
                PyObject *self)
            {
                return Py_None;
            }
            """
        )
    native_fallback = (
        "return this->::fluent::textfields::AutoSuggestBox::"
        "keyPressEvent(event);"
        if include_native_fallback
        else "return;"
    )
    setter_conversion = (
        "::QStringList cppArg0;\n"
        "pythonToCpp(pyArg, &cppArg0);"
        if include_setter_conversion
        else "int cppArg0 = 0;"
    )
    getter_conversion = (
        "QStringList cppResult = cppSelf->suggestions();\n"
        "pyResult = Shiboken::Conversions::copyToPython("
        "converter, &cppResult);"
        if include_getter_conversion
        else "auto cppResult = cppSelf->suggestions();"
    )
    return textwrap.dedent(
        """
        void AutoSuggestBoxWrapper::keyPressEvent(QKeyEvent *event)
        {{
            {native_fallback}
        }}

        static PyObject *Sbk_fluent_textfields_AutoSuggestBoxFunc_setSuggestions(
            PyObject *self, PyObject *pyArg)
        {{
            {setter_conversion}
            cppSelf->setSuggestions(cppArg0);
            return Py_None;
        }}

        static PyObject *Sbk_fluent_textfields_AutoSuggestBoxFunc_suggestions(
            PyObject *self)
        {{
            PyObject *pyResult = nullptr;
            {getter_conversion}
            return pyResult;
        }}

        static PyObject *Sbk_fluent_textfields_AutoSuggestBoxFunc_clearSuggestions(
            PyObject *self)
        {{
            cppSelf->clearSuggestions();
            return Py_None;
        }}

        static PyObject *Sbk_fluent_textfields_AutoSuggestBoxFunc_isSuggestionListOpen(
            PyObject *self)
        {{
            return PyBool_FromLong(cppSelf->isSuggestionListOpen());
        }}

        {theme_hook}
        """
    ).format(
        native_fallback=native_fallback,
        setter_conversion=setter_conversion,
        getter_conversion=getter_conversion,
        theme_hook=theme_hook,
    )


def menu_button_source(
    class_name,
    retain_menu=True,
    getter_operation="",
):
    retention = (
        'Shiboken::Object::keepReference(self, "menu", pyArg);'
        if retain_menu
        else ""
    )
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_basicinput_{class_name}Func_setMenu(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->setMenu(cppArg0);
            {retention}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_basicinput_{class_name}Func_menu(
            PyObject *self)
        {{
            pyResult = cppSelf->menu();
            {getter_operation}
            return pyResult;
        }}
        """
    ).format(
        class_name=class_name,
        retention=retention,
        getter_operation=getter_operation,
    )


def fluent_menu_source(class_name, expose_theme_hook=False):
    if not expose_theme_hook:
        return "// no public theme hook\n"
    return textwrap.dedent(
        """
        static PyObject *
        Sbk_fluent_menus_toolbars_{class_name}Func_onThemeUpdated(
            PyObject *self)
        {{
            return Py_None;
        }}
        """
    ).format(class_name=class_name)


def command_action_surface_source(
    class_name,
    sync_helper,
    subclassable,
    drop_sync_method=None,
    ownership_method=None,
    release_before_clear=False,
    retain_anchor=True,
    unresolved_placeholder=False,
    expose_theme_hook=False,
    complete_helper=True,
    callable_glue_count=4,
    use_qt_convenience_actions=False,
    wrapper_shell=False,
):
    if use_qt_convenience_actions:
        glue_body = "self->addAction(text);"
    else:
        glue_body = (
            "auto *action = new QAction(text, self); "
            "self->addAction(action);"
        )
    glue = "\n".join(
        "static inline PyObject *addActionWithPyObject() "
        "{{ {0} return nullptr; }}".format(glue_body)
        for _ in range(callable_glue_count)
    )
    secondary_check = (
        "else if (surface->secondaryActions().contains(action))\n"
        "        Shiboken::Object::keepReference(self, secondaryKey, "
        "pyAction, true);"
        if complete_helper
        else ""
    )
    helper = textwrap.dedent(
        """
        static void {sync_helper}(SbkObject *self, Surface *surface,
                                  QAction *action, PyObject *pyAction)
        {{
            static const char primaryKey[] = "{class_name}.primaryActions";
            static const char secondaryKey[] = "{class_name}.secondaryActions";
            Shiboken::Object::removeReference(self, primaryKey, pyAction);
            Shiboken::Object::removeReference(self, secondaryKey, pyAction);
            if (surface->primaryActions().contains(action))
                Shiboken::Object::keepReference(
                    self, primaryKey, pyAction, true);
            {secondary_check}
        }}
        """
    ).format(
        sync_helper=sync_helper,
        class_name=class_name,
        secondary_check=secondary_check,
    )

    functions = []
    for method_name in (
        "addAction",
        "insertAction",
        "removeAction",
        "addPrimaryAction",
        "insertPrimaryAction",
        "addSecondaryAction",
        "insertSecondaryAction",
        "removeCommandAction",
    ):
        arguments = (
            "cppArg0, cppArg1"
            if method_name.startswith("insert")
            else "cppArg0"
        )
        ownership = (
            "Shiboken::Object::setParent(self, pyArg);"
            if method_name == ownership_method
            else ""
        )
        sync = (
            ""
            if method_name == drop_sync_method
            else "{0}(self, cppSelf, cppArg0, pyArg);".format(sync_helper)
        )
        functions.append(
            textwrap.dedent(
                """
                static PyObject *Sbk_fluent_menus_toolbars_{class_name}Func_{method_name}(
                    PyObject *self)
                {{
                    cppSelf->{method_name}({arguments});
                    {ownership}
                    {sync}
                    return Py_None;
                }}
                """
            ).format(
                class_name=class_name,
                method_name=method_name,
                arguments=arguments,
                ownership=ownership,
                sync=sync,
            )
        )

    for method_name, section in (
        ("clearPrimaryActions", "primaryActions"),
        ("clearSecondaryActions", "secondaryActions"),
    ):
        release = textwrap.dedent(
            """
            Shiboken::Object::keepReference(
                self, "{class_name}.{section}", Py_None);
            """
        ).format(class_name=class_name, section=section)
        before = release if release_before_clear else ""
        after = "" if release_before_clear else release
        functions.append(
            textwrap.dedent(
                """
                static PyObject *Sbk_fluent_menus_toolbars_{class_name}Func_{method_name}(
                    PyObject *self)
                {{
                    {before}
                    cppSelf->{method_name}();
                    {after}
                    return Py_None;
                }}
                """
            ).format(
                class_name=class_name,
                method_name=method_name,
                before=before,
                after=after,
            )
        )

    if class_name == "CommandBarFlyout":
        retention = (
            'Shiboken::Object::keepReference(self, '
            '"CommandBarFlyout.anchor", pyArg);'
            if retain_anchor
            else ""
        )
        for method_name in ("setAnchor", "showAt", "showAtPoint"):
            functions.append(
                textwrap.dedent(
                    """
                    static PyObject *Sbk_fluent_menus_toolbars_CommandBarFlyoutFunc_{method_name}(
                        PyObject *self)
                    {{
                        cppSelf->{method_name}(cppArg0);
                        {retention}
                        return Py_None;
                    }}
                    """
                ).format(
                    method_name=method_name,
                    retention=retention,
                )
            )

    extras = []
    if unresolved_placeholder:
        extras.append("cppSelf->::%CLASS_NAME::eventFilter(nullptr, nullptr);")
    if expose_theme_hook:
        extras.append(
            "static PyObject *"
            "Sbk_fluent_menus_toolbars_{0}Func_onThemeUpdated("
            "PyObject *) {{ return Py_None; }}".format(class_name)
        )
    if wrapper_shell:
        extras.append(
            "void {0}Wrapper::eventFilter() {{}}".format(class_name)
        )
    flags = "Py_TPFLAGS_DEFAULT|Py_TPFLAGS_HAVE_GC"
    if subclassable:
        flags += "|Py_TPFLAGS_BASETYPE"
    return "\n".join(
        [glue, helper]
        + functions
        + extras
        + [flags]
    )


def fluent_menu_bar_source(expose_theme_hook=False):
    if not expose_theme_hook:
        return "// no public FluentMenuBar theme hook\n"
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_menus_toolbars_FluentMenuBarFunc_onThemeUpdated(
            PyObject *self)
        {
            return Py_None;
        }
        """
    )


def date_time_picker_source(
    class_name,
    lifecycle_methods,
    calendar_getter_operation="",
    expose_theme_hook=False,
):
    function_prefix = "Sbk_fluent_date_time_{0}Func_".format(class_name)
    functions = []
    for method_name in lifecycle_methods:
        functions.append(
            textwrap.dedent(
                """
                static PyObject *{prefix}{method_name}(PyObject *self)
                {{
                    cppSelf->{method_name}();
                    return Py_None;
                }}
                """
            ).format(prefix=function_prefix, method_name=method_name)
        )
    if class_name == "CalendarDatePicker":
        functions.append(
            textwrap.dedent(
                """
                static PyObject *{prefix}calendarView(PyObject *self)
                {{
                    pyResult = cppSelf->calendarView();
                    {getter_operation}
                    return pyResult;
                }}
                """
            ).format(
                prefix=function_prefix,
                getter_operation=calendar_getter_operation,
            )
        )
    if expose_theme_hook:
        functions.append(
            textwrap.dedent(
                """
                static PyObject *{prefix}onThemeUpdated(PyObject *self)
                {{
                    cppSelf->onThemeUpdated();
                    return Py_None;
                }}
                """
            ).format(prefix=function_prefix)
        )
    return "\n".join(functions)


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
    include_toast_factory=True,
    include_toast_update_factory=True,
    toast_factory_bookkeeping=(
        "Shiboken::Object::setParent(pyArgs[0], pyResult);"
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
    toast_factory = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_showToastForBinding(
                PyObject *self, PyObject *args)
            {{
                pyResult = showToastForBinding(cppArg0, cppArg1, cppArg2, cppArg3, cppArg4, cppArg5, cppArg6);
                {bookkeeping}
                return pyResult;
            }}
            """
        ).format(bookkeeping=toast_factory_bookkeeping or "")
        if include_toast_factory
        else ""
    )
    toast_update_factory = (
        textwrap.dedent(
            """
            static PyObject *Sbk_fluentqtModule_showOrUpdateToastForBinding(
                PyObject *self, PyObject *args)
            {{
                pyResult = showOrUpdateToastForBinding(cppArg0, cppArg1, cppArg2, cppArg3, cppArg4, cppArg5, cppArg6, cppArg7);
                {bookkeeping}
                return pyResult;
            }}
            """
        ).format(bookkeeping=toast_factory_bookkeeping or "")
        if include_toast_update_factory
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
        + toast_factory
        + toast_update_factory
    )


def fluent_namespace_source(
    selection_converter_count=1,
    unstable_collections_converter=False,
    leaked_menus_enum_helper=False,
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
    if leaked_menus_enum_helper:
        source += (
            "::fluent::menus_toolbars::qt_getEnumMetaObject(cppArg0);\n"
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


def field_source(include_theme_hook=False, **kwargs):
    source = scroll_view_source(**kwargs)
    source = source.replace(
        "fluent_scrolling_ScrollView",
        "fluent_layout_Field",
    ).replace("ScrollView::", "Field::")
    source = source.replace("setContentWidget", "setEditor")
    source = source.replace("contentWidget", "editor")
    source = source.replace("takeContentWidget", "takeEditor")
    if include_theme_hook:
        source += textwrap.dedent(
            """
            static PyObject *Sbk_fluent_layout_FieldFunc_onThemeUpdated(
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


def toast_source(
    public_bypass=None,
    internal_hook=None,
    present_bookkeeping=None,
    retain_action=True,
    action_bookkeeping=None,
    getter_bookkeeping=None,
):
    public_function = ""
    if public_bypass is not None:
        public_function = textwrap.dedent(
            """
            static PyObject *Sbk_fluent_status_info_ToastFunc_{0}(
                PyObject *self)
            {{
                return Py_None;
            }}
            """
        ).format(public_bypass)
    internal_function = ""
    if internal_hook is not None:
        internal_function = textwrap.dedent(
            """
            static PyObject *Sbk_fluent_status_info_ToastFunc_{0}(
                PyObject *self)
            {{
                return Py_None;
            }}

            void ToastWrapper::{0}() {{}}
            """
        ).format(internal_hook)
    action_keep = (
        "Shiboken::Object::keepReference(self, \"toast-action\", pyArg);"
        if retain_action
        else ""
    )
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_status_info_ToastFunc__present(
            PyObject *self, PyObject *pyArg)
        {{
            auto result = cppSelf->present(cppArg0);
            {present_bookkeeping}
            return PyBool_FromLong(result);
        }}

        static PyObject *Sbk_fluent_status_info_ToastFunc_setAction(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->setAction(cppArg0);
            {action_keep}
            {action_bookkeeping}
            return Py_None;
        }}

        static PyObject *Sbk_fluent_status_info_ToastFunc_action(
            PyObject *self)
        {{
            pyResult = cppSelf->action();
            {getter_bookkeeping}
            return pyResult;
        }}

        {public_function}
        {internal_function}
        """
    ).format(
        present_bookkeeping=present_bookkeeping or "",
        action_keep=action_keep,
        action_bookkeeping=action_bookkeeping or "",
        getter_bookkeeping=getter_bookkeeping or "",
        public_function=public_function,
        internal_function=internal_function,
    )


def tooltip_source(
    attach_bookkeeping=(
        "Shiboken::Object::setParent(pyArgs[0], pyResult);"
    ),
    retain_theme_source=True,
    theme_bookkeeping=None,
    expose_theme_hook=False,
):
    theme_keep = (
        "Shiboken::Object::keepReference(self, \"tooltip-theme\", pyArg);"
        if retain_theme_source
        else ""
    )
    theme_hook = ""
    if expose_theme_hook:
        theme_hook = textwrap.dedent(
            """
            static PyObject *Sbk_fluent_status_info_ToolTipFunc_onThemeUpdated(
                PyObject *self)
            {
                return Py_None;
            }
            void ToolTipWrapper::onThemeUpdated() {}
            """
        )
    return textwrap.dedent(
        """
        static PyObject *Sbk_fluent_status_info_ToolTipFunc_attach(
            PyObject *self, PyObject *args)
        {{
            pyResult = ToolTip::attach(cppArg0, cppArg1, cppArg2);
            {attach_bookkeeping}
            return pyResult;
        }}

        static PyObject *Sbk_fluent_status_info_ToolTipFunc_setThemeSource(
            PyObject *self, PyObject *pyArg)
        {{
            cppSelf->setThemeSource(cppArg0);
            {theme_keep}
            {theme_bookkeeping}
            return Py_None;
        }}

        {theme_hook}
        """
    ).format(
        attach_bookkeeping=attach_bookkeeping or "",
        theme_keep=theme_keep,
        theme_bookkeeping=theme_bookkeeping or "",
        theme_hook=theme_hook,
    )


class GeneratedContractVerifierTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.generated_dir = Path(self.temporary_directory.name)
        self.write_window()
        self.write_title_bar()
        self.write_namespace(1)
        self.write_module()
        self.write_fluent_namespace()
        self.write_scroll_view()
        self.write_drawer_view()
        self.write_popup()
        self.write_flyout()
        self.write_coach_mark()
        self.write_teaching_tip()
        self.write_dialog()
        self.write_content_dialog()
        self.write_combo_box()
        self.write_line_edit()
        self.write_auto_suggest_box()
        self.write_menu_buttons()
        self.write_fluent_menus()
        self.write_command_surfaces()
        self.write_date_time_pickers()
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
        self.write_field()
        self.write_info_bar()
        self.write_toast()
        self.write_tooltip()
        self.write_pips_pager()

    def tearDown(self):
        self.temporary_directory.cleanup()

    def write_window(
        self,
        limited_api_arguments=True,
        expose_result_pointer=False,
        split_override=True,
        title_bar_parent=True,
        expose_theme_hook=False,
    ):
        (self.generated_dir / WINDOW_WRAPPER).write_text(
            window_source(
                limited_api_arguments=limited_api_arguments,
                expose_result_pointer=expose_result_pointer,
                split_override=split_override,
                title_bar_parent=title_bar_parent,
                expose_theme_hook=expose_theme_hook,
            ),
            encoding="utf-8",
        )

    def write_title_bar(self, **kwargs):
        (self.generated_dir / TITLE_BAR_WRAPPER).write_text(
            title_bar_source(**kwargs),
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

    def write_drawer_view(self, **kwargs):
        (self.generated_dir / DRAWER_VIEW_WRAPPER).write_text(
            drawer_view_source(**kwargs),
            encoding="utf-8",
        )

    def write_popup(self, **kwargs):
        (self.generated_dir / POPUP_WRAPPER).write_text(
            popup_source(**kwargs),
            encoding="utf-8",
        )

    def write_flyout(self, **kwargs):
        (self.generated_dir / FLYOUT_WRAPPER).write_text(
            flyout_source(**kwargs),
            encoding="utf-8",
        )

    def write_coach_mark(self, **kwargs):
        (self.generated_dir / COACH_MARK_WRAPPER).write_text(
            coach_mark_source(**kwargs),
            encoding="utf-8",
        )

    def write_teaching_tip(self, **kwargs):
        (self.generated_dir / TEACHING_TIP_WRAPPER).write_text(
            teaching_tip_source(**kwargs),
            encoding="utf-8",
        )

    def reset_guidance_overlays(self):
        self.write_coach_mark()
        self.write_teaching_tip()

    def write_dialog(self, **kwargs):
        (self.generated_dir / DIALOG_WRAPPER).write_text(
            dialog_source(**kwargs),
            encoding="utf-8",
        )

    def write_content_dialog(self, **kwargs):
        (self.generated_dir / CONTENT_DIALOG_WRAPPER).write_text(
            content_dialog_source(**kwargs),
            encoding="utf-8",
        )

    def write_combo_box(self, **kwargs):
        (self.generated_dir / COMBO_BOX_WRAPPER).write_text(
            combo_box_source(**kwargs),
            encoding="utf-8",
        )

    def write_auto_suggest_box(self, **kwargs):
        (self.generated_dir / AUTO_SUGGEST_BOX_WRAPPER).write_text(
            auto_suggest_box_source(**kwargs),
            encoding="utf-8",
        )

    def write_line_edit(self, expose_theme_hook=False):
        source = "// no public LineEdit theme hook\n"
        if expose_theme_hook:
            source = (
                "static PyObject *"
                "Sbk_fluent_textfields_LineEditFunc_onThemeUpdated("
                "PyObject *self) { return Py_None; }\n"
            )
        (self.generated_dir / LINE_EDIT_WRAPPER).write_text(
            source,
            encoding="utf-8",
        )

    def write_menu_buttons(
        self,
        retain_drop_down=True,
        retain_split=True,
        drop_down_getter_operation="",
        split_getter_operation="",
    ):
        (self.generated_dir / DROP_DOWN_BUTTON_WRAPPER).write_text(
            menu_button_source(
                "DropDownButton",
                retain_menu=retain_drop_down,
                getter_operation=drop_down_getter_operation,
            ),
            encoding="utf-8",
        )
        (self.generated_dir / SPLIT_BUTTON_WRAPPER).write_text(
            menu_button_source(
                "SplitButton",
                retain_menu=retain_split,
                getter_operation=split_getter_operation,
            ),
            encoding="utf-8",
        )

    def write_fluent_menus(
        self,
        expose_menu_theme_hook=False,
        expose_item_theme_hook=False,
    ):
        (self.generated_dir / FLUENT_MENU_WRAPPER).write_text(
            fluent_menu_source(
                "FluentMenu",
                expose_theme_hook=expose_menu_theme_hook,
            ),
            encoding="utf-8",
        )
        (self.generated_dir / FLUENT_MENU_ITEM_WRAPPER).write_text(
            fluent_menu_source(
                "FluentMenuItem",
                expose_theme_hook=expose_item_theme_hook,
            ),
            encoding="utf-8",
        )

    def write_command_surfaces(
        self,
        command_bar_options=None,
        flyout_options=None,
        expose_menu_bar_theme_hook=False,
    ):
        command_bar_options = command_bar_options or {}
        flyout_options = flyout_options or {}
        command_bar_arguments = {"subclassable": True}
        command_bar_arguments.update(command_bar_options)
        flyout_arguments = {"subclassable": False}
        flyout_arguments.update(flyout_options)
        (self.generated_dir / COMMAND_BAR_WRAPPER).write_text(
            command_action_surface_source(
                "CommandBar",
                "syncCommandBarActionReference",
                **command_bar_arguments
            ),
            encoding="utf-8",
        )
        (self.generated_dir / COMMAND_BAR_FLYOUT_WRAPPER).write_text(
            command_action_surface_source(
                "CommandBarFlyout",
                "syncCommandBarFlyoutActionReference",
                **flyout_arguments
            ),
            encoding="utf-8",
        )
        (self.generated_dir / FLUENT_MENU_BAR_WRAPPER).write_text(
            fluent_menu_bar_source(
                expose_theme_hook=expose_menu_bar_theme_hook,
            ),
            encoding="utf-8",
        )

    def write_date_time_pickers(
        self,
        calendar_getter_operation="",
        theme_hook_class=None,
    ):
        for class_name, wrapper_name, lifecycle_methods in (
            (
                "CalendarDatePicker",
                CALENDAR_DATE_PICKER_WRAPPER,
                ("openCalendar", "closeCalendar"),
            ),
            (
                "DatePicker",
                DATE_PICKER_WRAPPER,
                ("openPicker", "closePicker"),
            ),
            (
                "TimePicker",
                TIME_PICKER_WRAPPER,
                ("openPicker", "closePicker"),
            ),
        ):
            (self.generated_dir / wrapper_name).write_text(
                date_time_picker_source(
                    class_name,
                    lifecycle_methods,
                    calendar_getter_operation=calendar_getter_operation,
                    expose_theme_hook=theme_hook_class == class_name,
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
        leaked_menus_enum_helper=False,
    ):
        (self.generated_dir / FLUENT_NAMESPACE_WRAPPER).write_text(
            fluent_namespace_source(
                selection_converter_count,
                unstable_collections_converter,
                leaked_menus_enum_helper,
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

    def write_field(self, include_theme_hook=False, **kwargs):
        (self.generated_dir / FIELD_WRAPPER).write_text(
            field_source(
                include_theme_hook=include_theme_hook,
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

    def write_toast(self, **kwargs):
        (self.generated_dir / TOAST_WRAPPER).write_text(
            toast_source(**kwargs),
            encoding="utf-8",
        )

    def write_tooltip(self, **kwargs):
        (self.generated_dir / TOOLTIP_WRAPPER).write_text(
            tooltip_source(**kwargs),
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

    def test_window_title_bar_requires_qt_child_parenting(self):
        self.write_window(title_bar_parent=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Qt-owned child lifetime", result.stderr)

    def test_window_and_title_bar_theme_hooks_are_rejected(self):
        for class_name in ("Window", "TitleBar"):
            with self.subTest(class_name=class_name):
                if class_name == "Window":
                    self.write_window(expose_theme_hook=True)
                else:
                    self.write_title_bar(expose_theme_hook=True)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "{0} exposes internal API".format(class_name),
                    result.stderr,
                )
                self.write_window()
                self.write_title_bar()

    def test_title_bar_content_requires_replace_child_contract(self):
        for options in (
            {"release_old_child": False},
            {"parent_new_child": False},
            {"getter_parent": False},
        ):
            with self.subTest(options=options):
                self.write_title_bar(**options)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("TitleBar::", result.stderr)
                self.write_title_bar()

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

    def test_drawer_view_public_ownership_bypass_is_rejected(self):
        self.write_drawer_view(public_setter=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("content ownership bypass", result.stderr)

    def test_drawer_view_adapter_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyArg);",
            "Shiboken::Object::getOwnership(pyArg);",
            'Shiboken::Object::keepReference(self, "content", pyArg);',
            "Shiboken::Object::setParent(self, pyArg);",
        ):
            with self.subTest(operation=operation):
                self.write_drawer_view(adapter_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("DrawerView ownership adapter", result.stderr)

    def test_drawer_view_getter_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
            'Shiboken::Object::keepReference(self, "content", pyResult);',
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_drawer_view(getter_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("DrawerView::contentWidget", result.stderr)

    def test_drawer_view_take_contract_is_enforced(self):
        self.write_drawer_view(transfer_taken=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Python ownership transfer", result.stderr)

        for operation in (
            'Shiboken::Object::keepReference(self, "content", pyResult);',
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_drawer_view(take_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("DrawerView::takeContentWidget", result.stderr)

    def test_popup_public_dependency_bypass_is_rejected(self):
        for method_name in (
            "setPosition",
            "setThemeSource",
            "addLightDismissPassthrough",
            "clearLightDismissPassthrough",
        ):
            with self.subTest(method_name=method_name):
                self.write_popup(public_bypass=method_name)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("dependency bypass", result.stderr)

    def test_popup_protected_overlay_hooks_are_rejected(self):
        for method_name in (
            "onThemeUpdated",
            "computePosition",
            "automaticPositionAnchor",
            "setFocusOnOpenEnabled",
        ):
            with self.subTest(method_name=method_name):
                self.write_popup(protected_hook=method_name)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("protected C++ overlay hook", result.stderr)

    def test_popup_dependency_adapter_bookkeeping_is_rejected(self):
        for adapter_name in (
            "_setPositionWithAnchor",
            "_setThemeSource",
            "_addLightDismissPassthrough",
            "_clearLightDismissPassthrough",
        ):
            for operation in (
                "Shiboken::Object::releaseOwnership(pyArg);",
                "Shiboken::Object::getOwnership(pyArg);",
                'Shiboken::Object::keepReference(self, "dependency", pyArg);',
                "Shiboken::Object::setParent(self, pyArg);",
            ):
                with self.subTest(
                    adapter_name=adapter_name,
                    operation=operation,
                ):
                    self.write_popup(
                        adapter_name=adapter_name,
                        adapter_operation=operation,
                    )
                    result = self.run_verifier()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(
                        "Popup {0} adapter".format(adapter_name),
                        result.stderr,
                    )

    def test_flyout_public_anchor_bypass_is_rejected(self):
        for method_name in ("setAnchor", "showAt"):
            with self.subTest(method_name=method_name):
                self.write_flyout(public_bypass=method_name)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("dependency bypass", result.stderr)

    def test_flyout_protected_placement_hooks_are_rejected(self):
        for method_name in ("computePosition", "automaticPositionAnchor"):
            with self.subTest(method_name=method_name):
                self.write_flyout(protected_hook=method_name)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("protected C++ placement hook", result.stderr)

    def test_flyout_anchor_adapter_bookkeeping_is_rejected(self):
        for adapter_name in ("_setAnchor", "_showAt"):
            for operation in (
                "Shiboken::Object::releaseOwnership(pyArg);",
                "Shiboken::Object::getOwnership(pyArg);",
                'Shiboken::Object::keepReference(self, "anchor", pyArg);',
                "Shiboken::Object::setParent(self, pyArg);",
            ):
                with self.subTest(
                    adapter_name=adapter_name,
                    operation=operation,
                ):
                    self.write_flyout(
                        adapter_name=adapter_name,
                        adapter_operation=operation,
                    )
                    result = self.run_verifier()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(
                        "Flyout {0} adapter".format(adapter_name),
                        result.stderr,
                    )

    def test_flyout_anchor_getter_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
            'Shiboken::Object::keepReference(self, "anchor", pyResult);',
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_flyout(getter_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Flyout::anchor", result.stderr)

    def test_flyout_anchor_getter_requires_native_call(self):
        self.write_flyout(include_anchor_call=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Flyout::anchor native call is missing", result.stderr)

    def test_guidance_overlay_dependency_bypasses_are_rejected(self):
        cases = (
            (self.write_coach_mark, "setTarget"),
            (self.write_teaching_tip, "setTarget"),
            (self.write_teaching_tip, "showAt"),
        )
        for writer, method_name in cases:
            with self.subTest(method_name=method_name):
                self.reset_guidance_overlays()
                writer(public_bypass=method_name)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("target dependency bypass", result.stderr)

    def test_guidance_overlay_protected_hooks_are_rejected(self):
        cases = (
            (self.write_coach_mark, "onThemeUpdated"),
            (self.write_teaching_tip, "onThemeUpdated"),
            (self.write_teaching_tip, "computePosition"),
            (self.write_teaching_tip, "automaticPositionAnchor"),
        )
        for writer, method_name in cases:
            with self.subTest(method_name=method_name):
                self.reset_guidance_overlays()
                writer(protected_hook=method_name)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("protected C++ overlay hook", result.stderr)

    def test_guidance_overlay_target_adapters_require_native_calls(self):
        cases = (
            (self.write_coach_mark, "_setTarget"),
            (self.write_teaching_tip, "_setTarget"),
            (self.write_teaching_tip, "_showAt"),
        )
        for writer, adapter_name in cases:
            with self.subTest(adapter_name=adapter_name):
                self.reset_guidance_overlays()
                writer(missing_adapter_call=adapter_name)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("adapter call", result.stderr)

    def test_guidance_overlay_adapter_bookkeeping_is_rejected(self):
        cases = (
            (self.write_coach_mark, "_setTarget"),
            (self.write_teaching_tip, "_setTarget"),
            (self.write_teaching_tip, "_showAt"),
        )
        for writer, adapter_name in cases:
            for operation in (
                "Shiboken::Object::releaseOwnership(pyArg);",
                "Shiboken::Object::getOwnership(pyArg);",
                'Shiboken::Object::keepReference(self, "target", pyArg);',
                "Shiboken::Object::setParent(self, pyArg);",
            ):
                with self.subTest(
                    adapter_name=adapter_name,
                    operation=operation,
                ):
                    self.reset_guidance_overlays()
                    writer(
                        adapter_name=adapter_name,
                        adapter_operation=operation,
                    )
                    result = self.run_verifier()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("adapter", result.stderr)

    def test_guidance_overlay_getters_require_native_calls(self):
        for writer in (self.write_coach_mark, self.write_teaching_tip):
            for getter_name in ("target", "contentHost"):
                with self.subTest(
                    writer=writer.__name__,
                    getter_name=getter_name,
                ):
                    self.reset_guidance_overlays()
                    writer(missing_getter_call=getter_name)
                    result = self.run_verifier()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("native call is missing", result.stderr)

    def test_guidance_overlay_getter_bookkeeping_is_rejected(self):
        for writer in (self.write_coach_mark, self.write_teaching_tip):
            for getter_name in ("target", "contentHost"):
                for operation in (
                    "Shiboken::Object::releaseOwnership(pyResult);",
                    "Shiboken::Object::getOwnership(pyResult);",
                    'Shiboken::Object::keepReference(self, "target", pyResult);',
                    "Shiboken::Object::setParent(self, pyResult);",
                ):
                    with self.subTest(
                        writer=writer.__name__,
                        getter_name=getter_name,
                        operation=operation,
                    ):
                        self.reset_guidance_overlays()
                        writer(
                            getter_name=getter_name,
                            getter_operation=operation,
                        )
                        result = self.run_verifier()
                        self.assertNotEqual(result.returncode, 0)
                        self.assertIn("::{0}".format(getter_name), result.stderr)

    def test_dialog_public_theme_source_bypass_is_rejected(self):
        self.write_dialog(public_bypass=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("QWidget dependency bypass", result.stderr)

    def test_dialog_protected_hooks_are_rejected(self):
        for method_name in (
            "onThemeUpdated",
            "isAnimating",
            "ownerWidget",
            "drawShadow",
        ):
            with self.subTest(method_name=method_name):
                self.write_dialog(protected_hook=method_name)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("protected C++ hook", result.stderr)

    def test_dialog_theme_source_adapter_contract_is_enforced(self):
        self.write_dialog(include_adapter_call=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("theme source adapter call", result.stderr)

        for operation in (
            "Shiboken::Object::releaseOwnership(pyArg);",
            "Shiboken::Object::getOwnership(pyArg);",
            'Shiboken::Object::keepReference(self, "source", pyArg);',
            "Shiboken::Object::setParent(self, pyArg);",
        ):
            with self.subTest(operation=operation):
                self.write_dialog(adapter_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Dialog theme source adapter", result.stderr)

    def test_content_dialog_public_content_bypass_is_rejected(self):
        self.write_content_dialog(public_bypass=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("content ownership bypass", result.stderr)

    def test_content_dialog_theme_hook_is_rejected(self):
        self.write_content_dialog(expose_theme_hook=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("protected C++ theme hook", result.stderr)

    def test_content_dialog_static_startup_is_rejected(self):
        self.write_content_dialog(unsafe_static_fields=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unsafe flattened static field startup", result.stderr)

    def test_content_dialog_adapter_contract_is_enforced(self):
        self.write_content_dialog(include_adapter_call=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("private content adapter call", result.stderr)

        for operation in (
            "Shiboken::Object::releaseOwnership(pyArg);",
            "Shiboken::Object::getOwnership(pyArg);",
            'Shiboken::Object::keepReference(self, "content", pyArg);',
            "Shiboken::Object::setParent(self, pyArg);",
        ):
            with self.subTest(operation=operation):
                self.write_content_dialog(adapter_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("ContentDialog content adapter", result.stderr)

    def test_content_dialog_getter_contract_is_enforced(self):
        self.write_content_dialog(include_getter_call=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("content native call is missing", result.stderr)

        for operation in (
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
            'Shiboken::Object::keepReference(self, "content", pyResult);',
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_content_dialog(getter_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("ContentDialog::content", result.stderr)

    def test_combo_box_internal_api_is_rejected(self):
        for internal_name in ("onThemeUpdated",):
            with self.subTest(internal_name=internal_name):
                self.write_combo_box(expose_internal=internal_name)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("ComboBox exposes", result.stderr)

    def test_combo_box_editor_parent_contract_is_required(self):
        self.write_combo_box(parent_editor=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Qt parent contract", result.stderr)

        self.write_combo_box(retain_editor=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("conflicting wrapper bookkeeping", result.stderr)

    def test_combo_box_model_retention_is_required(self):
        self.write_combo_box(retain_model=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("caller-owned retention", result.stderr)

    def test_combo_box_model_getter_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
            'Shiboken::Object::keepReference(self, "model", pyResult);',
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_combo_box(model_getter_operation=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("ComboBox::model", result.stderr)

    def test_combo_box_popup_native_fallback_is_required(self):
        self.write_combo_box(include_native_fallback=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("native fallback", result.stderr)

    def test_combo_box_internal_helper_wrapper_is_rejected(self):
        (self.generated_dir / COMBO_BOX_DELEGATE_WRAPPER).write_text(
            "// leaked internal ComboBox delegate\n",
            encoding="utf-8",
        )
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("internal helper wrapper", result.stderr)

    def test_auto_suggest_box_theme_hook_is_rejected(self):
        self.write_auto_suggest_box(expose_theme_hook=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("exposes internal API", result.stderr)

    def test_line_edit_inherited_theme_hook_is_rejected(self):
        self.write_line_edit(expose_theme_hook=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("inherited internal API", result.stderr)

    def test_auto_suggest_box_native_key_fallback_is_required(self):
        self.write_auto_suggest_box(include_native_fallback=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("keyPressEvent native fallback", result.stderr)

    def test_auto_suggest_box_string_list_conversion_is_required(self):
        for keyword in (
            "include_setter_conversion",
            "include_getter_conversion",
        ):
            with self.subTest(keyword=keyword):
                self.write_auto_suggest_box(**{keyword: False})
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("QStringList conversion", result.stderr)
                self.write_auto_suggest_box()

    def test_auto_suggest_box_internal_helpers_are_rejected(self):
        for internal_wrapper in AUTO_SUGGEST_INTERNAL_WRAPPERS:
            with self.subTest(internal_wrapper=internal_wrapper):
                internal_path = self.generated_dir / internal_wrapper
                internal_path.write_text(
                    "// leaked AutoSuggestBox implementation type\n",
                    encoding="utf-8",
                )
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("internal helper wrapper", result.stderr)
                internal_path.unlink()

    def test_menu_button_retention_is_required(self):
        for class_name, keyword in (
            ("DropDownButton", "retain_drop_down"),
            ("SplitButton", "retain_split"),
        ):
            with self.subTest(class_name=class_name):
                self.write_menu_buttons(**{keyword: False})
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("caller-owned retention", result.stderr)
                self.write_menu_buttons()

    def test_menu_button_getter_bookkeeping_is_rejected(self):
        operations = (
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
            'Shiboken::Object::keepReference(self, "menu", pyResult);',
            "Shiboken::Object::setParent(self, pyResult);",
        )
        for class_name, keyword in (
            ("DropDownButton", "drop_down_getter_operation"),
            ("SplitButton", "split_getter_operation"),
        ):
            for operation in operations:
                with self.subTest(class_name=class_name, operation=operation):
                    self.write_menu_buttons(**{keyword: operation})
                    result = self.run_verifier()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("{0}::menu".format(class_name), result.stderr)
                    self.write_menu_buttons()

    def test_fluent_menu_theme_hooks_are_rejected(self):
        for class_name, keyword in (
            ("FluentMenu", "expose_menu_theme_hook"),
            ("FluentMenuItem", "expose_item_theme_hook"),
        ):
            with self.subTest(class_name=class_name):
                self.write_fluent_menus(**{keyword: True})
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("exposes internal API", result.stderr)
                self.write_fluent_menus()

    def test_command_surface_action_sync_is_required(self):
        for class_name, options_name in (
            ("CommandBar", "command_bar_options"),
            ("CommandBarFlyout", "flyout_options"),
        ):
            with self.subTest(class_name=class_name):
                self.write_command_surfaces(
                    **{
                        options_name: {
                            "drop_sync_method": "addSecondaryAction"
                        }
                    }
                )
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "synchronize borrowed action references",
                    result.stderr,
                )
                self.write_command_surfaces()

    def test_command_surface_action_ownership_transfer_is_rejected(self):
        for class_name, options_name in (
            ("CommandBar", "command_bar_options"),
            ("CommandBarFlyout", "flyout_options"),
        ):
            with self.subTest(class_name=class_name):
                self.write_command_surfaces(
                    **{
                        options_name: {
                            "ownership_method": "addPrimaryAction"
                        }
                    }
                )
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("borrowed QAction ownership", result.stderr)
                self.write_command_surfaces()

    def test_command_surface_clear_must_release_after_native_call(self):
        self.write_command_surfaces(
            command_bar_options={"release_before_clear": True}
        )
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("before the native clear", result.stderr)

    def test_command_surface_helper_tracks_both_sections(self):
        self.write_command_surfaces(
            flyout_options={"complete_helper": False}
        )
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("secondary membership check", result.stderr)

    def test_command_surface_callable_add_action_glue_is_required(self):
        self.write_command_surfaces(
            command_bar_options={"callable_glue_count": 3}
        )
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("callable addAction glue", result.stderr)

    def test_command_surface_callable_actions_support_qt62(self):
        self.write_command_surfaces(
            flyout_options={"use_qt_convenience_actions": True}
        )
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Qt 6.2-compatible QAction construction", result.stderr)

    def test_menus_toolbars_q_enum_helpers_are_rejected(self):
        self.write_fluent_namespace(leaked_menus_enum_helper=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "invalid menus_toolbars Q_ENUM helper",
            result.stderr,
        )

    def test_command_bar_flyout_anchor_retention_is_required(self):
        self.write_command_surfaces(
            flyout_options={"retain_anchor": False}
        )
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("invocation-source retention", result.stderr)

    def test_command_bar_flyout_tolerates_qt62_basetype_flag(self):
        self.write_command_surfaces(
            flyout_options={"subclassable": True}
        )
        result = self.run_verifier()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_command_bar_flyout_must_not_generate_virtual_shell(self):
        self.write_command_surfaces(
            flyout_options={"wrapper_shell": True}
        )
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("generated a virtual wrapper shell", result.stderr)

    def test_command_surface_unresolved_placeholder_is_rejected(self):
        self.write_command_surfaces(
            flyout_options={"unresolved_placeholder": True}
        )
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unresolved Shiboken", result.stderr)

    def test_command_surface_theme_hooks_are_rejected(self):
        cases = (
            (
                "CommandBar",
                {"command_bar_options": {"expose_theme_hook": True}},
            ),
            (
                "CommandBarFlyout",
                {"flyout_options": {"expose_theme_hook": True}},
            ),
            (
                "FluentMenuBar",
                {"expose_menu_bar_theme_hook": True},
            ),
        )
        for class_name, options in cases:
            with self.subTest(class_name=class_name):
                self.write_command_surfaces(**options)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("exposes internal API", result.stderr)
                self.write_command_surfaces()

    def test_date_time_picker_theme_hooks_are_rejected(self):
        for class_name in (
            "CalendarDatePicker",
            "DatePicker",
            "TimePicker",
        ):
            with self.subTest(class_name=class_name):
                self.write_date_time_pickers(theme_hook_class=class_name)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("exposes internal API", result.stderr)
                self.write_date_time_pickers()

    def test_calendar_date_picker_getter_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
            'Shiboken::Object::keepReference(self, "calendar", pyResult);',
            "Shiboken::Object::setParent(self, pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_date_time_pickers(
                    calendar_getter_operation=operation
                )
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "CalendarDatePicker::calendarView",
                    result.stderr,
                )
                self.write_date_time_pickers()

    def test_date_time_picker_internal_helpers_are_rejected(self):
        for internal_wrapper in DATE_TIME_INTERNAL_WRAPPERS:
            with self.subTest(internal_wrapper=internal_wrapper):
                internal_path = self.generated_dir / internal_wrapper
                internal_path.write_text(
                    "// leaked date/time picker implementation type\n",
                    encoding="utf-8",
                )
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("internal helper wrapper", result.stderr)
                internal_path.unlink()

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

    def test_field_runtime_ownership_overload_is_rejected(self):
        self.write_field(runtime_overload=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Field::setEditor exposes", result.stderr)

    def test_field_parent_bookkeeping_is_rejected(self):
        self.write_field(use_parent=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Field::setEditor uses parent bookkeeping", result.stderr)

    def test_field_adapter_ownership_change_is_rejected(self):
        self.write_field(adapter_transfer_to_cpp=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Field private ownership adapter changes wrapper ownership",
            result.stderr,
        )

    def test_field_missing_take_ownership_is_rejected(self):
        self.write_field(transfer_to_python=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(
            "Field::takeEditor Python ownership transfer is missing",
            result.stderr,
        )

    def test_field_internal_theme_hook_is_rejected(self):
        self.write_field(include_theme_hook=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("internal theme refresh hook", result.stderr)

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

    def test_toast_public_lifecycle_bypasses_are_rejected(self):
        for bypass in ("present", "showToast", "showOrUpdateToast"):
            with self.subTest(bypass=bypass):
                self.write_toast(public_bypass=bypass)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Toast exposes native lifecycle bypass", result.stderr)

    def test_toast_internal_hooks_are_rejected(self):
        for hook in ("toastProgress", "setToastProgress", "onThemeUpdated"):
            with self.subTest(hook=hook):
                self.write_toast(internal_hook=hook)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Toast", result.stderr)
                self.assertIn(hook, result.stderr)

    def test_toast_present_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::setParent(self, pyArg);",
            "Shiboken::Object::keepReference(self, \"anchor\", pyArg);",
            "Shiboken::Object::releaseOwnership(pyArg);",
            "Shiboken::Object::getOwnership(pyArg);",
        ):
            with self.subTest(operation=operation):
                self.write_toast(present_bookkeeping=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Toast private present adapter", result.stderr)

    def test_toast_action_retention_is_required(self):
        self.write_toast(retain_action=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("borrowed QAction retention is missing", result.stderr)

    def test_toast_action_parent_or_ownership_change_is_rejected(self):
        for operation in (
            "Shiboken::Object::setParent(self, pyArg);",
            "Shiboken::Object::releaseOwnership(pyArg);",
            "Shiboken::Object::getOwnership(pyArg);",
        ):
            with self.subTest(operation=operation):
                self.write_toast(action_bookkeeping=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Toast::setAction changes", result.stderr)

    def test_toast_action_getter_bookkeeping_is_rejected(self):
        for operation in (
            "Shiboken::Object::setParent(self, pyResult);",
            "Shiboken::Object::keepReference(self, \"action\", pyResult);",
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_toast(getter_bookkeeping=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Toast::action", result.stderr)

    def test_toast_managed_factory_adapters_are_required(self):
        for option, adapter in (
            ("include_toast_factory", "showToastForBinding"),
            (
                "include_toast_update_factory",
                "showOrUpdateToastForBinding",
            ),
        ):
            with self.subTest(adapter=adapter):
                self.write_module(**{option: False})
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(adapter, result.stderr)

    def test_toast_managed_factory_host_parenting_is_required(self):
        for bookkeeping in (
            "",
            "Shiboken::Object::setParent(pyArgs[1], pyResult);",
        ):
            with self.subTest(bookkeeping=bookkeeping):
                self.write_module(
                    toast_factory_bookkeeping=bookkeeping
                )
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("actual-host parenting is missing", result.stderr)

    def test_toast_managed_factory_ownership_changes_are_rejected(self):
        for operation in (
            "Shiboken::Object::keepReference(self, \"toast\", pyResult);",
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_module(toast_factory_bookkeeping=(
                    "Shiboken::Object::setParent(pyArgs[0], pyResult);"
                    + operation
                ))
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("managed factory changes wrapper ownership", result.stderr)

    def test_tooltip_attach_target_parenting_is_required(self):
        for bookkeeping in (
            "",
            "Shiboken::Object::setParent(pyArgs[1], pyResult);",
        ):
            with self.subTest(bookkeeping=bookkeeping):
                self.write_tooltip(attach_bookkeeping=bookkeeping)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("target parenting is missing", result.stderr)

    def test_tooltip_attach_ownership_changes_are_rejected(self):
        for operation in (
            "Shiboken::Object::keepReference(self, \"tip\", pyResult);",
            "Shiboken::Object::releaseOwnership(pyResult);",
            "Shiboken::Object::getOwnership(pyResult);",
        ):
            with self.subTest(operation=operation):
                self.write_tooltip(attach_bookkeeping=(
                    "Shiboken::Object::setParent(pyArgs[0], pyResult);"
                    + operation
                ))
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("ToolTip::attach changes", result.stderr)

    def test_tooltip_theme_source_retention_is_required(self):
        self.write_tooltip(retain_theme_source=False)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("theme-source wrapper retention is missing", result.stderr)

    def test_tooltip_theme_source_parent_or_ownership_change_is_rejected(self):
        for operation in (
            "Shiboken::Object::setParent(self, pyArg);",
            "Shiboken::Object::releaseOwnership(pyArg);",
            "Shiboken::Object::getOwnership(pyArg);",
        ):
            with self.subTest(operation=operation):
                self.write_tooltip(theme_bookkeeping=operation)
                result = self.run_verifier()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("ToolTip::setThemeSource changes", result.stderr)

    def test_tooltip_theme_hook_is_rejected(self):
        self.write_tooltip(expose_theme_hook=True)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("ToolTip exposes internal API", result.stderr)

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
