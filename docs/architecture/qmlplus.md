# QMLPlus reactive composition

> **Status:** Accepted contract

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Architecture](README.md) › Runtime contracts

[Contents](../SUMMARY.md) · [Architecture index](README.md) · [Plugins: optional extension lifecycle →](plugins.md)
<!-- docs-nav:top:end -->

QMLPlus adds reactive composition to Qt Widgets without a Qt Quick or JavaScript
runtime. Include `<FluentQt/Foundation.h>` or the application umbrella
`<FluentQt/FluentQt.h>`. Existing `anchors()`, `bind()`, `addState()` and
`setState()` entry points remain available.

## Managed and computed bindings

`PropertyBinder::bindManaged()` and `QMLPlus::bindManaged()` return a
`BindingHandle`. The target owns the binding runtime; copying or dropping a
handle does not destroy the binding. Use `unbind()` to disconnect it explicitly.
`pause()` and `setEnabled(false)` hold the current display. Resuming an enabled
binding reads its current dependencies, rather than replaying old notifications.

`bindComputed()` accepts an explicit list of `BindingDependency` values and a
C++ evaluator returning `QVariant`. Every dependency must expose a readable Qt
meta-property with a NOTIFY signal, and the target property must be readable
and writable.
Evaluation is one-way. Two-way direct links require readable/writable properties
and change notification in both directions. Callers keep dependencies alive;
the runtime guards them and stops evaluation after destruction.

One managed binding owns each target/property pair. A valid replacement
invalidates its predecessor's handle. Invalid registrations leave an existing
binding intact. Initial synchronization is immediate; `BindingOptions::deferred`
coalesces later notifications into an event-loop update. These APIs execute on
the target's Qt thread and are intended for GUI-thread composition.

The C++ `bind()` convenience uses the same runtime and replacement rule. For
compatibility, it still synchronizes an initial value when a property has no
NOTIFY signal, then observes only directions that provide one. Source changes
without notification do not update the target automatically; leaving a state
override reads the current source value. Both endpoints must be on the calling
Qt thread. `bindManaged()` and `bindComputed()` keep their stricter notification
requirements. Invalid registrations leave the existing binding intact.

For example, with two existing Fluent sliders and a progress bar:

```cpp
auto binding = progress->bindComputed(
    "value", {{first, "value"}, {second, "value"}},
    [first, second]() -> QVariant {
        return (first->value() + second->value()) / 2.0;
    });
binding.pause();
binding.resume();
```

Native QWidget geometry properties do not all expose NOTIFY signals. Observe
resize events for viewport rules instead of assuming that a width dependency
behaves like a QML Item property. Evaluators should compute values without
mutating their dependencies.

## States and binding restoration

`QMLState` retains its `name` and `changes` fields and adds `when`,
`dependencies`, `extend`, and `anchorChanges`. Conditional states reevaluate
when a declared dependency notifies. The first registered matching condition
wins; replacing a definition preserves its registration position. An explicit
`evaluateStates()` reevaluates the conditions. Manual `setState()` applies
immediately and remains effective until the next condition evaluation.
Replacing the current state through C++ `addState()` applies the new definition
immediately. It does not emit `stateChanged` again when the state name is
unchanged; invalid active inheritance replacements keep the previous definition.
Dependency destruction schedules reevaluation on the next event-loop turn;
destroying the controller cancels that work, so parent teardown does not emit
state notifications into partially destroyed sibling controls.

An extending state inherits its base state's changes, with derived changes
taking precedence for the same target/property. Missing or cyclic inheritance
is rejected. Unknown manual state names keep the current state intact.

While a state overrides a bound property, its binding is suspended in both
directions. Source changes do not overwrite the state, and temporary state or
animation values do not write back into the application model. Leaving the
override restores the binding and reads its latest source value. Unbound
properties retain the existing default-value restoration behavior. Target
destruction is guarded, and destroying a state controller releases its binding
overrides.

