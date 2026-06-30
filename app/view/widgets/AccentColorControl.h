#ifndef ACCENTCOLORCONTROL_H
#define ACCENTCOLORCONTROL_H

#include <QColor>
#include <QWidget>

#include "components/foundation/FluentElement.h"

namespace fluent::gallery {

/**
 * @brief Trailing Settings control for picking the active style theme's accent color.
 * zh_CN: 用于挑选当前样式主题强调色的「设置」尾部控件。
 *
 * Renders a compact accent swatch button. Clicking it opens a flyout offering a grid of curated
 * preset accents, a full custom ColorPicker, a "reset to preset" action, and a shortcut to the
 * user-editable themes folder. Selecting any accent routes through GallerySettings, which persists
 * the choice into the style theme's themes/<key>.json override and repaints atomically — so the JSON
 * file is no longer the only way to customize colors.
 * zh_CN: 绘制一个紧凑的强调色色块按钮。点击后弹出浮层,提供精选预设强调色网格、完整自定义 ColorPicker、
 *「恢复预设」操作,以及通往用户可编辑主题文件夹的快捷入口。任何选择都经由 GallerySettings 持久化进该样式主题的
 * themes/<key>.json 覆盖并原子重绘——使 JSON 文件不再是定制颜色的唯一途径。
 */
class AccentColorControl : public QWidget, public fluent::FluentElement {
public:
    explicit AccentColorControl(QWidget* parent = nullptr);

    void onThemeUpdated() override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void openFlyout();
    void openCustomPicker(QWidget* anchor);
    void refreshAccent();

    QColor m_accent;
    bool m_hovered = false;
};

} // namespace fluent::gallery

#endif // ACCENTCOLORCONTROL_H
