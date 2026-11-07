#pragma once

#include <FluentQt/FluentQt.h>
#include <QItemSelectionModel>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace fluent::gallery {

class TransferSample final : public QWidget {
public:
    explicit TransferSample(QWidget* parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("transferSample"));
        setMaximumWidth(740);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(::Spacing::Standard);
        auto* model = new QStandardItemModel(this);
        const QList<QStringList> members{{"lin", "Lin Xiao", "Design"}, {"lin-yue", "Lin Yue", "Engineering"},
            {"xu", "Xu An", "QA"}, {"song", "Song Ke", "Unavailable"}, {"zhou", "Zhou Ning", "Engineering"},
            {"shen", "Shen Yi", "Product"}};
        for (const auto& member : members) {
            auto* item = new QStandardItem(member.at(1));
            item->setData(member.at(0), Qt::UserRole);
            item->setData(member.at(2), Qt::UserRole + 1);
            item->setEnabled(member.at(0) != QStringLiteral("song"));
            model->appendRow(item);
        }
        auto* transfer = new collections::Transfer(this);
        transfer->setObjectName(QStringLiteral("projectMemberTransfer"));
        transfer->setAccessibleName(QStringLiteral("Project members"));
        transfer->setSourceHeader(QStringLiteral("Available members"));
        transfer->setTargetHeader(QStringLiteral("Project members"));
        transfer->setSearchPlaceholderText(QStringLiteral("Search members"));
        transfer->setDescriptionRole(Qt::UserRole + 1);
        transfer->setModel(model);
        transfer->setSelectedKeys({QStringLiteral("zhou"), QStringLiteral("shen")});
        layout->addWidget(transfer);
        auto* status = new textfields::Label(this);
        status->setObjectName(QStringLiteral("transferMemberCount"));
        status->setFluentTypography(Typography::FontRole::Caption);
        status->setTextColorRole(textfields::Label::TextColorRole::Secondary);
        status->setWordWrap(true);
        const auto updateStatus = [transfer, status]() {
            status->setText(
                QStringLiteral("%1 project members · Source data stays unchanged").arg(transfer->selectedCount()));
        };
        connect(transfer, &collections::Transfer::countsChanged, status, updateStatus);
        updateStatus();
        layout->addWidget(status);
    }
};

} // namespace fluent::gallery
