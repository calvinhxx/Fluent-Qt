#ifndef FLUENTQT_COMPONENTS_SCROLLING_PRIVATE_IMAGEVIEWERACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_SCROLLING_PRIVATE_IMAGEVIEWERACCESSIBILITY_P_H

namespace fluent::scrolling {

class ImageViewer;

namespace detail {

void ensureImageViewerAccessibilityFactory();
// Emits ValueChanged, plus ActionChanged when zoom-in/out availability changed.
void notifyImageViewerAccessibilityZoomChanged(ImageViewer* viewer, bool actionsChanged);
void notifyImageViewerAccessibilityImageChanged(ImageViewer* viewer, bool actionsChanged);
void notifyImageViewerAccessibilityActionsChanged(ImageViewer* viewer);

} // namespace detail
} // namespace fluent::scrolling

#endif // FLUENTQT_COMPONENTS_SCROLLING_PRIVATE_IMAGEVIEWERACCESSIBILITY_P_H
