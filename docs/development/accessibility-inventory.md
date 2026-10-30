# Accessibility Inventory

> **Status:** Living reference

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › API, policy, and writing

[← Accessibility Contract](accessibility-contract.md) · [Contents](../SUMMARY.md) · [Development index](README.md)
<!-- docs-nav:top:end -->

This is the review view of the machine-checked
[accessibility-inventory.json](accessibility-inventory.json). The inventory is
keyed by the canonical Gallery component catalog, so every public visible
component has one classification before release.

## Current baseline

| Classification | Meaning |
| --- | --- |
| Native | The Qt base class supplies the appropriate role, state, actions, and logical children. |
| Augmented | Native semantics are retained while FluentQt manages additional text or events. |
| Adapter | A private accessible interface represents custom-drawn or composite semantics. |
| Gap | No known inventory contract is open. |
| Not applicable | The component is presentation-only; semantics belong to its containing control. |

All inventoried gaps are closed. `covered` means
there is deterministic repository evidence for the component boundary; it is
not a claim of platform assistive-technology certification.

## Hierarchical selection

`TreeSelect` exposes a ComboBox role, selected full paths, expanded/read-only
state and a show-menu action. Its input has one pointer target; the trailing
chevron is decorative rather than a second button. It composes native text input, TagInput removal
buttons and the existing TreeView, retaining hierarchy and check-state semantics.
The source model stays caller-owned; its values are never edited by selection.
The empty-result message is caller-configurable through `setEmptyText`; the same
text is exposed by the visible empty-state label.
Up/Down browse, Enter selects, Escape dismisses and Tab exits without selecting.
IME preedit does not filter or activate rows. The `tree-select` project-team
sample shares its behavior across C++, PySide6 and WebAssembly. Automated event
checks do not certify physical input methods or native screen readers.

On the pinned WebAssembly backend, the runtime reuses bounded visible-row
proxies for lists and trees. Qt-owned DOM nodes retain listbox/option or tree/treeitem
roles, selection, disabled and check states, row positions and the current item.
Selection-model updates refresh these states; reset and dismissal clear stale
active-descendant references. Popup relations follow the explicit accessible
Controller relation or owning presentation model; unrelated navigation trees
never attach themselves to the focused input. Browser tests inspect Chromium's
accessibility tree for selected and disabled options, including late activation.
Keyboard and composition events continue through Qt's native editor bridge.
The active input is created independently of the tree popup, which is created
on first opening. Mode changes retain selected values without exposing a hidden
second editor. Model updates keep selection synchronous and batch tree refreshes.
For large trees, rapid typed queries are coalesced; keyboard navigation and
confirmation apply the latest query first. Programmatic search remains immediate.
Custom input fonts determine the minimum control height. Shared single-line
control metrics include the native editor's vertical inset and positive leading,
so 18, 28, and 36 pixel fonts keep matching editor and selector heights without
clipping the text.

## Tagged member input

`TagInput` extends the AutoSuggestBox editable-text interface and exposes each
selected value as a label with a dedicated native removal Button. Pointer clicks
on the label focus its tag without deleting it; only the 24-pixel close target
removes the value. Its accessible bounds match that close target. Keys distinguish equal labels. Empty
Backspace moves focus to the previous tag before deletion; Left/Right and Tab
preserve the native focus path. Space and Enter activate the focused removal
button without inserting query text. Read-only values expose labels without removal
actions. Adding, reordering or renaming values retains existing tag widgets and
accessible identities; read-only changes hide removal actions without rebuilding
the tags. Batch token resolution updates the selection once. Qt owns IME preedit
and replacement ranges; preedit does not submit a
value, navigate suggestions, or remove a tag. A focused tag forwards composition
to the native editor and preserves its cursor rectangle. A standalone Space text
commit activates removal only outside composition. Focus returns after
the native commit finishes, unless the application moved it elsewhere. The `tag-input` Gallery sample
uses the same control in C++, PySide6 and WebAssembly. Injected IME event tests
and browser semantic checks remain distinct from physical input-method and
screen-reader acceptance.

## Adaptive list and detail

`ListDetailView` is a presentation container with native QWidget children. At
widths below `compactThreshold` it exposes only the current pane; wide mode shows
both, mirrored in RTL. Content widgets keep their identity, selection, document,
and scroll state. If switching hides the focused control, focus returns to the
last usable control in the destination, or its first tab stop. The application
can observe that transfer without a transient focus visit outside the container.
It supplies accessible names and a back control; the Gallery file browser includes
the keyboard-focusable **Files** button. No transition animation is required.

The file browser includes modified dates in accessible row names. Its search,
new-file and save actions are named, and returning with **Files** restores list
focus. Draft status is expressed in text as well as a status dot. The embedded
`TextEdit` keeps its native editing surface when its frame is hidden and its
height is assigned by the parent layout; selection and undo stay native.

`TestListDetailView.cpp` covers threshold geometry, focus, child lifetime,
selection, scrolling, text selection, and undo through repeated switches. The
`list-detail-view` Gallery route exercises the same native component in C++,
PySide6, and WebAssembly. Native screen-reader acceptance remains a manual check.

## DataGrid filters and columns

