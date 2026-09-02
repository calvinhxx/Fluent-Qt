#include "TextEdit.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/menus_toolbars/private/TextEditingMenu_p.h"
#include "components/scrolling/ScrollBar.h"
#include <QAbstractTextDocumentLayout>
#include <QContextMenuEvent>
#include <QEvent>
#include <QFocusEvent>
#include <QFontMetricsF>
#include <QInputMethodEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFrame>
#include <QTextFrameFormat>
#include <QTextLayout>
#include <QTextOption>
#include <QTimer>
#include <QVariantAnimation>
#include <QtMath>

namespace fluent::textfields {

// ── Helpers. zh_CN: 辅助函数 ───────────────────────────────────────────────────

constexpr char kHeightAnimationObjectName[] = "fluentTextEditHeightAnimation";

class TextEditHeightAnimation final : public QVariantAnimation {
public:
    explicit TextEditHeightAnimation(QObject* parent) : QVariantAnimation(parent) {}

    int targetHeight = 0;
    bool animateNextUpdate = false;
    bool programmaticTextChange = false;
    bool settleSynchronouslyNextUpdate = false;
};

static TextEditHeightAnimation* heightAnimationFor(TextEdit* edit)
{
    auto* animation =
        edit ? edit->findChild<QVariantAnimation*>(QString::fromLatin1(kHeightAnimationObjectName),
                                                   Qt::FindDirectChildrenOnly)
             : nullptr;
    return static_cast<TextEditHeightAnimation*>(animation);
}

static void requestSynchronousHeightSettlement(TextEdit* edit)
{
    if (auto* animation = heightAnimationFor(edit))
        animation->settleSynchronouslyNextUpdate = true;
}

static void applyTextEditHeight(TextEdit* edit, int height)
{
    if (!edit)
        return;

    const int boundedHeight = qMax(1, height);
    if (edit->height() == boundedHeight && edit->minimumHeight() == boundedHeight &&
        edit->maximumHeight() == boundedHeight) {
        return;
    }

    edit->setFixedHeight(boundedHeight);
    edit->updateGeometry();
}

static int metricLineHeight(const QFont& font)
{
    return qMax(1, qCeil(QFontMetricsF(font).lineSpacing()));
}

static int renderedLineHeight(QTextEdit* editor)
{
    if (!editor)
        return 1;

    // Qt 5/X11 can report a 17 px font line while QTextLayout and the caret
    // actually paint an 18 px box. Use the live box only for control sizing;
    // block formatting remains font-based so a late platform layout change
    // never appends an invisible command to the user's Undo stack.
    // zh_CN: Qt 5/X11 可能报告 17 px 字体行距，但 QTextLayout 与光标实际
    // 绘制 18 px。真实盒仅用于控件尺寸计算；block 格式仍基于字体度量，避免
    // 平台延迟布局变化向用户撤销栈追加不可见命令。
    int height = metricLineHeight(editor->font());
    QTextDocument* document = editor->document();
    if (!document)
        return qMax(1, height);

    document->documentLayout()->documentSize();
    const QTextBlock firstBlock = document->begin();
    if (!firstBlock.isValid())
        return qMax(1, height);

    if (QTextLayout* layout = firstBlock.layout(); layout && layout->lineCount() > 0) {
        height = qMax(height, qCeil(layout->lineAt(0).height()));
    }
    height = qMax(height, editor->cursorRect(QTextCursor(firstBlock)).height());
    return qMax(1, height);
}

static int effectiveLineHeight(const QFont& font, int requestedLineHeight)
{
    return qMax(requestedLineHeight, metricLineHeight(font));
}

static int verticalMarginOverflow(const QFont& font, int lineHeight, const QMargins& margins)
{
    const int fontLh = metricLineHeight(font);
    const int slotSlack = effectiveLineHeight(font, lineHeight) - fontLh;
    return qMax(0, margins.top() + margins.bottom() - slotSlack);
}

static QMargins calcContentViewportMargins(const QFont& font, int lineHeight,
                                           const QMargins& margins)
{
    const int fontLh = metricLineHeight(font);
    const int slotSlack = effectiveLineHeight(font, lineHeight) - fontLh;
    const int requestedTop = qMax(0, margins.top());
    const int requestedBottom = qMax(0, margins.bottom());
    const int centeredSlack = qMax(0, slotSlack - requestedTop - requestedBottom);
    const int top = requestedTop + centeredSlack / 2;
    const int bottom = requestedBottom + centeredSlack - centeredSlack / 2;
    return QMargins(qMax(0, margins.left()), top, qMax(0, margins.right()), bottom);
}

static int calcBotPad(const QFont& font, int lineHeight)
{
    return effectiveLineHeight(font, lineHeight) - metricLineHeight(font);
}

static int renderedLinePitch(QTextEdit* editor, int requestedLineHeight)
{
    if (!editor)
        return qMax(1, requestedLineHeight);
    return renderedLineHeight(editor) + calcBotPad(editor->font(), requestedLineHeight);
}

static bool formatMetricEquals(qreal lhs, qreal rhs)
{
    return qAbs(lhs - rhs) < 0.01;
}

// ── Inner editor. zh_CN: 内部编辑器 ────────────────────────────────────────────
//
// QTextEdit is used (not QPlainTextEdit) because QTextDocumentLayout natively
// honors QTextBlockFormat line spacing. Each text line and the caret center
// vertically inside the lineHeight slot via:
//   - viewport top/bottom margins = requested insets + remaining centered slack
//   - LineDistanceHeight = lineHeight - fontLh          … inter-line spacing
// Qt then handles caret placement, selection, and hit testing without a
// custom paintEvent.
// zh_CN: 使用 QTextEdit（而非 QPlainTextEdit），因为 QTextDocumentLayout 原生
// 支持 QTextBlockFormat 行距。借助上述两条公式，每行
// 文本与光标在 lineHeight 槽内垂直居中；光标定位、选区高亮、点击映射均由 Qt
// 自动处理，无需自定义 paintEvent。

class InnerTextEdit : public QTextEdit {
public:
    explicit InnerTextEdit(TextEdit* owner, QWidget* parent = nullptr)
        : QTextEdit(parent), m_owner(owner)
    {
        setAcceptRichText(false);
    }

