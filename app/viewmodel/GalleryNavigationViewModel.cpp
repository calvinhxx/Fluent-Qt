#include "GalleryNavigationViewModel.h"

#include "model/GalleryComponentCatalog.h"
#include "design/Typography.h"
#include "utils/Log.h"

namespace fluent::gallery {

namespace {

GalleryNavigationItem node(GalleryNavigationItem::Kind kind,
                           const QString& id,
                           const QString& title,
                           const QString& group,
                           const QString& iconGlyph,
                           const QColor& color,
                           const QString& parentId = QString(),
                           int depth = 0,
                           bool expandable = false)
{
    GalleryNavigationItem item;
    item.id = id;
    item.title = title;
    item.group = group;
    item.parentId = parentId;
    item.iconGlyph = iconGlyph;
    item.placeholderColor = color;
    item.kind = kind;
    item.depth = depth;
    item.expandable = expandable;
    return item;
}

GalleryNavigationItem section(const QString& title)
{
    GalleryNavigationItem item;
    item.title = title;
    item.kind = GalleryNavigationItem::Kind::SectionHeader;
    item.sectionHeader = true;
    return item;
}

QColor componentColor(int categoryIndex, int componentIndex)
{
    static const QVector<QColor> colors{
        QColor(QStringLiteral("#F2FBF8")),
        QColor(QStringLiteral("#FFF6FA")),
        QColor(QStringLiteral("#F3FBFF")),
        QColor(QStringLiteral("#FFF9EA")),
        QColor(QStringLiteral("#F8F8F8")),
        QColor(QStringLiteral("#F0FBFA")),
        QColor(QStringLiteral("#F8F5FF")),
        QColor(QStringLiteral("#F6FAF2")),
        QColor(QStringLiteral("#F7F7FF")),
        QColor(QStringLiteral("#F3F7FA"))
    };
    return colors.at((categoryIndex + componentIndex) % colors.size());
}

} // namespace

GalleryNavigationViewModel::GalleryNavigationViewModel()
{
    m_items.append(node(GalleryNavigationItem::Kind::RootRoute,
                        QStringLiteral("home"),
                        QStringLiteral("Home"),
                        QStringLiteral("Root"),
                        Typography::Icons::Home,
                        QColor(QStringLiteral("#F4F8FF"))));
    m_items.append(section(QStringLiteral("Controls")));
    m_items.append(node(GalleryNavigationItem::Kind::RootRoute,
                        QStringLiteral("all-controls"),
                        QStringLiteral("All"),
                        QStringLiteral("Controls"),
                        Typography::Icons::AllApps,
                        QColor(QStringLiteral("#F8F8FF")),
                        QStringLiteral("controls")));

    const QVector<GalleryComponentCategory>& catalog = galleryComponentCatalog();
    for (int categoryIndex = 0; categoryIndex < catalog.size(); ++categoryIndex) {
        const GalleryComponentCategory& category = catalog.at(categoryIndex);
        m_items.append(node(GalleryNavigationItem::Kind::CategoryRoute,
                            category.id,
                            category.title,
                            QStringLiteral("Controls"),
                            category.iconGlyph,
                            componentColor(categoryIndex, 0),
                            QStringLiteral("controls"),
                            0,
                            !category.components.isEmpty()));
        for (int componentIndex = 0; componentIndex < category.components.size(); ++componentIndex) {
            const GalleryComponentEntry& component = category.components.at(componentIndex);
            m_items.append(node(GalleryNavigationItem::Kind::ComponentRoute,
                                component.id,
                                component.title,
                                category.title,
                                component.iconGlyph,
                                componentColor(categoryIndex, componentIndex + 1),
                                category.id,
                                1));
        }
    }

    m_items.append(node(GalleryNavigationItem::Kind::FooterRoute,
                        QStringLiteral("settings"),
                        QStringLiteral("Settings"),
                        QStringLiteral("Footer"),
                        Typography::Icons::Settings,
                        QColor(QStringLiteral("#F7F7F7"))));

    int routeCount = 0;
    int categoryCount = 0;
    int componentCount = 0;
    int footerCount = 0;
    for (const GalleryNavigationItem& item : m_items) {
        switch (item.kind) {
        case GalleryNavigationItem::Kind::SectionHeader:
            break;
        case GalleryNavigationItem::Kind::CategoryRoute:
            ++routeCount;
            ++categoryCount;
            break;
        case GalleryNavigationItem::Kind::ComponentRoute:
            ++routeCount;
            ++componentCount;
            break;
        case GalleryNavigationItem::Kind::FooterRoute:
            ++routeCount;
            ++footerCount;
            break;
        case GalleryNavigationItem::Kind::RootRoute:
            ++routeCount;
            break;
        }
    }

    LOG_DEBUG(QStringLiteral("GalleryNavigationViewModel build catalogCategories=%1 totalItems=%2 routes=%3 categories=%4 components=%5 footerRoutes=%6 defaultRoute=%7")
                  .arg(catalog.size())
                  .arg(m_items.size())
                  .arg(routeCount)
                  .arg(categoryCount)
                  .arg(componentCount)
                  .arg(footerCount)
                  .arg(defaultRouteId()));
}

QVector<GalleryNavigationItem> GalleryNavigationViewModel::mainPaneItems() const
{
    QVector<GalleryNavigationItem> paneItems;
    for (const GalleryNavigationItem& item : m_items) {
        if (item.kind != GalleryNavigationItem::Kind::FooterRoute)
            paneItems.append(item);
    }
    return paneItems;
}

QVector<GalleryNavigationItem> GalleryNavigationViewModel::footerPaneItems() const
{
    QVector<GalleryNavigationItem> paneItems;
    if (const GalleryNavigationItem* settingsItem = itemById(QStringLiteral("settings")))
        paneItems.append(*settingsItem);
    return paneItems;
}

QStringList GalleryNavigationViewModel::navigationEntryIds() const
{
    QStringList ids;
    for (const GalleryNavigationItem& item : m_items) {
        if (item.kind != GalleryNavigationItem::Kind::SectionHeader)
            ids.append(item.id);
    }
    return ids;
}

QStringList GalleryNavigationViewModel::visibleTitles() const
{
    QStringList titles;
    for (const GalleryNavigationItem& item : m_items)
        titles.append(item.title);
    return titles;
}

QString GalleryNavigationViewModel::defaultRouteId() const
{
    return QStringLiteral("home");
}

const GalleryNavigationItem* GalleryNavigationViewModel::itemById(const QString& id) const
{
    for (const GalleryNavigationItem& item : m_items) {
        if (item.kind != GalleryNavigationItem::Kind::SectionHeader && item.id == id)
            return &item;
    }
    return nullptr;
}

QString GalleryNavigationViewModel::parentRouteId(const QString& id) const
{
    if (const GalleryNavigationItem* item = itemById(id))
        return item->parentId;
    return QString();
}

} // namespace fluent::gallery
