# Signature Surface

Complete the product's primary surface as well as its window shell. The sections
below define the required composition, states, and input behavior.

A Mica window with two labeled log rows, an opaque white composer, and a filled
ComboBox in the pane header fails this gate. Rebuild that surface before
continuing.

Use the sections that match the primary object: a conversation, run, document,
queue, or other content. Apply the input section when the workflow needs one.
FluentQt has no Transcript widget; compose one from `ListView`, delegates,
`TextEdit`, `InfoBar`, and low-emphasis chrome.

## Finish the product object, not the shell

A signature surface is done only when all of these are true:

1. A stranger can name the primary object from the window with the logo
   hidden.
2. Zero, one, two, and many items each have an intentional composition.
3. Visible copy is user-facing. Protocol names stay in logs and models.
4. Filled controls on Mica are rare. Pane chrome is Subtle, not stickers.
5. When a primary input exists, it is an integrated dock unless it is itself
   the document. A monitor or read-only surface may correctly have no input.

Meet the backdrop and window-material requirements in
[Premium shell](premium-shell.md) as well as the requirements here.

## Choose the finish from the time model

| Time model | Signature finish | Supporting chrome |
| --- | --- | --- |
| Ordered run or conversation | Virtualized turn timeline + integrated composer | Session list, permission `InfoBar`/`ContentDialog` |
| Document or artifact | Dominant canvas; run details transient | Tabs, tree, compact command surface |
| Queue, monitor, or pipeline | Stage/object collection + live status | Transient step detail, not a chat column |
| Spatial or board | Viewport owns the window | Selection-driven inspector as drawer |

If the identity card names the run or conversation as the primary object, do
not demote it into a labeled log to avoid a “chat shell.” Finish the
transcript. If the identity card names an artifact, do not add a permanent
chat column.

Query `agent-run-workspace` when the run is the product;
`agent-workbench` when artifacts are the product and the run is secondary.

## Conversation and run timeline

Use `ListView` with a custom delegate and mixed row heights. Do not stack one
widget per turn.

```cpp
auto* turns = new fluent::collections::ListView(canvas);
turns->setBackgroundVisible(false);
turns->setBorderVisible(false);
turns->setUniformItemSizes(false);
turns->setWordWrap(true);
turns->setItemDelegate(new RunTurnDelegate(turns));
revealWindowMaterial(turns);
revealWindowMaterial(turns->viewport());
```

Paint these kinds. Do not invent extra chrome to fake a missing control.

| Kind | Visual grammar | Visible copy |
| --- | --- | --- |
| User | BodyStrong, max-width measure (quiet inset or right-ragged). No bubble required on Mica. | The user's words. Never `Request`. |
| Assistant | Body; `#` headings, lists, and fenced code actually styled. Name at most once per run. | The assistant's words. Never `Agent`. |
| Tool / step | Compact chip: Caption **name · status on one line**; optional Caption detail on a second line only when present; 8 px inner inset. Height is `2×inset + caption lineSpacing × lines` (+ 2 px detail gap). Not a chat bubble and not a Standard-control (32/40/56) card. | Display name and short result, not JSON keys. |
| Permission | `InfoBar` in the canvas, or `ContentDialog` when blocking. Not a log row. | Allow / Deny in product language. |
| System | One Caption line. | Status the user asked for, not transport events. |

Row metrics: 12 px pane inset, 8 px between lines in a turn, 12 to 16 px between
turns, 16 to 18 px glyphs in a 24 px slot, Body for content, Caption for meta.
Markdown headings (`#` / lists) and fenced code are part of the first slice
when the domain has them; plain wrapping Body with no hierarchy is a
wireframe. Distinguish user and assistant content through typography and text
width: BodyStrong with a max-width column, or a low-emphasis inset. Avoid filling
Mica with opaque message bubbles.

Variable-height delegates are a hard geometry gate:

- `sizeHint` **must equal** the painted height of that row. Clip `paint` to
  `option.rect`. Gallery default 32 to 36 uniform rows hide this defect.
- Invalidate height caches when body, status, width, or theme changes; call
  `doItemsLayout()` on `dataChanged` (and insert/reset when heights can change).
- A transparent `ListView` / `TreeView` that directly reveals composited Mica
  **must erase** the viewport with `CompositionMode_Source` before items paint.
  Gallery filled lists hide this; skipping it stacks glyphs through the next
  row. When the view sits on an intentionally painted parent,
  preserve that surface instead of clearing through it.

After append or stream, keep the reader's scroll anchor. Follow the end only
when the viewport was already at the end.

For `service-api` live sessions: resume a disk session **before** WebSocket
subscribe or send. Handshake and control frames are not chat turns.

Agent snapshot pitfalls (these fail a conversation GUI even when the shell
is Fluent):

- Snapshot `role=tool` often has `tool_call_id` and no `name`. Resolve the
  name from the previous assistant `tool_calls`. An empty title paints as
  `· done`.
- Map `[User denied tool execution]` / `[Error]…` to status `denied` /
  `error` and drop the bracket text. Do not paint protocol payloads or
  multiline dumps as the tool body.
- Do not cap a turn body at 8 wrapped lines. ATX headings need a larger
  role than Body (`BodyLargeStrong`); `---` is an HR, not a paragraph.
- User vs assistant must differ by measure and a quiet fill, not two
  left-aligned Body runs in the same color.
