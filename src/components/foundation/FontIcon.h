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
 * @brief Theme-aware icon glyph with bundled or application-provided fonts.
 * zh_CN: 使用内置或应用自定义字体、支持主题颜色的图标字形控件。
 *
 * FontIcon accepts either a semantic glyph, a bundled catalog name, or a
 * catalog glyph and resolves the correct native design-size variant before
 * painting.
 * zh_CN: FontIcon 可接收语义字形、内置目录名称或目录字形，并在绘制前解析到
 * 对应的原生光学尺寸变体。
 * An explicit fontFamily paints the supplied glyph at iconSize without catalog
 * lookup or optical-size substitution. Applications own font registration.
 * zh_CN: 显式设置 fontFamily 后，按 iconSize 直接绘制字形，不查询内置目录或替换
 * 光学尺寸变体。字体注册由应用负责。
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

    /**
     * @brief Returns the explicit font family, or empty for bundled Fluent icons.
     * zh_CN: 返回显式字体族；空字符串表示使用内置 Fluent 图标。
     */
    QString fontFamily() const { return m_fontFamily; }

    /**
     * @brief Selects an application-registered font; empty restores bundled icons.
     * zh_CN: 选择应用已注册的字体；空字符串恢复内置图标。
     *
     * Pass the actual glyph string for a custom family, not a catalog name.
     * Register fonts with QFontDatabase and check applicationFontFamilies() first.
     * zh_CN: 自定义字体应传入真实字形字符串，而非目录名称。应用应先通过
     * QFontDatabase 注册字体并检查 applicationFontFamilies() 的结果。
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
