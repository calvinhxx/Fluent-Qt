#ifndef FLUENT_MENU_H
#define FLUENT_MENU_H

#include "../../compatibility/InputEventCompat.h"
#include <QMenu>
#include <QHash>
#include <QPointer>
#include <QWidgetAction>
#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"
#include "../../design/Spacing.h"

class QActionEvent;
class QKeyEvent;
class QMouseEvent;
class QResizeEvent;

namespace fluent::menus_toolbars {

/**
 * @brief Menu item action styled with a Fluent font role while keeping standard QAction behavior.
 * zh_CN: 使用 Fluent 字号样式的菜单动作项，在保留标准 QAction 行为的同时按主题字号解析文本样式。
 */
class FluentMenuItem : public QWidgetAction, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Fluent font style used by menu text.
     * zh_CN: 菜单文本使用的字号样式。
     */
    Q_PROPERTY(Typography::FontRole fontStyle READ fontStyle WRITE setFontStyle NOTIFY fontStyleChanged)
public:
    explicit FluentMenuItem(const QString& text, QObject* parent = nullptr);

    /**
     * @brief Sets the Fluent font role (default Typography::FontRole::Body).
     * zh_CN: 设置菜单项使用的字号样式，默认为 Typography::FontRole::Body。
     */
    void setFontStyle(Typography::FontRole role);
    Typography::FontRole fontStyle() const { return m_fontStyle; }

    void onThemeUpdated() override;

signals:
    void fontStyleChanged();

private:
    Typography::FontRole m_fontStyle = Typography::FontRole::Body;
};

/**
 * @brief Fluent-styled QMenu with token-driven typography and custom shadow painting.
 * zh_CN: Fluent 样式菜单，支持主题字号样式以及圆角与阴影绘制。
 */
class FluentMenu : public QMenu, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Fluent font style used by menu text.
     * zh_CN: 菜单文本使用的字号样式。
     */
    Q_PROPERTY(Typography::FontRole fontStyle READ fontStyle WRITE setFontStyle NOTIFY fontStyleChanged)
public:
    explicit FluentMenu(const QString& title, QWidget* parent = nullptr);

    /**
     * @brief Sets the Fluent font role (default Typography::FontRole::Body).
     * zh_CN: 设置菜单文本使用的字号样式，默认为 Typography::FontRole::Body。
     */
    void setFontStyle(Typography::FontRole role);
    Typography::FontRole fontStyle() const { return m_fontStyle; }

    QString shortcutTextForAction(QAction* action) const;
    QRect itemShortcutGeometry(QAction* action) const;
    QRect itemSubmenuIndicatorGeometry(QAction* action) const;

    void onThemeUpdated() override;
    QSize sizeHint() const override;

signals:
    void fontStyleChanged();

protected:
    void setItemLayoutMetrics(int verticalPadding, int separatorHeight);
    void actionEvent(QActionEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void syncAutoMinimumWidth();
    QHash<QAction*, QMetaObject::Connection> m_actionTriggerConnections;
    void updateSurfaceMask();
    void normalizePopupLayering();
    void positionCascade();

    QPointer<QMenu> m_cascadeParent;

    // Alpha-capable popups reserve painted shadow padding; opaque fallbacks use the full rect.
    int m_shadowSize = ::Spacing::Standard;
    bool m_translucentSurface = true;
    bool m_surfaceMaskApplied = false;
    int m_autoMinimumWidth = 0;
    int m_itemVerticalPadding = ::Spacing::Padding::ListItemVertical;
    int m_separatorHeight = ::Spacing::Gap::Normal + 1;

    Typography::FontRole m_fontStyle = Typography::FontRole::Body;
    qreal m_revealProgress = 1.0;
};

} // namespace fluent::menus_toolbars

#endif // FLUENT_MENU_H
