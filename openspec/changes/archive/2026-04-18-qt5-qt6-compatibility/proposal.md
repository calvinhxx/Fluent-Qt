## Why

当前工程虽在 `CMakeLists.txt` 中通过 `find_package(QT NAMES Qt6 Qt5 ...)` 声明同时支持 Qt5/Qt6，但源代码大量使用了 Qt6-only API（最典型的是 `enterEvent(QEnterEvent*)`），在 Qt5 环境下无法编译通过。需要建立一套统一的版本兼容机制，让同一份代码可在 Qt 5.15 LTS 与 Qt 6.x 上同时构建运行，方便老项目集成与逐步迁移。

## What Changes

- 新增 `src/common/QtCompat.h` 兼容层头文件，集中管理 Qt5/Qt6 API 差异（事件类型别名、通用宏 `FE_ENTER_EVENT_TYPE`、命名空间差异等）
- 在所有重写 `enterEvent()` 的组件（Button / Slider / RadioButton / ToggleSwitch / RatingControl / FlipView 等共 10+ 文件）补齐 `#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)` 守卫或改用 `QtCompat.h` 别名
- CMake 中新增最低版本约束（Qt 5.15 / Qt 6.2），并在 Qt5 路径下追加必要 link target（如 `Qt5::Widgets` 已可工作，但需验证 `QRegularExpressionValidator` 等模块归属）
- 新增 CI 双版本验证脚本说明（README/docs 文档化），但**不**在本次改动中接入 CI
- **不修改**任何视觉表现、Design Token、组件公共 API 行为；仅做编译期兼容

## Capabilities

### New Capabilities
- `qt-version-compatibility`: 工程级 Qt5/Qt6 双版本源码与构建兼容能力，定义事件 API 兼容契约、CMake 版本约束、最低支持版本

### Modified Capabilities
（无 — 现有组件 capability 的对外行为/视觉/API 不变）

## Impact

- **源码影响**: `src/view/basicinput/` 下 6 个文件、`src/view/collections/FlipView.{h,cpp}` 缺失版本守卫；`src/common/` 新增 `QtCompat.h`
- **构建影响**: `CMakeLists.txt` 增加 Qt 最低版本检查；不引入新依赖
- **测试影响**: 新增一个轻量编译期断言测试 `TestQtCompat.cpp` 验证别名宏在两个版本下都能正确解析；现有测试代码若使用 `enterEvent` 同样需补守卫
- **下游影响**: 集成方现可在 Qt 5.15+ 项目中直接使用本静态库
