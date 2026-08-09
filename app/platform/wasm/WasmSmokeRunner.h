#ifndef FLUENT_QT_GALLERY_WASM_SMOKE_RUNNER_H
#define FLUENT_QT_GALLERY_WASM_SMOKE_RUNNER_H

namespace fluent::gallery {

class GalleryWindow;

/** Starts the browser smoke sequence when the URL contains `wasm-smoke=fast|full`. */
void startWasmSmokeIfRequested(GalleryWindow* window);

} // namespace fluent::gallery

#endif // FLUENT_QT_GALLERY_WASM_SMOKE_RUNNER_H
