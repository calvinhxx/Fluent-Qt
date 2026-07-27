#ifndef FLUENTQT_COMPONENTS_LAYOUT_DIVIDER_H
#define FLUENTQT_COMPONENTS_LAYOUT_DIVIDER_H

#include <QColor>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::layout {

/**
 * @brief DPI-aligned horizontal or vertical Fluent separator.
 * zh_CN: 支持 DPI 对齐的横向或纵向 Fluent 分隔线。
 */
class Divider : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation
                   NOTIFY orientationChanged)
    Q_PROPERTY(int leadingInset READ leadingInset WRITE setLeadingInset
                   NOTIFY leadingInsetChanged)
    Q_PROPERTY(int trailingInset READ trailingInset WRITE setTrailingInset
                   NOTIFY trailingInsetChanged)
    Q_PROPERTY(qreal thickness READ thickness WRITE setThickness
                   NOTIFY thicknessChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

public:
    explicit Divider(QWidget* parent = nullptr);
    explicit Divider(Qt::Orientation orientation, QWidget* parent = nullptr);

    Qt::Orientation orientation() const { return m_orientation; }
    void setOrientation(Qt::Orientation orientation);

    int leadingInset() const { return m_leadingInset; }
    void setLeadingInset(int inset);

    int trailingInset() const { return m_trailingInset; }
    void setTrailingInset(int inset);

    qreal thickness() const { return m_thickness; }
    void setThickness(qreal thickness);

    QColor color() const { return m_color; }
    void setColor(const QColor& color);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

signals:
    void orientationChanged(Qt::Orientation orientation);
    void leadingInsetChanged(int inset);
    void trailingInsetChanged(int inset);
    void thicknessChanged(qreal thickness);
    void colorChanged(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor resolvedColor() const;

    Qt::Orientation m_orientation = Qt::Horizontal;
    int m_leadingInset = 0;
    int m_trailingInset = 0;
    qreal m_thickness = 1.0;
    QColor m_color;
};

} // namespace fluent::layout

#endif // FLUENTQT_COMPONENTS_LAYOUT_DIVIDER_H
