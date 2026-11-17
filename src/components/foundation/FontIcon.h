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
 * @brief Theme-aware icon widget supporting bundled Fluent icons or custom icon fonts.
 * Accepts an icon name or glyph for bundled icons, or paints directly with fontFamily when set.
 * zh_CN: 支持主题颜色的图标控件，可使用内置 Fluent 图标或应用自定义字体。传入图标名称或内置字形时会自动匹配对应尺寸；显式设置 fontFamily 后则直接按 iconSize 绘制给定字形。
 */
class FontIcon : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QString glyph READ glyph WRITE setGlyph NOTIFY glyphChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)
    Q_PROPERTY(int iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation NOTIFY rotationChanged)

public:
    explicit FontIcon(QWidget* parent = nullptr);
    explicit FontIcon(const QString& glyph, QWidget* parent = nullptr);

    QString glyph() const { return m_glyph; }
    void setGlyph(const QString& glyph);

    QString fontFamily() const { return m_fontFamily; }

    /**
     * @brief Sets an application-registered custom icon font; pass an empty string to restore bundled Fluent icons.
     * When using a custom font, pass the actual glyph string to setGlyph() instead of a catalog icon name.
     * zh_CN: 设置应用已注册的自定义图标字体（须先通过 QFontDatabase 注册），传入空字符串恢复使用内置 Fluent 图标；使用自定义字体时，glyph 须传入真实字符而非图标名称。
     */
    void setFontFamily(const QString& family);

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
    void fontFamilyChanged(const QString& family);
    void iconSizeChanged(int size);
    void colorChanged(const QColor& color);
    void rotationChanged(qreal degrees);

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    QColor resolvedColor() const;

    QString m_glyph;
    QString m_fontFamily;
    int m_iconSize = Typography::IconSize::Standard;
    QColor m_color;
    qreal m_rotation = 0.0;
};

} // namespace fluent

#endif // FLUENTQT_COMPONENTS_FOUNDATION_FONTICON_H
