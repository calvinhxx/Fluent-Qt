#ifndef FLUENTQT_WEBASSEMBLY_H
#define FLUENTQT_WEBASSEMBLY_H

class QRect;
class QWidget;

namespace fluent::webassembly {

/**
 * @brief Initial window mode when running inside a browser.
 * zh_CN: 浏览器环境下的初始窗口显示方式（常规窗口或最大化）。
 */
enum class WindowPresentation { Windowed, Maximized };

/**
 * @brief Initializes WebAssembly window and popup handling.
 * zh_CN: 初始化 WebAssembly 环境下的窗口和弹窗适配。
 *
 * Call once before creating Fluent widgets (safe to call multiple times).
 * Supplied by FluentQt::WebAssembly.
 * zh_CN: 请在创建 Fluent 控件前调用一次（可重复调用），由 FluentQt::WebAssembly 提供。
 */
void configureRuntime();

/**
 * @brief Shows a top-level widget inside the browser viewport.
 * zh_CN: 在浏览器页面区域内显示顶层窗口，并应用尺寸或最大化状态。
 *
 * Places the widget in a browser-sized Qt surface and applies its normal or
 * maximized geometry. Desktop builds continue to call QWidget::show() directly.
 * zh_CN: 将控件放入与浏览器画布等大的显示区域中，并设置常规或最大化尺寸。
 * 桌面原生应用仍直接调用 QWidget::show()。
 */
void showWindow(QWidget* window,
    const QRect& normalGeometry,
    WindowPresentation presentation = WindowPresentation::Windowed);

} // namespace fluent::webassembly

#endif // FLUENTQT_WEBASSEMBLY_H
