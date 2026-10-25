#include <QStyle>
#include "TagInput.h"
#include "compatibility/private/InputMethodQueryCompat_p.h"
#include "components/basicinput/Button.h"
#include "components/layout/Card.h"
#include "Label.h"
#include "design/Typography.h"

#include <QApplication>
#include <QEvent>
#include <QFontMetricsF>
#include <QInputMethodEvent>
#include <QHBoxLayout>
#include <QHash>
#include <QKeyEvent>
#include <QPointer>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSet>
#include <QTimer>
#include <QtMath>
#include <utility>

namespace fluent::textfields {
namespace {
class TagButton final : public basicinput::Button {
public:
    TagButton(const QString& text, TagInput* editor) : basicinput::Button(text, editor), m_editor(editor)
    {
        setAttribute(Qt::WA_InputMethodEnabled, true);
    }

    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override
    {
        return ::compatibility::detail::forwardedInputMethodQuery(this, m_editor, query);
    }

private:
    QPointer<TagInput> m_editor;
};

QVariantMap normalizedItem(const QVariant& value)
{
    const auto map = value.toMap();
    const QString key = map.value(QStringLiteral("key")).toString().trimmed();
    const QString text = map.value(QStringLiteral("text")).toString().trimmed();
    return key.isEmpty() || text.isEmpty()
        ? QVariantMap{}
        : QVariantMap{{QStringLiteral("key"), key}, {QStringLiteral("text"), text}};
}

const QRegularExpression& separators()
{
    static const QRegularExpression pattern(QStringLiteral("[,;\\n\\r\\t，；、]+"));
    return pattern;
}
} // namespace

TagInput::TagInput(QWidget* parent) : AutoSuggestBox(parent)
{
    setQueryIconVisible(false);
    setClearButtonEnabled(false);
    setInputHeight(40);
    setPlaceholderText(tr("Add items"));
    connect(this, &QLineEdit::textChanged, this, [this] { layoutTags(); });
    connect(this, &QLineEdit::textEdited, this, [this] { scheduleDelimitedInput(); });
    connect(this, &AutoSuggestBox::headerChanged, this, [this] { layoutTags(); });
    connect(this, &AutoSuggestBox::queryIconVisibleChanged, this, [this] { layoutTags(); });
    connect(this, &AutoSuggestBox::queryButtonSizeChanged, this, [this] { layoutTags(); });
    connect(this, &AutoSuggestBox::queryButtonPlacementChanged, this, [this] { layoutTags(); });
    connect(this, &AutoSuggestBox::querySubmitted, this, [this](const QString& query, const QVariant& chosen) {
        if (!chosen.isValid()) {
            submitText(query);
            return;
        }
        if (isReadOnly() || !isEnabled() || isInputMethodComposing())
            return;
        QVariantMap item = normalizedItem(chosen);
        if (item.isEmpty() && chosen.canConvert<QString>()) {
            const QString key = chosen.toString().trimmed();
            if (!key.isEmpty())
                item = {{QStringLiteral("key"), key}, {QStringLiteral("text"), query.trimmed()}};
        }
        if (item.isEmpty())
            return;
        QPointer<TagInput> guard(this);
        addItem(item.value(QStringLiteral("key")).toString(), item.value(QStringLiteral("text")).toString());
        if (guard)
            clear();
    });
    layoutTags();
}

TagInput::~TagInput() = default;

QStringList TagInput::selectedKeys() const
{
    QStringList result;
    for (const auto& item : m_items)
        result.append(item.toMap().value(QStringLiteral("key")).toString());
    return result;
}

QStringList TagInput::selectedTexts() const
{
    QStringList result;
    for (const auto& item : m_items)
        result.append(item.toMap().value(QStringLiteral("text")).toString());
    return result;
}

int TagInput::indexForKey(const QString& key) const
{
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items.at(i).toMap().value(QStringLiteral("key")).toString() == key)
            return i;
    return -1;
}

bool TagInput::containsKey(const QString& key) const
{
    return indexForKey(key.trimmed()) >= 0;
}

