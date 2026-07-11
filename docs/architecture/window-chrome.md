# Window Chrome Architecture

## Ownership and boundary

The three window background choices—`Solid`, `Mica`, and `Acrylic`—are UILib
capabilities. Gallery selects and persists a requested effect, but it does not
decide which platform backend is usable and it does not infer transparency from
an operating-system name.

The implementation is split into three layers:

1. `components/windowing/WindowBackdrop.*` defines the typed request, resolved
   state, capability model, and descendant-widget query helpers.
2. `compatibility/WindowChromeCompat*` probes and applies native compositor
   facilities. Each platform file implements the same capability/result
   contract.
3. `components/windowing/WindowBackdropMaterial.*` provides the portable,
   app-painted material used when a native/compositor backdrop is unavailable or
   fails to apply.

`Window` owns resolution and publishes the effective state. Application code may
call `Window::setBackdropEffect()` and observe `backdropStateChanged`, but should
not read private dynamic-property strings or duplicate platform checks.

## Typed backdrop state

`BackdropState` deliberately separates intent from what is actually visible:

| Field | Meaning |
| --- | --- |
| `requestedEffect` | Caller intent: `Solid`, `Mica`, or `Acrylic`. |
| `effectiveEffect` | Visual effect currently represented. A painted Mica/Acrylic fallback still represents the requested effect. |
| `backend` | Producer of the visible surface: `Solid`, `PaintedMaterial`, `DwmSystemBackdrop`, `MacVibrancy`, or `LinuxCompositor`. |
| `fidelity` | Quality tier: `Solid`, `Emulated`, `Composited`, or `Native`. |
| `surfaceMode` | Backing-store paint contract: `SolidOpaque`, `PaintedOpaque`, or `CompositedTransparent`. |
| `platformApplied` | Whether a native/compositor material was actually applied. |
| `reason` | Diagnostic resolution/apply result, including fallback reasons. |

`BackdropCapabilities` is a session probe, not a promise. It records alpha
surface availability, native Mica/Acrylic support, compositor blur support, and
the provider name. The authoritative transition to a transparent surface occurs
only after `WindowChromeCompat::applySystemBackdropDetailed()` returns a successful
`BackdropApplyResult`.

This distinction prevents the old failure mode in which theoretical OS support
was treated as successful installation and Qt cleared its backing store over no
real system material.

The normal resolution flow is:

```text
requested Solid
  -> Solid / Solid / SolidOpaque

requested Mica or Acrylic
  -> native or compositor capability advertised
     -> apply succeeds
        -> platform backend / Native or Composited / CompositedTransparent
     -> apply fails
        -> PaintedMaterial / Emulated / PaintedOpaque
  -> no transparent-material capability
     -> PaintedMaterial / Emulated / PaintedOpaque
```

Before a native window is ready, the state remains safely opaque. A later show or
explicit reapply may promote it to `CompositedTransparent` only after a successful
platform call. Runtime loss/failure similarly resolves back to `PaintedOpaque`.

## Surface-mode paint contract

`surfaceMode`—not the requested effect and not `WA_TranslucentBackground`—decides
whether a widget may erase backing-store pixels.

| Surface mode | Top-level behavior | Descendant chrome behavior | May use `CompositionMode_Clear` or transparent `CompositionMode_Source`? |
| --- | --- | --- | --- |
| `SolidOpaque` | Paint the normal opaque theme surface. | Paint/use the normal opaque chrome surface. | No. |
| `PaintedOpaque` | Paint deterministic Mica/Acrylic material in the app. | Leave shared chrome/page-gap regions unfilled so the top-level material remains visible; never punch top-level alpha holes. | No. |
| `CompositedTransparent` | Clear the material region so DWM, macOS, or the Linux compositor is visible. | Clear only the regions that are designed to expose that compositor surface. | Yes. |

Use the typed helpers from `WindowBackdrop.h`:

- `windowBackdropRequiresTransparentClear(widget)` for operations that really
  erase to transparent;
