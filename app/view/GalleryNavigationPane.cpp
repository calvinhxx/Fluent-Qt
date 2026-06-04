#include "GalleryNavigationPane.h"

#include <QHBoxLayout>
#include <QStyle>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"

namespace fluent::gallery {

GalleryNavigationPane::GalleryNavigationPane(const QVector<GalleryNavigationItem>& items, QWidget* parent)
    : QWidget(parent)
    , m_items(items)
{
    setObjectName(QStringLiteral("galleryNavigationPane"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rebuild();
}

QStringList GalleryNavigationPane::routeIds() const
{
    QStringList ids;
    for (const GalleryNavigationItem& item : m_items) {
        if (!item.sectionHeader)
            ids.append(item.id);
    }
    return ids;
}

QStringList GalleryNavigationPane::visibleTitles() const
{
    QStringList titles;
    for (const GalleryNavigationItem& item : m_items)
        titles.append(item.title);
    return titles;
}

bool GalleryNavigationPane::containsRoute(const QString& routeId) const
{
    return m_buttons.contains(routeId);
}

void GalleryNavigationPane::setSelectedRouteId(const QString& routeId)
{
    if (m_selectedRouteId == routeId)
        return;
    m_selectedRouteId = routeId;
    updateButtonStyles();
}

void GalleryNavigationPane::onThemeUpdated()
{
    updateButtonStyles();
}

void GalleryNavigationPane::rebuild()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 12, 6, 6);
    layout->setSpacing(2);

    for (const GalleryNavigationItem& item : m_items) {
        if (item.sectionHeader) {
            layout->addWidget(createSectionHeader(item));
        } else {
            auto* row = new QWidget(this);
            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(6 + item.depth * 18, 0, 0, 0);
            rowLayout->setSpacing(0);
            rowLayout->addWidget(createRouteButton(item));
            layout->addWidget(row);
        }
    }

    layout->addStretch(1);
    updateButtonStyles();
}

void GalleryNavigationPane::updateButtonStyles()
{
    for (auto iterator = m_buttons.begin(); iterator != m_buttons.end(); ++iterator) {
        fluent::basicinput::Button* button = iterator.value();
        if (!button)
            continue;
        const bool selected = iterator.key() == m_selectedRouteId;
        button->setFluentStyle(fluent::basicinput::Button::Subtle);
        button->setFocusVisual(selected);
        button->setProperty("gallerySelected", selected);
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
}

QWidget* GalleryNavigationPane::createSectionHeader(const GalleryNavigationItem& item)
{
    auto* label = new fluent::textfields::Label(item.title, this);
    label->setObjectName(QStringLiteral("galleryNavigationSection_%1").arg(item.title));
    label->setFluentTypography(Typography::FontRole::BodyStrong);
    label->setFixedHeight(34);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setContentsMargins(10, 8, 0, 0);
    return label;
}

fluent::basicinput::Button* GalleryNavigationPane::createRouteButton(const GalleryNavigationItem& item)
{
    const QString text = item.expandable
        ? QStringLiteral("%1    %2").arg(item.title, Typography::Icons::ChevronDownMed)
        : item.title;
    auto* button = new fluent::basicinput::Button(text, this);
    button->setObjectName(QStringLiteral("galleryNavigation_%1").arg(item.id));
    button->setProperty("galleryRouteId", item.id);
    button->setProperty("galleryExpandable", item.expandable);
    button->setFluentStyle(fluent::basicinput::Button::Subtle);
    button->setFluentSize(fluent::basicinput::Button::StandardSize);
    button->setFluentLayout(item.iconGlyph.isEmpty()
                                ? fluent::basicinput::Button::TextOnly
                                : fluent::basicinput::Button::IconBefore);
    if (!item.iconGlyph.isEmpty())
        button->setIconGlyph(item.iconGlyph, 16);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setMinimumHeight(36);
    button->setToolTip(item.title);

    m_buttons.insert(item.id, button);
    connect(button, &QPushButton::clicked, this, [this, routeId = item.id]() {
        emit routeActivated(routeId);
    });
    return button;
}

} // namespace fluent::gallery
