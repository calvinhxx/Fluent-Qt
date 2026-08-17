#ifndef SPLITBUTTON_H
#define SPLITBUTTON_H

#include "Button.h"
#include <QMenu>
#include <QPointer>

class QVariantAnimation;
class QKeyEvent;

namespace fluent::basicinput {

/**
 * @brief Button with separate primary action and secondary menu regions.
 * zh_CN: 将主操作区和二级菜单区拆开的按钮。
 *
 * SplitButton keeps the primary click distinct from the dropdown affordance so
 * callers can expose a default command together with additional choices.
 * zh_CN: SplitButton 将主点击和下拉入口分离，便于调用方同时提供默认命令和更多选项。
 */
class SplitButton : public Button {
    Q_OBJECT
    /**
     * @brief Menu opened by the secondary split-button region.
     * zh_CN: 拆分按钮二级区域打开的菜单。
     */
    Q_PROPERTY(QMenu* menu READ menu WRITE setMenu NOTIFY menuChanged)
    /**
     * @brief Whether the attached menu is currently visible.
     * zh_CN: 附加菜单当前是否可见。
     */
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY openChanged)
    /**
     * @brief Width of the split-button secondary action region.
     * zh_CN: 拆分按钮二级操作区域宽度。
     */
    Q_PROPERTY(int secondaryWidth READ secondaryWidth WRITE setSecondaryWidth NOTIFY secondaryWidthChanged)

public:
    enum SplitPart { None, Primary, Secondary };

    explicit SplitButton(const QString& text = "", QWidget* parent = nullptr);
    
    QMenu* menu() const { return m_menu.data(); }
    void setMenu(QMenu* menu);

    bool isOpen() const { return m_isOpen; }

    int secondaryWidth() const { return m_secondaryWidth; }
    void setSecondaryWidth(int width);

signals:
    void menuChanged();
    void openChanged();
    void secondaryWidthChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    SplitPart getPartAt(const QPoint& pos) const;

    int primaryTrailingInset() const;
    QRectF primaryContentRect(const QRectF& primaryRect) const;

    QPointer<QMenu> m_menu;
    SplitPart m_hoverPart = None;
    SplitPart m_pressPart = None;
    
    int m_secondaryWidth = 32;

private:
    void startPressAnimation(SplitPart part);
    void setOpen(bool open);

    QVariantAnimation* m_pressAnimation = nullptr;
    SplitPart m_animatedPart = None;
    qreal m_pressProgress = 0.0;
    bool m_isOpen = false;
};

} // namespace fluent::basicinput

#endif // SPLITBUTTON_H