void TagInput::setSelectedItems(const QVariantList& items)
{
    QVariantList normalized;
    QSet<QString> keys;
    for (const auto& value : items) {
        const auto item = normalizedItem(value);
        const auto key = item.value(QStringLiteral("key")).toString();
        if (!item.isEmpty() && !keys.contains(key)) {
            keys.insert(key);
            normalized.append(item);
        }
    }
    if (m_items == normalized)
        return;
    m_items = normalized;
    QPointer<TagInput> guard(this);
    rebuildTags();
    if (guard)
        emit selectionChanged();
}

bool TagInput::addItem(const QString& key, const QString& text)
{
    const auto item = normalizedItem(QVariantMap{{QStringLiteral("key"), key}, {QStringLiteral("text"), text}});
    if (item.isEmpty() || containsKey(item.value(QStringLiteral("key")).toString()))
        return false;
    m_items.append(item);
    QPointer<TagInput> guard(this);
    auto* tag = createTag(item);
    m_tags.append(tag);
    tag->show();
    layoutTags();
    if (guard)
        emit selectionChanged();
    return true;
}

bool TagInput::removeItem(const QString& key)
{
    const int index = indexForKey(key.trimmed());
    if (index < 0)
        return false;
    m_items.removeAt(index);
    QPointer<TagInput> guard(this);
    auto* tag = m_tags.takeAt(index);
    const auto* focused = QApplication::focusWidget();
    const bool restoreFocus = focused && (focused == tag || tag->isAncestorOf(focused));
    retireTag(tag);
    if (!guard)
        return true;
    layoutTags();
    if (guard && restoreFocus)
        setFocus(Qt::OtherFocusReason);
    if (guard)
        emit selectionChanged();
    return true;
}

void TagInput::clearSelection()
{
    setSelectedItems({});
}

QWidget* TagInput::createTag(const QVariantMap& item)
{
    const auto key = item.value(QStringLiteral("key")).toString();
    const auto label = item.value(QStringLiteral("text")).toString();
    auto* tag = new layout::Card(this);
    tag->setAppearance(layout::Card::LayerAlt);
    auto* row = new QHBoxLayout(tag);
    row->setContentsMargins(8, 2, isReadOnly() ? 8 : 2, 2);
    row->setSpacing(4);
    auto* valueLabel = new Label(label, tag);
    valueLabel->setTextFormat(Qt::PlainText);
    valueLabel->setTextElideMode(Qt::ElideRight);
    valueLabel->setAccessibleDescription(key);
    valueLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    row->addWidget(valueLabel, 1);
    {
        auto* button = new TagButton(QString(), this);
        button->setParent(tag);
        button->setObjectName(QStringLiteral("TagInputRemoveButton"));
        button->setProperty("tagKey", key);
        button->setFluentStyle(basicinput::Button::Subtle);
        button->setFluentSize(basicinput::Button::Small);
        button->setFluentLayout(basicinput::Button::IconOnly);
        button->setIconGlyph(Typography::Icons::ChromeClose, 12);
        button->setFixedSize(24, 24);
        button->setFocusPolicy(Qt::ClickFocus);
        button->setAutoDefault(false);
        button->setAccessibleName(tr("Remove %1").arg(label));
        button->setAccessibleDescription(key);
        button->installEventFilter(this);
        row->addWidget(button);
        tag->setFocusPolicy(Qt::ClickFocus);
        tag->setFocusProxy(button);
        connect(button, &basicinput::Button::clicked, this, [this, key] {
            if (isReadOnly() || !isEnabled())
                return;
            QPointer<TagInput> guard(this);
            removeItem(key);
            if (guard)
                setFocus(Qt::OtherFocusReason);
        });
    }
    tag->setObjectName(QStringLiteral("TagInputTag"));
    tag->setProperty("tagKey", key);
    tag->setToolTip(label);
    updateTag(tag, item);
    return tag;
}

