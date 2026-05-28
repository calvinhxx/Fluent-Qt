## Context

`AutoSuggestBox` 的 suggestions 使用内部 `SuggestionListPopup`，该类继承 `Flyout`/`Popup`。`Popup::open()` 会把 popup reparent 到所属 top-level window 并 `raise()`，同时为了绘制阴影保留 `Spacing::Standard` 的 contents margin。当前 `SuggestionListPopup` 使用较小的 anchor offset；popup 的透明阴影区域可能伸回 `AutoSuggestBox` 输入框区域，导致输入框右侧 clear/close button 的 hover 和 mouse press 被 popup widget 抢先命中。

用户给出的复现场景在 `tests/views/windowing/TestWindow.cpp`：`AutoSuggestBox` 被放入自定义 `TitleBar`，输入文本后 popup 展开，右侧 X 按钮无法正常 hover/click。该场景同时涉及 titlebar stacking，但行为归属仍是 `AutoSuggestBox`：popup 打开不能阻断控件内部按钮交互。

## Goals / Non-Goals

**Goals:**
- 保证 suggestion popup 打开时，`AutoSuggestBox` clear/close button 仍响应 hover、pressed 和 click。
- clear/close button click 后清空文本、隐藏按钮并关闭 suggestion popup。
- 保持 popup suggestion item click、keyboard navigation、Escape 和 outside press light-dismiss 行为。
- 在 `TestWindow` 标题栏搜索框场景中做人工点击验证，并尽量增加可自动运行的回归测试。

**Non-Goals:**
- 不重做 `AutoSuggestBox` 公共 API。
- 不改变 suggestion filtering/model 行为。
- 不改变通用 `Window` caption button 设计。
- 不引入新的 popup 动画或外部依赖。

## Decisions

1. **先定位真实事件接收方，再做最小修复。**
   - 复现时检查 popup 几何、shadow margin、`AutoSuggestBoxClearButton` 几何和鼠标命中对象。
   - 如果透明 shadow margin 覆盖 clear button，应优先让 popup 的有效鼠标区域或堆叠顺序不覆盖 owner input rect。
   - 备选方案是单纯增大 `anchorOffset`，但这会让可见 popup 离输入框过远，视觉上不如修正命中区域或 stacking 精确。

2. **修复点优先收敛在 `AutoSuggestBox` 的 suggestion popup。**
   - 该问题由 `AutoSuggestBox` 内置按钮与其内部 popup 的组合触发，首选在 `SuggestionListPopup::showForOwner()` 附近处理。
   - 只有确认通用 `Popup`/`Flyout` 的透明 margin 会影响其他控件时，才把修复上移到 `Popup`/`Flyout`。

3. **保留 light-dismiss 语义。**
   - 点击 suggestion list 外部时仍必须关闭 popup。
   - 点击 clear/close button 同时属于 owner 内部交互：应让按钮先完成 click，再关闭 popup 和清空文本。

4. **验证覆盖自动化与人工窗口点击。**
   - 自动化测试应模拟 popup 打开后移动到 clear button、点击按钮，并断言文本被清空、popup 关闭。
   - 人工验证运行 `TestWindow`，在标题栏搜索框输入文本打开 popup，实际 hover/click 红框 X 按钮确认视觉状态和点击行为。

## Risks / Trade-offs

- [Risk] 修复通用 `Popup` hit-test 可能影响 Dialog/Flyout 的 outside press 行为。→ Mitigation: 默认在 `AutoSuggestBox` 内部修复；若修改通用层，补跑 `test_popup` / `test_flyout`。
- [Risk] 仅增加 popup offset 可以修复点击但造成视觉间距异常。→ Mitigation: 优先选择不改变可见 card 与输入框关系的方案。
- [Risk] `TestWindow` 的原生窗口行为在 CI/offscreen 平台不可完全模拟。→ Mitigation: 自动化覆盖 QWidget 事件路径，人工运行 `TestWindow` 做最终交互确认。
