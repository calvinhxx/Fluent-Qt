# Widget cursor audit

> **Status:** Historical record. Source and local runtime baseline for the [cursor design](pointer-cursors.md), at commit `5225939d`.

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Architecture](README.md) › Runtime contracts

[← Mouse cursors in Widgets](pointer-cursors.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [Overlay Behavior Contract →](overlay-behavior.md)
<!-- docs-nav:top:end -->

## Coverage

The [API catalog](../../site/api/catalog.json) lists 95 base-library components and four optional entries. Cross-checking the [installed headers](../../cmake/FluentQtInstallHeaders.cmake) identified three additional widget types. The table assigns each entry to its interaction owner. We also searched component implementations, private helpers, compatibility and platform code for direct cursor writes and for input paths such as native editors, delegated item views, gesture handlers and embedded surfaces. Sixteen source files under `src/` directly set or clear a widget cursor.

For each catalog entry and each additional widget, a C++ probe linked the locally built libraries on macOS arm64 with Qt 6.9.3. It recorded the effective widget cursor, whether the widget set it explicitly, and the cursors on constructed child buttons. For `SpatialItem`, which is a QObject, it attached a widget with an explicit crosshair through `SpatialView::addWidget()` and inspected the hosted widget. The probe used Qt's offscreen platform. The observations below distinguish construction, targeted interaction sequences and source inspection. Hidden children, unopened popups and lazy editors require separate checks for reachability.

| Catalog area | Components | Cursor owner and action |
|---|---|---|
| Basic input | Button, CompoundButton, DropDownButton, RepeatButton, SplitButton, ToggleButton, ToggleSplitButton | Standard button family. Set the arrow at an embedded field command's creation site. Keep standalone buttons and application-provided children independently customizable. |
| Basic input | CheckBox, RadioButton, HyperlinkButton, ToggleSwitch, RatingControl | Explicit defaults. Checkboxes and radio buttons use arrows. Links, toggles and editable ratings use hands. Read-only rating sets an arrow. No temporary state is needed for a fixed default. |
| Basic input | Slider, RangeSlider, FileDropZone | Sliders own their tracks and handles. The drop zone negotiates drop acceptance with Qt and has a separate browse button. Keep those interaction paths independent. |
| Basic input | ComboBox, MultiSelectComboBox, TreeSelect | Inspect shell, editable text, trailing actions and popup independently. Caller-supplied editors and trailing widgets retain their own cursors. TreeSelect constructs affected field buttons. |
| Basic input | ColorPicker | Spectrum and hue controls change shape during a press. Text fields have their own clear actions. Exercise interrupted gestures as well as field buttons. |
| Charts | ChartView, LineChart, AreaChart, BarChart, HorizontalBarChart, PieChart, DonutChart, ScatterChart, Sparkline | ChartView owns plot hit testing. Its data cursor and hover readout are chart state. Neither requires changing the operating-system pointer. |
| Collections | ListView, GridView, FlowView, TreeView, Timeline, DataGrid | Viewport, delegate, active editor and header are separate targets. Preserve Qt header resizing, custom delegates and drop feedback. DataGrid can create LineEdit and NumberBox editors during editing. |
| Collections | FileListView, FlipView, SplitView | Painted row actions write to the viewport, page navigation writes to its surface, and splitter handles write to the splitter. Each is a candidate for scoped hover or drag feedback. |
| Collections | DrawerView, StackView, Transfer | Child pages, lists and commands own their regions. Transfer constructs AutoSuggestBox fields on both sides. |
| Date and time | CalendarDatePicker, CalendarView, DatePicker, DateTimeRangePicker, TimePicker | Picker buttons, calendar cells, popup content and range editors own their targets. DateTimeRangePicker constructs affected field buttons. |
| Dialogs and flyouts | Dialog, ContentDialog | Dialog implements manual drag. ContentDialog inherits the handler and disables dragging by default. Track the accepted press explicitly. |
| Dialogs and flyouts | Popup, Flyout, TeachingTip, CoachMark | Overlay content, buttons, scrim and any pass-through target have separate input ownership. Respect the existing dismissal and input routing. |
| Layout | Accordion, Card, Divider, Expander, Field, ListDetailView | Use existing child or header targets. A divider is mouse-transparent. Field and detail hosts may contain application widgets with explicit cursors. |
| Layout | DataGridToolBar, PropertyPanel | Composed field actions share the editor fix. Exercise the filter field and property editors created on demand. |
| Menus and toolbars | Menu, MenuBar, CommandBar, CommandBarFlyout, CommandPalette | Qt menu actions and embedded buttons own their targets. CommandPalette constructs a LineEdit. Keep command selection, separators and search editing distinct. |
| Navigation | Breadcrumb, NavigationView, Pivot, SelectorBar, Stepper, TabView | Breadcrumbs, items and tabs have component-defined hit areas. Stepper composes buttons. TabStrip owns tab closing and reordering. Preserve the existing keyboard and mouse behavior. |
| Scrolling | AnnotatedScrollBar, ScrollBar, ScrollView | Markers, thumb and viewport already own their input. A content widget may specify its own cursor. |
| Scrolling | ImageViewer, PipsPager | ImageViewer has pan hover, active drag and separate zoom controls. PipsPager has painted navigation targets and resets its cursor on leave. |
| Status and info | Avatar, InfoBadge, ProgressBar, ProgressRing, Shimmer, SplashScreen, ToolTip | Presentation and child content use their existing cursor. Respect pointer-transparent decorations. |
| Status and info | InfoBar, Toast | Action and close buttons own their hits. Toast can pass through input when the configured surface has no interactive behavior. |
| Text fields | LineEdit, NumberBox, PasswordBox, AutoSuggestBox, TagInput | Field text uses the editor cursor. Clear, step, reveal, query and per-tag removal are library-owned commands. NumberBox also has a painted scrub handle. |
| Text fields | CodeBlock, Label, TextEdit | Selectable content belongs to the Qt text surface. Preserve selection and link handling; keep the copy button separate. |
| Windowing | TitleBar, Window | Caption commands, client-side edge zones and native move/resize have distinct owners. Overlay and top-level cursor mirroring is a specific window-chrome requirement. |
| Foundation | FontIcon | A glyph has no independent command. Follow the containing hit area. |
| Optional modules | ParticleBackdrop, SpatialView, SpatialItem, QmlView | Particle interaction, hosted widgets and Quick scenes retain their own input policies. Check both flat and projected Spatial content, and embedded and texture-backed QML. The latter forwards mouse events to another surface. |
| Additional installed widgets | OverlayScrim, StackContentHost, TabStrip | Respect scrim interception and pass-through, hosted pages, and painted tab hit areas respectively. |

## Findings to reproduce

| Evidence | Sequence and observation | First owner to change |
|---|---|---|
| Widget properties | NumberBox up/down, LineEdit clear, PasswordBox reveal and AutoSuggestBox query/clear all inherited I-beam. Adding one item to TagInput revealed a seventh distinct button, `TagInputRemoveButton`, also with I-beam. | Set a static arrow on those seven built-in button types at their creation sites. |
| Composition | Twelve catalog entries had at least one I-beam child button at construction, counting hidden children. In addition to the five text fields these were ColorPicker, TreeSelect, Transfer, DateTimeRangePicker, DataGridToolBar, PropertyPanel and CommandPalette. | Fix the source field and test one consumer. Dynamic DataGrid editors and editable ComboBox are additional source-level consumers to exercise. |
| State transition | Turning NumberBox scrubbing on then off replaced an explicit I-beam or caller-set crosshair with an arrow. | Preserve the editor's previous state. Disabling unused feedback should perform zero writes. |
| Synthetic input | With Dialog dragging disabled and an explicit closed-hand cursor, a mouse move carrying the left-button flag moved the dialog despite no preceding press. An unmatched release cleared the cursor. | Store an active drag in Dialog and end it on cancellation. A cursor shape cannot establish gesture ownership. |
| Synthetic input | After pressing the ColorPicker spectrum, sending `UngrabMouse` left its widget cursor blank. | End the picker's transient blank cursor on interruption. Verify the physical pointer on a native window. |
| State transitions | A SplitView with two panes lost a caller-set crosshair on handle width change. A fit-sized image in ImageViewer also replaced a caller-set crosshair with an arrow. | Make idle cleanup harmless and preserve caller changes. |
| Synthetic input | A client-side frame zone changed the host's crosshair to a resize cursor on enter and to an arrow on leave. | Keep platform mirroring, with independent restoration for the overlay and host. |
| Source inspection | FileListView writes a cursor on every pointer move. FlipView clears its hover flags on leave. PipsPager clears the widget cursor on leave. | Verify repeated moves, a stationary pointer over a changed target and caller cursors before migrating these paths. |

The original report concerns Windows 11 and Qt 6.2.4. This audit measured local widget state and selected event sequences. Native Windows, macOS and Linux pointer appearance, PySide6 entry points and WebAssembly canvas behavior remain platform acceptance work. [Issue 142](https://github.com/calvinhxx/Fluent-Qt/issues/142)

<!-- docs-nav:bottom:start -->
---
[← Mouse cursors in Widgets](pointer-cursors.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [Overlay Behavior Contract →](overlay-behavior.md)
<!-- docs-nav:bottom:end -->
