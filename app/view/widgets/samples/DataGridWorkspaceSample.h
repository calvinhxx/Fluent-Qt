#ifndef GALLERY_DATAGRIDWORKSPACESAMPLE_H
#define GALLERY_DATAGRIDWORKSPACESAMPLE_H

#include <FluentQt/FluentQt.h>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace fluent::gallery {

// Filtering rules and saved views are application policy, outside DataGrid.
// zh_CN: 筛选规则与保存视图属于应用策略，不放入 DataGrid。
class ProjectFilterModel final : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void setCriteria(const QString& search, const QVariantMap& filters)
    {
        m_search = search.trimmed();
        m_filters = filters;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override
    {
        bool matchesText = m_search.isEmpty();
        for (int c = 0; c < sourceModel()->columnCount(parent); ++c)
            matchesText |=
                sourceModel()->index(row, c, parent).data().toString().contains(m_search, Qt::CaseInsensitive);
        if (!matchesText)
            return false;
        for (const auto& pair : {qMakePair(QStringLiteral("owner"), 1), qMakePair(QStringLiteral("status"), 2)}) {
            const auto values = m_filters.value(pair.first).toStringList();
            if (!values.isEmpty() && !values.contains(sourceModel()->index(row, pair.second, parent).data().toString()))
                return false;
        }
        return true;
    }

private:
    QString m_search;
    QVariantMap m_filters;
};

