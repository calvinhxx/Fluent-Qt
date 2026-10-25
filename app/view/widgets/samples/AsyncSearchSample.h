#ifndef GALLERY_ASYNCSEARCHSAMPLE_H
#define GALLERY_ASYNCSEARCHSAMPLE_H

#include <FluentQt/FluentQt.h>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>

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
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(12);
        auto* title = new textfields::Label(QStringLiteral("Project directory"), this);
        title->setFluentTypography(Typography::FontRole::BodyStrong);
        m_search = new textfields::AutoSuggestBox(this);
        m_search->setObjectName(QStringLiteral("asyncProjectSearch"));
        m_search->setAccessibleName(QStringLiteral("Search projects"));
        m_search->setPlaceholderText(QStringLiteral("Search projects"));
        m_search->setQueryButtonPlacement(textfields::AutoSuggestBox::QueryButtonPlacement::Left);
        m_search->setSuggestionItemHeight(36);
        m_search->setMaximumWidth(320);
        m_search->setMinimumWidth(180);
        m_search->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_search->setLoadingText(QStringLiteral("Searching projects…"));
        m_search->setEmptyText(QStringLiteral("No projects found"));
        m_search->setEmptyDescription(QStringLiteral("Try a different name."));
        m_model = new QStandardItemModel(this);
        m_search->setSuggestionModel(m_model);
        m_search->setAsyncSuggestionsEnabled(true);
        m_failNext = new basicinput::CheckBox(QStringLiteral("Fail next search"), this);
        m_failNext->setObjectName(QStringLiteral("asyncFailNext"));
        m_failNext->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        m_status = new textfields::Label(QStringLiteral("Try Aurora for paged results, or zzz for no results."), this);
        m_status->setObjectName(QStringLiteral("asyncSearchResult"));
        m_status->setWordWrap(true);
        m_status->setFluentTypography(Typography::FontRole::Caption);
        m_status->setTextColorRole(textfields::Label::TextColorRole::Secondary);
        layout->addWidget(title);
        layout->addWidget(m_status);
        layout->addWidget(m_search);
        layout->addWidget(m_failNext);

        connect(m_search, &textfields::AutoSuggestBox::suggestionsRequested, this,
            [this](const QString& query, quint64 token, bool append) {
                const bool fail = m_failNext->isChecked();
                m_failNext->setChecked(false);
                // Short queries deliberately finish later to demonstrate stale-result rejection.
                const int delay = query.size() < 3 ? 850 : 350;
                QTimer::singleShot(delay, this, [this, query, token, append, fail] {
                    // Check before touching the caller-owned model, not only before changing UI state.
                    if (!m_search->isSuggestionRequestCurrent(token)) {
                        setProperty("staleResponsesIgnored", ++m_ignoredResponses);
                        return;
                    }
                    if (fail) {
                        m_search->failSuggestionRequest(token, QStringLiteral("Couldn’t load projects"),
                            QStringLiteral("Try again in a moment."));
                        return;
                    }
                    QStringList matches;
                    for (const QString& name : projectNames()) {
                        if (name.contains(query.trimmed(), Qt::CaseInsensitive))
                            matches.append(name);
                    }
                    if (!append)
                        m_model->removeRows(0, m_model->rowCount());
                    const int end = qMin(m_model->rowCount() + 4, matches.size());
                    for (int row = m_model->rowCount(); row < end; ++row) {
                        auto* item = new QStandardItem(matches.at(row));
                        item->setData(QStringLiteral("project-%1").arg(projectNames().indexOf(matches.at(row)) + 1),
                            Qt::UserRole);
                        m_model->appendRow(item);
                    }
                    m_search->completeSuggestionRequest(token, end < matches.size());
                });
            });
        connect(m_search, &textfields::AutoSuggestBox::querySubmitted, this,
            [this](const QString& query, const QVariant& chosen) {
                m_status->setText(!chosen.toString().isEmpty()
                        ? QStringLiteral("Selected: %1").arg(query)
                        : QStringLiteral("Search: %1").arg(query));
            });
    }

private:
    static const QStringList& projectNames()
    {
        static const QStringList names{QStringLiteral("Aurora Workspace"), QStringLiteral("Aurora Mobile"),
            QStringLiteral("Aurora API"), QStringLiteral("Aurora Docs"), QStringLiteral("Aurora Design"),
            QStringLiteral("Aurora Analytics"), QStringLiteral("Aurora Release"), QStringLiteral("Aurora Support"),
            QStringLiteral("Canvas"), QStringLiteral("Canvas Studio"), QStringLiteral("Delta"),
            QStringLiteral("Harbor")};
        return names;
    }
    textfields::AutoSuggestBox* m_search = nullptr;
    QStandardItemModel* m_model = nullptr;
    basicinput::CheckBox* m_failNext = nullptr;
    textfields::Label* m_status = nullptr;
    int m_ignoredResponses = 0;
};

} // namespace fluent::gallery
#endif
