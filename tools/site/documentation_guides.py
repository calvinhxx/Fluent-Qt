"""Localized introductions to FluentQt's shared foundations and optional modules."""
from functools import lru_cache
import json
from pathlib import Path
import re

from site_locales import LOCALE_KEYS, locale_url_prefix

ROOT = Path(__file__).resolve().parents[2]
GUIDES_LOCALES_DIR = Path(__file__).with_name("locales") / "guides"
_PLACEHOLDER_RE = re.compile(r"\{[a-zA-Z0-9_]+\}")

_BUILTIN_UI = {
    "en": {
        "home": "Home",
        "documentation": "Documentation",
        "siteNav": "Site navigation",
        "breadcrumb": "Breadcrumb",
        "contentsAndSearch": "Contents and search",
        "searchDocumentation": "Search documentation",
        "resultsTemplate": "{count} results",
        "noResults": "No matching documentation",
        "overview": "Overview",
        "exampleHeading": "Example",
        "usage": "Usage",
        "api": "API",
        "source": "Declaration",
        "light": "Light",
        "dark": "Dark",
        "theme": "Example theme",
        "loadingLive": "Loading example…",
        "failedLive": "The example could not load. A preview is shown instead.",
        "retryLive": "Retry",
        "noScript": "Enable JavaScript to use this example.",
        "editorError": "Property preview could not load. Select Property preview to retry.",
        "previous": "Previous",
        "next": "Next",
        "filterApi": "Filter API members",
        "noApiMatches": "No matching API members.",
        "loadingPreview": "Loading preview…",
        "previewUnavailable": "Preview unavailable. Refresh the page to retry.",
        "headerFallback": "Public types, functions, and constants declared in this header.",
        "releasesAndMigrations": "Releases and migrations",
        "releasesDescription": "Updates and migration requirements for UILib and Gallery, grouped by version.",
        "copy": "Copy code",
        "copied": "Code copied",
        "copyError": "Could not copy. Select the code to copy it manually.",
        "skip": "Skip to content",
        "onPage": "On this page",
        "apiReference": "API reference",
        "playgroundHeading": "Component property preview",
        "playgroundCopy": "Adjust component properties and copy the corresponding C++ or PySide6 code.",
        "playgroundPrimary": "Preview CheckBox",
        "moduleHeading": "Components in this module",
        "moduleCopy": "Open the documentation or try a component and copy its code.",
        "propertyPreview": "Property preview",
        "unreleasedNotice": "This page includes unreleased changes not available in stable {latest}.",
        "viewReleaseNotes": "View related release notes",
        "apiGroups": {
            "constructors": "Constructors",
            "properties": "Properties",
            "methods": "Methods",
            "signals": "Signals",
            "enums": "Enumerations",
            "types": "Types and aliases",
            "fields": "Fields",
            "constants": "Constants",
            "destructors": "Destructors",
            "python": "PySide6 additions",
        },
        "inherits": "Inherits",
        "deprecated": "Deprecated",
        "accessors": {
            "read": "Read",
            "write": "Write",
            "reset": "Reset",
            "notify": "Notify",
        },
        "protectedMembers": "Protected members",
        "inheritedFrom": "Inherited from {name}",
        "qtBaseDocs": "Qt base class docs: ",
        "scopes": {
            "uilib": "UILib",
            "gallery": "Gallery",
            "project": "Project notes",
        },
        "introducedIn": "Introduced in",
        "before": "Before",
        "after": "After",
        "migrationRequired": "Migration required",
        "latest": "Latest",
        "releaseCandidate": "Release candidate",
        "dateNotRecorded": "Date not recorded",
        "unreleased": "Unreleased",
        "unreleasedMerged": "Merged into source since {latest}:",
        "unreleasedNone": "No unreleased public changes since {latest}.",
        "downloads": "Downloads",
        "releaseNotesEn": "Release notes",
        "compareWith": "Compare with {previous}",
        "commitsSince": "Commits since {latest}",
        "linksAria": "{heading} links",
        "filterReleases": "Filter releases",
        "showUpdatesFor": "Show updates for",
        "scopeAll": "All",
        "scopeUilib": "UILib",
        "scopeGallery": "Gallery",
        "searchLabel": "Search",
        "searchPlaceholder": "Component or keyword, e.g. ParticleBackdrop",
        "upgradingFrom": "Upgrading from",
        "anyVersion": "Any version",
        "onlyMigration": "Only releases that need migration",
        "clearFilters": "Clear filters",
        "releaseEmpty": "Nothing matches. Try another keyword or clear the filters.",
        "releaseCountTemplate": "{count} entries",
        "releaseCountSingleTemplate": "{count} entry",
        "releaseCountAfterTemplate": "{count} entries after {version}",
        "releaseCountSingleAfterTemplate": "{count} entry after {version}",
        "sections": {
            "project": "Project updates",
            "foundation": "Foundations",
            "components": "Components",
            "modules": "Modules",
            "reference": "API reference",
        },
        "categories": {
            "basic-input": "Basic input",
            "charts": "Charts",
            "collections": "Collections",
            "date-time": "Date & time",
            "dialogs-flyouts": "Dialogs & flyouts",
            "layout": "Layout",
            "menus-toolbars": "Menus & toolbars",
            "navigation": "Navigation",
            "scrolling": "Scrolling",
            "status-info": "Status & info",
            "text-fields": "Text fields",
            "windowing": "Windowing",
        },
    },
    "zh": {
        "home": "官网",
        "documentation": "文档",
        "siteNav": "网站导航",
        "breadcrumb": "当前位置",
        "contentsAndSearch": "目录与搜索",
        "searchDocumentation": "搜索文档",
        "resultsTemplate": "{count} 个结果",
        "noResults": "没有匹配的文档",
        "overview": "概览",
        "exampleHeading": "示例",
        "usage": "用法",
        "api": "API",
        "source": "查看声明",
        "light": "浅色",
        "dark": "深色",
        "theme": "示例主题",
        "loadingLive": "正在加载示例…",
        "failedLive": "示例加载失败，暂时显示预览图。",
        "retryLive": "重试",
        "noScript": "启用 JavaScript 后可操作示例。",
        "editorError": "属性预览加载失败。请再次点击属性预览重试。",
        "previous": "上一项",
        "next": "下一项",
        "filterApi": "筛选 API 成员",
        "noApiMatches": "没有匹配的 API 成员。",
        "loadingPreview": "正在加载预览…",
        "previewUnavailable": "预览加载失败，请刷新页面重试。",
        "headerFallback": "本头文件包含的类型、函数与常量。",
        "releasesAndMigrations": "版本记录与迁移",
        "releasesDescription": "按组件库（UILib）与 Gallery 分类查看各版本更新内容与升级说明。",
        "copy": "复制代码",
        "copied": "代码已复制",
        "copyError": "复制失败，请选择代码手动复制。",
        "skip": "跳转到正文",
        "onPage": "本页内容",
        "apiReference": "API 参考",
        "playgroundHeading": "组件属性预览",
        "playgroundCopy": "调整组件属性，然后复制对应的 C++ 或 PySide6 代码。",
        "playgroundPrimary": "预览 CheckBox",
        "moduleHeading": "本模块的组件",
        "moduleCopy": "打开组件文档，或直接调整属性并复制代码。",
        "propertyPreview": "属性动态预览",
        "unreleasedNotice": "本页包含尚未随稳定版 {latest} 发布的最新改动。",
        "viewReleaseNotes": "查看相关版本记录",
        "apiGroups": {
            "constructors": "构造函数",
            "properties": "属性",
            "methods": "方法",
            "signals": "信号",
            "enums": "枚举",
            "types": "类型与别名",
            "fields": "字段",
            "constants": "常量",
            "destructors": "析构函数",
            "python": "PySide6 扩展",
        },
        "inherits": "继承自",
        "deprecated": "已弃用",
        "accessors": {
            "read": "读取",
            "write": "设置",
            "reset": "重置",
            "notify": "变化信号",
        },
        "protectedMembers": "受保护成员",
        "inheritedFrom": "继承自 {name}",
        "qtBaseDocs": "Qt 基类文档：",
        "scopes": {
            "uilib": "组件库 UILib",
            "gallery": "Gallery 示例应用",
            "project": "项目说明",
        },
        "introducedIn": "引入提交",
        "before": "旧写法",
        "after": "新写法",
        "migrationRequired": "需要迁移",
        "latest": "最新版本",
        "releaseCandidate": "候选版本",
        "dateNotRecorded": "未记录发布日期",
        "unreleased": "开发中",
        "unreleasedMerged": "自 {latest} 发布以来已合入源码的改动：",
        "unreleasedNone": "{latest} 发布后暂无未发布的公开改动。",
        "downloads": "下载",
        "releaseNotesEn": "发布说明（英文）",
        "compareWith": "与 {previous} 对比",
        "commitsSince": "{latest} 之后的提交",
        "linksAria": "{heading} 相关链接",
        "filterReleases": "筛选版本记录",
        "showUpdatesFor": "查看范围",
        "scopeAll": "全部",
        "scopeUilib": "组件库",
        "scopeGallery": "Gallery",
        "searchLabel": "搜索",
        "searchPlaceholder": "组件名或关键词，如 ParticleBackdrop",
        "upgradingFrom": "从哪个版本升级",
        "anyVersion": "不限",
        "onlyMigration": "只看需要迁移的版本",
        "clearFilters": "清除筛选",
        "releaseEmpty": "没有符合条件的记录。换个关键词，或清除筛选。",
        "releaseCountTemplate": "共 {count} 条记录",
        "releaseCountSingleTemplate": "共 {count} 条记录",
        "releaseCountAfterTemplate": "{version} 之后共 {count} 条记录",
        "releaseCountSingleAfterTemplate": "{version} 之后共 {count} 条记录",
        "sections": {
            "project": "项目更新",
            "foundation": "基础指南",
            "components": "组件",
            "modules": "扩展模块",
            "reference": "API 参考",
        },
        "categories": {
            "basic-input": "基础输入",
            "charts": "图表",
            "collections": "集合视图",
            "date-time": "日期与时间",
            "dialogs-flyouts": "对话框与浮层",
            "layout": "布局",
            "menus-toolbars": "菜单与工具栏",
            "navigation": "导航",
            "scrolling": "滚动",
            "status-info": "状态与信息",
            "text-fields": "文本输入",
            "windowing": "窗口",
        },
    },
}


