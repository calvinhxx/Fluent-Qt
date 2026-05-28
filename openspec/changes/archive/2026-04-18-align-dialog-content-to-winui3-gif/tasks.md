## 1. Dialog scale 幅度加大

- [x] 1.1 在 `src/view/dialogs_flyouts/Dialog.cpp` 匿名 namespace 中，将 `constexpr double kScaleFrom = 0.96` 改为 `0.90`
- [x] 1.2 验证 `setAnimationProgress(p)` 中现有 resize + move 逻辑无需修改（scale 由 `kScaleFrom` 驱动）
- [x] 1.3 在 `Dialog.h` 顶部注释中更新动画方案说明：入场 90%→100%、退场 100%→90%（配合 opacity 0↔1）

## 2. Smoke 蒙层淡入淡出（使用自定义 progress 属性，非 QGraphicsOpacityEffect）

- [x] 2.1 在 `Dialog.cpp` 匿名 namespace 的 `SmokeOverlay` 类中新增 `Q_PROPERTY(double progress READ progress WRITE setProgress)` 与成员 `double m_progress = 0.0`
- [x] 2.2 `SmokeOverlay::setProgress(double p)` 更新 `m_progress` 并调用 `update()`
- [x] 2.3 修改 `SmokeOverlay::paintEvent`：将 `fillRect` 的颜色 alpha 按 `m_progress` 插值（`QColor(0,0,0, int(baseAlpha * m_progress))`）
- [x] 2.4 在 `Dialog` 类中新增成员 `QPropertyAnimation* m_smokeAnim = nullptr`
- [x] 2.5 修改 `Dialog::showSmokeOverlay()`：创建 overlay 后懒创建 `m_smokeAnim`，target 为 overlay 的 `progress` 属性，启动 `0.0 → 1.0`，duration = `themeAnimation().normal`，easing = `entrance`
- [x] 2.6 修改 `Dialog::hideSmokeOverlay()`：启动 `m_smokeAnim` `1.0 → 0.0`，`finished` 信号中 `deleteLater()` overlay 并将 `m_smokeOverlay = nullptr`；若已在淡出中则不重复启动
- [x] 2.7 `hideEvent` 路径中不立即销毁 overlay，让 `hideSmokeOverlay()` 的动画收尾后销毁
- [x] 2.8 确认 `SmokeOverlay` 的 `Q_OBJECT` 宏在 .cpp 中能被 moc 正确处理（CMake AUTOMOC 识别），否则将类提取到独立头文件

## 3. ContentDialog 按钮条左对齐 + 最小宽度

- [x] 3.1 在 `ContentDialog.cpp` 顶部添加 `static constexpr int kButtonMinWidth = 96;`
- [x] 3.2 在 `setupInternalLayout()` 中将 `btnLayout->addWidget(m_primaryBtn, 1)` 等三行的 stretch 参数改为 `0`
- [x] 3.3 在三个按钮 `new Button(m_buttonBar)` 后分别 `setMinimumWidth(kButtonMinWidth)`
- [x] 3.4 在 `btnLayout->addWidget(m_closeBtn, 0)` 之后追加 `btnLayout->addStretch(1)` 实现左对齐

## 4. ContentDialog 按钮条高度收紧

- [x] 4.1 在 `ContentDialog.cpp` 中将 `static constexpr int kButtonBarHeight = 80` 改为 `68`
- [x] 4.2 验证 `vPad = (68 - Spacing::ControlHeight::Standard) / 2 = 18` 计算仍然成立（无需硬编码，沿用现有算法）

## 5. 测试更新

- [x] 5.1 在 `tests/views/dialogs_flyouts/TestDialog.cpp` 新增测试 `DialogEntranceHasScalePopIn`：`open()` 后在动画早期帧断言 `size().width() < targetSize.width() * 0.95`（幅度明显超过旧 0.96）
- [x] 5.2 在 `TestDialog.cpp` 新增测试 `DialogExitHasScalePopOut`：驱动 `done()` 动画，末尾帧断言 `size().width() ≈ targetSize.width() * 0.90`
- [x] 5.3 在 `TestDialog.cpp` 新增测试 `SmokeOverlayFadesInOut`：启用 smoke，`open()` 后首帧断言 `m_smokeOverlay->progress() ≈ 0`；`done()` 后断言 overlay 仍存在（延迟销毁）且 `progress` 正在递减
- [x] 5.4 在 `tests/views/dialogs_flyouts/TestContentDialog.cpp` 新增测试 `ButtonsHaveMinimumWidth`：断言三个按钮 `minimumWidth() == 96`
- [x] 5.5 在 `TestContentDialog.cpp` 新增测试 `ButtonBarIsLeftAligned`：断言最右侧可见按钮的 `geometry().right() < buttonBar->contentsRect().right()`
- [x] 5.6 在 `TestContentDialog.cpp` 新增测试 `ButtonBarHeightIs68`：断言内部 `buttonBar` widget `height() == 68`

