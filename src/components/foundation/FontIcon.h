#ifndef FLUENTQT_COMPONENTS_FOUNDATION_FONTICON_H
#define FLUENTQT_COMPONENTS_FOUNDATION_FONTICON_H

#include <QColor>
#include <QString>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "design/Typography.h"

namespace fluent {

/**
 * @brief Theme-aware Fluent icon glyph with optical-size selection.
 * zh_CN: 支持主题颜色和光学尺寸选择的 Fluent 图标字形控件。
 *
 * FontIcon accepts either a semantic glyph, a bundled catalog name, or a
 * catalog glyph and resolves the correct native design-size variant before
 * painting.
 * zh_CN: FontIcon 可接收语义字形、内置目录名称或目录字形，并在绘制前解析到
 * 对应的原生光学尺寸变体。
 */
class FontIcon : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QString glyph READ glyph WRITE setGlyph NOTIFY glyphChanged)
    Q_PROPERTY(int iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation NOTIFY rotationChanged)

public:
    explicit FontIcon(QWidget* parent = nullptr);
    explicit FontIcon(const QString& glyph, QWidget* parent = nullptr);

    QString glyph() const { return m_glyph; }
    void setGlyph(const QString& glyph);

    int iconSize() const { return m_iconSize; }
    void setIconSize(int size);

    QColor color() const { return m_color; }
    void setColor(const QColor& color);

    qreal rotation() const { return m_rotation; }
    void setRotation(qreal degrees);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

signals:
    void glyphChanged(const QString& glyph);
    void iconSizeChanged(int size);
    void colorChanged(const QColor& color);
    void rotationChanged(qreal degrees);

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    QColor resolvedColor() const;

    QString m_glyph;
    int m_iconSize = Typography::IconSize::Standard;
    QColor m_color;
    qreal m_rotation = 0.0;
};

} // namespace fluent

#endif // FLUENTQT_COMPONENTS_FOUNDATION_FONTICON_H
