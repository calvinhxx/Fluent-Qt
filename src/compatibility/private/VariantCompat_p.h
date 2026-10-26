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

// Qt 5 compares floating numeric variants fuzzily. Preserve representable edits
// like adjacent large doubles, while retaining native numeric promotion and
// integer-to-integer precision.
// zh_CN: Qt 5 对浮点数使用模糊比较；保留相邻大数值改动、原生数值提升及整数间比较精度。
inline bool variantValuesEqual(const QVariant& first, const QVariant& second)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    const auto numeric = [](int type) {
        switch (type) {
        case QMetaType::Bool:
        case QMetaType::Char:
        case QMetaType::SChar:
        case QMetaType::UChar:
        case QMetaType::Short:
        case QMetaType::UShort:
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::Long:
        case QMetaType::ULong:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::Float:
        case QMetaType::Double:
            return true;
        default:
            return false;
        }
    };
    const int firstType = first.userType(), secondType = second.userType();
    const bool floating = firstType == QMetaType::Double || firstType == QMetaType::Float
        || secondType == QMetaType::Double || secondType == QMetaType::Float;
    if (floating && numeric(firstType) && numeric(secondType))
        return first.toDouble() == second.toDouble();
#endif
    return first == second;
}

} // namespace compatibility::detail