def _validate_ui_dict(actual, baseline, location):
    if not isinstance(actual, dict) or set(actual) != set(baseline):
        missing = sorted(set(baseline) - set(actual)) if isinstance(actual, dict) else ["<not dict>"]
        extra = sorted(set(actual) - set(baseline)) if isinstance(actual, dict) else []
        raise ValueError(f"UI schema mismatch at {location}: missing={missing}, extra={extra}")
    for key, base_val in baseline.items():
        act_val = actual[key]
        if isinstance(base_val, dict):
            _validate_ui_dict(act_val, base_val, f"{location}.{key}")
        else:
            if not isinstance(act_val, str) or not act_val.strip():
                raise ValueError(f"Empty UI string at {location}.{key}")
            if sorted(_PLACEHOLDER_RE.findall(act_val)) != sorted(_PLACEHOLDER_RE.findall(base_val)):
                raise ValueError(f"Placeholder mismatch at {location}.{key}: {act_val!r} vs {base_val!r}")


@lru_cache(maxsize=None)
def guide_locale_file(language):
    path = GUIDES_LOCALES_DIR / f"{language}.json"
    if not path.is_file():
        raise ValueError(f"Missing guide locale file: {path}")
    return json.loads(path.read_text(encoding="utf-8"))


@lru_cache(maxsize=None)
def guide_ui(language):
    if language in _BUILTIN_UI:
        return _BUILTIN_UI[language]
    ui = guide_locale_file(language)["ui"]
    _validate_ui_dict(ui, _BUILTIN_UI["en"], f"guides/{language}.json:ui")
    return ui


def section(en, zh, english, chinese, cpp='', python=''):
    return {'title': {'en': en, 'zh': zh}, 'body': {'en': english, 'zh': chinese}, 'cpp': cpp, 'python': python}


def guide(id, en, zh, english, chinese, headers, sections, group='foundation', *, component_category=None):
    return {'id': id, 'name': {'en': en, 'zh': zh}, 'description': {'en': english, 'zh': chinese},
            'headers': headers, 'sections': sections, 'group': group, 'component_category': component_category}


