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
#include "components/basicinput/Button.h"

namespace fluent::textfields {

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
    m_buttonLayout = new ::fluent::AnchorLayout(this);

    m_revealButton = new ::fluent::basicinput::Button(this);
    m_revealButton->setObjectName("PasswordBoxRevealButton");
    m_revealButton->setFluentStyle(::fluent::basicinput::Button::Subtle);
    m_revealButton->setFluentLayout(::fluent::basicinput::Button::IconOnly);
    m_revealButton->setFluentSize(::fluent::basicinput::Button::Small);
    m_revealButton->setFocusPolicy(Qt::NoFocus);
    m_revealButton->setFixedSize(kButtonWidth, kButtonHeight);
    m_revealButton->setIconGlyph(Typography::Icons::View,
                                 Typography::FontSize::Body,
                                 Typography::FontFamily::FluentIcons);
    m_revealButton->installEventFilter(this);
    m_buttonLayout->addWidget(m_revealButton);

    connect(m_revealButton, &::fluent::basicinput::Button::pressed, this, [this]() {
        if (!canPeekReveal()) return;
        setFocus(Qt::MouseFocusReason);
        setPeekActive(true);
    });
    connect(m_revealButton, &::fluent::basicinput::Button::released, this, [this]() {
        setPeekActive(false);
        setFocus(Qt::MouseFocusReason);
    });

    updateRevealButtonGeometry();
}

void PasswordBox::updateRevealButtonGeometry() {
    if (!m_revealButton) return;

    using Edge = ::fluent::AnchorLayout::Edge;
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
                                 Typography::FontFamily::FluentIcons);
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

bool PasswordBox::paintBrandInputFrame(QPainter& painter) {
    const auto lang = themeDesignLanguage();
    if (lang == DesignFluent) return false;

    const auto colors = themeColors();
    const QRectF base = QRectF(inputRect());

    if (lang == DesignMaterial) {
        // Material 3 outlined field: transparent fill, 1dp outline thickening to 2dp accent on focus.
        // zh_CN: M3 描边字段:透明填充,1dp 描边聚焦时加粗为 2dp accent。
        QColor outlineColor;
        qreal outlineWidth = 1.0;
        if (!isEnabled()) {
            outlineColor = colors.strokeDivider;
        } else if (m_focused) {
            outlineColor = colors.accentDefault;
            outlineWidth = 2.0;
        } else {
            outlineColor = colors.strokeStrong;
        }
        const qreal inset = outlineWidth / 2.0;
        const qreal r = themeRadius().control;
        QPainterPath framePath;
        framePath.addRoundedRect(base.adjusted(inset, inset, -inset, -inset), r, r);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(outlineColor, outlineWidth));
        painter.drawPath(framePath);
        return true;
    }

    // DesignCupertino: hairline rect + accent focus ring with a soft glow, all inset to avoid clipping.
    // zh_CN: macOS:发丝矩形 + 内缩的强调焦点环及柔和辉光,避免裁切。
    const qreal r = 6.0;

    // Bezel fill (docs §4): restrained near-white / white@10% surface, matching LineEdit/ComboBox so
    // the field reads as a raised control. zh_CN: bezel 填充(文档 §4):与 LineEdit/ComboBox 同款不透明 bezel。
    {
        const QColor fill = !isEnabled() ? colors.controlDisabled : colors.bgLayerAlt;
        QPainterPath fillPath;
        fillPath.addRoundedRect(base.adjusted(0.5, 0.5, -0.5, -0.5), r, r);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawPath(fillPath);
    }

    if (isEnabled() && m_focused) {
        QColor glow = colors.accentDefault;
        glow.setAlpha(0x40);
        QPainterPath glowPath;
        glowPath.addRoundedRect(base.adjusted(0.5, 0.5, -0.5, -0.5), r, r);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(glow, 2.0));
        painter.drawPath(glowPath);

        QPainterPath ringPath;
        ringPath.addRoundedRect(base.adjusted(1.5, 1.5, -1.5, -1.5),
                                qMax<qreal>(0.0, r - 1.0), qMax<qreal>(0.0, r - 1.0));
        painter.setPen(QPen(colors.accentDefault, 2.0));
        painter.drawPath(ringPath);
        return true;
    }

    QColor hairline = !isEnabled() ? colors.strokeDivider
                                   : (m_hovered ? colors.strokeStrong : colors.strokeDefault);
    QPainterPath hairPath;
    hairPath.addRoundedRect(base.adjusted(0.5, 0.5, -0.5, -0.5), r, r);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(hairline, 1.0));
    painter.drawPath(hairPath);
    return true;
}

void PasswordBox::paintInputFrame(QPainter& painter) {
    const auto colors = themeColors();
    const QRectF frameRect = QRectF(inputRect()).adjusted(0.5, 0.5, -0.5, -0.5);

    // Brand-aware frame for the input row; the reveal button is a fluent::Button that already follows the
    // design language. DesignFluent falls through to the original path below unchanged. zh_CN: 输入行的品牌
    // 感知边框;reveal 按钮是 fluent::Button,已跟随设计语言。DesignFluent 落到下方原路径,保持不变。
    if (paintBrandInputFrame(painter)) return;

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
        bgColor = (effectiveTheme() == Dark) ? colors.bgSolid : colors.controlDefault;
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

} // namespace fluent::textfields
