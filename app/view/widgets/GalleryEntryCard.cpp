#include "GalleryEntryCard.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QVBoxLayout>

#include "components/textfields/Label.h"
#include "design/CornerRadius.h"
#include "design/Typography.h"
#include "support/logging/Log.h"
#include "view/support/GalleryStyleSupport.h"
#include "view/widgets/GalleryIconTile.h"

namespace fluent::gallery {

GalleryEntryCard::GalleryEntryCard(const QString& targetRouteId,
                                   const QString& title,
                                   const QString& description,
                                   QWidget* parent)
    : QFrame(parent)
    , m_targetRouteId(targetRouteId)
{
    setObjectName(QStringLiteral("galleryEntryCard"));
    setProperty("galleryTargetRouteId", m_targetRouteId);
    setFrameShape(QFrame::NoFrame);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(86);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);

    m_iconTile = new GalleryIconTile(title, this);
    layout->addWidget(m_iconTile, 0, Qt::AlignTop);

    auto* textColumn = new QVBoxLayout;
    textColumn->setContentsMargins(0, 0, 0, 0);
    textColumn->setSpacing(3);

    m_titleLabel = new fluent::textfields::Label(title, this);
    m_titleLabel->setObjectName(QStringLiteral("galleryEntryCardTitle"));
    m_titleLabel->setProperty("galleryTargetRouteId", m_targetRouteId);
    m_titleLabel->setFluentTypography(Typography::FontRole::BodyStrong);
    textColumn->addWidget(m_titleLabel);

    if (!description.isEmpty()) {
        m_descriptionLabel = new fluent::textfields::Label(description, this);
        m_descriptionLabel->setObjectName(QStringLiteral("galleryEntryCardDescription"));
        m_descriptionLabel->setFluentTypography(Typography::FontRole::Caption);
        m_descriptionLabel->setWordWrap(true);
        textColumn->addWidget(m_descriptionLabel);
    }
    textColumn->addStretch(1);

    layout->addLayout(textColumn, 1);

    applyPalette();
}

void GalleryEntryCard::setIconGlyph(const QString& glyph)
{
    if (m_iconTile)
        m_iconTile->setIconGlyph(glyph);
}

void GalleryEntryCard::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        // Click-source anchor: distinguishes card navigation from nav-pane clicks
        // when tracing a route change.
        // zh_CN: 点击来源锚点：追踪路由变化时区分卡片导航与导航面板点击。
        LOG_DEBUG(QStringLiteral("GalleryEntryCard activated routeId=%1").arg(m_targetRouteId));
        emit activated(m_targetRouteId);
    }
    QFrame::mouseReleaseEvent(event);
}

void GalleryEntryCard::onThemeUpdated()
{
    applyPalette();
    if (m_titleLabel)
        m_titleLabel->onThemeUpdated();
    if (m_descriptionLabel)
        m_descriptionLabel->onThemeUpdated();
}

void GalleryEntryCard::applyPalette()
{
    const Colors colors = themeColors();
    setStyleSheet(QStringLiteral(
                      "#galleryEntryCard { background: %1; border: 1px solid %2; border-radius: %3px; }"
                      "#galleryEntryCard:hover { background: %4; }")
                      .arg(cssColor(colors.bgLayer),
                           cssColor(colors.strokeCard))
                      .arg(::CornerRadius::Overlay)
                      .arg(cssColor(colors.subtleSecondary)));
    if (m_titleLabel)
        m_titleLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                        .arg(cssColor(colors.textPrimary)));
    if (m_descriptionLabel)
        m_descriptionLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                              .arg(cssColor(colors.textSecondary)));
}

} // namespace fluent::gallery