`DataGridToolBar` composes named native inputs, multi-selection controls and
Fluent menus. Its column menu provides keyboard alternatives to header drag
reordering, exposes checked visibility/pinning state, and prevents hiding the
last visible column. The Columns trigger describes the visible column order and
pinned columns to assistive technology. `TestDataGridToolBar.cpp` covers search
semantics, repeated menu actions, borrowed-grid lifetime, filter notifications
and narrow/RTL layout. Toolbar controls share a 32 px height, growing together
for larger text, and keep their state when wrapping. Multi-select popups align
their visible card to the trigger's leading edge with an 8 px gap; RTL, upward
placement, and window-edge clamping are covered by geometry tests. DataGrid's
default numeric cell editor reuses NumberBox for int, uint, float and double,
preserves EditRole types and unchanged precision, and parses text before Tab or
focus-out commits. Every default editor suppresses the old cell content while
open; Escape leaves the model unchanged. NumberBox's inherited `frameVisible`
switch controls its whole surface. Numeric text and editors leave clearance for
the shared overlay scrollbar. The validation example starts with valid data and
uses a single error underline after a rejected edit.
Selected cells use the shared neutral collection surface; the leading accent
indicator and keyboard focus outline continue to distinguish selection and focus.
The browser regression also checks that committed numbers update Chromium's
accessibility tree. The private table compatibility adapter refreshes changed
visible cells when Qt's WASM backend omits the standard model-data notification;
native backends retain their existing notification path.

DataGrid's pinned surface shares the logical table selection and model. DataGrid rows and
MultiSelectComboBox triggers retain logical mouse focus but show the extra focus
outline only for keyboard navigation. Row outlines close at the visible column
edge, including pinned, hidden and RTL columns. Default wheel containment
covers both DataGrid surfaces, pixel/momentum scrolling and scrollbar boundaries;
a table with no scrollable range still allows its parent to scroll. Focused
tests cover these rules alongside hit testing and editing. The WebAssembly browser smoke
discovers current menu items on every reopening, checks their presence in
Chromium's accessibility tree, and verifies column changes through the live
Columns description. It covers command execution, Escape and outside-click
closures. Native screen-reader behavior still requires platform review.

Pinned editors retain Tab and Shift+Tab navigation across the whole table. The
main view owns row submission; the secondary surface forwards editor policy
without owning application delegates. Accessible visible-cell enumeration uses
the pinned surface after horizontal scrolling, and RTL scrollbar space remains
outside pinned cells.

Header dividers expose column boundaries for resizing; the sort arrow indicates
the active direction when a title is clicked. The Gallery starts in ascending
order and uses fixed-width numeric columns through the model's `FontRole`.
Default text editing reuses a frameless `LineEdit`: the editor owns text, caret
and selection, while the delegate paints a single focus underline without a
second cell-selection background. Tests cover plain editing surfaces in both
themes and pinned panes, alongside commit/cancel behavior.
Sorting also invalidates cached logical cell positions and sends a table-model
notification. The WebAssembly adapter hides retired virtual entries and
republishes the bounded visible children because Qt's browser bridge does not
handle that notification. Browser tests verify the exposed row order after
clicking the same header in both directions.

The pinned viewport also shares the main viewport's theme background painting,
including empty space below rows and transparent composition. A styled-ancestor
regression covers repeated global light/dark changes.
Pinned-column synchronization is inert when state is unchanged; painting must
settle without requesting more layout or frames. The fixed pane stays below
the border and scroll targets. Regressions also preserve selection, both scroll
positions and an unfinished edit through theme changes. Gallery's 3D theme
change invalidates navigation and content textures together with the material
so the next frame cannot combine old widget colors with a new background.
The horizontal track and thumb page size exclude the fixed pane, including
RTL and column resizing. The surface clips its header and frozen pane to the
same rounded boundary. Text editors reuse an inset Fluent LineEdit without a
clear action; the Gallery validation delegate retains that editor and geometry
instead of falling back to an unthemed Qt editor.

## Read-only code

`CodeBlock` exposes the existing Expander header, a named and focusable Copy
button, and selectable native Label text. The header can fold the block without
changing its code; copying does not toggle folding. Keyboard selection and copy
retain source whitespace, including in high contrast. Focused tests are in
`tests/components/textfields/TestCodeBlock.cpp`, including clipped rendering
against native Label selection and wrapping in both light and dark themes.
Focused and unfocused text use the same clipped rendering; native selection and
clipboard behavior remain available.
Its internal scroll viewport remains transparent through repeated theme changes,
so the surrounding CodeBlock surface remains visible behind highlighted text.
Native screen-reader navigation
through highlighted source still requires platform review.

## Field and form coordination

`PropertyPanel` generates the existing accessible Fluent editors and labels them
through Field. Search preserves editor identity and values; reset actions have
field-specific names and tooltips, and enter the tab order only while reset is
available. Reset actions align with the editor's center in inline, stacked, and
right-to-left layouts, including wrapped labels and validation messages. Their
reserved space keeps editors from moving when reset becomes available. Adaptive
layouts retain the same editors and label buddies. Numeric dragging retains keyboard stepping and precise
text entry through NumberBox's existing value interface. Color properties open
the shared ColorPicker in a Flyout; the swatch also has a readable color value.
The panel closes its color flyout when destroyed, including after overlay
reparenting. Modified-state comparisons respect color values and numeric roundoff.
Value assignment preserves distinct representable floating-point edits, including
adjacent large values on Qt 5; modified-state tolerance remains separate.
`TestPropertyPanel.cpp` covers editor names,
keyboard edits, read-only behavior and replacement lifetime; native screen-reader
and popup review remain platform-specific.

