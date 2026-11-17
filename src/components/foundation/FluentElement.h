#ifndef FLUENTELEMENT_H
#define FLUENTELEMENT_H

#include "compatibility/InputEventCompat.h"
#include <QColor>
#include <QFont>
#include <QJsonObject>
#include <QString>
#include <QEasingCurve>
#include "compatibility/FontCompat.h"
#include "design/Elevation.h"
#include "design/Animation.h"
#include "design/Breakpoints.h"
#include "design/Material.h"
#include "design/Typography.h"

class QWidget;

namespace fluent {

class FluentElementPrivate;

/**
 * @brief Provides shared Fluent design tokens and theme callbacks to QWidget-based components.
 * zh_CN: 为基于 QWidget 的组件提供共享 Fluent 设计 token 和主题更新回调。
 *
 * FluentElement is used as a mixin by reusable controls. It keeps token access
 * centralized while hiding global theme bookkeeping behind PImpl.
 * zh_CN: FluentElement 作为 mixin 被组件继承，用于集中访问主题色、字体、圆角、
 * 间距、动效、材质和阴影等设计元素，并通过 PImpl 隐藏全局主题管理细节。
 */
class FluentElement {
public:
    /**
     * @brief Global visual theme used when resolving design tokens.
     * zh_CN: 解析设计 token 时使用的全局视觉主题。
     */
    enum Theme { Light = 0, Dark = 1, HighContrast = 2 };

    // Design token snapshots returned to components.
    // zh_CN: 返回给组件使用的设计 token 快照。

    struct Colors {
        // Fill Colors
        QColor accentDefault, accentSecondary, accentTertiary, accentDisabled;
        QColor controlDefault, controlSecondary, controlTertiary, controlDisabled;
        QColor controlAltSecondary, controlAltTertiary;
        QColor subtleTransparent, subtleSecondary, subtleTertiary;

        // Stroke Colors
        QColor strokeDefault, strokeSecondary, strokeStrong;
        QColor strokeCard, strokeDivider, strokeSurface;
        QColor strokeFocusOuter,
            strokeFocusInner; // Two-layer focus ring strokes. zh_CN: 焦点环双层描边。

        // Text Colors
        QColor textPrimary, textSecondary, textTertiary, textDisabled;
        QColor textOnAccent;      // Text on accent backgrounds (white/black). zh_CN: 强调色背景上的文字。
        QColor textAccentPrimary; // Accent text on plain backgrounds (dark/light blue). zh_CN: 普通背景上的强调色文字。

        // Backgrounds & Neutrals
        QColor bgCanvas, bgLayer, bgLayerAlt, bgSolid;
        QColor grey10, grey20, grey30, grey40, grey50, grey60, grey90, grey130, grey160, grey190;

        // System / Semantic Colors
        QColor systemCritical, systemCriticalBg;
        QColor systemCaution, systemCautionBg;
        QColor systemInfo, systemInfoBg;
        QColor systemSuccess, systemSuccessBg;

        // Charts
        QList<QColor> charts;

        // Overlay / Translucent Layers
        QColor bgLayerOverlay; // layered content mask for translucent windows. zh_CN: 半透明窗口下的分层内容遮罩。
    };

    struct FontStyle {
        QString family;
        QString styleName; // Exact static face, e.g. "Regular" or "Semibold". zh_CN: 精确静态字体。
        int size;
        int weight;
        int lineHeight; // Absolute line height in px, measured from Figma. zh_CN: 绝对行高，Figma 实测值。
        QFont toQFont() const
        {
            QFont font(family, -1, weight);
            font.setPixelSize(size);
            fluentApplyFontStyleName(font, styleName);
            fluentConfigureTextRendering(font);
            return font;
        }
    };

    struct Radius {
        int none;    // 0: square corners. zh_CN: 直角。
        int control; // 4: in-page controls. zh_CN: 页面内控件。
        int overlay; // 8: overlay containers (Dialog, ToolTip, Flyout). zh_CN: 浮层容器。
    };

