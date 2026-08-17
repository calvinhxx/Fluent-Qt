#include "ToggleButton.h"
#include "design/CornerRadius.h"

#include <QPainter>
#include <QScopedValueRollback>

namespace fluent::basicinput {

ToggleButton::ToggleButton(const QString& text, QWidget* parent)
    : Button(text, parent) {
    setCheckable(true);
    // Keep m_checkState in sync via the toggled signal. zh_CN: 连接 toggled 信号同步 m_checkState。
    connect(this, &QPushButton::toggled, this, [this](bool checked) {
        if (m_syncingCheckedState)
            return;
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
        const bool checked = m_checkState != Qt::Unchecked;
        if (isChecked() != checked) {
            const QScopedValueRollback<bool> syncingGuard(
                m_syncingCheckedState, true);
            setChecked(checked);
        }
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
    // The indeterminate state uses a plain Button plus a bottom accent bar.
    // zh_CN: 中间态使用普通 Button 加底部强调色指示条。
    if (m_threeState && m_checkState == Qt::PartiallyChecked) {
        Button::paintEvent(event);

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto& colors = themeColorsRef();

        // A small bottom bar marks the indeterminate state. zh_CN: 底部小横条表示中间态。
        int barHeight = 2;
        int barWidth = width() / 2;
        QRect barRect((width() - barWidth) / 2, height() - barHeight - 4, barWidth, barHeight);
        p.setPen(Qt::NoPen);
        p.setBrush(colors.accentDefault);
        p.drawRoundedRect(barRect, ::CornerRadius::Indicator, ::CornerRadius::Indicator);
        return;
    }

    Button::paintEvent(event);
}

} // namespace fluent::basicinput
