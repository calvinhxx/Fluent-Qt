#include "TextEdit.h"
#include <QTextEdit>
#include <QScrollBar>
#include <QTextOption>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextFrameFormat>
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QFocusEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QTextLayout>
#include "components/scrolling/ScrollBar.h"

namespace fluent::textfields {

// ── Helpers. zh_CN: 辅助函数 ───────────────────────────────────────────────────

static int calcTopPad(const QFont& font, int lineHeight) {
    const int fontLh = QFontMetrics(font).lineSpacing();
    return qMax(0, lineHeight - fontLh) / 2;
}

static int calcBotPad(const QFont& font, int lineHeight) {
    const int fontLh = QFontMetrics(font).lineSpacing();
    return qMax(0, lineHeight - fontLh);
}

// ── Inner editor. zh_CN: 内部编辑器 ────────────────────────────────────────────
//
// QTextEdit is used (not QPlainTextEdit) because QTextDocumentLayout natively
// honors QTextBlockFormat top/bottom margins plus the rootFrame margin. Each
// text line and the caret center vertically inside the lineHeight slot via:
//   - rootFrame topMargin = (lineHeight - fontLh) / 2  … space above line one
//   - per-block bottomMargin = lineHeight - fontLh     … line spacing
// Qt then handles caret placement, selection, and hit testing without a
// custom paintEvent.
// zh_CN: 使用 QTextEdit（而非 QPlainTextEdit），因为 QTextDocumentLayout 原生
// 支持 QTextBlockFormat 的上下边距及 rootFrame 边距。借助上述两条公式，每行
// 文本与光标在 lineHeight 槽内垂直居中；光标定位、选区高亮、点击映射均由 Qt
// 自动处理，无需自定义 paintEvent。

class InnerTextEdit : public QTextEdit {
public:
    explicit InnerTextEdit(TextEdit* owner, QWidget* parent = nullptr)
        : QTextEdit(parent), m_owner(owner) {
        setAcceptRichText(false);
    }

    void setContentViewportMargins(int left, int top, int right, int bottom) {
        setViewportMargins(left, top, right, bottom);
    }

protected:
    void wheelEvent(QWheelEvent* event) override {
        if (m_owner && handleBoundaryWheel(event))
            return;
        QTextEdit::wheelEvent(event);
        if (m_owner && !m_owner->isScrollChainingEnabled() && hasScrollableRange())
            event->accept();
    }

    void paintEvent(QPaintEvent* e) override {
        QTextEdit::paintEvent(e);
        // Qt's stock placeholder ignores the rootFrame topMargin, so paint it
        // ourselves to keep it vertically centered.
        // zh_CN: Qt 原生 placeholder 不受 rootFrame topMargin 影响，自绘实现垂直居中。
        if (!m_owner || !document()->isEmpty()) return;
        const QString ph = m_owner->placeholderText();
        if (ph.isEmpty()) return;

        QPainter painter(viewport());
        const int topPad = calcTopPad(font(), m_owner->lineHeight());
        const int fontLh = QFontMetrics(font()).lineSpacing();
        QRect textRect(0, topPad, viewport()->width(), fontLh);
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.setFont(font());
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, ph);
    }

private:
    bool hasScrollableRange() const {
        const QScrollBar* bar = verticalScrollBar();
        return bar && bar->maximum() > bar->minimum();
    }

    bool handleBoundaryWheel(QWheelEvent* event) {
        if (!event)
            return false;
        QScrollBar* bar = verticalScrollBar();
        if (!bar || bar->maximum() <= bar->minimum()) {
            event->ignore();
            return true;
        }

        const qreal scrollPx = !event->pixelDelta().isNull()
            ? static_cast<qreal>(event->pixelDelta().y())
            : static_cast<qreal>(event->angleDelta().y());
        if (qFuzzyIsNull(scrollPx))
            return false;

        const bool atTop = bar->value() <= bar->minimum();
        const bool atBottom = bar->value() >= bar->maximum();
        const bool boundaryTail = (atTop && scrollPx > 0.0) ||
                                  (atBottom && scrollPx < 0.0);
        if (!boundaryTail)
            return false;

        if (m_owner->isScrollChainingEnabled()) {
            event->ignore();
            return true;
        }

        event->accept();
        return true;
    }

    TextEdit* m_owner = nullptr;
};

// ── Construction. zh_CN: 构造 ───────────────────────────────────────────────────