void TagInput::updateTag(QWidget* tag, const QVariantMap& item)
{
    const auto text = item.value(QStringLiteral("text")).toString();
    auto* label = tag->findChild<Label*>();
    if (label->text() != text)
        label->setText(text);
    tag->setToolTip(text);
    auto* close = tag->findChild<basicinput::Button*>(QStringLiteral("TagInputRemoveButton"));
    close->setAccessibleName(tr("Remove %1").arg(text));
    tag->setFocusProxy(isReadOnly() ? nullptr : close);
    tag->setFocusPolicy(isReadOnly() ? Qt::NoFocus : Qt::ClickFocus);
    close->setVisible(!isReadOnly());
}

void TagInput::retireTag(QWidget* tag)
{
    auto* close = tag->findChild<basicinput::Button*>(QStringLiteral("TagInputRemoveButton"));
    close->removeEventFilter(this);
    close->hide();
    tag->setObjectName(QString());
    tag->setProperty("tagKey", QVariant());
    tag->hide();
    tag->deleteLater();
}

void TagInput::rebuildTags()
{
    QPointer<TagInput> guard(this);
    const QPointer<QWidget> focused = QApplication::focusWidget();
    const bool hadTagFocus = focused && isAncestorOf(focused);
    QHash<QString, QWidget*> existing;
    for (auto* tag : std::as_const(m_tags))
        existing.insert(tag->property("tagKey").toString(), tag);
    QVector<QWidget*> next;
    next.reserve(m_items.size());
    // Stable identities retain their widgets, focus and accessible objects.
    // zh_CN: 按稳定身份复用控件、焦点与无障碍对象，仅创建或移除变动项。
    for (const auto& value : std::as_const(m_items)) {
        const auto item = value.toMap();
        auto* tag = existing.take(item.value(QStringLiteral("key")).toString());
        if (tag)
            updateTag(tag, item);
        else
            tag = createTag(item);
        next.append(tag);
    }
    bool reordered = false;
    for (int i = 0; i < qMin(m_tags.size(), next.size()); ++i)
        reordered |= m_tags.at(i) != next.at(i);
    m_tags = std::move(next);
    for (auto* tag : std::as_const(existing))
        retireTag(tag);
    for (auto* tag : std::as_const(m_tags)) {
        // Qt's accessible child order follows widget stacking order.
        // zh_CN: Qt 无障碍子项顺序跟随控件层叠顺序，重排时同步且不重建身份。
        if (reordered)
            tag->raise();
        tag->show();
    }
    layoutTags();
    if (guard && hadTagFocus && focused && !focused->isVisible())
        setFocus(Qt::OtherFocusReason);
}

void TagInput::layoutTags()
{
    if (m_layouting)
        return;
    QPointer<TagInput> guard(this);
    m_layouting = true;
    const int edge = 8;
    const int gap = 4;
    const int rowHeight = qMax(28, fontMetrics().height() + 8);
    const int query = isQueryIconVisible() ? queryButtonSize() + 6 : 0;
    const int leading = edge + (queryButtonPlacement() == QueryButtonPlacement::Left ? query : 0);
    const int trailing = edge + (queryButtonPlacement() == QueryButtonPlacement::Right ? query : 0);
    const int right = width() - trailing;
    const int available = qMax(1, right - leading);
    const int top = height() - inputHeight();
    int x = leading;
    int y = 6;
    for (auto* tag : std::as_const(m_tags)) {
        const bool removable = tag->focusProxy() != nullptr;
        const bool rtl = layoutDirection() == Qt::RightToLeft;
        tag->layout()->setContentsMargins(removable && rtl ? 2 : 8, 2, removable && !rtl ? 2 : 8, 2);
        auto* label = tag->findChild<Label*>();
        if (label)
            label->setTextColorRole(isEnabled() ? Label::TextColorRole::Primary : Label::TextColorRole::Disabled);
        // The label's rendered text may already be elided at the old width.
        // Measure the caller's full value so widening restores the whole tag.
        // zh_CN: 以完整文本计算标签宽度，避免上次省略后的尺寸阻止标签重新展开。
        const auto margins = tag->layout()->contentsMargins();
        const int textWidth = label ? qCeil(QFontMetricsF(label->font()).horizontalAdvance(label->text())) : 0;
        const int closeWidth = removable ? tag->focusProxy()->width() + tag->layout()->spacing() : 0;
        const int w = qMin(available, textWidth + margins.left() + margins.right() + closeWidth);
        if (x > leading && x + w > right) {
            x = leading;
            y += rowHeight + gap;
        }
        const QRect bounds(leading, top, available, inputHeight());
        tag->setGeometry(QStyle::visualRect(layoutDirection(), bounds, QRect(x, top + y, w, rowHeight)));
        x += w + gap;
    }
    if (!isReadOnly() || !text().isEmpty()) {
        if (x > leading && right - x < 96) {
            x = leading;
            y += rowHeight + gap;
        }
    } else {
        x = width();
    }
    setInputHeight(qMax(40, y + rowHeight + 6));
    if (!guard)
        return;
    setContentMargins(QMargins(leading, 4, trailing, 4));
    if (!guard)
        return;
    const int occupied = qMax(0, x - leading);
    setTextMargins(layoutDirection() == Qt::RightToLeft ? 0 : occupied, top + y - 6,
        layoutDirection() == Qt::RightToLeft ? occupied : 0, 0);
    m_layouting = false;
    updateGeometry();
    update();
}

