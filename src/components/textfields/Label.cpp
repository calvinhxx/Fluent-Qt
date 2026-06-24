#include "Label.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QResizeEvent>
#include <QScreen>

#include "components/status_info/ToolTip.h"

namespace fluent::textfields {

namespace {
constexpr int kToolTipGap = 8;

QString cssRgba(const QColor& c) {
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
}
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
    if (m_styleName == styleName && !m_customFont) return;
    m_styleName = styleName;
    m_customFont = false;
    applyTypographyFont();
    emit typographyChanged();
}

void Label::setTextElideMode(Qt::TextElideMode mode) {
    if (m_textElideMode == mode) return;
    m_textElideMode = mode;
    updateRenderedText();
    emit textElideModeChanged(m_textElideMode);
}

void Label::setFont(const QFont& font) {
    m_customFont = true;
    QLabel::setFont(font);
    updateRenderedText();
}

void Label::setTextColorRole(TextColorRole role) {
    if (m_textColorRole == role)
        return;
    m_textColorRole = role;
    applyTextColor();
    updateRenderedText();
}

void Label::onThemeUpdated() {
    // Preserve caller-supplied fonts such as Segoe Fluent Icons across theme changes.
    // zh_CN: 主题切换时保留调用方设置的字体，例如 Segoe Fluent Icons。
    if (!m_customFont)
        applyTypographyFont();

    // 2. Refresh the color. zh_CN: 更新颜色。
    applyTextColor();
    updateRenderedText();
}

QColor Label::resolveTextColor() const {
    const auto& c = themeColors();
    switch (m_textColorRole) {
    case TextColorRole::Secondary: return c.textSecondary;
    case TextColorRole::Tertiary:  return c.textTertiary;
    case TextColorRole::Disabled:  return c.textDisabled;
    case TextColorRole::OnAccent:  return c.textOnAccent;
    case TextColorRole::Accent:    return c.textAccentPrimary;
    case TextColorRole::Primary:
    case TextColorRole::Default:
    default:                       return c.textPrimary;
    }
}

void Label::applyTextColor() {
    if (m_textColorRole == TextColorRole::Default) {
        // Legacy palette-based coloring. Correct when no ancestor has a style sheet; left untouched so
        // components that set their own label palette (InfoBar status text, ToolTip, collection-view
        // headers, …) keep working. zh_CN: 原有基于 palette 的上色。无祖先样式表时正确；保持不变，使自行设置标签
        // palette 的组件（InfoBar 状态文本、ToolTip、集合视图表头等）继续工作。
        QPalette p = palette();
        p.setColor(QPalette::WindowText, resolveTextColor());
        setPalette(p);
    } else {
        // An explicit role colors through the label's OWN style sheet, so an ancestor style sheet
        // (which installs QStyleSheetStyle and makes Qt ignore the child palette) can't drop it. This is
        // what value/status labels on a styled preview surface need. zh_CN: 指定角色改用标签自身样式表上色,
        // 使祖先样式表(安装 QStyleSheetStyle、让 Qt 忽略子 palette)无法丢弃它。带样式表预览面上的取值/状态标签即需此。
        setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                          .arg(cssRgba(resolveTextColor())));
    }
}

void Label::applyTypographyFont() {
    QLabel::setFont(themeFont(m_styleName).toQFont());
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
