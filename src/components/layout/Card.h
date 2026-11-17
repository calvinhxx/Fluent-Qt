#ifndef FLUENTQT_COMPONENTS_LAYOUT_CARD_H
#define FLUENTQT_COMPONENTS_LAYOUT_CARD_H

#include <QFrame>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::layout {

/**
 * @brief Rounded Fluent container frame for grouping child widgets with standard layouts.
 * zh_CN: 圆角 Fluent 容器卡片，直接使用常规 QWidget 布局组织子控件。
 */
class Card : public QFrame, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(Appearance appearance READ appearance WRITE setAppearance NOTIFY appearanceChanged)
    Q_PROPERTY(bool borderVisible READ isBorderVisible WRITE setBorderVisible NOTIFY borderVisibleChanged)

public:
    /**
     * @brief Background color token used to fill the card.
     * zh_CN: 卡片背景填充颜色样式。
     */
    enum Appearance { Layer, LayerAlt, Canvas };
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
