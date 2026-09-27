#ifndef FLUENT_QT_GALLERY_WASM_SMOKE_RUNNER_H
#define FLUENT_QT_GALLERY_WASM_SMOKE_RUNNER_H

namespace fluent::gallery {

class GalleryWindow;

/** Enables opt-in timing before creating a Gallery renderer for Spatial smoke. */
void prepareWasmSmokeIfRequested();

/** Starts a browser smoke or read-only input probe selected by `wasm-smoke`. */
void startWasmSmokeIfRequested(GalleryWindow* window);

} // namespace fluent::gallery

#endif // FLUENT_QT_GALLERY_WASM_SMOKE_RUNNER_H
