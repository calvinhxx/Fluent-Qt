#pragma once

#include <FluentQt/FluentQt.h>
#include <QApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QPointer>
#include <QRegularExpression>
#include <QShowEvent>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>

namespace fluent::gallery {

// Same-window suggestions preserve the editor's native caret and IME focus.
// zh_CN: 同窗口建议列表保留编辑器原生光标与输入法焦点。
class CaretCompletionPopup final : public dialogs_flyouts::Popup {
public:
    explicit CaretCompletionPopup(QWidget* parent) : Popup(parent)
    {
        setModal(false);
        setDim(false);
        setFocusOnOpenEnabled(false);
        setAnimationEnabled(false);
        setClosePolicy(CloseOnPressOutside | CloseOnEscape);
    }
};

class CaretCompletionSample final : public QWidget {
public:
    explicit CaretCompletionSample(QWidget* parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("caretCompletionSample"));
        setMaximumWidth(740);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(::Spacing::Standard);
        m_edit = new textfields::TextEdit(this);
        m_edit->setObjectName(QStringLiteral("caretCompletionEditor"));
        m_edit->setAccessibleName(QStringLiteral("Review comment"));
        m_edit->setAccessibleDescription(QStringLiteral(
            "Type @ to mention a member. Use Up and Down to choose, Enter to insert, and Escape to dismiss."));
        m_edit->setPlaceholderText(QStringLiteral("Type @ to mention a member"));
        m_edit->setMinVisibleLines(3);
        m_edit->setMaxVisibleLines(5);
        m_edit->setPlainText(QStringLiteral("Ready for the next design review.\nPlease review with @lin"));
        auto cursor = m_edit->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_edit->setTextCursor(cursor);
        layout->addWidget(m_edit);
        layout->addSpacing(160);
        auto* hint = new textfields::Label(
            QStringLiteral("Type @lin, @zhou or a name with no matches. The popup follows the caret."), this);
        hint->setFluentTypography(Typography::FontRole::Caption);
        hint->setTextColorRole(textfields::Label::TextColorRole::Secondary);
        hint->setWordWrap(true);
        layout->addWidget(hint);

        m_popup = new CaretCompletionPopup(this);
        m_popup->setObjectName(QStringLiteral("caretCompletionPopup"));
        m_popup->setThemeSource(m_edit);
        auto* popupLayout = new QVBoxLayout(m_popup);
        popupLayout->setContentsMargins(4, 8, 4, 8);
        popupLayout->setSpacing(4);
        auto* heading = new textfields::Label(QStringLiteral("Mention a member"), m_popup);
        heading->setContentsMargins(8, 0, 8, 0);
        heading->setFluentTypography(Typography::FontRole::Caption);
        heading->setTextColorRole(textfields::Label::TextColorRole::Secondary);
        popupLayout->addWidget(heading);
        m_results = new QStandardItemModel(this);
        m_list = new collections::ListView(m_popup);
        m_list->setObjectName(QStringLiteral("caretCompletionResults"));
        m_list->setAccessibleName(QStringLiteral("Mention suggestions"));
        m_list->setModel(m_results);
        m_list->setTrailingTextRole(Qt::UserRole);
        m_list->setSelectionIndicatorVisible(false);
        m_list->setBorderVisible(false);
        m_list->setBackgroundVisible(false);
        m_list->setProperty("fluentPreserveParentSurface", true);
        m_list->setFocusPolicy(Qt::NoFocus);
        m_list->setPlaceholderText(QStringLiteral("No matching members"));
        popupLayout->addWidget(m_list);
        m_edit->focusProxy()->installEventFilter(this);
        connect(m_edit, &textfields::TextEdit::textChanged, this, [this]() {
            if (!m_committing) {
                m_dismissed = false;
                scheduleUpdate();
            }
        });
        connect(m_edit, &textfields::TextEdit::cursorPositionChanged, this, [this]() { scheduleUpdate(); });
        connect(m_edit, &textfields::TextEdit::cursorRectChanged, this, [this]() { scheduleUpdate(); });
        connect(m_list, &QListView::clicked, this, [this](const QModelIndex& index) { insertMember(index); });
        connect(m_popup, &dialogs_flyouts::Popup::isOpenChanged, this, [this](bool open) {
            setProperty("completionOpen", open);
            if (!open)
                m_dismissed = true;
        });
    }

