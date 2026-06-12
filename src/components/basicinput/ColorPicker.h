#ifndef COLORPICKER_H
#define COLORPICKER_H

#include <QWidget>
#include <QColor>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::textfields { class Label; class LineEdit; }

namespace fluent::basicinput {

class Slider;

/**
 * @brief Fluent color picker for RGB, HSV, hex, and optional alpha editing.
 * zh_CN: 支持 RGB、HSV、Hex 和可选 Alpha 编辑的 Fluent 颜色选择器。
 *
 * ColorPicker keeps QColor as the public value while synchronizing spectrum,
 * sliders, preview, and text fields behind one component API.
 * zh_CN: ColorPicker 以 QColor 作为公开值，并在组件内部同步色谱、滑块、预览和文本输入。
 */
class ColorPicker : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT

    using Label = fluent::textfields::Label;
    using LineEdit  = fluent::textfields::LineEdit;

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

    // HSV state exposed to the internal child widgets. zh_CN: 提供给内部子控件访问的 HSV 信息。
    qreal hue() const { return m_h; }          // 0.0 - 1.0
    qreal saturation() const { return m_s; }   // 0.0 - 1.0
    qreal value() const { return m_v; }        // 0.0 - 1.0

    void onThemeUpdated() override;

signals:
    void colorChanged(const QColor& color);
    void alphaEnabledChanged(bool enabled);

private slots:
    void handleChannelEdited();
    void handleHexEdited();

public: // For the internal child widgets. zh_CN: 供内部子控件调用。
    // HSV updates from the hue bar / spectrum. zh_CN: 从色相条/色谱更新 HSV。
    void setHueFromBar(qreal h);          // 0.0 - 1.0
    void setSVFromSpectrum(qreal s, qreal v); // 0.0 - 1.0
    void setValueFromSlider(int percent);   // 0-100 → m_v
    void setAlphaFromSlider(int alpha);     // 0-255

private:
    void initUi();
    void updateFromColor();
    QString colorToHex(const QColor& c, bool withAlpha) const;
    QColor hexToColor(const QString& text, bool* ok) const;

    QColor m_color;
    bool m_alphaEnabled = true;

    // HSV components, kept in sync with m_color. zh_CN: HSV 分量（与 m_color 同步）。
    qreal m_h = 0.0;
    qreal m_s = 0.0;
    qreal m_v = 1.0;

    QWidget* m_spectrum = nullptr;
    QWidget* m_hueBar = nullptr;
    QWidget* m_previewPane = nullptr;   // Live preview pane (checkerboard + alpha). zh_CN: 右侧实时颜色预览。
    Slider* m_valueSlider = nullptr;    // Value (V) 0-100. zh_CN: 明度。
    Slider* m_alphaSlider = nullptr;    // Alpha 0-255
    QWidget* m_alphaRowWidget = nullptr;     // Alpha slider row container. zh_CN: Alpha Slider 行容器。
    QWidget* m_alphaInputRowWidget = nullptr; // Alpha input row container. zh_CN: Alpha 输入行容器。

    LineEdit* m_rEdit = nullptr;
    LineEdit* m_gEdit = nullptr;
    LineEdit* m_bEdit = nullptr;
    LineEdit* m_aEdit = nullptr;

    LineEdit* m_hexEdit = nullptr;

    bool m_isInternalUpdate = false;
};

} // namespace fluent::basicinput

#endif // COLORPICKER_H

