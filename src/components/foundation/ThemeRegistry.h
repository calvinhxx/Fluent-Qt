#ifndef THEMEREGISTRY_H
#define THEMEREGISTRY_H

#include <QString>
#include <QtGlobal>

#include "FluentElement.h"

namespace fluent {

/**
 * @brief Runtime store for Fluent design tokens used by FluentElement.
 * Initialized with built-in defaults and updated atomically when applications or UserTheme apply overrides.
 * zh_CN: 供 FluentElement 使用的运行时设计 token 存储。初始化为内置默认值，可通过应用代码或 UserTheme 原子更新。
 */
class ThemeRegistry {
public:
    /**
     * @brief Legacy Light/Dark palettes and shared runtime theme state.
     * zh_CN: 保持兼容的 Light/Dark 调色板与共享运行时主题状态。
     */
    struct Snapshot {
        FluentElement::Colors lightColors;
        FluentElement::Colors darkColors;
        FluentElement::Radius radius{0, 4, 8};
        QString fontFamilyOverride;
        qreal fontScale = 1.0;
    };

    /**
     * @brief Extended theme state including the High Contrast palette.
     * Keeps Snapshot as its base member for source compatibility with existing initializers.
     * zh_CN: 包含高对比度调色板的扩展主题状态；保留 Snapshot 成员以兼容既有初始化写法。
     */
    struct ExtendedSnapshot {
        Snapshot base;
        FluentElement::Colors contrastColors;
    };

    static ThemeRegistry& instance();

    /**
     * @brief Returns the compatible Light/Dark and shared portion of the current state.
     * zh_CN: 返回当前状态中兼容的 Light/Dark 与共享部分。
     */
    Snapshot snapshot() const;

    /**
     * @brief Returns the complete three-palette theme state.
     * zh_CN: 返回完整的三调色板主题状态。
     */
    ExtendedSnapshot extendedSnapshot() const;

    /**
     * @brief Returns the built-in Light/Dark and shared state without changing the registry.
     * zh_CN: 返回内置 Light/Dark 与共享状态，不修改注册表。
     */
    static Snapshot defaultSnapshot();

    /**
     * @brief Returns the built-in three-palette theme state.
     * zh_CN: 返回内置三调色板主题状态。
     */
    static ExtendedSnapshot defaultExtendedSnapshot();

    /**
     * @brief Atomically commits the compatible snapshot while preserving High Contrast; returns true if the state changed.
     * zh_CN: 原子提交兼容快照并保留当前高对比度调色板；仅在状态发生变化时返回 true。
     */
    bool applySnapshot(const Snapshot& snapshot);

    /**
     * @brief Atomically commits all three palettes and shared theme tokens; returns true if the state changed.
     * zh_CN: 原子提交三套调色板与共享主题 token；仅在状态发生变化时返回 true。
     */
    bool applyExtendedSnapshot(const ExtendedSnapshot& snapshot);

    /**
     * @brief Returns a Light or Dark palette selected by a boolean (deprecated: use colors(FluentElement::Theme) to support HighContrast).
     * zh_CN: 按布尔值返回 Light 或 Dark 调色板（已废弃，请改用 colors(FluentElement::Theme) 以支持 HighContrast）。
     */
    Q_DECL_DEPRECATED_X("Use colors(FluentElement::Theme)")
    const FluentElement::Colors& colors(bool dark) const { return dark ? m_dark : m_light; }

    const FluentElement::Colors& colors(FluentElement::Theme theme) const;
    FluentElement::Radius radius() const { return {m_radiusNone, m_radiusControl, m_radiusOverlay}; }

    /**
     * @brief Resolves the font configuration for @p role with the active family override and scale applied.
     * zh_CN: 按当前自定义字族与缩放倍率解析指定字号档位的字体配置。
     */
    FluentElement::FontStyle resolvedFontStyle(Typography::FontRole role) const;

    QString fontFamilyOverride() const { return m_fontFamily; }
    qreal fontScale() const { return m_fontScale; }

    /**
     * @brief Updates a Light or Dark palette selected by a boolean (deprecated: use setColors(FluentElement::Theme, const Colors&)).
     * zh_CN: 按布尔值更新 Light 或 Dark 调色板（已废弃，请改用 setColors(FluentElement::Theme, const Colors&) 以支持 HighContrast）。
     */
    Q_DECL_DEPRECATED_X("Use setColors(FluentElement::Theme, const Colors&)")
    void setColors(bool dark, const FluentElement::Colors& colors);

    // Convenience setters; each commits an atomic snapshot update.
    // zh_CN: 便捷设置方法，每次调用均原子更新主题快照。
    void setColors(FluentElement::Theme theme, const FluentElement::Colors& colors);
    void setRadius(int none, int control, int overlay);
    void setFontFamilyOverride(const QString& family);
    void setFontScale(qreal scale);

    /** @brief Restores built-in Fluent defaults. zh_CN: 恢复内置 Fluent 默认值。 */
    void resetToDefaults();

    /** @brief Monotonic counter incremented once per committed snapshot change. zh_CN: 每次快照实际变化时递增一次的单调计数器。 */
    int revision() const { return m_revision; }

private:
    struct UninitializedTag {};

    ThemeRegistry();
    explicit ThemeRegistry(UninitializedTag);
    void seedDefaults();

    FluentElement::Colors m_light;
    FluentElement::Colors m_dark;
    FluentElement::Colors m_contrast;
    int m_radiusNone = 0;
    int m_radiusControl = 4;
    int m_radiusOverlay = 8;
    QString m_fontFamily;    // empty => keep each tier's default family. zh_CN: 空 => 保留各档位默认字族。
    qreal m_fontScale = 1.0; // multiplies every tier's size and line height. zh_CN: 缩放各档位字号与行高。
    int m_revision = 0;
};

} // namespace fluent

#endif // THEMEREGISTRY_H
