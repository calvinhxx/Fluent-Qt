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

The caller owns the session, plugin, and created widgets. A parent widget owns
its children as usual in Qt; attaching an existing backdrop keeps its current
parent. Keep Python references to any unparented wrappers. Do not parent a
session or plugin under a backdrop it tracks.

The host pointer is non-owning; if the host is destroyed, its reference clears
automatically. Deactivate the session explicitly or parent it to an application
QObject to match the scope lifetime.

Deactivating a session pauses its tracked backdrops without modifying
`animationEnabled`, the active effect, speed, particle count, child widgets, or
simulation phase. Reactivating or destroying the session resumes playback (if
multiple sessions track the same backdrop, all of them must be active for it
to run). All lifecycle methods run on the GUI thread.

Visibility, clipping, window activation, High Contrast, and motion policy
settings continue to drive the particle timer as usual. Backdrops do not accept
keyboard focus, leaving child controls' input and accessibility intact.

## Dependencies

The implementation lives in `src/components/particles` (`fluent::particles`), and
`plugin/Particles/CMakeLists.txt` owns its target. The core `FluentQt::FluentQt`
library contains no particle code. Dependencies run one way: Spatial → Particles →
Plugins → FluentQt.

Gallery, PySide6, and Spatial builds enable Particles automatically. Spatial's
`ParticleLayer` renders a caller-managed backdrop on an existing OpenGL context,
falling back to CPU painting on failure while keeping the animation running.
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
