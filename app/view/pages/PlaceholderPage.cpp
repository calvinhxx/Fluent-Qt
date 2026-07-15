#include "PlaceholderPage.h"

#include <QFrame>
#include <QPalette>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "support/logging/Log.h"

namespace fluent::gallery {

PlaceholderPage::PlaceholderPage(const GalleryNavigationItem& item, QWidget* parent)
    : QWidget(parent)
    , m_routeId(item.id)
    , m_title(item.title)
    , m_placeholderColor(item.placeholderColor)
{
    setObjectName(QStringLiteral("galleryPlaceholderPage"));
    setProperty("galleryRouteId", m_routeId);
    setAutoFillBackground(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(48, 34, 48, 48);
    layout->setSpacing(18);

    m_titleLabel = new fluent::textfields::Label(m_title, this);
    m_titleLabel->setObjectName(QStringLiteral("galleryPlaceholderTitle"));
    m_titleLabel->setProperty("galleryRouteId", m_routeId);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_titleLabel->setFluentTypography(Typography::FontRole::Title);
    m_titleLabel->setWordWrap(true);

    layout->addWidget(m_titleLabel);
    layout->addWidget(createPlaceholderCard(QStringLiteral("Overview"),
                                            QStringLiteral("This page is reserved for the %1 gallery experience.").arg(m_title)));
    layout->addWidget(createPlaceholderCard(QStringLiteral("Examples"),
                                            QStringLiteral("Component examples, API notes, and live preview content will be added here.")));
    layout->addStretch(1);

    applyPalette();
    LOG_DEBUG(QStringLiteral("PlaceholderPage created routeId=%1 title=%2 color=%3")
                  .arg(m_routeId, m_title, m_placeholderColor.name(QColor::HexRgb)));
}

void PlaceholderPage::onThemeUpdated()
{
    applyPalette();
    if (m_titleLabel)
        m_titleLabel->onThemeUpdated();
}

void PlaceholderPage::applyPalette()
{
    QPalette currentPalette = palette();
    currentPalette.setColor(QPalette::Window, m_placeholderColor);
    setPalette(currentPalette);

    if (m_titleLabel)
        m_titleLabel->setStyleSheet(QStringLiteral("#galleryPlaceholderTitle { color: #1A1A1A; font-weight: 600; }"));
}

QWidget* PlaceholderPage::createPlaceholderCard(const QString& title, const QString& body)
{
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("galleryPlaceholderCard"));
    card->setFrameShape(QFrame::NoFrame);
    card->setMinimumHeight(92);
    card->setStyleSheet(QStringLiteral(
        "#galleryPlaceholderCard { background: rgba(255, 255, 255, 225); border: 1px solid #E5E5E5; border-radius: 8px; }"));

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 14, 20, 14);
    layout->setSpacing(4);

    auto* titleLabel = new fluent::textfields::Label(title, card);
    titleLabel->setFluentTypography(Typography::FontRole::BodyStrong);
    auto* bodyLabel = new fluent::textfields::Label(body, card);
    bodyLabel->setFluentTypography(Typography::FontRole::Body);
    bodyLabel->setWordWrap(true);

    layout->addWidget(titleLabel);
    layout->addWidget(bodyLabel);
    return card;
}

} // namespace fluent::gallery
