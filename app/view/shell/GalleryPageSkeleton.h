#ifndef GALLERYPAGESKELETON_H
#define GALLERYPAGESKELETON_H

#include <QWidget>

class QResizeEvent;

namespace fluent::status_info {
class Shimmer;
}

namespace fluent::gallery {

/**
 * @brief Lightweight shimmer skeleton shown while a cold content page builds incrementally.
 * Reused as a single resident stack page so cold route switches give immediate visual feedback.
 * zh_CN: 冷路由分步构建期间显示的轻量 Shimmer 骨架屏，作为单个常驻页面复用以提供即时切换反馈。
 */
class GalleryPageSkeleton : public QWidget {
    Q_OBJECT
public:
    explicit GalleryPageSkeleton(QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateSkeletonElements();

    fluent::status_info::Shimmer* m_shimmer = nullptr;
};

} // namespace fluent::gallery

#endif // GALLERYPAGESKELETON_H
