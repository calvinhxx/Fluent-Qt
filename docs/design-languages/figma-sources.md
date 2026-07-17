# Design Kit Sources and Verification

The Material 3 and macOS references in this folder were measured against the linked Community
design kits. The Fluent reference is grounded in the app's own `src/design/*.h` headers, with the
Windows UI kit as its original measurement source. This page records the exact sources and node
IDs so values can be re-verified without redistributing design-kit source layers or screenshots.

## Files

| Design system | Figma file | `fileKey` |
|---|---|---|
| Fluent (Windows) | [Windows UI kit (Community)](https://www.figma.com/design/qpecbg7hOfos9DcHWeKlfw/Windows-UI-kit--Community-?node-id=2434-129659) | `qpecbg7hOfos9DcHWeKlfw` |
| Material 3 (Google) | [Material 3 Design Kit (Community)](https://www.figma.com/design/sfn7GB1zXX6Lu8hfhYqhbA/Material-3-Design-Kit--Community-?node-id=49823-12141) | `sfn7GB1zXX6Lu8hfhYqhbA` |
| macOS 27 (Apple) | [macOS 27 UI Kit (Community)](https://www.figma.com/design/W0PjLoNXuQyLACYlAE3QKi/macOS-27--Community-?node-id=131-8996) | `W0PjLoNXuQyLACYlAE3QKi` |

> Fluent is the app's **own** design language: its canonical values are quoted from
> `src/design/*.h` in [fluent.md](fluent.md), not re-pulled from Figma each time. The Windows UI
> kit above is the original measurement source and visual grounding.

## MCP access

Use the **remote Figma MCP** (authenticated; `whoami` confirms the session). Tools:
`get_metadata`, `get_screenshot`, `get_variable_defs`, `get_design_context`.

- `get_metadata` with **no `nodeId`** lists only the *first* page. To get the **full page list**,
  call it with an obviously-invalid `nodeId` (e.g. `0:1`) — the error response enumerates every
  top-level page with its id. That is how the node IDs below were discovered.
- `get_variable_defs` needs a concrete **frame/section** node (a whole page/canvas returns
  "nothing selected"). It resolves variables to a single appearance — for these captures the
  macOS values came back in the **dark** appearance.
- `get_screenshot` returns a short-lived URL; download with `curl` then view. Whole component
  pages are large (M3 Buttons is 16933 px wide) so detail is coarse at `maxDimension`.

## Node IDs used

### Windows UI kit (`qpecbg7hOfos9DcHWeKlfw`)
| Page | nodeId | Page | nodeId |
|---|---|---|---|
| Cover | `2434:129659` | Text fields | `72491:280395` |
| Primitives | `238:0` | Shell | `32021:96` |
| Basic input | `72491:280391` | Navigation | `72491:280399` |

The Fluent token values quoted in [fluent.md](fluent.md) come from `src/design/ThemeColors.h`,
`CornerRadius.h`, and `Typography.h` (which seed `ThemeRegistry`); the pages above were the
original measurement source.

### Material 3 (`sfn7GB1zXX6Lu8hfhYqhbA`)
| Page | nodeId | Page | nodeId |
|---|---|---|---|
| Styles (color/type/elevation) | `49823:12141` | **Buttons** | `55141:14168` |
| └ Color Guidance section | `55343:13516` | Checkboxes | `55141:14173` |
| Shape | `58548:7093` | Radio button | `55141:14253` |
| Switch | `55141:14257` | Sliders | `55141:14255` |
| Text fields | `55141:14259` | Tabs | `55141:14258` |
| Menu | `55141:14250` | Search | `55141:14254` |
| Loading & progress | `55141:14252` | Badges | `55141:14167` |
| Snackbar | `55141:14256` | Date & time pickers | `55141:14175` |
| Dialogs | `55141:14176` | Tooltips | `55141:14261` |
| Toolbars | `58295:22726` | Sheets | `55141:14170` |
| Lists | `55141:14249` | | |

`get_variable_defs` on `55343:13516` returned the full `Schemes/*`, `M3/sys/dark/*`,
`M3/ref/*` token set quoted in [material-3.md](material-3.md).

The **Buttons** page (`55141:14168`) is the source for the Button-family ports (Button,
DropDownButton, ToggleButton, SplitButton/ToggleSplitButton, HyperlinkButton). The
dropdown/flyout menu surface comes from the **Menu** page (`55141:14250`).

### macOS 27 (`W0PjLoNXuQyLACYlAE3QKi`)
| Page | nodeId | Page | nodeId |
|---|---|---|---|
| Colors | `0:687` (frame `0:765`) | Buttons | `207:14487` |
| Materials | `483:8848` | Toggles (switch/checkbox/radio) | `207:14470` |
| Text Styles | `0:962` | Sliders and Dials | `207:14494` |
| Segmented Controls | `207:14492` | Text Fields | `207:14500` |
| Pop-up & Pull-down Buttons | `207:14484` | Search Fields | `207:14491` |

`get_variable_defs` on `0:765` returned the `Accents/*`, `Labels/*`, `Fills - Opaque/*` set
quoted in [macos.md](macos.md).

## Repository screenshot policy

Fluent-Qt does not commit or distribute screenshots or source layers from the
external design kits. The written specifications are independent implementation
notes and factual measurements; the source links and node IDs above provide the
audit trail. Project-owned Gallery screenshots may still be used to document
Fluent-Qt itself.

## Refreshing

To re-check a value, use `get_variable_defs(fileKey, nodeId)` or inspect the
linked frame using the IDs above. A maintainer with authorized access may use a
temporary local render for comparison, but it must not be committed or packaged.
If a page id drifts as a kit is updated, rediscover the page list and update the
measurement notes rather than adding a captured copy.
