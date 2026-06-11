#ifndef GALLERYCOMPACTFLYOUT_H
#define GALLERYCOMPACTFLYOUT_H

#include <functional>

#include <QString>
#include <QWidget>

#include "components/foundation/FluentElement.h"

class QEvent;
class QMouseEvent;
class QPaintEvent;

namespace fluent::gallery {

/**
 * @brief Self-painting row used inside the compact navigation flyout.
 * zh_CN: 紧凑导航 flyout 内使用的自绘行控件。
 */
class CompactFlyoutRow : public QWidget, public fluent::FluentElement {
public:
    CompactFlyoutRow(const QString& routeId,
                     const QString& text,
                     bool selected,
                     QWidget* parent = nullptr);

    QSize sizeHint() const override;

    std::function<void(const QString&)> onActivated;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QString m_routeId;
    QString m_text;
    bool m_selected = false;
    bool m_hovered = false;
    bool m_pressed = false;
};

/**
 * @brief Opaque content panel that paints the compact flyout background.
 * zh_CN: 绘制紧凑 flyout 背景的不透明内容面板。
 */
class CompactFlyoutPanel : public QWidget, public fluent::FluentElement {
public:
    explicit CompactFlyoutPanel(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

} // namespace fluent::gallery

#endif // GALLERYCOMPACTFLYOUT_H
