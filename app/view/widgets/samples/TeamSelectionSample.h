#ifndef GALLERY_TEAMSELECTIONSAMPLE_H
#define GALLERY_TEAMSELECTIONSAMPLE_H

#include <FluentQt/FluentQt.h>
#include <QStandardItemModel>
#include <QPointer>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace fluent::gallery {

// The application owns the directory. Keys distinguish teams with equal names.
// zh_CN: 应用持有团队目录，稳定身份用于区分同名团队。
class TeamSelectionSample final : public QWidget {
public:
    explicit TeamSelectionSample(QWidget* parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("teamSelectionSample"));
        setMaximumWidth(392);
        auto* body = new QVBoxLayout(this);
        body->setContentsMargins(16, 16, 16, 16);
        body->setSpacing(12);
        auto* model = new QStandardItemModel(this);
        auto team = [](const QString& key, const QString& name) {
            auto* item = new QStandardItem(name);
            item->setData(key, Qt::UserRole);
            return item;
        };
        auto* product = team(QStringLiteral("product"), QStringLiteral("Product engineering"));
        product->setSelectable(false);
        product->appendRow(team(QStringLiteral("client"), QStringLiteral("Client apps")));
        product->appendRow(team(QStringLiteral("platform"), QStringLiteral("Platform services")));
        product->appendRow(team(QStringLiteral("product-design"), QStringLiteral("Design")));
        auto* brand = team(QStringLiteral("brand"), QStringLiteral("Brand and marketing"));
        brand->setSelectable(false);
        brand->appendRow(team(QStringLiteral("content"), QStringLiteral("Content")));
        brand->appendRow(team(QStringLiteral("brand-design"), QStringLiteral("Design")));
        auto* archived = team(QStringLiteral("archived"), QStringLiteral("Archived team"));
        archived->setEnabled(false);
        model->appendRow(product);
        model->appendRow(brand);
        model->appendRow(archived);

        auto* owner = new basicinput::TreeSelect(this);
        owner->setObjectName(QStringLiteral("projectOwningTeam"));
        owner->setAccessibleName(QStringLiteral("Owning team"));
        owner->setModel(model);
        owner->setPlaceholderText(QStringLiteral("Search teams…"));
        owner->setSelectedKeys({QStringLiteral("client")});
        auto* ownerField = new layout::Field(this);
        ownerField->setLabelText(QStringLiteral("Owning team"));
        ownerField->setEditor(owner);
        body->addWidget(ownerField);

        auto* collaborators = new basicinput::TreeSelect(this);
        collaborators->setObjectName(QStringLiteral("projectCollaboratingTeams"));
        collaborators->setAccessibleName(QStringLiteral("Collaborating teams"));
        collaborators->setModel(model);
        collaborators->setSelectionMode(basicinput::TreeSelect::SelectionMode::Multiple);
        collaborators->setPlaceholderText(QStringLiteral("Add teams…"));
        collaborators->setSelectedKeys({QStringLiteral("brand-design")});
        auto* collaboratorsField = new layout::Field(this);
        collaboratorsField->setLabelText(QStringLiteral("Collaborating teams"));
        collaboratorsField->setEditor(collaborators);
        collaboratorsField->setHelperText(QStringLiteral("Search by team or department. Select teams independently."));
        body->addWidget(collaboratorsField);

        auto* status = new textfields::Label(this);
        status->setObjectName(QStringLiteral("teamSelectionStatus"));
        status->setWordWrap(true);
        status->setFluentTypography(Typography::FontRole::Caption);
        status->setTextColorRole(textfields::Label::TextColorRole::Secondary);
        body->addWidget(status);
        auto updateStatus =
            [owner = QPointer<basicinput::TreeSelect>(owner),
                collaborators = QPointer<basicinput::TreeSelect>(collaborators),
                status = QPointer<textfields::Label>(status)] {
                if (!owner || !collaborators || !status)
                    return;
                QStringList paths;
                for (const auto& item : collaborators->selectedItems())
                    paths.append(item.toMap().value(QStringLiteral("path")).toString());
                const QString primary = owner->selectedItems().isEmpty()
                    ? QStringLiteral("Not assigned")
                    : owner->selectedItems().first().toMap().value(QStringLiteral("path")).toString();
                status->setText(QStringLiteral("Owner: %1\nCollaborators: %2")
                        .arg(primary, paths.isEmpty() ? QStringLiteral("None") : paths.join(QStringLiteral("; "))));
            };
        QObject::connect(owner, &basicinput::TreeSelect::selectionChanged, status, updateStatus);
        QObject::connect(collaborators, &basicinput::TreeSelect::selectionChanged, status, updateStatus);
        auto* actions = new QHBoxLayout;
        auto* readOnly = new basicinput::CheckBox(QStringLiteral("Read only"), this);
        readOnly->setObjectName(QStringLiteral("teamReadOnly"));
        QObject::connect(readOnly, &QAbstractButton::toggled, owner, &basicinput::TreeSelect::setReadOnly);
        QObject::connect(readOnly, &QAbstractButton::toggled, collaborators, &basicinput::TreeSelect::setReadOnly);
        actions->addWidget(readOnly);
        actions->addStretch();
        auto* clear = new basicinput::Button(QStringLiteral("Clear"), this);
        clear->setObjectName(QStringLiteral("teamClear"));
        QObject::connect(clear, &QAbstractButton::clicked, owner, &basicinput::TreeSelect::clearSelection);
        QObject::connect(clear, &QAbstractButton::clicked, collaborators, &basicinput::TreeSelect::clearSelection);
        QObject::connect(readOnly, &QAbstractButton::toggled, clear, [clear](bool checked) {
            clear->setEnabled(!checked);
        });
        actions->addWidget(clear);
        body->addLayout(actions);
        updateStatus();
    }
};
} // namespace fluent::gallery
#endif
