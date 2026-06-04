#include "GalleryNavigationViewModel.h"

#include "design/Typography.h"

namespace fluent::gallery {

namespace {

GalleryNavigationItem route(const QString& id,
                            const QString& title,
                            const QString& group,
                            const QString& iconGlyph,
                            const QColor& color,
                            int depth = 0,
                            bool expandable = false)
{
    GalleryNavigationItem item;
    item.id = id;
    item.title = title;
    item.group = group;
    item.iconGlyph = iconGlyph;
    item.placeholderColor = color;
    item.depth = depth;
    item.expandable = expandable;
    return item;
}

GalleryNavigationItem section(const QString& title)
{
    GalleryNavigationItem item;
    item.title = title;
    item.sectionHeader = true;
    return item;
}

} // namespace

GalleryNavigationViewModel::GalleryNavigationViewModel()
    : m_items({
          route(QStringLiteral("home"),
                QStringLiteral("Home"),
                QStringLiteral("Root"),
                Typography::Icons::Home,
                QColor(QStringLiteral("#F4F8FF"))),
          route(QStringLiteral("fundamentals"),
                QStringLiteral("Fundamentals"),
                QStringLiteral("Root"),
                Typography::Icons::Document,
                QColor(QStringLiteral("#F5FAEF")),
                0,
                true),
          route(QStringLiteral("design"),
                QStringLiteral("Design"),
                QStringLiteral("Root"),
                Typography::Icons::Brush,
                QColor(QStringLiteral("#FBF7FF")),
                0,
                true),
          route(QStringLiteral("accessibility"),
                QStringLiteral("Accessibility"),
                QStringLiteral("Root"),
                Typography::Icons::People,
                QColor(QStringLiteral("#FFF8EF")),
                0,
                true),
          section(QStringLiteral("Controls")),
          route(QStringLiteral("all-controls"),
                QStringLiteral("All"),
                QStringLiteral("Controls"),
                Typography::Icons::AllApps,
                QColor(QStringLiteral("#F8F8FF"))),
          route(QStringLiteral("basic-input"),
                QStringLiteral("Basic input"),
                QStringLiteral("Controls"),
                Typography::Icons::CheckMark,
                QColor(QStringLiteral("#F2FBF8")),
                0,
                true),
          route(QStringLiteral("collections"),
                QStringLiteral("Collections"),
                QStringLiteral("Controls"),
                Typography::Icons::Grid,
                QColor(QStringLiteral("#FFF6FA")),
                0,
                true),
          route(QStringLiteral("date-time"),
                QStringLiteral("Date & time"),
                QStringLiteral("Controls"),
                Typography::Icons::Calendar,
                QColor(QStringLiteral("#F3FBFF")),
                0,
                true),
          route(QStringLiteral("dialogs-flyouts"),
                QStringLiteral("Dialogs & flyouts"),
                QStringLiteral("Controls"),
                Typography::Icons::Message,
                QColor(QStringLiteral("#FFF9EA")),
                0,
                true),
          route(QStringLiteral("layout"),
                QStringLiteral("Layout"),
                QStringLiteral("Controls"),
                Typography::Icons::List,
                QColor(QStringLiteral("#F7F7F7")),
                0,
                true),
          route(QStringLiteral("media"),
                QStringLiteral("Media"),
                QStringLiteral("Controls"),
                Typography::Icons::Play,
                QColor(QStringLiteral("#F4F7FF")),
                0,
                true),
          route(QStringLiteral("menus-toolbars"),
                QStringLiteral("Menus & toolbars"),
                QStringLiteral("Controls"),
                Typography::Icons::Save,
                QColor(QStringLiteral("#F8F8F8")),
                0,
                true),
          route(QStringLiteral("motion"),
                QStringLiteral("Motion"),
                QStringLiteral("Controls"),
                Typography::Icons::Refresh,
                QColor(QStringLiteral("#FFF9F1")),
                0,
                true),
          route(QStringLiteral("navigation"),
                QStringLiteral("Navigation"),
                QStringLiteral("Controls"),
                Typography::Icons::GlobalNav,
                QColor(QStringLiteral("#F0FBFA")),
                0,
                true),
          route(QStringLiteral("scrolling"),
                QStringLiteral("Scrolling"),
                QStringLiteral("Controls"),
                Typography::Icons::Down,
                QColor(QStringLiteral("#F8F5FF")),
                0,
                true),
          route(QStringLiteral("settings"),
                QStringLiteral("Settings"),
                QStringLiteral("Footer"),
                Typography::Icons::Settings,
                QColor(QStringLiteral("#F7F7F7")))
      })
{
}

QVector<GalleryNavigationItem> GalleryNavigationViewModel::mainPaneItems() const
{
    QVector<GalleryNavigationItem> paneItems;
    for (const GalleryNavigationItem& item : m_items) {
        if (item.id != QStringLiteral("settings"))
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
        if (!item.sectionHeader)
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
        if (!item.sectionHeader && item.id == id)
            return &item;
    }
    return nullptr;
}

} // namespace fluent::gallery
