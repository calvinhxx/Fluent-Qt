#pragma once

#include <QtGlobal>

// Optional Quick capabilities; this header adds no Qt Quick dependency to Widgets.
// zh_CN: 可选 Quick 能力开关，不向 Widgets 引入 Qt Quick 依赖。
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define FLUENT_QUICK_USES_RHI 1
#else
#define FLUENT_QUICK_USES_RHI 0
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
#define FLUENT_QUICK_HAS_RENDER_TARGET_DPR 1
#else
#define FLUENT_QUICK_HAS_RENDER_TARGET_DPR 0
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0) && QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
#define FLUENT_QUICK_NEEDS_PRIVATE_RENDER_WINDOW_HOOK 1
#else
#define FLUENT_QUICK_NEEDS_PRIVATE_RENDER_WINDOW_HOOK 0
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0) && QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
#define FLUENT_QUICK_NEEDS_UNSHOWN_REPARENT_GUARD 1
#else
#define FLUENT_QUICK_NEEDS_UNSHOWN_REPARENT_GUARD 0
#endif
