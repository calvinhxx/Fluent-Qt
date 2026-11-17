#ifndef FLUENTWINDOWBACKDROPMATERIAL_H
#define FLUENTWINDOWBACKDROPMATERIAL_H

#include <QColor>
#include <QRectF>

#include "components/windowing/WindowBackdrop.h"
#include "design/Material.h"

class QPainter;

namespace fluent::windowing {

/**
 * @brief Options for software-painted Fluent window backdrop effects; always draws an opaque base layer first.
 * zh_CN: 软件绘制的 Fluent 窗口背景参数；绘制时会先铺设不透明底色，即使无系统模糊支持也可安全使用。
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
 * @brief Software renderer for opaque Normal, Mica, and Acrylic window backdrop gradients.
 * zh_CN: 软件绘制的不透明窗口背景效果渲染器，支持纯色、Mica 和 Acrylic 渐变效果。
 */
class WindowBackdropMaterial final {
public:
    /**
     * @brief Renders the Normal, Mica, or Acrylic backdrop gradient in rect.
     * zh_CN: 在 rect 内绘制纯色、Mica 或 Acrylic 背景效果。
     */
    static void paint(QPainter& painter, const QRectF& rect, const WindowBackdropMaterialOptions& options);

    /**
     * @brief Renders the opaque Solid/Normal window background in rect.
     * zh_CN: 在 rect 内绘制不透明的纯色窗口背景。
     */
    static void paintNormal(QPainter& painter, const QRectF& rect, const WindowBackdropMaterialOptions& options);

    /**
     * @brief Renders the software Mica backdrop gradient with subtle color fields.
     * zh_CN: 绘制带柔和色场的软件 Mica 背景效果。
     */
    static void paintMica(QPainter& painter, const QRectF& rect, const WindowBackdropMaterialOptions& options);

    /**
     * @brief Renders the software Acrylic backdrop gradient with luminosity, tint, and grain.
     * zh_CN: 绘制带明度、着色与颗粒层的软件 Acrylic 背景效果。
     */
    static void paintAcrylic(QPainter& painter, const QRectF& rect, const WindowBackdropMaterialOptions& options);

    /**
     * @brief Returns the guaranteed-opaque base color for the selected backdrop effect, excluding gradients and grain.
     * zh_CN: 返回所选背景效果的不透明基础色（不含渐变和颗粒），用于窗口调色板和底层填充初始化。
     */
    static QColor opaqueBaseColor(const WindowBackdropMaterialOptions& options);

private:
    WindowBackdropMaterial() = delete;
};

} // namespace fluent::windowing

#endif // FLUENTWINDOWBACKDROPMATERIAL_H
