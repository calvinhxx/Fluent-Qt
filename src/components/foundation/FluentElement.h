#ifndef FLUENTELEMENT_H
#define FLUENTELEMENT_H

#include <QColor>
#include <QFont>
#include <QString>
#include <QEasingCurve>
#include "compatibility/FontCompat.h"
#include "compatibility/QtCompat.h"
#include "design/Elevation.h"
#include "design/Animation.h"
#include "design/Material.h"

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
    enum Theme { Light, Dark };

    /**
     * @brief Shape/interaction language a brand style theme paints in.
     * zh_CN: 品牌样式主题所采用的形状/交互设计语言。
     *
     * Colors and radius are data (ThemeRegistry), but some fidelity is structural — Material 3 filled
     * buttons are stadium-shaped with a translucent state layer; macOS push buttons use a vertical
     * bezel gradient. Controls read this to branch their paint beyond what a palette swap can express.
     * Defaults to DesignFluent so nothing changes unless a brand preset opts in.
     * zh_CN: 颜色与圆角是数据(ThemeRegistry),但部分保真是结构性的——Material 3 填充按钮为胶囊形并带半透明
     * state layer;macOS 按钮用纵向斜面渐变。控件据此在调色板替换之外分支绘制。默认 DesignFluent,品牌预设不
     * 选用则一切不变。
     */
    enum DesignLanguage { DesignFluent, DesignMaterial, DesignCupertino };

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
        QColor strokeFocusOuter, strokeFocusInner;  // Two-layer focus ring strokes. zh_CN: 焦点环双层描边。

        // Text Colors
        QColor textPrimary, textSecondary, textTertiary, textDisabled;
        QColor textOnAccent;        // Text on accent backgrounds (white/black). zh_CN: 强调色背景上的文字。
        QColor textAccentPrimary;   // Accent text on plain backgrounds (dark/light blue). zh_CN: 普通背景上的强调色文字。

        // Backgrounds & Neutrals
        QColor bgCanvas, bgLayer, bgLayerAlt, bgSolid;
        QColor grey10, grey20, grey30, grey40, grey50, grey60, grey90, grey130, grey160, grey190;

        // System / Semantic Colors
        QColor systemCritical,    systemCriticalBg;
        QColor systemCaution,     systemCautionBg;
        QColor systemInfo,        systemInfoBg;
        QColor systemSuccess,     systemSuccessBg;

        // Charts
        QList<QColor> charts;
    };

    struct FontStyle {
        QString family;
        QString styleName;   // Exact static face, e.g. "Regular" or "Semibold". zh_CN: 精确静态字体。
        int size;
        int weight;
        int lineHeight;      // Absolute line height in px, measured from Figma. zh_CN: 绝对行高，Figma 实测值。
        QFont toQFont() const {
            QFont font(family, -1, weight);
            font.setPixelSize(size);
            fluentApplyFontStyleName(font, styleName);
            return font;
        }
    };

    struct Radius {
        int none;     // 0: square corners. zh_CN: 直角。
        int control;  // 4: in-page controls. zh_CN: 页面内控件。
        int overlay;  // 8: overlay containers (Dialog, ToolTip, Flyout). zh_CN: 浮层容器。
    };

    struct Spacing {
        struct {
            int controlH, controlV;
            int card, dialog;
            int textFieldH, textFieldV;   // Text-field padding. zh_CN: 输入框内边距。
            int listItemH, listItemV;     // List-item padding. zh_CN: 列表项内边距。
        } padding;
        struct { int tight, normal, loose, section; } gap;
        struct { int small, standard, large; } controlHeight;  // Standard control heights. zh_CN: 标准控件高度。
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
     * @brief Re-broadcasts the current theme to repaint after a design-token change.
     * zh_CN: 在设计 token 变化后重新广播当前主题以触发重绘。
     *
     * setThemeDeferred() early-outs when the Light/Dark mode is unchanged, so it cannot repaint a
     * pure palette swap (e.g. switching the brand style theme via ThemeRegistry while staying in the
     * same mode). This forces the same atomic visible-first refresh without changing the mode.
     * zh_CN: setThemeDeferred() 在明暗模式未变时会提前返回,无法重绘纯调色板替换(如保持同一模式下经
     * ThemeRegistry 切换品牌主题)。此入口在不改模式的前提下,强制同样的「可见优先原子刷新」。
     */
    static void refreshTheme();

    /**
     * @brief Monotonic counter bumped on every theme change, for staleness checks.
     * zh_CN: 每次主题变化自增的单调计数器，用于过期判断。
     *
     * A subtree hidden during a theme change is themed lazily; comparing the generation it last
     * refreshed against this lets a host (e.g. a stacked page) refresh it exactly once on show.
     * zh_CN: 切换期间隐藏的子树会延后刷新；用它上次刷新的代次与此比较，宿主（如分页栈中的页）可在显示时恰好刷新一次。
     */
    static int themeGeneration();

    /**
     * @brief Resolves the theme used by this element, honoring an inherited QWidget override.
     * zh_CN: 解析此元素实际使用的主题，会优先读取 QWidget 父链上的局部主题覆盖。
     *
     * Gallery samples can set the dynamic property `fluentThemeOverride` on a container
     * (value: `Light`/`Dark` enum integer or string) so only that subtree renders in the
     * requested theme while the application chrome keeps the global theme.
     * zh_CN: Gallery 示例可在容器上设置 `fluentThemeOverride` 动态属性（值为 `Light`/`Dark`
     * 枚举整数或字符串），从而只让该子树使用指定主题，应用外壳仍保持全局主题。
     */
    Theme effectiveTheme() const;

    // Component-facing token accessors.
    // zh_CN: 供组件侧访问的设计 token 接口。
    Colors themeColors() const;

    /**
     * @brief Non-copying access to the active color set (for hot paint paths).
     * zh_CN: 对当前色板的零拷贝访问（用于绘制热路径）。
     *
     * themeColors() returns the full ~50-QColor Colors struct BY VALUE; a delegate or tab strip that
     * calls it per item/tab/frame pays that copy (plus the QList<QColor> charts refcount) every time.
     * This returns the ThemeRegistry's own const reference instead — identical data, no copy. The
     * reference is owned by the registry singleton and stays valid until the next theme/registry change,
     * so use it within a single paint() and do NOT cache it across a theme switch.
     * zh_CN: themeColors() 按值返回整个 ~50 个 QColor 的结构体;在每项/每帧绘制里调用会反复付出该拷贝
     *（外加 charts QList 引用计数)。此方法改为返回 ThemeRegistry 自有的 const 引用——数据相同、零拷贝。
     * 该引用归注册表单例所有,在下次主题/注册表变化前一直有效,故应在单次 paint() 内使用,切勿跨主题切换缓存。
     */
    const Colors& themeColorsRef() const;

    FontStyle themeFont(const QString& styleName = "Body") const;
    Radius themeRadius() const;

    /**
     * @brief Active shape/interaction design language (from the installed brand style theme).
     * zh_CN: 当前生效的形状/交互设计语言(来自已安装的品牌样式主题)。
     */
    DesignLanguage themeDesignLanguage() const;
    Spacing themeSpacing() const;
    Animation themeAnimation() const;
    
    // Material, elevation, and adaptive layout tokens.
    // zh_CN: 材质、阴影和自适应布局 token。
    Material::AcrylicToken themeAcrylic() const;
    Material::MicaToken    themeMica()    const;
    Material::SmokeToken   themeSmoke()   const;
    Elevation::ShadowParams themeShadow(Elevation::Level level) const;
    int themeBreakpoint(const QString& size = "Medium") const;

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
     * @brief The backdrop fill a chrome surface (title bar, nav pane) should paint, given its host
     * window and activation state. zh_CN: 给定宿主窗口与激活状态，chrome 表面（标题栏、导航栏）应填充的背景色。
     *
     * Single source of truth for the chrome-backdrop decision that used to be duplicated across the
     * title bar and nav-pane paints. When the host window carries a real OS-composited backdrop
     * (Windows DWM/Acrylic or macOS vibrancy) the surface must let it show through, so this returns
     * an **invalid QColor** — the caller's contract is "erase to transparent". Otherwise it returns
     * the opaque app-painted fallback for the requested effect; Normal maps to themeBackdrop().
     * zh_CN: chrome 背景决策的单一真相源（原先散落在标题栏与导航栏的绘制里）。宿主窗口带真实系统合成背景
     *（Windows DWM/Acrylic 或 macOS vibrancy）时，表面须透出该背景，故返回**无效 QColor**——调用方据此擦成透明；
     * 否则返回该激活状态下的纯色 themeBackdrop()（跨平台回退）。
     */
    QColor chromeBackdropFill(const QWidget* hostWindow, bool active) const;

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

    FluentElementPrivate* d_ptr; // PImpl pointer. zh_CN: PImpl 指针。

private:
    Q_DISABLE_COPY(FluentElement)
};

} // namespace fluent

#endif // FLUENTELEMENT_H
