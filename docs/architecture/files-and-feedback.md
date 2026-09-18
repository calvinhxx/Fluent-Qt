# Files and feedback

> **Status:** Accepted contract

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Architecture](README.md) › Runtime contracts

[← Charts](charts.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md)
<!-- docs-nav:top:end -->

`FileDropZone` receives files, `FileListView` presents a caller-owned model, and
`Toast` acknowledges an action or offers recovery. These components can be used
independently. None starts a transfer, reads file contents, or owns an upload
queue. They use Qt Widgets and add no dependency.

## File entry

Include `<FluentQt/BasicInput.h>` and use `fluent::basicinput::FileDropZone`.
The embedded Fluent Button is the single keyboard tab stop. Activating it emits
`browseRequested()`; the application opens its asynchronous file picker.
Dropping local URLs emits `filesDropped(const QList<QUrl>&)` in their original
order. Mixed local and remote URLs are rejected as a whole.

`title`, `description`, `hintText`, and `browseText` contain application copy.
Set `errorMessage` after application validation and clear it explicitly when
appropriate. Dropping another file does not silently clear the error. File
types, sizes, permissions, duplicates, and directories are application policy.
The Gallery's 20 MB limit and sample formats are illustrative.

Browser file pickers return file contents rather than usable local file URLs.
The Web Gallery connects `browseRequested()` to Qt's asynchronous browser file
picker, selecting one file per invocation; the desktop Gallery allows multiple
selection. Browser OS-file drag support is a separate platform boundary.

## File presentation

Include `<FluentQt/Collections.h>` and use `fluent::collections::FileListView`.
It reuses `ListView` scrolling and accepts a caller-owned `QAbstractItemModel`.
The view owns its file-row delegate. Use one model with several views or a proxy
model without copying the file queue into a widget.

| Model role | Value |
| --- | --- |
| `Qt::DisplayRole` | Complete file name |
| `MetadataRole` | Application-supplied size, status, or recovery text |
| `StatusRole` | `Ready`, `Uploading`, or `Rejected` |
| `ProgressRole` | Transfer fraction from 0 to 1 |
| `RemovableRole` | Whether removal is available; defaults to true |
| `RetryableRole` | Whether retry is available; defaults to false |

Names and metadata wrap. Rows are painted by a delegate, without allocating
widgets for each file. Progress changes invalidate visible content without
rebuilding the model. Animations are finite and limited to visible rows.
Status appears beside the supporting text, leaving the trailing edge for file
actions. Compact rows grow for long names; remove and retry retain 32-pixel
targets.

`removeRequested(index)` and `retryRequested(index)` ask the application to
perform an action; the view never removes or retries a file itself. Handle these
signals against the same model that supplied the index. Delete or Backspace
requests removal; Ctrl+R (Cmd+R on macOS with Qt's default modifier mapping)
requests retry for the current row when available.

## Actionable Toast

Include `<FluentQt/StatusInfo.h>` and use `fluent::status_info::Toast`.
Short acknowledgements size to their content with semibold text, normally within
220–380 logical pixels and capped by the available host width. Height grows when
content wraps. Detailed cards use a separate title, supporting text, and an
optional borrowed `QAction` below the message. Text and long action captions
wrap within the host width. Both
layouts use an opaque neutral surface and a 16-pixel status icon in a 28-pixel
semantic marker. `setClosable(true)` adds a 24-pixel close button with a 12-pixel
icon, inset 8 pixels from the top trailing corner (mirrored for RTL).
`DismissReason::CloseButton` distinguishes explicit dismissal from timeout.
The default remains non-closable and preserves pointer pass-through when no
interactive option is enabled.

Entry uses a short fade with 8 pixels of travel; exit fades in place. Stack
repositioning remains interruptible, including when notifications arrive or close
during a transition.
Content measurement and the cached shadow are not rebuilt on each animation
frame. `MotionPolicy::Reduced` removes travel and shortens fades, and `Disabled`
settles immediately. The file components follow the same policy; hidden or
inactive surfaces do not keep animation work running.

## Examples and verification

The Gallery routes `file-drop-zone`, `file-list-view`, and `toast` demonstrate
selection, validation, removal, retry, progress, and compact/detailed feedback.
Displayed C++ and Python samples use the same public contracts.
Focused owner targets are `test_file_drop_zone`, `test_file_list_view`, and
`test_toast`. Native drag delivery, screen readers, and animation appearance
still require review on each target operating system.

<!-- docs-nav:bottom:start -->
---
[← Charts](charts.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md)
<!-- docs-nav:bottom:end -->
