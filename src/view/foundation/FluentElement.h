#ifndef FLUENTELEMENT_H
#define FLUENTELEMENT_H

#include <QColor>
#include <QFont>
#include <QString>
#include <QEasingCurve>
#include "compatibility/QtCompat.h"
#include "design/Elevation.h"
#include "design/Animation.h"
#include "design/Material.h"

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
        QColor strokeFocusOuter, strokeFocusInner;  // 焦点环双层描边

        // Text Colors
        QColor textPrimary, textSecondary, textTertiary, textDisabled;
        QColor textOnAccent;        // 强调色背景上的文字（白/黑）
        QColor textAccentPrimary;   // 普通背景上的强调色文字（深蓝/亮蓝）

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
        QString styleName;   // Segoe UI Variable 光学尺寸变体，如 "Text Regular"
        int size;
        int weight;
        int lineHeight;      // 绝对行高（px），来自 Figma MCP 实测值
        QFont toQFont() const {
            QFont font(family, -1, weight);
            font.setPixelSize(size);
            if (!styleName.isEmpty())
                font.setStyleName(styleName);
            return font;
        }
    };

    struct Radius {
        int none;     // 0  直角
        int control;  // 4  页面内控件
        int overlay;  // 8  浮层容器（Dialog、Tooltip、Flyout）
    };

    struct Spacing {
        struct {
            int controlH, controlV;
            int card, dialog;
            int textFieldH, textFieldV;   // 输入框内边距
            int listItemH, listItemV;     // 列表项内边距
        } padding;
        struct { int tight, normal, loose, section; } gap;
        struct { int small, standard, large; } controlHeight;  // 标准控件高度
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
     * @brief Returns the currently active global theme.
     * zh_CN: 返回当前生效的全局主题。
     */
    static Theme currentTheme();

    // Component-facing token accessors.
    // zh_CN: 供组件侧访问的设计 token 接口。
    Colors themeColors() const;
    FontStyle themeFont(const QString& styleName = "Body") const;
    Radius themeRadius() const;
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

    FluentElementPrivate* d_ptr; // PImpl 指针

private:
    Q_DISABLE_COPY(FluentElement)
};

#endif // FLUENTELEMENT_H