    ~CaretCompletionSample() override { delete m_popup; }

protected:
    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        if (m_host != window()) {
            if (m_host)
                m_host->removeEventFilter(this);
            m_host = window();
            m_host->installEventFilter(this);
        }
    }

    void hideEvent(QHideEvent* event) override
    {
        if (m_popup)
            m_popup->close();
        QWidget::hideEvent(event);
    }

    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (object == m_host && event->type() == QEvent::Resize)
            scheduleUpdate();
        if (object != m_edit->focusProxy())
            return QWidget::eventFilter(object, event);
        if (event->type() == QEvent::InputMethod) {
            m_composing = !static_cast<QInputMethodEvent*>(event)->preeditString().isEmpty();
            scheduleUpdate();
        } else if (event->type() == QEvent::FocusIn) {
            m_dismissed = false;
            scheduleUpdate();
        } else if (event->type() == QEvent::FocusOut || event->type() == QEvent::EnabledChange) {
            scheduleUpdate();
        } else if ((event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) && m_popup
            && m_popup->isOpen() && !m_composing) {
            auto* key = static_cast<QKeyEvent*>(event);
            const int code = key->key();
            // Keypad marks the key's location, not an editing shortcut.
            // zh_CN: Keypad 仅标记按键区域，不代表编辑快捷键组合。
            if ((key->modifiers() & ~Qt::KeypadModifier) != Qt::NoModifier)
                return false;
            if (code != Qt::Key_Up && code != Qt::Key_Down && code != Qt::Key_Return && code != Qt::Key_Enter
                && code != Qt::Key_Escape)
                return false;
            // With no candidate, Enter remains an ordinary newline.
            // zh_CN: 没有候选时，Enter 仍执行普通换行。
            if (m_results->rowCount() == 0 && code != Qt::Key_Escape)
                return false;
            if (event->type() == QEvent::ShortcutOverride) {
                event->accept();
                return true;
            }
            if (code == Qt::Key_Escape)
                m_popup->close();
            else if (code == Qt::Key_Return || code == Qt::Key_Enter)
                insertMember(m_list->currentIndex());
            else {
                const int count = m_results->rowCount();
                const int current = m_list->currentIndex().row();
                const int next = (qMax(0, current) + (code == Qt::Key_Down ? 1 : -1) + count) % count;
                m_list->setCurrentIndex(m_results->index(next, 0));
            }
            event->accept();
            return true;
        }
        return QWidget::eventFilter(object, event);
    }

private:
    void scheduleUpdate()
    {
        if (m_updatePending)
            return;
        m_updatePending = true;
        QTimer::singleShot(0, this, [this]() {
            m_updatePending = false;
            updateSuggestions();
        });
    }

    void updateSuggestions()
    {
        if (!m_popup)
            return;
        if (!isVisible() || !m_edit->hasFocus() || !m_edit->isEnabled() || m_edit->isReadOnly() || m_composing
            || m_dismissed || m_edit->textCursor().hasSelection()) {
            m_popup->close();
            return;
        }
        const auto cursor = m_edit->textCursor();
        const QString before = m_edit->toPlainText().left(cursor.position());
        const QRegularExpression pattern(QStringLiteral("(?:^|[\\s(，。])@([^\\s@，。]{0,32})$"));
        const auto match = pattern.match(before);
        const QRect caret = m_edit->cursorRect();
        if (!match.hasMatch() || !caret.intersects(m_edit->rect())) {
            m_popup->close();
            return;
        }
        m_start = before.lastIndexOf(QLatin1Char('@'));
        const QString query = match.captured(1);
        if (query != m_query || !m_hasQuery) {
            m_query = query;
            m_hasQuery = true;
            m_results->clear();
            const QList<QPair<QString, QString>> members{{QStringLiteral("Lin Xiao"), QStringLiteral("Design")},
                {QStringLiteral("Lin Yue"), QStringLiteral("Engineering")},
                {QStringLiteral("Zhou Ning"), QStringLiteral("Engineering")},
                {QStringLiteral("Xu An"), QStringLiteral("QA")}};
            for (const auto& member : members) {
                if (!member.first.contains(query, Qt::CaseInsensitive))
                    continue;
                auto* item = new QStandardItem(member.first);
                item->setData(member.second, Qt::UserRole);
                m_results->appendRow(item);
            }
            if (m_results->rowCount())
                m_list->setCurrentIndex(m_results->index(0, 0));
        }
        m_list->setFixedHeight(qMax(40, m_results->rowCount() * 40));
        const auto margins = m_popup->contentsMargins();
        const int cardWidth = qMin(300, qMax(160, window()->width() - 16));
        m_popup->setFixedWidth(cardWidth + margins.left() + margins.right());
        m_popup->adjustSize();
        const int cardHeight = m_popup->height() - margins.top() - margins.bottom();
        const QPoint bottom = m_edit->mapTo(window(), caret.bottomLeft());
        const QPoint top = m_edit->mapTo(window(), caret.topLeft());
        int x = layoutDirection() == Qt::RightToLeft ? bottom.x() - cardWidth : bottom.x();
        x = qBound(8, x, qMax(8, window()->width() - cardWidth - 8));
        int y = bottom.y() + 8;
        if (y + cardHeight > window()->height() - 8)
            y = top.y() - cardHeight - 8;
        y = qBound(8, y, qMax(8, window()->height() - cardHeight - 8));
        m_popup->setPosition(m_edit, m_edit->mapFrom(window(), QPoint(x, y)));
        if (!m_popup->isOpen())
            m_popup->open();
    }

    void insertMember(const QModelIndex& index)
    {
        if (!index.isValid() || m_start < 0 || m_composing || m_edit->isReadOnly())
            return;
        const QString name = index.data().toString();
        auto cursor = m_edit->textCursor();
        const int end = cursor.position();
        cursor.setPosition(m_start);
        cursor.setPosition(end, QTextCursor::KeepAnchor);
        m_committing = true;
        cursor.beginEditBlock();
        cursor.insertText(QLatin1Char('@') + name + QLatin1Char(' '));
        cursor.endEditBlock();
        m_edit->setTextCursor(cursor);
        m_committing = false;
        m_popup->close();
        m_edit->setFocus();
    }

    textfields::TextEdit* m_edit = nullptr;
    QPointer<CaretCompletionPopup> m_popup;
    QPointer<QWidget> m_host;
    collections::ListView* m_list = nullptr;
    QStandardItemModel* m_results = nullptr;
    QString m_query;
    int m_start = -1;
    bool m_hasQuery = false;
    bool m_updatePending = false;
    bool m_composing = false;
    bool m_dismissed = false;
    bool m_committing = false;
};

} // namespace fluent::gallery