`Field` places its caption above or beside the editor, with a single helper/status position, while keeping
the hosted editor's native role, focus proxy and caller-owned value. Its
Validating state has informational text and no error glyph. `FormController` aggregates
errors in registration order and focuses the first visible, enabled invalid
editor after a failed submit. The connection Gallery sample uses the same
native controller through C++, PySide6 and WebAssembly. These contracts are
covered by `TestField.cpp` and `TestFormController.cpp`; native screen-reader
announcements remain a platform review boundary.

## Live Scene editing tools

The opt-in Python preview editor composes existing Fluent buttons, sliders,
toggle switches, combo boxes, and labels. Each parameter has an accessible
name. A named region combo box is the keyboard alternative to mouse picking;
`F8` toggles picking and `Esc` cancels it. Picking consumes the selection click
instead of activating the application control. Its outline is a transparent,
non-interactive development overlay. The tools panel scrolls at short window
heights, and comparison windows close independently of the scene.

The [preview editor tests](../../bindings/pyside6/gallery/tests/test_preview_editor.py)
cover selection, state retention, source-conflict protection, and comparison
lifetime. This development surface adds no public component classification or
platform screen-reader certification. See the
[region-editing workflow](gallery-preview-workflow.md#select-edit-and-compare-a-region).

## Cross-cutting motion and contrast contract

QMLPlus reactive composition preserves the accessible roles and actions of its
native controls. Managed motion follows MotionPolicy, while conditional states
apply enabled/value changes to the actual controls. The focused C++ QML+
examples supply accessible names for its inputs and controls; its focused
Gallery tests cover observable final state. This supporting infrastructure is
not a new visible component classification. See [QMLPlus](../architecture/qmlplus.md).

The component classifications above do not change when visual motion is
reduced or disabled. `MotionPolicy::Reduced` keeps only short finite
transitions and stops continuous motion; `MotionPolicy::Disabled` settles
finite transitions at their final state and stops continuous motion. Logical
open, selected, expanded, busy, value, focus, and action state therefore remain
observable without depending on an intermediate animation frame. Shared policy
contracts live in `tests/components/TestMotionPolicy.cpp`; focused component and
Gallery-shell contracts cover active-transition convergence and local animation
switches.

`TextEdit` animates only its wrapper height; the native editor remains the value,
selection, and focus surface. Reduced and Disabled modes preserve the same final
line-count geometry without requiring an intermediate frame. Focused contracts
cover focus retention, motion-policy timing, and final geometry in
`tests/components/textfields/TestTextEditMotion.cpp`; they are not a claim of
platform assistive-technology certification. Input-method preedit leaves the
wrapper height stable until composition commits, so candidate exploration does
not repeatedly reflow the surrounding form.

When the initial editor viewport is shorter than a fallback-font caret, height
growth restores the whole caret instead of retaining Qt's temporary scroll
offset. Later resizing preserves an ordinary user scroll position. These
contracts are covered in `tests/components/textfields/TestTextEdit.cpp`.

`TextEdit::appendPlainText()` allows programmatic updates in read-only mode
while preserving the native editor's cursor and selection. Appending follows
the tail only when the reader was already at the bottom; browsing earlier
records retains the scroll offset. C++ and PySide6 contract tests cover these
behaviors, and Gallery's `text-edit-log-append` sample provides named log,
append, and clear controls. This does not establish native screen-reader
announcement behavior.

`FluentElement::HighContrast` resolves a complete third semantic palette, so
controls continue to expose the same roles, state, actions, and logical child
trees while using opaque high-contrast foreground, background, focus, disabled,
and status colors. Native applications currently select this deterministic
theme explicitly; the WebAssembly host additionally maps browser
`forced-colors` state to it. Palette completeness, contrast ratios, local theme
overrides, and legacy theme-envelope compatibility are covered in
`tests/components/TestHighContrastTheme.cpp`. This repository evidence is not
a claim that native builds import or certify every operating-system custom
contrast scheme.

Custom global and per-element theme token overrides preserve the existing accessible
roles and actions. Typography and per-mode color resolution are covered by
`tests/components/TestThemeOverrides.cpp`; caller-defined colors still require
contrast review. See [custom themes](../design-languages/custom-themes.md).

`CheckBox` and `RadioButton` keep enabled, unchecked indicators visible with a softer
resting outline in Light/Dark and a stronger outline on hover. High Contrast retains
full outline opacity. Their focused tests check rendered visibility and the hover
contrast increase at 1x and 2x scale. Disabled and selected states retain their
existing treatment. These pixel checks supplement native Gallery review; they do
not certify contrast on every application background or custom palette.
`CheckBox` and `RadioButton` use only their Fluent keyboard focus cue; the platform adapter disables
the additional Cocoa focus ring. Tab and Space retain native checkbox behavior,
and mouse input removes the keyboard-only cue. C++ and PySide6 regression checks
cover this contract; the native documentation focus capture supplies visual evidence.
`RadioButton` reserves space around its indicator so the keyboard cue does not
overpaint the selected circle and fractional scaling does not clip its edge.
At an explicitly fixed legacy size, optional horizontal padding yields to the
indicator and label; keyboard focus uses the indicator outline when a surrounding
frame would overlap the content.
Focused rendering tests cover both text directions, custom circle sizes and
Light, Dark and High Contrast. Mouse, arrow-key and Space selection preserve
the native exclusive-group behavior.

`CheckBox` and `ComboBox` reserve their full painted widget bounds in layouts;
native optical margins cannot push their borders outside a parent with zero
padding. Both editable and non-editable combo boxes show the focus underline.
`RatingControl` leaves space between the star tips and the keyboard cue and
centers the stars when its row is taller than its content. Fixed legacy sizes
drop optional padding and use an underline for keyboard focus, preserving the
original leading hit target. `HyperlinkButton` shows keyboard focus by default
and clears that cue on mouse input. Its inherited `focusVisual` opt-in remains
available; the base Button default is unchanged.
`Slider` uses a single thumb outline for keyboard focus and updates its layout
hints when handle or track dimensions change. Its minimum size accommodates
the painted handle at both endpoints plus at least one handle's width of drag
travel. Focus and edge regression tests cover
Light/Dark/High Contrast, both text directions and 1x/1.25x/1.5x/2x rendering.
PySide6 exercises the same shared implementations.

The [WebAssembly runtime](../../platforms/webassembly/Runtime.cpp) keeps Qt's HTML
accessibility proxies readable and focusable without painting a second control
surface through the transparent desktop canvas. A stylesheet scoped to each
application Qt screen makes only identified proxy nodes transparent; it excludes
the screen-reader enable button and leaves the separate keyboard/IME input,
ARIA state, geometry, and event handlers unchanged. The proxy container clips
overflow without becoming a scroll container, so focusing an offscreen proxy
does not scroll the sibling Qt canvas out of view. Browser source-copy feedback
waits for the actual clipboard write result; a rejected write reports failure
and leaves the action available for retry. This presentation adaptation
does not repair upstream accessibility geometry updates or replace native
screen-reader and IME acceptance.

## Component contracts

`FontIcon` remains a non-focusable, mouse-transparent decoration with either
bundled icons or an explicit application-registered `fontFamily`. Both paths
retain theme and disabled text colors, explicit color overrides, rotation, and
the same size hints. Font selection does not create an action or an accessible
name from a private-use glyph; the caller supplies meaningful names and labels
on the containing control. `TestFontIcon.cpp` covers family notifications,
restoring the bundled mode, raw supplementary glyphs, and fractional/high DPI.

`CommandPalette` composes the native accessible search editor and model-backed
`ListView` rows inside a same-window `Popup`. Row names include the command group
and shortcut; enabled and checked states follow the borrowed `QAction`.
Up/Down select available commands, Enter executes once, and Escape restores
the invoking focus. Tab cycles between search and results; the search editor
provides an accessible keyboard description. Search does not replace the
`EditingCommandRouter` target. Optional backdrop blur is suppressed in High Contrast.
`TestCommandPalette.cpp` covers these semantics and Light/Dark, narrow and RTL
geometry. `TestCommandPaletteSamples.cpp` also covers the full Gallery surface,
editor focus restoration, resetting an empty editing sample, and modal dismissal
without activating Gallery navigation.

`Popup`, `Dialog`, and `DrawerView` share modal pointer blocking, background
shortcut suppression, focus containment, and topmost-overlay input routing.
Calendar triggers retain their normal press/release/click signals and toggle
open state once per activation. Their outside-dismissal exclusions do not
weaken modal background blocking or Escape handling; C++ and PySide6 regressions
cover both standalone and date-range triggers.
Their Gallery effect samples expose named blur/dim sliders and checkboxes for
modality, dimming, and outside dismissal. A background counter verifies that
visual effects and input blocking remain independent. `TestOverlayContract.cpp`
runs the same interaction contract against all three components, and
`TestGalleryOverlaySamples.cpp` checks their controls and full Gallery navigation.
Modal surfaces default to a subtle 4 px content blur unless explicitly configured;
non-modal surfaces remain clear by default. High Contrast suppresses blur without
weakening modal input containment.
Blur, dimming, and the surface share one transition and settle together when
motion is reduced or disabled. CommandPalette cancellation fades out; command
execution still restores focus before dispatch.
Native screen-reader and IME acceptance remain platform review work.

`Button` retains Qt's native role, keyboard actions, and accessible widget
bounds. Layouts use its whole painted widget rectangle, so declared spacing
separates adjacent Buttons, including dynamically added outputs. The focused
geometry contract in [TestButton.cpp](../../tests/components/basicinput/TestButton.cpp)
covers native-style layout insets and multiple Buttons.

Timeline exposes a read-only list with logical model rows, including caller
names, supporting text, timestamps and node status. Stepper exposes a page-tab
list whose native focus buttons share the same step interfaces, with selected,
disabled and focused states. Tab entry skips an unavailable current step.
Its press action emits a
navigation request; acceptance remains application-controlled. Shared logical
adapters retain full item text when visual captions elide. Focused contracts
are in `TestTimeline.cpp` and `TestStepper.cpp`; see the
[sequence controls contract](../architecture/sequence-controls.md).

CommandBar and CommandBarFlyout retain their toolbar/menu roles, action state
and focus contracts when embedded in a graphics proxy. Presented-window
validation and inverse anchor hit testing preserve reopen and dismissal
behavior; the focused command-bar tests cover these paths and ordinary 2D
behavior. These tests do not certify native screen-reader geometry for
transformed content.

Menu-bearing CommandBar and CommandBarFlyout presenters expose popup availability,
expanded/collapsed state, and Show Menu. Inline branches use the menu-button role;
overflow branches retain the menu-item role. The same QAction owns the caption,
check state, and enablement. `TestCommandSubmenus.cpp` covers these semantics,
three-level navigation, focus return, and borrowed-menu lifetime across all five
presentation locations. Pointer activation retains logical focus without a focus
outline. Tab, arrow-key navigation, and Escape focus return show the outline;
popup restoration preserves the input mode.

Menu-button expanded state follows a real menu. An empty `QMenu` can emit
`aboutToShow` without opening; DropDownButton and SplitButton restore their
collapsed state in that case, while still allowing actions to be populated
inside `aboutToShow`.

FluentMenu preserves an open submenu while the pointer crosses overlapping
popup shadows. Actual parent reentry, keyboard navigation, and action activation
retain Qt behavior; `TestMenu.cpp` covers the boundary event sequence and native
pointer traversal. The boundary test includes follow-up mouse moves and the
close timeout under both parent-reentry policies. These checks do not establish
native screen-reader acceptance.

The WebAssembly runtime republishes a menu's direct accessible children after
each opening. Qt can recreate the popup window while retaining action proxies
in the old browser container; standard Qt visibility events attach those same
proxies to the current window and refresh their names and geometry. Unopened
submenus are left alone. `wasm_command_submenus.py` and `wasm_data_grid.py` check
reopened items in both the DOM and Chromium accessibility tree. The C++ menu
and PySide6 binding tests cover repeated opening, action names, checked/disabled
states, hidden items and keyboard activation. These automated checks do not
replace VoiceOver or NVDA acceptance.

Resting checked FluentMenu items show their check glyph without a selection fill;
hover and keyboard selection retain the Fluent highlight. Command menu rows
keep their existing target sizes and keyboard focus outline. The menu surface
tests cover Light/Dark theme refresh and reopening, and cascade geometry tests
cover LTR, RTL, and screen-edge containment.

`QmlView` is the optional QWidget host under **Plugins → QmlBridge**. It delegates
input to its Quick scene; the application QML page supplies its own
Accessible names, roles, actions, and keyboard order through Qt Quick semantics.
Scene unloading immediately removes its old interactive surface from the host;
scene/engine deletion follows on the next event-loop turn. The caller-owned host
and context models are preserved. `QmlPlugin`, `QmlSession`, `QmlTheme`, and
`QmlTextureLayer` are nonvisual lifecycle, theme, and rendering infrastructure.
Fluent theme and motion values
are explicit QML bindings, not automatic restyling or platform assistive-technology
certification. See [QmlBridge](../architecture/qml-bridge.md) and its
[host contracts](../../tests/components/qmlbridge/TestQmlBridge.cpp).

Gallery's QmlView source cards use a named **Source file** combo box to select
the host, QML page, or build file. **Source detail** switches between the host's
key usage and full example; QML and build files show their complete source.
The copy action and keyboard text selection use the selected file's source.
These behaviors are covered by the
[Gallery source contracts](../../tests/gallery/TestGalleryQmlBridge.cpp).

Visible Quick widgets require direct composition outside ancestor graphics
effects. On supported OpenGL hosts, Gallery hides that native Quick child and
uses `QmlTextureLayer` to include the same QML scene in its projected panel,
sharing the shell's inverse input mapping. Unsupported composition or texture
failure restores native 2D without changing the 3D preference. Models and
surrounding native controls remain intact; motion and contrast policy still
apply. This rendering boundary does not change the host's accessibility
classification or certify projected screen-reader or IME behavior.

`SpatialView` is a C++ composition surface in the **Spatial** category. Its
native accessibility contract applies in **2D mode**: the same widgets, values,
names and ownership remain in a normal scrollable layout. Scene Tab/Escape,
reduced motion and high contrast select that layout. Projected proxy geometry
is not a platform screen-reader or IME contract. `SpatialItem` is a non-visual
QObject pose and finish handle; semantics belong to the hosted content. Optional
shadow and rim decoration has no input or accessibility target. Hover lift freezes
during pointer input and disappears in native 2D, reduced-motion and high-contrast
layouts, preserving the original controls and values.

`SpatialPlugin` and `SpatialSession` add nonvisual lifecycle infrastructure.
Deactivating a session returns its tracked views to flat raster/native 2D
presentation while retaining the same controls, values, ownership, and models.
Activation does not override reduced-motion or HighContrast policy and does
not automatically re-enable old views. This integration does not add an
accessibility target or extend projected screen-reader/IME acceptance.

Anchored overlays from an embedded Spatial card now resolve the actual host
window through the shared Qt presentation adapter. Chart readouts no longer
clip to the card; menus and attached tooltips stay native rather than becoming
child scene proxies. Existing focus policies, target relations and values are
unchanged. `TestFoundationContracts.cpp` and `TestSpatialView.cpp` cover
ownership, containment, movement and source hiding. This is not projected
screen-reader or IME acceptance.

Gallery provides controls, lists, calendar, navigation and chart compositions
with one mode switch in Settings > 3D Gallery. It controls the shell and every
Spatial example, including hidden and newly opened examples; values and models
remain intact. Spatial pages link to Settings and disclose additional component
combinations in a keyboard-accessible Expander. Navigation and
content share a finite transition into opposing Y rotations (left navigation) or
X rotations (top navigation). A single GL compositor preserves window material;
the title bar, native widget ownership and focus remain stable. Long-lens
perspective limits text distortion, with thin material surfaces and low-elevation
content cards; decoration has no input target. Resize settles
the transition. Sub-degree pointer following stops when settled and freezes during
held input, scrolling and popups. Startup completes the Splash and logo handoff
before starting 3D. Desktop Splash prepares only Home/Settings, not unvisited
demos; cold pages yield between samples and retain completed pages for reuse.
Splash reports completed page readiness rather than
a time-based percentage; failed pages are identified explicitly. Cached hidden
pages keep their continuous animations stopped. The first 2D-to-3D opt-in on
Qt 6.4+ desktop hosts prepares the native surface before display, preserving
the visible window, focus and restore geometry. Reduced/Disabled motion and
High Contrast restore native 2D.
Projected text uses bounded cubic reconstruction to reduce texture-sampling
softness, with color-range and premultiplied-alpha limits to avoid halos.
Stronger minification smoothly returns to footprint filtering to preserve thin
strokes. Native regressions check contrast, text coverage during subpixel motion,
and translucent edges; focus and hit geometry retain the original projection.
The low-DPI glyph adapter rasterizes unhinted text at the existing panel-cache density,
including Home's particle foreground. Hinted text retains its native pixel grid so
small-text contrast is preserved; the policy follows each shaped item's font.
Each Gallery surface retains at most 2 MiB of exact raster tiles, including a
fixed metadata charge per entry. The 192 MiB renderer plan reserves another
2 MiB for their GPU copies in addition to scratch space. Pixel content, geometry
and density determine reuse; text is never reshaped for a cache lookup. Leaving
3D or losing the GL context releases these images. Cache regressions preserve
mixed-script shaping, opacity and fractional coverage while checking eviction.
Perspective still differs from native 2D text. At DPR 2 and above, the OpenGL
Gallery uses up to 1.5 times native sampling density and native window output, reducing
paint-strip repetition while retaining detail through projection. Native GPU
regressions compare both flat and projected text with the original 2D rendering.
Material-to-Solid changes publish the opaque state and refresh the 3D caches
before the platform removes the old
material, preserving window identity and avoiding a transparent handoff. Window
startup prepares the effective theme background before native exposure; palette
and native background updates retain the window, geometry, alpha format and focus.
Hidden native-surface preparation also preserves the caller's requested geometry
when configuring the title bar, so first-show navigation and focus targets use the
intended window size. The Windows native window regression covers this ordering.
The low-DPI text adapter reports only its actual backend's paint capabilities,
so focused native editors do not request unsupported XOR caret drawing in 3D.
Automated input checks cover text fields, editable ComboBox and collection-cell
editors; injected IME commits do not replace a native IME candidate-window review.
AutoSuggestBox exposes busy state while asynchronous suggestions load, keeps the
editor linked to its suggestion list after overlay reparenting, and provides native
keyboard-focusable Retry and Load more actions. Tab enters that action; Shift+Tab
returns to the editor. Escape, outside press, hiding and disabling invalidate pending
requests. Compact status rows grow with wrapped descriptions in narrow popups,
keeping the Retry button separate from the text and reachable by keyboard.
Applications can override initial/paged loading, more-results and action captions;
visible labels and accessible button names use the same configured text. TagInput
inherits those overrides without changing application-owned selected labels.
Late results cannot reopen the dismissed popup. Component tests cover
request races, pagination retention and borrowed-model lifetime; the browser check
exercises loading, empty, retry, paging and repeated opening with Qt accessibility
proxies. The WebAssembly runtime republishes visible list cells through Qt events
after model changes and hides them on dismissal; it does not copy the whole model
into browser nodes. Actual assistive-technology announcements remain a manual check.
Unchanged hover repaints do not republish list semantics. Enabling accessibility
after widgets are already open explicitly refreshes their current presentation.
Synchronous suggestions also honor focus loss and Escape dismissal until new input.

The AutoSuggestBox Gallery check also verifies visible unselected text in Light
and Dark GPU frames. LineEdit finishes its frame painter before native text
painting, preserving text contrast when widgets render into the projected cache.
Gallery keeps the 3D preference separate from runtime availability. Without acceleration,
its Settings switch is off and disabled with an accessible explanation, and the shell
restores native 2D widgets in both the shell and the Spatial examples. Tab/Escape
from a scene also restores the shared 2D mode for native input.
Spatial navigation and the Settings switch have matching InfoBadge indicators:
neutral while checking or available, critical after runtime fallback. Their hover
tooltips and accessible descriptions explain GPU requirements or the fallback
reason; selecting 2D or reduced motion does not report a hardware failure.
The fallback badge remains enabled beside the disabled switch.
ParticleBackdrop is listed under **Plugins → Particles**. Moving its source and
build ownership does not change its decorative, non-focusable role or child
controls' semantics. ParticlesSession pauses tracked widgets on deactivation;
reduced motion, High Contrast, hidden/clipped surfaces and inactive windows
retain the existing animation policy. See the [module contract](../architecture/particles.md).

Particle acceleration follows the existing 3D Gallery control; there is no
second renderer preference. The component defaults to CPU and requires an
explicit request plus a capable host. 2D and fallback restore CPU painting.
The separate Home particle effects switch continues to control visibility and motion.
Pointer hits and drags are mapped back from the displayed panels; keyboard and
IME input still reach the focused native control. Projected shell geometry is not
a platform screen-reader geometry contract; use 2D for assistive technology.
Anchored overlays and tooltips follow the presented target bounds through scrolling,
panel motion and return to 2D. First-level native menus map before screen containment;
submenus remain in native coordinates, while dialogs and drawers retain window-relative
placement. Existing focus, modal blocking and light-dismiss contracts are unchanged.
Text editing and dropdowns in the local Spatial examples
remain outside the scene. The pose workbench hosts its parameter sliders in a
separate projected card, with a stable pose while adjusting the preview, responsive
placement. The view workbench keeps camera/zoom sliders and its follow switch
outside the projection. Scene controls are disabled
in 2D and their values are retained when switching back. The item workbench limits
its controls to surface intensity, rotation and depth, with native input in 2D.
Spatial source blocks offer a keyboard-accessible SelectorBar for the focused
excerpt and full example; Copy uses the currently displayed source.
Top mode hides the unavailable title-bar menu button and reclaims its space.
The Gallery's Top navigation labels Home, Settings and the active category;
icon-only categories retain accessible names and tooltips. Narrow layouts keep
the active category visible and expose the remaining routes through More.
The 3D shell captures navigation and content independently, so an overlapping
compact drawer preserves complete rows, projected hit targets and light dismiss.
Intro keeps its modal scrim and CoachMark above the compositor, with matching
projected anchors in Left and Top layouts. Pointer following pauses during the
tour; finishing restores window chrome and normal input. The scrim hides as soon
as its fade completes, including Disabled motion, so deferred destruction cannot
leave an invisible input barrier. Settings update status
uses the description column beside the action and wraps in narrow layouts.
Toggle tracks and labels, checkbox clicks,
slider dragging, list/tree selection and expansion, ratings, date selection and tabs have projected-input
regressions. Native GPU checks cover nested opacity above and below paint-strip
boundaries and StackView's SlideFade/ScaleFade start and intermediate frames.
The C++ and Python Gallery compositors preserve the same effect coordinates;
full-opacity StackView and SplashScreen frames bypass unnecessary effects.
`SpatialView` releases the OpenGL viewport while hidden or fully clipped, and
uses Raster under an ancestor graphics effect without changing the requested
render mode. Its private Qt proxy adapter refreshes cached pixels when the
embedded widget requests a repaint, while unchanged content keeps using the cache;
hidden construction defers GPU initialization so startup prewarm preserves the
native window surface. Desktop Gallery checks OpenGL before creating its GPU
child; an unavailable driver retains native 2D controls and the Settings reason.
The original Home Hero is unchanged. See the [composition guide](../architecture/spatial-view.md),
[core tests](../../tests/components/spatial/TestSpatialView.cpp) and
[Gallery tests](../../tests/gallery/TestGallerySpatial.cpp). Native animation
and assistive-technology acceptance remain human-required.

Private adapters preserve caller-owned content and the existing public APIs.
The table lists the semantic boundary and its focused regression source.

Toast sizes short messages to their content and wraps complete titles and messages
within the host. Actions sit below the message, wrap long captions, and retain
native Button keyboard semantics. The optional close button keeps its accessible
name, keyboard focus, and 24-pixel target at the top trailing corner. Semantic
icons, announcements, dismissal reasons, and
reduced-motion behavior remain available in both compact and detailed layouts.

| Components | Contract | Tests |
| --- | --- | --- |
| CalendarView | Day, month, and year tables; locale-aware names; partially visible scroll rows with clipped hit targets and coalesced end-of-gesture resets; paging, selection, range, focus, and no-op events | [CalendarView](../../tests/components/date_time/TestCalendarView.cpp) |
| DateTimeRangePicker | Compact combo-box entry with expanded state and open actions; nullable values, range-calendar selected cells, compact endpoint calendars, inline time inputs that retain the range locale across overlay attachment, locale-aware presets and default labels with Simplified/Traditional Chinese fallback and application-translation refresh, embedded clear actions, and shared range validation | [Date/time range](../../tests/components/date_time/TestDateTimeRangePicker.cpp) |
| Breadcrumb, Pivot, SelectorBar, TabView, PipsPager | Ordered logical items, selection, focus, and press actions without per-item widgets; TabView add/close/reorder and all PipsPager pages | [Navigation](../../tests/components/navigation/TestNavigationAccessibility.cpp) |
| ToggleSwitch, RatingControl, NumberBox, ProgressBar, ProgressRing | Toggle and bounded value actions; NumberBox editable text, selection and invalid state; determinate values and observable busy state | [Values](../../tests/components/TestValueAccessibility.cpp) |
| SplitButton, ToggleSplitButton | Separate primary and menu actions; real menu state; Space, Alt+Down, and F4; menu replacement and destruction | [Split actions](../../tests/components/basicinput/TestSplitButtonAccessibility.cpp) |
| Popup, Flyout, TeachingTip, CoachMark | Pane/help-balloon roles, real child trees, target relations, logical open/modal state, dismissal, focus and announcements | [Transient surfaces](../../tests/components/dialogs_flyouts/TestTransientAccessibility.cpp) |
| ColorPicker, DatePicker, TimePicker, AnnotatedScrollBar, AutoSuggestBox | Color/value controls, pending picker values, authored jump links, autocomplete relations and editor focus | [Complex inputs](../../tests/components/TestComplexInputAccessibility.cpp) |
| DropDownButton, DrawerView, ToolTip | Menu actions, drawer content and focus return, tooltip text/target relations and logical visibility | [Auxiliary surfaces](../../tests/components/TestAuxiliarySurfaceAccessibility.cpp) |
| FlipView, SplitView | Ordered pages and navigation; native pane subtrees and keyboard-operable splitter grips with value bounds | [Collections](../../tests/components/TestCollectionSurfaceAccessibility.cpp) |
| HyperlinkButton, InfoBar, Shimmer | Link/visited state, notification severity and dismissal, loading/busy state independent of animation | [Presentation](../../tests/components/TestSemanticPresentationAccessibility.cpp) |
| SplashScreen | Covered-host input and focus restoration, busy state and progress; cached painting preserves live child controls and motion-policy behavior | [SplashScreen](../../tests/components/status_info/TestSplashScreen.cpp) |
| MultiSelectComboBox | Button-menu root, selected labels, expanded state, popup relation and named trigger/search/list | [Multi-selection](../../tests/components/basicinput/TestMultiSelectComboBox.cpp) |
| ComboBox | Explicit fonts reach the field, editor and dropdown; rows grow with text. The popup list takes focus; arrows stage selection, Enter/Return commits once and Escape cancels with focus return. Disabled items and an unselected placeholder cannot activate | [ComboBox](../../tests/components/basicinput/TestComboBox.cpp) |

`ToggleSwitch` keeps a minimum interactive height of 24 logical pixels.
`visualScale` changes its graphics while the accessible rectangle continues to
cover the whole widget; the default scale retains the 40 × 20 Fluent track.

Each family checks effective-change events and no-op silence. New or changed
visible components need an inventory classification and a focused contract
before release. Native screen-reader acceptance remains a separate gate in the
[technical debt roadmap](technical-debt-roadmap.md#td-4-acceptance-boundary).

## Validation

```bash
python3 tools/quality/validate_accessibility_inventory.py --project-root .
ctest --preset vcpkg-osx -R 'AccessibilityInventory|CalendarViewTest\.Contract_Accessibility|NavigationAccessibilityTest\.Contract_Accessibility|ValueAccessibilityTest\.Contract_Accessibility|SplitButtonAccessibilityTest\.Contract_Accessibility|TransientAccessibilityTest\.Contract_Accessibility|ComplexInputAccessibilityTest\.Contract_Accessibility|AuxiliarySurfaceAccessibilityTest\.Contract_Accessibility|CollectionSurfaceAccessibilityTest\.Contract_Accessibility|SemanticPresentationAccessibilityTest\.Contract_Accessibility' --output-on-failure
```

When Python 3.10+ is available, CTest registers
`AccessibilityInventory.Contract_Complete` in the `ci_fast`, `ci_full`,
`contract`, and `local_full` labels. The validator rejects missing, duplicate,
or unknown component IDs, invalid classifications, missing evidence paths, and
open gaps without a next gate.

## Charts

`ChartView` and its eight dedicated chart subclasses expose a Chart role, a caller-owned name/description, and a raw-data
cursor with value-change events and next/previous actions. Arrow keys navigate
original rows, including rows omitted from the visual projection. Attach a Qt
table view to the same `ChartModel` when a tabular alternative is needed.
The outer focus ring is visible for Tab or keyboard interaction; mouse clicks
hide it while retaining focus and the selected data point.
Readouts reuse a passive `Popup` with `Label` children. Pointer exit and focus
loss dismiss the readout while preserving selection and the accessible value;
keyboard navigation shows it again without moving focus into the popup.
Escape resets an explicit Cartesian X range once; otherwise it propagates to
the parent, preserving dialog close policies. Pie and Donut always propagate
Escape because their rendering ignores X ranges.


FileDropZone retains a native browse Button as its single tab stop and exposes
full application validation feedback. FileListView exposes complete row text,
status, and guarded remove/retry actions through a private accessible adapter;
its model remains caller-owned. Their focused owner tests cover these boundaries.
FileListView keeps status beside its supporting text and retains 32-pixel action
targets in compact rows. Long text still wraps fully, and keyboard focus remains
visible independently of hover.

<!-- docs-nav:bottom:start -->
---
[← Accessibility Contract](accessibility-contract.md) · [Contents](../SUMMARY.md) · [Development index](README.md)
<!-- docs-nav:bottom:end -->