TextEdit::TextEdit(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_Hover);

    m_editor = new InnerTextEdit(this, this);
    m_editor->setFrameStyle(QFrame::NoFrame);
    m_editor->setBackgroundRole(QPalette::NoRole);
    m_editor->setLineWrapMode(QTextEdit::WidgetWidth);
    m_editor->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setAutoFillBackground(false);

    // Remove the document's default padding; rootFrame and block margins own it.
    // zh_CN: 移除文档默认四周留白（由 rootFrame margin + block margin 全权控制）。
    m_editor->document()->setDocumentMargin(0);

    // On text change: apply the centering format to new blocks and update height.
    // zh_CN: 监听文本变化：对新 block 应用居中格式并更新高度。
    connect(m_editor, &QTextEdit::textChanged, this, [this]() {
        if (!m_updatingFormat) {
            applyBlockCenterFormat();
            updateHeightForContent();
        }
        emit textChanged();
    });
    connect(m_editor, &QTextEdit::cursorPositionChanged,
            this, &TextEdit::cursorPositionChanged);
    connect(m_editor, &QTextEdit::selectionChanged,
            this, &TextEdit::selectionChanged);
    m_editor->installEventFilter(this);

    // Custom fluent scroll bar. zh_CN: 自定义 Fluent 滚动条。
    m_vScrollBar = new ::fluent::scrolling::ScrollBar(Qt::Vertical, this);
    m_vScrollBar->hide();

    auto* innerVBar = m_editor->verticalScrollBar();
    m_vScrollBar->setRange(innerVBar->minimum(), innerVBar->maximum());
    m_vScrollBar->setPageStep(innerVBar->pageStep());
    m_vScrollBar->setValue(innerVBar->value());

    connect(innerVBar, &QScrollBar::rangeChanged,
            this, [this, innerVBar](int /*min*/, int /*max*/) {
                if (!m_vScrollBar) return;
                m_vScrollBar->setRange(innerVBar->minimum(), innerVBar->maximum());
                m_vScrollBar->setPageStep(innerVBar->pageStep());
                // Scroll bar visibility is owned by updateHeightForContent.
                // zh_CN: 滚动条可见性由 updateHeightForContent 统一管理。
            });
    connect(innerVBar, &QScrollBar::valueChanged,
            this, [this](int v) {
                if (m_vScrollBar && m_vScrollBar->value() != v)
                    m_vScrollBar->setValue(v);
            });
    connect(m_vScrollBar, &QScrollBar::valueChanged,
            this, [this, innerVBar](int v) {
                if (innerVBar->value() != v)
                    innerVBar->setValue(v);
            });

    // Initial theme, line format, then height (order matters). zh_CN: 初始主题 + 行格式 + 高度（顺序重要）。
    onThemeUpdated();
    updateHeightForContent();
}

// ── Text API. zh_CN: 文本 API ───────────────────────────────────────────────────

void TextEdit::setPlainText(const QString& text) {
    if (m_editor) {
        m_editor->setPlainText(text);
        applyBlockCenterFormat();
        updateHeightForContent();
    }
}

QString TextEdit::toPlainText() const {
    return m_editor ? m_editor->toPlainText() : QString();
}

void TextEdit::clear() {
    if (m_editor) m_editor->clear();
}

void TextEdit::setPlaceholderText(const QString& text) {
    m_placeholderText = text;
    if (m_editor && m_editor->viewport()) m_editor->viewport()->update();
}

QString TextEdit::placeholderText() const {
    return m_placeholderText;
}

void TextEdit::setReadOnly(bool readOnly) {
    if (m_editor) m_editor->setReadOnly(readOnly);
}

bool TextEdit::isReadOnly() const {
    return m_editor ? m_editor->isReadOnly() : false;
}

::fluent::scrolling::ScrollBar* TextEdit::verticalScrollBar() const {
    return m_vScrollBar;
}

void TextEdit::setFocus() {
    if (m_editor) m_editor->setFocus(Qt::OtherFocusReason);
}

void TextEdit::setFocus(Qt::FocusReason reason) {
    if (m_editor) m_editor->setFocus(reason);
}

// ── Events. zh_CN: 事件 ─────────────────────────────────────────────────────────

void TextEdit::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    paintFrame(p);
    QWidget::paintEvent(event);
}

void TextEdit::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_editor) {
        QRect r = rect();
        int sbw = (m_vScrollBar && m_vScrollBar->isVisible()) ? m_vScrollBar->thickness() : 0;
        m_editor->setGeometry(r.adjusted(0, 0, -sbw, 0));
        if (m_vScrollBar) {
            int x = r.right() - m_vScrollBar->thickness() + 1;
            int y = r.top() + 2;
            int h = r.height() - 4;
            m_vScrollBar->setGeometry(x, y, m_vScrollBar->thickness(), h);
        }
    }
}

