#include "CodeBlock.h"
#include <QApplication>
#include <QContextMenuEvent>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QHash>
#include <QKeyEvent>
#include <QPainter>
#include <QPointer>
#include <QScrollBar>
#include <QTimer>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextLayout>
#include <QAbstractTextDocumentLayout>
#include <QPaintEvent>
#include <memory>
#include <QVBoxLayout>
#include <QtMath>
#include "components/basicinput/Button.h"
#include "components/foundation/overlay/OverlayPresentation.h"
#include "components/menus_toolbars/Menu.h"
#include "components/scrolling/ScrollView.h"
#include "components/status_info/ToolTip.h"
#include "components/textfields/Label.h"
#include "compatibility/private/ClipboardCompat_p.h"
#include "private/CodeHighlighter_p.h"

namespace fluent::textfields::detail {
class CodeContextMenu final : public menus_toolbars::FluentMenu {
public:
    explicit CodeContextMenu(QWidget* parent) : FluentMenu(QString(), parent)
    {
        setObjectName(QStringLiteral("FluentLabel.ContextMenu"));
        setFontStyle(Typography::FontRole::Caption);
    }

    void onThemeUpdated() override
    {
        FluentMenu::onThemeUpdated();
        const auto spacing = themeSpacing();
        const int shadow = contentsMargins().left();
        const int inset = shadow + qMax(1, spacing.gap.tight / 2);
        setContentsMargins(shadow, inset, shadow, inset);
        setItemLayoutMetrics(qMax(1, spacing.padding.listItemV / 2), spacing.gap.normal);
        setMinimumWidth(sizeHint().width());
        updateGeometry();
        update();
    }

    QIcon glyphIcon(const QString& glyph) const
    {
        auto pixmap = [this, &glyph](const QColor& color) {
            constexpr int size = Typography::IconSize::Standard;
            const qreal dpr = qMax<qreal>(1.0, devicePixelRatioF());
            QPixmap result(qCeil(size * dpr), qCeil(size * dpr));
            result.setDevicePixelRatio(dpr);
            result.fill(Qt::transparent);
            QPainter painter(&result);
            painter.setPen(color);
            Typography::Icons::paintGlyph(painter, QRectF(0, 0, size, size), glyph, size, Qt::AlignCenter);
            return result;
        };
        const auto& colors = themeColorsRef();
        QIcon icon;
        icon.addPixmap(pixmap(colors.textPrimary), QIcon::Normal);
        icon.addPixmap(pixmap(colors.textPrimary), QIcon::Active);
        icon.addPixmap(pixmap(colors.textDisabled), QIcon::Disabled);
        return icon;
    }
};

// The displayed rich text contains expanded whitespace and synthetic wrapping
// points. Copy a source range, never strip characters from the displayed text:
// a real U+200B inside a string must remain part of the copied source.
class CodeText final : public textfields::Label {
public:
    CodeText(CodeBlock* block, QWidget* parent) : Label(parent), m_block(block) {}

    QTextDocument& layoutForWidth(int width) const
    {
        refreshDocument();
        const qreal textWidth = wordWrap() ? qMax(1, width) : -1;
        if (m_document->textWidth() != textWidth)
            m_document->setTextWidth(textWidth);
        return *m_document;
    }

