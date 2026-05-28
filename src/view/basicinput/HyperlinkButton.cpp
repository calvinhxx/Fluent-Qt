#include "HyperlinkButton.h"
#include <QDesktopServices>
#include <QPainter>
#include <QStyleOptionButton>

namespace view::basicinput {

HyperlinkButton::HyperlinkButton(const QString& text, QWidget* parent)
    : Button(text, parent) {
    setFluentStyle(Subtle);
    setCursor(Qt::PointingHandCursor);
    
    // 连接点击信号，如果设置了 URL 则自动打开
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
    // 强制使用 Accent 颜色作为文本色
    // 注意：Button::paintEvent 会根据 m_style 获取颜色
    // 对于 HyperlinkButton，我们希望即使是 Subtle 样式，文本也使用 Accent 颜色
    update();
}

void HyperlinkButton::paintEvent(QPaintEvent* event) {
    // 我们需要重写绘制逻辑，或者在调用基类前后修改 Painter 状态
    // 因为 Button::paintEvent 已经很完整了，我们采取“代理”模式：
    // 1. 临时修改本组件的颜色，让基类按我们的颜色画
    // 2. 如果是 Hover，额外画下划线
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColors();
    const auto& radius = themeRadius();
    const auto& spacing = themeSpacing();

    // 1. 确定交互状态
    InteractionState state = interactionState();
    if (!isEnabled()) {
        state = Disabled;
    } else if (state == Rest) {
        if (isDown()) state = Pressed;
        else if (underMouse()) state = Hover;
    }

    // 2. 颜色逻辑：Hyperlink 始终使用 Accent 颜色（除非禁用）
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

    // 3. 绘制背景
    if (bgColor != Qt::transparent) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(bgColor);
        painter.drawRoundedRect(rect(), radius.control, radius.control);
    }

    // 4. 绘制内容
    painter.setFont(font());
    painter.setPen(textColor);

    // 计算位置（复用 Button 逻辑，但这里我们直接手写，保持代码紧凑）
    QString txt = text();
    QFontMetrics fm = painter.fontMetrics();
    int txtWidth = fm.horizontalAdvance(txt);
    
    // 我们暂时只支持文字，如果需要图标可以扩展
    // 这里为了简单，直接居中绘制文字
    QRectF textRect = rect();
    painter.drawText(textRect, Qt::AlignCenter, txt);

    // 5. 绘制下划线 (仅 Hover 且开启时)
    if (state == Hover && m_showUnderline && !txt.isEmpty()) {
        int textX = (width() - txtWidth) / 2;
        int textY = (height() + fm.ascent()) / 2; // 文字基线位置
        // 在文字下方 2 像素处画线
        painter.drawLine(textX, textY + 2, textX + txtWidth, textY + 2);
    }
}

} // namespace view::basicinput
