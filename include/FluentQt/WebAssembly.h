#ifndef FLUENTQT_WEBASSEMBLY_H
#define FLUENTQT_WEBASSEMBLY_H

class QRect;
class QWidget;

namespace fluent::webassembly {

/**
 * @brief Initial presentation for a browser-hosted top-level widget.
 * zh_CN: 浏览器宿主中顶层控件的初始展示形态。
 */
enum class WindowPresentation {
    Windowed,
    Maximized
};

/**
 * @brief Configures FluentQt's browser window and popup capabilities.
 * zh_CN: 配置 FluentQt 的浏览器窗口与弹出表面能力。
 *
 * Call once before constructing Fluent widgets. The function is idempotent and
 * is supplied by the optional FluentQt::WebAssembly target.
 * zh_CN: 请在创建 Fluent 控件前调用一次。该函数可重复调用，由可选的
 * FluentQt::WebAssembly target 提供。
 */
void configureRuntime();

/**
 * @brief Shows a top-level widget with stable browser geometry.
 * zh_CN: 使用稳定的浏览器几何显示顶层控件。
 *
 * The adapter hosts the widget inside a single browser-sized Qt desktop
 * surface and applies its normal or maximized geometry there. Native
 * applications continue to call QWidget::show() directly.
 * zh_CN: 适配器会将控件承载到单一浏览器尺寸的 Qt 桌面表面中，并在其中应用
 * normal 或 maximized 几何。原生应用仍直接调用 QWidget::show()。
 */
void showWindow(QWidget* window,
                const QRect& normalGeometry,
                WindowPresentation presentation = WindowPresentation::Windowed);

} // namespace fluent::webassembly

#endif // FLUENTQT_WEBASSEMBLY_H
