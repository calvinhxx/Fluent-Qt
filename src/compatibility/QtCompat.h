#pragma once

/**
 * @brief Source-compatible umbrella for Qt and platform adaptation helpers.
 * zh_CN: 保留既有调用的 Qt 与平台兼容层总入口。
 *
 * New implementation files include the owning domain header directly.
 * Existing types, functions, and event-construction macros remain available.
 * zh_CN: 新实现按职责引用对应头文件；已有类型、函数和事件构造宏继续可用。
 */

#include "AccessibilityCompat.h"
#include "ApplicationCompat.h"
#include "EventConstructionCompat.h"
#include "FontCompat.h"
#include "InputEventCompat.h"
#include "ObjectCompat.h"
#include "PaintCompat.h"
#include "QtFeatures.h"
#include "WidgetCompat.h"