void TagInput::resizeEvent(QResizeEvent* event)
{
    QPointer<TagInput> guard(this);
    AutoSuggestBox::resizeEvent(event);
    if (guard)
        layoutTags();
}

void TagInput::focusTag(int index)
{
    if (index < 0 || index >= m_tags.size()) {
        setFocus(Qt::OtherFocusReason);
        setCursorPosition(0);
        return;
    }
    QPointer<TagInput> guard(this);
    QPointer<QWidget> tag(m_tags.at(index));
    cancelSuggestions();
    if (guard && tag) {
        tag->setFocus(Qt::OtherFocusReason);
        update();
    }
}

void TagInput::keyPressEvent(QKeyEvent* event)
{
    if (!isInputMethodComposing() && !isReadOnly() && !m_tags.isEmpty() && event->modifiers() == Qt::NoModifier
        && !hasSelectedText()
        && ((event->key() == Qt::Key_Backspace && text().isEmpty())
            || (event->key() == (layoutDirection() == Qt::RightToLeft ? Qt::Key_Right : Qt::Key_Left)
                && cursorPosition() == 0))) {
        focusTag(m_tags.size() - 1);
        event->accept();
        return;
    }
    AutoSuggestBox::keyPressEvent(event);
}

void TagInput::inputMethodEvent(QInputMethodEvent* event)
{
    QPointer<TagInput> guard(this);
    AutoSuggestBox::inputMethodEvent(event);
    if (!guard)
        return;
    if (!isInputMethodComposing())
        scheduleDelimitedInput();
}