`runtime()` exposes a lazily created QObject companion for `stateChanged`,
`transitionRunningChanged`, and `transitionFinished`. QMLPlus remains a mixin;
components retain their existing QWidget and FluentElement inheritance.
QObject-only state controllers, including the Python StateGroup adapter, use
the same companion without requiring a QWidget.

## Managed motion

`setBehavior()` supplies a default `PropertyAnimation` for managed writes to
one host property. `writeProperty()` is an explicit managed write;
`bindManaged()` and `bindComputed()` also use this path. Arbitrary native
setters remain ordinary Qt calls.

`addTransition()` registers a `StateTransition` with `from`, `to`, property
selection, and animation options. A matching state transition takes precedence
over a default behavior. Retargeting starts from the current displayed value.
The runtime uses the application MotionPolicy: Disabled settles to the final
value, and Reduced shortens finite transitions. Boolean and other
non-interpolated changes apply immediately.

Animations do not replace layout ownership. Use a writable presentation
property for a visual transition; avoid animating geometry that a QLayout
controls. Logical state and model values remain observable independently of an
intermediate animation frame.

## Stateful anchors

`setAnchors()` replaces a host's complete anchor set and invalidates its owning
AnchorLayout. `resetAnchors()` clears the set. A state's `AnchorChange` records
a complete anchor set for its target; exiting the state restores that target's
prior anchors. Keep one controller responsible for a given target's anchor set.
This supports compact/expanded arrangements without stale
constraints or a window resize to trigger recalculation.

AnchorLayout continues to own constraint resolution, natural size hints, cycle
detection, and stable fallback. Breakpoint thresholds remain FluentElement
design tokens.

## Delivery and inspection

Python uses `fluentqt.bind_managed`, `fluentqt.bind_computed`, and
`fluentqt.set_behavior`. `StateGroup.add()` accepts `when`, `dependencies`,
`extend`, and `anchor_changes`; `add_transition()` configures native state
motion. Dependencies use `(QObject, property_name)` pairs. These adapters call
the same native runtime and do not create a second binding or animation engine.
They run on the target/controller's Qt thread. The target or controller retains
the Python evaluator through a Qt-owned expression object. Callers own the
dependency objects; objects captured by a Python callable follow normal Python
reference semantics. Dependency destruction invalidates the binding before
another evaluation. Initial callback errors raise before installation; later
computed errors pause the binding and retain its current value. Inspect
`BindingHandle.last_error`; `resume()` retries against current inputs. A failed
state predicate retains its last result until the state is redefined.
See the [binding coverage ledger](../../bindings/pyside6/ROADMAP.md).

Python `fluentqt.bind()` retains its NOTIFY requirement and uses the strict
native installer. It raises `ValueError` when native registration fails,
including a thread mismatch or target destruction during initial synchronization.
Repeated valid calls replace the binding on the same target property; rejected
calls leave the previous binding intact.

Open **Foundation → QML+** in any Gallery. Each card introduces one
concept, in prerequisite order: direct bindings, computed bindings, pause/resume,
named and conditional states, inheritance, binding restoration, motion, and
anchors. Snippets assume a QWidget host that inherits QMLPlus and omit only
preview layout, control captions, and accessibility metadata. Python snippets
assume `import fluentqt` and an existing `panel` widget. C++ Gallery shows C++ source, Python Gallery
shows Python source, and WebAssembly Gallery offers both.

Launch one compiled example directly:

```bash
python3 tools/dev/fluent_qt_preview.py \
  --route foundation-qmlplus --sample qmlplus-binding-computed \
  --theme light --size 800x640
```

Focused contracts belong to `test_qml_plus`, `test_anchor_layout`,
`test_gallery_content_pages`, `test_pyside6_foundation_authoring`, and
`test_pyside6_gallery_qmlplus`. Native captures describe only their recorded
host; other Qt versions, operating systems and physical assistive technology
need their own verification.

<!-- docs-nav:bottom:start -->
---
[Contents](../SUMMARY.md) · [Architecture index](README.md) · [Plugins: optional extension lifecycle →](plugins.md)
<!-- docs-nav:bottom:end -->