    void setContentViewportMargins(int left, int top, int right, int bottom)
    {
        setViewportMargins(left, top, right, bottom);
    }

    bool hasActivePreedit() const { return m_hasPreedit; }

protected:
    void contextMenuEvent(QContextMenuEvent* event) override
    {
        if (!event)
            return;

        // Keep QTextEdit's standard editing actions and their enabled state,
        // but host them in FluentMenu so the popup does not inherit this
        // editor's transparent Base/Window palette. Native QMenu renders that
        // inherited palette differently across Windows 10 and Windows 11.
        // zh_CN: 保留 QTextEdit 标准编辑动作及其启用状态，但改由 FluentMenu
        // 承载，避免原生 QMenu 继承编辑器透明的 Base/Window 调色板；Win10
        // 与 Win11 对该透明调色板的回退绘制并不一致。
        auto* standardMenu = createStandardContextMenu(event->pos());
        if (!::fluent::menus_toolbars::detail::showTextEditingContextMenu(
                this, standardMenu, event->globalPos(),
                QStringLiteral("FluentTextEdit.ContextMenu"))) {
            event->ignore();
            return;
        }
        event->accept();
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (m_owner && handleBoundaryWheel(event))
            return;
        QTextEdit::wheelEvent(event);
        if (m_owner && !m_owner->isScrollChainingEnabled() && hasScrollableRange())
            event->accept();
    }

    void paintEvent(QPaintEvent* e) override
    {
        QTextEdit::paintEvent(e);
        // Paint the placeholder on the same viewport and font metrics as real
        // text so empty and populated states keep identical alignment.
        // zh_CN: placeholder 与真实文本共用 viewport 和字体度量，自绘以保证
        // 空态与有内容状态对齐一致。
        if (!m_owner || !document()->isEmpty() || m_hasPreedit)
            return;
        const QString ph = m_owner->placeholderText();
        if (ph.isEmpty())
            return;

        QPainter painter(viewport());
        const int fontLh = renderedLineHeight(this);
        QRect textRect(0, 0, viewport()->width(), fontLh);
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.setFont(font());
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, ph);
    }