class DataGridWorkspaceSample final : public layout::Card {
public:
    explicit DataGridWorkspaceSample(QWidget* parent = nullptr) : Card(parent)
    {
        setObjectName(QStringLiteral("dataGridWorkspace"));
        setBorderVisible(false);
        setMaximumWidth(864);
        setMinimumWidth(300);
        setMinimumHeight(480);
        auto* body = new QVBoxLayout(this);
        body->setContentsMargins(24, 24, 24, 24);
        body->setSpacing(12);
        auto* title = new textfields::Label(QStringLiteral("Projects"), this);
        title->setFluentTypography(Typography::FontRole::Subtitle);
        body->addWidget(title);
        auto* description = new textfields::Label(QStringLiteral("Search, combine filters, and arrange your columns."),
            this);
        description->setFluentTypography(Typography::FontRole::Caption);
        description->setTextColorRole(textfields::Label::TextColorRole::Secondary);
        description->setWordWrap(true);
        body->addWidget(description);

        m_toolbar = new layout::DataGridToolBar(this);
        m_toolbar->searchField()->setPlaceholderText(QStringLiteral("Search projects…"));
        m_toolbar->addFilter(QStringLiteral("status"), QStringLiteral("Status"),
            {QStringLiteral("Active"), QStringLiteral("Review")});
        m_toolbar->addFilter(QStringLiteral("owner"), QStringLiteral("Owner"),
            {QStringLiteral("Maya"), QStringLiteral("Noah"), QStringLiteral("Priya"), QStringLiteral("Riley")});
        body->addWidget(m_toolbar);

        m_model = new QStandardItemModel(this);
        m_model->setHorizontalHeaderLabels(
            {QStringLiteral("Project"), QStringLiteral("Owner"), QStringLiteral("Status"), QStringLiteral("Priority")});
        m_model->setHeaderData(3, Qt::Horizontal, int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
        const QStringList names{QStringLiteral("Aurora"), QStringLiteral("Beacon"), QStringLiteral("Canvas"),
            QStringLiteral("Delta"), QStringLiteral("Ember"), QStringLiteral("Fjord"), QStringLiteral("Grove"),
            QStringLiteral("Harbor")};
        const QStringList owners{QStringLiteral("Maya"), QStringLiteral("Noah"), QStringLiteral("Priya"),
            QStringLiteral("Riley")};
        for (int r = 0; r < names.size(); ++r) {
            auto* priority = new QStandardItem;
            priority->setData(r % 4 + 1, Qt::EditRole);
            priority->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_model->appendRow({new QStandardItem(names.at(r)), new QStandardItem(owners.at(r % 4)),
                new QStandardItem(r % 3 == 0 ? QStringLiteral("Review") : QStringLiteral("Active")), priority});
        }
        m_proxy = new ProjectFilterModel(this);
        m_proxy->setSourceModel(m_model);
        m_grid = new collections::DataGrid(this);
        m_grid->setObjectName(QStringLiteral("workspaceDataGrid"));
        m_grid->setAccessibleName(QStringLiteral("Projects"));
        m_grid->setModel(m_proxy);
        m_grid->setBorderVisible(false);
        m_grid->setPlaceholderText(QStringLiteral("No projects match these filters."));
        m_grid->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_grid->setSelectionMode(collections::SelectionMode::Extended);
        m_grid->setSortingEnabled(true);
        m_grid->sortByColumn(0, Qt::AscendingOrder);
        m_grid->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
        m_grid->setColumnWidth(0, 300);
        m_grid->setColumnWidth(1, 180);
        m_grid->setColumnWidth(2, 160);
        m_grid->setColumnWidth(3, 176);
        m_grid->setMinimumHeight(220);
        m_toolbar->setDataGrid(m_grid);
        body->addWidget(m_grid, 1);

        auto* footer = new QHBoxLayout;
        m_summary = new textfields::Label(this);
        m_summary->setObjectName(QStringLiteral("dataGridSummary"));
        m_summary->setFluentTypography(Typography::FontRole::Caption);
        m_summary->setTextColorRole(textfields::Label::TextColorRole::Secondary);
        m_summary->setWordWrap(true);
        footer->addWidget(m_summary, 1);
        auto* save = new basicinput::Button(QStringLiteral("Save view"), this);
        save->setObjectName(QStringLiteral("dataGridSaveView"));
        auto* restore = new basicinput::Button(QStringLiteral("Restore"), this);
        restore->setObjectName(QStringLiteral("dataGridRestoreView"));
        restore->setEnabled(false);
        footer->addWidget(save);
        footer->addWidget(restore);
        body->addLayout(footer);
        connect(save, &QAbstractButton::clicked, this, [this, restore] {
            m_savedColumns = m_grid->saveColumnState();
            m_savedSearch = m_toolbar->searchText();
            m_savedFilters = m_toolbar->filters();
            restore->setEnabled(true);
            status_info::Toast::showOrUpdateToast(this, QStringLiteral("gallery.action-feedback"),
                QStringLiteral("View saved for this demo"), status_info::Toast::Success, 2200, status_info::Toast::Top,
                QMargins(16, 50, 16, 16));
        });
        connect(restore, &QAbstractButton::clicked, this, [this] {
            m_grid->restoreColumnState(m_savedColumns);
            m_toolbar->clearFilters();
            m_toolbar->setSearchText(m_savedSearch);
            for (auto it = m_savedFilters.cbegin(); it != m_savedFilters.cend(); ++it)
                m_toolbar->setFilterValues(it.key(), it.value().toStringList());
            updateSummary();
            status_info::Toast::showOrUpdateToast(this, QStringLiteral("gallery.action-feedback"),
                QStringLiteral("View restored"), status_info::Toast::Success, 2200, status_info::Toast::Top,
                QMargins(16, 50, 16, 16));
        });
        const auto filter = [this] {
            m_proxy->setCriteria(m_toolbar->searchText(), m_toolbar->filters());
            updateSummary();
        };
        connect(m_toolbar, &layout::DataGridToolBar::searchTextChanged, this, filter);
        connect(m_toolbar, &layout::DataGridToolBar::filtersChanged, this, filter);
        connect(m_grid->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] { updateSummary(); });
        m_grid->selectRow(2);
        updateSummary();
    }

private:
    void updateSummary()
    {
        m_summary->setText(QStringLiteral("%1 of %2 projects · %3 selected")
                .arg(m_proxy->rowCount())
                .arg(m_model->rowCount())
                .arg(m_grid->selectionModel()->selectedRows().size()));
    }
    layout::DataGridToolBar* m_toolbar = nullptr;
    collections::DataGrid* m_grid = nullptr;
    QStandardItemModel* m_model = nullptr;
    ProjectFilterModel* m_proxy = nullptr;
    textfields::Label* m_summary = nullptr;
    QByteArray m_savedColumns;
    QString m_savedSearch;
    QVariantMap m_savedFilters;
};
} // namespace fluent::gallery
#endif
