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
 * Clears to transparent only when the top-level window is translucent and uses
 * a compositor-backed backdrop.
 * zh_CN: 仅当顶层窗口启用了半透明且由系统合成器提供背景时，才允许擦除为透明。
 */
inline bool usesCompositedWindowBackdrop(const QWidget* widget)
{
    const QWidget* topLevel = widget ? widget->window() : nullptr;
    return topLevel && topLevel->testAttribute(Qt::WA_TranslucentBackground)
        && windowing::windowBackdropRequiresTransparentClear(topLevel);
}

/**
 * @brief Whether the top-level window supplies an app-painted backdrop material.
 * zh_CN: 顶层窗口是否使用应用侧绘制的背景材质。
 */
inline bool usesPaintedWindowBackdrop(const QWidget* widget)
{
    return windowing::windowBackdropUsesPaintedMaterial(widget);
}

/**
 * @brief Whether the window currently uses Mica or Acrylic backdrop material.
 * zh_CN: 窗口当前是否启用了 Mica 或 Acrylic 背景材质。
 */
inline bool usesWindowMaterialBackdrop(const QWidget* widget)
{
    return windowing::windowHasMaterialBackdrop(widget);
}

} // namespace fluent::gallery

#endif // GALLERYSTYLESUPPORT_H
