#ifndef BREAKPOINTS_H
#define BREAKPOINTS_H

/**
 * @brief Defines Fluent adaptive layout breakpoints and window-size constraints.
 * zh_CN: 定义 Fluent 自适应布局断点和窗口尺寸约束。
 */
namespace Breakpoints {

    // Layout breakpoints in pixels.
    // zh_CN: 布局断点，单位为像素。
    const int Small  =  640;  // 0-640: Compact phone or narrow-window layout. zh_CN: 手机或窄窗口紧凑布局。
    const int Medium = 1007;  // 641-1007: medium tablet or half-screen layout. zh_CN: 平板或半屏中等布局。
    const int Large  = 1920;  // 1008-1920: expanded desktop layout. zh_CN: 桌面展开布局。

    // Minimum top-level window size constraints in pixels.
    // zh_CN: 顶层窗口最小尺寸约束，单位为像素。
    const int MinWindowWidth  = 320;
    const int MinWindowHeight = 500;

    // Standard NavigationView pane widths in pixels.
    // zh_CN: NavigationView 窗格标准宽度，单位为像素。
    const int NavigationPaneCompactWidth  =  48;
    const int NavigationPaneExpandedWidth = 320;
}

#endif // BREAKPOINTS_H
