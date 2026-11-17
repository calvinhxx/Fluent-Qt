#ifndef COLORPICKER_H
#define COLORPICKER_H

#include <QWidget>
#include <QColor>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::textfields {
class Label;
class LineEdit;
} // namespace fluent::textfields

namespace fluent::basicinput {

class Slider;

/**
 * @brief Fluent color picker with spectrum, hue bar, RGB/HSV/Hex inputs, and optional alpha channel.
 * zh_CN: 支持色盘、色相条、RGB/HSV/Hex 输入及可选透明通道的 Fluent 颜色选择器。
 */
class ColorPicker : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT

    using Label = fluent::textfields::Label;
    using LineEdit = fluent::textfields::LineEdit;

    /**
     * @brief Currently selected color value.
     * zh_CN: 当前选中的颜色值。
     */
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    /**
     * @brief Whether alpha channel editing is available.
     * zh_CN: 是否允许编辑 Alpha 透明通道。
     */
    Q_PROPERTY(bool alphaEnabled READ alphaEnabled WRITE setAlphaEnabled NOTIFY alphaEnabledChanged)

public:
    explicit ColorPicker(QWidget* parent = nullptr);

    QColor color() const { return m_color; }
    void setColor(const QColor& c);

    bool alphaEnabled() const { return m_alphaEnabled; }
    void setAlphaEnabled(bool enabled);

    /** @brief Normalized HSV hue in [0.0, 1.0] for internal picker widgets. zh_CN: 供内部子控件读取的归一化色相（[0.0, 1.0]）。 */
    qreal hue() const { return m_h; }
    /** @brief Normalized HSV saturation in [0.0, 1.0]. zh_CN: 归一化饱和度（[0.0, 1.0]）。 */
    qreal saturation() const { return m_s; }
    /** @brief Normalized HSV brightness value in [0.0, 1.0]. zh_CN: 归一化明度（[0.0, 1.0]）。 */
    qreal value() const { return m_v; }

    void onThemeUpdated() override;

signals:
    void colorChanged(const QColor& color);
    void alphaEnabledChanged(bool enabled);

private slots:
    void handleChannelEdited();
    void handleHexEdited();

public:
    /** @brief Updates HSV hue from the internal hue bar ([0.0, 1.0]). zh_CN: 内部色相条回调，更新色相（[0.0, 1.0]）。 */
    void setHueFromBar(qreal h);
    /** @brief Updates saturation and brightness from the spectrum ([0.0, 1.0]). zh_CN: 更新饱和度与明度。 */
    void setSVFromSpectrum(qreal s, qreal v);
    /** @brief Updates HSV brightness from the internal value slider (0-100). zh_CN: 内部明度滑块回调（0-100）。 */
    void setValueFromSlider(int percent);
    /** @brief Updates alpha channel from the internal alpha slider (0-255). zh_CN: 内部透明度滑块回调（0-255）。 */
    void setAlphaFromSlider(int alpha);

private:
    void initUi();
    void updateFromColor();
    QString colorToHex(const QColor& c, bool withAlpha) const;
    QColor hexToColor(const QString& text, bool* ok) const;

    QColor m_color;
    bool m_alphaEnabled = true;

    // HSV components kept in sync with m_color.
    qreal m_h = 0.0;
    qreal m_s = 0.0;
    qreal m_v = 1.0;

    QWidget* m_spectrum = nullptr;
    QWidget* m_hueBar = nullptr;
    QWidget* m_previewPane = nullptr;
    Slider* m_valueSlider = nullptr;
    Slider* m_alphaSlider = nullptr;
    QWidget* m_alphaRowWidget = nullptr;
    QWidget* m_alphaInputRowWidget = nullptr;

    LineEdit* m_rEdit = nullptr;
    LineEdit* m_gEdit = nullptr;
    LineEdit* m_bEdit = nullptr;
    LineEdit* m_aEdit = nullptr;

    LineEdit* m_hexEdit = nullptr;

    bool m_isInternalUpdate = false;
};

} // namespace fluent::basicinput

#endif // COLORPICKER_H
