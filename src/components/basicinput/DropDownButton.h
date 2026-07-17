#ifndef DROPDOWNBUTTON_H
#define DROPDOWNBUTTON_H

#include "Button.h"
#include <QMenu>
#include "design/Typography.h"
#include "design/Spacing.h"

class QPropertyAnimation;

namespace fluent::basicinput {

/**
 * @brief Fluent button that opens an attached dropdown or flyout.
 * zh_CN: 用于打开下拉菜单或浮层的 Fluent 按钮。
 *
 * DropDownButton extends Button with open-state visuals, chevron glyph metrics,
 * and press progress so popup-backed entry controls can share one surface style.
 * zh_CN: DropDownButton 在 Button 基础上增加打开态视觉、下拉箭头绘制参数和按压进度，
 * 让基于弹层的入口控件复用同一套按钮表面风格。
 */
class DropDownButton : public Button {
    Q_OBJECT
    /**
     * @brief Whether the popup, drawer, or notification surface is open.
     * zh_CN: 弹层、抽屉或通知表面是否处于打开状态。
     */
    Q_PROPERTY(bool isOpen READ isOpen WRITE setIsOpen NOTIFY openChanged)
    /**
     * @brief Iconfont glyph used for the chevron affordance.
     * zh_CN: 下拉箭头使用的 iconfont 字符。
     */
    Q_PROPERTY(QString chevronGlyph READ chevronGlyph WRITE setChevronGlyph NOTIFY chevronChanged)
    /**
     * @brief Iconfont family used for glyph rendering.
     * zh_CN: 图标字符绘制使用的 iconfont 字体族。
     */
    Q_PROPERTY(QString iconFontFamily READ iconFontFamily WRITE setIconFontFamily NOTIFY chevronChanged)
    /**
     * @brief Chevron glyph pixel size.
     * zh_CN: 下拉箭头图标像素尺寸。
     */
    Q_PROPERTY(int chevronSize READ chevronSize WRITE setChevronSize NOTIFY chevronChanged)
    /**
     * @brief Pixel offset applied to chevron drawing.
     * zh_CN: 下拉箭头绘制时应用的像素偏移。
     */
    Q_PROPERTY(QPoint chevronOffset READ chevronOffset WRITE setChevronOffset NOTIFY chevronChanged)
    /**
     * @brief Animated press progress used by the painted surface.
     * zh_CN: 自绘表面使用的按压动画进度。
     */
    Q_PROPERTY(qreal pressProgress READ pressProgress WRITE setPressProgress)

public:
    explicit DropDownButton(const QString& text, QWidget* parent = nullptr);
    explicit DropDownButton(QWidget* parent = nullptr);

    void setMenu(QMenu* menu);
    QMenu* menu() const { return m_menu; }

    bool isOpen() const { return m_isOpen; }
    void setOpen(bool open);
    void setIsOpen(bool open) { setOpen(open); }

    QString chevronGlyph() const { return m_chevronGlyph; }
    void setChevronGlyph(const QString& glyph);

    QString iconFontFamily() const { return m_iconFontFamily; }
    void setIconFontFamily(const QString& family);

    int chevronSize() const { return m_chevronSize; }
    void setChevronSize(int size);

    QPoint chevronOffset() const { return m_chevronOffset; }
    void setChevronOffset(const QPoint& offset);

    qreal pressProgress() const { return m_pressProgress; }
    void setPressProgress(qreal value);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void openChanged();
    void chevronChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    QRectF contentPaintRect(const QRectF& surfaceRect) const override;

private:
    void initAnimation();
    int chevronReserveWidth() const;

    QMenu* m_menu = nullptr;
    bool m_isOpen = false;
    QString m_chevronGlyph = Typography::Icons::ChevronDown;
    QString m_iconFontFamily = Typography::FontFamily::FluentIcons;
    int m_chevronSize = Typography::FontSize::Caption;
    QPoint m_chevronOffset {::Spacing::Padding::ControlHorizontal, 0}; // x: 右侧间距, y: 垂直偏移
    qreal m_pressProgress = 0.0;              // 0 = 静止, 1 = 点击/展开高亮
    QPropertyAnimation* m_pressAnimation = nullptr;
};

} // namespace fluent::basicinput

#endif // DROPDOWNBUTTON_H
