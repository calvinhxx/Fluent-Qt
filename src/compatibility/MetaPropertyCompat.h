#pragma once

#include <QByteArray>
#include <QMetaProperty>
#include <QVariant>

/**
 * @brief Matches a qualified property type, including namespace enum aliases retained by older moc.
 * zh_CN: 匹配属性的限定类型名，兼容旧版 moc 保留的未限定命名空间枚举别名。
 */
inline bool fluentMetaPropertyHasTypeName(const QMetaProperty& property, const QByteArray& qualifiedType)
{
    if (!property.isValid())
        return false;
    const QByteArray declaredType(property.typeName());
    if (declaredType == qualifiedType)
        return true;
    if (declaredType.contains("::") || !property.enclosingMetaObject())
        return false;
    const QByteArray owner(property.enclosingMetaObject()->className());
    const int namespaceEnd = owner.lastIndexOf("::");
    return namespaceEnd >= 0 && owner.left(namespaceEnd + 2) + declaredType == qualifiedType;
}

/**
 * @brief Packs an enum for a property whose moc metadata may expose its alias as an integer.
 * zh_CN: 为元属性封装枚举值，兼容 moc 将枚举别名暴露为整数的情况。
 */
template <typename Enum> QVariant fluentEnumPropertyValue(const QMetaProperty& property, Enum value)
{
    return property.userType() == QMetaType::Int ? QVariant(static_cast<int>(value)) : QVariant::fromValue(value);
}
