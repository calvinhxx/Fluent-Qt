## Why

`AutoSuggestBox` 放在自定义 `TitleBar` 中时，suggestion popup 弹出后会覆盖或拦截输入框右侧 clear/close button 的 hover/click 交互。用户在 popup 打开状态下仍应能直接清空当前输入，否则搜索框弹窗会让控件自身的关闭入口失效。

## What Changes

- 修复 `AutoSuggestBox` suggestion popup 打开时对输入框内置 clear/close button 的鼠标事件影响。
- 保持 suggestion popup 的 light-dismiss 行为：点击 popup 外部仍会关闭建议列表。
- 保证 clear/close button 在 popup 打开时仍能进入 hover/pressed 状态并清空文本、关闭建议列表。
- 补充 `TestWindow`/相关自动化或可视化验证，覆盖用户给出的标题栏搜索框场景。
- 不改变 `AutoSuggestBox` 公共 API、suggestions 数据模型或 query submission 语义。

## Capabilities

### New Capabilities

### Modified Capabilities
- `auto-suggest-box`: suggestion popup 打开时不得阻断控件内置 clear/close button 的 hover 和 click 交互。

## Impact

- `src/view/textfields/AutoSuggestBox.*`：调整 suggestion popup 打开、定位或事件处理策略。
- `src/view/dialogs_flyouts/Popup.*` / `Flyout.*`：如根因位于通用 popup light-dismiss 或窗口内覆盖层事件处理，需要做最小范围修复。
- `tests/views/windowing/TestWindow.cpp`：增加覆盖标题栏搜索框 popup 打开时 clear/close button hover/click 的验证，并保留人工点击 VisualCheck 流程。
