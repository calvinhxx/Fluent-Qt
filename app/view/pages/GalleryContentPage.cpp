#include "GalleryContentPage.h"

#include <QEvent>
#include <QFrame>
#include <QPalette>
#include <QVBoxLayout>

#include "components/scrolling/ScrollView.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "view/support/GalleryStyleSupport.h"

namespace fluent::gallery {

GalleryContentPage::GalleryContentPage(const QString& routeId,
                                       const QString& title,
                                       const QString& subtitle,
                                       QWidget* parent)
    : QWidget(parent)
    , m_routeId(routeId)
    , m_title(title)
    , m_subtitle(subtitle)
{
    setObjectName(QStringLiteral("galleryContentPage"));
    setProperty("galleryRouteId", m_routeId);
    setAutoFillBackground(true);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_scrollArea = new fluent::scrolling::ScrollView(this);
    m_scrollArea->setObjectName(QStringLiteral("galleryContentScrollArea"));
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollMode(fluent::scrolling::ScrollView::ScrollMode::Disabled);
    m_scrollArea->setHorizontalScrollBarVisibility(
        fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);
    // Reserve the vertical scrollbar gutter even before content overflows. With Qt's
    // ScrollBarAsNeeded policy the viewport width changes on the first code-block expansion,
    // making the entire sample column flash/shrink horizontally.
    // zh_CN: 提前保留垂直滚动条槽位。Qt 的按需策略会在代码块首次展开时改变 viewport 宽度，
    // 导致整列示例卡片横向闪缩。
    m_scrollArea->setVerticalScrollBarVisibility(
        fluent::scrolling::ScrollView::ScrollBarVisibility::Visible);

    // The scroll area + its internal clip viewport are transparent so the window's Mica
    // backdrop shows through behind the page. zh_CN: 滚动区及其内部裁剪视口透明，使窗口 Mica 背景在页面之后透出。
    m_scrollArea->setAutoFillBackground(false);
    if (QWidget* clipViewport = m_scrollArea->viewport()) {
        clipViewport->setAutoFillBackground(false);
        clipViewport->setAttribute(Qt::WA_TranslucentBackground, true);
    }

    m_viewport = new QWidget(m_scrollArea);
    m_viewport->setObjectName(QStringLiteral("galleryContentViewport"));
    m_contentLayout = new QVBoxLayout(m_viewport);
    m_contentLayout->setContentsMargins(48, 34, 48, 48);
    m_contentLayout->setSpacing(16);

    m_titleLabel = new fluent::textfields::Label(m_title, m_viewport);
    m_titleLabel->setObjectName(QStringLiteral("galleryContentTitle"));
    m_titleLabel->setProperty("galleryRouteId", m_routeId);
    m_titleLabel->setFluentTypography(Typography::FontRole::Title);
    m_titleLabel->setWordWrap(true);
    m_contentLayout->addWidget(m_titleLabel);

    if (!m_subtitle.isEmpty()) {
        m_subtitleLabel =
            createTrackedLabel(m_subtitle, Typography::FontRole::Body, TextRole::Secondary);
        m_subtitleLabel->setObjectName(QStringLiteral("galleryContentSubtitle"));
        m_subtitleLabel->setWordWrap(true);
        m_contentLayout->addWidget(m_subtitleLabel);
    }

    m_contentLayout->addStretch(1);

    m_scrollArea->setWidget(m_viewport);
    outerLayout->addWidget(m_scrollArea);

    applyPalette();
}

fluent::textfields::Label* GalleryContentPage::addSectionHeader(const QString& text)
{
    fluent::textfields::Label* header =
        createTrackedLabel(text, Typography::FontRole::Subtitle, TextRole::Primary);
    header->setObjectName(QStringLiteral("galleryContentSectionHeader"));
    addContentSpacing(8);
    addContentWidget(header);
    return header;
}

fluent::textfields::Label* GalleryContentPage::addBodyText(const QString& text)
{
    fluent::textfields::Label* body =
        createTrackedLabel(text, Typography::FontRole::Body, TextRole::Secondary);
    body->setObjectName(QStringLiteral("galleryContentBody"));
    body->setWordWrap(true);
    addContentWidget(body);
    return body;
}

void GalleryContentPage::addContentWidget(QWidget* widget)
{
    // Insert before the trailing stretch so content stacks top-down.
    m_contentLayout->insertWidget(m_contentLayout->count() - 1, widget);
}

void GalleryContentPage::addContentSpacing(int pixels)
{
    m_contentLayout->insertSpacing(m_contentLayout->count() - 1, pixels);
}

void GalleryContentPage::setPageHeaderVisible(bool visible)
{
    if (m_titleLabel)
        m_titleLabel->setVisible(visible);
    if (m_subtitleLabel)
        m_subtitleLabel->setVisible(visible);
}

void GalleryContentPage::setViewportMargins(const QMargins& margins)
{
    m_contentLayout->setContentsMargins(margins);
}

void GalleryContentPage::setContentSpacing(int spacing)
{
    m_contentLayout->setSpacing(spacing);
}

void GalleryContentPage::onThemeUpdated()
{
    applyPalette();
    if (m_titleLabel)
        m_titleLabel->onThemeUpdated();
    for (const TrackedLabel& tracked : m_trackedLabels) {
        if (tracked.label)
            tracked.label->onThemeUpdated();
    }
}

fluent::textfields::Label* GalleryContentPage::createTrackedLabel(const QString& text,
                                                                  const QString& fontRole,
                                                                  TextRole textRole)
{
    auto* label = new fluent::textfields::Label(text, m_viewport);
    label->setFluentTypography(fontRole);
    m_trackedLabels.append({label, textRole});
    return label;
}

void GalleryContentPage::applyBackdrop()
{
    // The whole window is a real Mica surface, so the page is transparent and shows the OS
    // backdrop; opaque cards/controls float on top. (A distinct opaque content layer/frame
    // isn't feasible here: on a translucent window every content widget's unpainted area
    // clears to Mica — which is also why card gaps already show Mica.)
    // zh_CN: 整窗是真实 Mica 表面，故页面透明、露出系统背景；不透明卡片/控件浮于其上。（此处无法做出独立的
    // 不透明内容层/框：半透明窗口下每个内容控件的未绘制区都会清成 Mica——这也是卡片间隙已透出 Mica 的原因。）
    setAutoFillBackground(false);
    QPalette pagePalette = palette();
    pagePalette.setColor(QPalette::Window, Qt::transparent);
    setPalette(pagePalette);
    setStyleSheet(QStringLiteral(
                      "#galleryContentPage, #galleryContentViewport { background: transparent; }"));
}

void GalleryContentPage::applyPalette()
{
    const Colors colors = themeColors();

    applyBackdrop();

    if (m_titleLabel) {
        m_titleLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                        .arg(cssColor(colors.textPrimary)));
    }
    for (const TrackedLabel& tracked : m_trackedLabels) {
        if (!tracked.label)
            continue;
        const QColor color = tracked.role == TextRole::Primary
            ? colors.textPrimary
            : colors.textSecondary;
        tracked.label->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                         .arg(cssColor(color)));
    }
}

} // namespace fluent::gallery