    void inputMethodEvent(QInputMethodEvent* event) override
    {
        const bool hasPreedit = event && !event->preeditString().isEmpty();
        const bool preeditStateChanged = m_hasPreedit != hasPreedit;
        // Update the state before QTextEdit emits textChanged/documentSizeChanged
        // so the owner can keep composition geometry stable until commit.
        // zh_CN: 在 QTextEdit 发出 textChanged/documentSizeChanged 前更新状态，
        // 让外层在正式提交前保持组字期间的几何稳定。
        m_hasPreedit = hasPreedit;
        QTextEdit::inputMethodEvent(event);
        if (!preeditStateChanged)
            return;
        if (viewport())
            viewport()->update();
    }

private:
    bool hasScrollableRange() const
    {
        const QScrollBar* bar = verticalScrollBar();
        return bar && bar->maximum() > bar->minimum();
    }

    bool handleBoundaryWheel(QWheelEvent* event)
    {
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
        const bool boundaryTail = (atTop && scrollPx > 0.0) || (atBottom && scrollPx < 0.0);
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
    bool m_hasPreedit = false;
};

// ── Construction. zh_CN: 构造 ───────────────────────────────────────────────────

TextEdit::TextEdit(QWidget* parent) : QWidget(parent)
{
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
    setFocusPolicy(m_editor->focusPolicy());
    setFocusProxy(m_editor);

    auto* heightAnimation = new TextEditHeightAnimation(this);
    heightAnimation->setObjectName(QString::fromLatin1(kHeightAnimationObjectName));
    connect(heightAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) { applyTextEditHeight(this, value.toInt()); });
    connect(heightAnimation, &QVariantAnimation::finished, this, [this, heightAnimation]() {
        if (heightAnimation->targetHeight > 0)
            applyTextEditHeight(this, heightAnimation->targetHeight);
        heightAnimation->targetHeight = 0;
    });

    // Remove the document's default padding; viewport margins and block line
    // spacing own the geometry.
    // zh_CN: 移除文档默认四周留白，由 viewport margin 与 block 行距统一控制。
    m_editor->document()->setDocumentMargin(0);

    // New blocks inherit the current QTextBlockFormat, so ordinary typing
    // makes applyBlockCenterFormat() a no-op. A direct QTextEdit/AX value
    // replacement can reset those formats, though; re-check them here so the
    // real editing surface keeps the same line geometry as setPlainText().
    // The helper compares formats before merging, which preserves the user's
    // normal Undo stack instead of appending an invisible command per key.
    // zh_CN: 新 block 会继承当前 QTextBlockFormat，普通输入时
    // applyBlockCenterFormat() 不会写入任何格式。但直接通过 QTextEdit/辅助
    // 功能替换值可能重置格式，因此在真实编辑入口重新校验，保证它与
    // setPlainText() 的行几何一致；helper 仅在格式确有变化时 merge，避免每次
    // 按键都向 Undo 栈加入不可见命令。
    connect(m_editor, &QTextEdit::textChanged, this, [this]() {
        if (m_updatingFormat)
            return;
        const bool hasActivePreedit =
            static_cast<const InnerTextEdit*>(m_editor)->hasActivePreedit();
        auto* heightAnimation = heightAnimationFor(this);
        if (heightAnimation) {
            heightAnimation->animateNextUpdate = isVisible() && m_editor->hasFocus() &&
                                                 !heightAnimation->programmaticTextChange &&
                                                 !hasActivePreedit;
        }
        applyBlockCenterFormat();
        if (!hasActivePreedit)
            scheduleHeightForContentUpdate();
        emit textChanged();
    });
    connect(m_editor, &QTextEdit::cursorPositionChanged, this, &TextEdit::cursorPositionChanged);
    connect(m_editor, &QTextEdit::selectionChanged, this, &TextEdit::selectionChanged);
    connect(m_editor->document()->documentLayout(),
            &QAbstractTextDocumentLayout::documentSizeChanged, this,
            [this](const QSizeF&) { scheduleHeightForContentUpdate(); });
    m_editor->installEventFilter(this);

    // Custom fluent scroll bar. zh_CN: 自定义 Fluent 滚动条。
    m_vScrollBar = new ::fluent::scrolling::ScrollBar(Qt::Vertical, this);
    m_vScrollBar->hide();

    auto* innerVBar = m_editor->verticalScrollBar();
    m_vScrollBar->setRange(innerVBar->minimum(), innerVBar->maximum());
    m_vScrollBar->setPageStep(innerVBar->pageStep());
    m_vScrollBar->setValue(innerVBar->value());

