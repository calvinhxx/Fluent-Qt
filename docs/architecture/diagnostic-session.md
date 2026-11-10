# Diagnostic reports

> **Status:** Accepted contract

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Architecture](README.md) › Runtime contracts

[← Inspector Report Contract](inspector-report.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [Charts →](charts.md)
<!-- docs-nav:top:end -->

FluentQt provides a reusable `DiagnosticSession` collector and in-memory ZIP
packager through `<FluentQt/Diagnostics.h>` and `fluentqt.DiagnosticSession`.
It uses Qt only and performs no file or network I/O. Application policy owns
startup, storage, logs, associated crash files, clipboard delivery, and submission.

## Gallery workflow

Desktop C++ and Python Gallery start collecting when the application window is
created. Click **Report a problem**, the Bug icon beside **Settings** in the
navigation footer. Gallery prepares a ZIP in the background, copies the file to
the clipboard, then shows **Diagnostic ZIP ready** with a `fluentqt-report.zip`
attachment capsule displaying the archive size and **Copied to clipboard** status.
The compact notification emerges beside the clicked button and remains visible
until the user dismisses it or chooses **Open GitHub issue**. Only this
action opens the prefilled issue. Press the native paste shortcut (`⌘V` / `Ctrl+V`)
in its attachment field, describe the problem and submit. Preparation failures offer **Try again** without
opening a browser. A failed browser launch preserves the ready ZIP capsule and an explicit retry action.
The button is also available in the compact rail and top navigation. The notification
follows its anchor and stays within the window without moving keyboard focus.
It prefers a centered position on the right or left, otherwise aligning with the
anchor edge above, beside, or below so the card does not straddle the navigation
pane seam. The tail-free surface uses standard overlay tokens and elevation with a
24 px semantic status badge, a 13 px semibold title, a 40 px file capsule, a 12 px
secondary paste hint, and a flush-aligned 12 px semibold accent action with a 24 px
click target. Full motion uses a 220 ms decelerated entrance with a 6 px outward
slide, a 140 ms accelerated exit, a continuous status-icon spin while preparing,
and a 160 ms in-place height transition when the ready capsule and action appear;
reduced motion suppresses slide, spin, and size transitions, and disabled motion
settles immediately. Escape, an outside click, hiding the anchor, or the close
button dismisses it. Completing a background report does not reopen a dismissed
notification.

There are no recording controls, file pickers or diagnostics settings panel.
Version, environment and a session ID are prefilled; logs are never put in the
issue URL. Opening the page does not upload or submit anything. GitHub uploads
an attachment when the user pastes or attaches it, before submitting the issue.

The collector retains the latest 512 events and bounded widget snapshots; it is
not a complete interaction replay. Desktop Gallery saves a checkpoint about
every 15 seconds on a background worker, retaining the current session and two
previous session files under its application data directory's `diagnostics`
folder. The ZIP may include the previous session; a previous file alone is not
proof of a crash. An abrupt exit can lose events since the last checkpoint.
Startup failures before initialization are not captured. C++ Gallery also
includes up to 256 KiB from its existing current log file. The latest five ZIPs
remain in `diagnostics/reports` so clipboard file references survive application
exit. Creating more reports eventually replaces older ZIPs.

WebAssembly uses the same button but downloads the ZIP instead of copying a
native file. Its anchored notification asks the user to attach the download and offers the same
explicit **Open GitHub issue** action. It retains history
only for the current page and cannot read host crash files. A download handoff
does not prove the file was saved. Browser tab-opening policies still apply.

## Existing crash files

Desktop export searches the current and previous recorded process identities in
macOS `~/Library/Logs/DiagnosticReports`, Windows `%LOCALAPPDATA%/CrashDumps`, or
Linux `/var/crash`. Matching requires a process ID, executable and file timestamp
within the recorded session's time window (with a two-minute allowance after its
last checkpoint). macOS `.ips` and macOS/Apport `.crash` headers carry identity;
WER `.dmp` filenames carry executable and PID. A Python interpreter name alone
never matches a file. Local process metadata stays in checkpoints and is removed
from exported JSON. Older checkpoints without identity cannot claim crash files.

