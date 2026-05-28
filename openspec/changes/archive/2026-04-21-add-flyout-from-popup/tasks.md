## 1. 源文件创建

- [x] 1.1 创建 `src/view/dialogs_flyouts/Flyout.h`：声明 `Flyout` 类、`Placement` 枚举（Top/Bottom/Left/Right/Full/Auto）、`anchor`/`placement`/`anchorOffset`/`clampToWindow` 属性、`showAt(QWidget*)`/`setAnchor(QWidget*)` 接口
- [x] 1.2 创建 `src/view/dialogs_flyouts/Flyout.cpp`：实现构造函数（默认 modal=false、dim=false、CloseOnPressOutside|CloseOnEscape）、`computePosition()` 重写、`anchorRectInTopLevel()` / `clampCardPos()` / `resolveAutoPlacement()` 私有辅助
- [x] 1.3 在 `Flyout.cpp` 顶部添加 `kShadowMargin` 常量并注释说明与 `Popup.cpp` 的隐式契约（`computePosition()` 路径需子类自减 shadow margin）

## 2. CMake 集成

- [x] 2.1 验证 `src/CMakeLists.txt`（或顶层 globbing）能自动收集到新增的 `Flyout.h/.cpp`，必要时手动添加
- [x] 2.2 构建验证：`cmake --build build --target fluent_qt_lib` 无错误无新警告

## 3. 单元测试

- [x] 3.1 创建 `tests/views/dialogs_flyouts/TestFlyout.cpp`，复用 `TestPopup.cpp` 的 `FluentTestWindow` + `SetUpTestSuite` 模式
- [x] 3.2 `DefaultProperties`：验证默认 placement=Bottom、anchorOffset=8、clampToWindow=true、modal=false、dim=false、closePolicy 含 CloseOnPressOutside|CloseOnEscape
- [x] 3.3 `ShowAtSetsAnchorAndOpens`：调用 `flyout->showAt(button)`，验证 `anchor()==button` 且 `isOpen()==true`
- [x] 3.4 `BottomPlacement`：anchor 居窗口中央，验证 flyout 卡片中心 X 与 anchor 中心 X 对齐，卡片顶部 = anchor 底部 + 8
- [x] 3.5 `TopPlacement`：同 3.4，验证卡片底部 = anchor 顶部 - 8
- [x] 3.6 `LeftPlacement` / `RightPlacement`：验证水平距离 8、垂直居中
- [x] 3.7 `AutoFlipsToTopWhenBottomInsufficient`：anchor 紧贴底部，验证 Auto 反转到 Top
- [x] 3.8 `ClampToWindowKeepsCardInside`：anchor 紧贴右边缘，验证卡片右边 ≤ window.right - 4
- [x] 3.9 `NoAnchorFallsBackToCenter`：未设 anchor 直接 open，验证位置 = 基类居中
- [x] 3.10 `AnchorDestroyedSafely`：构造 anchor → showAt → delete anchor → 触发重新计算位置（如 resize），不 crash
- [x] 3.11 `EscapeClosesFlyout` / `ClickOutsideClosesFlyout`：验证继承自 Popup 的 light-dismiss 行为
- [x] 3.12 `FullPlacementCenters`：Placement=Full，验证位置与 Popup 居中一致
- [x] 3.13 `VisualCheck`（带 SKIP_VISUAL_TEST 守卫）：在 800×600 窗口四角各放一个 Button，每个挂一个 Flyout 演示 6 种 Placement 与 Auto 反转

## 4. CMake 测试注册

- [x] 4.1 在 `tests/views/dialogs_flyouts/CMakeLists.txt` 末尾追加 `add_qt_test_module(test_flyout TestFlyout.cpp)`

## 5. 验证

- [x] 5.1 增量构建并运行：`cmake --build build --target test_flyout && ./build/tests/views/dialogs_flyouts/test_flyout`，所有用例通过
- [x] 5.2 回归 Popup：`cmake --build build --target test_popup && ./build/tests/views/dialogs_flyouts/test_popup`，确认 Popup 行为零回归
- [x] 5.3 `openspec validate add-flyout-from-popup --strict` 通过
- [x] 5.4 `openspec status --change add-flyout-from-popup` 显示 4/4 artifacts done
