#include "HyperlinkButton.h"
#include <QDesktopServices>
#include <QPainter>
#include <QStyleOptionButton>

namespace fluent::basicinput {

HyperlinkButton::HyperlinkButton(const QString& text, QWidget* parent)
    : Button(text, parent) {
    setFluentStyle(Subtle);
    setCursor(Qt::PointingHandCursor);
    
    // On click, open the URL automatically when one is set.
    // zh_CN: 连接点击信号，设置了 URL 则自动打开。
    connect(this, &QPushButton::clicked, this, [this]() {
        if (m_url.isValid()) {
            QDesktopServices::openUrl(m_url);
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
        m_url = url;
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
    // Button::paintEvent is already complete, so this acts as a proxy:
    // 1. temporarily swap the colors the base class paints with,
    // 2. add the underline on hover.
    // zh_CN: Button::paintEvent 已经很完整，这里采用“代理”模式：
    // 1. 临时修改本组件颜色，让基类按该颜色绘制；2. Hover 时额外画下划线。
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColors();
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

    // 2. Colors: hyperlinks always use accent unless disabled.
    // zh_CN: Hyperlink 始终使用 Accent 颜色（除非禁用）。
    QColor bgColor = Qt::transparent;
    QColor textColor = colors.accentDefault;
    
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

    // 3. Paint the background. zh_CN: 绘制背景。
    if (bgColor != Qt::transparent) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(bgColor);
        painter.drawRoundedRect(rect(), radius.control, radius.control);
    }

    // 4. Paint the content. zh_CN: 绘制内容。
    painter.setFont(font());
    painter.setPen(textColor);

    // Position math mirrors Button but stays inline for brevity.
    // zh_CN: 位置计算复用 Button 思路，这里直接手写保持紧凑。
    QString txt = text();
    QFontMetrics fm = painter.fontMetrics();
    int txtWidth = fm.horizontalAdvance(txt);
    
    // Text only for now (icons could be added later); draw it centered.
    // zh_CN: 暂时只支持文字（需要时可扩展图标），简单起见直接居中绘制。
    QRectF textRect = rect();
    painter.drawText(textRect, Qt::AlignCenter, txt);

    // 5. Underline, only on hover and when enabled. zh_CN: 绘制下划线（仅 Hover 且开启时）。
    if (state == Hover && m_showUnderline && !txt.isEmpty()) {
        int textX = (width() - txtWidth) / 2;
        int textY = (height() + fm.ascent()) / 2; // Text baseline. zh_CN: 文字基线位置。
        // Draw the line 2px under the text. zh_CN: 在文字下方 2 像素处画线。
        painter.drawLine(textX, textY + 2, textX + txtWidth, textY + 2);
    }
}

} // namespace fluent::basicinput
