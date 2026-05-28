#ifndef ANIMATION_H
#define ANIMATION_H

#include <QEasingCurve>

/**
 * @brief Defines Fluent motion duration and easing tokens.
 * zh_CN: 定义 Fluent 动效时长和缓动曲线 token。
 */
namespace Animation {

    /**
     * @brief Shared motion duration tokens in milliseconds.
     * zh_CN: 通用动效时长 token，单位为毫秒。
     */
    namespace Duration {
        const int Fast     = 150;  // Instant feedback, such as button clicks and small control toggles. zh_CN: 即时反馈，如按钮点击和小控件切换。
        const int Normal   = 250;  // Standard expand/collapse and simple enter/exit transitions. zh_CN: 标准展开/收起和简单进出场。
        const int Slow     = 400;  // Noticeable page, popup, or layout transitions. zh_CN: 页面、弹层或布局等较明显变化。
        const int VerySlow = 700;  // Initial loading of large containers. zh_CN: 大型容器初始加载。
    }

    /**
     * @brief Semantic easing choices used by component animations.
     * zh_CN: 组件动画使用的语义化缓动曲线。
     */
    enum class EasingType {
        Standard,   // InOutSine: smooth start and end. zh_CN: 平滑开始与结束。
        Accelerate, // InCubic: slow start, fast finish. zh_CN: 慢启快收。
        Decelerate, // OutCubic: fast start, slow finish. zh_CN: 快启慢收。
        Entrance,   // OutBack: entrance with subtle overshoot. zh_CN: 带轻微回弹的进入。
        Exit        // InQuint: quick disappearance. zh_CN: 快速消失。
    };

    /**
     * @brief Resolves a semantic easing token to a Qt easing curve.
     * zh_CN: 将语义化缓动 token 转换为 Qt 缓动曲线。
     */
    inline QEasingCurve getEasing(EasingType type) {
        switch (type) {
            case EasingType::Standard:   return QEasingCurve::InOutSine;
            case EasingType::Accelerate: return QEasingCurve::InCubic;
            case EasingType::Decelerate: return QEasingCurve::OutCubic;
            case EasingType::Entrance: {
                QEasingCurve curve(QEasingCurve::OutBack);
                curve.setAmplitude(0.5);
                return curve;
            }
            case EasingType::Exit: return QEasingCurve::InQuint;
            default:               return QEasingCurve::Linear;
        }
    }
}

#endif // ANIMATION_H
