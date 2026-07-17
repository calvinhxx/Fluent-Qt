#include "GallerySampleCard.h"

#include <QColor>
#include <QEvent>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QVariant>
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
constexpr char kThemeOverrideProperty[] = "fluentThemeOverride";

class SamplePreviewSurface final : public QFrame, public fluent::FluentElement {
public:
    explicit SamplePreviewSurface(QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setObjectName(QStringLiteral("gallerySampleCardPreview"));
        setFrameShape(QFrame::NoFrame);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        onThemeUpdated();
    }

    void onThemeUpdated() override
    {
        const Colors colors = themeColors();
        const QString style = QStringLiteral(
                                  "#gallerySampleCardPreview { background: %1; border: 1px solid %2; border-radius: %3px; }")
                                  .arg(cssColor(colors.bgLayerAlt),
                                       cssColor(colors.strokeCard))
                                  .arg(::CornerRadius::Control);
        if (styleSheet() != style)
            setStyleSheet(style);
        if (property("fluentSurfaceColor").value<QColor>() != colors.bgLayerAlt)
            setProperty("fluentSurfaceColor", colors.bgLayerAlt);
        update();
    }
};

void refreshFluentSubtree(QWidget* root)
{
    if (!root)
        return;

    if (auto* element = dynamic_cast<fluent::FluentElement*>(root))
        element->onThemeUpdated();
    root->update();

    const auto children = root->children();
    for (QObject* child : children) {
        if (auto* childWidget = qobject_cast<QWidget*>(child))
            refreshFluentSubtree(childWidget);
    }
}
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

    m_previewSurface = new SamplePreviewSurface(this);
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
        connect(m_codeBlock, &GalleryCodeBlock::layoutHeightChanged, this,
                [this]() { updateCodeBlockTransitionLayout(); });
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
    refreshPreviewTheme();
}

void GallerySampleCard::setPreviewThemeOverride(fluent::FluentElement::Theme theme)
{
    if (!m_previewSurface)
        return;

    const QVariant currentOverride = m_previewSurface->property(kThemeOverrideProperty);
    if (currentOverride.isValid() && currentOverride.toInt() == static_cast<int>(theme))
        return;
    m_previewSurface->setProperty(kThemeOverrideProperty, static_cast<int>(theme));
    refreshPreviewTheme();
}

void GallerySampleCard::clearPreviewThemeOverride()
{
    if (!m_previewSurface)
        return;

    if (!m_previewSurface->property(kThemeOverrideProperty).isValid())
        return;
    m_previewSurface->setProperty(kThemeOverrideProperty, QVariant());
    refreshPreviewTheme();
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

bool GallerySampleCard::updateCardHeight()
{
    const int cardWidth = qMax(width(), kMinimumCardWidth);
    const int cardHeight = calculatedHeightForWidth(cardWidth);
    if (minimumHeight() == cardHeight && maximumHeight() == cardHeight)
        return false;
    setFixedHeight(cardHeight);
    return true;
}

void GallerySampleCard::updateCodeBlockTransitionLayout()
{
    updateCardHeight();
    updateGeometry();

    // Finish the card's own geometry first. GalleryCodeBlock synchronizes the
    // enclosing scroll page after every direct layoutHeightChanged slot returns,
    // so the page observes the new card height in this same animation frame.
    if (layout()) {
        layout()->invalidate();
        layout()->activate();
    }
}

void GallerySampleCard::refreshPreviewTheme()
{
    refreshFluentSubtree(m_previewSurface);
    updateAnchoredLayout();
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
    const QString cardStyle = QStringLiteral(
                                  "#gallerySampleCard { background: %1; border: 1px solid %2; border-radius: %3px; }")
                                  .arg(cssColor(colors.bgLayer),
                                       cssColor(colors.strokeCard))
                                  .arg(::CornerRadius::Overlay);
    if (styleSheet() != cardStyle)
        setStyleSheet(cardStyle);
    // Color the text via each label's OWN style sheet, not the palette: this card sets a style sheet
    // on itself, which installs QStyleSheetStyle over the whole subtree and makes a child Label's
    // palette-based WindowText color (Label::onThemeUpdated) be ignored — so the title/description
    // rendered as near-black on the dark card. Setting the color on the label directly (as
    // GalleryEntryCard already does) wins regardless of the ancestor style sheet.
    // zh_CN: 用每个标签自身的样式表上色，而非 palette：本卡片给自己设了样式表，会在整个子树上安装 QStyleSheetStyle，
    // 使子 Label 基于 palette 的 WindowText 颜色（Label::onThemeUpdated）被忽略——于是标题/描述在深色卡片上渲染成近黑。
    // 直接给标签设颜色（与 GalleryEntryCard 一致）则无视祖先样式表生效。
    if (m_titleLabel) {
        const QString titleStyle = QStringLiteral("color: %1; background: transparent;")
                                       .arg(cssColor(colors.textPrimary));
        if (m_titleLabel->styleSheet() != titleStyle)
            m_titleLabel->setStyleSheet(titleStyle);
    }
    if (m_descriptionLabel) {
        const QString descriptionStyle = QStringLiteral("color: %1; background: transparent;")
                                             .arg(cssColor(colors.textSecondary));
        if (m_descriptionLabel->styleSheet() != descriptionStyle)
            m_descriptionLabel->setStyleSheet(descriptionStyle);
    }
}

} // namespace fluent::gallery
