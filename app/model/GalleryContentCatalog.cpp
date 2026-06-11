#include "GalleryContentCatalog.h"

#include <QHash>

#include "GalleryComponentCatalog.h"

namespace fluent::gallery {
namespace {

/**
 * @brief Per-route page descriptions, adapted from the WinUI Gallery wording.
 * zh_CN: 每个路由的页面描述，措辞参考 WinUI Gallery。
 */
const QHash<QString, QString>& routeDescriptions()
{
    static const QHash<QString, QString> descriptions{
        // Categories
        {QStringLiteral("all-controls"),
         QStringLiteral("Browse the full set of Fluent Qt controls in one place.")},
        {QStringLiteral("basic-input"),
         QStringLiteral("Controls that let people trigger actions and make choices.")},
        {QStringLiteral("collections"),
         QStringLiteral("Controls that present and organize sets of related items.")},
        {QStringLiteral("date-time"),
         QStringLiteral("Controls for picking and displaying dates, times, and calendars.")},
        {QStringLiteral("dialogs-flyouts"),
         QStringLiteral("Transient surfaces that show messages or request input without leaving the page.")},
        {QStringLiteral("menus-toolbars"),
         QStringLiteral("Surfaces that organize commands and actions for quick invocation.")},
        {QStringLiteral("navigation"),
         QStringLiteral("Controls that help people move between and within app areas.")},
        {QStringLiteral("scrolling"),
         QStringLiteral("Controls that scroll, pan, and page through content that exceeds the viewport.")},
        {QStringLiteral("status-info"),
         QStringLiteral("Controls that communicate state, progress, and contextual information.")},
        {QStringLiteral("text-fields"),
         QStringLiteral("Controls for entering and editing text, numbers, and passwords.")},
        {QStringLiteral("windowing"),
         QStringLiteral("Window chrome building blocks: title bars and top-level windows.")},
        // Basic input
        {QStringLiteral("button"),
         QStringLiteral("A control that responds to user input and raises a click event.")},
        {QStringLiteral("checkbox"),
         QStringLiteral("Lets people select or clear an option, with an optional indeterminate state.")},
        {QStringLiteral("combobox"),
         QStringLiteral("A drop-down list for selecting one item from a set of options.")},
        {QStringLiteral("dropdown-button"),
         QStringLiteral("A button that reveals a menu of related commands when invoked.")},
        {QStringLiteral("hyperlink-button"),
         QStringLiteral("A button styled like a hyperlink that opens a URL when clicked.")},
        {QStringLiteral("radio-button"),
         QStringLiteral("Lets people select one option from a mutually exclusive group.")},
        {QStringLiteral("rating-control"),
         QStringLiteral("Lets people view, set, or clear star ratings.")},
        {QStringLiteral("repeat-button"),
         QStringLiteral("A button that raises its click event repeatedly while it stays pressed.")},
        {QStringLiteral("slider"),
         QStringLiteral("Lets people select a value from a range by dragging a handle along a track.")},
        {QStringLiteral("split-button"),
         QStringLiteral("A two-part button: a primary action plus a chevron that opens a menu.")},
        {QStringLiteral("toggle-button"),
         QStringLiteral("A button that switches between checked and unchecked states.")},
        {QStringLiteral("toggle-split-button"),
         QStringLiteral("A split button whose primary half toggles on and off.")},
        {QStringLiteral("toggle-switch"),
         QStringLiteral("A switch that flips between on and off states.")},
        // Collections
        {QStringLiteral("drawer-view"),
         QStringLiteral("A panel that slides in from a window edge over the content.")},
        {QStringLiteral("flip-view"),
         QStringLiteral("Lets people flip through a collection of pages one at a time.")},
        {QStringLiteral("flow-view"),
         QStringLiteral("Lays out variable-sized items in wrapping rows, like a tag cloud.")},
        {QStringLiteral("grid-view"),
         QStringLiteral("Presents items in a grid of uniform cells with selection.")},
        {QStringLiteral("list-view"),
         QStringLiteral("Presents a vertical list of interactive items with selection.")},
        {QStringLiteral("split-view"),
         QStringLiteral("Hosts resizable panes separated by draggable handles.")},
        {QStringLiteral("stack-view"),
         QStringLiteral("A stack-based container that pushes and pops pages with transitions.")},
        {QStringLiteral("tree-view"),
         QStringLiteral("A control that presents hierarchical data with expandable nodes.")},
        // Date & time
        {QStringLiteral("calendar-date-picker"),
         QStringLiteral("A drop-down calendar for picking a single date.")},
        {QStringLiteral("calendar-view"),
         QStringLiteral("A calendar surface for browsing months and selecting dates.")},
        {QStringLiteral("date-picker"),
         QStringLiteral("A spinning-field picker for choosing a day, month, and year.")},
        {QStringLiteral("time-picker"),
         QStringLiteral("A spinning-field picker for choosing a time of day.")},
        // Dialogs & flyouts
        {QStringLiteral("content-dialog"),
         QStringLiteral("A modal dialog with title, content, and up to three commit buttons.")},
        {QStringLiteral("dialog"),
         QStringLiteral("A modal window surface that hosts arbitrary content above the app.")},
        {QStringLiteral("flyout"),
         QStringLiteral("A light-dismiss container anchored to the control that opened it.")},
        {QStringLiteral("popup"),
         QStringLiteral("A floating surface positioned over app content.")},
        {QStringLiteral("teaching-tip"),
         QStringLiteral("An anchored tip that draws attention to a feature without blocking interaction.")},
        // Menus & toolbars
        {QStringLiteral("menu"),
         QStringLiteral("A list of commands shown in a transient surface.")},
        {QStringLiteral("menu-bar"),
         QStringLiteral("A horizontal bar of top-level menus, each hosting commands.")},
        // Navigation
        {QStringLiteral("breadcrumb"),
         QStringLiteral("Shows the trail to the current location and lets people go back up the hierarchy.")},
        {QStringLiteral("navigation-view"),
         QStringLiteral("An adaptive left pane plus content area for top-level navigation.")},
        {QStringLiteral("pivot"),
         QStringLiteral("Header-based navigation between peer views with an animated indicator.")},
        {QStringLiteral("selector-bar"),
         QStringLiteral("A row of selectable items for switching between views.")},
        {QStringLiteral("tab-view"),
         QStringLiteral("A control that hosts multiple pages of content behind a tab strip.")},
        // Scrolling
        {QStringLiteral("annotated-scrollbar"),
         QStringLiteral("A scrollbar with labels that make long content easier to navigate.")},
        {QStringLiteral("pips-pager"),
         QStringLiteral("Dot-style pagination for moving through a small set of pages.")},
        {QStringLiteral("scrollbar"),
         QStringLiteral("A Fluent-styled scrollbar with hover expansion.")},
        {QStringLiteral("scroll-view"),
         QStringLiteral("A scrollable viewport with Fluent scrollbars and optional zoom.")},
        // Status & info
        {QStringLiteral("info-badge"),
         QStringLiteral("A small badge that shows counts, icons, or attention dots.")},
        {QStringLiteral("info-bar"),
         QStringLiteral("An inline bar for status messages with severity and optional actions.")},
        {QStringLiteral("progress-bar"),
         QStringLiteral("Shows determinate or indeterminate progress along a line.")},
        {QStringLiteral("progress-ring"),
         QStringLiteral("Shows determinate or indeterminate progress around a ring.")},
        {QStringLiteral("tooltip"),
         QStringLiteral("A popup label that shows extra information about a control on hover.")},
        // Text fields
        {QStringLiteral("auto-suggest-box"),
         QStringLiteral("A text box that suggests matching items while people type.")},
        {QStringLiteral("label"),
         QStringLiteral("Static text rendered with Fluent typography roles and elision.")},
        {QStringLiteral("line-edit"),
         QStringLiteral("A single-line text box with Fluent styling and a clear button.")},
        {QStringLiteral("number-box"),
         QStringLiteral("A numeric input with spin buttons and inline expression support.")},
        {QStringLiteral("password-box"),
         QStringLiteral("A text box that conceals input with an optional reveal button.")},
        {QStringLiteral("text-edit"),
         QStringLiteral("A multi-line text surface for longer-form input.")},
        // Windowing
        {QStringLiteral("title-bar"),
         QStringLiteral("A custom window title bar with drag regions and themed chrome.")},
        {QStringLiteral("window"),
         QStringLiteral("A Fluent top-level window with custom chrome and theming.")}
    };
    return descriptions;
}

QString descriptionFor(const QString& routeId, const QString& title)
{
    const auto it = routeDescriptions().constFind(routeId);
    if (it != routeDescriptions().constEnd())
        return it.value();
    return QStringLiteral("A %1 control from the Fluent Qt component library.").arg(title);
}

/**
 * @brief Builds the full content catalog from the component catalog.
 * zh_CN: 由组件目录生成完整内容目录。
 *
 * Every navigation route except the footer-owned Settings gets a content entry,
 * so the right pane never falls back to a placeholder for catalog routes.
 * zh_CN: 除页脚 Settings 外的所有导航路由都有内容条目，目录路由不再回退到占位页。
 */
QVector<GalleryContentEntry> buildCatalog()
{
    QVector<GalleryContentEntry> catalog;

    // Home features a curated set of controls on the landing page.
    // zh_CN: Home 在落地页精选展示一组控件。
    catalog.append({GalleryPageKind::Home,
                    QStringLiteral("home"),
                    QStringLiteral("Home"),
                    QStringLiteral("Explore the Fluent Qt control library with live samples and copyable code."),
                    QString(),
                    {QStringLiteral("button"),
                     QStringLiteral("toggle-switch"),
                     QStringLiteral("combobox"),
                     QStringLiteral("list-view"),
                     QStringLiteral("calendar-view"),
                     QStringLiteral("info-bar"),
                     QStringLiteral("tree-view"),
                     QStringLiteral("slider"),
                     QStringLiteral("tab-view")},
                    {}});

    // "All" lists every component; an empty categoryId means no category filter.
    // zh_CN: “All”列出全部组件；categoryId 为空表示不过滤分类。
    catalog.append({GalleryPageKind::Category,
                    QStringLiteral("all-controls"),
                    QStringLiteral("All controls"),
                    descriptionFor(QStringLiteral("all-controls"), QStringLiteral("All controls")),
                    QString(),
                    {},
                    {}});

    for (const GalleryComponentCategory& category : galleryComponentCatalog()) {
        catalog.append({GalleryPageKind::Category,
                        category.id,
                        category.title,
                        descriptionFor(category.id, category.title),
                        category.id,
                        {},
                        {}});
        for (const GalleryComponentEntry& component : category.components) {
            catalog.append({GalleryPageKind::Component,
                            component.id,
                            component.title,
                            descriptionFor(component.id, component.title),
                            category.id,
                            {category.id},
                            {}});
        }
    }

    return catalog;
}

} // namespace

const QVector<GalleryContentEntry>& galleryContentCatalog()
{
    static const QVector<GalleryContentEntry> catalog = buildCatalog();
    return catalog;
}

const GalleryContentEntry* galleryContentEntry(const QString& routeId)
{
    static const QHash<QString, const GalleryContentEntry*> index = []() {
        QHash<QString, const GalleryContentEntry*> map;
        for (const GalleryContentEntry& entry : galleryContentCatalog())
            map.insert(entry.routeId, &entry);
        return map;
    }();
    return index.value(routeId, nullptr);
}

} // namespace fluent::gallery