    connect(innerVBar, &QScrollBar::rangeChanged, this,
            [this, innerVBar](int /*min*/, int /*max*/) {
                if (!m_vScrollBar)
                    return;
                m_vScrollBar->setRange(innerVBar->minimum(), innerVBar->maximum());
                m_vScrollBar->setPageStep(innerVBar->pageStep());
                // Scroll bar visibility is owned by updateHeightForContent.
                // zh_CN: 滚动条可见性由 updateHeightForContent 统一管理。
            });
    connect(innerVBar, &QScrollBar::valueChanged, this, [this](int v) {
        if (m_vScrollBar && m_vScrollBar->value() != v)
            m_vScrollBar->setValue(v);
    });
    connect(m_vScrollBar, &QScrollBar::valueChanged, this, [innerVBar](int v) {
        if (innerVBar->value() != v)
            innerVBar->setValue(v);
    });

    // Initial theme, line format, then height (order matters). zh_CN: 初始主题 + 行格式 + 高度（顺序重要）。
    onThemeUpdated();
    updateHeightForContent();
}

// ── Text API. zh_CN: 文本 API ───────────────────────────────────────────────────

void TextEdit::setPlainText(const QString& text)
{
    if (m_editor) {
        if (m_editor->toPlainText() == text)
            return;
        auto* heightAnimation = heightAnimationFor(this);
        const bool previousProgrammaticState = heightAnimation->programmaticTextChange;
        heightAnimation->programmaticTextChange = true;
        m_editor->setPlainText(text);
        heightAnimation->programmaticTextChange = previousProgrammaticState;
        applyBlockCenterFormat();
        requestSynchronousHeightSettlement(this);
        updateHeightForContent();
    }
}

QString TextEdit::toPlainText() const
{
    return m_editor ? m_editor->toPlainText() : QString();
}

void TextEdit::clear()
{
    if (m_editor) {
        if (m_editor->toPlainText().isEmpty())
            return;
        auto* heightAnimation = heightAnimationFor(this);
        const bool previousProgrammaticState = heightAnimation->programmaticTextChange;
        heightAnimation->programmaticTextChange = true;
        m_editor->clear();
        heightAnimation->programmaticTextChange = previousProgrammaticState;
        applyBlockCenterFormat();
        requestSynchronousHeightSettlement(this);
        updateHeightForContent();
    }
}

void TextEdit::setPlaceholderText(const QString& text)
{
    m_placeholderText = text;
    if (m_editor && m_editor->viewport())
        m_editor->viewport()->update();
}

QString TextEdit::placeholderText() const
{
    return m_placeholderText;
}

void TextEdit::setReadOnly(bool readOnly)
{
    if (m_editor)
        m_editor->setReadOnly(readOnly);
}

bool TextEdit::isReadOnly() const
{
    return m_editor ? m_editor->isReadOnly() : false;
}

::fluent::scrolling::ScrollBar* TextEdit::verticalScrollBar() const
{
    return m_vScrollBar;
}

void TextEdit::setFocus()
{
    if (m_editor)
        m_editor->setFocus(Qt::OtherFocusReason);
}

void TextEdit::setFocus(Qt::FocusReason reason)
{
    if (m_editor)
        m_editor->setFocus(reason);
}

// ── Events. zh_CN: 事件 ─────────────────────────────────────────────────────────

void TextEdit::paintEvent(QPaintEvent* event)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    paintFrame(p);
    QWidget::paintEvent(event);
}

void TextEdit::resizeEvent(QResizeEvent* event)
{
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
        if (event->oldSize().width() != event->size().width())
            scheduleHeightForContentUpdate();
    }
}

void TextEdit::paintFrame(QPainter& painter)
{
    const auto& colors = themeColorsRef();
    const auto& radius = themeRadius();

    // Fluent field treatment: fill + border + bottom accent underline
    // on focus. zh_CN: Fluent 字段使用填充、边框及聚焦时的底部强调线。
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
        bgColor = effectiveThemeUsesDarkAppearance() ? colors.bgSolid : colors.controlDefault;
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
        qreal bottomY =
            bgRect.bottom() - (bottomBorderWidth > 1 ? (bottomBorderWidth - 1) / 2.0 : 0);
        QPainterPath bottomPath;
        bottomPath.moveTo(bgRect.left() + r, bottomY);
        bottomPath.lineTo(bgRect.right() - r, bottomY);
        painter.drawPath(bottomPath);
    }
}

