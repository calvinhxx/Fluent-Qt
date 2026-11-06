#ifndef GALLERY_ASYNCSEARCHSAMPLE_H
#define GALLERY_ASYNCSEARCHSAMPLE_H

#include <FluentQt/FluentQt.h>
#include <QHBoxLayout>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>
#include "InputSampleColumns.h"

namespace fluent::gallery {

// A simulated application service. The component owns neither requests nor data.
// zh_CN: 模拟应用服务；控件不拥有网络请求或业务数据。
class AsyncSearchSample final : public QWidget {
public:
    explicit AsyncSearchSample(QWidget* parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("asyncSearchSample"));
        setProperty("staleResponsesIgnored", 0);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(::Spacing::Standard);
        auto* columns = new InputSampleColumns(this);
        layout->addWidget(columns);
        m_failNext = new basicinput::CheckBox(QStringLiteral("Fail next search"), this);
        m_failNext->setObjectName(QStringLiteral("asyncFailNext"));
        m_failNext->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        layout->addWidget(m_failNext);
        createSearch(columns->addColumn(), false);
        createSearch(columns->addColumn(), true);
    }

private:
    textfields::AutoSuggestBox* createSearch(QWidget* column, bool slow)
    {
        auto* layout = column->layout();
        auto* title = new textfields::Label(slow ? QStringLiteral("While searching") : QStringLiteral("Suggestions"),
            column);
        title->setMinimumHeight(Typography::LineHeight::Body);
        auto* search = new textfields::AutoSuggestBox(column);
        search->setObjectName(slow ? QStringLiteral("asyncProjectSearchSlow") : QStringLiteral("asyncProjectSearch"));
        search->setAccessibleName(slow ? QStringLiteral("Search projects slowly") : QStringLiteral("Search projects"));
        search->setPlaceholderText(QStringLiteral("Search projects"));
        search->setQueryButtonPlacement(textfields::AutoSuggestBox::QueryButtonPlacement::Leading);
        search->setSuggestionItemHeight(::Spacing::ControlHeight::Standard);
        search->setMaximumWidth(370);
        search->setMinimumWidth(180);
        search->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        search->setLoadingText(QStringLiteral("Searching projects…"));
        search->setEmptyText(QStringLiteral("No projects found"));
        search->setEmptyDescription(QStringLiteral("Try a different name."));
        title->setBuddy(search);
        auto* model = new QStandardItemModel(this);
        search->setSuggestionModel(model);
        search->setAsyncSuggestionsEnabled(true);
        auto* count = new textfields::Label;
        count->setObjectName(
            slow ? QStringLiteral("asyncSearchSlowTrailingStatus") : QStringLiteral("asyncSearchTrailingStatus"));
        count->setFluentTypography(Typography::FontRole::Caption);
        count->setTextColorRole(textfields::Label::TextColorRole::Secondary);
        count->setAttribute(Qt::WA_TransparentForMouseEvents);
        count->setFocusPolicy(Qt::NoFocus);
        search->setTrailingWidget(count, WidgetOwnership::Owned);
        const auto updateTrailing = [search, count] {
            using State = textfields::AutoSuggestBox::LoadState;
            const State state = search->loadState();
            count->setText(state == State::Ready ? QStringLiteral("%1 shown").arg(search->suggestionCount())
                    : state == State::Failed
                    ? QStringLiteral("Failed")
                    : QString());
            count->setTextColorRole(state == State::Failed
                    ? textfields::Label::TextColorRole::Critical
                    : textfields::Label::TextColorRole::Secondary);
            count->setVisible(!count->text().isEmpty());
        };
        connect(search, &textfields::AutoSuggestBox::loadStateChanged, count, updateTrailing);
        connect(search, &textfields::AutoSuggestBox::suggestionCountChanged, count, updateTrailing);
        updateTrailing();
        auto* status = new textfields::Label(slow
                ? QStringLiteral("Keep typing while we search your projects.")
                : QStringLiteral("Try Aurora for paged results, or zzz for no results."),
            column);
        status->setObjectName(slow ? QStringLiteral("asyncSearchSlowResult") : QStringLiteral("asyncSearchResult"));
        status->setWordWrap(true);
        status->setFluentTypography(Typography::FontRole::Caption);
        status->setTextColorRole(textfields::Label::TextColorRole::Secondary);
        layout->addWidget(title);
        layout->addWidget(search);
        layout->addWidget(status);

        connect(search, &textfields::AutoSuggestBox::suggestionsRequested, this,
            [this, search, model, slow](const QString& query, quint64 token, bool append) {
                const bool fail = m_failNext->isChecked();
                m_failNext->setChecked(false);
                // Short queries deliberately finish later to demonstrate stale-result rejection.
                const int delay = slow ? 1500 : (query.size() < 3 ? 850 : 350);
                QTimer::singleShot(delay, this, [this, search, model, query, token, append, fail] {
                    // Check before touching the caller-owned model, not only before changing UI state.
                    if (!search->isSuggestionRequestCurrent(token)) {
                        setProperty("staleResponsesIgnored", ++m_ignoredResponses);
                        return;
                    }
                    if (fail) {
                        search->failSuggestionRequest(token, QStringLiteral("Couldn’t load projects"),
                            QStringLiteral("Try again in a moment."));
                        return;
                    }
                    QStringList matches;
                    for (const QString& name : projectNames()) {
                        if (name.contains(query.trimmed(), Qt::CaseInsensitive))
                            matches.append(name);
                    }
                    if (!append)
                        model->removeRows(0, model->rowCount());
                    const int end = qMin(model->rowCount() + 4, matches.size());
                    for (int row = model->rowCount(); row < end; ++row) {
                        auto* item = new QStandardItem(matches.at(row));
                        item->setData(QStringLiteral("project-%1").arg(projectNames().indexOf(matches.at(row)) + 1),
                            Qt::UserRole);
                        model->appendRow(item);
                    }
                    search->completeSuggestionRequest(token, end < matches.size());
                });
            });
        connect(search, &textfields::AutoSuggestBox::querySubmitted, this,
            [status](const QString& query, const QVariant& chosen) {
                status->setText(!chosen.toString().isEmpty()
                        ? QStringLiteral("Selected: %1").arg(query)
                        : QStringLiteral("Search: %1").arg(query));
            });
        return search;
    }

    static const QStringList& projectNames()
    {
        static const QStringList names{QStringLiteral("Aurora Workspace"), QStringLiteral("Aurora Mobile"),
            QStringLiteral("Aurora API"), QStringLiteral("Aurora Docs"), QStringLiteral("Aurora Design"),
            QStringLiteral("Aurora Analytics"), QStringLiteral("Aurora Release"), QStringLiteral("Aurora Support"),
            QStringLiteral("Canvas"), QStringLiteral("Canvas Studio"), QStringLiteral("Delta"),
            QStringLiteral("Harbor")};
        return names;
    }
    basicinput::CheckBox* m_failNext = nullptr;
    int m_ignoredResponses = 0;
};

} // namespace fluent::gallery
#endif
