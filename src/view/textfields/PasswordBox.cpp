#include "PasswordBox.h"

#include <QEvent>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>

#include "design/Spacing.h"
#include "design/Typography.h"
#include "view/basicinput/Button.h"

namespace view::textfields {

PasswordBox::PasswordBox(QWidget* parent)
    : LineEdit(parent) {
    setAttribute(Qt::WA_Hover);
    setClearButtonEnabled(false);
    setFrameVisible(false);
    setEchoMode(QLineEdit::Password);
    setFixedHeight(totalPreferredHeight());

    initializeRevealButton();

    connect(this, &QLineEdit::textChanged, this, [this](const QString& value) {
        if (value.isEmpty()) setPeekActive(false);
        updateRevealButtonState();
        updateTextMargins();
        emit passwordChanged(value);
    });

    updateHeaderTextMargins();
    updateTextMargins();
    updateRevealButtonState();
    updateEchoMode();
}

void PasswordBox::setPassword(const QString& password) {
    setText(password);
}

void PasswordBox::setHeader(const QString& header) {
    if (m_header == header) return;
    m_header = header;
    setFixedHeight(totalPreferredHeight());
    updateHeaderTextMargins();
    updateRevealButtonGeometry();
    updateGeometry();
    update();
    emit headerChanged();
}

void PasswordBox::setPasswordRevealMode(PasswordRevealMode mode) {
    if (m_revealMode == mode) return;
    m_revealMode = mode;
    if (m_revealMode != PasswordRevealMode::Peek) setPeekActive(false);
    updateEchoMode();
    updateRevealButtonState();
    updateTextMargins();
    update();
    emit passwordRevealModeChanged();
}

QSize PasswordBox::sizeHint() const {
    QSize hint = LineEdit::sizeHint();
    hint.setHeight(totalPreferredHeight());
    hint.setWidth(qMax(hint.width(), 160));
    return hint;
}

QSize PasswordBox::minimumSizeHint() const {
    QSize hint = sizeHint();
    hint.setWidth(qMax(hint.width(), 120));
    return hint;
}

void PasswordBox::onThemeUpdated() {
    LineEdit::onThemeUpdated();
    if (m_revealButton) m_revealButton->onThemeUpdated();
    update();
}

void PasswordBox::paintEvent(QPaintEvent* event) {
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (!m_header.isEmpty()) paintHeader(painter);
        paintInputFrame(painter);
    }
    LineEdit::paintEvent(event);
}

void PasswordBox::resizeEvent(QResizeEvent* event) {
    LineEdit::resizeEvent(event);
    updateRevealButtonGeometry();
}

void PasswordBox::focusInEvent(QFocusEvent* event) {
    m_focused = true;
    LineEdit::focusInEvent(event);
    update();
}

void PasswordBox::focusOutEvent(QFocusEvent* event) {
    m_focused = false;
    setPeekActive(false);
    LineEdit::focusOutEvent(event);
    update();
}

void PasswordBox::enterEvent(FluentEnterEvent* event) {
    m_hovered = true;
    LineEdit::enterEvent(event);
    update();
}

void PasswordBox::leaveEvent(QEvent* event) {
    m_hovered = false;
    LineEdit::leaveEvent(event);
    update();
}

void PasswordBox::mousePressEvent(QMouseEvent* event) {
    if (event && event->button() == Qt::LeftButton && isEnabled() && !isReadOnly()) {
        m_pressed = true;
        update();
    }
    LineEdit::mousePressEvent(event);
}

void PasswordBox::mouseReleaseEvent(QMouseEvent* event) {
    if (m_pressed) {
        m_pressed = false;
        update();
    }
    LineEdit::mouseReleaseEvent(event);
}

void PasswordBox::changeEvent(QEvent* event) {
    LineEdit::changeEvent(event);
    if (!event) return;
    if (event->type() == QEvent::EnabledChange || event->type() == QEvent::ReadOnlyChange) {
        if (!canPeekReveal()) setPeekActive(false);
        updateRevealButtonState();
        updateTextMargins();
        updateEchoMode();
        update();
    }
}

bool PasswordBox::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_revealButton && event) {
        switch (event->type()) {
        case QEvent::Leave:
            setPeekActive(false);
            break;
        case QEvent::MouseButtonRelease:
            setPeekActive(false);
            setFocus(Qt::MouseFocusReason);
            break;
        default:
            break;
        }
    }
    return LineEdit::eventFilter(watched, event);
}

QRect PasswordBox::inputRect() const {
    return QRect(0, inputTop(), width(), kInputHeight);
}

int PasswordBox::inputTop() const {
    return m_header.isEmpty() ? 0 : kHeaderHeight + kHeaderGap;
}

int PasswordBox::totalPreferredHeight() const {
    return inputTop() + kInputHeight;
}

void PasswordBox::initializeRevealButton() {
    m_buttonLayout = new ::view::AnchorLayout(this);

    m_revealButton = new ::view::basicinput::Button(this);
    m_revealButton->setObjectName("PasswordBoxRevealButton");
    m_revealButton->setFluentStyle(::view::basicinput::Button::Subtle);
    m_revealButton->setFluentLayout(::view::basicinput::Button::IconOnly);
    m_revealButton->setFluentSize(::view::basicinput::Button::Small);
    m_revealButton->setFocusPolicy(Qt::NoFocus);
    m_revealButton->setFixedSize(kButtonWidth, kButtonHeight);
    m_revealButton->setIconGlyph(Typography::Icons::View,
                                 Typography::FontSize::Body,
                                 Typography::FontFamily::SegoeFluentIcons);
    m_revealButton->installEventFilter(this);
    m_buttonLayout->addWidget(m_revealButton);

    connect(m_revealButton, &::view::basicinput::Button::pressed, this, [this]() {
        if (!canPeekReveal()) return;
        setFocus(Qt::MouseFocusReason);
        setPeekActive(true);
    });
    connect(m_revealButton, &::view::basicinput::Button::released, this, [this]() {
        setPeekActive(false);
        setFocus(Qt::MouseFocusReason);
    });

    updateRevealButtonGeometry();
}

