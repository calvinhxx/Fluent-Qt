#pragma once

class QWidget;

namespace compatibility::detail {

// Mark a render-to-texture widget (QOpenGLWidget/QRhiWidget) dirty for the next backing-store pass
// instead of calling update(), which forces Qt to repaint everything underneath it.
// zh_CN: 仅标记纹理控件在下次绘制缓冲经过时重绘；改用 update() 会导致 Qt 重绘其下方全部区域，进而触发多余特效捕获。
bool schedulePaintInBackingStorePass(QWidget* textureWidget);

} // namespace compatibility::detail
