#ifndef ACCENTCOLORCONTROL_H
#define ACCENTCOLORCONTROL_H

#include <QColor>
#include <QWidget>

#include "compatibility/QtCompat.h"
#include "components/foundation/FluentElement.h"

namespace fluent::gallery {

/**
 * @brief Trailing Settings control for picking the Fluent accent color.
 *
 * Displays a compact swatch button whose flyout offers curated preset accents,
 * a custom ColorPicker, preset reset, and a shortcut to the user themes folder.
 * Selected accents persist via GallerySettings and update the theme atomically.
 * zh_CN: Settings 页用于挑选 Fluent 强调色的尾部控件：点击色块弹出浮层，
 * 提供预设色板、自定义 ColorPicker、恢复默认与打开主题目录，选择后经 GallerySettings 持久化并原子刷新主题。
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
    void enterEvent(FluentEnterEvent* event) override;
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
