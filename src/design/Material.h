#ifndef MATERIAL_H
#define MATERIAL_H

#include <QColor>

/**
 * @brief Defines Fluent material tokens for Acrylic, Mica, and Smoke surfaces.
 * zh_CN: 定义 Acrylic、Mica 和 Smoke 表面的 Fluent 材质 token。
 *
 * Each material is exposed as plain data. Rendering code decides how to apply
 * the values through QPainter, QSS, QGraphicsEffect, or platform APIs.
 * zh_CN: 每种材质都以纯数据暴露；渲染代码自行决定通过 QPainter、QSS、
 * zh_CN: QGraphicsEffect 或平台 API 使用这些值。
 *
 * Acrylic is a translucent glass material, Mica is an opaque app background
 * material, and Smoke is the modal scrim material.
 * zh_CN: Acrylic 是透明毛玻璃材质，Mica 是非透明应用背景材质，Smoke 是模态遮罩材质。
 */
namespace Material {

    /**
     * @brief Acrylic material parameters.
     * zh_CN: Acrylic 材质参数。
     */
    struct AcrylicToken {
        QColor tintColor;           // Overlay tint color. zh_CN: Tint 叠加色。
        double tintOpacity;         // Tint-layer opacity in [0, 1]. zh_CN: Tint 层透明度，[0, 1]。
        double luminosityOpacity;   // Luminosity-layer opacity in [0, 1]. zh_CN: Luminosity 层透明度，[0, 1]。
        int    blurRadius;          // Background blur radius; implementation-dependent. zh_CN: 背景模糊半径，具体实现依赖平台或效果。
    };

    /**
     * @brief Mica material parameters.
     * zh_CN: Mica 材质参数。
     */
    struct MicaToken {
        QColor baseColor;           // Material base color. zh_CN: 材质基础色。
        double opacity;             // Overall opacity in [0, 1]. zh_CN: 整体透明度，[0, 1]。
    };

    /**
     * @brief Smoke scrim material parameters.
     * zh_CN: Smoke 遮罩材质参数。
     */
    struct SmokeToken {
        QColor baseColor;           // Scrim base color, usually black. zh_CN: 遮罩基础色，通常为黑色。
        double opacity;             // Overall opacity in [0, 1]. zh_CN: 整体透明度，[0, 1]。
    };

    // Acrylic material presets.
    // zh_CN: Acrylic 材质预设。
    namespace Acrylic {
        inline AcrylicToken light() {
            return { QColor(252, 252, 252), 0.60, 0.22, 30 };
        }
        inline AcrylicToken dark() {
            return { QColor(44, 44, 44), 0.65, 0.16, 30 };
        }
        inline AcrylicToken get(bool isDark) {
            return isDark ? dark() : light();
        }
    }

    // Mica material presets.
    // zh_CN: Mica 材质预设。
    namespace Mica {
        inline MicaToken light() {
            return { QColor(243, 242, 241), 0.90 };
        }
        inline MicaToken dark() {
            return { QColor(32, 32, 32), 0.90 };
        }
        inline MicaToken get(bool isDark) {
            return isDark ? dark() : light();
        }
    }

    // Modal smoke/scrim material presets.
    // zh_CN: 模态 Smoke/遮罩材质预设。
    namespace Smoke {
        inline SmokeToken light() {
            return { QColor(0, 0, 0), 0.40 };
        }
        inline SmokeToken dark() {
            return { QColor(0, 0, 0), 0.60 };
        }
        inline SmokeToken get(bool isDark) {
            return isDark ? dark() : light();
        }
    }

    /**
     * @brief Material family identifier for APIs that accept multiple material kinds.
     * zh_CN: 接受多种材质类型的 API 使用的材质族标识。
     */
    enum class Type { Acrylic, Mica, Smoke };
}

#endif // MATERIAL_H
