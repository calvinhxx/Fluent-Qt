#include "GalleryNavigationViewModel.h"

#include "model/GalleryComponentCatalog.h"
#include "design/Typography.h"
#include "support/logging/Log.h"

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

    // Foundation mirrors the WinUI Gallery "Design / Fundamentals" area: a landing
    // route (feature cards) with one expandable child per design-token topic. Each
    // child is a ComponentRoute so it renders indented under the parent, exactly like
    // a control under its category. No section header — the "Foundation" item names
    // itself, so a "Foundation" header above it would just be a duplicate label.
    // zh_CN: Foundation 对标 WinUI Gallery 的「Design / Fundamentals」区：一个落地路由（特性卡片）
    // 加每个设计 token 主题一个可展开子项。子项用 ComponentRoute，使其缩进显示在父项下，
    // 与分类下的控件一致。不加分区头——「Foundation」项本身就是名字，再加「Foundation」头只会重复。
    m_items.append(node(GalleryNavigationItem::Kind::CategoryRoute,
                        QStringLiteral("foundation"),
                        QStringLiteral("Foundation"),
                        QStringLiteral("Foundation"),
                        Typography::Icons::Color,
                        QColor(QStringLiteral("#F3F1FB")),
                        QString(),   // top-level category: no parent (not itself). zh_CN: 顶级分类，无父项（不是自己）。
                        0,
                        true));
    const struct {
        const char* id;
        const char* title;
    } foundationTopics[] = {
        {"foundation-qmlplus", "QML+"},
        {"foundation-typography", "Typography"},
        {"foundation-color", "Color"},
        {"foundation-iconography", "Iconography"},
        {"foundation-geometry", "Geometry & spacing"},
    };
    for (const auto& topic : foundationTopics) {
        m_items.append(node(GalleryNavigationItem::Kind::ComponentRoute,
                            QString::fromLatin1(topic.id),
                            QString::fromLatin1(topic.title),
                            QStringLiteral("Foundation"),
                            QString(),
                            QColor(QStringLiteral("#F7F6FD")),
                            QStringLiteral("foundation"),
                            1));
    }

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
