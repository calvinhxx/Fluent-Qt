#pragma once

#include <QVariant>

namespace compatibility::detail {

// Keep QVariant's type-id conversion behind the Qt compatibility boundary.
// zh_CN: 将 QVariant 的类型 ID 转换差异集中在 Qt 兼容层。
inline bool convertVariantToType(QVariant& value, int type)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return value.convert(QMetaType(type));
#else
    return value.convert(type);
#endif
}

} // namespace compatibility::detail
