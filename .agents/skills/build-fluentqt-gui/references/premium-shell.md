# Premium Shell

Install FluentQt's Gallery-grade window material before composing product
content. A compiling window filled with opaque `QWidget` hosts is not a Fluent
application.

Use this recipe when the slice owns a top-level application window. Do not
apply it to a plugin pane or a surface embedded in a host-owned window. In that
case inherit the host's chrome, material, lifecycle, and unload rules.

## Default window material

`fluent::windowing::Window` defaults to `BackdropEffect::Mica`. Keep that
default. Gallery ships Mica as the quiet app background; Acrylic is for
transient surfaces that should sample more of the desktop.

```cpp
auto* window = new fluent::windowing::Window();
window->setAttribute(Qt::WA_DeleteOnClose);
window->setWindowTitle(QStringLiteral("Product"));
window->setCustomWindowChromeEnabled(true);
window->setCaptionButtonToolTips(
    QStringLiteral("Minimize"), QStringLiteral("Maximize"),
    QStringLiteral("Close"), QStringLiteral("Restore"));
window->setCaptionButtonAccessibleNames(
    QStringLiteral("Minimize"), QStringLiteral("Maximize"),
    QStringLiteral("Close"), QStringLiteral("Restore"));
window->setBackdropEffect(fluent::windowing::BackdropEffect::Mica);

auto* content = new QWidget(window);
revealWindowMaterial(content);
window->setContentWidget(content);
```

Install the theme before constructing the window:

```cpp
fluent::prepareHighDpiApplication();
QApplication app(argc, argv);
fluent::initializeResources();
app.setFont(Typography::fontStyle(Typography::FontRole::Body).toQFont());
fluent::UserTheme::apply();
```

## Respect a host-owned window

For a plugin, IDE panel, or other embedded surface, do not construct a second
`fluent::windowing::Window` merely to obtain Mica. Use the host-provided parent,
event loop, theme bridge, focus model, and teardown contract. Record
`window_backdrop: "host-owned"`, the host constraint in
`window_backdrop_reason`, `surface_fill_policy: "inherit-host"`, and a concrete
`surface_fill_reason` in visual evidence.

An embedded pane may still use transparent child hosts when the host supports
them. If the host requires an opaque surface, keep that surface and document
the reason instead of punching through it with `Qt::WA_TranslucentBackground`.

For an application-owned top-level window, call `setBackdropEffect(Mica)` even
though it is the default so the intent is visible in review. Use Acrylic only
for a recorded transient-window reason. Use Solid only when a host, capture
harness, or accessibility surface requires an opaque backing store, and record
that reason in the visual-evidence manifest as `window_backdrop_reason`.

## Reveal the window material

The window paints one native or UILib material. Chrome, pane gaps, and unused
canvas must show that material. Opaque cards and controls float on top.

```cpp
void revealWindowMaterial(QWidget* widget)
{
    if (!widget)
        return;
    widget->setAutoFillBackground(false);
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Window, Qt::transparent);
    palette.setColor(QPalette::Base, Qt::transparent);
    widget->setPalette(palette);
}
```

Apply this to hosts that are intended to reveal the owning material: content
hosts, split-pane gaps, quiet headers, empty-state canvas, and backgroundless
collection views. For such a `ListView` / `TreeView` / `GridView`:

```cpp
list->setBackgroundVisible(false);
list->setBorderVisible(false);
revealWindowMaterial(list);
revealWindowMaterial(list->viewport());
```

When a backgroundless collection sits on an intentionally painted parent
surface rather than directly on composited material, preserve the complete
viewport chain:

```cpp
list->setProperty("fluentPreserveParentSurface", true);
list->viewport()->setProperty("fluentPreserveParentSurface", true);
list->viewport()->setAttribute(Qt::WA_NoSystemBackground, true);
```

The property prevents FluentQt's composited-backdrop clear; the viewport
attribute prevents Qt from restoring its default base fill afterward. Apply
and remove all three together when the surface topology changes. Keep the
component background when the collection is itself a deliberate bounded
surface; transparency is not a quota.

Do **not** set `Qt::WA_TranslucentBackground` on descendant content widgets.
On the painted-Mica fallback that attribute punches a hole through the window
instead of revealing material.

Do **not** write a helper that `setAutoFillBackground(true)` and stamps
`bgCanvas` / `bgLayer` onto every host. That is the most common way generated
GUIs lose grain, vibrancy, and depth.

## Surface roles

Build hierarchy with these roles, in this order:

| Role | Use | Typical implementation |
| --- | --- | --- |
| Window material | Quiet large background | `Window` + Mica/Acrylic, gaps unfilled |
| Alternate layer | Persistent sidebar or inspector | Transparent pane on material, or one `Card` `LayerAlt` only when the pane must read as a slab |
| Canvas | Signature work surface | Transparent; typography and spacing create hierarchy |
| Card | One independent floating object | `fluent::layout::Card` for a document, module, or tool-step chip—not the composer or pane header |
| Overlay | Dialog, flyout, drawer, menu | Fluent overlay components; they own elevation and scrim |

Add a card or border only when the object is independent of the canvas.
Spacing and typography are the default separators. A card around every
section, nested rounded rectangles, and full-pane fills are visual defects.

The composer, pane header, and workspace switcher are not independent
objects. They stay on material as quiet chrome. Follow
[Signature surface](signature-surface.md) instead of wrapping them in
`Card::Layer` to satisfy a one-card quota.

## Density

Use one compact desktop density unless the product is touch-first:

| Region | Logical px |
| --- | --- |
| Title bar | 40–44; mixed chrome in a shared 24-high slot |
| Icon-only chrome | 16–18 icon in a 24–28 `Button::Small` Subtle slot |
| Text control / list row | 32–36 |
| Panel inset | 12 |
| Related controls | 8 |
| Separate sections | 12–16 |

At most one title, one body, and one caption role per surface. One accent
action per local decision region.

## Reject these first-render patterns

Stop and rebuild the shell when any of these appear:

- `setBackdropEffect(Solid)` without a recorded host/capture reason
- `setAutoFillBackground(true)` on the content host, split panes, headers,
  composer chrome, or item-view viewports
- A `QPalette` helper that paints `bgCanvas` / `bgLayer` onto every `QWidget`
- `Qt::WA_TranslucentBackground` on descendant content widgets
- Nested `Card`s, a card around every heading, or a `Card` wrapping the
  composer or pane header
- A filled `ComboBox` used as a pane title on Mica
- Raw `QLabel` / `QPushButton` / `QListWidget` for visible chrome
- Display typography or oversized Standard/Large buttons used to fill space
- A persistent navigation + session list + chat column + inspector copied from
  a previous unrelated GUI

## Premium-shell acceptance gate

- An application-owned top-level window requests Mica or Acrylic, and the
  effective backdrop is not Solid unless `window_backdrop_reason` is recorded.
- An embedded surface keeps the host-owned window and records the host material
  and surface constraints; it does not introduce a nested top-level shell.
- Pane gaps, title-bar rest areas, and unused canvas reveal window material in
  both Light and Dark.
- Cards are rare independent objects. The composer and pane chrome are not
  cards; collection views reveal material when they belong to the canvas and
  keep one deliberate background when they are a bounded surface.
- Overlays use Fluent dialog/flyout/drawer components, not restyled `QDialog`.
- The first screenshot is compared with Gallery window chrome on the same
  platform and theme; an unexplained downgrade to a flat gray slab fails.
