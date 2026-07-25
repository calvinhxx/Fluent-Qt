#ifndef THEMEREGISTRY_H
#define THEMEREGISTRY_H

#include <QString>

#include "components/foundation/FluentElement.h"

namespace fluent {

/**
 * @brief Runtime store for the theme-able design tokens consumed by every FluentElement.
 * zh_CN: 供所有 FluentElement 消费的、可在运行时替换的设计 token 存储。
 *
 * Historically FluentElement::themeColors()/themeRadius()/themeFont() read straight from the
 * compile-time tables in design/. That made the look impossible to customize without recompiling.
 * ThemeRegistry inserts a single runtime indirection behind those accessors: it is seeded from the
 * built-in Fluent palette (so default behavior is byte-for-byte unchanged), and the application
 * layer can install a different brand preset (Material 3 / macOS) or user-supplied overrides loaded
 * from a config file. Because every control already funnels through themeColors() etc., installing a
 * new palette repaints the whole UI without touching any control.
 * zh_CN: 过去 themeColors()/themeRadius()/themeFont() 直接读 design/ 的编译期常量,导致不重编无法定制外观。
 * ThemeRegistry 在这些访问器后插入唯一一层运行时间接:默认从内置 Fluent 调色板播种(默认行为逐字节不变),
 * 应用层可安装其它品牌预设(Material 3 / macOS)或从配置文件加载的用户覆盖。由于所有控件都已汇聚到
 * themeColors() 等漏斗,安装新调色板即可在不改任何控件的前提下重绘整个界面。
 */
class ThemeRegistry {
public:
    /**
     * @brief Complete runtime theme state committed as one transaction.
     * zh_CN: 作为单次事务提交的完整运行时主题状态。
     */
    struct Snapshot {
        FluentElement::Colors lightColors;
        FluentElement::Colors darkColors;
        FluentElement::Radius radius{0, 4, 8};
        FluentElement::DesignLanguage designLanguage = FluentElement::DesignFluent;
        QString fontFamilyOverride;
        qreal fontScale = 1.0;
    };

    static ThemeRegistry& instance();

    /**
     * @brief Returns a copy of the currently committed theme state.
     * zh_CN: 返回当前已提交主题状态的副本。
     */
    Snapshot snapshot() const;

    /**
     * @brief Returns the built-in Fluent theme state without changing the registry.
     * zh_CN: 返回内置 Fluent 主题状态，不修改注册表。
     */
    static Snapshot defaultSnapshot();

    /**
     * @brief Atomically commits a complete snapshot and refreshes components once.
     * zh_CN: 原子提交完整快照，并仅刷新组件一次。
     *
     * @return true when the committed state changed; false for an identical or invalid snapshot.
     * zh_CN: 状态发生变化时返回 true；快照相同或无效时返回 false。
     */
    bool applySnapshot(const Snapshot& snapshot);

    // Token reads (called from FluentElement's accessors).
    // zh_CN: token 读取(由 FluentElement 的访问器调用)。
    const FluentElement::Colors& colors(bool dark) const { return dark ? m_dark : m_light; }
    FluentElement::Radius radius() const { return { m_radiusNone, m_radiusControl, m_radiusOverlay }; }
    FluentElement::DesignLanguage designLanguage() const { return m_designLanguage; }
    QString fontFamilyOverride() const { return m_fontFamily; }
    qreal fontScale() const { return m_fontScale; }

    // Convenience token updates. Each changed value is committed through applySnapshot().
    // zh_CN: token 安装(由应用层主题目录调用)。每次都会自增 revision()。
    void setColors(bool dark, const FluentElement::Colors& colors);
    void setRadius(int none, int control, int overlay);
    void setDesignLanguage(FluentElement::DesignLanguage language);
    void setFontFamilyOverride(const QString& family);
    void setFontScale(qreal scale);

    /// Restore the built-in Fluent defaults. zh_CN: 恢复内置 Fluent 默认值。
    void resetToDefaults();

    /// Monotonic counter bumped once per changed snapshot. zh_CN: 每次快照变化仅自增一次。
    int revision() const { return m_revision; }

private:
    struct UninitializedTag {};

    ThemeRegistry();
    explicit ThemeRegistry(UninitializedTag);
    void seedDefaults();

    FluentElement::Colors m_light;
    FluentElement::Colors m_dark;
    int m_radiusNone = 0;
    int m_radiusControl = 4;
    int m_radiusOverlay = 8;
    FluentElement::DesignLanguage m_designLanguage = FluentElement::DesignFluent;
    QString m_fontFamily;     // empty => keep each role's default family. zh_CN: 空 => 保留各角色默认字族。
    qreal m_fontScale = 1.0;  // multiplies every role's size and line height. zh_CN: 缩放各角色字号与行高。
    int m_revision = 0;
};

} // namespace fluent

#endif // THEMEREGISTRY_H