    int heightForWidth(int width) const override
    {
        refreshDocument();
        const int key = wordWrap() ? qMax(1, width) : -1;
        const auto found = m_heights.constFind(key);
        if (found != m_heights.cend())
            return *found;
        const int height = qCeil(layoutForWidth(width).size().height());
        // Layouts alternate between preferred and actual width. Bound this cache
        // so interactive window resizing cannot retain a height for every pixel.
        if (m_heights.size() >= 4)
            m_heights.clear();
        m_heights.insert(key, height);
        return height;
    }
    QSize sizeHint() const override
    {
        if (wordWrap())
            return QSize(420, heightForWidth(420));
        const auto size = layoutForWidth(-1).size();
        return QSize(qCeil(size.width()), qCeil(size.height()));
    }
    QSize minimumSizeHint() const override { return QSize(0, fontMetrics().lineSpacing()); }

private:
    void refreshDocument() const
    {
        if (!m_document)
            m_document = std::make_unique<QTextDocument>();
        if (m_html != text() || m_font != font() || m_wrap != wordWrap()) {
            m_html = text();
            m_font = font();
            m_wrap = wordWrap();
            m_heights.clear();
            m_document->setUndoRedoEnabled(false);
            m_document->setDefaultFont(m_font);
            m_document->setDocumentMargin(0);
            QTextOption option;
            option.setAlignment(Qt::AlignLeft);
            option.setWrapMode(m_wrap ? QTextOption::WordWrap : QTextOption::ManualWrap);
            m_document->setDefaultTextOption(option);
            m_document->setHtml(m_html);
        }
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        // QLabel keeps native selection, keyboard and accessibility behavior.
        // Reuse the measurement document for clipped drawing: QLabel's rich-text
        // painter otherwise traverses the entire source, even outside the viewport.
        // zh_CN: 保留 Label 的原生选择、键盘和无障碍；复用测量文档按可见区域绘制，
        // 避免 QLabel 在滚动和展开时遍历屏外的整份源码。
        auto& document = layoutForWidth(width());
        QPainter painter(this);
        QAbstractTextDocumentLayout::PaintContext context;
        context.clip = event->rect();
        context.palette = palette();
        // Qt still owns hit testing, keyboard navigation and selection ranges.
        // Paint that selection on the same clipped document in every focus state.
        // zh_CN: Qt 继续负责命中、键盘导航和选区；所有焦点状态用同一文档裁剪绘制选区。
        if (hasSelectedText()) {
            QAbstractTextDocumentLayout::Selection selection;
            selection.cursor = QTextCursor(&document);
            const int first = qBound(0, selectionStart(), document.characterCount() - 1);
            const int last = qMin(document.characterCount() - 1, first + selectedText().size());
            selection.cursor.setPosition(first);
            selection.cursor.setPosition(last, QTextCursor::KeepAnchor);
            const auto group = !isEnabled() ? QPalette::Disabled : hasFocus() ? QPalette::Active : QPalette::Inactive;
            selection.format.setBackground(palette().brush(group, QPalette::Highlight));
            selection.format.setForeground(palette().brush(group, QPalette::HighlightedText));
            context.selections.append(selection);
        }
        document.documentLayout()->draw(&painter, context);
    }

    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::ShortcutOverride) {
            auto* key = static_cast<QKeyEvent*>(event);
            if (key->matches(QKeySequence::Copy) || key->matches(QKeySequence::SelectAll)) {
                // Reserve editing shortcuts for the focused source selection.
                // The actual command still runs only on KeyPress.
                key->accept();
                return true;
            }
        }
        return Label::event(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->matches(QKeySequence::Copy)) {
            copySourceSelection();
            event->accept();
        } else if (event->matches(QKeySequence::SelectAll)) {
            selectAllSource();
            event->accept();
        } else {
            Label::keyPressEvent(event);
        }
    }

    void contextMenuEvent(QContextMenuEvent* event) override
    {
        auto* menu = new CodeContextMenu(this);
        auto* copy = menu->addAction(menu->glyphIcon(Typography::Icons::Copy),
            QCoreApplication::translate("QLineEdit", "&Copy"));
        copy->setShortcuts(QKeySequence::keyBindings(QKeySequence::Copy));
        copy->setEnabled(hasSelectedText());
        connect(copy, &QAction::triggered, this, [this]() { copySourceSelection(); });
        auto* selectAll = menu->addAction(menu->glyphIcon(Typography::Icons::SelectAll),
            QCoreApplication::translate("QLineEdit", "Select All"));
        selectAll->setShortcuts(QKeySequence::keyBindings(QKeySequence::SelectAll));
        selectAll->setEnabled(!text().isEmpty());
        connect(selectAll, &QAction::triggered, this, [this]() { selectAllSource(); });
        connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
        overlay::presentation::popupMenu(menu, this, event->pos());
        event->accept();
    }

