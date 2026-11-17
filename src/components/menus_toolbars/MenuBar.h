#ifndef FLUENT_MENUBAR_H
#define FLUENT_MENUBAR_H

#include <QHash>
#include <QMenuBar>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QAction;
class QActionEvent;
class QEvent;
class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;

namespace fluent::basicinput {
class DropDownButton;
}

namespace fluent::menus_toolbars {

class FluentMenu;

/**
 * @brief Fluent-styled menu bar with overflow menu and configurable font role.
 * Actions that exceed the available bar width remain accessible from the overflow button.
 * zh_CN: 支持响应式溢出与字体角色配置的 Fluent 菜单栏；宽度不足时超出的菜单项会收起到溢出按钮中。
 */
class FluentMenuBar : public QMenuBar, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Fluent typography style used by menu text.
     * zh_CN: 菜单文本使用的 Fluent 排版样式。
     */
    Q_PROPERTY(Typography::FontRole fontStyle READ fontStyle WRITE setFontStyle NOTIFY fontStyleChanged)
    /**
     * @brief Whether the menu bar paints its background fill.
     * Disable to blend into a parent container that already provides a background, such as a card or custom title bar.
     * zh_CN: 是否绘制菜单栏背景；关闭后可融入卡片或自定义标题栏等自带背景的容器。
     */
    Q_PROPERTY(bool backgroundVisible READ backgroundVisible WRITE setBackgroundVisible NOTIFY backgroundVisibleChanged)
public:
    explicit FluentMenuBar(QWidget* parent = nullptr);

    void setFontStyle(Typography::FontRole role);
    Typography::FontRole fontStyle() const { return m_fontStyle; }

    bool backgroundVisible() const { return m_backgroundVisible; }
    bool isBackgroundVisible() const { return backgroundVisible(); }
    void setBackgroundVisible(bool visible);

    QRect fluentActionGeometry(QAction* action) const;
    QAction* hoveredAction() const { return m_hoveredAction; }
    QAction* focusedAction() const { return m_focusedAction; }
    QAction* openAction() const { return m_openAction; }

    void onThemeUpdated() override;
    QSize sizeHint() const override;

signals:
    void fontStyleChanged();
    void backgroundVisibleChanged(bool visible);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool event(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void actionEvent(QActionEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Metrics {
        int rowHeight = 40;
        int itemHeight = 40;
        int horizontalPadding = 12;
        int itemSpacing = 4;
        int cornerRadius = 4;
    };

    Metrics metrics() const;
    void invalidateLayout();
    void ensureLayout() const;
    void syncOverflow();
    QAction* actionAt(const QPoint& position, bool enabledOnly) const;
    QAction* firstEnabledAction() const;
    QAction* nextEnabledAction(QAction* from, int direction) const;
    void setHoveredAction(QAction* action);
    void setFocusedAction(QAction* action);
    void clearPressedAction();
    void activateAction(QAction* action);
    void openMenuForAction(QAction* action);
    void closeOpenMenu();

    Typography::FontRole m_fontStyle = Typography::FontRole::Body;
    bool m_backgroundVisible = true;
    mutable QHash<QAction*, QRect> m_actionRects;
    QHash<QAction*, QMetaObject::Connection> m_actionChangedConnections;
    mutable bool m_layoutDirty = true;
    mutable int m_naturalWidth = 0;
    mutable QRect m_overflowRect;
    mutable QList<QAction*> m_overflowActions;
    basicinput::DropDownButton* m_overflowButton = nullptr;
    FluentMenu* m_overflowMenu = nullptr;
    QAction* m_hoveredAction = nullptr;
    QAction* m_pressedAction = nullptr;
    QAction* m_focusedAction = nullptr;
    QAction* m_openAction = nullptr;
};

} // namespace fluent::menus_toolbars

#endif // FLUENT_MENUBAR_H
