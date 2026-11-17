#include "components/foundation/private/TextInputFrame_p.h"
#include "compatibility/InputEventCompat.h"
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
    auto* animation = edit
        ? edit->findChild<QVariantAnimation*>(QString::fromLatin1(kHeightAnimationObjectName),
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
    if (edit->height() == boundedHeight && edit->minimumHeight() == boundedHeight
        && edit->maximumHeight() == boundedHeight) {
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

    // Qt 5/X11 can report fontLineSpacing 1 px smaller than QTextLayout paints; use live layout height
    // for widget sizing while keeping block formats font-based so late relayouts never push Undo entries.
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

static QMargins calcContentViewportMargins(const QFont& font, int lineHeight, const QMargins& margins)
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

// Use QTextEdit (not QPlainTextEdit) so QTextDocumentLayout honors QTextBlockFormat line spacing.
// Viewport margins handle outer padding + centering slack, and LineDistanceHeight handles inter-line gaps.
class InnerTextEdit : public QTextEdit {
public:
    explicit InnerTextEdit(TextEdit* owner, QWidget* parent = nullptr) : QTextEdit(parent), m_owner(owner)
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

        // Host QTextEdit's actions in FluentMenu so native QMenu does not inherit the transparent palette inconsistently on Win10/11.
        auto* standardMenu = createStandardContextMenu(event->pos());
        if (!::fluent::menus_toolbars::detail::showTextEditingContextMenu(this, standardMenu, event->globalPos(),
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
        // Paint placeholder directly on the viewport so alignment matches real text metrics.
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
        // Update preedit state before QTextEdit emits change signals so height remains stable during IME composition.
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
    connect(heightAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        applyTextEditHeight(this, value.toInt());
    });
    connect(heightAnimation, &QVariantAnimation::finished, this, [this, heightAnimation]() {
        if (heightAnimation->targetHeight > 0)
            applyTextEditHeight(this, heightAnimation->targetHeight);
        heightAnimation->targetHeight = 0;
    });

    // Clear default document margin so viewport margins and block spacing control layout.
    m_editor->document()->setDocumentMargin(0);

    // Re-check block formatting after edits in case direct/accessibility replacements reset it;
    // applyBlockCenterFormat() skips unchanged blocks to keep the Undo stack clean.
    connect(m_editor, &QTextEdit::textChanged, this, [this]() {
        if (m_updatingFormat)
            return;
        const bool hasActivePreedit = static_cast<const InnerTextEdit*>(m_editor)->hasActivePreedit();
        auto* heightAnimation = heightAnimationFor(this);
        if (heightAnimation) {
            heightAnimation->animateNextUpdate = isVisible() && m_editor->hasFocus()
                && !heightAnimation->programmaticTextChange && !hasActivePreedit;
        }
        applyBlockCenterFormat();
        if (!hasActivePreedit)
            scheduleHeightForContentUpdate();
        emit textChanged();
        scheduleCursorRectUpdate();
    });
    connect(m_editor, &QTextEdit::cursorPositionChanged, this, &TextEdit::cursorPositionChanged);
    connect(m_editor, &QTextEdit::cursorPositionChanged, this, &TextEdit::scheduleCursorRectUpdate);
    connect(m_editor, &QTextEdit::selectionChanged, this, &TextEdit::selectionChanged);
    connect(m_editor->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this,
        [this](const QSizeF&) {
            scheduleHeightForContentUpdate();
            scheduleCursorRectUpdate();
        });
    m_editor->installEventFilter(this);
    m_editor->viewport()->installEventFilter(this);

    m_vScrollBar = new ::fluent::scrolling::ScrollBar(Qt::Vertical, this);
    m_vScrollBar->hide();

    auto* innerVBar = m_editor->verticalScrollBar();
    m_vScrollBar->setRange(innerVBar->minimum(), innerVBar->maximum());
    m_vScrollBar->setPageStep(innerVBar->pageStep());
    m_vScrollBar->setValue(innerVBar->value());

    connect(innerVBar, &QScrollBar::rangeChanged, this, [this, innerVBar](int /*min*/, int /*max*/) {
        if (!m_vScrollBar)
            return;
        m_vScrollBar->setRange(innerVBar->minimum(), innerVBar->maximum());
        m_vScrollBar->setPageStep(innerVBar->pageStep());
        if (!m_autoResizeEnabled) {
            m_scrollEnabled = innerVBar->maximum() > innerVBar->minimum();
            m_vScrollBar->setVisible(m_scrollEnabled);
            updateEditorGeometry();
        }
        // Auto-resizing scroll bar visibility is owned by updateHeightForContent.
        // zh_CN: 滚动条可见性由 updateHeightForContent 统一管理。
    });
    connect(innerVBar, &QScrollBar::valueChanged, this, [this](int v) {
        if (m_vScrollBar && m_vScrollBar->value() != v)
            m_vScrollBar->setValue(v);
        scheduleCursorRectUpdate();
    });
    connect(m_editor->horizontalScrollBar(), &QScrollBar::valueChanged, this, &TextEdit::scheduleCursorRectUpdate);
    connect(m_vScrollBar, &QScrollBar::valueChanged, this, [innerVBar](int v) {
        if (innerVBar->value() != v)
            innerVBar->setValue(v);
    });

    // Initial theme, line format, then height (order matters).
    onThemeUpdated();
    updateHeightForContent();
}

void TextEdit::setAutoResizeEnabled(bool enabled)
{
    if (m_autoResizeEnabled == enabled)
        return;
    m_autoResizeEnabled = enabled;
    requestSynchronousHeightSettlement(this);
    if (!enabled) {
        m_autoResizePolicy = sizePolicy();
        heightAnimationFor(this)->stop();
        heightAnimationFor(this)->targetHeight = 0;
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
        auto policy = m_autoResizePolicy;
        policy.setHorizontalPolicy(QSizePolicy::Expanding);
        policy.setVerticalPolicy(QSizePolicy::Expanding);
        setSizePolicy(policy);
    } else {
        auto temporary = m_autoResizePolicy;
        temporary.setHorizontalPolicy(QSizePolicy::Expanding);
        temporary.setVerticalPolicy(QSizePolicy::Expanding);
        if (sizePolicy() == temporary)
            setSizePolicy(m_autoResizePolicy);
    }
    updateHeightForContent();
    updateGeometry();
    emit autoResizeEnabledChanged();
}

void TextEdit::setFrameVisible(bool visible)
{
    if (m_frameVisible == visible)
        return;
    m_frameVisible = visible;
    update();
    emit frameVisibleChanged();
}

QTextCursor TextEdit::textCursor() const
{
    return m_editor->textCursor();
}

QRect TextEdit::cursorRect() const
{
    return m_editor->cursorRect().translated(m_editor->viewport()->mapTo(this, QPoint()));
}

void TextEdit::scheduleCursorRectUpdate()
{
    if (m_cursorRectUpdateScheduled)
        return;
    m_cursorRectUpdateScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        m_cursorRectUpdateScheduled = false;
        const QRect current = cursorRect();
        if (current == m_lastCursorRect)
            return;
        m_lastCursorRect = current;
        emit cursorRectChanged();
    });
}

void TextEdit::setTextCursor(const QTextCursor& cursor)
{
    if (cursor.document() == m_editor->document())
        m_editor->setTextCursor(cursor);
}

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

void TextEdit::appendPlainText(const QString& text)
{
    if (!m_editor)
        return;

    auto* scrollBar = m_editor->verticalScrollBar();
    const int previousScroll = scrollBar->value();
    const bool followTail = previousScroll >= scrollBar->maximum();
    QTextCursor previousCursor = m_editor->textCursor();
    const int previousPosition = previousCursor.position();
    const int previousAnchor = previousCursor.anchor();
    auto* heightAnimation = heightAnimationFor(this);
    const bool previousProgrammaticState = heightAnimation->programmaticTextChange;
    heightAnimation->programmaticTextChange = true;

    // An independent cursor inserts at the tail without replacing a selection.
    // Group the paragraph break and text into one change and one undo command.
    // zh_CN: 独立光标在末尾插入，避免替换选区；换段和文本合并为一次变更及撤销。
    QTextCursor cursor(m_editor->document());
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::End);
    if (!m_editor->document()->isEmpty())
        cursor.insertBlock(cursor.blockFormat(), previousCursor.charFormat());
    else
        cursor.setCharFormat(previousCursor.charFormat());
    cursor.insertText(text);
    cursor.endEditBlock();
    heightAnimation->programmaticTextChange = previousProgrammaticState;

    // Document insertion can move an existing selection endpoint at the tail.
    // zh_CN: 文档插入可能移动恰好位于末尾的选区端点，显式保留原位置。
    previousCursor.setPosition(previousAnchor);
    previousCursor.setPosition(previousPosition, QTextCursor::KeepAnchor);
    m_editor->setTextCursor(previousCursor);
    applyBlockCenterFormat();
    requestSynchronousHeightSettlement(this);
    updateHeightForContent();
    scrollBar->setValue(followTail ? scrollBar->maximum() : previousScroll);
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
    updateEditorGeometry();
    if (event->oldSize().width() != event->size().width())
        scheduleHeightForContentUpdate();
}

