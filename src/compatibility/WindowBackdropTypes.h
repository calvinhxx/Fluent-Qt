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
enum class BackdropBackend { Solid, PaintedMaterial, DwmSystemBackdrop, MacVibrancy, LinuxCompositor };
Q_ENUM_NS(BackdropBackend)

/**
 * @brief Fidelity of the effective background relative to the requested material.
 * zh_CN: 当前实际背景相对请求材质的保真层级。
 */
enum class BackdropFidelity { Solid, Emulated, Composited, Native };
Q_ENUM_NS(BackdropFidelity)

/**
 * @brief Background painting rule used by widgets sharing the top-level backing store.
 * zh_CN: 共享顶层绘制缓冲的控件所遵循的背景绘制模式。
 */
enum class BackdropSurfaceMode { SolidOpaque, PaintedOpaque, CompositedTransparent };
Q_ENUM_NS(BackdropSurfaceMode)

/**
 * @brief Per-effect capabilities discovered for the current platform session.
 * zh_CN: 当前平台环境探测到的各项背景能力。
 */
struct BackdropCapabilities {
    bool alphaSurfaceSupported = false;
    bool nativeMica = false;
    bool nativeAcrylic = false;
    bool compositorBlur = false;
    QString provider;

    /**
     * @brief Whether a native backend supports this effect directly.
     * zh_CN: 原生后端是否直接支持该效果。
     */
    bool supportsNative(BackdropEffect effect) const;
    /**
     * @brief Whether generic compositor blur-behind can provide this effect (maps to Acrylic; Mica requires native support or software fallback).
     * zh_CN: 通用合成器背景模糊是否可提供该效果（仅用于 Acrylic；Mica 需原生支持或软件材质回退）。
     */
    bool supportsCompositor(BackdropEffect effect) const;
    /**
     * @brief Whether this effect may use a transparent material surface.
     * zh_CN: 该效果是否可使用透明材质表面。
     */
    bool supportsTransparentMaterial(BackdropEffect effect) const;
};

/**
 * @brief Result returned by a platform backend after applying a background effect.
 * zh_CN: 平台后端应用背景效果后的返回结果。
 */
struct BackdropApplyResult {
    bool applied = false;
    BackdropBackend backend = BackdropBackend::Solid;
    BackdropFidelity fidelity = BackdropFidelity::Solid;
    BackdropSurfaceMode surfaceMode = BackdropSurfaceMode::SolidOpaque;
    QString reason;
};

/**
 * @brief Requested and effective backdrop state published by a Fluent window.
 * zh_CN: Fluent 窗口上记录的请求背景效果与实际生效状态。
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
