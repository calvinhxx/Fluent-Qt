#ifndef FLUENTWINDOWBACKDROP_H
#define FLUENTWINDOWBACKDROP_H

#include <QColor>
#include "compatibility/WindowBackdropTypes.h"

class QWidget;

namespace fluent {
class FluentElement;
}

namespace fluent::windowing {

/**
 * @brief Returns the published backdrop state for a widget's top-level window.
 * zh_CN: 返回控件所属顶层窗口发布的背景状态。
 */
BackdropState windowBackdropState(const QWidget* widget);

/**
 * @brief Tries to read a published state without synthesizing a default.
 * zh_CN: 尝试读取已发布状态，不合成默认值。
 */
bool tryWindowBackdropState(const QWidget* widget, BackdropState* state);

/**
 * @brief Publishes a backdrop state on a top-level window for descendant consumers.
 * zh_CN: 在顶层窗口发布背景状态，供后代控件读取。
 */
void publishWindowBackdropState(QWidget* window, const BackdropState& state);

/**
 * @brief Whether transparent clearing is safe for this window.
 * zh_CN: 当前窗口是否可安全擦除为透明。
 */
bool windowBackdropRequiresTransparentClear(const QWidget* widget);
/**
 * @brief Whether UILib paints an opaque software material.
 * zh_CN: UILib 是否正在绘制不透明软件材质。
 */
bool windowBackdropUsesPaintedMaterial(const QWidget* widget);
/**
 * @brief Whether the effective effect is Mica or Acrylic.
 * zh_CN: 实际效果是否为 Mica 或 Acrylic。
 */
bool windowHasMaterialBackdrop(const QWidget* widget);

/**
 * @brief Resolves the fill a window-chrome surface should paint.
 * zh_CN: 解析窗口 chrome 表面应绘制的填充色。
 *
 * An invalid color means that a real composited backdrop must remain visible
 * through a transparent clear. Otherwise the returned color is the opaque
 * software fallback for the requested effect and activation state.
 * zh_CN: 无效颜色表示应透明擦除以露出真实合成背景；否则返回请求效果和激活状态对应的不透明软件回退色。
 */
QColor windowChromeBackdropFill(const FluentElement& themeHost, const QWidget* hostWindow,
                                bool active);

} // namespace fluent::windowing

#endif // FLUENTWINDOWBACKDROP_H
