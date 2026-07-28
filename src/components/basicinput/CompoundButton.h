#ifndef FLUENTQT_COMPONENTS_BASICINPUT_COMPOUNDBUTTON_H
#define FLUENTQT_COMPONENTS_BASICINPUT_COMPOUNDBUTTON_H

#include <QString>

#include "components/basicinput/Button.h"

namespace fluent::basicinput {

/**
 * @brief Button with a primary label and a secondary description.
 * zh_CN: 同时呈现主标签与次级说明的按钮。
 *
 * CompoundButton preserves Button's click, keyboard, focus, icon, style, and
 * interaction contracts. The secondary text only changes content measurement
 * and painting; it does not introduce a second interactive target.
 * zh_CN: CompoundButton 保留 Button 的点击、键盘、焦点、图标、样式与交互契约。
 * 次级文本只参与内容测量和绘制，不会引入第二个可交互目标。
 */
class CompoundButton : public Button {
    Q_OBJECT
    Q_PROPERTY(QString secondaryText READ secondaryText
                   WRITE setSecondaryText NOTIFY secondaryTextChanged)

public:
    explicit CompoundButton(QWidget* parent = nullptr);
    explicit CompoundButton(const QString& text,
                            QWidget* parent = nullptr);
    explicit CompoundButton(const QString& text,
                            const QString& secondaryText,
                            QWidget* parent = nullptr);

    QString secondaryText() const { return m_secondaryText; }
    void setSecondaryText(const QString& text);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

signals:
    void secondaryTextChanged(const QString& text);

protected:
    QRectF contentPaintRect(const QRectF& surfaceRect) const override;
    void paintEvent(QPaintEvent* event) override;

private:
    QFont secondaryFont() const;
    QRectF secondaryPaintRect(const QRectF& surfaceRect) const;
    QColor secondaryTextColor() const;

    QString m_secondaryText;
};

} // namespace fluent::basicinput

#endif // FLUENTQT_COMPONENTS_BASICINPUT_COMPOUNDBUTTON_H