void PasswordBox::updateRevealButtonGeometry() {
    if (!m_revealButton) return;

    using Edge = ::view::AnchorLayout::Edge;
    const int centerOffset = inputTop() / 2;

    m_revealButton->setFixedSize(kButtonWidth, kButtonHeight);
    m_revealButton->anchors()->right = {this, Edge::Right, -kButtonRightMargin};
    m_revealButton->anchors()->verticalCenter = {this, Edge::VCenter, centerOffset};

    if (m_buttonLayout) {
        m_buttonLayout->invalidate();
        m_buttonLayout->setGeometry(rect());
    }
}

void PasswordBox::updateRevealButtonState() {
    if (!m_revealButton) return;
    const bool visible = m_revealMode == PasswordRevealMode::Peek && canPeekReveal();
    m_revealButton->setVisible(visible);
    m_revealButton->setEnabled(visible);
    m_revealButton->setIconGlyph(m_peekActive ? Typography::Icons::Hide
                                              : Typography::Icons::View,
                                 Typography::FontSize::Body,
                                 Typography::FontFamily::SegoeFluentIcons);
    updateRevealButtonGeometry();
}

void PasswordBox::updateTextMargins() {
    int rightMargin = ::Spacing::Padding::TextFieldHorizontal;
    if (m_revealMode == PasswordRevealMode::Peek && canPeekReveal()) {
        rightMargin += kButtonRightMargin + kButtonWidth + kTextButtonGap;
    }

    QMargins margins = contentMargins();
    margins.setLeft(::Spacing::Padding::TextFieldHorizontal);
    margins.setRight(rightMargin);
    margins.setTop(::Spacing::Padding::TextFieldVertical);
    margins.setBottom(::Spacing::Padding::TextFieldVertical);
    setContentMargins(margins);
}

void PasswordBox::updateHeaderTextMargins() {
    setTextMargins(0, inputTop(), 0, 0);
}

void PasswordBox::updateEchoMode() {
    const bool revealVisible = m_revealMode == PasswordRevealMode::Visible
        || (m_revealMode == PasswordRevealMode::Peek && m_peekActive && canPeekReveal());
    setEchoMode(revealVisible ? QLineEdit::Normal : QLineEdit::Password);
}

void PasswordBox::setPeekActive(bool active) {
    const bool nextActive = active && m_revealMode == PasswordRevealMode::Peek && canPeekReveal();
    if (m_peekActive == nextActive) {
        updateEchoMode();
        updateRevealButtonState();
        return;
    }
    m_peekActive = nextActive;
    updateEchoMode();
    updateRevealButtonState();
    update();
}

bool PasswordBox::canPeekReveal() const {
    return isEnabled() && !isReadOnly() && !text().isEmpty();
}

void PasswordBox::paintInputFrame(QPainter& painter) {
    const auto colors = themeColors();
    const QRectF frameRect = QRectF(inputRect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QColor bgColor;
    QColor borderColor;
    QColor bottomColor;
    int bottomWidth = ::Spacing::Border::Normal;

    if (!isEnabled()) {
        bgColor = colors.controlDisabled;
        borderColor = colors.strokeDivider;
        bottomColor = borderColor;
    } else if (isReadOnly()) {
        bgColor = colors.controlAltSecondary;
        borderColor = colors.strokeDefault;
        bottomColor = colors.strokeDivider;
    } else if (m_focused) {
        bgColor = (currentTheme() == Dark) ? colors.bgSolid : colors.controlDefault;
        borderColor = colors.strokeSecondary;
        bottomColor = colors.accentDefault;
        bottomWidth = ::Spacing::Border::Focused;
    } else if (m_pressed) {
        bgColor = colors.controlTertiary;
        borderColor = colors.strokeSecondary;
        bottomColor = colors.strokeSecondary;
    } else if (m_hovered) {
        bgColor = colors.controlSecondary;
        borderColor = colors.strokeSecondary;
        bottomColor = colors.strokeSecondary;
    } else {
        bgColor = colors.controlDefault;
        borderColor = colors.strokeDefault;
        bottomColor = colors.strokeDivider;
    }

    const qreal radius = themeRadius().control;
    QPainterPath framePath;
    framePath.addRoundedRect(frameRect, radius, radius);
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawPath(framePath);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(borderColor, 1));
    painter.drawPath(framePath);

    if (isEnabled() && !isReadOnly()) {
        QPen bottomPen(bottomColor, bottomWidth);
        bottomPen.setCapStyle(Qt::RoundCap);
        painter.setPen(bottomPen);
        const qreal bottomY = frameRect.bottom() - (bottomWidth > 1 ? (bottomWidth - 1) / 2.0 : 0);
        QPainterPath bottomPath;
        bottomPath.moveTo(frameRect.left() + radius, bottomY);
        bottomPath.lineTo(frameRect.right() - radius, bottomY);
        painter.drawPath(bottomPath);
    }
}

void PasswordBox::paintHeader(QPainter& painter) {
    if (m_header.isEmpty()) return;
    painter.setFont(themeFont(Typography::FontRole::Body).toQFont());
    painter.setPen(isEnabled() ? themeColors().textPrimary : themeColors().textDisabled);
    const QRect headerRect(0, 0, width(), kHeaderHeight);
    painter.drawText(headerRect, Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(m_header, Qt::ElideRight, headerRect.width()));
}

} // namespace view::textfields