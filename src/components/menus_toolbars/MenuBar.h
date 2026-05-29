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

namespace fluent::menus_toolbars {

class FluentMenu;

/**
 * @brief Fluent-styled menu bar with configurable font role.
 * zh_CN: 支持配置字体角色的 Fluent 菜单栏。
 *
 * FluentMenuBar keeps QMenuBar behavior while aligning menu-bar typography and
 * theme updates with the rest of the component library.
 * zh_CN: FluentMenuBar 保留 QMenuBar 行为，并让菜单栏排版和主题更新与组件库保持一致。
 */
class FluentMenuBar : public QMenuBar, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Fluent typography style used by menu text.
     * zh_CN: 菜单文本使用的 Fluent 排版样式。
     */
    Q_PROPERTY(QString fontStyle READ fontStyle WRITE setFontStyle NOTIFY fontStyleChanged)
public:
    explicit FluentMenuBar(QWidget* parent = nullptr);

    void setFontStyle(const QString& style);
    QString fontStyle() const { return m_fontStyle; }

    QRect fluentActionGeometry(QAction* action) const;
    QAction* hoveredAction() const { return m_hoveredAction; }
    QAction* focusedAction() const { return m_focusedAction; }
    QAction* openAction() const { return m_openAction; }

    void onThemeUpdated() override;
    QSize sizeHint() const override;

signals:
    void fontStyleChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
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
    QAction* actionAt(const QPoint& position, bool enabledOnly) const;
    QAction* firstEnabledAction() const;
    QAction* nextEnabledAction(QAction* from, int direction) const;
    void setHoveredAction(QAction* action);
    void setFocusedAction(QAction* action);
    void clearPressedAction();
    void activateAction(QAction* action);
    void openMenuForAction(QAction* action);
    void closeOpenMenu();

    QString m_fontStyle = QStringLiteral("Body");
    mutable QHash<QAction*, QRect> m_actionRects;
    mutable bool m_layoutDirty = true;
    QAction* m_hoveredAction = nullptr;
    QAction* m_pressedAction = nullptr;
    QAction* m_focusedAction = nullptr;
    QAction* m_openAction = nullptr;
};

} // namespace fluent::menus_toolbars

#endif // FLUENT_MENUBAR_H
