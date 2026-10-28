# Particles: optional decorative effects

> **Status:** Accepted contract

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Architecture](README.md) › Runtime contracts

[← Plugins: optional extension lifecycle](plugins.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [QmlBridge: application QML in a QWidget host →](qml-bridge.md)
<!-- docs-nav:top:end -->

`FluentQt::Particles` provides `ParticleBackdrop`, `ParticlesPlugin`, and
`ParticlesSession`. Its CPU renderer supports flowing ribbons, floating dots,
and a starfield without Spatial, OpenGL, or Qt Quick. Gallery exposes the same
samples under **Plugins → Particles → ParticleBackdrop**.

## Build and consume

Set `FLUENT_QT_BUILD_PARTICLES=ON` before adding FluentQt, then link
`FluentQt::Particles` and include `<FluentQt/Particles.h>`. For an installed SDK:

```cmake
find_package(FluentQt CONFIG REQUIRED COMPONENTS Particles)
target_link_libraries(my_app PRIVATE FluentQt::Particles)
```

Construct a backdrop directly:

```cpp
auto* backdrop = new fluent::particles::ParticleBackdrop(parent);
backdrop->setEffect(fluent::particles::ParticleBackdrop::Starfield);
```

Use a session when an application scope needs activation and cleanup:

```cpp
auto* session = new fluent::particles::ParticlesSession(host, window);
if (session->activate()) {
    auto* backdrop = session->createBackdrop(parent);
    backdrop->setParticleCount(240);
}
```

Keep the session alive for the intended scope; a QObject parent such as the
window does this. The session uses the existing [Plugins lifecycle](plugins.md);
it does not discover or load binary plugins.
`ParticlesPlugin` also supports generic `PluginManager` registration with ID
`fluentqt.particles` and widget key `particle-backdrop`.

## Ownership and motion

The caller owns the session, plugin, and produced widgets. A requested QWidget
parent owns its child normally; attaching an existing backdrop never reparents
it. Keep unparented Python wrappers alive. A session or plugin must not belong
to the QObject ancestry below a tracked backdrop.

The host is a borrowed context, not the lifetime owner of the session. As with
PluginManager, a destroyed host becomes null; explicitly deactivate the session
or give it an appropriate QObject parent to tie its lifetime to the application.

Deactivation pauses tracked backdrops through an internal gate. It does not
change `animationEnabled`, the effect, speed, particle count, child content, or
simulation phase, so a preference the application changes while the session is
inactive is kept. Reactivating or destroying the session removes its pause.
When several sessions track one backdrop, it runs only after every one of them
has removed its pause. All lifecycle operations run on QApplication's GUI thread.

Visibility, clipping, inactive-window handling, High Contrast, and reduced or
disabled motion continue to gate the component's existing timer. Decoration
does not acquire keyboard focus; child controls keep their ordinary input and
accessibility semantics.

## Dependencies

The implementation lives in `src/components/particles` (`fluent::particles`), and
`plugin/Particles/CMakeLists.txt` owns its target. The base `FluentQt::FluentQt`
library contains no particle code; themes and the shared motion policy stay in
it. Dependencies run one way: Spatial → Particles → Plugins → FluentQt.

Gallery, PySide6 and Spatial builds turn Particles on automatically. Spatial's
`ParticleLayer` borrows a backdrop's state and an existing OpenGL context; if
acceleration fails, drawing returns to the CPU without restarting the effect.
The installed-package CI integration requests `COMPONENTS Particles` and checks
that the base package contains no particle headers or targets.

## Python and WebAssembly

Matched PySide6 builds expose:

```python
from fluentqt.particles import ParticleBackdrop, ParticlesSession

session = ParticlesSession(host, window)
if session.activate():
    backdrop = session.createBackdrop(parent)
    backdrop.setEffect(ParticleBackdrop.Effect.FloatingDots)
```

WebAssembly builds use the same C++ module, factories, motion policy, and CPU
fallback. There is no separate browser plugin manager. GPU integration retains
the optional [Spatial contract](spatial-view.md#gallery-particle-layers).

<!-- docs-nav:bottom:start -->
---
[← Plugins: optional extension lifecycle](plugins.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [QmlBridge: application QML in a QWidget host →](qml-bridge.md)
<!-- docs-nav:bottom:end -->
