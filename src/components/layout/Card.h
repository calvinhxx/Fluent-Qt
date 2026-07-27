#ifndef FLUENTQT_COMPONENTS_LAYOUT_CARD_H
#define FLUENTQT_COMPONENTS_LAYOUT_CARD_H

#include <QFrame>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::layout {

/**
 * @brief Non-interactive Fluent surface for grouping caller-owned content.
 * zh_CN: 用于组织调用方内容的非交互式 Fluent 表面。
 *
 * Card paints a token-backed rounded fill and optional hairline border. It
 * intentionally relies on normal QWidget layouts instead of owning a separate
 * content widget, so child ownership and composition stay explicit.
 * zh_CN: Card 绘制由 token 驱动的圆角填充和可选细边框，并直接使用常规 QWidget
 * 布局，不额外接管内容控件，从而保持子控件所有权与组合关系清晰。
 */
class Card : public QFrame, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(Appearance appearance READ appearance WRITE setAppearance
                   NOTIFY appearanceChanged)
    Q_PROPERTY(bool borderVisible READ isBorderVisible WRITE setBorderVisible
                   NOTIFY borderVisibleChanged)

public:
    /**
     * @brief Token surface used to fill the card.
     * zh_CN: Card 填充所使用的 token 表面。
     */
    enum Appearance {
        Layer,
        LayerAlt,
        Canvas
    };
    Q_ENUM(Appearance)

    explicit Card(QWidget* parent = nullptr);

    Appearance appearance() const { return m_appearance; }
    void setAppearance(Appearance appearance);

    bool isBorderVisible() const { return m_borderVisible; }
    void setBorderVisible(bool visible);

    void onThemeUpdated() override;

signals:
    void appearanceChanged(Appearance appearance);
    void borderVisibleChanged(bool visible);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor surfaceColor() const;
    void publishSurfaceColor();

    Appearance m_appearance = Layer;
    bool m_borderVisible = true;
};

} // namespace fluent::layout

#endif // FLUENTQT_COMPONENTS_LAYOUT_CARD_H