GUIDES = [
    guide('foundation', 'Getting started', '快速开始',
        'Initialize FluentQt in C++ or PySide6 and pick only the modules your app uses.',
        '在 C++ 或 PySide6 中初始化 FluentQt，按需引入所需模块。',
        ['include/FluentQt/FluentQt.h', 'src/components/foundation/FluentElement.h'], [
            section('Built on Qt Widgets', '直接基于 Qt Widgets',
                'FluentQt controls are standard Qt Widgets: Button subclasses QPushButton and LineEdit subclasses QLineEdit (a few helpers such as SpatialItem subclass QObject or QGraphicsObject). Most widgets also inherit FluentElement for theming and QMLPlus for bindings, anchors, and states.',
                'FluentQt 控件本身就是 Qt Widgets：Button 继承 QPushButton，LineEdit 继承 QLineEdit（少数辅助类型如 SpatialItem 基于 QObject 或 QGraphicsObject）。大多数控件还通过 FluentElement 支持主题，并通过 QMLPlus 提供属性绑定、锚点和状态功能。'),
            section('Initialize the app', '初始化应用',
                'Call prepareHighDpiApplication() before creating QApplication, then call initializeResources() right after to load bundled fonts and icons.',
                '创建 QApplication 前先调用 prepareHighDpiApplication() 开启高分屏适配，创建后再调用 initializeResources() 加载内置字体和图标。',
                '#include <FluentQt/FluentQt.h>\n#include <QApplication>\n\nint main(int argc, char* argv[]) {\n    fluent::prepareHighDpiApplication();\n    QApplication app(argc, argv);\n    fluent::initializeResources();\n\n    fluent::windowing::Window window;\n    window.show();\n    return app.exec();\n}',
                'import sys\nimport fluentqt\nfrom PySide6.QtWidgets import QApplication\n\nfluentqt.prepare_high_dpi_application()\napp = QApplication(sys.argv)\nfluentqt.initialize_resources()\n\nwindow = fluentqt.Window()\nwindow.show()\nsys.exit(app.exec())'),
            section('Optional modules', '按需选择模块',
                'FluentQt::FluentQt includes core widgets, charts, and design constants. Link optional targets only when needed: FluentQt::Plugins, FluentQt::Particles (animated backdrops), FluentQt::Spatial (3D perspective views), or FluentQt::QmlBridge (embedded QML).',
                '基础库 FluentQt::FluentQt 包含常用控件、图表和样式常量。需要高级功能时，再按需链接可选模块：FluentQt::Plugins（应用内插件）、FluentQt::Particles（粒子背景）、FluentQt::Spatial（3D 透视视图）和 FluentQt::QmlBridge（嵌入 QML 页面）。')]),
    guide('foundation-color', 'Themes and colors', '主题与颜色',
        'Switch Light, Dark, or High Contrast themes and read theme colors in custom paint code.',
        '切换浅色、深色或高对比度主题，并在自绘控件中读取当前配色。',
        ['src/components/foundation/FluentElement.h', 'src/components/foundation/ThemeRegistry.h',
         'src/components/foundation/UserTheme.h', 'src/design/ThemeColors.h'], [
            section('Switch the theme', '切换全局主题',
                'FluentElement::setTheme() switches between Light, Dark, and HighContrast and repaints all controls automatically. To customize colors or sizes on a single widget only, pass a JSON object to setThemeOverrides()—those overrides stay on that widget across theme switches.',
                '调用 FluentElement::setTheme() 可在 Light、Dark 和 HighContrast 之间切换，所有控件会自动刷新外观。如果只想修改某一个控件的配色或尺寸，用 setThemeOverrides() 传入 JSON 即可，设置仅对当前控件生效且切换主题后不会丢失。',
                '#include <FluentQt/Foundation.h>\n\nfluent::FluentElement::setTheme(fluent::FluentElement::Dark);',
                'import fluentqt\n\nfluentqt.set_theme(fluentqt.Theme.Dark)'),
            section('Read theme colors', '读取主题颜色',
                'Use named colors such as bgCanvas or textPrimary instead of hard-coded RGB values so custom painting follows theme switches. Call themeColorsRef() in C++, or subclass fluentqt.FluentWidget and call theme_tokens() in Python.',
                '自绘界面时不要写死 RGB 颜色，改用 bgCanvas（画布底色）、textPrimary（主文字色）等颜色名即可自动跟随深浅主题变化。C++ 用 themeColorsRef()，Python 继承 fluentqt.FluentWidget 后用 theme_tokens()。',
                '#include <FluentQt/Foundation.h>\n\n// Inside a FluentElement-based widget\'s paintEvent()\nconst auto& colors = themeColorsRef();\nconst QColor background = colors.bgCanvas;\nconst QColor text = colors.textPrimary;',
                'import fluentqt\n\nclass Swatch(fluentqt.FluentWidget):\n    def paintEvent(self, event):\n        colors = self.theme_tokens().colors\n        background = colors.bgCanvas\n        text = colors.textPrimary')]),
    guide('foundation-typography', 'Fonts and text styles', '字体与字号',
        'Apply preset Fluent font sizes and weights to labels and custom widgets.',
        '使用内置的 9 档预设字号和字重，统一界面文字大小。',
        ['src/design/Typography.h'], [
            section('Preset font sizes', '预设字体档位',
                'Typography provides nine presets from 12 px captions to 68 px display titles: Caption (12px), Body (14px), BodyStrong (14px semibold), BodyLarge (18px), BodyLargeStrong (18px semibold), Subtitle (20px), Title (28px), TitleLarge (40px), and Display (68px). Call setFluentTypography() on Label to apply one.',
                'Typography 内置了 9 档常用字号：Caption（12px）、Body（14px）、BodyStrong（14px 半粗）、BodyLarge（18px）、BodyLargeStrong（18px 半粗）、Subtitle（20px）、Title（28px）、TitleLarge（40px）和 Display（68px）。在 Label 上调用 setFluentTypography() 即可使用。',
                '#include <FluentQt/TextFields.h>\n\nauto* title = new fluent::textfields::Label("Account settings", this);\ntitle->setFluentTypography(Typography::FontRole::Subtitle);',
                'import fluentqt\n\ntitle = fluentqt.Label("Account settings")\ntitle.setFluentTypography(fluentqt.FontRole.Subtitle)'),
            section('Use on plain QWidgets', '给普通 QWidget 设置字体',
                'For plain Qt widgets, get a matching QFont with Typography::fontStyle(role).toQFont() in C++ or fluentqt.font_for_role(role) in Python. Calling QWidget::setFont directly on a Fluent control may stop it from updating fonts on theme switches.',
                '普通 Qt 控件可通过 Typography::fontStyle(role).toQFont()（C++）或 fluentqt.font_for_role(role)（Python）拿到对应的 QFont。注意：如果手动对 Fluent 控件调用 QWidget::setFont，后续切换主题时可能不再自动更新字体。',
                '#include <FluentQt/Design.h>\n\ncustomWidget->setFont(Typography::fontStyle(Typography::FontRole::BodyStrong).toQFont());',
                'import fluentqt\n\ncustom_widget.setFont(fluentqt.font_for_role(fluentqt.FontRole.BodyStrong))')]),
    guide('foundation-iconography', 'Icons', '图标',
        'Show bundled Fluent icons or icons from your own TTF/OTF font file.',
        '使用内置 Fluent 图标，或加载你自己的 TTF/OTF 图标字体。',
        ['src/design/IconCatalog.h', 'src/design/Typography.h', 'src/components/foundation/FontIcon.h'], [
            section('Use icon names', '使用内置图标名',
                'Use named constants from Typography::Icons (C++) or fluentqt.Icons (Python) instead of raw code points like "\\uE713". Pass them straight to FontIcon or Button.',
                '推荐直接使用 Typography::Icons（C++）或 fluentqt.Icons（Python）里的命名常量，不必手写 "\\uE713" 等字符编码。FontIcon 和 Button 都能直接传入。',
                '#include <FluentQt/BasicInput.h>\n#include <FluentQt/Foundation.h>\n\nauto* icon = new fluent::FontIcon(Typography::Icons::Settings, this);\nicon->setIconSize(Typography::IconSize::Standard);\nauto* btn = new fluent::basicinput::Button("Settings", this);\nbtn->setIconGlyph(Typography::Icons::Settings);',
                'import fluentqt\n\nicon = fluentqt.FontIcon(fluentqt.Icons.Settings)\nicon.setIconSize(fluentqt.IconSize.Standard)\nbtn = fluentqt.Button("Settings")\nbtn.setIconGlyph(fluentqt.Icons.Settings)'),
            section('Use a custom icon font', '使用自定义图标字体',
                'Load your TTF/OTF file with QFontDatabase, then pass its family name to FontIcon::setFontFamily() along with the icon character. Custom fonts render directly at iconSize while keeping tint color, opacity, and rotation. Pass an empty string to switch back to bundled icons.',
                '先用 QFontDatabase 加载 TTF/OTF 字体文件并获取字体名，再传给 FontIcon::setFontFamily() 即可显示该字体里的图标字符。自定义图标字体直接使用你设定的 iconSize（不会自动切字重），颜色、禁用透明度和旋转仍正常生效；传空字符串即可切回内置图标。',
                '#include <FluentQt/Foundation.h>\n#include <QFontDatabase>\n\nconst int id = QFontDatabase::addApplicationFont(\n    QStringLiteral(":/icons/application.ttf"));\nconst QStringList families = QFontDatabase::applicationFontFamilies(id);\nif (id >= 0 && !families.isEmpty()) {\n    // Replace this codepoint with one defined by your font.\n    auto* icon = new fluent::FontIcon(QStringLiteral("\\uE064"), this);\n    icon->setFontFamily(families.first());\n    icon->setIconSize(20);\n    icon->setAccessibleName(QStringLiteral("Application icon"));\n}',
                'import fluentqt\nfrom PySide6.QtGui import QFontDatabase\n\nfont_id = QFontDatabase.addApplicationFont(":/icons/application.ttf")\nfamilies = QFontDatabase.applicationFontFamilies(font_id)\nif font_id >= 0 and families:\n    # Replace this codepoint with one defined by your font.\n    icon = fluentqt.FontIcon(chr(0xE064))\n    icon.setFontFamily(families[0])\n    icon.setIconSize(20)\n    icon.setAccessibleName("Application icon")'),
            section('Sizes and accessibility', '常用尺寸与无障碍名称',
                'Use Typography::IconSize presets: Compact (12px) for small arrows, Standard (16px) for controls, and Large (20px) / XLarge (24px) for headers. On icon-only buttons, call setAccessibleName() so screen readers can announce what the button does.',
                '常用大小可通过 Typography::IconSize 选择：小箭头用 Compact（12px），常规按钮用 Standard（16px），标题用 Large（20px）或 XLarge（24px）。纯图标按钮记得设置 setAccessibleName()，方便读屏软件识别按钮作用。')], component_category='foundation'),
    guide('foundation-geometry', 'Corners, shadows, and backdrops', '圆角、阴影与背景',
        'Standard corner radii, shadow presets, and Mica/Acrylic window backgrounds.',
        '统一的圆角大小、阴影参数以及 Mica / Acrylic 窗口背景效果。',
        ['src/design/CornerRadius.h', 'src/design/Elevation.h', 'src/design/Material.h'], [
            section('Corner radii', '圆角',
                'Use CornerRadius::Control (4px) for buttons and inputs, and CornerRadius::Overlay (8px) for cards, popups, and dialogs.',
                '按钮、输入框使用 CornerRadius::Control（4px），卡片、弹窗和对话框使用 CornerRadius::Overlay（8px）。',
                '#include <FluentQt/Design.h>\n\nconst int radius = CornerRadius::Control; // 4px\nconst int cardRadius = CornerRadius::Overlay; // 8px',
                'from fluentqt.design import CornerRadius\n\nradius = CornerRadius.Control      # 4px\ncard_radius = CornerRadius.Overlay # 8px'),
            section('Shadows and window backgrounds', '阴影与窗口背景',
                'Elevation defines shadow presets for cards, popups, and dialogs. On a top-level Window, call setBackdropEffect() for Solid, Mica, or Acrylic backgrounds and check backdropState() to see what the OS applied.',
                'Elevation 提供了卡片、浮层和对话框的标准阴影大小。在顶层 Window 上调用 Window::setBackdropEffect() 可开启 Solid、Mica 或 Acrylic 半透明背景，用 backdropState() 可确认当前系统是否支持并生效。')]),
    guide('foundation-spacing', 'Spacing and responsive sizes', '间距与自适应',
        '4px-grid spacing constants and window-width thresholds for responsive layouts.',
        '基于 4px 栅格的常用间距，以及用于响应式布局的窗口宽度阈值。',
        ['src/design/Spacing.h', 'src/design/Breakpoints.h'], [
            section('Spacing', '间距',
                'Spacing constants follow a 4px grid: use Spacing::Gap::Tight (4px) or Normal (8px) inside a group, Gap::Loose (16px) between groups, Gap::Section (24px) between sections, and Spacing::Padding::Card (16px) inside cards. Standard controls are Spacing::ControlHeight::Standard (32px) tall.',
                '布局间距以 4px 为基本单位：同组内用 Spacing::Gap::Tight（4px）或 Normal（8px），不同组之间用 Gap::Loose（16px），大区块之间用 Gap::Section（24px），卡片内边距用 Spacing::Padding::Card（16px）。标准控件高度为 Spacing::ControlHeight::Standard（32px）。',
                '#include <FluentQt/Design.h>\n\nlayout->setSpacing(Spacing::Gap::Normal);\nconst int pad = Spacing::Padding::Card;\nlayout->setContentsMargins(pad, pad, pad, pad);',
                'from fluentqt.design import Spacing\n\nlayout.setSpacing(Spacing.Gap.Normal)\npad = Spacing.Padding.Card\nlayout.setContentsMargins(pad, pad, pad, pad)'),
            section('Window-width thresholds', '窗口宽度阈值',
                'Use Breakpoints::Small (640px), Medium (1007px), and Large (1920px) to adapt layouts when the window resizes—for instance collapsing a sidebar or stacking cards vertically.',
                'Breakpoints::Small（640px）、Medium（1007px）和 Large（1920px）用于判断窗口宽窄变化，例如窗口变窄时收起侧边栏，或把多栏卡片改成单列排列。')]),
    guide('foundation-qmlplus', 'Bindings, anchors, and states', '绑定、锚点与状态',
        'Property bindings, relative anchors, and state switches for QWidget without loading QML.',
        '直接在 QWidget 中使用属性绑定、相对锚点定位和状态切换，无需引入 QML 引擎。',
        ['src/components/foundation/QMLPlus.h'], [
            section('Bind properties', '属性绑定',
                'bind() keeps a target property synced whenever the source changes. If you need to pause, resume, or clear the binding later, use QMLPlus::bindManaged() (C++) or fluentqt.bind_managed() (Python) and keep the returned handle. Both widgets must live on the same thread.',
                '调用 bind() 可让目标属性自动跟随源属性变化。如果之后需要暂停或取消绑定，C++ 请用 QMLPlus::bindManaged()、Python 请用 fluentqt.bind_managed()，并保存返回的控制对象。两个控件须在同一线程内。',
                '#include <FluentQt/BasicInput.h>\n#include <FluentQt/StatusInfo.h>\n\nauto* slider = new fluent::basicinput::Slider(Qt::Horizontal, this);\nslider->setRange(0, 100);\nauto* bar = new fluent::status_info::ProgressBar(this);\nbar->setRange(0, 100);\nauto binding = bar->bindManaged("value", slider, "value");',
                'import fluentqt\nfrom PySide6.QtCore import Qt\n\nslider = fluentqt.Slider(Qt.Horizontal, panel)\nslider.setRange(0, 100)\nbar = fluentqt.ProgressBar(panel)\nbar.setRange(0, 100)\nbinding = fluentqt.bind_managed(slider, "value", bar, "value")'),
            section('Anchors and states', '锚点定位与状态',
                'In C++, anchors() aligns edges or centers against a parent or sibling, and addState(), setState(), and addTransition() switch property states with animations. In Python, pass fluentqt.anchors() to AnchorLayout and manage states with StateGroup.',
                'C++ 中通过 anchors() 可把控件对齐到父控件或相邻控件的边线/中心；addState()、setState() 和 addTransition() 用于切换界面状态并附带过渡动画。Python 中可用 fluentqt.anchors() 搭配 AnchorLayout 定位，用 StateGroup 切换状态。',
                '#include <FluentQt/Foundation.h>\n\n// badge is a child of card\nusing Edge = fluent::AnchorLayout::Edge;\nbadge->anchors()->top = {card, Edge::Top, 12};\nbadge->anchors()->right = {card, Edge::Right, -12};',
                'import fluentqt\n\nlayout = fluentqt.AnchorLayout(card)\nlayout.addWidget(badge, fluentqt.anchors(top_right=(card, 12)))')]),
    guide('foundation-motion', 'Animations', '动画',
        'Enable, speed up, or disable widget animations globally or per control.',
        '全局或按单个控件开关动画、缩短过渡时长或完全禁用动画。',
        ['src/components/foundation/MotionPolicy.h', 'src/design/Animation.h'], [
            section('Global animation mode', '全局动画开关',
                'MotionPolicy sets how FluentQt widgets animate: Full plays normal durations, Reduced caps transitions at 50 ms and stops loops, and Disabled snaps straight to the final state. Local switches on individual controls can still turn animations off.',
                'MotionPolicy 统一控制全局动画：Full 正常播放；Reduced 把所有过渡限制在 50 ms 以内并暂停循环动画；Disabled 直接跳到结束状态。单个控件的本地开关仍可独立禁用动画。',
                '#include <FluentQt/Foundation.h>\n\nfluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Reduced);',
                'import fluentqt\n\nfluentqt.set_motion_mode(fluentqt.MotionMode.Reduced)'),
            section('Per-control switches', '单个控件的开关',
                'Animated controls expose animationEnabled (such as Dialog, Popup, and ProgressRing), while StackView uses setTransitionAnimationEnabled(false). Controls that only repaint hover/press colors (like Button) have no animation toggle.',
                '带有动画的控件提供 animationEnabled（例如 Dialog、Popup、ProgressRing），页面容器 StackView 提供 setTransitionAnimationEnabled(false)。Button 等仅重绘样式的简单控件不单独设动画开关。'),
            section('Custom animations', '在自定义动画中遵守全局设置',
                'For custom animations, check MotionPolicy::instance().shouldAnimate() and resolvedDuration() (pass MotionPolicy::Kind::Continuous for loops). Connect modeChanged if running animations should respond to live setting changes.',
                '自定义动画建议通过 MotionPolicy::instance() 的 shouldAnimate() 和 resolvedDuration() 获取是否播放与实际时长（循环动画传入 MotionPolicy::Kind::Continuous）；连接 modeChanged 信号可在设置修改时立即停止正在播放的动画。')]),
    guide('foundation-models', 'Data models', '数据模型',
        'Bind data models to lists, tables, trees, and charts without giving up model ownership.',
        '将列表、表格、树和图表绑定到数据模型，数据的创建和释放仍由应用自己掌控。',
        ['src/components/charts/ChartModel.h', 'src/components/collections/SelectionMode.h'], [
            section('List, tree, and table models', '列表、树与表格模型',
                'ListView, TreeView, and DataGrid use standard Qt QAbstractItemModel and QItemSelectionModel. Your application creates and deletes the model and emits standard Qt change signals to refresh views.',
                'ListView、TreeView 和 DataGrid 直接使用 Qt 的 QAbstractItemModel 和 QItemSelectionModel。模型由应用自行管理，更新数据时按 Qt 常规方式发出变更信号即可刷新界面。'),
            section('Chart models', '图表模型',
                'Charts attach your ChartModel without taking ownership, so several charts can share one model. Call ChartModel::setPoints() on the model\'s thread; background threads can build a ChartData snapshot and pass it back to ChartModel::setDataSnapshot() on the model\'s thread.',
                '图表只读取你的 ChartModel 数据而不会替你销毁它，因此多个图表可以共用一份模型。请在模型所在线程调用 ChartModel::setPoints()；如果在后台线程计算数据，可先整理为 ChartData 快照，再回到主线程调用 ChartModel::setDataSnapshot()。',
                '#include <FluentQt/Charts.h>\n\nauto* model = new fluent::charts::ChartModel(this);\nmodel->setName("CPU");\nmodel->setPoints({QPointF(0, 32), QPointF(1, 48), QPointF(2, 41)});\nlineChart->setModel(model);',
                'import fluentqt\nfrom PySide6.QtCore import QPointF\n\nmodel = fluentqt.ChartModel(window)\nmodel.setPoints([QPointF(0, 32), QPointF(1, 48), QPointF(2, 41)])\nline_chart.setModel(model)')]),
    guide('foundation-forms', 'Forms and validation', '表单与校验',
        'Track modified fields, error messages, and asynchronous validation in forms.',
        '集中管理表单字段的修改记录、错误提示以及异步校验流程。',
        ['src/components/foundation/FormController.h'], [
            section('Register fields', '添加表单字段',
                'Wrap each input widget in a layout::Field and register it via FormController::addField() (non-owning). The controller tracks modified, touched, validating, and valid states and updates Field error text automatically. Call markFieldChanged() when the input changes.',
                '用 layout::Field 包裹每个输入框后，调用 FormController::addField() 注册（不会接管控件生命周期）。控制器会自动记录是否修改、是否点过、校验状态和报错提示，并同步更新 Field 界面。输入值变动时调用 markFieldChanged() 通知控制器即可。',
                '#include <FluentQt/Foundation.h>\n#include <FluentQt/Layout.h>\n\nauto* form = new fluent::FormController(this);\nform->addField("email", emailField);  // emailField: fluent::layout::Field*\nQObject::connect(emailEdit, &QLineEdit::textChanged, form, [form] {\n    form->markFieldChanged("email");\n});',
                'import fluentqt\n\nform = fluentqt.FormController(window)\nform.addField("email", email_field)\nemail_edit.textChanged.connect(lambda _: form.markFieldChanged("email"))'),
            section('Async validation', '异步校验',
                'Handle validationRequested(key, token), run your async check, and call completeValidation(key, token, valid, message) with the same token. Results from an older token after the user edits again are ignored automatically.',
                '监听 validationRequested(key, token) 发起异步检查，完成后带着相同的 token 调用 completeValidation(key, token, valid, message) 返回结果。若用户期间又改了内容，旧 token 的结果会自动丢弃。')]),
    guide('foundation-ownership', 'Child widget lifetime', '子控件生命周期',
        'Decide whether a container deletes inserted widgets or leaves cleanup to your app.',
        '控制嵌入容器的子控件由谁负责销毁，避免重复释放或内存泄漏。',
        ['src/components/foundation/WidgetOwnership.h'], [
            section('Three cleanup modes', '三种清理方式',
                'APIs such as Expander::setContentWidget(), Field::setEditor(), SplitView::addPane(), and Accordion::addItem() accept WidgetOwnership: Borrowed leaves cleanup to your app, Reparented restores the old parent when removed, and Owned deletes the widget with the container. Expander, Field, and Accordion default to Borrowed; SplitView::addPane() defaults to Owned.',
                'Expander::setContentWidget()、Field::setEditor()、SplitView::addPane() 和 Accordion::addItem() 等接口支持传入 WidgetOwnership：Borrowed（移除时不删除，仍归应用管理）、Reparented（移除时恢复原来的父控件）、Owned（随容器一起销毁）。默认情况下，Expander、Field 和 Accordion 为 Borrowed，SplitView::addPane() 为 Owned。',
                '#include <FluentQt/Layout.h>\n\nexpander->setContentWidget(editor, fluent::WidgetOwnership::Borrowed);\n// Later, detach without destroying:\nQWidget* taken = expander->takeContentWidget();',
                'expander.setBorrowedContentWidget(editor)\n# Later, detach without destroying:\ntaken = expander.takeContentWidget()'),
            section('PySide6 helpers', 'PySide6 专用方法',
                'PySide6 adds clearer methods such as setBorrowedContentWidget(), setOwnedContentWidget(), and setBorrowedEditor(). Keep a Python reference to any non-owned widget without a Qt parent so garbage collection does not destroy it early.',
                'PySide6 额外提供了直白的区分方法，如 setBorrowedContentWidget()（不接管销毁）、setOwnedContentWidget()（交由容器销毁）和 setBorrowedEditor()。在 Python 中，若控件没有 Qt 父对象且未被容器 Owned，请保留变量引用，防止被垃圾回收提前销毁。')]),
    guide('foundation-qt', 'Using Qt APIs', 'Qt 原生接口',
        'Use standard Qt widget properties, signals, and slots alongside FluentQt.',
        'FluentQt 控件与标准 Qt Widgets 属性、信号槽的配合方式。',
        ['src/compatibility/ApplicationCompat.h', 'src/compatibility/ObjectCompat.h'], [
            section('Inherited Qt methods', '直接使用 Qt 基类方法',
                'Every control inherits standard APIs from its Qt base class—Button inherits QPushButton::clicked and LineEdit inherits QLineEdit::textChanged (composite widgets may hold an inner editor). Check each component\'s API section for inherited methods and Qt doc links.',
                '控件完全保留了 Qt 原生类的方法与信号，例如 Button 可直接用 QPushButton::clicked，LineEdit 可直接用 QLineEdit::textChanged（少数复合控件内部嵌套了子输入框）。具体成员与对应 Qt 官方文档链接见各组件 API 页面。'),
            section('Safe signal connections', '连接信号与防野指针',
                'Use standard Qt signals/slots or QML+ bindings to react to changes. Pass a receiver context object to QObject::connect so connections disconnect automatically when that object is destroyed.',
                '可用 Qt 信号槽或绑定响应状态变化。调用 QObject::connect 时请传入接收方上下文对象，这样对象销毁时连接会自动断开，避免回调已释放的对象。')]),
    guide('module-plugins', 'Plugins', '插件',
        'Start and stop optional app feature modules in dependency order.',
        '按依赖顺序启动和关闭应用内的可选功能模块。',
        ['src/components/foundation/plugins/Plugin.h', 'src/components/foundation/plugins/PluginContext.h',
         'src/components/foundation/plugins/PluginManager.h', 'src/components/foundation/plugins/WidgetPlugin.h'], [
            section('Register and activate', '注册与启动',
                'Link FluentQt::Plugins, register Plugin objects on PluginManager, and call activate() with metadata().id. Dependencies start first and roll back automatically if any plugin fails (PluginManager manages C++/Python objects in-process; it does not load external DLLs/SOs).',
                '链接 FluentQt::Plugins 后，向 PluginManager 注册插件对象并用 metadata().id 调用 activate()。管理器会自动先启动前置依赖，任一步失败时整体回滚。它只管理当前进程内的模块对象，不负责加载外部 DLL/SO 动态库。',
                '#include <FluentQt/Plugins.h>\n\n// greetingPlugin.metadata().id == "example.greeting"\nfluent::plugins::PluginManager manager(host);\nif (manager.registerPlugin(&greetingPlugin))\n    manager.activate(QStringLiteral("example.greeting"));',
                'from fluentqt.plugins import PluginManager\n\n# greeting_plugin.metadata() returns PluginMetadata("example.greeting", ...)\nmanager = PluginManager(host, host)\nif manager.registerPlugin(greeting_plugin):\n    manager.activate("example.greeting")'),
            section('Cleanup and threads', '释放与线程要求',
                'PluginManager does not delete registered plugins, the host widget, or widgets created by WidgetPlugin. Call it only from its own thread, and run deactivateAll() before destroying the host.',
                'PluginManager 不会销毁插件实例、宿主窗口或 WidgetPlugin 创建的子控件。所有调用须在同一线程中进行，销毁宿主前先调用 deactivateAll() 完成清理。')], 'modules'),
    guide('module-particles', 'Particles', '粒子背景',
        'Animated particle backgrounds with optional session pause and cleanup.',
        '可动画化的粒子背景效果，支持按会话统一暂停与释放。',
        ['src/components/particles/ParticleBackdrop.h', 'src/components/particles/ParticlesPlugin.h',
         'src/components/particles/ParticlesSession.h'], [
            section('Add a backdrop', '添加粒子背景',
                'Enable FLUENT_QT_BUILD_PARTICLES and link FluentQt::Particles. Constructing ParticleBackdrop directly runs on CPU (no OpenGL or Spatial required). ParticlesSession pauses and resumes active backdrops without losing their settings.',
                '开启 FLUENT_QT_BUILD_PARTICLES 并链接 FluentQt::Particles 即可使用。直接创建的 ParticleBackdrop 采用 CPU 绘制，不需要 OpenGL 或 Spatial；配合 ParticlesSession 可以一键暂停、恢复背景效果并保留原有参数。',
                '#include <FluentQt/Particles.h>\n\nauto* backdrop = new fluent::particles::ParticleBackdrop(parent);\nbackdrop->setEffect(fluent::particles::ParticleBackdrop::Starfield);',
                'from fluentqt.particles import ParticleBackdrop\n\nbackdrop = ParticleBackdrop(parent)\nbackdrop.setEffect(ParticleBackdrop.Effect.Starfield)'),
            section('Dependencies', '模块依赖',
                'The core FluentQt::FluentQt target stays lightweight without particle code. FluentQt::Particles depends on FluentQt::Plugins and is enabled automatically for Gallery, PySide6, and Spatial.',
                '核心库 FluentQt::FluentQt 不打包粒子代码。FluentQt::Particles 依赖 FluentQt::Plugins，在构建 Gallery、PySide6 或 Spatial 时会自动开启。')], group='modules', component_category='particles'),
    guide('module-spatial', '3D Perspective (Spatial)', '3D 透视视图',
        'Display existing QWidgets with 3D tilt and depth. If OpenGL is unavailable, SpatialView uses CPU rendering and keeps the perspective effects.',
        '给现有 QWidget 添加透视、旋转和景深。OpenGL 不可用时，SpatialView 改用 CPU 绘制，保留透视效果。',
        ['src/components/spatial/SpatialSession.h', 'src/components/spatial/SpatialRuntime.h', 'src/components/spatial/SpatialPlugin.h'], [
            section('Create a 3D view', '创建透视视图',
                'Link FluentQt::Spatial, start a SpatialSession, and create a SpatialView. SpatialView::addWidget() returns a SpatialItem to control offset, rotation, and depth while keeping normal mouse and keyboard input working.',
                '链接 FluentQt::Spatial，启动 SpatialSession 后即可创建 SpatialView。用 SpatialView::addWidget() 添加控件后会返回 SpatialItem，可直接设置它的偏移、旋转和景深，原控件的鼠标与键盘交互照常工作。',
                '#include <FluentQt/Spatial.h>\n\n// The session manages its own PluginManager; window stays caller-owned.\nauto* session = new fluent::spatial::SpatialSession(window, window);\nif (session->activate()) {\n    auto* view = session->createView(parentWidget);\n    fluent::spatial::SpatialItem* item = view->addWidget(card);  // non-owning by default\n}',
                'from fluentqt.spatial import SpatialSession\n\n# The session manages its own PluginManager; window stays caller-owned.\nsession = SpatialSession(window, window)\nif session.activate():\n    view = session.createView(parent_widget)\n    item = view.addBorrowedWidget(card)'),
            section('Renderer and 2D fallback', '渲染模式与 2D 退避',
                'setRenderMode() lets you pick Auto, Raster, or OpenGL; when OpenGL is unavailable, it switches to CPU perspective and explains why in activeBackend() and fallbackReason(). Calling setSpatialEnabled(false) switches to a plain flat column layout (also triggered automatically by reduced motion, high contrast, Tab, or Esc).',
                'setRenderMode() 可选择 Auto、Raster（CPU）或 OpenGL（GPU）绘制；环境不支持 OpenGL 时会自动切到 CPU 透视，并在 activeBackend() 与 fallbackReason() 中说明原因。调用 setSpatialEnabled(false) 可切换回普通纵向 2D 排列；系统开启减弱动效、高对比度或按下 Tab / Esc 时也会自动切回 2D，保证输入与无障碍体验。')], 'modules', component_category='spatial'),
    guide('module-qmlbridge', 'Embed QML (QmlBridge)', '嵌入 QML',
        'Host QML views inside a Qt Widgets window while sharing app themes and data.',
        '在 Qt Widgets 窗口里嵌入 QML 页面，并自动同步当前主题与业务数据。',
        ['src/components/qmlbridge/QmlSession.h', 'src/components/qmlbridge/QmlTheme.h', 'src/components/qmlbridge/QmlPlugin.h'], [
            section('Create a view', '创建 QML 视图',
                'Link FluentQt::QmlBridge, start a QmlSession, and create QmlView widgets. Exposed models remain owned by your app; calling deactivate unloads QML scenes without deleting your views or models.',
                '链接 FluentQt::QmlBridge，激活 QmlSession 后通过 createView() 创建 QmlView。传给 QML 的数据模型仍由应用自己管理；关闭会话只会卸载 QML 页面，不会删掉 QmlView 控件或模型对象。',
                '#include <FluentQt/QmlBridge.h>\n\n// The session manages its own PluginManager; window and models stay caller-owned.\nauto* session = new fluent::qmlbridge::QmlSession(window, window);\nif (session->activate()) {\n    auto* qmlView = session->createView(parentWidget);\n    // Load a QML scene into qmlView.\n}',
                'from fluentqt.qmlbridge import QmlSession\n\n# The session manages its own PluginManager; window and models stay caller-owned.\nsession = QmlSession(window, window)\nif session.activate():\n    qml_view = session.createView(parent_widget)'),
            section('OpenGL compatibility and theme', '图形兼容与主题同步',
                'If Qt Quick shares a window with SpatialView or QOpenGLWidget, call QmlSession::prepareApplication() before creating the first Quick view. Inside QML, use the read-only fluentTheme object (provided by QmlTheme) to read theme colors and fonts.',
                '如果同一窗口里还要放 SpatialView 或 QOpenGLWidget，请在创建第一个 Qt Quick 视图前先调用 QmlSession::prepareApplication()。在 QML 代码中可直接访问只读对象 fluentTheme（由 QmlTheme 提供）来读取当前主题的颜色与字体。')], 'modules', component_category='qml-bridge'),
    guide('foundation-diagnostics', 'UI Diagnostics', '界面自检',
        'Scan a running window for clipped text, missing accessibility names, and tiny buttons.',
        '自动扫描运行中的界面树，排查文字被截断、按钮太小和无障碍名称缺失等问题。',
        ['src/utils/Inspector.h'], [
            section('Run a check', '执行检查',
                'After your window is shown and laid out, call fluent::diagnostics::Inspector::report() to get a JSON report of clipped text, missing accessible names, small click targets, broken focus order, duplicate shortcuts, nested scroll areas, and optional grid alignment.',
                '窗口显示并完成布局后，调用 fluent::diagnostics::Inspector::report() 即可得到一份 JSON 检查报告，列出文字截断、缺少无障碍名称、点击区域过小、焦点顺序异常、重复操作、嵌套滚动区域及可选的对齐检查结果。',
                '#include <FluentQt/Diagnostics.h>\n\nconst QJsonObject report = fluent::diagnostics::Inspector::report(window);',
                'import fluentqt\n\nreport = fluentqt.inspect_widget(window)'),
            section('Test vs visual review', '适用范围',
                'Run Inspector in automated tests to catch layout and accessibility regressions early. Still check colors, backdrop blur, shadows, and interaction feel visually in your app or Gallery.',
                '适合放在自动化测试里提前发现布局截断和无障碍问题；至于实际配色、半透明背景、阴影效果和交互手感，仍建议直接运行应用或 Gallery 查看。')]),
]


