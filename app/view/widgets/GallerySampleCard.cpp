#include "GallerySampleCard.h"

#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QtGlobal>

#include "components/textfields/Label.h"
#include "design/CornerRadius.h"
#include "design/Typography.h"
#include "GalleryCodeBlock.h"
#include "view/support/GalleryStyleSupport.h"

namespace fluent::gallery {

namespace {
constexpr int kCardLeftMargin = 20;
constexpr int kCardTopMargin = 18;
constexpr int kCardRightMargin = 20;
constexpr int kCardBottomMargin = 18;
constexpr int kCardSpacing = 12;
constexpr int kDefaultCardWidth = 640;
constexpr int kMinimumCardWidth = 280;
}

GallerySampleCard::GallerySampleCard(const GallerySample& sample, QWidget* parent)
    : QFrame(parent)
    , m_sampleId(sample.id)
{
    setObjectName(QStringLiteral("gallerySampleCard"));
    setProperty("gallerySampleId", m_sampleId);
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* layout = new AnchorLayout(this);

    m_titleLabel = new fluent::textfields::Label(sample.title, this);
    m_titleLabel->setObjectName(QStringLiteral("gallerySampleCardTitle"));
    m_titleLabel->setFluentTypography(Typography::FontRole::BodyStrong);

    if (!sample.description.isEmpty()) {
        m_descriptionLabel = new fluent::textfields::Label(sample.description, this);
        m_descriptionLabel->setObjectName(QStringLiteral("gallerySampleCardDescription"));
        m_descriptionLabel->setFluentTypography(Typography::FontRole::Body);
        m_descriptionLabel->setWordWrap(true);
    }

    m_previewSurface = new QFrame(this);
    m_previewSurface->setObjectName(QStringLiteral("gallerySampleCardPreview"));
    m_previewSurface->setFrameShape(QFrame::NoFrame);
    m_previewSurface->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_previewSurface->installEventFilter(this);
    auto* previewLayout = new QHBoxLayout(m_previewSurface);
    previewLayout->setContentsMargins(20, 20, 20, 20);
    previewLayout->setSpacing(16);

    if (sample.createPreview) {
        m_preview = sample.createPreview(m_previewSurface);
        if (m_preview) {
            m_preview->setObjectName(QStringLiteral("gallerySamplePreviewWidget"));
            m_preview->installEventFilter(this);
            previewLayout->addWidget(m_preview);
        }
    }
    previewLayout->addStretch(1);

    if (sample.createOptions) {
        m_options = sample.createOptions(m_previewSurface);
        if (m_options) {
            m_options->setObjectName(QStringLiteral("gallerySampleOptionsWidget"));
            m_options->installEventFilter(this);
            previewLayout->addWidget(m_options, 0, Qt::AlignTop);
        }
    }

    if (!sample.codeSnippet.isEmpty()) {
        m_codeBlock = new GalleryCodeBlock(sample.codeSnippet, this);
        // Expander animation only changes the card's own height; skip the full
        // child re-measure so each animation frame costs one layout pass, not two.
        // zh_CN: expander 动画只改变卡片自身高度；跳过子项的整体重测量，
        // 让每个动画帧只走一遍布局而不是两遍。
        connect(m_codeBlock, &GalleryCodeBlock::layoutHeightChanged, this,
                [this]() { updateCardHeight(); });
    }

    using Edge = AnchorLayout::Edge;
    AnchorLayout::Anchors titleAnchors;
    titleAnchors.left = {this, Edge::Left, kCardLeftMargin};
    titleAnchors.right = {this, Edge::Right, -kCardRightMargin};
    titleAnchors.top = {this, Edge::Top, kCardTopMargin};
    layout->addAnchoredWidget(m_titleLabel, titleAnchors);

    QWidget* previous = m_titleLabel;
    if (m_descriptionLabel) {
        AnchorLayout::Anchors descriptionAnchors;
        descriptionAnchors.left = {this, Edge::Left, kCardLeftMargin};
        descriptionAnchors.right = {this, Edge::Right, -kCardRightMargin};
        descriptionAnchors.top = {previous, Edge::Bottom, kCardSpacing};
        layout->addAnchoredWidget(m_descriptionLabel, descriptionAnchors);
        previous = m_descriptionLabel;
    }

    AnchorLayout::Anchors previewAnchors;
    previewAnchors.left = {this, Edge::Left, kCardLeftMargin};
    previewAnchors.right = {this, Edge::Right, -kCardRightMargin};
    previewAnchors.top = {previous, Edge::Bottom, kCardSpacing};
    layout->addAnchoredWidget(m_previewSurface, previewAnchors);
    previous = m_previewSurface;

    if (m_codeBlock) {
        AnchorLayout::Anchors codeAnchors;
        codeAnchors.left = {this, Edge::Left, kCardLeftMargin};
        codeAnchors.right = {this, Edge::Right, -kCardRightMargin};
        codeAnchors.top = {previous, Edge::Bottom, kCardSpacing};
        layout->addAnchoredWidget(m_codeBlock, codeAnchors);
    }

    applyPalette();
    updateAnchoredLayout();
}

void GallerySampleCard::onThemeUpdated()
{
    applyPalette();
    if (m_titleLabel)
        m_titleLabel->onThemeUpdated();
    if (m_descriptionLabel)
        m_descriptionLabel->onThemeUpdated();
    if (m_codeBlock)
        m_codeBlock->onThemeUpdated();
    updateAnchoredLayout();
}

QSize GallerySampleCard::sizeHint() const
{
    const int preferredWidth = qMax(width(), kDefaultCardWidth);
    return QSize(preferredWidth, calculatedHeightForWidth(preferredWidth));
}

QSize GallerySampleCard::minimumSizeHint() const
{
    return QSize(kMinimumCardWidth, calculatedHeightForWidth(kMinimumCardWidth));
}

bool GallerySampleCard::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == m_previewSurface || watched == m_preview || watched == m_options)
        && event->type() == QEvent::LayoutRequest) {
        queueAnchoredLayoutUpdate();
    }
    return QFrame::eventFilter(watched, event);
}

