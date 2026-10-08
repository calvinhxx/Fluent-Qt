#include "Plugin.h"

namespace fluent::plugins {

Plugin::Plugin(QObject* parent) : QObject(parent) {}

Plugin::~Plugin() = default;

} // namespace fluent::plugins
