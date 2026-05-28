#ifndef THEMECOLORS_H
#define THEMECOLORS_H

#include <QColor>
#include <QString>
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
            const QColor AccentDefault   = QColor("#005FB8");        // Measured from Figma. zh_CN: Figma 实测值。
            const QColor AccentSecondary = QColor(0, 95, 184, 230);  // ~90% opacity
            const QColor AccentTertiary  = QColor(0, 95, 184, 204);  // ~80% opacity
            const QColor AccentDisabled  = QColor(0, 0, 0, 37);      // ~15% black

            // Control background fills.
            // zh_CN: 控件背景填充色。
            const QColor ControlDefault    = QColor("#FFFFFF");
            const QColor ControlSecondary  = QColor(249, 249, 249, 242); // 95%
            const QColor ControlTertiary   = QColor(249, 249, 249, 204); // 80%
            const QColor ControlDisabled   = QColor(249, 249, 249, 77);  // 30%

            // Alternate control fills.
            // zh_CN: 交替控件背景填充色。
            const QColor ControlAltTransparent = QColor(255, 255, 255, 0);
            const QColor ControlAltSecondary   = QColor(0, 0, 0, 5);    // ~2%
            const QColor ControlAltTertiary    = QColor(0, 0, 0, 15);   // ~6%
            const QColor ControlAltQuarternary = QColor(0, 0, 0, 31);   // ~12%

            // Subtle hover/pressed fills.
            // zh_CN: 轻量 hover/pressed 填充色。
            const QColor SubtleTransparent = QColor(0, 0, 0, 0);
            const QColor SubtleSecondary   = QColor(0, 0, 0, 9);        // Hover
            const QColor SubtleTertiary    = QColor(0, 0, 0, 6);        // Drag over
            const QColor SubtleDisabled    = QColor(0, 0, 0, 0);
        }

        // --- Stroke Colors ---
        namespace Stroke {
            const QColor ControlDefault    = QColor(0, 0, 0, 12);    // ~5% black; corrected from 15. zh_CN: 约 5% 黑色，由原 15 修正。
            const QColor ControlSecondary  = QColor(0, 0, 0, 41);    // ~16% black
            const QColor ControlStrong     = QColor(0, 0, 0, 112);   // ~44% black
            const QColor ControlOnImage    = QColor(28, 28, 28, 196);
            const QColor CardDefault       = QColor(0, 0, 0, 13);    // ~5% black
            const QColor DividerDefault    = QColor(0, 0, 0, 20);    // ~8% black
            const QColor SurfaceDefault    = QColor("#757575");
            const QColor FocusOuter        = QColor(0, 0, 0, 230);   // Outer focus ring. zh_CN: 外层焦点环。
            const QColor FocusInner        = QColor("#FFFFFF");       // Inner focus ring. zh_CN: 内层焦点环。
        }

        // --- Text Colors ---
        namespace Text {
            const QColor Primary        = QColor(0, 0, 0, 230);   // ~90%
            const QColor Secondary      = QColor(0, 0, 0, 154);   // ~60%
            const QColor Tertiary       = QColor(0, 0, 0, 112);   // ~44%
            const QColor Disabled       = QColor(0, 0, 0, 92);    // ~36%
            const QColor OnAccentPrimary   = QColor("#FFFFFF");
            const QColor OnAccentSecondary = QColor(255, 255, 255, 204);
            const QColor OnAccentTertiary  = QColor(255, 255, 255, 179);
            const QColor OnAccentDisabled  = QColor(255, 255, 255, 128);
            // Accent foreground measured from Figma; used for text links on neutral surfaces.
            // zh_CN: 从 Figma 实测的强调色前景，用于中性表面上的文本链接。
            const QColor AccentPrimary  = QColor("#003E92");
        }

        // --- System / Semantic Colors ---
        // zh_CN: 系统/语义色。
        namespace System {
            const QColor Critical           = QColor("#C42B1C");
            const QColor CriticalBackground = QColor("#FDE7E9");
            // Remaining semantic colors follow WinUI references when not exported by the current Figma node.
            // zh_CN: 当前 Figma node 未导出的语义色参考 WinUI。
            const QColor Caution            = QColor("#9D5D00");
            const QColor CautionBackground  = QColor("#FFF4CE");
            const QColor Informational      = QColor("#015CDA");
            const QColor InfoBackground     = QColor("#F6F6F6");
            const QColor Success            = QColor("#0F7B0F");
            const QColor SuccessBackground  = QColor("#DFF6DD");
        }

        // --- Backgrounds ---
        const QColor BackgroundCanvas     = QColor("#F3F3F3");
        const QColor BackgroundLayer      = QColor("#FFFFFF");
        const QColor BackgroundLayerAlt   = QColor("#F9F9F9");
        const QColor BackgroundSolid      = QColor("#EEEEEE");

        // --- Neutral Palette ---
        // zh_CN: 中性灰阶。
        const QColor Grey10  = QColor("#FAF9F8");
        const QColor Grey20  = QColor("#F3F2F1");
        const QColor Grey30  = QColor("#EDEBE9");
        const QColor Grey40  = QColor("#E1DFDD");
        const QColor Grey50  = QColor("#D2D0CE");
        const QColor Grey60  = QColor("#C8C6C4");
        const QColor Grey80  = QColor("#B3B0AD");
        const QColor Grey90  = QColor("#A19F9D");
        const QColor Grey100 = QColor("#979593");
        const QColor Grey110 = QColor("#8A8886");
        const QColor Grey120 = QColor("#797775");
        const QColor Grey130 = QColor("#605E5C");
        const QColor Grey140 = QColor("#484644");
        const QColor Grey150 = QColor("#3B3A39");
        const QColor Grey160 = QColor("#323130");
        const QColor Grey170 = QColor("#292827");
        const QColor Grey180 = QColor("#252423");
        const QColor Grey190 = QColor("#201F1E");
        const QColor Grey200 = QColor("#11100F");

        // --- Chart Colors ---
        // zh_CN: Guidance & Charts 页面使用的图表色。
        const std::vector<QColor> Charts = {
            QColor("#005FB8"), QColor("#00BCF2"), QColor("#2B88D8"),
            QColor("#004B50"), QColor("#00AD56"), QColor("#007833"),
            QColor("#881798"), QColor("#B4009E"), QColor("#E3008C"),
            QColor("#D83B01"), QColor("#EA4300"), QColor("#FF8C00")
        };
    }

    // Dark theme tokens.
    // zh_CN: Dark 主题 token。
    namespace Dark {

        // --- Fill Colors ---
        namespace Fill {
            const QColor AccentDefault   = QColor("#60CDFF");
            const QColor AccentSecondary = QColor(96, 205, 255, 230);  // ~90%
            const QColor AccentTertiary  = QColor(96, 205, 255, 204);  // ~80%
            const QColor AccentDisabled  = QColor(255, 255, 255, 37);  // ~15%

            const QColor ControlDefault    = QColor(255, 255, 255, 15);  // ~6%
            const QColor ControlSecondary  = QColor(255, 255, 255, 23);  // ~9%
            const QColor ControlTertiary   = QColor(255, 255, 255, 10);  // ~4%
            const QColor ControlDisabled   = QColor(255, 255, 255, 6);   // ~2.5%

            const QColor ControlAltTransparent = QColor(255, 255, 255, 0);
            const QColor ControlAltSecondary   = QColor(255, 255, 255, 15);
            const QColor ControlAltTertiary    = QColor(255, 255, 255, 20);
            const QColor ControlAltQuarternary = QColor(255, 255, 255, 30);

            const QColor SubtleTransparent = QColor(255, 255, 255, 0);
            const QColor SubtleSecondary   = QColor(255, 255, 255, 15); // Hover
            const QColor SubtleTertiary    = QColor(255, 255, 255, 10); // Drag over
            const QColor SubtleDisabled    = QColor(255, 255, 255, 0);
        }

        // --- Stroke Colors ---
        namespace Stroke {
            const QColor ControlDefault   = QColor(255, 255, 255, 17);  // ~7%
            const QColor ControlSecondary = QColor(255, 255, 255, 23);  // ~9%
            const QColor ControlStrong    = QColor(255, 255, 255, 138); // ~54%
            const QColor ControlOnImage   = QColor(255, 255, 255, 26);
            const QColor CardDefault      = QColor(255, 255, 255, 10);  // ~4%
            const QColor DividerDefault   = QColor(255, 255, 255, 20);  // ~8%
            const QColor SurfaceDefault   = QColor(255, 255, 255, 30);  // ~12%
            const QColor FocusOuter       = QColor(255, 255, 255, 230);
            const QColor FocusInner       = QColor(0, 0, 0, 230);
        }

        // --- Text Colors ---
        namespace Text {
            const QColor Primary           = QColor("#FFFFFF");
            const QColor Secondary         = QColor(255, 255, 255, 199); // ~78%
            const QColor Tertiary          = QColor(255, 255, 255, 138); // ~54%
            const QColor Disabled          = QColor(255, 255, 255, 92);  // ~36%
            const QColor OnAccentPrimary   = QColor("#000000");
            const QColor OnAccentSecondary = QColor(0, 0, 0, 128);
            const QColor OnAccentTertiary  = QColor(0, 0, 0, 77);
            const QColor OnAccentDisabled  = QColor(0, 0, 0, 128);
            // Dark-theme accent foreground measured from Figma.
            // zh_CN: 从 Figma 实测的 Dark 主题强调色前景。
            const QColor AccentPrimary     = QColor("#99EBFF");
        }

        // --- System / Semantic Colors ---
        // zh_CN: 系统/语义色。
        namespace System {
            const QColor Critical           = QColor("#FF99A4");
            const QColor CriticalBackground = QColor("#442726");
            const QColor Caution            = QColor("#FCE100");
            const QColor CautionBackground  = QColor("#433519");
            const QColor Informational      = QColor("#60CDFF");
            const QColor InfoBackground     = QColor("#1F3150");
            const QColor Success            = QColor("#6CCB5F");
            const QColor SuccessBackground  = QColor("#1E3C1F");
        }

        // --- Backgrounds ---
        // zh_CN: 背景色；Canvas 基础值来自 Figma #202020。
        const QColor BackgroundCanvas   = QColor("#202020");  // Figma: Dark/Background/Solid Background/Base
        const QColor BackgroundLayer    = QColor("#2C2C2C");
        const QColor BackgroundLayerAlt = QColor("#3D3D3D");
        const QColor BackgroundSolid    = QColor("#1C1C1C");

        // --- Neutral Palette ---
        // zh_CN: 中性灰阶；Dark 下灰阶方向反转。
        const QColor Grey10  = QColor("#FAF9F8");
        const QColor Grey20  = QColor("#F3F2F1");
        const QColor Grey30  = QColor("#EDEBE9");
        const QColor Grey40  = QColor("#E1DFDD");
        const QColor Grey50  = QColor("#D2D0CE");
        const QColor Grey60  = QColor("#C8C6C4");
        const QColor Grey90  = QColor("#A19F9D");
        const QColor Grey130 = QColor("#605E5C");
        const QColor Grey160 = QColor("#323130");
        const QColor Grey190 = QColor("#201F1E");

        // --- Chart Colors ---
        // zh_CN: 图表色。
        const std::vector<QColor> Charts = {
            QColor("#60CDFF"), QColor("#00BCF2"), QColor("#2B88D8"),
            QColor("#00AD56"), QColor("#107C10"), QColor("#004B50"),
            QColor("#FF8C00"), QColor("#F7630C"), QColor("#EA4300"),
            QColor("#E3008C"), QColor("#BF0077"), QColor("#C239B3")
        };
    }

    // High-contrast theme tokens from the third Figma color-style group.
    // zh_CN: 来自 Figma Color Styles 面板第三组的高对比度主题 token。
    namespace Contrast {

        namespace Fill {
            const QColor AccentDefault  = QColor("#1AEBFF");  // High-contrast accent. zh_CN: 高对比 Accent。
            const QColor AccentSelected = QColor("#000000");
            const QColor ControlDefault = QColor("#000000");
            const QColor ControlFocus   = QColor("#000000");
            const QColor ButtonText     = QColor("#FFFFFF");
        }

        namespace Stroke {
            const QColor ControlDefault  = QColor("#FFFFFF");
            const QColor ControlFocused  = QColor("#1AEBFF");
            const QColor ButtonBorder    = QColor("#FFFFFF");
        }

        namespace Text {
            const QColor Primary     = QColor("#FFFFFF");
            const QColor Secondary   = QColor("#FFFFFF");
            const QColor Disabled    = QColor("#3FF23F");  // High-contrast disabled text uses green. zh_CN: 高对比度禁用文字使用绿色。
            const QColor OnAccent    = QColor("#000000");
            const QColor Hyperlink   = QColor("#FFFF00");
        }

        const QColor BackgroundCanvas = QColor("#000000");
        const QColor BackgroundLayer  = QColor("#000000");
    }
}

#endif // THEMECOLORS_H
