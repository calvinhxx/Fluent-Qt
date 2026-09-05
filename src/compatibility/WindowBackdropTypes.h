#ifndef FLUENTWINDOWBACKDROPTYPES_H
#define FLUENTWINDOWBACKDROPTYPES_H

#include <QMetaType>
#include <QString>

namespace fluent::windowing {

Q_NAMESPACE

/**
 * @brief Logical background material requested for a Fluent window.
 * zh_CN: Fluent 窗口请求的逻辑背景材质。
 */
enum class BackdropEffect { Solid, Mica, Acrylic };
Q_ENUM_NS(BackdropEffect)

/**
 * @brief Backend that currently supplies the visible window background.
 * zh_CN: 当前为窗口提供可见背景的后端。
 */
enum class BackdropBackend {
    Solid,
    PaintedMaterial,
    DwmSystemBackdrop,
    MacVibrancy,
    LinuxCompositor
};
Q_ENUM_NS(BackdropBackend)

/**
 * @brief Fidelity of the effective background relative to the requested material.
 * zh_CN: 当前实际背景相对请求材质的保真层级。
 */
enum class BackdropFidelity { Solid, Emulated, Composited, Native };
Q_ENUM_NS(BackdropFidelity)

/**
 * @brief Painting contract used by widgets sharing the top-level backing store.
 * zh_CN: 共享顶层后备缓冲的控件所使用的绘制契约。
 */
enum class BackdropSurfaceMode { SolidOpaque, PaintedOpaque, CompositedTransparent };
Q_ENUM_NS(BackdropSurfaceMode)

/**
 * @brief Per-effect capabilities discovered for the current platform session.
 * zh_CN: 当前平台会话探测到的逐效果能力。
 */
struct BackdropCapabilities {
    bool alphaSurfaceSupported = false;
    bool nativeMica = false;
    bool nativeAcrylic = false;
    bool compositorBlur = false;
    QString provider;

    /**
     * @brief Whether a native backend represents this effect.
     * zh_CN: 原生后端是否支持该效果。
     */
    bool supportsNative(BackdropEffect effect) const;
    /**
     * @brief Whether generic compositor blur can represent this effect.
     * zh_CN: 通用合成器模糊是否可表示该效果。
     *
     * Blur-behind maps to Acrylic. Mica requires a native Mica provider or the
     * stable UILib-painted material fallback.
     * zh_CN: 背景模糊映射为 Acrylic；Mica 需要原生 Mica 后端或稳定的
     * UILib 软件材质回退。
     */
    bool supportsCompositor(BackdropEffect effect) const;
    /**
     * @brief Whether this effect may use a transparent material surface.
     * zh_CN: 该效果是否可使用透明材质表面。
     */
    bool supportsTransparentMaterial(BackdropEffect effect) const;
};

/**
 * @brief Result of asking a platform backend to apply one background effect.
 * zh_CN: 请求平台后端施加某一背景效果后的结果。
 */
struct BackdropApplyResult {
    bool applied = false;
    BackdropBackend backend = BackdropBackend::Solid;
    BackdropFidelity fidelity = BackdropFidelity::Solid;
    BackdropSurfaceMode surfaceMode = BackdropSurfaceMode::SolidOpaque;
    QString reason;
};

/**
 * @brief Requested and effective state published by a Fluent window.
 * zh_CN: Fluent 窗口发布的请求状态与实际状态。
 */
struct BackdropState {
    BackdropEffect requestedEffect = BackdropEffect::Solid;
    BackdropEffect effectiveEffect = BackdropEffect::Solid;
    BackdropBackend backend = BackdropBackend::Solid;
    BackdropFidelity fidelity = BackdropFidelity::Solid;
    BackdropSurfaceMode surfaceMode = BackdropSurfaceMode::SolidOpaque;
    bool platformApplied = false;
    QString reason;

    bool operator==(const BackdropState& other) const;
    bool operator!=(const BackdropState& other) const { return !(*this == other); }
};

} // namespace fluent::windowing

Q_DECLARE_METATYPE(fluent::windowing::BackdropEffect)
Q_DECLARE_METATYPE(fluent::windowing::BackdropBackend)
Q_DECLARE_METATYPE(fluent::windowing::BackdropFidelity)
Q_DECLARE_METATYPE(fluent::windowing::BackdropSurfaceMode)
Q_DECLARE_METATYPE(fluent::windowing::BackdropState)

#endif // FLUENTWINDOWBACKDROPTYPES_H