- `windowBackdropUsesPaintedMaterial(widget)` when chrome should reveal the
  UILib-painted root material without erasing it;
- `windowHasMaterialBackdrop(widget)` when either a painted or composited
  Mica/Acrylic effect matters visually.

The top-level may carry `WA_TranslucentBackground` for native frame/shadow
integration even while its effective surface is opaque. Therefore that Qt
attribute is never evidence that clearing is safe.

`fluentMicaBackdrop` remains only as a compatibility alias for older consumers.
It is now `true` exactly when `surfaceMode == CompositedTransparent`; it is not a
Mica identity flag, a capability flag, or an apply-success substitute. New UILib
and Gallery code must use the typed API. The published
`fluentWindowBackdropState` dynamic property is also an implementation detail;
prefer `Window::backdropState()` or the typed helper functions.

## App-painted material

`WindowBackdropMaterial` makes Mica and Acrylic useful on Win10, Wayland, Linux
desktops without a supported blur protocol, and any native-apply failure path.
It always establishes an opaque base before decorative layers, so the final
surface is deterministic and cannot reveal black/undefined backing-store pixels.

The renderer intentionally performs no screen capture and no CPU blur. It uses
theme material tokens, low-frequency color fields, tint/luminosity layers, and a
small cached deterministic grain tile. Mica is quieter and more cohesive;
Acrylic has stronger luminosity/tint/grain separation. The output remains stable
across frames and safe for virtual machines and remote sessions.

This is a visual-semantic fallback, not a claim of desktop sampling. Its state is
therefore `PaintedMaterial / Emulated / PaintedOpaque`, which lets diagnostics and
applications explain the actual fidelity without changing the requested setting.

## Platform matrix

| Platform/session | Effect backend | Resolved state | Notes |
| --- | --- | --- | --- |
| Windows 11 22H2+ with successful DWM call | `DwmSystemBackdrop` | `Native / CompositedTransparent` | Uses `DWMWA_SYSTEMBACKDROP_TYPE`; Mica maps to main-window material and Acrylic to transient-window material. A first-show recomposition nudge handles the observed DWM activation race. |
| Windows 10, older Windows 11, unavailable DWM entry point, or failed DWM call | `PaintedMaterial` | `Emulated / PaintedOpaque` | Legacy Win10 Acrylic is intentionally not used for a whole Qt Widgets window because transparent backing-store regions can render black, especially in VMs. |
| macOS Cocoa with a successfully installed `NSVisualEffectView` | `MacVibrancy` | `Native / CompositedTransparent` | Mica and Acrylic use distinct vibrancy material/tint mappings; native traffic lights and unified title-bar behavior remain in the mac backend. |
| macOS host/view resolution or vibrancy installation failure | `PaintedMaterial` | `Emulated / PaintedOpaque` | Apply failure is authoritative even if the class was discovered during capability probing. |
| Linux X11 with active compositor, ARGB window, advertised `_KDE_NET_WM_BLUR_BEHIND_REGION`, and successful property update | Acrylic: `LinuxCompositor`; Mica: `PaintedMaterial` | Acrylic: `Composited / CompositedTransparent`; Mica: `Emulated / PaintedOpaque` | KWin blur-behind represents Acrylic's live background sampling. Mica deliberately keeps the stable UILib-painted material. Detection is capability-based; it is not gated only by desktop-environment environment variables. |
| Linux Wayland, X11 without an active/supported blur compositor, missing alpha visual, or failed blur-property update | `PaintedMaterial` | `Emulated / PaintedOpaque` | Qt Widgets has no stable cross-compositor Wayland blur API, so UILib paints the full material instead of degrading to a flat color. |
| Other platforms | `PaintedMaterial` for Mica/Acrylic | `Emulated / PaintedOpaque` | The generic compatibility backend remains free of platform-specific assumptions. |