Up to two matching files, totaling at most 16 MiB, are included automatically.
Empty, unreadable, symlink, unrelated and oversized files are skipped. The report
states how many crash files were included. Bundle size is capped at 20 MiB.

This version does not generate minidumps, install crash handlers, change Windows
registry settings or enable system core dumps. No matching file is normal when
the OS has not produced one. Windows WER local dumps require prior configuration;
raw/systemd Linux cores are not automatically collected because their origin
cannot be established by this adapter. Gallery does not symbolicate crashes.
Crash contents and log text can include private paths or memory; review the ZIP
before sharing it publicly.

## Data and ownership

The JSON report contains environment, initial/current widget state, bounded
events, duration, session ID and truncation counts. Environment includes FluentQt,
application and Qt versions, OS, architecture, font, theme, layout direction and
up to 16 display geometries. Python adds Python and PySide6 versions.

Widget snapshots visit at most 256 widgets. They include type, geometry,
visibility, enabled/focus state, editor margins and model dimensions. They omit
object names, text values, model cell contents, clipboard data, paths and
environment variables. Only navigation keys are observed; typed characters and
IME contents are not captured. Paint, mouse movement and animation ticks are not
recorded. Events are observed without consuming them.

Library sessions exclude log bodies by default. Gallery enables recent log
messages for local troubleshooting. Log bodies are limited to 512 UTF-16 code
units and categories to 80; arbitrary log messages are **not automatically
redacted**. Review the ZIP before attaching it to a public issue. No automatic
network transmission, screenshot or recording is added.
The session ID identifies one run, not an installation or person.

For bounded trees, Inspector findings are restricted to rule, severity and
rectangle, with at most 64 entries. Large roots skip the full Inspector pass.
The previous developer-facing Inspector schema is unchanged.

## Integration

```cpp
#include <FluentQt/Diagnostics.h>

// Explicitly start bounded continuous collection for an application-owned window.
auto* session = new fluent::diagnostics::DiagnosticSession(application);
session->start(window, false, 0);
auto sink = session->messageSink(); // forward from the host's existing logger
// Later, on the GUI thread; recording continues.
const auto report = session->captureReport();
const QByteArray zip = session->createBundle(report);
// The application owns local saving and sharing.
```

```python
import fluentqt
session = fluentqt.DiagnosticSession()  # retain this owner
session.start(window, False, 0)
report = session.captureReport()
archive = bytes(session.createBundle(report))
```

The default duration remains five minutes for explicit consumer sessions. Passing
zero selects continuous bounded history. Repeated starts while recording are
no-ops. The root window is borrowed. `captureReport`, `start`, `stop`, `clear` and
`environment` belong on the GUI thread. `report`, `recordMessage`, `isRecording`
and the weak-state message sink synchronize access. The sink can safely outlive
the session. Applications must provide their own logging bridge; the core never
installs a global message handler or changes logging categories.

`createBundle` accepts optional filename-to-`QByteArray` attachments. It accepts
only short, flat ASCII filenames, reserves `report.json`, allows at most eight
attachments, and returns empty on invalid names or size overflow. The ordinary
ZIP format is verified with an independent ZIP reader. It performs no file access.

## Verification

`test_inspector` covers unchanged input, privacy boundaries, continuous capture,
size bounds, concurrent forwarding and lifetime-safe sinks. Gallery tests cover
button recreation, persistence, prefilled issue URLs, ZIP contents, file clipboard
delivery, crash identity matching, layout and save failure. Browser checks exercise the real ZIP
download. Native system dump generation is outside this version's verification.

<!-- docs-nav:bottom:start -->
---
[← Inspector Report Contract](inspector-report.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [Charts →](charts.md)
<!-- docs-nav:bottom:end -->