private:
    void ensureSourceMap()
    {
        if (!m_block)
            return;
        const QString source = m_block->code();
        const auto language =
            m_block->effectiveTheme() == FluentElement::HighContrast ? QStringLiteral("plain") : m_block->language();
        if (m_source == source && m_language == language && !m_sourceMap.isEmpty())
            return;
        m_source = source;
        m_language = language;
        highlightCodeToHtml(source, language, false, &m_sourceMap);
    }

    void selectAllSource()
    {
        ensureSourceMap();
        setSelection(0, static_cast<int>(m_sourceMap.size()));
    }

    void copySourceSelection()
    {
        if (!hasSelectedText() || !m_block)
            return;
        ensureSourceMap();
        const int start = selectionStart();
        const int end = qMin(start + static_cast<int>(selectedText().size()), static_cast<int>(m_sourceMap.size()));
        if (start < 0 || start >= end)
            return;
        int sourceStart = -1;
        int sourceEnd = -1;
        for (int i = start; i < end; ++i) {
            const auto& span = m_sourceMap.at(i);
            if (span.start == span.end)
                continue;
            if (sourceStart < 0)
                sourceStart = span.start;
            sourceEnd = span.end;
        }
        const QPointer<CodeBlock> block = m_block;
        ::compatibility::detail::copyText(this,
            sourceStart < 0 ? QString() : m_source.mid(sourceStart, sourceEnd - sourceStart), [block](bool success) {
                if (block)
                    emit block->copyFinished(success);
            });
    }

    QPointer<CodeBlock> m_block;
    QString m_source;
    QString m_language;
    CodeSourceMap m_sourceMap;
    mutable std::unique_ptr<QTextDocument> m_document;
    mutable QString m_html;
    mutable QFont m_font;
    mutable bool m_wrap = true;
    mutable QHash<int, int> m_heights;
};

class CodeScroll final : public scrolling::ScrollView {
public:
    explicit CodeScroll(CodeText* text, QWidget* parent) : ScrollView(parent), text(text)
    {
        setObjectName(QStringLiteral("fluentCodeBlockScroll"));
        setFocusPolicy(Qt::NoFocus);
        setLayoutDirection(Qt::LeftToRight);
        setScrollChainingEnabled(true);
        setVerticalScrollBarVisibility(ScrollBarVisibility::Visible);
        viewport()->setObjectName(QStringLiteral("fluentCodeViewport"));
        onThemeUpdated();
        setContentWidget(text, WidgetOwnership::Owned);
        text->setMinimumSize(0, 0);
        text->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        text->setAutoFillBackground(false);
    }
    void onThemeUpdated() override
    {
        ScrollView::onThemeUpdated();
        // This is an internal viewport on the CodeBlock surface, not a separate
        // canvas. Restore transparency after every ScrollView theme update.
        // zh_CN: 内部视口沿用代码块底色，每次主题更新后恢复透明，避免叠加矩形背景。
        QPalette transparent = viewport()->palette();
        transparent.setBrush(QPalette::Window, Qt::NoBrush);
        transparent.setBrush(QPalette::Base, Qt::NoBrush);
        viewport()->setPalette(transparent);
        viewport()->setAutoFillBackground(false);
    }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override
    {
        const int natural = textHeight(qMax(1, width));
        const int limit = visibleHeight(width, natural);
        const int bar =
            !text->wordWrap() && text->sizeHint().width() > width ? horizontalScrollBar()->sizeHint().height() : 0;
        return qMin(natural, limit) + bar;
    }
    QSize sizeHint() const override { return QSize(420, heightForWidth(420)); }
    QSize minimumSizeHint() const override { return QSize(0, qCeil(QFontMetricsF(text->font()).lineSpacing())); }
    void refresh()
    {
        const int width =
            text->wordWrap() ? qMax(1, viewport()->width()) : qMax(viewport()->width(), text->sizeHint().width());
        text->resize(width, textHeight(width));
        updateGeometry();
    }
    CodeText* text;
    int maxLines = 16;

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        ScrollView::resizeEvent(event);
        refresh();
    }

