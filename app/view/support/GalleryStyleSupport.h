#ifndef GALLERYSTYLESUPPORT_H
#define GALLERYSTYLESUPPORT_H

#include <QColor>
#include <QString>
#include <QWidget>

#include "components/windowing/WindowBackdrop.h"

namespace fluent::gallery {

/**
 * @brief Formats a QColor as a Qt style-sheet `rgba(...)` literal including alpha.
 * zh_CN: 将 QColor 格式化为带 alpha 的 Qt 样式表 `rgba(...)` 字面量。
 */
inline QString cssColor(const QColor& color)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

/**
 * @brief Whether a widget shares a composited transparent window backdrop.
 * zh_CN: 控件是否共享系统合成的透明窗口背景。
 *
 * Keep the Gallery's backing-store paint decision behind the typed UILib
 * backdrop contract. The attribute guard preserves the existing safety rule:
 * a transparent clear is valid only when the top-level surface is translucent.
 * zh_CN: 将 Gallery 的后备缓冲绘制判断收敛到 UILib 的强类型背景契约；属性守卫保留现有安全规则：
 * 仅当顶层表面确实为半透明时，才能擦除为透明。
 */
inline bool usesCompositedWindowBackdrop(const QWidget* widget)
{
    const QWidget* topLevel = widget ? widget->window() : nullptr;
    return topLevel
        && topLevel->testAttribute(Qt::WA_TranslucentBackground)
        && windowing::windowBackdropRequiresTransparentClear(topLevel);
}

/**
 * @brief Whether the top-level window supplies an app-painted backdrop material.
 * zh_CN: 顶层窗口是否由 UILib 提供应用侧绘制的背景材质。
 */
inline bool usesPaintedWindowBackdrop(const QWidget* widget)
{
    return windowing::windowBackdropUsesPaintedMaterial(widget);
}

/**
 * @brief Whether the window currently represents Mica or Acrylic material.
 * zh_CN: 窗口当前是否呈现 Mica 或 Acrylic 材质。
 */
inline bool usesWindowMaterialBackdrop(const QWidget* widget)
{
    return windowing::windowHasMaterialBackdrop(widget);
}

} // namespace fluent::gallery

#endif // GALLERYSTYLESUPPORT_H
