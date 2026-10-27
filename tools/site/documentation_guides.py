"""Bilingual introductions to FluentQt's shared foundations and optional modules."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def section(en, zh, english, chinese, cpp='', python=''):
    return {'title': {'en': en, 'zh': zh}, 'body': {'en': english, 'zh': chinese}, 'cpp': cpp, 'python': python}


def guide(id, en, zh, english, chinese, headers, sections, group='foundation'):
    return {'id': id, 'name': {'en': en, 'zh': zh}, 'description': {'en': english, 'zh': chinese},
            'headers': headers, 'sections': sections, 'group': group}


GUIDES = [
    guide('foundation', 'FluentQt foundations', 'FluentQt 基础',
        'The shared building blocks behind the controls.', '了解控件共用的主题、布局、数据和运行方式。',
        ['include/FluentQt/FluentQt.h', 'src/components/foundation/FluentElement.h'], [
            section('Widgets and shared behavior', '控件与共用能力',
                'FluentQt builds on Qt Widgets. Controls retain Qt object ownership, signals, properties and model/view behavior. FluentElement supplies theme access; QML+ adds anchors, property bindings and named states to ordinary widgets.',
                'FluentQt 基于 Qt Widgets，沿用 Qt 的对象归属、信号、属性和模型／视图机制。FluentElement 提供主题访问，QML+ 为普通 QWidget 增加锚点、属性绑定和命名状态。'),
            section('Initialize an application', '初始化应用',
                'Prepare high-DPI settings before QApplication, then initialize Fluent resources. Use the same library through C++ or PySide6. The browser build compiles the C++ implementation to WebAssembly.',
                '先准备高分屏设置，再创建 QApplication 并初始化 Fluent 资源。C++ 和 PySide6 使用同一套库；浏览器版本由 C++ 实现编译为 WebAssembly。',
                '#include <FluentQt/FluentQt.h>\n#include <QApplication>\n\nfluent::prepareHighDpiApplication();\nQApplication app(argc, argv);\nfluent::initializeResources();',
                'import sys\nimport fluentqt\nfrom PySide6.QtWidgets import QApplication\n\nfluentqt.prepare_high_dpi_application()\napp = QApplication(sys.argv)\nfluentqt.initialize_resources()'),
            section('Choose a module', '选择模块',
                'FluentQt::FluentQt provides the 2D controls and design foundations. Plugins, Spatial and QmlBridge are optional targets. Add them when the application needs extension lifetimes, perspective presentation or embedded QML.',
                'FluentQt::FluentQt 提供二维控件和设计基础。Plugins、Spatial 和 QmlBridge 是可选模块，分别用于扩展生命周期、透视展示和嵌入 QML。')]),
    guide('foundation-color', 'Themes and color', '主题与颜色',
        'Use semantic colors and change the application theme.', '使用语义颜色，统一切换应用主题。',
        ['src/components/foundation/FluentElement.h', 'src/components/foundation/ThemeRegistry.h',
         'src/components/foundation/UserTheme.h', 'src/design/ThemeColors.h'], [
            section('Set the theme', '设置主题',
                'Light, Dark and HighContrast are application theme choices. Use the theme API to update existing Fluent controls together. A widget can also have a local theme override.',
                '应用可选择 Light、Dark 和 HighContrast 主题。通过主题 API 可统一更新已有 Fluent 控件，也可以为局部控件单独设置主题。',
                '#include <FluentQt/Foundation.h>\n\nfluent::FluentElement::setTheme(fluent::FluentElement::Dark);',
                'import fluentqt\n\nfluentqt.set_theme(fluentqt.Theme.Dark)'),
            section('Use semantic roles', '按语义选色',
                'Text, fill, stroke, background and accent tokens describe a role rather than a fixed RGB value. Read the current theme tokens when drawing custom content. ThemeRegistry and UserTheme provide application-level customization.',
                '文字、填充、描边、背景和强调色表示用途，不是固定的 RGB 值。自绘内容应读取当前主题对应的颜色。ThemeRegistry 和 UserTheme 用于应用级定制。')]),
    guide('foundation-typography', 'Typography', '字体与排版',
        'Choose font roles for headings, body text and labels.', '用字体角色区分标题、正文和辅助文字。',
        ['src/design/Typography.h'], [
            section('Choose a font role', '选择字体角色',
                'The type ramp includes Caption, Body, BodyStrong, BodyLarge, Subtitle, Title, TitleLarge and Display. Prefer a role so labels follow the application font settings.',
                '字体阶梯包含 Caption、Body、BodyStrong、BodyLarge、Subtitle、Title、TitleLarge 和 Display 等角色。优先使用角色，使文字跟随应用的字体设置。',
                '#include <FluentQt/TextFields.h>\n\nauto* title = new fluent::textfields::Label("Settings", this);\ntitle->setFluentTypography(Typography::FontRole::Subtitle);',
                'import fluentqt\n\ntitle = fluentqt.Label("Settings")\ntitle.setFluentTypography(fluentqt.FontRole.Subtitle)'),
            section('Custom fonts', '自定义字体',
                'Use Typography::fontStyle(...).toQFont() in C++ or font_for_role in Python when a Qt widget needs a QFont. Explicit setFont calls may opt a component out of subsequent theme font updates; check the component API before mixing both approaches.',
                '普通 Qt 控件需要 QFont 时，C++ 可使用 Typography::fontStyle(...).toQFont()，Python 使用 font_for_role。显式调用 setFont 后，部分控件会保留指定字体，不再随主题字体更新；具体行为见控件 API。')]),
    guide('foundation-iconography', 'Iconography', '图标',
        'Use the bundled Fluent icon font and semantic icon names.', '使用内置 Fluent 图标字体和语义化图标名。',
        ['src/design/IconCatalog.h', 'src/design/Typography.h'], [
            section('Use named icons', '使用图标名称',
                'FontIcon and Button can render icon-font glyphs. Prefer named icons from Typography::Icons or fluentqt.Icons instead of storing private-use Unicode codepoints in application code.',
                'FontIcon 和 Button 支持图标字体。优先使用 Typography::Icons 或 fluentqt.Icons 中的名称，避免在业务代码中保存私用区 Unicode 编码。'),
            section('Size and accessible names', '尺寸与无障碍名称',
                'Choose the optical icon size for the control. Icon-only actions still need an accessible name describing their action. The FontIcon component page contains a live example.',
                '根据控件选择图标的光学尺寸。只有图标的操作仍应设置描述动作的无障碍名称。FontIcon 组件页提供可操作示例。')]),
    guide('foundation-geometry', 'Geometry', '圆角与描边',
        'Share shape tokens across controls and surfaces.', '让控件和内容表面使用一致的形状参数。',
        ['src/design/CornerRadius.h', 'src/design/Elevation.h', 'src/design/Material.h'], [
            section('Corner radii', '圆角',
                'CornerRadius provides shared shape values. Theme-aware components read the current radius tokens; application compositions should reuse these values instead of assigning unrelated radii to every surface.',
                'CornerRadius 提供共用圆角值。支持主题的控件读取当前圆角参数；组合界面时也应复用这些值，避免每个表面各用一套圆角。'),
            section('Elevation and materials', '层级与材质',
                'Elevation and Material describe shadows and surface treatments. Overlays remain attached to their owning window. Use the component backdrop API when the material depends on the platform.',
                'Elevation 和 Material 描述阴影与表面效果。浮层依附于所属窗口；材质涉及平台能力时，应使用对应组件的背景 API。')]),
    guide('foundation-spacing', 'Spacing', '间距',
        'Use the spacing scale, padding and control heights.', '统一布局间距、内边距和控件高度。',
        ['src/design/Spacing.h', 'src/design/Breakpoints.h'], [
            section('Compose with spacing tokens', '用间距参数组织布局',
                'Spacing uses a four-pixel base scale and separate names for padding, gaps, borders and control heights. Choose a token by its role; nested groups can use tighter spacing than separate sections.',
                'Spacing 采用 4 像素基础刻度，并区分内边距、控件间隔、边框和控件高度。按用途选择参数：同组控件可更紧凑，不同段落之间应留出更多空间。',
                '#include <FluentQt/Design.h>\n\nlayout->setSpacing(Spacing::Gap::Normal);\nconst int padding = Spacing::Padding::Card;\nlayout->setContentsMargins(padding, padding, padding, padding);',
                'from fluentqt.design import Spacing\n\nlayout.setSpacing(Spacing.Gap.Normal)\npadding = Spacing.Padding.Card\nlayout.setContentsMargins(padding, padding, padding, padding)'),
            section('Adapt to available space', '适配可用空间',
                'Use layout size policies and minimum sizes to let content wrap or scroll. Breakpoint tokens support application layout decisions; they do not replace the widget layout system.',
                '通过尺寸策略和最小尺寸让内容换行或滚动。断点参数用于应用布局决策，不替代 Qt 的控件布局系统。')]),
    guide('foundation-qmlplus', 'QML+ for Widgets', 'QML+：锚点、绑定与状态',
        'Add declarative relationships to QWidget controls.', '为 QWidget 控件建立声明式关系。',
        ['src/components/foundation/QMLPlus.h'], [
            section('Bind properties', '绑定属性',
                'A binding keeps a target property synchronized with its source. QML+ works on Widgets and does not require a QML engine. Retain a managed BindingHandle when the application needs pause, resume or reset control.',
                '属性绑定让目标属性跟随源属性变化。QML+ 直接工作于 Widgets，不需要 QML 引擎。需要暂停、恢复或重置时，保留可管理的 BindingHandle。',
                'auto* slider = new fluent::basicinput::Slider(Qt::Horizontal, this);\nslider->setRange(0, 100);\nauto* bar = new fluent::status_info::ProgressBar(this);\nbar->setRange(0, 100);\nbar->bind("value", slider, "value");',
                'import fluentqt\nfrom PySide6.QtCore import Qt\n\nslider = fluentqt.Slider(Qt.Horizontal, panel)\nslider.setRange(0, 100)\nbar = fluentqt.ProgressBar(panel)\nbar.setRange(0, 100)\nfluentqt.bind(slider, "value", bar, "value")'),
            section('Anchors and named states', '锚点与命名状态',
                'AnchorLayout positions widgets through edge relationships. Named states group property changes, while behaviors and transitions animate those changes. Keep the application model as the source of business data.',
                'AnchorLayout 通过边缘关系定位控件。命名状态将一组属性变化组织在一起，行为和过渡负责变化过程。业务数据仍由应用模型持有。')]),
    guide('foundation-motion', 'Motion', '动效',
        'Respect application-wide and local animation preferences.', '统一应用动效，并尊重控件自身的开关。',
        ['src/components/foundation/MotionPolicy.h', 'src/design/Animation.h'], [
            section('Choose a motion mode', '选择动效模式',
                'Full uses normal motion. Reduced keeps short state transitions and stops continuous motion. Disabled stops motion. A component whose animationEnabled is false remains still regardless of the global mode.',
                'Full 使用常规动效；Reduced 保留短暂状态过渡并停止持续动画；Disabled 停止动效。控件自身的 animationEnabled 为 false 时，不受全局模式影响。',
                '#include <FluentQt/Foundation.h>\n\nfluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Reduced);',
                'import fluentqt\n\nfluentqt.set_motion_mode(fluentqt.MotionMode.Reduced)'),
            section('Resolve custom animations', '处理自定义动画',
                'Custom effects can use shouldAnimate and resolvedDuration to follow the same policy. Use continuous-motion classification for loops rather than treating a loop as repeated short transitions.',
                '自定义效果可使用 shouldAnimate 和 resolvedDuration 跟随同一策略。循环动画应标记为持续动效，不应当作不断重复的短过渡。')]),
    guide('foundation-models', 'Data models', '数据模型',
        'Keep data separate from its visual presentation.', '将数据与显示方式分开。',
        ['src/components/charts/ChartModel.h', 'src/components/collections/SelectionMode.h'], [
            section('Collection views', '集合视图',
                'ListView, TreeView and DataGrid use Qt item models and selection models. The application owns the data and supplies the model. Change the model through its public API and notifications so every attached view stays synchronized.',
                'ListView、TreeView 和 DataGrid 使用 Qt 条目模型与选择模型。应用持有数据并提供模型，通过模型的公开 API 和通知更新数据，使多个视图保持同步。'),
            section('Chart models', '图表模型',
                'ChartModel stores chart samples, labels and derived queries. Chart views borrow their model; painting and data export read from it. ChartData is the snapshot value type used to transfer a dataset.',
                'ChartModel 保存图表样本、标签并提供数据查询。图表视图借用模型，绘制与数据导出从模型读取内容。ChartData 是用于传递整组数据的快照值类型。')]),
    guide('foundation-forms', 'Forms and validation', '表单与校验',
        'Coordinate fields without moving business rules into the UI.', '协调字段状态，业务校验仍由应用负责。',
        ['src/components/foundation/FormController.h'], [
            section('Register fields', '登记字段',
                'FormController borrows Field objects and tracks dirty, touched, validating, valid and submitting states. It does not own field values or validators. Report every value change, including programmatic changes, with markFieldChanged.',
                'FormController 借用 Field，跟踪修改、触碰、校验和提交状态，不接管字段值或校验器。用户编辑和程序修改都应调用 markFieldChanged。'),
            section('Complete asynchronous validation', '完成异步校验',
                'Handle validationRequested in application code and return its token to completeValidation. Edits, resets and field replacement invalidate older tokens. Perform controller operations on its GUI thread.',
                '应用处理 validationRequested，并携带对应 token 调用 completeValidation。编辑、重置和字段替换会使旧 token 失效；控制器操作应在其 GUI 线程执行。')]),
    guide('foundation-ownership', 'Widget ownership', '控件归属与生命周期',
        'Know who retains and destroys embedded widgets.', '明确嵌入控件由谁持有、由谁销毁。',
        ['src/components/foundation/WidgetOwnership.h'], [
            section('Borrowed, owned and reparented content', '借用、持有与重挂载',
                'Container APIs distinguish borrowed, owned and reparented widgets. Select the ownership mode explicitly where supported, and use take methods when transferring content out of a host. Models remain application-owned.',
                '容器 API 区分借用、持有和重挂载。支持归属参数的接口应明确选择模式；从宿主转移内容时使用 take 类方法。模型仍由应用持有。'),
            section('Python references', 'Python 引用',
                'PySide6 wrappers expose explicit ownership helpers such as setBorrowedContentWidget and setOwnedContentWidget. Keep references to parentless objects while they are in use. The component API lists binding-specific helpers separately.',
                'PySide6 提供 setBorrowedContentWidget、setOwnedContentWidget 等明确归属的辅助方法。无父对象的实例在使用期间应保留 Python 引用；各组件 API 单独列出绑定层扩展。')]),
    guide('foundation-qt', 'Qt objects and signals', 'Qt 对象与信号',
        'Use Qt behavior that FluentQt controls inherit.', '使用 FluentQt 控件继承的 Qt 能力。',
        ['src/compatibility/ApplicationCompat.h', 'src/compatibility/ObjectCompat.h'], [
            section('Inherited APIs', '继承的 API',
                'Fluent controls preserve their Qt base classes. Button inherits QPushButton, for example, so text, clicked and other Qt APIs remain available. Each component reference links to its Qt base and lists FluentQt additions and inherited Fluent capabilities.',
                'Fluent 控件保留 Qt 基类。例如 Button 继承 QPushButton，仍可使用 text、clicked 等 Qt API。组件参考页链接对应 Qt 基类，并列出 FluentQt 新增接口及继承的 Fluent 能力。'),
            section('Signals and object lifetimes', '信号与对象生命周期',
                'Connect signals to application operations instead of polling widgets. Qt parent-child ownership and receiver contexts govern object and connection lifetimes. Use Qt Core documentation for the object model, event loop and threading rules.',
                '通过信号连接应用操作，避免轮询控件。Qt 父子关系与接收上下文决定对象和连接的生命周期。对象模型、事件循环和线程规则可继续查阅 Qt Core 文档。')]),
    guide('module-plugins', 'Plugins', '扩展生命周期',
        'Register and activate optional linked extensions.', '登记和激活可选扩展。',
        ['src/components/foundation/plugins/Plugin.h', 'src/components/foundation/plugins/PluginContext.h',
         'src/components/foundation/plugins/PluginManager.h', 'src/components/foundation/plugins/WidgetPlugin.h'], [
            section('Activation and dependencies', '激活与依赖',
                'Link FluentQt::Plugins when an application needs managed extension lifetimes. PluginManager activates dependencies first and rolls back the current attempt on failure. It manages linked plugin objects rather than discovering shared libraries.',
                '需要管理扩展生命周期时链接 FluentQt::Plugins。PluginManager 先激活依赖，失败时回滚本次激活。它管理已链接的插件对象，不负责扫描和加载动态库。'),
            section('Ownership and cleanup', '归属与清理',
                'Plugins, the host and returned widgets remain caller-owned. Keep them on the manager thread and retain parentless Python wrappers. Deactivate extensions before destroying the host.',
                '插件、宿主和返回的控件仍由调用方持有。它们应与管理器位于同一线程，Python 端应保留无父对象包装器的引用。销毁宿主前先停用扩展。')], 'modules'),
    guide('module-spatial', 'Spatial', 'Spatial 空间展示',
        'Present existing Widgets with perspective and depth.', '为已有 Widgets 增加透视和层次。',
        ['src/components/spatial/SpatialSession.h', 'src/components/spatial/SpatialRuntime.h', 'src/components/spatial/SpatialPlugin.h'], [
            section('Use a session', '使用会话',
                'Link FluentQt::Spatial and use SpatialSession to activate the extension and create views. SpatialView and SpatialItem handle presentation and transforms. Retain the native 2D fallback when perspective rendering is unavailable.',
                '链接 FluentQt::Spatial，通过 SpatialSession 激活扩展并创建视图。SpatialView 和 SpatialItem 负责展示与变换；透视渲染不可用时保留原生二维回退。'),
            section('Compose with application content', '承载应用内容',
                'The application keeps its widgets and models. Spatial changes how that content is presented; input, accessibility and overlays retain their owning-window contracts. The SpatialView component page provides the live scene.',
                '控件和模型仍由应用持有。Spatial 改变内容的呈现方式，输入、无障碍和浮层继续遵守所属窗口的行为约定。SpatialView 组件页提供交互场景。')], 'modules'),
    guide('module-qmlbridge', 'QmlBridge', 'QmlBridge 桥接',
        'Host application QML alongside Widgets.', '在 Widgets 应用中承载 QML。',
        ['src/components/qmlbridge/QmlSession.h', 'src/components/qmlbridge/QmlTheme.h', 'src/components/qmlbridge/QmlPlugin.h'], [
            section('Load QML through a session', '通过会话加载 QML',
                'Link FluentQt::QmlBridge and use QmlSession to activate the bridge and create QmlView instances. Application context models remain borrowed. Deactivation unloads tracked scenes and preserves caller-owned views and models.',
                '链接 FluentQt::QmlBridge，通过 QmlSession 激活桥接并创建 QmlView。应用上下文模型仍被借用；停用时卸载受追踪场景，保留调用方的视图和模型。'),
            section('Graphics and theme', '图形后端与主题',
                'Choose the Qt Quick graphics API before creating its first surface, especially when sharing a window with Spatial or QOpenGLWidget. QmlTheme exposes Fluent theme values to QML. QML+ bindings for Widgets do not require this module.',
                '首次创建 Qt Quick 表面前应选择图形后端，与 Spatial 或 QOpenGLWidget 共用窗口时尤其如此。QmlTheme 向 QML 提供 Fluent 主题值。Widgets 的 QML+ 绑定不依赖此模块。')], 'modules'),
    guide('foundation-diagnostics', 'Diagnostics', '检查与诊断',
        'Inspect the widget tree and application state.', '检查控件树和应用状态。',
        ['src/utils/Inspector.h'], [
            section('Inspect a live interface', '检查运行中的界面',
                'Inspector reports widget hierarchy, geometry and exposed state for diagnostics. Read a snapshot when investigating layout or interaction; it is not a replacement for the application data model.',
                'Inspector 提供控件层级、几何和公开状态，用于诊断布局与交互。它读取界面快照，不替代应用的数据模型。'),
            section('Verification scope', '验证范围',
                'A widget snapshot can explain geometry and state. Pointer, keyboard, accessibility and platform rendering still need checks in the running interface. Public inspection entry points are listed in the API reference.',
                '控件快照可以解释几何与状态。鼠标、键盘、无障碍和平台绘制仍需在运行界面中检查。公开检查入口列在 API 参考中。')]),
]


def guides():
    for item in GUIDES:
        for header in item['headers']:
            if not (ROOT / header).is_file():
                raise ValueError('Missing guide API header: ' + header)
    return GUIDES


def guide_path(id, language='en'):
    return ('zh-CN/' if language == 'zh' else '') + 'api/' + id + '/index.html'