void TextEdit::updateEditorGeometry()
{
    if (m_editor) {
        const QRect previousCaret = m_editor->cursorRect();
        const QRect previousViewport = m_editor->viewport()->rect();
        const bool hadOversizedVisibleCaret = previousCaret.height() > previousViewport.height()
            && previousCaret.intersects(previousViewport);
        QRect r = rect();
        int sbw = (m_vScrollBar && m_scrollEnabled) ? m_vScrollBar->thickness() : 0;
        m_editor->setGeometry(r.adjusted(0, 0, -sbw, 0));
        // A tall fallback glyph can cause Qt to scroll to the caret bottom before height settles;
        // re-reveal the whole caret once the viewport grows enough, without disturbing scrolled views.
        if (hadOversizedVisibleCaret && m_editor->cursorRect().height() <= m_editor->viewport()->height()) {
            m_editor->ensureCursorVisible();
        }
        if (m_vScrollBar) {
            int x = r.right() - m_vScrollBar->thickness() + 1;
            int y = r.top() + 2;
            int h = r.height() - 4;
            m_vScrollBar->setGeometry(x, y, m_vScrollBar->thickness(), h);
        }
    }
}

void TextEdit::paintFrame(QPainter& painter)
{
    if (!m_frameVisible)
        return;
    ::fluent::detail::TextInputFrameState state;
    state.enabled = isEnabled();
    state.readOnly = isReadOnly();
    state.focused = m_isFocused;
    state.hovered = m_isHovered;
    state.pressed = false;
    state.normalWidth = m_unfocusedBorderWidth;
    state.focusedWidth = m_focusedBorderWidth;
    ::fluent::detail::paintTextInputFrame(painter, QRectF(rect()), themeColorsRef(), themeRadius().control, state);
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
    if (obj == m_editor || obj == m_editor->viewport()) {
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Move:
        case QEvent::Show:
        case QEvent::FontChange:
        case QEvent::LayoutRequest:
        case QEvent::InputMethod:
            scheduleCursorRectUpdate();
            break;
        default:
            break;
        }
    }
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
    const bool hadScrollableRange = innerScrollBar && innerScrollBar->maximum() > innerScrollBar->minimum();
    const int previousValue = innerScrollBar ? innerScrollBar->value() : 0;
    const bool wasAtTop = hadScrollableRange && previousValue <= innerScrollBar->minimum();
    const bool wasAtBottom = hadScrollableRange && previousValue >= innerScrollBar->maximum();

    applyThemeStyle();
    requestSynchronousHeightSettlement(this);
    updateHeightForContent();

    // A parent surface may update its QSS later in the same theme pass and repolish children;
    // reapply only our color palette after the pass finishes.
    QTimer::singleShot(0, this, [this]() {
        applyEditorPalette();
        if (m_editor && m_editor->viewport())
            m_editor->viewport()->update();
    });

    // QTextDocument may finish relayout after this callback; anchor top/bottom logically to avoid clipped half-lines.
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

    // Keep QSS theme-independent (colors live in QPalette) so theme switches don't rebuild unchanged line geometry.
    const QString editorQss = QStringLiteral("QTextEdit { background: transparent; border: none; }");
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

    // Cancel LineDistanceHeight's trailing space after the final line on the root frame
    // so viewport margins own both outer edges and max scroll aligns on a full line.
    QTextFrame* rootFrame = m_editor->document()->rootFrame();
    QTextFrameFormat rff = rootFrame->frameFormat();
    const qreal trailingLineDistance = -static_cast<qreal>(botPad);
    const bool frameFormatChanged = !formatMetricEquals(rff.topMargin(), 0)
        || !formatMetricEquals(rff.bottomMargin(), trailingLineDistance) || !formatMetricEquals(rff.leftMargin(), 0)
        || !formatMetricEquals(rff.rightMargin(), 0);
    if (frameFormatChanged) {
        rff.setTopMargin(0);
        rff.setBottomMargin(trailingLineDistance);
        rff.setLeftMargin(0);
        rff.setRightMargin(0);
        rootFrame->setFrameFormat(rff);
    }

    // Use LineDistanceHeight (not bottomMargin, which would double the spacing).
    QTextBlockFormat fmt;
    fmt.setLineHeight(botPad, QTextBlockFormat::LineDistanceHeight);
    fmt.setBottomMargin(0);

    // Skip unchanged blocks so QTextCursor::mergeBlockFormat() does not push Undo entries on theme updates.
    bool blockFormatChanged = false;
    for (QTextBlock block = m_editor->document()->begin(); block.isValid(); block = block.next()) {
        const QTextBlockFormat current = block.blockFormat();
        if (current.lineHeightType() != fmt.lineHeightType()
            || !formatMetricEquals(current.lineHeight(), fmt.lineHeight())
            || !formatMetricEquals(current.bottomMargin(), fmt.bottomMargin())) {
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

    // Viewport margins own outer padding plus centering slack so scroll edges align to full visual lines.
    const QMargins viewportMargins = calcContentViewportMargins(f, m_lineHeight, m_contentMargins);
    static_cast<InnerTextEdit*>(m_editor)->setContentViewportMargins(viewportMargins.left(), viewportMargins.top(),
        viewportMargins.right(), viewportMargins.bottom());

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
    if (!m_autoResizeEnabled) {
        m_scrollEnabled = m_editor->verticalScrollBar()->maximum() > 0;
        m_vScrollBar->setVisible(m_scrollEnabled);
        updateEditorGeometry();
        return;
    }
    m_updatingHeight = true;

    // Querying documentSize() flushes pending wrap layout before reading block line counts.
    m_editor->document()->documentLayout()->documentSize();

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

    // Default insets fit inside the line slot (keeping 1-line height at 32 px); add only the overflow.
    const int marginOverflow = verticalMarginOverflow(m_editor->font(), m_lineHeight, m_contentMargins);
    const int targetHeight = clamped * renderedLinePitch(m_editor, m_lineHeight) + marginOverflow;

    m_scrollEnabled = (visualLines > maximumLines);
    if (m_vScrollBar)
        m_vScrollBar->setVisible(m_scrollEnabled);
    if (!m_scrollEnabled && m_editor)
        m_editor->verticalScrollBar()->setValue(0);

    TextEditHeightAnimation* heightAnimation = heightAnimationFor(this);
    const bool animateHeight = heightAnimation && heightAnimation->animateNextUpdate;
    const bool settleSynchronously = heightAnimation && heightAnimation->settleSynchronouslyNextUpdate;
    if (heightAnimation) {
        heightAnimation->animateNextUpdate = false;
        heightAnimation->settleSynchronouslyNextUpdate = false;
    }

    // Keep an active transition running on same-target relayouts unless a setter requested synchronous settlement.
    if (!settleSynchronously && heightAnimation && heightAnimation->state() == QAbstractAnimation::Running
        && heightAnimation->targetHeight == targetHeight) {
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