void TextEdit::enterEvent(FluentEnterEvent* event)
{
    m_isHovered = true;
    update();
    QWidget::enterEvent(event);
}

void TextEdit::leaveEvent(QEvent* event)
{
    m_isHovered = false;
    update();
    QWidget::leaveEvent(event);
}

void TextEdit::focusInEvent(QFocusEvent* event)
{
    m_isFocused = true;
    update();
    QWidget::focusInEvent(event);
}

void TextEdit::focusOutEvent(QFocusEvent* event)
{
    m_isFocused = false;
    update();
    QWidget::focusOutEvent(event);
}

bool TextEdit::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_editor) {
        if (event->type() == QEvent::FocusIn) {
            m_isFocused = true;
            update();
        } else if (event->type() == QEvent::FocusOut) {
            m_isFocused = false;
            update();
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ── Property setters. zh_CN: 属性 setter ────────────────────────────────────────

void TextEdit::setContentMargins(const QMargins& margins)
{
    if (m_contentMargins == margins)
        return;
    m_contentMargins = margins;
    applyThemeStyle();
    requestSynchronousHeightSettlement(this);
    updateHeightForContent();
    if (m_editor && m_editor->viewport())
        m_editor->viewport()->update();
    emit contentMarginsChanged();
}

void TextEdit::setFontRole(Typography::FontRole role)
{
    if (m_fontRole == role)
        return;
    m_fontRole = role;
    applyThemeStyle();
    requestSynchronousHeightSettlement(this);
    updateHeightForContent();
    if (m_editor && m_editor->viewport())
        m_editor->viewport()->update();
    emit fontRoleChanged();
}

void TextEdit::setFocusedBorderWidth(int width)
{
    if (m_focusedBorderWidth == width)
        return;
    m_focusedBorderWidth = width;
    update();
    emit focusedBorderWidthChanged();
}

void TextEdit::setUnfocusedBorderWidth(int width)
{
    if (m_unfocusedBorderWidth == width)
        return;
    m_unfocusedBorderWidth = width;
    update();
    emit unfocusedBorderWidthChanged();
}

void TextEdit::setLineHeight(int height)
{
    if (height <= 0 || m_lineHeight == height)
        return;
    m_lineHeight = height;
    applyBlockCenterFormat();
    requestSynchronousHeightSettlement(this);
    updateHeightForContent();
    emit layoutMetricsChanged();
}

void TextEdit::setMinVisibleLines(int lines)
{
    if (lines <= 0)
        return;
    const bool maxChanged = m_maxVisibleLines < lines;
    if (m_minVisibleLines == lines && !maxChanged)
        return;
    m_minVisibleLines = lines;
    if (maxChanged)
        m_maxVisibleLines = lines;
    requestSynchronousHeightSettlement(this);
    updateHeightForContent();
    emit layoutMetricsChanged();
}

void TextEdit::setMaxVisibleLines(int lines)
{
    if (lines <= 0)
        return;
    const bool minChanged = m_minVisibleLines > lines;
    if (m_maxVisibleLines == lines && !minChanged)
        return;
    m_maxVisibleLines = lines;
    if (minChanged)
        m_minVisibleLines = lines;
    requestSynchronousHeightSettlement(this);
    updateHeightForContent();
    emit layoutMetricsChanged();
}

void TextEdit::setTabChangesFocus(bool enabled)
{
    if (m_tabChangesFocus == enabled)
        return;
    m_tabChangesFocus = enabled;
    if (m_editor)
        m_editor->setTabChangesFocus(enabled);
    emit tabChangesFocusChanged();
}

void TextEdit::setScrollChainingEnabled(bool enabled)
{
    if (m_scrollChainingEnabled == enabled)
        return;
    m_scrollChainingEnabled = enabled;
    emit scrollChainingEnabledChanged();
}

void TextEdit::onThemeUpdated()
{
    QScrollBar* innerScrollBar = m_editor ? m_editor->verticalScrollBar() : nullptr;
    const bool hadScrollableRange =
        innerScrollBar && innerScrollBar->maximum() > innerScrollBar->minimum();
    const int previousValue = innerScrollBar ? innerScrollBar->value() : 0;
    const bool wasAtTop = hadScrollableRange && previousValue <= innerScrollBar->minimum();
    const bool wasAtBottom = hadScrollableRange && previousValue >= innerScrollBar->maximum();

    applyThemeStyle();
    requestSynchronousHeightSettlement(this);
    updateHeightForContent();

    // A parent Fluent surface may update its style sheet later in the same
    // theme-notification pass. Qt then repolishes child widgets and can restore
    // the platform selection palette. Reapply only the color palette after the
    // notification stack unwinds; geometry stays untouched.
    // zh_CN: 同一轮主题通知中，父级 Fluent 容器可能稍后更新样式表，Qt 会重新
    // polish 子控件并恢复平台默认选区色。通知栈结束后仅重应用调色板，不改几何。
    QTimer::singleShot(0, this, [this]() {
        applyEditorPalette();
        if (m_editor && m_editor->viewport())
            m_editor->viewport()->update();
    });

    // QTextDocument may finish a palette/font relayout after this callback.
    // Preserve a logical boundary anchor instead of retaining a stale pixel
    // offset that can expose a clipped line after a Light/Dark transition.
    // zh_CN: QTextDocument 可能在本回调后才完成调色板/字体重排。主题切换时
    // 保留顶部或尾部的逻辑锚点，避免沿用旧像素偏移而露出半行。
    const auto restoreScrollAnchor = [this, previousValue, wasAtTop, wasAtBottom]() {
        if (!m_editor)
            return;
        m_editor->document()->documentLayout()->documentSize();
        requestSynchronousHeightSettlement(this);
        updateHeightForContent();
        QScrollBar* bar = m_editor->verticalScrollBar();
        if (!bar || bar->maximum() <= bar->minimum())
            return;
        if (wasAtBottom)
            bar->setValue(bar->maximum());
        else if (wasAtTop)
            bar->setValue(bar->minimum());
        else
            bar->setValue(qBound(bar->minimum(), previousValue, bar->maximum()));
    };
    restoreScrollAnchor();
    if (hadScrollableRange)
        QTimer::singleShot(0, this, restoreScrollAnchor);
}

// ── Core internals. zh_CN: 核心私有方法 ─────────────────────────────────────────

void TextEdit::applyEditorPalette()
{
    if (!m_editor)
        return;
    const auto& c = themeColorsRef();
    QPalette pal = palette();
    pal.setColor(QPalette::Base, Qt::transparent);
    pal.setColor(QPalette::Window, Qt::transparent);
    pal.setColor(QPalette::Text, c.textPrimary);
    pal.setColor(QPalette::PlaceholderText, c.textSecondary);
    pal.setColor(QPalette::Highlight, c.accentDefault);
    pal.setColor(QPalette::HighlightedText, c.textOnAccent);
    pal.setColor(QPalette::Inactive, QPalette::Highlight, c.accentDefault);
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText, c.textOnAccent);
    pal.setColor(QPalette::Disabled, QPalette::Text, c.textDisabled);
    pal.setColor(QPalette::Disabled, QPalette::PlaceholderText, c.textDisabled);
    m_editor->setPalette(pal);
}

void TextEdit::applyThemeStyle()
{
    if (!m_editor)
        return;

    applyEditorPalette();
    const QFont roleFont = themeFont(m_fontRole).toQFont();
    if (m_editor->font() != roleFont)
        m_editor->setFont(roleFont);

    // Geometry-affecting QSS stays theme-independent. Text and selection
    // colors come from QPalette, so a Light/Dark transition does not force
    // QTextDocument to rebuild otherwise unchanged line geometry.
    // zh_CN: 影响几何的 QSS 与主题无关；文本和选区颜色完全由 QPalette
    // 提供，避免 Light/Dark 切换重建未变化的 QTextDocument 行布局。
    const QString editorQss =
        QStringLiteral("QTextEdit { background: transparent; border: none; }");
    if (m_editor->styleSheet() != editorQss)
        m_editor->setStyleSheet(editorQss);

    if (auto* vp = m_editor->viewport()) {
        vp->setAutoFillBackground(false);
        QPalette vpal = vp->palette();
        vpal.setColor(QPalette::Base, Qt::transparent);
        vpal.setColor(QPalette::Window, Qt::transparent);
        vp->setPalette(vpal);
        const QString viewportQss = QStringLiteral("background: transparent; border: none;");
        if (vp->styleSheet() != viewportQss)
            vp->setStyleSheet(viewportQss);
    }

    // Recompute centering margins after font changes (fontLineSpacing may differ).
    // zh_CN: 字体变更后重算居中 margin（fontLineSpacing 可能不同）。
    applyBlockCenterFormat();
}

void TextEdit::applyBlockCenterFormat()
{
    if (!m_editor)
        return;

    m_updatingFormat = true;

    const QFont f = m_editor->font();
    const int botPad = calcBotPad(f, m_lineHeight);

    // 1. Keep outer insets out of the document. LineDistanceHeight contributes
    // spacing after the final visual line as well as between lines; cancel that
    // trailing distance at the root frame so the maximum scroll position lands
    // on a complete first tail line. Viewport margins own both outer edges.
    // zh_CN: 外围 inset 不进入文档。LineDistanceHeight 除行间距外还会在末行后
    // 留出距离；在 root frame 抵消这段尾距，使最大滚动位置落在完整行边界。
    // 上下外围间距仍统一交给 viewport margin。
    QTextFrame* rootFrame = m_editor->document()->rootFrame();
    QTextFrameFormat rff = rootFrame->frameFormat();
    const qreal trailingLineDistance = -static_cast<qreal>(botPad);
    const bool frameFormatChanged = !formatMetricEquals(rff.topMargin(), 0) ||
                                    !formatMetricEquals(rff.bottomMargin(), trailingLineDistance) ||
                                    !formatMetricEquals(rff.leftMargin(), 0) ||
                                    !formatMetricEquals(rff.rightMargin(), 0);
    if (frameFormatChanged) {
        rff.setTopMargin(0);
        rff.setBottomMargin(trailingLineDistance);
        rff.setLeftMargin(0);
        rff.setRightMargin(0);
        rootFrame->setFrameFormat(rff);
    }

    // 2. LineDistanceHeight: each visual line = fontLh + botPad = lineHeight.
    //    bottomMargin is avoided; combined with LineDistanceHeight it would
    //    double the line spacing.
    // zh_CN: LineDistanceHeight — 每个视觉行高 = fontLh + botPad = lineHeight；
    // 不使用 bottomMargin，否则与 LineDistanceHeight 重叠导致行间距翻倍。
    QTextBlockFormat fmt;
    fmt.setLineHeight(botPad, QTextBlockFormat::LineDistanceHeight);
    fmt.setBottomMargin(0);

    // QTextCursor formatting participates in QTextDocument's undo stack even
    // when the merged values are identical. Avoid a no-op merge so theme and
    // palette refreshes never consume the user's next Undo command.
    // zh_CN: QTextCursor 的格式操作即使数值未变化也会进入撤销栈；先判断现有
    // block 格式，避免主题/调色板刷新吞掉用户下一次 Undo。
    bool blockFormatChanged = false;
    for (QTextBlock block = m_editor->document()->begin(); block.isValid(); block = block.next()) {
        const QTextBlockFormat current = block.blockFormat();
        if (current.lineHeightType() != fmt.lineHeightType() ||
            !formatMetricEquals(current.lineHeight(), fmt.lineHeight()) ||
            !formatMetricEquals(current.bottomMargin(), fmt.bottomMargin())) {
            blockFormatChanged = true;
            break;
        }
    }

    if (blockFormatChanged) {
        QTextCursor cursor(m_editor->document());
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        cursor.mergeBlockFormat(fmt);
    }

    // 3. Viewport margins own the requested outer insets and distribute any
    // line-slot slack that remains after satisfying them. The text viewport
    // therefore contains whole visual lines at both scroll boundaries.
    // zh_CN: viewport margin 承担外部 inset，并分配满足 inset 后剩余的行槽
    // 余量，因此滚动顶部和尾部都只显示完整视觉行。
    const QMargins viewportMargins = calcContentViewportMargins(f, m_lineHeight, m_contentMargins);
    static_cast<InnerTextEdit*>(m_editor)->setContentViewportMargins(
        viewportMargins.left(), viewportMargins.top(), viewportMargins.right(),
        viewportMargins.bottom());

    m_updatingFormat = false;
}

void TextEdit::scheduleHeightForContentUpdate()
{
    if (!m_editor || static_cast<const InnerTextEdit*>(m_editor)->hasActivePreedit())
        return;
    if (m_heightUpdateScheduled)
        return;
    m_heightUpdateScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        m_heightUpdateScheduled = false;
        updateHeightForContent();
    });
}