void GallerySampleCard::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    updateAnchoredLayout();
}

void GallerySampleCard::queueAnchoredLayoutUpdate()
{
    if (m_layoutUpdateQueued)
        return;

    m_layoutUpdateQueued = true;
    QTimer::singleShot(0, this, [this]() {
        m_layoutUpdateQueued = false;
        updateAnchoredLayout();
    });
}

void GallerySampleCard::updateAnchoredLayout()
{
    const int cardWidth = qMax(width(), kMinimumCardWidth);
    const int contentWidth = contentWidthForCardWidth(cardWidth);

    auto setStableHeight = [this, contentWidth](QWidget* widget) {
        if (!widget)
            return;
        const int height = preferredHeightForWidget(widget, contentWidth);
        if (widget->minimumHeight() != height || widget->maximumHeight() != height)
            widget->setFixedHeight(height);
    };

    setStableHeight(m_titleLabel);
    setStableHeight(m_descriptionLabel);
    setStableHeight(m_previewSurface);

    updateCardHeight();

    if (layout())
        layout()->activate();
    updateGeometry();
}

void GallerySampleCard::updateCardHeight()
{
    const int cardWidth = qMax(width(), kMinimumCardWidth);
    const int cardHeight = calculatedHeightForWidth(cardWidth);
    if (minimumHeight() == cardHeight && maximumHeight() == cardHeight)
        return;
    setFixedHeight(cardHeight);
    // The code block has already resized itself by the time this runs, but the
    // card and the page column only follow on the queued layout pass. A frame
    // composited in between paints the block overflowing the card while the
    // cards below sit still — the intermittent toggle jitter. Deliver the
    // queued layout passes now so every painted frame is geometry-consistent.
    // zh_CN: 走到这里时代码块已先行改变了自身高度，而卡片和页面纵列要等排队的
    // 布局事件才跟上；若系统恰好在两者之间合成一帧，就会画出代码块溢出卡片、
    // 下方卡片纹丝不动的画面——即偶现的切换抖动。此处立刻派发排队的布局事件，
    // 保证每一帧画面几何一致。
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
}

int GallerySampleCard::preferredHeightForWidget(QWidget* widget, int width) const
{
    if (!widget)
        return 0;
    if (width > 0 && widget->hasHeightForWidth())
        return qMax(0, widget->heightForWidth(width));
    return qMax(0, widget->sizeHint().height());
}

int GallerySampleCard::calculatedHeightForWidth(int width) const
{
    const int contentWidth = contentWidthForCardWidth(width);
    int height = kCardTopMargin + kCardBottomMargin;
    int visibleItemCount = 0;

    auto addWidgetHeight = [&](QWidget* widget) {
        if (!widget)
            return;
        if (visibleItemCount > 0)
            height += kCardSpacing;
        height += preferredHeightForWidget(widget, contentWidth);
        ++visibleItemCount;
    };

    addWidgetHeight(m_titleLabel);
    addWidgetHeight(m_descriptionLabel);
    addWidgetHeight(m_previewSurface);
    addWidgetHeight(m_codeBlock);

    return height;
}

int GallerySampleCard::contentWidthForCardWidth(int width) const
{
    return qMax(0, width - kCardLeftMargin - kCardRightMargin);
}

void GallerySampleCard::applyPalette()
{
    const Colors colors = themeColors();
    setStyleSheet(QStringLiteral(
                      "#gallerySampleCard { background: %1; border: 1px solid %2; border-radius: %3px; }")
                      .arg(cssColor(colors.bgLayer),
                           cssColor(colors.strokeCard))
                      .arg(::CornerRadius::Overlay));
    if (m_previewSurface) {
        m_previewSurface->setStyleSheet(QStringLiteral(
                                            "#gallerySampleCardPreview { background: %1; border: 1px solid %2; border-radius: %3px; }")
                                            .arg(cssColor(colors.bgLayerAlt),
                                                 cssColor(colors.strokeCard))
                                            .arg(::CornerRadius::Control));
    }
}

} // namespace fluent::gallery