On Linux, client-side move/resize remains independent from the backdrop tier.
Both X11 and Wayland first use `QWindow::startSystemMove()` /
`QWindow::startSystemResize()`. X11 may use a manual geometry fallback; Wayland
must not choose global window geometry. The client-side shadow margin is removed
while maximized/fullscreen so the window still reaches screen edges.

While KWin Acrylic blur is active, a low-frequency capability probe detects compositor
or blur-effect shutdown even when Qt emits no window event. Loss disables the
native hint and asks `Window` to re-resolve to `PaintedOpaque`; surface, screen,
DPR, and window-state changes still trigger immediate validation.

## Z-order and content surfaces

The window is still organized as three visual layers:

```text
z-0  Window material root
z-1  title bar and navigation chrome
z-2  content pages and opaque cards/controls
```

- In `CompositedTransparent`, z-0/z-1 expose the native/compositor surface.
- In `PaintedOpaque`, z-0 paints the software material and z-1 shares it without
  replacing it with a flat fallback color.
- Transparent page gaps share the same z-0 material in both native/composited
  and app-painted modes. Cards, controls, dialogs, and any explicitly configured
  content surfaces remain opaque Fluent layers above it.

## Historical pitfalls retained as rules

Several bugs established the current contract:

- Clearing based on `WA_TranslucentBackground` produced black navigation/title
  seams in Normal mode because the attribute describes the top-level window
  setup, not the effective material.
- An unconditional `CompositionMode_Clear` in the raised content-corner overlay
  punched a transparent hole through an opaque surface, producing dark/light
  artifacts where title bar, navigation, and content meet.
- On macOS, repeatedly drawing translucent pixels without first replacing the
  backing-store content caused ghosting, cumulative darkening, and first-frame
  flashes. Transparent replacement is still needed for real vibrancy, but is now
  gated strictly by `CompositedTransparent`. Page switches request a coalesced
  replacement frame with `update()`; they must not synchronously re-enter Cocoa
  painting with `repaint()` from inside a navigation input handler.
- Treating support detection as apply success could clear over a rejected DWM,
  Cocoa, or X11 operation. Capability probing and structured apply results are
  now separate stages.

These lead to one hard rule: **only `CompositedTransparent` may clear pixels to
reveal a system/compositor background. `PaintedOpaque` must remain opaque from
the first pixel through the final frame.**

## Code map

| Concern | Location |
| --- | --- |
| Public effect/state/capability types and widget helpers | `src/components/windowing/WindowBackdrop.h/.cpp` |
| Opaque fallback material renderer | `src/components/windowing/WindowBackdropMaterial.h/.cpp` |
| Resolution, publication, and root painting | `src/components/windowing/Window.h/.cpp` |
| Platform-neutral chrome facade | `src/compatibility/WindowChromeCompat.h/.cpp` |
| DWM backend | `src/compatibility/WindowChromeCompat_win.cpp` |
| Cocoa vibrancy backend | `src/compatibility/WindowChromeCompat_mac.cpp` |
| X11/KWin blur and Linux chrome behavior | `src/compatibility/WindowChromeCompat_linux.cpp` |
| Unsupported-platform adapter | `src/compatibility/WindowChromeCompat_fallback.cpp` |
| Shared title/navigation/content paint consumers | `src/components/windowing/TitleBar.cpp`, `src/components/navigation/NavigationView.cpp`, `src/components/navigation/StackContentHost.cpp` |

## Review checklist

When changing backdrop behavior, verify:

1. requested and effective state can differ in backend/fidelity without losing
   the user's selected effect;
2. a failed native apply resolves to `PaintedOpaque` in the same update;
3. no Clear/transparent Source path is reachable outside
   `CompositedTransparent`;
4. painted Mica and Acrylic are visually distinct in Light/Dark and remain fully
   opaque;
5. content islands and their rounded-corner overlays do not expose black or
   undefined pixels;
6. runtime effect/theme changes and the Win11 first-show reapply update the typed
   state consistently;
7. Linux tests cover Wayland/unsupported compositor fallback as well as the X11
   compositor capability path.
