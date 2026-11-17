#ifndef ELEVATION_H
#define ELEVATION_H

#include <QtGui/QColor>

/**
 * @brief Defines Fluent shadow elevation levels and paint parameters.
 * Shadow parameters are plain data for QPainter or platform shadows, without rendering logic.
 * zh_CN: 定义 Fluent 阴影层级和绘制参数，仅提供给 QPainter 或平台阴影使用的纯数据，不包含绘制逻辑。
 */
namespace Elevation {

// Semantic elevation levels.
// zh_CN: 阴影层级。
enum Level : int { None = 0, Low = 1, Medium = 2, High = 3, VeryHigh = 4 };

/**
     * @brief Shadow parameters used by control painting code.
     * zh_CN: 控件阴影绘制参数。
     */
struct ShadowParams {
    int offsetX;      // Horizontal offset in pixels. zh_CN: 水平偏移，单位为像素。
    int offsetY;      // Vertical offset in pixels. zh_CN: 垂直偏移，单位为像素。
    int blurRadius;   // Blur radius in pixels. zh_CN: 模糊半径，单位为像素。
    int spreadRadius; // Spread radius in pixels. zh_CN: 扩散半径，单位为像素。
    QColor color;     // Base shadow color before opacity is applied. zh_CN: 应用透明度前的阴影基础色。
    double opacity;   // Final opacity multiplier. zh_CN: 最终透明度倍率。
};

// No elevation is shared by both themes and paints no visible shadow.
// zh_CN: 无阴影层级由两个主题共享，不绘制任何可见阴影。
const ShadowParams NoShadow = {0, 0, 0, 0, QColor(0, 0, 0, 0), 0.0};

// Light-theme shadows.
// zh_CN: Light 主题阴影。
namespace Light {
const ShadowParams Low = {0, 1, 2, 0, QColor(0, 0, 0), 0.10};
const ShadowParams Medium = {0, 2, 4, 0, QColor(0, 0, 0), 0.15};
const ShadowParams High = {0, 4, 8, 0, QColor(0, 0, 0), 0.20};
const ShadowParams VeryHigh = {0, 8, 16, 0, QColor(0, 0, 0), 0.25};
} // namespace Light

// Dark-theme shadows use higher opacity to preserve contrast on dark backgrounds.
// zh_CN: Dark 主题阴影使用更高透明度，以补偿深色背景下的对比度损失。
namespace Dark {
const ShadowParams Low = {0, 1, 2, 0, QColor(0, 0, 0), 0.30};
const ShadowParams Medium = {0, 2, 4, 0, QColor(0, 0, 0), 0.40};
const ShadowParams High = {0, 4, 8, 0, QColor(0, 0, 0), 0.50};
const ShadowParams VeryHigh = {0, 8, 16, 0, QColor(0, 0, 0), 0.60};
} // namespace Dark

/**
     * @brief Returns the shadow parameters for an elevation level and theme.
     * None and unrecognized values return NoShadow.
     * zh_CN: 根据阴影层级和主题返回阴影参数；None 与未识别的值返回 NoShadow。
     */
inline const ShadowParams& getShadow(Level level, bool isDark = false)
{
    if (isDark) {
        switch (level) {
        case None:
            return NoShadow;
        case Low:
            return Dark::Low;
        case Medium:
            return Dark::Medium;
        case High:
            return Dark::High;
        case VeryHigh:
            return Dark::VeryHigh;
        default:
            return NoShadow;
        }
    } else {
        switch (level) {
        case None:
            return NoShadow;
        case Low:
            return Light::Low;
        case Medium:
            return Light::Medium;
        case High:
            return Light::High;
        case VeryHigh:
            return Light::VeryHigh;
        default:
            return NoShadow;
        }
    }
}
} // namespace Elevation

#endif // ELEVATION_H
