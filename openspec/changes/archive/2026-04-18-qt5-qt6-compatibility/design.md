## Context

工程当前 `CMakeLists.txt` 已声明 `find_package(QT NAMES Qt6 Qt5 REQUIRED ...)`，目标链接采用 `Qt${QT_VERSION_MAJOR}::Widgets`，CMake 层面已是 Qt5/Qt6 双兼容。

但源代码层面存在 Qt 主版本之间的 API breaking change，最广泛的就是 `QWidget::enterEvent` 签名：
- **Qt 5.x**: `virtual void enterEvent(QEvent *event)`
- **Qt 6.x**: `virtual void enterEvent(QEnterEvent *event)`

排查发现：
- `src/view/textfields/`、`src/view/basicinput/Button.h`、`src/view/basicinput/ComboBox.{h,cpp}`、`src/view/scrolling/ScrollBar.{h,cpp}`、`src/view/collections/{ListView,GridView,TreeView}.{h,cpp}` 已使用 `#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)` 守卫
- 以下文件**缺失**版本守卫（Qt5 编译会报"override 不匹配"或"未知类型 QEnterEvent"）：
  - `src/view/basicinput/RatingControl.{h,cpp}`
  - `src/view/basicinput/ToggleSwitch.{h,cpp}`
  - `src/view/basicinput/RadioButton.{h,cpp}`
  - `src/view/basicinput/Slider.{h,cpp}`
  - `src/view/collections/FlipView.{h,cpp}`

其他常见 Qt5/Qt6 差异（如 `QStringRef`、`QString::SkipEmptyParts`、`QRegExp`）经全量搜索**未发现使用**，本工程 API 差异点收敛于 `enterEvent` 一处。

## Goals / Non-Goals

**Goals:**
- 同一份源码可在 **Qt 5.15.x LTS** 与 **Qt 6.2+** 上无修改编译通过
- 提供集中式 `QtCompat.h` 兼容层，封装 `enterEvent` 参数类型差异，避免在每个组件文件分散 `#if QT_VERSION ...`
- 不破坏现有 Qt6 构建（macOS/MSVC）
- 在 CMake 层面强制最低版本检查，防止 Qt 5.14 及以下版本静默通过却运行异常

**Non-Goals:**
- **不**引入 Qt5 / Qt6 任何运行时行为差异（如 HiDPI 默认值、QString 编码默认值）
- **不**实现 CI 自动化双版本构建（仅文档说明手动验证步骤）
- **不**支持 Qt 5.14 及以下版本（缺少 C++17 标准库特性 / `Q_PROPERTY` BINDABLE 等）
- **不**为 Qt5 用户回退视觉效果（如缺失某些 Qt6 渲染优化时不补 polyfill）

## Decisions

### Decision 1: 兼容层入口为单文件 `src/common/QtCompat.h`，仅含宏与 typedef，不引入 .cpp

**Rationale**: 
- API 差异收敛于事件类型一处，无需独立编译单元
- 头文件被 `FluentElement.h` 直接 include 后可下沉到所有组件，零侵入
- 纯宏/类型别名零运行时成本

**Pattern**:
```cpp
// QtCompat.h
#pragma once
#include <QtGlobal>
#include <QEvent>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEnterEvent>
using FluentEnterEvent = QEnterEvent;
#else
using FluentEnterEvent = QEvent;
#endif
```

调用方统一写 `void enterEvent(FluentEnterEvent* event) override;`，无需 `#if`。

### Decision 2: `FluentEnterEvent` 类型别名而非宏

**Rationale**: 
- typedef/using 提供编译期类型安全，IDE 可跳转
- 宏在 moc 解析时偶尔出问题（实测 Q_OBJECT 类中 override 声明用宏会触发 moc warning）

### Decision 3: CMake 中追加版本下限检查

```cmake
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Test Network)
if(QT_VERSION_MAJOR EQUAL 5 AND QT_VERSION VERSION_LESS 5.15)
    message(FATAL_ERROR "Qt 5.15 or newer is required (found ${QT_VERSION})")
endif()
if(QT_VERSION_MAJOR EQUAL 6 AND QT_VERSION VERSION_LESS 6.2)
    message(FATAL_ERROR "Qt 6.2 or newer is required (found ${QT_VERSION})")
endif()
```

**Rationale**: Qt 5.15 是最后一个 LTS 且 API 接近 Qt6，Qt 6.2 是 macOS Universal binary 起点。

### Decision 4: 已有 `#if QT_VERSION_CHECK` 守卫的文件保留原写法，不强制替换为 `FluentEnterEvent`

**Rationale**: 
- 避免大面积 churn，本次改动 scope 限定为"使能 Qt5 编译通过"
- 守卫与别名等价，但守卫更显式
- 后续可单独发起重构 change 统一为别名

### Decision 5: 测试文件中的 `enterEvent` 同样需要兼容

测试 mock 类若重写 `enterEvent` 必须使用 `FluentEnterEvent` 或加守卫。新增 `tests/views/TestQtCompat.cpp` 进行编译期断言：
```cpp
static_assert(std::is_base_of_v<QEvent, FluentEnterEvent>, "FluentEnterEvent must derive from QEvent");
```

## Risks / Trade-offs

### Risk 1: Qt5 默认 HiDPI 行为不同
- **影响**: Qt 5.15 默认未启用 `Qt::AA_EnableHighDpiScaling`，部分组件在 Retina 屏可能渲染模糊
- **缓解**: 在示例文档中提示集成方在 `main()` 中显式调用，**本库不强制**

### Risk 2: Qt5 缺失 `QStringConverter` 等 Qt6-only 类
- **影响**: 若未来组件需要新 API，会再次出现兼容缺口
- **缓解**: 维护方在引入新依赖时优先查阅 Qt5 是否存在等价 API；必要时扩展 `QtCompat.h`

### Risk 3: Qt5 与 Qt6 二进制不兼容，集成方需重新编译
- **影响**: `fluent_qt_lib.a` 静态库无法跨版本复用
- **缓解**: 文档说明，构建产物按 Qt 主版本归档（如 `lib/qt5/`、`lib/qt6/`）

### Trade-off: 不接入 CI 双构建
- **得**: 本次改动小、可快速合入
- **失**: 后续修改可能再次引入 Qt5 不兼容代码，需依赖人工 review

## Migration Plan

1. 新增 `src/common/QtCompat.h`，由 `FluentElement.h` include 一次
2. 修复 5 组缺失守卫的文件：直接改写为 `FluentEnterEvent`（最简最一致）
3. CMake 追加版本下限检查
4. 在 macOS 上分别用 Qt 5.15 与 Qt 6.9 构建并 `ctest` 验证（手动），文档化步骤
5. 已有 `#if QT_VERSION_CHECK(6, 0, 0)` 写法的文件**不变**
