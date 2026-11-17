#ifndef FLUENTQTRESOURCES_P_H
#define FLUENTQTRESOURCES_P_H

namespace fluent::resources {

// Registers the compiled Qt resource collection without requiring QGuiApplication,
// keeping resource-backed catalogs safe to read during static-library startup.
void ensureRegistered();

} // namespace fluent::resources

#endif // FLUENTQTRESOURCES_P_H