- Empty session `title` must not fall back to `cwd`. A filesystem path is
  not a run name.

## Composer and command dock

When the workflow has a primary input, place its composer on the owning
material. `TextEdit` paints its own control chrome, so an outer `Card` would
duplicate the frame.

```cpp
auto* dock = new QWidget(canvas);
revealWindowMaterial(dock);
auto* dockLayout = new QVBoxLayout(dock);
dockLayout->setContentsMargins(12, 8, 12, 12);
dockLayout->setSpacing(8);

auto* tools = new QHBoxLayout();
tools->setSpacing(4);
auto* attach = new fluent::basicinput::Button(dock);
attach->setFluentStyle(fluent::basicinput::Button::Subtle);
attach->setFluentSize(fluent::basicinput::Button::Small);
attach->setFluentLayout(fluent::basicinput::Button::IconOnly);

auto* edit = new fluent::textfields::TextEdit(dock);
edit->setMinVisibleLines(1);
edit->setMaxVisibleLines(6);
edit->setPlaceholderText(QStringLiteral("Message"));
const QFontMetrics bodyMetrics(edit->themeFont(Typography::FontRole::Body).toQFont());
edit->setLineHeight(qMax(bodyMetrics.lineSpacing(), bodyMetrics.height()));
edit->setTabChangesFocus(true);

auto* send = new fluent::basicinput::Button(QStringLiteral("Send"), dock);
send->setFluentStyle(fluent::basicinput::Button::Accent);
send->setFluentSize(fluent::basicinput::Button::Small);
send->setFocusPolicy(Qt::StrongFocus);
QWidget::setTabOrder(edit, send);
```

Rest height of the dock is about 44 to 56 px plus a compact tool row. One Accent
action. Stop/cancel replaces Send while a run is active; do not keep both as
equal Standard buttons.

Use `primary_input_treatment: independent-card` only when the input is the
document (mail compose, note body). Record `primary_input_reason`.

## Quiet chrome on material

Use the filled bezel of `ComboBox` in forms and settings. For a pane title on
Mica, use a Subtle switcher:

```cpp
auto* workspace = new fluent::basicinput::DropDownButton(
    QStringLiteral("Workspace"), header);
workspace->setFluentStyle(fluent::basicinput::Button::Subtle);
workspace->setFluentSize(fluent::basicinput::Button::Small);
```

The same rule applies to Standard `Button`, filled `Card`, and opaque
`LineEdit` used as window or pane chrome. Subtle Small, `CommandBar` with
`setBackgroundVisible(false)`, or typography-only headers are the default.

Duplicate product names in the title bar and the first pane header fail.
Keep one title.

## Sparse and empty canvas

`ListView::setPlaceholderText` is not an empty state.

| Cardinality | Required composition |
| --- | --- |
| 0 | Title + wrapped Body + one optional action, placed near the composer or optically centered. Not a blank mica field. |
| 1+ | Top-align turns with deliberate measure, hierarchy, and nearby context so remaining canvas reads as intentional. Do not vertically center or bottom-anchor a short transcript. |
| 8+ | Virtualized rows, last item fully readable, streaming follows the reader. |

Capture `empty` and `collection-density` against these compositions, not
against a vacant window.

## User-facing copy

Models may keep `role=user` / `type=tool_call`. The painted UI may not.

Reject these as visible labels: `Request`, `Agent`, `tool_call`, `function`,
`rpc`, `stdout`, `payload`, raw JSON keys, and duplicate window titles.
Permission chrome uses Allow / Deny, not protocol field names.

Internal-protocol copy is allowed only on an explicit ops/debug surface.
`visible_copy_register: developer-labeled` never passes a product GUI.

## Reject these unfinished surfaces

Correct and rebuild the signature surface before continuing when any of these appear:

- Two `Request` / `Agent` (or equivalent) labeled rows as the timeline
- A large opaque `Card` wrapping the composer on Mica
- A filled `ComboBox` as the pane header or workspace switcher
- One or two bare, undifferentiated rows with no intentional measure or nearby
  context, or a short transcript vertically centered/bottom-anchored in a tall canvas
- Empty state that is only placeholder text or blank material
- Markdown, code, or tool steps flattened to a single plain-text row when
  the domain has those objects
- Tool chips sized to `ControlHeight::Standard` (32) or 40/56 placeholders
  instead of Caption `lineSpacing` plus inset
- A composer `TextEdit` whose line slot is 32 px because `setLineHeight`
  was skipped
- Next-row text visible through or overlapping a transparent collection row
  (missing viewport erase or `sizeHint` ≠ paint)
- Stop, permission, and send presented as an equal-weight form row
- A generic navigation/session/chat/inspector skeleton copied onto a product
  whose primary object is something else

## Signature-surface acceptance gate

- `signature_finish` is `product`. `wireframe` never passes.
- `chrome_on_material` is `quiet`. `filled-stickers` never passes.
- `sparse_canvas_treatment` is `composed`. `dead-space` never passes.
- `primary_input_treatment` is `integrated-dock` or `none`, unless
  `independent-card` has a document-level `primary_input_reason`.
- `visible_copy_register` is `user-facing`.
- Light and Dark both show the same hierarchy: quiet material, designed
  turns or canvas, compact dock, Subtle pane chrome.
- The first full-window screenshot is compared with Gallery window chrome
  **and** with a named product/Gallery surface for the primary object. A
  Mica shell around a log is not Gallery-equivalent quality.
