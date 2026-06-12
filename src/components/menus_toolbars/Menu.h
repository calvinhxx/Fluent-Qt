#ifndef FLUENT_MENU_H
#define FLUENT_MENU_H

#include <QMenu>
#include <QWidgetAction>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "design/Spacing.h"

class QActionEvent;
class QKeyEvent;
class QMouseEvent;

namespace fluent::menus_toolbars {

/**
 * @brief Menu item action with Fluent typography metadata.
 * zh_CN: 带 Fluent 排版元数据的菜单项 action。
 *
 * FluentMenuItem lets menu rows resolve text styling from theme font roles while
 * keeping QAction behavior and menu ownership intact.
 * zh_CN: FluentMenuItem 允许菜单行从主题字体角色解析文本样式，同时保留 QAction 行为和菜单所有权。
 */
class FluentMenuItem : public QWidgetAction, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Fluent typography style used by menu text.
     * zh_CN: 菜单文本使用的 Fluent 排版样式。
     */
    Q_PROPERTY(QString fontStyle READ fontStyle WRITE setFontStyle NOTIFY fontStyleChanged)
public:
    explicit FluentMenuItem(const QString& text, QObject* parent = nullptr);

    /** @brief 设置菜单项的字体样式，对应 themeFont() 的 style 参数，默认 "Body"。 */
    void setFontStyle(const QString& style);
    QString fontStyle() const { return m_fontStyle; }

    void onThemeUpdated() override;

signals:
    void fontStyleChanged();

private:
    QString m_fontStyle = QStringLiteral("Body");
};

/**
 * @brief Fluent-styled QMenu with token-driven item typography.
 * zh_CN: 使用 token 驱动菜单项排版的 Fluent QMenu。
 *
 * FluentMenu exposes FluentElement access so menu styling can follow the same
 * theme update path as widget-based controls.
 * zh_CN: FluentMenu 暴露 FluentElement 能力，使菜单样式跟随 QWidget 控件相同的主题更新路径。
 */
class FluentMenu : public QMenu, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Fluent typography style used by menu text.
     * zh_CN: 菜单文本使用的 Fluent 排版样式。
     */
    Q_PROPERTY(QString fontStyle READ fontStyle WRITE setFontStyle NOTIFY fontStyleChanged)
public:
    explicit FluentMenu(const QString& title, QWidget* parent = nullptr);

    /** @brief 设置菜单的字体样式，对应 themeFont() 的 style 参数，默认 "Body"。 */
    void setFontStyle(const QString& style);
    QString fontStyle() const { return m_fontStyle; }

    QString shortcutTextForAction(QAction* action) const;
    QRect itemShortcutGeometry(QAction* action) const;
    QRect itemSubmenuIndicatorGeometry(QAction* action) const;

    void onThemeUpdated() override;
    QSize sizeHint() const override;

signals:
    void fontStyleChanged();

protected:
    void actionEvent(QActionEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void normalizePopupLayering();
    void drawShadow(QPainter& painter, const QRect& contentRect);

    // Matches the drawShadow spread with a little headroom for a natural fade. zh_CN: 与 drawShadow 扩散范围一致，略留余量自然淡出。
    const int m_shadowSize = ::Spacing::Standard;

    QString m_fontStyle = QStringLiteral("Body");
    qreal m_revealProgress = 1.0;
};

} // namespace fluent::menus_toolbars

#endif // FLUENT_MENU_H
