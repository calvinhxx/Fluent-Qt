#ifndef GALLERY_MEMBERSELECTIONSAMPLE_H
#define GALLERY_MEMBERSELECTIONSAMPLE_H

#include <FluentQt/FluentQt.h>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>

namespace fluent::gallery {

class MemberSelectionSample final : public QWidget {
public:
    explicit MemberSelectionSample(QWidget* parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("tagMembersSample"));
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(12);
        auto* title = new textfields::Label(QStringLiteral("Project members"), this);
        title->setFluentTypography(Typography::FontRole::BodyStrong);
        layout->addWidget(title);
        m_input = new textfields::TagInput(this);
        m_input->setObjectName(QStringLiteral("projectMemberInput"));
        m_input->setAccessibleName(QStringLiteral("Project members"));
        m_input->setPlaceholderText(QStringLiteral("Search members"));
        m_input->setMinimumWidth(180);
        m_input->setMaximumWidth(360);
        m_input->setSuggestionItemHeight(36);
        m_input->setAsyncSuggestionsEnabled(true);
        m_input->setLoadingText(QStringLiteral("Searching members…"));
        m_input->setEmptyText(QStringLiteral("No members found"));
        m_input->setEmptyDescription(QStringLiteral("Try a name or email address."));
        m_model = new QStandardItemModel(this);
        m_input->setSuggestionModel(m_model);
        layout->addWidget(m_input);
        auto* help = new textfields::Label(
            QStringLiteral("Search Chen Chen or an email. Paste several emails separated by commas or semicolons."),
            this);
        help->setWordWrap(true);
        help->setFluentTypography(Typography::FontRole::Caption);
        help->setTextColorRole(textfields::Label::TextColorRole::Secondary);
        layout->addWidget(help);
        m_status = new textfields::Label(this);
        m_status->setObjectName(QStringLiteral("tagSelectionStatus"));
        m_status->setWordWrap(true);
        m_status->setFluentTypography(Typography::FontRole::Caption);
        layout->addWidget(m_status);
        m_fail = new basicinput::CheckBox(QStringLiteral("Fail next search"), this);
        m_fail->setObjectName(QStringLiteral("tagFailNext"));
        m_fail->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        layout->addWidget(m_fail);
        auto* readonly = new basicinput::CheckBox(QStringLiteral("Read only"), this);
        readonly->setObjectName(QStringLiteral("tagReadOnly"));
        readonly->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        layout->addWidget(readonly);
        auto* reset = new basicinput::Button(QStringLiteral("Reset members"), this);
        reset->setObjectName(QStringLiteral("tagResetMembers"));
        layout->addWidget(reset, 0, Qt::AlignLeft);
        connect(readonly, &basicinput::CheckBox::toggled, m_input, &QLineEdit::setReadOnly);
        connect(reset, &basicinput::Button::clicked, this, [this] { resetMembers(); });
        connect(m_input, &textfields::TagInput::selectionChanged, this, [this] { updateSelection(); });
        connect(m_input, &textfields::TagInput::tokensSubmitted, this, [this](const QStringList& tokens) {
            resolveTokens(tokens);
        });
        connect(m_input, &textfields::AutoSuggestBox::suggestionsRequested, this,
            [this](const QString& query, quint64 requestId, bool) {
                const bool fail = m_fail->isChecked();
                m_fail->setChecked(false);
                QTimer::singleShot(query.size() < 2 ? 650 : 250, this, [this, query, requestId, fail] {
                    if (!m_input->isSuggestionRequestCurrent(requestId))
                        return;
                    if (fail) {
                        m_input->failSuggestionRequest(requestId, QStringLiteral("Couldn’t load members"),
                            QStringLiteral("Your selection is kept."));
                        return;
                    }
                    m_model->clear();
                    for (const auto& member : directory()) {
                        if (!(member.name + member.team + member.email).contains(query, Qt::CaseInsensitive))
                            continue;
                        auto* row = new QStandardItem(member.name + QStringLiteral(" · ") + member.team);
                        row->setData(selectionItem(member), Qt::UserRole);
                        row->setData(member.name + QStringLiteral(" · ") + member.team, Qt::UserRole + 1);
                        m_model->appendRow(row);
                    }
                    updateSelection();
                    m_input->completeSuggestionRequest(requestId);
                });
            });
        resetMembers();
    }

private:
    struct Member {
        QString key, name, team, email;
    };
    static QVector<Member> directory()
    {
        return {{QStringLiteral("chen-design"), QStringLiteral("Chen Chen"), QStringLiteral("Design team"),
                    QStringLiteral("chen.design@example.com")},
            {QStringLiteral("chen-dev"), QStringLiteral("Chen Chen"), QStringLiteral("Engineering team"),
                QStringLiteral("chen.dev@example.com")},
            {QStringLiteral("lin"), QStringLiteral("Lin Xiao"), QStringLiteral("Product team"),
                QStringLiteral("lin@example.com")},
            {QStringLiteral("alex"), QStringLiteral("Alex Chen"), QStringLiteral("Platform team"),
                QStringLiteral("alex@example.com")},
            {QStringLiteral("wang"), QStringLiteral("Ming Wang"), QStringLiteral("Engineering team"),
                QStringLiteral("wang@example.com")},
            {QStringLiteral("li"), QStringLiteral("An Li"), QStringLiteral("Design team"),
                QStringLiteral("li@example.com")},
            {QStringLiteral("maria"), QStringLiteral("María García"), QStringLiteral("Product team"),
                QStringLiteral("maria@example.com")}};
    }
    static QString selectionLabel(const Member& member)
    {
        int sameName = 0;
        for (const auto& other : directory())
            if (other.name == member.name)
                ++sameName;
        return sameName > 1 ? member.name + QStringLiteral(" · ") + member.team : member.name;
    }
    static QVariantMap selectionItem(const Member& member)
    {
        return {{QStringLiteral("key"), member.key}, {QStringLiteral("text"), selectionLabel(member)}};
    }
    void resetMembers()
    {
        m_input->cancelSuggestions();
        m_input->clear();
        QVariantList initial;
        for (const auto& member : directory())
            if (member.key == QStringLiteral("chen-design") || member.key == QStringLiteral("lin"))
                initial.append(selectionItem(member));
        m_input->setSelectedItems(initial);
        updateSelection();
    }
    void updateSelection()
    {
        m_status->setText(QStringLiteral("%1 selected: %2")
                .arg(m_input->selectedCount())
                .arg(m_input->selectedTexts().join(QStringLiteral(", "))));
        for (int i = 0; i < m_model->rowCount(); ++i) {
            auto* row = m_model->item(i);
            const bool selected = m_input->containsKey(
                row->data(Qt::UserRole).toMap().value(QStringLiteral("key")).toString());
            row->setEnabled(!selected);
            row->setSelectable(!selected);
            row->setText(row->data(Qt::UserRole + 1).toString() + (selected ? QStringLiteral(" · Added") : QString()));
        }
    }
    void resolveTokens(const QStringList& tokens)
    {
        QStringList unresolved;
        auto selected = m_input->selectedItems();
        for (const auto& token : tokens) {
            QVector<Member> matches;
            for (const auto& member : directory())
                if (member.key == token || member.email.compare(token, Qt::CaseInsensitive) == 0
                    || member.name == token)
                    matches.append(member);
            if (matches.size() == 1)
                selected.append(QVariantMap{{QStringLiteral("key"), matches[0].key},
                    {QStringLiteral("text"), selectionLabel(matches[0])}});
            else
                unresolved.append(token);
        }
        m_input->setSelectedItems(selected);
        m_input->setText(unresolved.join(QStringLiteral("; ")));
        updateSelection();
        if (!unresolved.isEmpty())
            m_status->setText(QStringLiteral("No unique match: %1. Choose a result or use an email.")
                    .arg(unresolved.join(QStringLiteral(", "))));
    }
    textfields::TagInput* m_input = nullptr;
    QStandardItemModel* m_model = nullptr;
    textfields::Label* m_status = nullptr;
    basicinput::CheckBox* m_fail = nullptr;
};
} // namespace fluent::gallery
#endif