## 6. 构建与验证

- [x] 6.1 增量构建并运行 `test_dialog`：`cmake --build build --target test_dialog && ./build/tests/views/dialogs_flyouts/test_dialog`
- [x] 6.2 增量构建并运行 `test_content_dialog`：`cmake --build build --target test_content_dialog && ./build/tests/views/dialogs_flyouts/test_content_dialog`
- [x] 6.3 运行 VisualCheck 人工复核：`./build/tests/views/dialogs_flyouts/test_content_dialog --gtest_filter="*VisualCheck*"`，与参考 GIF 比对入/出场缩放幅度、蒙层淡入淡出、按钮左对齐、按钮条紧凑度

## 7. 动画速度调优（VisualCheck 反馈后追加）

- [x] 7.1 `Dialog::showEvent` 中主动画 `m_animation->setDuration(anim.normal)` 改为 `anim.fast`（250ms → 150ms），贴合 GIF 节奏
- [x] 7.2 `Dialog::showSmokeOverlay` 中 `m_smokeAnim->setDuration(anim.normal)` 改为 `anim.fast`，与主动画同步
- [x] 7.3 `Dialog::done` 中退场 duration 保持 `anim.fast`（未改）；验证入/出场对称

## 8. Smoke 蒙层残留 Bug 修复

- [x] 8.1 在 `Dialog.h` 中添加 `~Dialog() override;` 声明
- [x] 8.2 在 `Dialog.cpp` 中实现 `~Dialog()`：若 `m_smokeOverlay` 非空则 `hide()` + `delete`（同步销毁，放弃未完成的淡出动画）
- [x] 8.3 保留原有 `hideSmokeOverlay()` 异步 `deleteLater()` 路径（正常关闭仍走完整淡出）
- [x] 8.4 重新运行 `test_dialog` + `test_content_dialog` 回归：全部通过

## 9. VisualCheck 示例内容对齐参考图

- [x] 9.1 在 `TestContentDialog.cpp` 顶部添加 `#include <QVBoxLayout>` 与 `#include "view/basicinput/CheckBox.h"`
- [x] 9.2 将"Standard 2-Button Dialog"示例的尺寸从 `572x300` 调整为 `454x232`
- [x] 9.3 Content 从单一长段落 TextBlock 改为 `QWidget` + `QVBoxLayout` 容器，依次放：短正文 TextBlock（"Lorem ipsum dolor sit amet, adipisicing elit."）+ CheckBox（"Upload your content to the cloud."）+ stretch
- [x] 9.4 按钮组增加 `setCloseButtonText("Cancel")`，形成 Save / Don't Save / Cancel 三按钮（对齐参考图）
- [x] 9.5 重新构建 `test_content_dialog` 验证编译通过

## 10. ContentDialog 布局不显示 Bug 修复

现象：参考图应显示 Title + Body + CheckBox + ButtonBar，实际运行时 Title 被遮盖、ButtonBar 消失、整个 dialog 变灰。

根因：`QWidget::isVisible()` 返回"有效可视性"——祖先未 `show()` 时永远返回 `false`。ContentDialog 两处在 dialog 显示**之前**（构造阶段 `setTitle()` / `setPrimaryButtonText()` 等 setter 调用）使用了 `isVisible()` 做布局决策，导致误判。

- [x] 10.1 `ContentDialog::updateContentAnchors()`：将 `if (m_titleLabel->isVisible())` 改为 `if (!m_titleLabel->text().isEmpty())`，避免 content 错误锚到 dialog 顶部覆盖 title
- [x] 10.2 `ContentDialog::updateButtonBar()`：将 `anyVisible = m_primaryBtn->isVisible() || ...` 改为基于 `text().isEmpty()` 的判断，避免 buttonBar 被错误隐藏
- [x] 10.3 `updateButtonBar()` 末尾追加 `layout()->invalidate()` 确保布局重算
- [x] 10.4 重新运行 `test_content_dialog` + VisualCheck 验证 Title/Content/ButtonBar 三区完整显示

## 11. VisualCheck 内容区背景色污染修复

现象：Title 与 Content 区域显示为灰色（应为白色 bgLayer），看起来像 Dialog 内的 TextBlock / CheckBox 自带灰底。

根因：`FluentTestWindow::onThemeUpdated()` 用 `setStyleSheet("background-color: ...")` 设置主窗口背景。**Qt QSS 会沿 widget 父子链向所有子孙传播**，包括弹出的 ContentDialog 内的 TextBlock / CheckBox / contentHost，导致它们被错误染成 `bgCanvas` 灰色。

- [x] 11.1 `TestContentDialog.cpp` 中 `FluentTestWindow::onThemeUpdated()`：将 `setStyleSheet(...)` 改为 `QPalette::Window` + `setAutoFillBackground(true)`，避免样式表传播到 dialog 子控件
- [x] 11.2 重新运行 VisualCheck 验证：Title 与 Content 区域显示为白色（bgLayer），与参考图一致