void TextEdit::updateHeightForContent()
{
    if (!m_editor || m_updatingHeight)
        return;
    m_updatingHeight = true;

    // Asking the document layout for its size flushes any pending width-driven
    // wrap recalculation before individual QTextLayout line counts are read.
    // zh_CN: 读取各 QTextLayout 行数前先查询文档尺寸，刷新宽度变化引起的
    // 待处理换行布局。
    m_editor->document()->documentLayout()->documentSize();

    // Count all visual lines, including wrap-generated ones. zh_CN: 统计所有可视行数（包括自动换行产生的行）。
    int visualLines = 0;
    QTextBlock block = m_editor->document()->begin();
    while (block.isValid()) {
        int lc = block.layout()->lineCount();
        visualLines += (lc > 0) ? lc : 1;
        block = block.next();
    }
    if (visualLines < 1)
        visualLines = 1;

    const int minimumLines = qMax(1, m_minVisibleLines);
    const int maximumLines = qMax(minimumLines, m_maxVisibleLines);
    const int clamped = qBound(minimumLines, visualLines, maximumLines);

    // The normal Fluent margin fits inside the line slot and therefore keeps a
    // one-line editor equal to other 32 px controls. Larger caller-provided
    // vertical insets add only the overflow required to keep both edges real.
    // zh_CN: 默认 Fluent 内边距包含在行槽中，因此单行编辑器仍与 32px 控件等高；
    // 调用方设置更大的上下内边距时，仅补足超出行槽余量的高度。
    const int marginOverflow =
        verticalMarginOverflow(m_editor->font(), m_lineHeight, m_contentMargins);
    const int targetHeight = clamped * renderedLinePitch(m_editor, m_lineHeight) + marginOverflow;

    // The scroll bar only appears once content exceeds maxVisibleLines.
    // zh_CN: 滚动条仅在内容实际超过 maxVisibleLines 时显示。
    m_scrollEnabled = (visualLines > maximumLines);
    if (m_vScrollBar)
        m_vScrollBar->setVisible(m_scrollEnabled);
    // Reset the inner scroll position when not needed to avoid content drift.
    // zh_CN: 无需滚动时重置内部滚动位置，避免内容偏移。
    if (!m_scrollEnabled && m_editor)
        m_editor->verticalScrollBar()->setValue(0);

    TextEditHeightAnimation* heightAnimation = heightAnimationFor(this);
    const bool animateHeight = heightAnimation && heightAnimation->animateNextUpdate;
    const bool settleSynchronously =
        heightAnimation && heightAnimation->settleSynchronouslyNextUpdate;
    if (heightAnimation) {
        heightAnimation->animateNextUpdate = false;
        heightAnimation->settleSynchronouslyNextUpdate = false;
    }

    // Preserve an active user transition across its own same-target document
    // relayout. Explicit text and metric setters still stop and settle
    // synchronously, even when their target matches the transition.
    // zh_CN: 动画自身触发同目标文档重排时继续当前用户过渡；显式文本与度量
    // setter 即使目标相同，仍会停止动画并同步收敛。
    if (!settleSynchronously && heightAnimation &&
        heightAnimation->state() == QAbstractAnimation::Running &&
        heightAnimation->targetHeight == targetHeight) {
        m_updatingHeight = false;
        return;
    }

    if (heightAnimation)
        heightAnimation->stop();

    if (!settleSynchronously && animateHeight && heightAnimation && targetHeight != height()) {
        heightAnimation->setStartValue(height());
        heightAnimation->setEndValue(targetHeight);
        heightAnimation->setEasingCurve(themeAnimation().decelerate);
        heightAnimation->targetHeight = targetHeight;
        m_updatingHeight = false;
        ::fluent::detail::startMotionTransition(heightAnimation, themeAnimation().fast);
        return;
    }

    if (heightAnimation)
        heightAnimation->targetHeight = 0;
    applyTextEditHeight(this, targetHeight);

    m_updatingHeight = false;
}

} // namespace fluent::textfields
