#ifndef CORNERRADIUS_H
#define CORNERRADIUS_H

#include <QtGlobal>

/**
 * @brief Defines Fluent corner-radius tokens.
 * zh_CN: 定义 Fluent 圆角 token。
 *
 * WinUI-style controls mostly use two radius levels: 4px for controls and
 * 8px for overlay containers.
 * zh_CN: WinUI 风格控件主要使用两个圆角档位：页面控件 4px，浮层容器 8px。
 */
namespace CornerRadius {

    const int None    = 0;  // Square corners for flush edges. zh_CN: 与直边相接时使用的直角。
    const int Control = 4;  // In-page controls such as Button and TextBox. zh_CN: Button、TextBox 等页面内控件。
    const int Overlay = 8;  // Overlay containers such as Flyout, Dialog, and ToolTip. zh_CN: Flyout、Dialog、ToolTip 等浮层容器。

    // Selection-indicator pills (TabView, SelectorBar, Pivot, ToggleButton bar):
    // a 3px-thick accent bar fully rounds at half its thickness.
    // zh_CN: 选中指示条（TabView、SelectorBar、Pivot、ToggleButton 横条）：
    // 3px 厚的强调色胶囊条按厚度一半取圆角。
    const qreal Indicator = 1.5;
}

#endif // CORNERRADIUS_H
