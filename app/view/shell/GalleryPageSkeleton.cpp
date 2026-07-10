#include "GalleryPageSkeleton.h"

#include <QResizeEvent>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "components/status_info/Shimmer.h"
#include "design/CornerRadius.h"
#include "design/Spacing.h"

namespace fluent::gallery {

using fluent::status_info::Shimmer;
using fluent::status_info::ShimmerPainter;

namespace {

constexpr int kLeftMargin = 24;
constexpr int kTopMargin = 34;
constexpr int kRightMargin = 24;
constexpr int kBottomMargin = 48;
constexpr int kCardCount = 3;
constexpr int kCardHeight = 132;

} // namespace

GalleryPageSkeleton::GalleryPageSkeleton(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // One full-page Shimmer paints every placeholder shape. The previous five
    // child Shimmers each owned a 16 ms timer and a separate paint pass; creating
    // them on the first cold navigation made the click path visibly hitch.
    // zh_CN: 使用一个全页 Shimmer 绘制全部占位形状。此前五个子 Shimmer 各自持有
    // 16ms 定时器并单独绘制，首次冷导航时创建它们会让点击路径明显卡顿。
    m_shimmer = new Shimmer(this);
    m_shimmer->setObjectName(QStringLiteral("galleryPageSkeletonShimmer"));
    m_shimmer->setShimmerTemplate(Shimmer::ShimmerTemplate::Custom);
    m_shimmer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_shimmer);
    updateSkeletonElements();
}

void GalleryPageSkeleton::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateSkeletonElements();
}

void GalleryPageSkeleton::updateSkeletonElements()
{
    if (!m_shimmer)
        return;

    QRectF content = QRectF(rect()).adjusted(kLeftMargin, kTopMargin,
                                             -kRightMargin, -kBottomMargin);
    if (content.isEmpty()) {
        m_shimmer->clearElements();
        return;
    }

    QVector<ShimmerPainter::Element> elements;
    elements.reserve(2 + kCardCount);
    qreal y = content.top();
    const qreal titleWidth = qMin<qreal>(320.0, content.width());
    const qreal subtitleWidth = qMin<qreal>(460.0, content.width());
    elements.append({ShimmerPainter::Shape::RoundedRect,
                     QRectF(content.left(), y, titleWidth, ::Spacing::ControlHeight::Large),
                     CornerRadius::Control});
    y += ::Spacing::ControlHeight::Large + ::Spacing::Standard;
    elements.append({ShimmerPainter::Shape::RoundedRect,
                     QRectF(content.left(), y, subtitleWidth, ::Spacing::ControlHeight::Small),
                     CornerRadius::Control});
    y += ::Spacing::ControlHeight::Small + ::Spacing::Medium + ::Spacing::Standard;

    for (int i = 0; i < kCardCount; ++i) {
        elements.append({ShimmerPainter::Shape::RoundedRect,
                         QRectF(content.left(), y, content.width(), kCardHeight),
                         CornerRadius::Control});
        y += kCardHeight + ::Spacing::Standard;
    }
    m_shimmer->setElements(elements);
}

} // namespace fluent::gallery