@lru_cache(maxsize=1)
def guides():
    localized = {
        language: guide_locale_file(language)["guides"]
        for language in LOCALE_KEYS
        if language not in ("en", "zh")
    }
    merged = []
    for item in GUIDES:
        for header in item['headers']:
            if not (ROOT / header).is_file():
                raise ValueError('Missing guide API header: ' + header)
        name = dict(item['name'])
        description = dict(item['description'])
        sections = [
            {
                'title': dict(sec['title']),
                'body': dict(sec['body']),
                'cpp': sec['cpp'],
                'python': sec['python'],
            }
            for sec in item['sections']
        ]
        for language, guide_map in localized.items():
            entry = guide_map.get(item['id'])
            if not entry:
                raise ValueError(f"Missing {language} translation for guide {item['id']}")
            if len(entry['sections']) != len(sections):
                raise ValueError(f"Section count mismatch in {language} guide {item['id']}")
            if not entry['name'].strip() or not entry['description'].strip():
                raise ValueError(f"Empty {language} metadata for guide {item['id']}")
            name[language] = entry['name']
            description[language] = entry['description']
            for index, sec in enumerate(entry['sections']):
                if not sec['title'].strip() or not sec['body'].strip():
                    raise ValueError(f"Empty {language} section {index} in guide {item['id']}")
                sections[index]['title'][language] = sec['title']
                sections[index]['body'][language] = sec['body']
        merged.append({**item, 'name': name, 'description': description, 'sections': sections})
    return merged


def guide_path(id, language='en'):
    return locale_url_prefix(language) + 'api/' + id + '/index.html'
