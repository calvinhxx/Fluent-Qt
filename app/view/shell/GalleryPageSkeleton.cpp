#include "GalleryPageSkeleton.h"

#include <QVBoxLayout>

#include "components/status_info/Shimmer.h"
#include "design/Spacing.h"

namespace fluent::gallery {

using fluent::status_info::Shimmer;

namespace {

// One shimmer block sized like a page element. ImageCard renders a single rounded rect that
// re-fits the widget on resize, so blocks stay responsive to the content width. The phase
// offset staggers each block's light sweep so the page shimmers as a gentle cascade rather
// than in lockstep. zh_CN: 一个像页面元素的 shimmer 块。ImageCard 渲染单个圆角矩形并在 resize
// 时重适配 widget，使块随内容宽度自适应。phase 偏移错开每块的扫光，使页面像柔和瀑布般依次微光而非整齐划一。
Shimmer* makeBlock(QWidget* parent, int height, int maxWidth, qreal phase)
{
    auto* block = new Shimmer(parent);
    block->setShimmerTemplate(Shimmer::ShimmerTemplate::ImageCard);
    block->setFixedHeight(height);
    if (maxWidth > 0)
        block->setMaximumWidth(maxWidth);
    block->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    block->setShimmerProgress(phase);
    return block;
}

} // namespace

GalleryPageSkeleton::GalleryPageSkeleton(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    // Mirror GalleryContentPage's content margins (24, 34, 24, 48) and section spacing so the
    // skeleton lands where the real heading/cards will, keeping the handover steady.
    // zh_CN: 沿用 GalleryContentPage 的内容边距 (24, 34, 24, 48) 与分区间距，使骨架落在真标题/卡片
    // 将出现的位置，让切换稳定不跳。
    layout->setContentsMargins(24, 34, 24, 48);
    layout->setSpacing(::Spacing::Standard);

    // Heading: a wide title bar and a narrower subtitle, left-aligned like the real header.
    // zh_CN: 标题区：一条较宽的标题栏和一条较窄的副标题，左对齐，呼应真实头部。
    layout->addWidget(makeBlock(this, ::Spacing::ControlHeight::Large, 320, 0.00), 0, Qt::AlignLeft);
    layout->addWidget(makeBlock(this, ::Spacing::ControlHeight::Small, 460, 0.06), 0, Qt::AlignLeft);
    layout->addSpacing(::Spacing::Medium);

    // A few full-width cards echoing the sample stack below the heading.
    // zh_CN: 几张全宽卡片，呼应标题下方的示例堆叠。
    constexpr int kCardCount = 3;
    constexpr int kCardHeight = 132;
    for (int i = 0; i < kCardCount; ++i)
        layout->addWidget(makeBlock(this, kCardHeight, 0, 0.14 + 0.08 * i));

    layout->addStretch(1);
}

} // namespace fluent::gallery
