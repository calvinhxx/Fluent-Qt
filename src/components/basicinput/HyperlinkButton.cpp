#include "HyperlinkButton.h"
#include "components/basicinput/private/HyperlinkButtonAccessibility_p.h"
#include <QDesktopServices>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionButton>

namespace fluent::basicinput {

HyperlinkButton::HyperlinkButton(const QString& text, QWidget* parent)
    : Button(text, parent) {
    detail::ensureHyperlinkButtonAccessibilityFactory();
    setFluentStyle(Subtle);
    setCursor(Qt::PointingHandCursor);
    
    // On click, open the URL automatically when one is set.
    // zh_CN: 连接点击信号，设置了 URL 则自动打开。
    connect(this, &QPushButton::clicked, this, [this]() {
        if (m_url.isValid()) {
            if (QDesktopServices::openUrl(m_url)
                && !m_accessibilityVisited) {
                m_accessibilityVisited = true;
                detail::notifyHyperlinkButtonAccessibilityVisited(
                    this);
            }
        }
    });

    onThemeUpdated();
}

HyperlinkButton::HyperlinkButton(const QString& text, const QUrl& url, QWidget* parent)
    : HyperlinkButton(text, parent) {
    setUrl(url);
}

void HyperlinkButton::setUrl(const QUrl& url) {
    if (m_url != url) {
        const bool visitedChanged = m_accessibilityVisited;
        m_url = url;
        m_accessibilityVisited = false;
        detail::notifyHyperlinkButtonAccessibilityUrlChanged(
            this, visitedChanged);
        emit urlChanged();
    }
}

void HyperlinkButton::setShowUnderline(bool show) {
    if (m_showUnderline != show) {
        m_showUnderline = show;
        update();
        emit showUnderlineChanged();
    }
}

void HyperlinkButton::onThemeUpdated() {
    // Force accent text: Button::paintEvent derives colors from m_style, but a
    // HyperlinkButton wants accent text even in the Subtle style.
    // zh_CN: 强制使用 Accent 文本色——Button::paintEvent 按 m_style 取色，而
    // HyperlinkButton 即使是 Subtle 样式也希望文本用 Accent 色。
    update();
}

void HyperlinkButton::paintEvent(QPaintEvent* event) {
    // A hyperlink is an accent-colored text link with minimal fill.
    // zh_CN: 超链接是使用强调色文字和轻量填充的文本链接。

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColorsRef();
    const auto& radius = themeRadius();
    const auto& spacing = themeSpacing();

    // 1. Resolve the interaction state. zh_CN: 确定交互状态。
    InteractionState state = interactionState();
    if (!isEnabled()) {
        state = Disabled;
    } else if (state == Rest) {
        if (isDown()) state = Pressed;
        else if (underMouse()) state = Hover;
    }

    // 2. Resolve colors. Default-init the fill to transparent so the invalid-QColor trap
    // (alpha()==255 on an unassigned QColor → solid black) can never fire. zh_CN: 填充默认透明,
    // 杜绝「未赋值 QColor 的 alpha()==255 → 涂黑」陷阱。
    QColor bgColor = Qt::transparent;   // resting/pressed surface fill (Fluent path). zh_CN: 静息/按下表面填充(Fluent)。
    QColor textColor = colors.accentDefault;

    // Fluent text-link treatment. zh_CN: Fluent 文本链接样式。
        if (state == Hover) {
            bgColor = colors.subtleSecondary;
            textColor = colors.accentSecondary;
        } else if (state == Pressed) {
            bgColor = colors.subtleTertiary;
            textColor = colors.accentTertiary;
        } else if (state == Disabled) {
            bgColor = Qt::transparent;
            textColor = colors.textDisabled;
        }

    // 3. Paint the subtle Fluent surface.
    // zh_CN: 绘制轻量 Fluent 表面。
    const QRectF surfaceRect = rect();
    if (bgColor.isValid() && bgColor.alpha() > 0 && bgColor != QColor(Qt::transparent)) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(bgColor);
        painter.drawRoundedRect(surfaceRect, radius.control, radius.control);
    }

    // 4. Paint the text (centered). zh_CN: 居中绘制文字。
    painter.setFont(font());
    painter.setPen(textColor);

    QString txt = text();
    QFontMetrics fm = painter.fontMetrics();
    int txtWidth = fm.horizontalAdvance(txt);

    QRectF textRect = rect();
    painter.drawText(textRect, Qt::AlignCenter, txt);

    // 5. Underline: the explicit opt-in draws on hover. zh_CN:
    // 显式开启后在悬停时绘制下划线。
    const bool underlineExplicit = m_showUnderline && state == Hover;
    if (underlineExplicit && !txt.isEmpty()) {
        int textX = (width() - txtWidth) / 2;
        int textY = (height() + fm.ascent()) / 2; // Text baseline. zh_CN: 文字基线位置。
        // Draw the line 2px under the text in the text color. zh_CN: 在文字下方 2 像素处用文字色画线。
        painter.setPen(textColor);
        painter.drawLine(textX, textY + 2, textX + txtWidth, textY + 2);
    }

    Q_UNUSED(spacing);
    Q_UNUSED(event);
}

} // namespace fluent::basicinput