    struct Spacing {
        struct {
            int controlH, controlV;
            int card, dialog;
            int textFieldH, textFieldV; // Text-field padding. zh_CN: 输入框内边距。
            int listItemH, listItemV;   // List-item padding. zh_CN: 列表项内边距。
        } padding;
        struct {
            int tight, normal, loose, section;
        } gap;
        struct {
            int small, standard, large;
        } controlHeight; // Standard control heights. zh_CN: 标准控件高度。
        int xSmall, small, medium, standard, large, xLarge, xxLarge;
    };

    struct Animation {
        // Durations (ms)
        int fast, normal, slow, verySlow;
        // Easings
        QEasingCurve standard, accelerate, decelerate, entrance, exit;
    };

    /**
     * @brief Updates the global theme and notifies active FluentElement instances.
     * zh_CN: 更新全局主题，并通知所有活跃的 FluentElement 实例。
     */
    static void setTheme(Theme theme);

    /**
     * @brief Schedules a visible-first batched theme refresh without blocking the current interaction.
     * zh_CN: 以可见元素优先的批次调度主题刷新，避免阻塞当前交互。
     */
    static void setThemeDeferred(Theme theme);

    /**
     * @brief Returns the currently active global theme.
     * zh_CN: 返回当前生效的全局主题。
     */
    static Theme currentTheme();

    /**
     * @brief Returns whether a theme uses dark-backed chrome and material tokens.
     * zh_CN: 返回指定主题是否使用暗色底的窗口与材质 token。
     *
     * HighContrast is a distinct semantic mode whose built-in palette uses a black
     * canvas with light foregrounds. Callers choosing between light and dark chrome
     * should use this helper instead of comparing only against Dark.
     * zh_CN: HighContrast 是独立语义模式，其内置调色板采用黑底亮色前景。
     * 需要区分浅色与深色样式的调用方应使用此方法，而非仅判断是否等于 Dark。
     */
    static bool themeUsesDarkAppearance(Theme theme);

    /**
     * @brief Re-broadcasts the current theme to repaint after a design-token change.
     * zh_CN: 在设计 token 变化后重新广播当前主题以触发重绘。
     *
     * setThemeDeferred() returns early when the theme mode is unchanged, so it cannot
     * repaint a token update within the same mode. This forces the same visible-first
     * batched refresh without switching modes.
     * zh_CN: setThemeDeferred() 在明暗模式未变时会直接返回，无法触发同模式下的 token 重绘；
     * 此方法在不切换模式的前提下强制执行可见优先的批量刷新。
     */
    static void refreshTheme();

    /**
     * @brief Monotonic counter bumped on every theme change, for staleness checks.
     * zh_CN: 每次主题变化自增的单调版本号，用于过期判断。
     *
     * A subtree hidden during a theme change is refreshed lazily; comparing its last
     * refreshed generation against this lets a container (e.g. a stacked page) refresh once on show.
     * zh_CN: 主题切换期间处于隐藏状态的子树会延迟刷新；容器（如分页栈页面）可比较页面上次刷新的版本号与当前值，
     * 在页面显示时按需刷新一次。
     */
    static int themeGeneration();

    /**
     * @brief Resolves the theme used by this element, honoring an inherited QWidget override.
     * zh_CN: 解析此元素实际使用的主题，会优先读取 QWidget 父链上的局部主题覆盖。
     *
     * Gallery samples can set the dynamic property `fluentThemeOverride` on a container
     * (value: `Light`/`Dark`/`HighContrast` enum integer or string) so only that subtree renders in the
     * requested theme while the application chrome keeps the global theme.
     * zh_CN: Gallery 示例可在容器上设置 `fluentThemeOverride` 动态属性（值为 `Light`/`Dark`/`HighContrast`
     * 枚举整数或字符串），从而只让该子树使用指定主题，应用外壳仍保持全局主题。
     */
    Theme effectiveTheme() const;

    /**
     * @brief Returns whether this element's effective theme uses dark-backed chrome.
     * zh_CN: 返回此元素的实际主题是否使用暗色底 chrome。
     */
    bool effectiveThemeUsesDarkAppearance() const;