bool TagInput::eventFilter(QObject* watched, QEvent* event)
{
    auto* tag = qobject_cast<basicinput::Button*>(watched);
    const int index = tag ? m_tags.indexOf(tag->parentWidget()) : -1;
    if (index >= 0 && event->type() == QEvent::InputMethod && !isReadOnly() && isEnabled()) {
        const auto* input = static_cast<QInputMethodEvent*>(event);
        // An input context can deliver a plain Space as committed text. Keep
        // the focused button's activation semantics, but never consume IME composition.
        // zh_CN: 输入上下文可能把普通空格提交为文本；保留按钮激活语义，不拦截组字提交。
        if (!isInputMethodComposing() && input->preeditString().isEmpty()
            && input->commitString() == QStringLiteral(" ") && input->replacementStart() == 0
            && input->replacementLength() == 0) {
            tag->click();
            return true;
        }
        QPointer<TagInput> guard(this);
        QPointer<basicinput::Button> source(tag);
        inputMethodEvent(static_cast<QInputMethodEvent*>(event));
        if (guard && source && source->hasFocus() && !isInputMethodComposing()) {
            // Let the native input context finish its commit before changing
            // focus; changing it inside delivery can recursively commit again.
            // zh_CN: 原生输入法完成提交后再切换焦点，避免事件分发中递归提交。
            QTimer::singleShot(0, this, [this, source] {
                if (source && source->hasFocus() && !isInputMethodComposing())
                    setFocus(Qt::OtherFocusReason);
            });
        }
        return true;
    }
    if (index >= 0 && event->type() == QEvent::KeyPress && !isReadOnly()) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (isInputMethodComposing()) {
            AutoSuggestBox::keyPressEvent(key);
            return true;
        }
        if (key->modifiers() == Qt::NoModifier) {
            if (key->key() == Qt::Key_Space)
                return false;
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                tag->click();
                return true;
            }
            if (key->key() == (layoutDirection() == Qt::RightToLeft ? Qt::Key_Right : Qt::Key_Left)) {
                focusTag(index - 1);
                return true;
            }
            if (key->key() == (layoutDirection() == Qt::RightToLeft ? Qt::Key_Left : Qt::Key_Right)) {
                focusTag(index + 1);
                return true;
            }
            if (key->key() == Qt::Key_Escape) {
                focusTag(-1);
                return true;
            }
            if (key->key() == Qt::Key_Backspace || key->key() == Qt::Key_Delete) {
                QPointer<TagInput> guard(this);
                removeItem(tag->property("tagKey").toString());
                if (guard)
                    focusTag(qMin(index, m_tags.size() - 1));
                return true;
            }
            if (key->key() == Qt::Key_Tab) {
                focusNextPrevChild(true);
                return true;
            }
        }
        if (key->key() == Qt::Key_Backtab) {
            focusNextPrevChild(false);
            return true;
        }
        if (!key->text().isEmpty() && key->key() != Qt::Key_Space && key->key() != Qt::Key_Return
            && key->key() != Qt::Key_Enter
            && !(key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
            setFocus(Qt::OtherFocusReason);
            AutoSuggestBox::keyPressEvent(key);
            return true;
        }
    }
    if (index >= 0 && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut))
        update();
    return AutoSuggestBox::eventFilter(watched, event);
}

bool TagInput::focusNextPrevChild(bool next)
{
    if (QApplication::focusWidget() && isAncestorOf(QApplication::focusWidget()))
        setFocus(Qt::OtherFocusReason);
    return AutoSuggestBox::focusNextPrevChild(next);
}

void TagInput::submitText(const QString& value)
{
    if (isInputMethodComposing() || isReadOnly() || !isEnabled())
        return;
    QStringList tokens;
    for (const auto& piece : value.split(separators(), Qt::SkipEmptyParts)) {
        const auto token = piece.trimmed();
        if (!token.isEmpty() && !tokens.contains(token))
            tokens.append(token);
    }
    if (tokens.isEmpty())
        return;
    QPointer<TagInput> guard(this);
    cancelSuggestions();
    if (guard)
        emit tokensSubmitted(tokens);
}

void TagInput::scheduleDelimitedInput()
{
    if (m_delimitedPending || isInputMethodComposing() || !text().contains(separators()))
        return;
    m_delimitedPending = true;
    QTimer::singleShot(0, this, [this] {
        m_delimitedPending = false;
        if (text().contains(separators()))
            submitText(text());
    });
}

void TagInput::changeEvent(QEvent* event)
{
    QPointer<TagInput> guard(this);
    AutoSuggestBox::changeEvent(event);
    if (guard && event && event->type() == QEvent::ReadOnlyChange) {
        rebuildTags();
        return;
    }
    if (guard && event
        && (event->type() == QEvent::EnabledChange || event->type() == QEvent::FontChange
            || event->type() == QEvent::LayoutDirectionChange))
        layoutTags();
}

void TagInput::onThemeUpdated()
{
    AutoSuggestBox::onThemeUpdated();
    for (auto* tag : std::as_const(m_tags)) {
        if (auto* element = dynamic_cast<FluentElement*>(tag))
            element->onThemeUpdated();
        if (auto* label = tag->findChild<Label*>())
            label->onThemeUpdated();
    }
    layoutTags();
}
} // namespace fluent::textfields
