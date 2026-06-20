#include "GalleryComponentCatalog.h"

#include <QFile>
#include <QHash>

#include "design/Typography.h"

namespace fluent::gallery {

const QVector<GalleryComponentCategory>& galleryComponentCatalog()
{
    static const QVector<GalleryComponentCategory> catalog{
        {QStringLiteral("basic-input"),
         QStringLiteral("Basic input"),
         QStringLiteral("basicinput"),
         Typography::Icons::CheckMark,
         {
             {QStringLiteral("button"), QStringLiteral("Button"), Typography::Icons::Add},
             {QStringLiteral("checkbox"), QStringLiteral("CheckBox"), Typography::Icons::CheckMark},
             {QStringLiteral("color-picker"), QStringLiteral("ColorPicker"), Typography::Icons::Color},
             {QStringLiteral("combobox"), QStringLiteral("ComboBox"), Typography::Icons::ChevronDown},
             {QStringLiteral("dropdown-button"), QStringLiteral("DropDownButton"), Typography::Icons::ChevronDownMed},
             {QStringLiteral("hyperlink-button"), QStringLiteral("HyperlinkButton"), Typography::Icons::Link},
             {QStringLiteral("radio-button"), QStringLiteral("RadioButton"), Typography::Icons::CheckmarkBadge12},
             {QStringLiteral("rating-control"), QStringLiteral("RatingControl"), Typography::Icons::FavoriteStar},
             {QStringLiteral("repeat-button"), QStringLiteral("RepeatButton"), Typography::Icons::Refresh},
             {QStringLiteral("slider"), QStringLiteral("Slider"), Typography::Icons::SelectAll},
             {QStringLiteral("split-button"), QStringLiteral("SplitButton"), Typography::Icons::ChevronDown},
             {QStringLiteral("toggle-button"), QStringLiteral("ToggleButton"), Typography::Icons::Power},
             {QStringLiteral("toggle-split-button"), QStringLiteral("ToggleSplitButton"), Typography::Icons::ChevronDown},
             {QStringLiteral("toggle-switch"), QStringLiteral("ToggleSwitch"), Typography::Icons::Power}
         }},
        {QStringLiteral("collections"),
         QStringLiteral("Collections"),
         QStringLiteral("collections"),
         Typography::Icons::Grid,
         {
             {QStringLiteral("drawer-view"), QStringLiteral("DrawerView"), Typography::Icons::List},
             {QStringLiteral("flip-view"), QStringLiteral("FlipView"), Typography::Icons::Forward},
             {QStringLiteral("flow-view"), QStringLiteral("FlowView"), Typography::Icons::Grid},
             {QStringLiteral("grid-view"), QStringLiteral("GridView"), Typography::Icons::Grid},
             {QStringLiteral("list-view"), QStringLiteral("ListView"), Typography::Icons::List},
             {QStringLiteral("split-view"), QStringLiteral("SplitView"), Typography::Icons::BackToWindow},
             {QStringLiteral("stack-view"), QStringLiteral("StackView"), Typography::Icons::AllApps},
             {QStringLiteral("tree-view"), QStringLiteral("TreeView"), Typography::Icons::Folder}
         }},
        {QStringLiteral("date-time"),
         QStringLiteral("Date & time"),
         QStringLiteral("date_time"),
         Typography::Icons::Calendar,
         {
             {QStringLiteral("calendar-date-picker"), QStringLiteral("CalendarDatePicker"), Typography::Icons::Calendar},
             {QStringLiteral("calendar-view"), QStringLiteral("CalendarView"), Typography::Icons::Calendar},
             {QStringLiteral("date-picker"), QStringLiteral("DatePicker"), Typography::Icons::Calendar},
             {QStringLiteral("time-picker"), QStringLiteral("TimePicker"), Typography::Icons::Clock}
         }},
        {QStringLiteral("dialogs-flyouts"),
         QStringLiteral("Dialogs & flyouts"),
         QStringLiteral("dialogs_flyouts"),
         Typography::Icons::Message,
         {
             {QStringLiteral("content-dialog"), QStringLiteral("ContentDialog"), Typography::Icons::Message},
             {QStringLiteral("dialog"), QStringLiteral("Dialog"), Typography::Icons::Message},
             {QStringLiteral("flyout"), QStringLiteral("Flyout"), Typography::Icons::Message},
             {QStringLiteral("popup"), QStringLiteral("Popup"), Typography::Icons::BackToWindow},
             {QStringLiteral("teaching-tip"), QStringLiteral("TeachingTip"), Typography::Icons::Info}
         }},
        {QStringLiteral("menus-toolbars"),
         QStringLiteral("Menus & toolbars"),
         QStringLiteral("menus_toolbars"),
         Typography::Icons::Save,
         {
             {QStringLiteral("menu"), QStringLiteral("Menu"), Typography::Icons::List},
             {QStringLiteral("menu-bar"), QStringLiteral("MenuBar"), Typography::Icons::Save}
         }},
        {QStringLiteral("navigation"),
         QStringLiteral("Navigation"),
         QStringLiteral("navigation"),
         Typography::Icons::GlobalNav,
         {
             {QStringLiteral("breadcrumb"), QStringLiteral("Breadcrumb"), Typography::Icons::ChevronRight},
             {QStringLiteral("navigation-view"), QStringLiteral("NavigationView"), Typography::Icons::GlobalNav},
             {QStringLiteral("pivot"), QStringLiteral("Pivot"), Typography::Icons::AllApps},
             {QStringLiteral("selector-bar"), QStringLiteral("SelectorBar"), Typography::Icons::SelectAll},
             {QStringLiteral("tab-view"), QStringLiteral("TabView"), Typography::Icons::AllApps}
         }},
        {QStringLiteral("scrolling"),
         QStringLiteral("Scrolling"),
         QStringLiteral("scrolling"),
         Typography::Icons::Down,
         {
             {QStringLiteral("annotated-scrollbar"), QStringLiteral("AnnotatedScrollBar"), Typography::Icons::List},
             {QStringLiteral("pips-pager"), QStringLiteral("PipsPager"), Typography::Icons::More},
             {QStringLiteral("scrollbar"), QStringLiteral("ScrollBar"), Typography::Icons::Down},
             {QStringLiteral("scroll-view"), QStringLiteral("ScrollView"), Typography::Icons::Down}
         }},
        {QStringLiteral("status-info"),
         QStringLiteral("Status & info"),
         QStringLiteral("status_info"),
         Typography::Icons::Info,
         {
             {QStringLiteral("info-badge"), QStringLiteral("InfoBadge"), Typography::Icons::Info},
             {QStringLiteral("info-bar"), QStringLiteral("InfoBar"), Typography::Icons::Info},
             {QStringLiteral("progress-bar"), QStringLiteral("ProgressBar"), Typography::Icons::Refresh},
             {QStringLiteral("progress-ring"), QStringLiteral("ProgressRing"), Typography::Icons::Refresh},
             {QStringLiteral("shimmer"), QStringLiteral("Shimmer"), Typography::Icons::Refresh},
             {QStringLiteral("tooltip"), QStringLiteral("ToolTip"), Typography::Icons::Info}
         }},
        {QStringLiteral("text-fields"),
         QStringLiteral("Text fields"),
         QStringLiteral("textfields"),
         Typography::Icons::Edit,
         {
             {QStringLiteral("auto-suggest-box"), QStringLiteral("AutoSuggestBox"), Typography::Icons::Search},
             {QStringLiteral("label"), QStringLiteral("Label"), Typography::Icons::Font},
             {QStringLiteral("line-edit"), QStringLiteral("LineEdit"), Typography::Icons::Edit},
             {QStringLiteral("number-box"), QStringLiteral("NumberBox"), Typography::Icons::Calculator},
             {QStringLiteral("password-box"), QStringLiteral("PasswordBox"), Typography::Icons::Lock},
             {QStringLiteral("text-edit"), QStringLiteral("TextEdit"), Typography::Icons::Edit}
         }},
        {QStringLiteral("windowing"),
         QStringLiteral("Windowing"),
         QStringLiteral("windowing"),
         Typography::Icons::BackToWindow,
         {
             {QStringLiteral("title-bar"), QStringLiteral("TitleBar"), Typography::Icons::BackToWindow},
             {QStringLiteral("window"), QStringLiteral("Window"), Typography::Icons::FullScreen}
         }}
    };
    return catalog;
}

QString galleryControlImageResource(const QString& controlTitle)
{
    static const QString placeholder =
        QStringLiteral(":/app/assets/control_images/Placeholder.png");

    // Foundation topics are content routes, not entries in galleryComponentCatalog(), so the
    // title->category lookup below can't resolve them — and their display titles ("QML+",
    // "Geometry & spacing") aren't valid filenames anyway. Map them explicitly to clean ASCII
    // image names. zh_CN: foundation 主题是内容路由，不在 galleryComponentCatalog() 中，下方的
    // 标题->分类查找解析不到它们；而且其显示标题（"QML+"、"Geometry & spacing"）也不是合法文件名。
    // 故在此用干净的 ASCII 图片名显式映射它们。
    static const QHash<QString, QString> foundationOverrides = {
        {QStringLiteral("QML+"),               QStringLiteral(":/app/assets/control_images/foundation/QMLPlus.png")},
        {QStringLiteral("Typography"),         QStringLiteral(":/app/assets/control_images/foundation/Typography.png")},
        {QStringLiteral("Color"),              QStringLiteral(":/app/assets/control_images/foundation/Color.png")},
        {QStringLiteral("Iconography"),        QStringLiteral(":/app/assets/control_images/foundation/Iconography.png")},
        {QStringLiteral("Geometry & spacing"), QStringLiteral(":/app/assets/control_images/foundation/Geometry.png")},
    };
    const auto foundationIt = foundationOverrides.constFind(controlTitle);
    if (foundationIt != foundationOverrides.constEnd())
        return QFile::exists(foundationIt.value()) ? foundationIt.value() : placeholder;

    // Map each control title to its category id once, straight from the catalog, so the
    // folder layout and the lookup never drift apart.
    // zh_CN: 从目录里一次性建立"控件标题 → 分类 id"映射，保证文件夹结构与查找逻辑不会脱节。
    static const QHash<QString, QString> titleToCategory = []() {
        QHash<QString, QString> map;
        for (const GalleryComponentCategory& category : galleryComponentCatalog())
            for (const GalleryComponentEntry& component : category.components)
                map.insert(component.title, category.id);
        return map;
    }();

    const auto it = titleToCategory.constFind(controlTitle);
    if (it == titleToCategory.constEnd())
        return placeholder;

    const QString candidate = QStringLiteral(":/app/assets/control_images/%1/%2.png")
                                  .arg(it.value(), controlTitle);
    return QFile::exists(candidate) ? candidate : placeholder;
}

} // namespace fluent::gallery
