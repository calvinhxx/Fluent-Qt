#ifndef THEMEREGISTRY_H
#define THEMEREGISTRY_H

#include <QString>
#include <QtGlobal>

#include "components/foundation/FluentElement.h"

namespace fluent {

/**
 * @brief Runtime store for Fluent design tokens consumed by FluentElement.
 * zh_CN: 供 FluentElement 使用的运行时设计 token 存储。
 *
 * Seeded with built-in Fluent defaults and updated atomically when applications
 * or UserTheme commit custom palettes, corner radii, or font overrides.
 * zh_CN: 初始化为内置 Fluent 默认值，可在运行时通过应用或 UserTheme 原子更新调色板、圆角与字体覆盖。
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
     * zh_CN: 包含高对比度调色板的扩展主题状态。
     *
     * Snapshot remains the original five-member aggregate so existing
     * aggregate initialization and structured bindings stay source compatible.
     * zh_CN: Snapshot 保留原五成员聚合，确保既有聚合初始化和结构化绑定
     * 继续源码兼容。
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
     * @brief Atomically commits the compatible snapshot while preserving High Contrast.
     * zh_CN: 原子提交兼容快照，同时保留当前高对比度调色板。
     *
     * @return true when the committed state changed; false for an identical or invalid snapshot.
     * zh_CN: 状态发生变化时返回 true；快照相同或无效时返回 false。
     */
    bool applySnapshot(const Snapshot& snapshot);

    /**
     * @brief Atomically commits all three palettes and shared theme tokens.
     * zh_CN: 原子提交三套调色板与共享主题 token。
     */
    bool applyExtendedSnapshot(const ExtendedSnapshot& snapshot);

    /**
     * @brief Returns a legacy Light/Dark palette selected by a boolean.
     * zh_CN: 返回由布尔值选择的旧版 Light/Dark 调色板。
     *
     * @deprecated Use colors(FluentElement::Theme) so HighContrast cannot be
     * silently collapsed to the Light palette.
     * zh_CN: 请改用 colors(FluentElement::Theme)，避免 HighContrast 被静默按 Light 处理。
     */
    Q_DECL_DEPRECATED_X("Use colors(FluentElement::Theme)")
    const FluentElement::Colors& colors(bool dark) const { return dark ? m_dark : m_light; }

    const FluentElement::Colors& colors(FluentElement::Theme theme) const;
    FluentElement::Radius radius() const { return {m_radiusNone, m_radiusControl, m_radiusOverlay}; }

    /**
     * @brief Resolves a typography role with the active family override and scale.
     * zh_CN: 使用当前字族覆盖与缩放解析排版角色。
     */
    FluentElement::FontStyle resolvedFontStyle(Typography::FontRole role) const;

    QString fontFamilyOverride() const { return m_fontFamily; }
    qreal fontScale() const { return m_fontScale; }

    /**
     * @brief Updates a legacy Light/Dark palette selected by a boolean.
     * zh_CN: 更新由布尔值选择的旧版 Light/Dark 调色板。
     *
     * @deprecated Use setColors(FluentElement::Theme, const Colors&) so the
     * HighContrast palette remains addressable.
     * zh_CN: 请改用 setColors(FluentElement::Theme, const Colors&)，以便显式
     * 选择 HighContrast 调色板。
     */
    Q_DECL_DEPRECATED_X("Use setColors(FluentElement::Theme, const Colors&)")
    void setColors(bool dark, const FluentElement::Colors& colors);

    // Convenience setters; each commits an atomic snapshot update.
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
    QString m_fontFamily;    // empty => keep each role's default family. zh_CN: 空 => 保留各角色默认字族。
    qreal m_fontScale = 1.0; // multiplies every role's size and line height. zh_CN: 缩放各角色字号与行高。
    int m_revision = 0;
};

} // namespace fluent

#endif // THEMEREGISTRY_H
