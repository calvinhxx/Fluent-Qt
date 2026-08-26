#ifndef OVERLAYSHADOW_H
#define OVERLAYSHADOW_H

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QRect>

#include "design/Elevation.h"

namespace fluent::overlay {

/**
 * @brief Default per-layer opacity multiplier for overlay card shadows.
 * zh_CN: 浮层卡片阴影的默认逐层透明度系数。
 */
constexpr qreal defaultShadowIntensity = 0.35;
constexpr int defaultShadowLayerCount = 10;
constexpr int defaultShadowVerticalOffset = 2;

/**
 * @brief Paints a WinUI-style soft shadow as stacked translucent rounded rects.
 * zh_CN: 以叠加的半透明圆角矩形绘制 WinUI 风格柔和阴影。
 *
 * Dialog, Menu, Popup, and other overlay cards share this painter so the
 * shadow shape stays identical everywhere; only the intensity differs per
 * surface (denser for modal dialogs, lighter for menus).
 * zh_CN: Dialog、Menu、Popup 等浮层卡片共用此绘制器，保证阴影形状各处一致；
 * 仅强度按表面区分（模态对话框更重，菜单更轻）。
 *
 * @param cardRect Visible card rect the shadow grows from.
 *                 zh_CN: 阴影起始的可见卡片矩形。
 * @param cornerRadius Card corner radius; each layer expands it in step.
 *                     zh_CN: 卡片圆角，各层随扩散同步增大。
 * @param params Theme shadow token (color + base opacity).
 *               zh_CN: 主题阴影 token（颜色与基础透明度）。
 * @param intensity Per-layer opacity multiplier; see defaultShadowIntensity.
 *                  zh_CN: 逐层透明度系数，见 defaultShadowIntensity。
 * @param layerCount Number of expanding shadow layers.
 *                   zh_CN: 向外扩散的阴影层数。
 * @param verticalOffset Downward offset applied to every layer.
 *                       zh_CN: 每层阴影统一向下偏移的像素数。
 */
inline void paintLayeredShadow(QPainter& painter,
                               const QRect& cardRect,
                               qreal cornerRadius,
                               const Elevation::ShadowParams& params,
                               qreal intensity = defaultShadowIntensity,
                               int layerCount = defaultShadowLayerCount,
                               int verticalOffset = defaultShadowVerticalOffset)
{
    const int layers = layerCount > 0 ? layerCount : 1;

    painter.setPen(Qt::NoPen);
    for (int i = 0; i < layers; ++i) {
        const qreal ratio = 1.0 - static_cast<qreal>(i) / layers;
        QColor layerColor = params.color;
        layerColor.setAlphaF(params.opacity * ratio * intensity);
        painter.setBrush(layerColor);
        painter.drawRoundedRect(
            cardRect.adjusted(-i, -i, i, i).translated(0, verticalOffset),
            cornerRadius + i, cornerRadius + i);
    }
}

/**
 * @brief Paints the shared soft shadow around an arbitrary overlay outline.
 * zh_CN: 沿任意浮层轮廓绘制共享的柔和阴影。
 *
 * Use this overload for callouts whose visible surface includes a pointer or
 * another non-rectangular edge. Each layer expands the complete outline, so
 * the shadow remains visible on all sides instead of looking like a vertical
 * stack of duplicated shapes.
 * zh_CN: 适用于包含指示尾或其他非矩形边缘的浮层。每层沿完整轮廓向外扩散，
 * 避免阴影退化成仅向下重复叠印的形状。
 */
inline void paintLayeredShadow(QPainter& painter,
                               const QPainterPath& outline,
                               const Elevation::ShadowParams& params,
                               qreal intensity = defaultShadowIntensity,
                               int layerCount = defaultShadowLayerCount,
                               int verticalOffset = defaultShadowVerticalOffset)
{
    if (outline.isEmpty())
        return;

    const int layers = layerCount > 0 ? layerCount : 1;
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < layers; ++i) {
        const qreal ratio = 1.0 - static_cast<qreal>(i) / layers;
        QColor layerColor = params.color;
        layerColor.setAlphaF(params.opacity * ratio * intensity);
        painter.setBrush(layerColor);

        QPainterPath expanded = outline;
        if (i > 0) {
            QPainterPathStroker stroker;
            stroker.setWidth(i * 2.0);
            stroker.setCapStyle(Qt::RoundCap);
            stroker.setJoinStyle(Qt::RoundJoin);
            expanded = expanded.united(stroker.createStroke(outline));
        }
        painter.drawPath(expanded.translated(0, verticalOffset));
    }
}

} // namespace fluent::overlay

#endif // OVERLAYSHADOW_H
