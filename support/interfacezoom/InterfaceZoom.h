#ifndef FLUENTQT_SUPPORT_INTERFACEZOOM_INTERFACEZOOM_H
#define FLUENTQT_SUPPORT_INTERFACEZOOM_INTERFACEZOOM_H

#include <QtGlobal>

/**
 * @brief Browser-style whole-interface zoom shared by the C++ and Python Galleries.
 * zh_CN: C++ 与 Python Gallery 共用的浏览器式整体界面缩放。
 *
 * The backend uses Qt's private high-DPI and window-system hooks, so it is
 * application infrastructure: it is not installed with the SDK and stays out of
 * the Widgets core. Desktop Gallery builds require the matching private headers.
 * zh_CN: 后端使用 Qt 私有的高 DPI 与窗口系统接口，因此属于应用基础设施：不随 SDK
 * 安装，也不进入 Widgets 核心。桌面 Gallery 构建必须提供匹配的私有头。
 *
 * Fractional Cocoa damage is aligned without forcing whole-window repaints.
 * The startup-only helper remains available for binding/bootstrap tests; it
 * does not install a repaint filter or provide Gallery's rendering contract.
 * zh_CN: Cocoa 分数倍率下对齐脏区域，不强制整窗刷新。仅启动缩放的辅助路径供绑定
 * 和启动测试使用，不安装重绘过滤器，也不提供 Gallery 的绘制保证。
 */
namespace fluent::support::interfacezoom {

/**
 * @brief Reports whether open windows can rescale at runtime.
 * zh_CN: 返回已打开的窗口能否在运行时缩放。
 */
bool isSupported();

/**
 * @brief Applies saved zoom through QT_SCALE_FACTOR before constructing the application.
 * zh_CN: 在创建应用对象前通过 QT_SCALE_FACTOR 应用已保存倍率。
 */
bool prepareStartup(qreal zoom);

/**
 * @brief Returns the applied zoom independently of pending settings.
 * zh_CN: 返回实际生效倍率，与待生效设置分开。
 */
qreal appliedZoom();

/**
 * @brief Rescales every window by @p zoom while keeping each native window size.
 * zh_CN: 按 @p zoom 缩放所有窗口，同时保持各原生窗口尺寸不变。
 *
 * QT_SCALE_FACTOR remains the base scale and @p zoom multiplies it. Widgets
 * receive the new logical size and relayout, as a web page does under browser
 * zoom. Returns false when unsupported or when @p zoom is not positive.
 * zh_CN: QT_SCALE_FACTOR 仍是基础倍率，@p zoom 在其上相乘。控件获得新的逻辑尺寸并
 * 重新布局，与网页在浏览器缩放下一致；不支持或 @p zoom 非正数时返回 false。
 */
bool apply(qreal zoom);

} // namespace fluent::support::interfacezoom

#endif // FLUENTQT_SUPPORT_INTERFACEZOOM_INTERFACEZOOM_H
