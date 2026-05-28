## 1. 兼容层 QtCompat.h

- [x] 1.1 创建 `src/common/QtCompat.h`，定义 `FluentEnterEvent` 类型别名（Qt6→`QEnterEvent`，Qt5→`QEvent`），包含必要的 `<QtGlobal>`、`<QEvent>`、`<QEnterEvent>`（条件 include）
- [x] 1.2 在 `src/view/FluentElement.h` 顶部 include `<common/QtCompat.h>`，使所有继承 `FluentElement` 的组件透明可用
- [x] 1.3 添加 `static_assert` 检查 `FluentEnterEvent` 派生自 `QEvent`（写在 QtCompat.h 末尾）

## 2. CMake 版本下限

- [x] 2.1 在 `CMakeLists.txt` 的 `find_package` 之后追加 Qt5 < 5.15 与 Qt6 < 6.2 的 `FATAL_ERROR` 检查
- [x] 2.2 验证 `cmake -B build` 在当前 Qt 6.9 环境仍正常通过

## 3. 修复缺失版本守卫的组件

- [x] 3.1 `src/view/basicinput/RatingControl.{h,cpp}`：将 `enterEvent(QEnterEvent*)` 改为 `enterEvent(FluentEnterEvent*)`
- [x] 3.2 `src/view/basicinput/ToggleSwitch.{h,cpp}`：同上改写
- [x] 3.3 `src/view/basicinput/RadioButton.{h,cpp}`：同上改写
- [x] 3.4 `src/view/basicinput/Slider.{h,cpp}`：同上改写
- [x] 3.5 `src/view/collections/FlipView.{h,cpp}`：同上改写

## 4. 测试

- [x] 4.1 创建 `tests/views/TestQtCompat.cpp`，包含 3 个 TEST：
  - `FluentEnterEventDerivesFromQEvent`：static_assert 断言
  - `FluentEnterEventMatchesQt6Type`（仅 Qt6）：`std::is_same_v<FluentEnterEvent, QEnterEvent>`
  - `FluentEnterEventMatchesQt5Type`（仅 Qt5）：`std::is_same_v<FluentEnterEvent, QEvent>`
- [x] 4.2 在 `tests/views/CMakeLists.txt` 中通过 `add_qt_test_module(test_qt_compat TestQtCompat.cpp)` 注册
- [x] 4.3 在 Qt 6.9 环境运行 `cmake --build build --target test_qt_compat && ./build/tests/views/test_qt_compat` 验证通过
- [x] 4.4 在 Qt 6.9 环境运行 `cmake --build build && ctest --output-on-failure -E VisualCheck` 全量回归（确认未引入退化）

## 5. 文档

- [x] 5.1 在 `readme.md` 添加"Qt 版本支持"章节，说明：最低 Qt 5.15 / Qt 6.2，构建命令在两版本下均为 `cmake -B build && cmake --build build`
- [x] 5.2 更新 `.github/copilot-instructions.md` 的 Conventions 章节，加入"新组件重写 enterEvent 时 MUST 使用 `FluentEnterEvent`"规则

## 6. Qt5 实机验证（手动）

- [ ] 6.1 在装有 Qt 5.15.x 的环境（如 Linux/macOS Homebrew qt@5）执行 `cmake -B build-qt5 -DCMAKE_PREFIX_PATH=<qt5_path>`
- [ ] 6.2 执行 `cmake --build build-qt5` 确认编译通过
- [ ] 6.3 执行 `cd build-qt5 && ctest --output-on-failure` 确认所有非 VisualCheck 测试通过

## 7. QtCompat.h 全面整合（消除散落 #if 守卫）

- [x] 7.1 扩展 `QtCompat.h` 新增以下兼容设施：
  - `inline QPoint fluentMousePos(const QMouseEvent*)` — 包装 Qt6 `position().toPoint()` / Qt5 `pos()`
  - `inline QPoint fluentMouseGlobalPos(const QMouseEvent*)` — 包装 Qt6 `globalPosition().toPoint()` / Qt5 `globalPos()`
  - `FLUENT_INIT_VIEW_ITEM_OPTION(optPtr)` 宏 — 包装 Qt6 `initViewItemOption(opt)` / Qt5 `*opt = viewOptions()`
  - `FLUENT_MAKE_ENTER_EVENT(name, x, y)` 宏 — 测试代码统一构造 enter 事件
  - `FluentColorComponent` 类型别名 — Qt6→`float`，Qt5→`qreal`，用于 `QColor::getHsvF/getRgbF/getHslF`
- [x] 7.2 扫描并移除 `src/view/` 下所有 `#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)` 守卫，统一改用 `QtCompat.h` 提供的别名/宏：
  - `Button.h`、`ComboBox.{h,cpp}`、`ScrollBar.{h,cpp}`、`LineEdit.{h,cpp}`、`TextEdit.{h,cpp}`
  - `ListView.{h,cpp}`、`GridView.{h,cpp}`、`TreeView.{h,cpp}`、`FlipView.{h,cpp}`
  - `RatingControl.{h,cpp}`、`ToggleSwitch.{h,cpp}`、`RadioButton.{h,cpp}`、`Slider.{h,cpp}`
  - `Dialog.cpp`（mouse globalPos）
- [x] 7.3 扫描并移除 `tests/views/` 下所有 `#if QT_VERSION_CHECK` 守卫：
  - `TestTreeView.cpp` / `TestListView.cpp` / `TestGridView.cpp` 改用 `#include "common/QtCompat.h"` 与 `FLUENT_MAKE_ENTER_EVENT` 宏
- [x] 7.4 验证 `grep -rn 'QT_VERSION_CHECK' src/ tests/` 仅在 `QtCompat.h` 与必要测试断言中出现

## 8. Qt5 实测兼容性修复（4 项）

- [x] 8.1 `ColorPicker.cpp:346` — `QColor::getHsvF` 类型差异：Qt5 用 `qreal*`、Qt6 用 `float*`。改用 `FluentColorComponent h, s, v, a;` 局部变量后调用，赋值给 `qreal m_h/m_s/m_v` 时由编译器隐式转换
- [x] 8.2 `Slider.cpp:107,121` — Qt5 `QMouseEvent::position()` 不存在，改用 `fluentMousePos(event).x()/y()`
- [x] 8.3 `ListView.cpp:481`、`GridView.cpp:762`、`TreeView.cpp:736` — Qt5 无 `initViewItemOption`，改用 `FLUENT_INIT_VIEW_ITEM_OPTION(&opt)` 宏
- [x] 8.4 `TextBlock.h` — 移除 `using Label = TextBlock;`（Qt5/Windows SDK 已存在 `Label` 符号导致 `C2371: Label 重定义`），并在原位添加注释说明禁止再次引入

## 9. Qt5 / MSVC 实测补漏（测试代码）

- [x] 9.1 测试源文件补齐 `#include <QDebug>`：Qt5 / MSVC 不会通过其他头隐式引入 `QDebug`，会触发 `C2027: 使用了未定义的类型 "QDebug"`
  - `tests/views/basicinput/TestDropDownButton.cpp`
  - `tests/views/basicinput/TestSplitButton.cpp`
  - `tests/views/basicinput/TestButton.cpp`
  - `tests/views/TestSegoe.cpp`
- [x] 9.2 `tests/views/collections/FluentTreeItemDelegate.cpp:298` — `QMouseEvent::position()` 在 Qt5 不存在，改用 `fluentMousePos(me)`，并添加 `#include "common/QtCompat.h"`
