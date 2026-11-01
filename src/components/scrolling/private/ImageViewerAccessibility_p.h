#ifndef FLUENTQT_COMPONENTS_SCROLLING_PRIVATE_IMAGEVIEWERACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_SCROLLING_PRIVATE_IMAGEVIEWERACCESSIBILITY_P_H

namespace fluent::scrolling {

class ImageViewer;

namespace detail {

void ensureImageViewerAccessibilityFactory();
// Sends a value change, and ActionChanged when zoom in/out availability changed.
// zh_CN: 发送数值变化事件；放大/缩小可用性变化时额外发送 ActionChanged。
void notifyImageViewerAccessibilityZoomChanged(ImageViewer* viewer, bool actionsChanged);
void notifyImageViewerAccessibilityImageChanged(ImageViewer* viewer, bool actionsChanged);
void notifyImageViewerAccessibilityActionsChanged(ImageViewer* viewer);

} // namespace detail
} // namespace fluent::scrolling

#endif // FLUENTQT_COMPONENTS_SCROLLING_PRIVATE_IMAGEVIEWERACCESSIBILITY_P_H
