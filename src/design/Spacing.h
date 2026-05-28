#ifndef SPACING_H
#define SPACING_H

/**
 * @brief Defines Fluent spacing, padding, border, and control-size tokens.
 * zh_CN: 定义 Fluent 间距、内边距、描边和控件尺寸 token。
 *
 * Values follow a 4px base grid and align common control metrics with the
 * Windows UI Kit basic input patterns.
 * zh_CN: 数值基于 4px 基础网格，并让常用控件尺寸对齐 Windows UI Kit Basic Input 模式。
 */
namespace Spacing {

    // Base spacing scale on a 4px grid.
    // zh_CN: 基于 4px 网格的基础间距阶梯。
    const int BaseUnit = 4;

    const int XSmall   =  4;   // 1×
    const int Small    =  8;   // 2×
    const int Medium   = 12;   // 3×
    const int Standard = 16;   // 4×
    const int Large    = 24;   // 6×
    const int XLarge   = 32;   // 8×
    const int XXLarge  = 48;   // 12×

    // Component padding tokens.
    // zh_CN: 组件内边距 token。
    namespace Padding {
        const int ControlHorizontal  = 12;  // Generic horizontal control padding. zh_CN: 通用控件水平内边距。
        const int ControlVertical    =  8;  // Generic vertical control padding. zh_CN: 通用控件垂直内边距。

        /**
         * @brief ComboBox content padding from the Windows UI Kit rest state.
         * zh_CN: 来自 Windows UI Kit / Figma ComboBox Rest 状态的内容区内边距。
         */
        const int ComboBoxHorizontal = 11;
        const int ComboBoxVertical   = 4;

        const int TextFieldHorizontal = 8;  // Text input horizontal padding. zh_CN: 输入框内容区水平内边距。
        const int TextFieldVertical   = 4;  // Text input vertical padding. zh_CN: 输入框内容区垂直内边距。

        const int Card   = 16;  // Card content padding. zh_CN: 卡片内容内边距。
        const int Dialog = 24;  // Dialog content padding. zh_CN: 对话框内容内边距。

        const int ListItemHorizontal = 12;  // List item horizontal padding. zh_CN: 列表项水平内边距。
        const int ListItemVertical   =  8;  // List item vertical padding. zh_CN: 列表项垂直内边距。
    }

    // Border width tokens.
    // zh_CN: 描边宽度 token。
    namespace Border {
        const int Normal  = 1;  // Default border width. zh_CN: 默认描边宽度。
        const int Focused = 2;  // Focus highlight stroke width. zh_CN: 聚焦高亮条宽度。
    }

    // Gap tokens between related controls or content groups.
    // zh_CN: 相关控件或内容组之间的间距 token。
    namespace Gap {
        const int Tight   =  4;  // Icon-to-text spacing. zh_CN: 图标与文字之间。
        const int Normal  =  8;  // Controls in the same group. zh_CN: 同组控件之间。
        const int Loose   = 16;  // Controls in different groups. zh_CN: 不同组控件之间。
        const int Section = 24;  // Section-to-section spacing. zh_CN: 区块之间。
    }

    // Standard control heights.
    // zh_CN: 标准控件高度。
    namespace ControlHeight {
        const int Small    = 24;  // Compact layout. zh_CN: 紧凑布局。
        const int Standard = 32;  // Default TextBox/Button height. zh_CN: TextBox/Button 默认高度。
        const int Large    = 40;  // Spacious layout. zh_CN: 宽松布局。
    }
}

#endif // SPACING_H