void TextEdit::paintFrame(QPainter& painter) {
    const auto& colors = themeColors();
    const auto& radius = themeRadius();
    QRectF bgRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QColor bgColor, borderColor, bottomBorderColor;
    int bottomBorderWidth = m_unfocusedBorderWidth;
    if (!isEnabled()) {
        bgColor = colors.controlDisabled;
        borderColor = colors.strokeDivider;
        bottomBorderColor = borderColor;
    } else if (isReadOnly()) {
        bgColor = colors.controlAltSecondary;
        borderColor = colors.strokeDefault;
        bottomBorderColor = colors.strokeDivider;
    } else if (m_isFocused) {
        bgColor = (currentTheme() == Dark) ? colors.bgSolid : colors.controlDefault;
        borderColor = colors.strokeSecondary;
        bottomBorderColor = colors.accentDefault;
        bottomBorderWidth = m_focusedBorderWidth;
    } else if (m_isHovered) {
        bgColor = colors.controlSecondary;
        borderColor = colors.strokeSecondary;
        bottomBorderColor = colors.strokeSecondary;
    } else {
        bgColor = colors.controlDefault;
        borderColor = colors.strokeDefault;
        bottomBorderColor = colors.strokeDivider;
    }

    qreal r = radius.control;
    QPainterPath framePath;
    framePath.addRoundedRect(bgRect, r, r);
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawPath(framePath);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(borderColor, 1));
    painter.drawPath(framePath);

    if (isEnabled() && !isReadOnly()) {
        QPen pen(bottomBorderColor, bottomBorderWidth);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        qreal bottomY = bgRect.bottom() - (bottomBorderWidth > 1 ? (bottomBorderWidth - 1) / 2.0 : 0);
        QPainterPath bottomPath;
        bottomPath.moveTo(bgRect.left() + r, bottomY);
        bottomPath.lineTo(bgRect.right() - r, bottomY);
        painter.drawPath(bottomPath);
    }
}

void TextEdit::enterEvent(FluentEnterEvent* event) {
    m_isHovered = true; update(); QWidget::enterEvent(event);
}

void TextEdit::leaveEvent(QEvent* event) {
    m_isHovered = false; update(); QWidget::leaveEvent(event);
}

void TextEdit::focusInEvent(QFocusEvent* event) {
    m_isFocused = true; update(); QWidget::focusInEvent(event);
}

void TextEdit::focusOutEvent(QFocusEvent* event) {
    m_isFocused = false; update(); QWidget::focusOutEvent(event);
}

