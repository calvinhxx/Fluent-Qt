#ifndef FLUENTQT_COMPONENTS_SCROLLING_PRIVATE_ANNOTATEDSCROLLBARACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_SCROLLING_PRIVATE_ANNOTATEDSCROLLBARACCESSIBILITY_P_H

class QWidget;

namespace fluent::scrolling {
class AnnotatedScrollBar;

namespace detail {

void ensureAnnotatedScrollBarAccessibilityFactory();
void notifyAnnotatedScrollBarValueChanged(AnnotatedScrollBar* bar);
void notifyAnnotatedScrollBarRangeChanged(AnnotatedScrollBar* bar);
void notifyAnnotatedScrollBarStructureChanged(AnnotatedScrollBar* bar);

} // namespace detail
} // namespace fluent::scrolling

#endif // FLUENTQT_COMPONENTS_SCROLLING_PRIVATE_ANNOTATEDSCROLLBARACCESSIBILITY_P_H
