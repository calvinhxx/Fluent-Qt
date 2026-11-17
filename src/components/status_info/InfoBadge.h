#ifndef INFOBADGE_H
#define INFOBADGE_H

#include <QColor>
#include <QPoint>
#include <QString>
#include <QWidget>

#include "design/Typography.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QPainter;
class QRectF;
class QHideEvent;
class QShowEvent;

namespace fluent::status_info {

/**
 * @brief Compact Fluent badge for counts, icons, status dots, or alerts.
 * zh_CN: 紧凑 Fluent 徽标，用于显示数字计数、图标、状态点或提醒。
 *
 * Supports numeric values, icon glyphs, dot indicators, status colors, custom
 * background/text colors, and badge opacity.
 * zh_CN: 支持数值、图标字符、提示点模式、状态配色、自定义颜色与不透明度配置。
 */
class InfoBadge : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Numeric badge value shown when display mode resolves to Value.
     * zh_CN: 显示模式解析为 Value 时展示的数字徽标值。
     */
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    /**
     * @brief Icon glyph shown when display mode resolves to Icon.
     * zh_CN: 显示模式解析为 Icon 时展示的图标字符。
     */
    Q_PROPERTY(QString iconGlyph READ iconGlyph WRITE setIconGlyph NOTIFY iconGlyphChanged)
    /**
     * @brief Content mode used to choose dot, icon, value, or automatic badge rendering.
     * zh_CN: 用于选择提示点、图标、数值或自动徽标绘制的内容模式。
     */
    Q_PROPERTY(InfoBadgeDisplayMode displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    /**
     * @brief Status level determining the badge color.
     * zh_CN: 决定徽标配色的状态级别。
     */
    Q_PROPERTY(InfoBadgeStatus status READ status WRITE setStatus NOTIFY statusChanged)
    /**
     * @brief Opacity applied to the badge visual.
     * zh_CN: 应用到徽标视觉的不透明度。
     */
    Q_PROPERTY(qreal badgeOpacity READ badgeOpacity WRITE setBadgeOpacity NOTIFY badgeOpacityChanged)
    /**
     * @brief Custom badge background color override.
     * zh_CN: 徽标背景色自定义覆盖值。
     */
    Q_PROPERTY(QColor customBackgroundColor
        READ customBackgroundColor
        WRITE setCustomBackgroundColor
        NOTIFY customBackgroundColorChanged)
    /**
     * @brief Custom badge text color override.
     * zh_CN: 徽标文本颜色自定义覆盖值。
     */
    Q_PROPERTY(QColor customTextColor READ customTextColor WRITE setCustomTextColor NOTIFY customTextColorChanged)
    /**
     * @brief Typography role used by badge value text.
     * zh_CN: 徽标数值文本使用的排版角色。
     */
    Q_PROPERTY(Typography::FontRole valueFontRole READ valueFontRole WRITE setValueFontRole NOTIFY valueFontRoleChanged)
    /**
     * @brief Diameter of dot-mode badges.
     * zh_CN: 点状徽标的直径。
     */
    Q_PROPERTY(int beaconDiameter READ beaconDiameter WRITE setBeaconDiameter NOTIFY beaconDiameterChanged)
    /**
     * @brief Height of icon or value badges.
     * zh_CN: 图标或数值模式徽标的高度。
     */
    Q_PROPERTY(int badgeHeight READ badgeHeight WRITE setBadgeHeight NOTIFY badgeHeightChanged)
    /**
     * @brief Horizontal padding around badge value text.
     * zh_CN: 徽标数值文本的水平内边距。
     */
    Q_PROPERTY(int valueHorizontalPadding
        READ valueHorizontalPadding
        WRITE setValueHorizontalPadding
        NOTIFY valueHorizontalPaddingChanged)
    /**
     * @brief Icon glyph size in pixels.
     * zh_CN: 图标字符尺寸，单位为像素。
     */
    Q_PROPERTY(int iconGlyphSize READ iconGlyphSize WRITE setIconGlyphSize NOTIFY iconGlyphSizeChanged)
    /**
     * @brief Iconfont family used for glyph rendering.
     * zh_CN: 图标字符绘制使用的 iconfont 字体族。
     */
    Q_PROPERTY(QString iconFontFamily READ iconFontFamily WRITE setIconFontFamily NOTIFY iconFontFamilyChanged)
    /**
     * @brief Inset applied to badge background painting.
     * zh_CN: 徽标背景绘制时应用的内缩量。
     */
    Q_PROPERTY(int badgeBackgroundInset
        READ badgeBackgroundInset
        WRITE setBadgeBackgroundInset
        NOTIFY badgeBackgroundInsetChanged)
    /**
     * @brief Pixel offset applied to badge content drawing.
     * zh_CN: 徽标内容绘制时应用的像素偏移。
     */
    Q_PROPERTY(QPoint contentOffset READ contentOffset WRITE setContentOffset NOTIFY contentOffsetChanged)

public:
    enum class InfoBadgeDisplayMode { Auto, Dot, Icon, Value };
    Q_ENUM(InfoBadgeDisplayMode)

    enum class InfoBadgeStatus { Informational, Attention, Caution, Success, Critical };
    Q_ENUM(InfoBadgeStatus)

    explicit InfoBadge(QWidget* parent = nullptr);

    int value() const { return m_value; }
    void setValue(int value);

    QString iconGlyph() const { return m_iconGlyph; }
    void setIconGlyph(const QString& glyph);

    InfoBadgeDisplayMode displayMode() const { return m_displayMode; }
    void setDisplayMode(InfoBadgeDisplayMode mode);

    InfoBadgeStatus status() const { return m_status; }
    void setStatus(InfoBadgeStatus status);

    qreal badgeOpacity() const { return m_badgeOpacity; }
    void setBadgeOpacity(qreal opacity);

    QColor customBackgroundColor() const { return m_customBackgroundColor; }
    void setCustomBackgroundColor(const QColor& color);

    QColor customTextColor() const { return m_customTextColor; }
    void setCustomTextColor(const QColor& color);

    Typography::FontRole valueFontRole() const { return m_valueFontRole; }
    void setValueFontRole(Typography::FontRole role);

    int beaconDiameter() const { return m_beaconDiameter; }
    void setBeaconDiameter(int diameter);

    int badgeHeight() const { return m_badgeHeight; }
    void setBadgeHeight(int height);

    int valueHorizontalPadding() const { return m_valueHorizontalPadding; }
    void setValueHorizontalPadding(int padding);

    int iconGlyphSize() const { return m_iconGlyphSize; }
    void setIconGlyphSize(int size);

    QString iconFontFamily() const { return m_iconFontFamily; }
    void setIconFontFamily(const QString& family);

    int badgeBackgroundInset() const { return m_badgeBackgroundInset; }
    void setBadgeBackgroundInset(int inset);

    QPoint contentOffset() const { return m_contentOffset; }
    void setContentOffset(const QPoint& offset);

    InfoBadgeDisplayMode effectiveDisplayMode() const;
    QSize effectiveBadgeSize() const;
    QColor effectiveBackgroundColor() const;
    QColor effectiveForegroundColor() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

signals:
    void valueChanged(int value);
    void iconGlyphChanged(const QString& glyph);
    void displayModeChanged(InfoBadgeDisplayMode mode);
    void statusChanged(InfoBadgeStatus status);
    void badgeOpacityChanged(qreal opacity);
    void customBackgroundColorChanged(const QColor& color);
    void customTextColorChanged(const QColor& color);
    void valueFontRoleChanged(Typography::FontRole role);
    void beaconDiameterChanged(int diameter);
    void badgeHeightChanged(int height);
    void valueHorizontalPaddingChanged(int padding);
    void iconGlyphSizeChanged(int size);
    void iconFontFamilyChanged(const QString& family);
    void badgeBackgroundInsetChanged(int inset);
    void contentOffsetChanged(const QPoint& offset);

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QString valueText() const;
    QFont valueFont() const;
    QFont iconFont() const;
    void drawCenteredTextPath(QPainter& painter,
        const QRectF& targetRect,
        const QFont& font,
        const QString& text,
        const QColor& color = QColor()) const;
    QColor statusBackgroundColor() const;
    void updateThemeColors();
    void invalidateLayoutAndPaint();
    void notifyAccessibleValueChanged();
    void notifyAccessibleParentReordered();

    int m_value = -1;
    QString m_iconGlyph;
    InfoBadgeDisplayMode m_displayMode = InfoBadgeDisplayMode::Auto;
    InfoBadgeStatus m_status = InfoBadgeStatus::Attention;
    qreal m_badgeOpacity = 1.0;
    QColor m_customBackgroundColor;
    QColor m_customTextColor;
    Typography::FontRole m_valueFontRole = Typography::FontRole::Caption;
    int m_beaconDiameter = 4;
    int m_badgeHeight = 16;
    int m_valueHorizontalPadding = 8;
    int m_iconGlyphSize = 10;
    QString m_iconFontFamily = Typography::FontFamily::FluentIcons;
    int m_badgeBackgroundInset = 0;
    QPoint m_contentOffset{0, 0};

    QColor m_backgroundColor;
    QColor m_foregroundColor;
    QColor m_disabledBackgroundColor;
    QColor m_disabledForegroundColor;
};

} // namespace fluent::status_info

#endif // INFOBADGE_H
