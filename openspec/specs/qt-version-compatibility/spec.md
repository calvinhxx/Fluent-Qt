# qt-version-compatibility Specification

## Purpose

工程 SHALL 同时兼容 Qt 5.15+ 与 Qt 6.2+，所有版本差异通过集中式兼容层 `src/compatibility/QtCompat.h` 封装；UI 源码与测试代码禁止散落 `#if QT_VERSION_CHECK` 守卫。

## Requirements

### Requirement: 工程 SHALL 同时兼容 Qt 5.15+ 与 Qt 6.2+ 编译构建

工程源码 MUST 在 Qt 5.15.x 与 Qt 6.2+ 两个主版本下使用同一份代码无修改编译通过，且 `ctest` 全量单元测试 MUST 通过（VisualCheck 测试除外）。

CMake 配置 MUST：
- 通过 `find_package(QT NAMES Qt6 Qt5 REQUIRED ...)` 自动选择可用版本
- 通过 `Qt${QT_VERSION_MAJOR}::<Module>` 进行 target 链接
- 在配置阶段对 Qt 5 < 5.15 与 Qt 6 < 6.2 的版本 `message(FATAL_ERROR)`

#### Scenario: Qt 5.15 环境下 cmake configure 与 build 成功
- **WHEN** 在仅安装 Qt 5.15.x 的环境执行 `cmake -B build && cmake --build build`
- **THEN** 配置与构建 MUST 成功，`fluent_qt_lib` 静态库与所有测试 target MUST 生成

#### Scenario: Qt 6.2+ 环境下 cmake configure 与 build 成功
- **WHEN** 在仅安装 Qt 6.2+ 的环境执行 `cmake -B build && cmake --build build`
- **THEN** 配置与构建 MUST 成功，输出与 Qt5 路径在公共 API 上完全一致

#### Scenario: Qt 5.14 及以下版本被显式拒绝
- **WHEN** 用户在 Qt 5.14 环境执行 `cmake -B build`
- **THEN** CMake MUST 以 `FATAL_ERROR` 终止，并输出 `Qt 5.15 or newer is required` 提示

#### Scenario: 两个版本下 ctest 全量通过
- **WHEN** 在 Qt5 或 Qt6 环境下执行 `ctest --output-on-failure`（设置 `SKIP_VISUAL_TEST=1`）
- **THEN** 所有非 VisualCheck 测试 MUST PASS

### Requirement: 工程 SHALL 提供集中式 Qt 版本兼容层 `QtCompat.h`

工程 MUST 在 `src/compatibility/QtCompat.h` 提供单一头文件，集中封装 Qt5/Qt6 之间的事件类型差异：

- MUST 定义类型别名 `FluentEnterEvent`，在 Qt6 等价于 `QEnterEvent`，在 Qt5 等价于 `QEvent`
- MUST 通过 `using` 别名（而非 `#define` 宏）实现，以保证 IDE 跳转与 moc 兼容
- MUST 不引入任何 .cpp 实现，零运行时成本
- MUST 被 `FluentElement.h` 直接 include，使所有继承 `FluentElement` 的组件透明可用

新增组件 MUST 在重写 `enterEvent` 时使用 `FluentEnterEvent` 类型而非 `QEvent` 或 `QEnterEvent`，避免再次出现版本耦合。

#### Scenario: FluentEnterEvent 在 Qt6 解析为 QEnterEvent
- **WHEN** 工程在 Qt 6.x 编译，`QtCompat.h` 被 include
- **THEN** `static_assert(std::is_same_v<FluentEnterEvent, QEnterEvent>)` MUST 通过

#### Scenario: FluentEnterEvent 在 Qt5 解析为 QEvent
- **WHEN** 工程在 Qt 5.15 编译，`QtCompat.h` 被 include
- **THEN** `static_assert(std::is_same_v<FluentEnterEvent, QEvent>)` MUST 通过

#### Scenario: FluentEnterEvent 始终派生自 QEvent
- **WHEN** 工程在任一 Qt 版本编译
- **THEN** `static_assert(std::is_base_of_v<QEvent, FluentEnterEvent>)` MUST 通过

### Requirement: 所有重写 enterEvent 的组件 SHALL 使用版本兼容签名

所有继承 `QWidget` 子类并重写 `enterEvent` 的工程组件文件，MUST 使用 `FluentEnterEvent` 类型别名作为参数类型。

MUST NOT 在 UI 源码中使用 `#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)` 形式的散落版本守卫；所有版本差异 MUST 通过 `QtCompat.h` 提供的别名/内联函数/宏统一封装。