private:
    int visibleHeight(int width, int natural) const
    {
        const qint64 estimated = qint64(maxLines) * qCeil(QFontMetricsF(text->font()).lineSpacing());
        if (maxLines <= 0 || natural <= estimated)
            return natural;

        // Fallback glyphs may be taller than the monospace metrics. Use the
        // same laid-out rows for painting and the visible-line limit.
        auto& measurement = text->layoutForWidth(width);
        measurement.size();
        int rows = 0;
        for (auto block = measurement.begin(); block.isValid(); block = block.next()) {
            const auto* layout = block.layout();
            for (int index = 0; index < layout->lineCount(); ++index) {
                const auto line = layout->lineAt(index);
                if (++rows == maxLines)
                    return qCeil(layout->position().y() + line.y() + line.height());
            }
        }
        return natural;
    }
    int textHeight(int width) const
    {
        return qMax(text->fontMetrics().lineSpacing(),
            text->wordWrap() ? text->heightForWidth(width) : text->sizeHint().height());
    }
};
} // namespace fluent::textfields::detail

namespace fluent::textfields {

CodeBlock::CodeBlock(QWidget* parent) : CodeBlock(QString(), parent) {}

CodeBlock::CodeBlock(const QString& code, QWidget* parent) : Expander(parent), m_code(code)
{
    setObjectName(QStringLiteral("fluentCodeBlock"));
    setHeaderText(tr("Plain text"));
    auto* caption = findChild<Label*>(QStringLiteral("fluentExpanderHeaderText"));
    caption->setFluentTypography(Typography::FontRole::Caption);
    caption->setTextColorRole(Label::TextColorRole::Secondary);
    setAppearance(layout::Card::LayerAlt);
    auto* body = new QWidget;
    body->setObjectName(QStringLiteral("fluentCodeBlockBody"));
    body->setMinimumWidth(0);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(16, 12, 16, 16);
    bodyLayout->setSpacing(10);

    m_text = new detail::CodeText(this, nullptr);
    m_text->setObjectName(QStringLiteral("fluentCodeBlockText"));
    m_text->setTextFormat(Qt::RichText);
    m_text->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    m_text->setFocusPolicy(Qt::StrongFocus);
    m_text->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_text->setWordWrap(true);
    m_text->setMinimumWidth(0);
    m_text->setTextColorRole(Label::TextColorRole::Primary);
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPixelSize(Typography::FontSize::Body);
    m_text->setFont(font);
    m_scroll = new detail::CodeScroll(m_text, body);
    bodyLayout->addWidget(m_scroll);
    setContentWidget(body, WidgetOwnership::Owned);

    m_copyButton = new basicinput::Button(headerButton());
    m_copyButton->setObjectName(QStringLiteral("fluentCodeBlockCopyButton"));
    m_copyButton->setAccessibleName(tr("Copy code"));
    m_copyButton->setFluentStyle(basicinput::Button::Subtle);
    m_copyButton->setFluentSize(basicinput::Button::Small);
    m_copyButton->setFluentLayout(basicinput::Button::IconOnly);
    m_copyButton->setFixedSize(28, 28);
    m_copyButton->setIconGlyph(Typography::Icons::Copy, Typography::IconSize::Standard);
    m_copyButton->setEnabled(!m_code.isEmpty());
    status_info::ToolTip::attach(m_copyButton, tr("Copy code"));
    auto* headerLayout = qobject_cast<QHBoxLayout*>(headerButton()->layout());
    headerLayout->insertWidget(headerLayout->count() - 1, m_copyButton);
    QWidget::setTabOrder(headerButton(), m_copyButton);
    QWidget::setTabOrder(m_copyButton, m_text);
    connect(m_copyButton, &basicinput::Button::clicked, this, &CodeBlock::copy);
    connect(this, &layout::Expander::expansionTransitionStarted, this, [this](bool expanding) {
        if (!expanding && m_text->hasFocus())
            headerButton()->setFocus(Qt::OtherFocusReason);
        m_text->setFocusPolicy(expanding ? Qt::StrongFocus : Qt::NoFocus);
        if (expanding)
            ensureHighlighted();
    });
    setExpandedAnimated(true, false);
}

void CodeBlock::setCode(const QString& code)
{
    if (m_code == code)
        return;
    m_code = code;
    m_copyButton->setEnabled(!m_copying && !code.isEmpty());
    m_scroll->verticalScrollBar()->setValue(0);
    m_scroll->horizontalScrollBar()->setValue(0);
    refreshCode();
    emit codeChanged(m_code);
}

void CodeBlock::setLanguage(const QString& language)
{
    QString value = language.trimmed().toLower();
    if (value == QStringLiteral("c++") || value == QStringLiteral("cxx"))
        value = QStringLiteral("cpp");
    if (value == QStringLiteral("py"))
        value = QStringLiteral("python");
    if (value.isEmpty() || value == QStringLiteral("text") || value == QStringLiteral("plaintext"))
        value = QStringLiteral("plain");
    if (m_language == value)
        return;
    m_language = value;
    setHeaderText(value == QStringLiteral("cpp") ? QStringLiteral("C++")
            : value == QStringLiteral("python")  ? QStringLiteral("Python")
            : value == QStringLiteral("qml")     ? QStringLiteral("QML")
            : value == QStringLiteral("plain")
            ? tr("Plain text")
            : value);
    refreshCode();
    emit languageChanged(m_language);
}

bool CodeBlock::isWordWrapEnabled() const
{
    return m_text->wordWrap();
}
void CodeBlock::setWordWrap(bool enabled)
{
    if (enabled == isWordWrapEnabled())
        return;
    m_text->setWordWrap(enabled);
    m_scroll->refresh();
    contentWidget()->updateGeometry();
    emit wordWrapChanged(enabled);
}
int CodeBlock::maxVisibleLines() const
{
    return m_scroll->maxLines;
}
void CodeBlock::setMaxVisibleLines(int lines)
{
    lines = qMax(0, lines);
    if (lines == maxVisibleLines())
        return;
    m_scroll->maxLines = lines;
    m_scroll->refresh();
    contentWidget()->updateGeometry();
    emit maxVisibleLinesChanged(lines);
}

void CodeBlock::refreshCode()
{
    m_dirty = true;
    if (isExpanded())
        ensureHighlighted();
    else
        m_text->setText(QString());
}
void CodeBlock::ensureHighlighted()
{
    if (!m_dirty)
        return;
    m_text->setText(effectiveTheme() == HighContrast
            ? detail::highlightPlainToHtml(m_code, themeColorsRef().textPrimary.name())
            : detail::highlightCodeToHtml(m_code, m_language, effectiveThemeUsesDarkAppearance()));
    m_dirty = false;
    m_scroll->refresh();
    contentWidget()->updateGeometry();
    QCoreApplication::sendPostedEvents(contentWidget(), QEvent::LayoutRequest);
}
void CodeBlock::onThemeUpdated()
{
    Expander::onThemeUpdated();
    if (m_text) {
        m_text->onThemeUpdated();
        refreshCode();
    }
}

void CodeBlock::copy()
{
    if (m_copying || m_code.isEmpty())
        return;
    m_copying = true;
    m_copyButton->setEnabled(false);
    const QPointer<CodeBlock> guard(this);
    ::compatibility::detail::copyText(this, m_code, [guard](bool success) {
        if (!guard)
            return;
        guard->m_copying = false;
        guard->m_copyButton->setEnabled(!guard->m_code.isEmpty());
        guard->m_copyButton->setIconGlyph(success ? Typography::Icons::CheckMark : Typography::Icons::Copy,
            Typography::IconSize::Standard);
        status_info::ToolTip::attach(guard->m_copyButton, success ? tr("Copied") : tr("Copy failed. Try again."));
        QTimer::singleShot(1300, guard, [guard] {
            if (!guard)
                return;
            guard->m_copyButton->setIconGlyph(Typography::Icons::Copy, Typography::IconSize::Standard);
            status_info::ToolTip::attach(guard->m_copyButton, tr("Copy code"));
        });
        emit guard->copyFinished(success);
    });
}
} // namespace fluent::textfields
