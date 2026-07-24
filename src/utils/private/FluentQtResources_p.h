#ifndef FLUENTQTRESOURCES_P_H
#define FLUENTQTRESOURCES_P_H

namespace fluent::resources {

// Registers the compiled Qt resource collection without requiring a
// QGuiApplication. This private entry point keeps resource-backed catalogs safe
// during static-library startup.
// zh_CN: 在不依赖 QGuiApplication 的情况下注册编译资源集合，使静态库启动
// 阶段读取资源目录保持安全。
void ensureRegistered();

} // namespace fluent::resources

#endif // FLUENTQTRESOURCES_P_H
