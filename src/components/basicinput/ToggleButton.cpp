#include "ToggleButton.h"
#include "design/CornerRadius.h"

#include <QMetaType>
#include <QPainter>
#include <QPointer>

namespace fluent::basicinput {

ToggleButton::ToggleButton(const QString& text, QWidget* parent) : Button(text, parent)
{
    qRegisterMetaType<Qt::CheckState>("Qt::CheckState");
    setCheckable(true);
    connect(this, &QPushButton::toggled, this, [this](bool checked) {
        if (m_syncingCheckedState)
            return;
        setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    });
}

ToggleButton::ToggleButton(QWidget* parent) : ToggleButton("", parent) {}

void ToggleButton::setThreeState(bool threeState)
{
    if (m_threeState != threeState) {
        m_threeState = threeState;
        emit threeStateChanged();
    }
}

Qt::CheckState ToggleButton::checkState() const
{
    return m_checkState;
}

void ToggleButton::setCheckState(Qt::CheckState state)
{
    if (m_checkState == state)
        return;

    m_checkState = state;
    const bool checked = state != Qt::Unchecked;
    if (isChecked() != checked) {
        const bool wasSyncing = m_syncingCheckedState;
        m_syncingCheckedState = true;
        QPointer<ToggleButton> guard(this);

        // Complete QPushButton::setChecked before emitting toggled when ungrouped, in case a slot destroys this button.
        const bool stageToggled = !group() && !autoExclusive();
        const bool wasBlocked = signalsBlocked();
        if (stageToggled)
            blockSignals(true);
        setChecked(checked);
        if (!guard)
            return;
        if (stageToggled) {
            blockSignals(wasBlocked);
            if (isChecked() == checked)
                emit toggled(checked);
            if (!guard)
                return;
        }
        m_syncingCheckedState = wasSyncing;

        // Re-sync if a toggled() slot modified checked or checkState.
        if (isChecked() != (m_checkState != Qt::Unchecked)) {
            setCheckState(isChecked() ? Qt::Checked : Qt::Unchecked);
            return;
        }
        if (m_checkState != state)
            return;
    }
    update();
    emit checkStateChanged(m_checkState);
}

void ToggleButton::nextCheckState()
{
    if (m_threeState) {
        // Unchecked -> Checked -> PartiallyChecked -> Unchecked
        if (m_checkState == Qt::Unchecked)
            setCheckState(Qt::Checked);
        else if (m_checkState == Qt::Checked)
            setCheckState(Qt::PartiallyChecked);
        else
            setCheckState(Qt::Unchecked);
    } else {
        if (group() || autoExclusive())
            Button::nextCheckState();
        else if (isCheckable())
            setCheckState(isChecked() ? Qt::Unchecked : Qt::Checked);
    }
}

void ToggleButton::onThemeUpdated()
{
    Button::onThemeUpdated();
}

void ToggleButton::paintEvent(QPaintEvent* event)
{
    // PartiallyChecked paints a normal button with a bottom accent indicator bar.
    if (m_threeState && m_checkState == Qt::PartiallyChecked) {
        Button::paintEvent(event);

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto& colors = themeColorsRef();

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