    // Component-facing token accessors.
    // zh_CN: 供组件侧访问的设计 token 接口。
    /**
     * @brief Replaces this element's sparse color, radius, and font token overrides.
     * zh_CN: 替换当前元素的颜色、圆角和字体 token 局部覆盖。
     *
     * Unspecified fields follow the global theme, and overrides survive theme changes.
     * Applies only to this element (not inherited by children); an empty object clears
     * overrides, and any invalid key or value rejects the entire update.
     * zh_CN: 未指定字段跟随全局主题，覆盖设置会在主题切换后保留；仅作用于当前元素，
     * 不向子控件继承。传入空对象会清除覆盖；包含无效键或值时拒绝整次更新并保持原状态。
     *
     * @return true if the override object changed; false for invalid or identical input.
     * zh_CN: 覆盖配置发生变更时返回 true；输入无效或与当前相同时返回 false。
     */
    bool setThemeOverrides(const QJsonObject& overrides);

    /** @brief Returns this element's authored overrides. zh_CN: 返回当前元素的覆盖配置。 */
    QJsonObject themeOverrides() const;

    /** @brief Restores all token fields to global theme defaults. zh_CN: 恢复全部 token 跟随全局主题。 */
    void clearThemeOverrides();

    Colors themeColors() const;

    /**
     * @brief Non-copying access to the active color set for hot paint paths.
     * zh_CN: 对当前色板的零拷贝访问（适用于绘制热路径）。
     *
     * Unlike themeColors() which returns a full copy of Colors, this returns a cached
     * const reference owned by ThemeRegistry or this element. It stays valid until the
     * next token change; use within a single paint pass and do not cache across theme switches.
     * zh_CN: 相比按值拷贝整个 Colors 结构体的 themeColors()，此方法返回缓存的 const 引用
     * （由 ThemeRegistry 或当前元素持有），在下次 token 变化前有效；请在单次绘制内使用，不要跨主题切换缓存。
     */
    const Colors& themeColorsRef() const;

    FontStyle themeFont(Typography::FontRole role = Typography::FontRole::Body) const;
    Radius themeRadius() const;

    Spacing themeSpacing() const;
    Animation themeAnimation() const;

    // Material, elevation, and adaptive layout tokens.
    // zh_CN: 材质、阴影和自适应布局 token。
    Material::AcrylicToken themeAcrylic() const;
    Material::MicaToken themeMica() const;
    Material::SmokeToken themeSmoke() const;
    Elevation::ShadowParams themeShadow(Elevation::Level level) const;
    int themeBreakpoint(Breakpoints::Breakpoint breakpoint = Breakpoints::Breakpoint::Medium) const;

    /**
     * @brief Window-chrome backdrop color for the given activation state.
     * zh_CN: 给定激活状态下的窗口 chrome 背景色。
     *
     * Active windows use the canvas tint (Mica-like); inactive windows ease that
     * toward the content layer, so the title bar and nav pane visibly dim when the
     * window loses focus — matching the WinUI Mica active/inactive fallback.
     * zh_CN: 激活窗口用 canvas 色调（类 Mica）；非激活窗口将其向内容层过渡，
     * 使标题栏与导航栏在窗口失焦时明显变淡——对齐 WinUI Mica 的激活/非激活回退。
     */
    QColor themeBackdrop(bool active) const;

    /**
     * @brief Called after the global theme changes.
     * zh_CN: 全局主题变化后调用。
     *
     * Components override this hook to refresh cached fonts, colors, geometry,
     * or style state derived from Fluent design tokens.
     * zh_CN: 组件可重写此回调，用于刷新来自 Fluent 设计 token 的字体、颜色、
     * 几何或样式缓存。
     */
    virtual void onThemeUpdated() {}

protected:
    FluentElement();
    virtual ~FluentElement();

    FluentElementPrivate* d_ptr; // Reserved for 1.x source compatibility. zh_CN: 为 1.x 源码兼容保留。

private:
    void resolveThemeOverrides() const;
    Q_DISABLE_COPY(FluentElement)
};

} // namespace fluent

#endif // FLUENTELEMENT_H