#### Scenario: 全部 enterEvent 重写均使用 FluentEnterEvent
- **WHEN** 在 `src/view/` 下 grep `enterEvent\(`
- **THEN** 所有命中行 MUST 使用 `FluentEnterEvent*` 作为参数类型

#### Scenario: src/ 与 tests/ 不存在散落的 QT_VERSION_CHECK
- **WHEN** 执行 `grep -rn 'QT_VERSION_CHECK' src/ tests/`
- **THEN** 命中 MUST 仅限于 `src/compatibility/QtCompat.h` 内部，以及测试中针对版本断言的少量必要语句

### Requirement: QtCompat.h SHALL 封装鼠标事件坐标 API 差异

`QtCompat.h` MUST 提供以下内联函数：

- `inline QPoint fluentMousePos(const QMouseEvent* e)` — Qt6 返回 `e->position().toPoint()`，Qt5 返回 `e->pos()`
- `inline QPoint fluentMouseGlobalPos(const QMouseEvent* e)` — Qt6 返回 `e->globalPosition().toPoint()`，Qt5 返回 `e->globalPos()`

调用方 MUST 在所有依赖鼠标坐标的事件处理中使用这两个 helper，禁止直接调用 `position()`/`globalPosition()`。

#### Scenario: Slider 与 Dialog 的拖拽逻辑在 Qt5 与 Qt6 下表现一致
- **WHEN** 在任一 Qt 版本下用鼠标拖拽 `Slider` 滑块或 `Dialog` 标题栏
- **THEN** 视觉响应 MUST 一致；源码 MUST NOT 出现 `position()` / `globalPosition()` 直接调用

### Requirement: QtCompat.h SHALL 封装 QStyledItemDelegate::initStyleOption 的版本差异

`QtCompat.h` MUST 提供宏 `FLUENT_INIT_VIEW_ITEM_OPTION(optPtr)`：

- Qt6 展开为 `view()->initViewItemOption(optPtr)`（要求调用方上下文存在 `view()` 访问器）
- Qt5 展开为 `*(optPtr) = view()->viewOptions()`

`ListView` / `GridView` / `TreeView` 内部的 delegate 在初始化 `QStyleOptionViewItem` 时 MUST 使用此宏。

#### Scenario: 三大集合视图 delegate 在 Qt5 与 Qt6 下都能取到 view item option
- **WHEN** 在任一 Qt 版本编译并运行 ListView/GridView/TreeView 单测
- **THEN** 编译 MUST 通过、`paint` 路径 MUST 取得正确的 `QStyleOptionViewItem`

### Requirement: QtCompat.h SHALL 封装 QColor::getHsvF 等 API 的浮点类型差异

`QtCompat.h` MUST 提供类型别名 `FluentColorComponent`：

- Qt6 等价于 `float`
- Qt5 等价于 `qreal`

调用 `QColor::getHsvF` / `getRgbF` / `getHslF` 时 MUST 使用 `FluentColorComponent` 局部变量传入 `&` 地址，禁止硬编码 `float*` 或 `qreal*`。

#### Scenario: ColorPicker 在 Qt5 与 Qt6 下都能编译并取得 HSV 分量
- **WHEN** 在任一 Qt 版本编译 `ColorPicker.cpp`
- **THEN** 编译 MUST 通过；运行时 HSV 分量数值 MUST 与原 `QColor` 一致

### Requirement: 测试代码 SHALL 通过 FLUENT_MAKE_ENTER_EVENT 宏构造 enter 事件

`QtCompat.h` MUST 提供宏 `FLUENT_MAKE_ENTER_EVENT(name, x, y)`：

- Qt6 展开为 `QEnterEvent name(QPointF(x, y), QPointF(x, y), QPointF(x, y))`
- Qt5 展开为 `QEvent name(QEvent::Enter)`

测试代码 MUST 使用此宏构造 enter 事件，禁止散落 `#if QT_VERSION_CHECK` 守卫。

#### Scenario: 三大集合视图测试在两个 Qt 版本下都能构造并派发 enter 事件
- **WHEN** 在任一 Qt 版本运行 `test_list_view`/`test_grid_view`/`test_tree_view`
- **THEN** 测试 MUST 通过

### Requirement: QtCompat.h SHALL 封装滚轮与原生手势事件 API 差异

`QtCompat.h` MUST provide centralized helpers for Qt version differences used by ScrollView zoom input:

- `fluentWheelPosition(const QWheelEvent*)` MUST return the wheel event local position as `QPointF` on all supported Qt versions.
- `fluentNativeGesturePosition(const FluentNativeGestureEvent*)` MUST return the native gesture local position as `QPointF` on all supported Qt versions that expose native gesture events.
- Production view code MUST use these helpers instead of local `QT_VERSION_CHECK` branches for `QWheelEvent::position()` / `posF()` or `QNativeGestureEvent::position()` / `localPos()`.

#### Scenario: ScrollView zoom wheel position is version-independent

- **WHEN** `ScrollView` handles Ctrl+wheel zoom input on Qt 5.15 or Qt 6.2+
- **THEN** it MUST obtain the zoom anchor through `fluentWheelPosition`
- **AND** `src/view/scrolling/ScrollView.cpp` MUST NOT contain a local `QT_VERSION_CHECK` branch for wheel event position APIs

#### Scenario: ScrollView native gesture position is version-independent

- **WHEN** `ScrollView` handles `QEvent::NativeGesture` zoom input on Qt 5.15 or Qt 6.2+
- **THEN** it MUST obtain the zoom anchor through `fluentNativeGesturePosition`
- **AND** `src/view/scrolling/ScrollView.cpp` MUST NOT contain a local `QT_VERSION_CHECK` branch for native gesture position APIs

### Requirement: QtCompat.h SHALL 封装测试所需的原生手势构造差异

`QtCompat.h` MUST provide a centralized way for tests to determine whether synthetic native gesture events can be constructed and to construct the event when supported.

The helper surface MUST:

- Avoid requiring component tests to conditionally include `QNativeGestureEvent` or `QPointingDevice`.
- Keep Qt 5.15 builds valid when native gesture event construction is unavailable.
- Preserve explicit test skip behavior for native gesture tests on unsupported Qt versions.

#### Scenario: TestScrollView has no conditional native gesture includes

- **WHEN** `tests/views/scrolling/TestScrollView.cpp` is compiled on any supported Qt version
- **THEN** it MUST include `compatibility/QtCompat.h` for native gesture test helpers
- **AND** it MUST NOT contain local conditional includes for `QNativeGestureEvent` or `QPointingDevice`

#### Scenario: Native gesture tests skip through compatibility capability

- **WHEN** the supported Qt version cannot synthesize the native gesture event used by the test
- **THEN** `TestScrollView` MUST skip native gesture tests through a compatibility capability helper or macro
- **AND** the skip reason MUST remain clear to the test reader

### Requirement: Component code SHALL NOT introduce scattered Qt version guards outside compatibility boundaries

Project source and tests MUST keep Qt version checks inside compatibility boundaries except for explicit compatibility self-tests. Ordinary view component code and component tests MUST use `QtCompat.h` helpers when a Qt API difference is already supported by the compatibility layer.

Allowed boundaries include:

- `src/compatibility/QtCompat.h`
- `src/compatibility/WindowChromeCompat.h`
- `src/compatibility/WindowChromeCompat_*.cpp`
- focused Qt compatibility tests such as `tests/views/TestQtCompat.cpp`

#### Scenario: Audit catches component-level QT_VERSION_CHECK usage

- **WHEN** the compatibility audit test scans `src/view` and component tests
- **THEN** any `QT_VERSION_CHECK` usage outside the allowlisted compatibility boundaries MUST fail the test
- **AND** the failure message MUST identify the offending file path

#### Scenario: Compatibility boundary version checks remain allowed

- **WHEN** the compatibility audit test scans the project
- **THEN** intentional version checks inside `QtCompat.h` and platform compatibility files MUST remain allowed
- **AND** the audit MUST NOT require removing platform feature checks from `WindowChromeCompat_mac.cpp`

### Requirement: TextBlock 头文件 SHALL NOT 引入 `Label` 类型别名

After the rename, `src/view/textfields/Label.h` MUST declare a real `view::textfields::Label` class and MUST NOT introduce `using Label = TextBlock` or `using TextBlock = Label`. The old `TextBlock.h` public header is removed by the rename.

#### Scenario: Label.h 在 Qt5 / MSVC 编译通过
- **WHEN** 在 Qt 5.15 + MSVC 环境编译包含 `Label.h` 的翻译单元
- **THEN** 编译 MUST 不出现 `C2371: Label 重定义`

#### Scenario: 不保留 TextBlock 别名
- **WHEN** 引入 `view/textfields/Label.h`
- **THEN** `Label` MUST be a class declaration, not an alias to `TextBlock`
- **AND** `src/view/textfields/TextBlock.h` MUST NOT exist
