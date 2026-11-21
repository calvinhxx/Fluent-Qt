#pragma once

class QWidget;

namespace fluent::spatial {
class SpatialItem;
class SpatialView;
} // namespace fluent::spatial

namespace fluentqt_designer {

// Pass a heap-allocated form body after setupUi(). On success the host owns it.
// Ordinary forms return the body itself. Unknown or unavailable hosts return nullptr.
QWidget* makeFormHost(QWidget* body, QWidget* owner = nullptr);
void showFormHost(QWidget* host);

#ifdef FLUENT_QT_HAS_SPATIAL
// Insert an edited SpatialItem template into an existing scene. The view owns the body.
fluent::spatial::SpatialItem* addSpatialForm(fluent::spatial::SpatialView* view, QWidget* body);
#endif

} // namespace fluentqt_designer