bool TextEdit::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_editor) {
        if (event->type() == QEvent::FocusIn) {
            m_isFocused = true; update();
        } else if (event->type() == QEvent::FocusOut) {
            m_isFocused = false; update();
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ── Property setters. zh_CN: 属性 setter ────────────────────────────────────────

void TextEdit::setContentMargins(const QMargins& margins) {
    if (m_contentMargins == margins) return;
    m_contentMargins = margins;
    applyThemeStyle();
    emit contentMarginsChanged();
}

void TextEdit::setFontRole(const QString& role) {
    if (m_fontRole == role) return;
    m_fontRole = role;
    applyThemeStyle();
    if (m_editor && m_editor->viewport())
        m_editor->viewport()->update();
    emit fontRoleChanged();
}

void TextEdit::setFocusedBorderWidth(int width) {
    if (m_focusedBorderWidth == width) return;
    m_focusedBorderWidth = width;
    update();
    emit focusedBorderWidthChanged();
}

void TextEdit::setUnfocusedBorderWidth(int width) {
    if (m_unfocusedBorderWidth == width) return;
    m_unfocusedBorderWidth = width;
    update();
    emit unfocusedBorderWidthChanged();
}

void TextEdit::setLineHeight(int height) {
    if (height <= 0 || m_lineHeight == height) return;
    m_lineHeight = height;
    applyBlockCenterFormat();
    updateHeightForContent();
    emit layoutMetricsChanged();
}

void TextEdit::setMinVisibleLines(int lines) {
    if (lines <= 0 || m_minVisibleLines == lines) return;
    m_minVisibleLines = lines;
    updateHeightForContent();
    emit layoutMetricsChanged();
}

void TextEdit::setMaxVisibleLines(int lines) {
    if (lines <= 0 || m_maxVisibleLines == lines) return;
    m_maxVisibleLines = lines;
    updateHeightForContent();
    emit layoutMetricsChanged();
}

void TextEdit::setScrollChainingEnabled(bool enabled) {
    if (m_scrollChainingEnabled == enabled) return;
    m_scrollChainingEnabled = enabled;
    emit scrollChainingEnabledChanged();
}

void TextEdit::onThemeUpdated() {
    applyThemeStyle();
}

// ── Core internals. zh_CN: 核心私有方法 ─────────────────────────────────────────

void TextEdit::applyThemeStyle() {
    if (!m_editor) return;

    const auto& c = themeColors();
    QPalette pal = palette();
    pal.setColor(QPalette::Base,             Qt::transparent);
    pal.setColor(QPalette::Window,           Qt::transparent);
    pal.setColor(QPalette::Text,             c.textPrimary);
    pal.setColor(QPalette::PlaceholderText,  c.textSecondary);
    pal.setColor(QPalette::Highlight,        c.accentDefault);
    pal.setColor(QPalette::HighlightedText,  c.textOnAccent);
    pal.setColor(QPalette::Inactive, QPalette::Highlight,        c.accentDefault);
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText,  c.textOnAccent);
    pal.setColor(QPalette::Disabled, QPalette::Text,             c.textDisabled);
    pal.setColor(QPalette::Disabled, QPalette::PlaceholderText,  c.textDisabled);
    m_editor->setPalette(pal);
    m_editor->setFont(themeFont(m_fontRole).toQFont());

    QString qss = QString(
                    "QTextEdit { "
                    "background: transparent; "
                    "color: %1; "
                    "selection-background-color: %2; "
                    "selection-color: %3; "
                    "border: none; }")
                    .arg(c.textPrimary.name(QColor::HexArgb))
                    .arg(c.accentDefault.name(QColor::HexArgb))
                    .arg(c.textOnAccent.name(QColor::HexArgb));
    m_editor->setStyleSheet(qss);

    if (auto* vp = m_editor->viewport()) {
        vp->setAutoFillBackground(false);
        QPalette vpal = vp->palette();
        vpal.setColor(QPalette::Base,   Qt::transparent);
        vpal.setColor(QPalette::Window, Qt::transparent);
        vp->setPalette(vpal);
        vp->setStyleSheet("background: transparent; border: none;");
    }

    // Recompute centering margins after font changes (fontLineSpacing may differ).
    // zh_CN: 字体变更后重算居中 margin（fontLineSpacing 可能不同）。
    applyBlockCenterFormat();
}

void TextEdit::applyBlockCenterFormat() {
    if (!m_editor) return;

    m_updatingFormat = true;

    const QFont f = m_editor->font();
    const int topPad = calcTopPad(f, m_lineHeight);
    const int botPad = calcBotPad(f, m_lineHeight);

    // 1. rootFrame topMargin: space above line one for vertical centering.
    // zh_CN: rootFrame topMargin — 首行上方留白，实现垂直居中。
    QTextFrameFormat rff = m_editor->document()->rootFrame()->frameFormat();
    rff.setTopMargin(topPad);
    rff.setBottomMargin(0);
    rff.setLeftMargin(0);
    rff.setRightMargin(0);
    m_editor->document()->rootFrame()->setFrameFormat(rff);

    // 2. LineDistanceHeight: each visual line = fontLh + botPad = lineHeight.
    //    bottomMargin is avoided; combined with LineDistanceHeight it would
    //    double the line spacing.
    // zh_CN: LineDistanceHeight — 每个视觉行高 = fontLh + botPad = lineHeight；
    // 不使用 bottomMargin，否则与 LineDistanceHeight 重叠导致行间距翻倍。
    QTextBlockFormat fmt;
    fmt.setLineHeight(botPad, QTextBlockFormat::LineDistanceHeight);
    fmt.setBottomMargin(0);

    QTextCursor cursor(m_editor->document());
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.mergeBlockFormat(fmt);

    // 3. Left/right viewport margins. zh_CN: 左右 viewport margins。
    static_cast<InnerTextEdit*>(m_editor)->setContentViewportMargins(
        m_contentMargins.left(), 0, m_contentMargins.right(), 0);

    m_updatingFormat = false;
}

void TextEdit::updateHeightForContent() {
    if (!m_editor) return;

    // Count all visual lines, including wrap-generated ones. zh_CN: 统计所有可视行数（包括自动换行产生的行）。
    int visualLines = 0;
    QTextBlock block = m_editor->document()->begin();
    while (block.isValid()) {
        int lc = block.layout()->lineCount();
        visualLines += (lc > 0) ? lc : 1;
        block = block.next();
    }
    if (visualLines < 1) visualLines = 1;

    const int clamped = qBound(m_minVisibleLines, visualLines, m_maxVisibleLines);

    // height = clampedLines × lineHeight
    setFixedHeight(clamped * m_lineHeight);

    // The scroll bar only appears once content exceeds maxVisibleLines.
    // zh_CN: 滚动条仅在内容实际超过 maxVisibleLines 时显示。
    m_scrollEnabled = (visualLines > m_maxVisibleLines);
    if (m_vScrollBar)
        m_vScrollBar->setVisible(m_scrollEnabled);
    // Reset the inner scroll position when not needed to avoid content drift.
    // zh_CN: 无需滚动时重置内部滚动位置，避免内容偏移。
    if (!m_scrollEnabled && m_editor)
        m_editor->verticalScrollBar()->setValue(0);

    updateGeometry();
}

} // namespace fluent::textfields
