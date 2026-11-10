#pragma once

namespace fluent::support::clipboard {
// Application-facing facade for native clipboard write confirmation.
// zh_CN: 应用侧原生剪贴板写入确认入口，不属于安装后的公共 API。
bool writeSucceeded();
} // namespace fluent::support::clipboard
