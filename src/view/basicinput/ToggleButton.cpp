#include "ToggleButton.h"

namespace view::basicinput {

ToggleButton::ToggleButton(const QString& text, QWidget* parent)
    : Button(text, parent) {
    setCheckable(true);
    // 连接 toggled 信号来同步 m_checkState
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
    // 如果是中间态，我们需要特殊的绘制逻辑，或者稍微修改颜色
    if (m_threeState && m_checkState == Qt::PartiallyChecked) {
        // 这里可以实现中间态的视觉效果，例如在按钮底部显示一个 Accent 色的指示条
        // 或者使用半透明的 Accent 背景。为了简单起见，我们暂时让它表现得像 Accent 悬停态。
        Button::paintEvent(event);
        
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto& colors = themeColors();
        const auto& radius = themeRadius();
        
        // 在底部画一个小横条表示中间态
        int barHeight = 2;
        int barWidth = width() / 2;
        QRect barRect((width() - barWidth) / 2, height() - barHeight - 4, barWidth, barHeight);
        p.setPen(Qt::NoPen);
        p.setBrush(colors.accentDefault);
        p.drawRoundedRect(barRect, 1, 1);
    } else {
        Button::paintEvent(event);
    }
}

} // namespace view::basicinput
