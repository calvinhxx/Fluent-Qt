#ifndef FLUENTWINDOWBACKDROPMATERIAL_H
#define FLUENTWINDOWBACKDROPMATERIAL_H

#include <QColor>
#include <QRectF>

#include "components/windowing/WindowBackdrop.h"
#include "design/Material.h"

class QPainter;

namespace fluent::windowing {

/**
 * @brief Parameters for the app-painted Fluent window backdrop.
 * zh_CN: Fluent 窗口应用侧绘制背景所使用的参数。
 *
 * The material renderer always establishes an opaque base before applying its
 * decorative layers. It is therefore safe to use when the platform cannot
 * provide a blurred or translucent compositor surface.
 * zh_CN: 材质渲染器始终先建立不透明底层，再叠加装饰层；因此在平台无法提供
 * 模糊或半透明合成表面时也可安全使用。
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
     * Device-pixel ratio used by the cached grain texture. Values less than or
     * equal to zero are resolved from the painter's target device.
     * zh_CN: 缓存颗粒纹理使用的设备像素比；小于等于零时从绘制目标自动解析。
     */
    qreal devicePixelRatio = 0.0;

    /**
     * @brief Creates options populated with the built-in Mica and Acrylic tokens.
     * zh_CN: 创建已填充内置 Mica 与 Acrylic token 的参数。
     */
    static WindowBackdropMaterialOptions forTheme(bool dark,
                                                   const QColor& normalColor,
                                                   const QColor& accentColor = QColor());
};

/**
 * @brief Paints deterministic, opaque Fluent window-material fallbacks.
 * zh_CN: 绘制确定性且最终不透明的 Fluent 窗口材质回退效果。
 *
 * The renderer performs no screen capture or CPU blur. Mica and Acrylic are
 * distinguished with token-driven opaque bases, low-frequency color fields,
 * luminosity/tint layers, and a small cached grain tile.
 * zh_CN: 渲染器不抓屏，也不执行 CPU 模糊；Mica 与 Acrylic 通过 token 驱动的
 * 不透明底色、低频色场、明度/tint 层以及小型缓存颗粒纹理形成区分。
 */
class WindowBackdropMaterial final {
public:
    /**
     * @brief Paints the selected Normal, Mica, or Acrylic fallback into rect.
     * zh_CN: 在 rect 内绘制所选的 Normal、Mica 或 Acrylic 回退材质。
     */
    static void paint(QPainter& painter,
                      const QRectF& rect,
                      const WindowBackdropMaterialOptions& options);

    /**
     * @brief Paints the opaque Normal/Solid window surface.
     * zh_CN: 绘制不透明的 Normal/Solid 窗口表面。
     */
    static void paintNormal(QPainter& painter,
                            const QRectF& rect,
                            const WindowBackdropMaterialOptions& options);

    /**
     * @brief Paints the opaque, softly personalized Mica fallback.
     * zh_CN: 绘制不透明、带轻微个性化色场的 Mica 回退材质。
     */
    static void paintMica(QPainter& painter,
                          const QRectF& rect,
                          const WindowBackdropMaterialOptions& options);

    /**
     * @brief Paints the opaque Acrylic fallback with luminosity, tint, and grain.
     * zh_CN: 绘制带明度、tint 与颗粒层的不透明 Acrylic 回退材质。
     */
    static void paintAcrylic(QPainter& painter,
                             const QRectF& rect,
                             const WindowBackdropMaterialOptions& options);

    /**
     * @brief Returns the guaranteed-opaque base color for the selected material.
     * zh_CN: 返回所选材质保证不透明的基础颜色。
     *
     * This excludes gradients and grain and is useful for palette or backing-
     * store initialization that must agree with the material painter.
     * zh_CN: 返回值不包含渐变与颗粒，可用于需要与材质绘制器保持一致的调色板
     * 或 backing-store 初始化。
     */
    static QColor opaqueBaseColor(const WindowBackdropMaterialOptions& options);

private:
    WindowBackdropMaterial() = delete;
};

} // namespace fluent::windowing

#endif // FLUENTWINDOWBACKDROPMATERIAL_H
