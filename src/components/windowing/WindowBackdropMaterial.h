#ifndef FLUENTWINDOWBACKDROPMATERIAL_H
#define FLUENTWINDOWBACKDROPMATERIAL_H

#include <QColor>
#include <QRectF>

#include "components/windowing/WindowBackdrop.h"
#include "design/Material.h"

class QPainter;

namespace fluent::windowing {

/**
 * @brief Options for the software-painted Fluent window backdrop; always establishes an opaque base layer so it is safe without compositor blur.
 * zh_CN: 软件绘制的 Fluent 窗口背景参数；渲染时会先铺设不透明底色，即使无系统模糊支持也可安全使用。
 */
struct WindowBackdropMaterialOptions {
    BackdropEffect effect = BackdropEffect::Solid;
    QColor normalColor;
    QColor accentColor;
    Material::MicaToken mica = Material::Mica::light();
    Material::AcrylicToken acrylic = Material::Acrylic::light();
    bool dark = false;
    bool active = true;

    /**
     * @brief Device-pixel ratio for the cached grain tile; values <= 0 use the painter target's DPR.
     * zh_CN: 颗粒纹理使用的设备像素比；小于等于 0 时自动取绘制目标的 DPR。
     */
    qreal devicePixelRatio = 0.0;

    /**
     * @brief Creates options initialized with the built-in Mica and Acrylic tokens.
     * zh_CN: 使用内置 Mica 和 Acrylic token 初始化绘制参数。
     */
    static WindowBackdropMaterialOptions
    forTheme(bool dark, const QColor& normalColor, const QColor& accentColor = QColor());
};

/**
 * @brief Paints deterministic, opaque software fallbacks for Fluent window materials without screen capture or CPU blur.
 * zh_CN: 绘制确定性的不透明 Fluent 窗口材质（不截屏也不做 CPU 模糊），通过底色、低频色场与缓存颗粒纹理区分 Mica 和 Acrylic。
 */
class WindowBackdropMaterial final {
public:
    /**
     * @brief Paints the selected Normal, Mica, or Acrylic fallback into rect.
     * zh_CN: 在 rect 内绘制所选的 Normal、Mica 或 Acrylic 回退材质。
     */
    static void paint(QPainter& painter, const QRectF& rect, const WindowBackdropMaterialOptions& options);

    /**
     * @brief Paints the opaque Solid/Normal window background.
     * zh_CN: 绘制不透明的纯色窗口背景。
     */
    static void paintNormal(QPainter& painter, const QRectF& rect, const WindowBackdropMaterialOptions& options);

    /**
     * @brief Paints the opaque Mica fallback with subtle color fields.
     * zh_CN: 绘制带柔和色场的不透明 Mica 回退材质。
     */
    static void paintMica(QPainter& painter, const QRectF& rect, const WindowBackdropMaterialOptions& options);

    /**
     * @brief Paints the opaque Acrylic fallback with luminosity, tint, and grain.
     * zh_CN: 绘制带明度、着色与颗粒层的不透明 Acrylic 回退材质。
     */
    static void paintAcrylic(QPainter& painter, const QRectF& rect, const WindowBackdropMaterialOptions& options);

    /**
     * @brief Returns the guaranteed-opaque base color for the selected material, excluding gradients and grain.
     * zh_CN: 返回所选材质的不透明基础色（不含渐变和颗粒），用于窗口调色板和底层填充初始化。
     */
    static QColor opaqueBaseColor(const WindowBackdropMaterialOptions& options);

private:
    WindowBackdropMaterial() = delete;
};

} // namespace fluent::windowing

#endif // FLUENTWINDOWBACKDROPMATERIAL_H
