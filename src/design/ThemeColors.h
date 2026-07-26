#ifndef THEMECOLORS_H
#define THEMECOLORS_H

#include <QColor>
#include <vector>

/**
 * @brief Defines Fluent semantic color tokens for light, dark, and contrast themes.
 * zh_CN: 定义 Light、Dark 和 Contrast 主题下的 Fluent 语义化颜色 token。
 *
 * Key accent colors:
 * - Light: #005FB8
 * - Dark:  #60CDFF
 * zh_CN: 关键 Accent 色值为 Light #005FB8、Dark #60CDFF。
 *
 * System colors include foreground/background pairs for Critical, Caution,
 * Informational, and Success states.
 * zh_CN: System 颜色包含 Critical、Caution、Informational 和 Success 的前景/背景配对。
 */
namespace ThemeColors {

    // Light theme tokens.
    // zh_CN: Light 主题 token。
    namespace Light {

        // --- Fill Colors ---
        namespace Fill {
            // Accent colors measured from Figma.
            // zh_CN: 从 Figma 实测的强调色。
            extern const QColor AccentDefault;
            extern const QColor AccentSecondary;
            extern const QColor AccentTertiary;
            extern const QColor AccentDisabled;

            // Control background fills.
            // zh_CN: 控件背景填充色。
            extern const QColor ControlDefault;
            extern const QColor ControlSecondary;
            extern const QColor ControlTertiary;
            extern const QColor ControlDisabled;

            // Alternate control fills.
            // zh_CN: 交替控件背景填充色。
            extern const QColor ControlAltTransparent;
            extern const QColor ControlAltSecondary;
            extern const QColor ControlAltTertiary;
            extern const QColor ControlAltQuarternary;

            // Subtle hover/pressed fills.
            // zh_CN: 轻量 hover/pressed 填充色。
            extern const QColor SubtleTransparent;
            extern const QColor SubtleSecondary;
            extern const QColor SubtleTertiary;
            extern const QColor SubtleDisabled;
        }

        // --- Stroke Colors ---
        namespace Stroke {
            extern const QColor ControlDefault;
            extern const QColor ControlSecondary;
            extern const QColor ControlStrong;
            extern const QColor ControlOnImage;
            extern const QColor CardDefault;
            extern const QColor DividerDefault;
            extern const QColor SurfaceDefault;
            extern const QColor FocusOuter;
            extern const QColor FocusInner;
        }

        // --- Text Colors ---
        namespace Text {
            extern const QColor Primary;
            extern const QColor Secondary;
            extern const QColor Tertiary;
            extern const QColor Disabled;
            extern const QColor OnAccentPrimary;
            extern const QColor OnAccentSecondary;
            extern const QColor OnAccentTertiary;
            extern const QColor OnAccentDisabled;
            // Accent foreground measured from Figma; used for text links on neutral surfaces.
            // zh_CN: 从 Figma 实测的强调色前景，用于中性表面上的文本链接。
            extern const QColor AccentPrimary;
        }

        // --- System / Semantic Colors ---
        // zh_CN: 系统/语义色。
        namespace System {
            extern const QColor Critical;
            extern const QColor CriticalBackground;
            // Remaining semantic colors follow WinUI references when not exported by the current Figma node.
            // zh_CN: 当前 Figma node 未导出的语义色参考 WinUI。
            extern const QColor Caution;
            extern const QColor CautionBackground;
            extern const QColor Informational;
            extern const QColor InfoBackground;
            extern const QColor Success;
            extern const QColor SuccessBackground;
        }

        // --- Backgrounds ---
        extern const QColor BackgroundCanvas;
        extern const QColor BackgroundLayer;
        extern const QColor BackgroundLayerAlt;
        extern const QColor BackgroundSolid;

        // --- Neutral Palette ---
        // zh_CN: 中性灰阶。
        extern const QColor Grey10;
        extern const QColor Grey20;
        extern const QColor Grey30;
        extern const QColor Grey40;
        extern const QColor Grey50;
        extern const QColor Grey60;
        extern const QColor Grey80;
        extern const QColor Grey90;
        extern const QColor Grey100;
        extern const QColor Grey110;
        extern const QColor Grey120;
        extern const QColor Grey130;
        extern const QColor Grey140;
        extern const QColor Grey150;
        extern const QColor Grey160;
        extern const QColor Grey170;
        extern const QColor Grey180;
        extern const QColor Grey190;
        extern const QColor Grey200;

