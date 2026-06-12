#include "ToggleButton.h"
#include "design/CornerRadius.h"

namespace fluent::basicinput {

ToggleButton::ToggleButton(const QString& text, QWidget* parent)
    : Button(text, parent) {
    setCheckable(true);
    // Keep m_checkState in sync via the toggled signal. zh_CN: 连接 toggled 信号同步 m_checkState。
    connect(this, &QPushButton::toggled, this, [this](bool checked) {
        setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    });
}

ToggleButton::ToggleButton(QWidget* parent)
    : ToggleButton("", parent) {
}

void ToggleButton::setThreeState(bool threeState) {
    if (m_threeState != threeState) {
        m_threeState = threeState;
        emit threeStateChanged();
    }
}

Qt::CheckState ToggleButton::checkState() const {
    return m_checkState;
}

void ToggleButton::setCheckState(Qt::CheckState state) {
    if (m_checkState != state) {
        m_checkState = state;
        setChecked(m_checkState != Qt::Unchecked);
        update();
        emit checkStateChanged(m_checkState);
    }
}

void ToggleButton::nextCheckState() {
    if (m_threeState) {
        // Unchecked -> Checked -> PartiallyChecked -> Unchecked
        if (m_checkState == Qt::Unchecked) setCheckState(Qt::Checked);
        else if (m_checkState == Qt::Checked) setCheckState(Qt::PartiallyChecked);
        else setCheckState(Qt::Unchecked);
    } else {
        Button::nextCheckState();
    }
}

void ToggleButton::onThemeUpdated() {
    Button::onThemeUpdated();
}

void ToggleButton::paintEvent(QPaintEvent* event) {
    // The indeterminate state needs its own visual treatment.
    // zh_CN: 中间态需要单独的绘制处理。
    if (m_threeState && m_checkState == Qt::PartiallyChecked) {
        // Rendered as the plain button plus a bottom accent bar; a translucent
        // accent fill would be an alternative.
        // zh_CN: 以普通按钮加底部 Accent 指示条呈现；半透明 Accent 背景是可选方案。
        Button::paintEvent(event);
        
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto& colors = themeColors();
        const auto& radius = themeRadius();
        
        // A small bottom bar marks the indeterminate state. zh_CN: 底部小横条表示中间态。
        int barHeight = 2;
        int barWidth = width() / 2;
        QRect barRect((width() - barWidth) / 2, height() - barHeight - 4, barWidth, barHeight);
        p.setPen(Qt::NoPen);
        p.setBrush(colors.accentDefault);
        p.drawRoundedRect(barRect, ::CornerRadius::Indicator, ::CornerRadius::Indicator);
    } else {
        Button::paintEvent(event);
    }
}

} // namespace fluent::basicinput
