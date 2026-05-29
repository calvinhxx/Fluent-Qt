#include "Label.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QResizeEvent>
#include <QScreen>

#include "components/status_info/ToolTip.h"

namespace fluent::textfields {

namespace {
constexpr int kToolTipGap = 8;
}

Label::Label(const QString& text, QWidget* parent)
    : QLabel(parent)
    , m_fullText(text)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    onThemeUpdated();
    updateRenderedText();
}

Label::Label(QWidget* parent)
    : Label("", parent) {
}

Label::~Label() {
    if (m_elideToolTip) {
        m_elideToolTip->setAnimationEnabled(false);
        m_elideToolTip->hide();
    }
}

void Label::setText(const QString& text) {
    if (m_fullText == text) return;
    m_fullText = text;
    updateRenderedText();
}

void Label::setFluentTypography(const QString& styleName) {
    if (m_styleName == styleName) return;
    m_styleName = styleName;
    setFont(themeFont(m_styleName).toQFont());
    emit typographyChanged();
}

void Label::setTextElideMode(Qt::TextElideMode mode) {
    if (m_textElideMode == mode) return;
    m_textElideMode = mode;
    updateRenderedText();
    emit textElideModeChanged(m_textElideMode);
}

void Label::setFont(const QFont& font) {
    QLabel::setFont(font);
    updateRenderedText();
}

void Label::onThemeUpdated() {
    // 1. 使用保存的样式名更新字体 (解决切换主题后字体统一的问题)
    setFont(themeFont(m_styleName).toQFont());

    // 2. 更新颜色
    const auto& c = themeColors();
    QPalette p = palette();
    p.setColor(QPalette::WindowText, c.textPrimary);
    setPalette(p);
    updateRenderedText();
}

void Label::resizeEvent(QResizeEvent* event) {
    QLabel::resizeEvent(event);
    updateRenderedText();
}

void Label::enterEvent(FluentEnterEvent* event) {
    QLabel::enterEvent(event);
    showElideToolTip();
}

void Label::leaveEvent(QEvent* event) {
    QLabel::leaveEvent(event);
    hideElideToolTip();
}

void Label::changeEvent(QEvent* event) {
    QLabel::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        updateRenderedText();
    }
}

void Label::updateRenderedText() {
    QString renderedText = m_fullText;
    bool isElided = false;

    if (m_textElideMode != Qt::ElideNone && !m_fullText.isEmpty()) {
        const int textWidth = availableTextWidth();
        const QFontMetrics metrics(font());
        renderedText = metrics.elidedText(m_fullText, m_textElideMode, textWidth);
        isElided = renderedText != m_fullText;
    }

    if (QLabel::text() != renderedText) {
        QLabel::setText(renderedText);
    }

    m_isTextElided = isElided;
    if (m_elideToolTip) {
        m_elideToolTip->setText(m_fullText);
        if (!m_isTextElided) {
            hideElideToolTip();
        } else if (m_elideToolTip->isVisible()) {
            positionElideToolTip();
        }
    }
}

int Label::availableTextWidth() const {
    const int contentWidth = contentsRect().width();
    return qMax(0, contentWidth > 0 ? contentWidth : width());
}

void Label::ensureElideToolTip() {
    if (m_elideToolTip) return;

    m_elideToolTip = new fluent::status_info::ToolTip(this);
    m_elideToolTip->setObjectName(QStringLiteral("LabelElideToolTip"));
}

void Label::showElideToolTip() {
    if (!m_isTextElided || m_fullText.isEmpty()) return;

    ensureElideToolTip();
    m_elideToolTip->setText(m_fullText);
    positionElideToolTip();
    m_elideToolTip->show();
    m_elideToolTip->raise();
}

void Label::hideElideToolTip() {
    if (!m_elideToolTip) return;
    m_elideToolTip->hide();
}

void Label::positionElideToolTip() {
    if (!m_elideToolTip) return;

    m_elideToolTip->adjustSize();
    const QSize tipSize = m_elideToolTip->sizeHint().expandedTo(m_elideToolTip->size());
    const QPoint labelTopLeft = mapToGlobal(QPoint(0, 0));

    QPoint tipPos(labelTopLeft.x() + (width() - tipSize.width()) / 2,
                  labelTopLeft.y() - tipSize.height() - kToolTipGap);

    QScreen* screen = QGuiApplication::screenAt(mapToGlobal(rect().center()));
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen) {
        const QRect available = screen->availableGeometry();
        const int maxX = available.right() - tipSize.width() + 1;
        tipPos.setX(qBound(available.left(), tipPos.x(), qMax(available.left(), maxX)));
        if (tipPos.y() < available.top()) {
            tipPos.setY(labelTopLeft.y() + height() + kToolTipGap);
        }
        const int maxY = available.bottom() - tipSize.height() + 1;
        tipPos.setY(qBound(available.top(), tipPos.y(), qMax(available.top(), maxY)));
    }

    m_elideToolTip->move(tipPos);
}

} // namespace fluent::textfields