        // --- Chart Colors ---
        // zh_CN: Guidance & Charts 页面使用的图表色。
        extern const std::vector<QColor> Charts;
    }

    // Dark theme tokens.
    // zh_CN: Dark 主题 token。
    namespace Dark {

        // --- Fill Colors ---
        namespace Fill {
            extern const QColor AccentDefault;
            extern const QColor AccentSecondary;
            extern const QColor AccentTertiary;
            extern const QColor AccentDisabled;

            extern const QColor ControlDefault;
            extern const QColor ControlSecondary;
            extern const QColor ControlTertiary;
            extern const QColor ControlDisabled;

            extern const QColor ControlAltTransparent;
            extern const QColor ControlAltSecondary;
            extern const QColor ControlAltTertiary;
            extern const QColor ControlAltQuarternary;

            extern const QColor SubtleTransparent;
            extern const QColor SubtleSecondary;
            extern const QColor SubtleTertiary;
            extern const QColor SubtleDisabled;
        }

        // --- Stroke Colors ---
        namespace Stroke {
            extern const QColor ControlDefault;
            extern const QColor ControlSecondary;
            extern const QColor ControlStrong;
            extern const QColor ControlOnImage;
            extern const QColor CardDefault;
            extern const QColor DividerDefault;
            extern const QColor SurfaceDefault;
            extern const QColor FocusOuter;
            extern const QColor FocusInner;
        }

        // --- Text Colors ---
        namespace Text {
            extern const QColor Primary;
            extern const QColor Secondary;
            extern const QColor Tertiary;
            extern const QColor Disabled;
            extern const QColor OnAccentPrimary;
            extern const QColor OnAccentSecondary;
            extern const QColor OnAccentTertiary;
            extern const QColor OnAccentDisabled;
            // Dark-theme accent foreground measured from Figma.
            // zh_CN: 从 Figma 实测的 Dark 主题强调色前景。
            extern const QColor AccentPrimary;
        }

        // --- System / Semantic Colors ---
        // zh_CN: 系统/语义色。
        namespace System {
            extern const QColor Critical;
            extern const QColor CriticalBackground;
            extern const QColor Caution;
            extern const QColor CautionBackground;
            extern const QColor Informational;
            extern const QColor InfoBackground;
            extern const QColor Success;
            extern const QColor SuccessBackground;
        }

        // --- Backgrounds ---
        // zh_CN: 背景色；Canvas 基础值来自 Figma #202020。
        extern const QColor BackgroundCanvas;
        extern const QColor BackgroundLayer;
        extern const QColor BackgroundLayerAlt;
        extern const QColor BackgroundSolid;

        // --- Neutral Palette ---
        // zh_CN: 中性灰阶；Dark 下灰阶方向反转。
        extern const QColor Grey10;
        extern const QColor Grey20;
        extern const QColor Grey30;
        extern const QColor Grey40;
        extern const QColor Grey50;
        extern const QColor Grey60;
        extern const QColor Grey90;
        extern const QColor Grey130;
        extern const QColor Grey160;
        extern const QColor Grey190;

        // --- Chart Colors ---
        // zh_CN: 图表色。
        extern const std::vector<QColor> Charts;
    }

    // High-contrast theme tokens from the third Figma color-style group.
    // zh_CN: 来自 Figma Color Styles 面板第三组的高对比度主题 token。
    namespace Contrast {

        namespace Fill {
            extern const QColor AccentDefault;
            extern const QColor AccentSelected;
            extern const QColor ControlDefault;
            extern const QColor ControlFocus;
            extern const QColor ButtonText;
        }

        namespace Stroke {
            extern const QColor ControlDefault;
            extern const QColor ControlFocused;
            extern const QColor ButtonBorder;
        }

        namespace Text {
            extern const QColor Primary;
            extern const QColor Secondary;
            extern const QColor Disabled;
            extern const QColor OnAccent;
            extern const QColor Hyperlink;
        }

        extern const QColor BackgroundCanvas;
        extern const QColor BackgroundLayer;
    }
}

#endif // THEMECOLORS_H
