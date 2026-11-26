#ifndef FLUENTQT_COMPONENTS_BASICINPUT_COMPOUNDBUTTON_H
#define FLUENTQT_COMPONENTS_BASICINPUT_COMPOUNDBUTTON_H

#include <QString>

#include "Button.h"

namespace fluent::basicinput {

/**
 * @brief Push button with a primary label and a display-only secondary description.
 * zh_CN: 带主标题和次级说明文字（仅展示，不单独交互）的按钮。
 */
class CompoundButton : public Button {
    Q_OBJECT
    Q_PROPERTY(QString secondaryText READ secondaryText WRITE setSecondaryText NOTIFY secondaryTextChanged)

public:
    explicit CompoundButton(QWidget* parent = nullptr);
    explicit CompoundButton(const QString& text, QWidget* parent = nullptr);
    explicit CompoundButton(const QString& text, const QString& secondaryText, QWidget* parent = nullptr);

    QString secondaryText() const { return m_secondaryText; }
    void setSecondaryText(const QString& text);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

signals:
    void secondaryTextChanged(const QString& text);

protected:
    QSize stackedContentSize() const override;
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
