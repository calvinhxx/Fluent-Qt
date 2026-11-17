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
 * @brief Returns the active backdrop state of the widget's top-level window.
 * zh_CN: 返回控件所在顶层窗口的背景状态。
 */
BackdropState windowBackdropState(const QWidget* widget);

/**
 * @brief Reads the published backdrop state if present, returning false otherwise.
 * zh_CN: 读取顶层窗口已记录的背景状态；尚未设置时返回 false。
 */
bool tryWindowBackdropState(const QWidget* widget, BackdropState* state);

/**
 * @brief Stores the resolved backdrop state on a top-level window for child widgets to query.
 * zh_CN: 将背景状态写入顶层窗口，供子控件查询。
 */
void publishWindowBackdropState(QWidget* window, const BackdropState& state);

/**
 * @brief Whether widgets on this window should clear to transparent so the native backdrop shows through.
 * zh_CN: 当前窗口是否需要透明擦除以透出系统合成背景。
 */
bool windowBackdropRequiresTransparentClear(const QWidget* widget);
/**
 * @brief Whether this window paints an opaque software material instead of using a native backdrop.
 * zh_CN: 当前窗口是否使用软件绘制的不透明材质。
 */
bool windowBackdropUsesPaintedMaterial(const QWidget* widget);
/**
 * @brief Whether the effective effect is Mica or Acrylic.
 * zh_CN: 实际生效的效果是否为 Mica 或 Acrylic。
 */
bool windowHasMaterialBackdrop(const QWidget* widget);

/**
 * @brief Resolves the background fill color for title bars and navigation panes; returns an invalid color when the area must stay transparent for a composited backdrop.
 * zh_CN: 解析标题栏、导航栏等区域应绘制的背景色；若需透明以透出系统合成背景，则返回无效颜色。
 */
QColor windowChromeBackdropFill(const FluentElement& themeHost, const QWidget* hostWindow, bool active);

} // namespace fluent::windowing

#endif // FLUENTWINDOWBACKDROP_H
