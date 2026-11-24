#include <gtest/gtest.h>

#include <QItemSelectionModel>
#include <QPersistentModelIndex>

#include "ui/models/TelemetryTableModel.h"

namespace {

TEST(MonitorModelTest, EqualFilterResultsKeepTheSelectedCell)
{
    fluentqt_test_monitor::ui::TelemetryTableModel model;
    fluentqt_test_monitor::domain::TelemetryEvent event{"gateway", "eu", 25, "Healthy"};
    model.setEvents({event});
    QItemSelectionModel selection(&model);
    const QPersistentModelIndex selected(model.index(0, 0));
    selection.setCurrentIndex(selected, QItemSelectionModel::ClearAndSelect);

    model.setEvents({event});
    ASSERT_TRUE(selected.isValid());
    EXPECT_TRUE(selection.isSelected(selected));
    EXPECT_EQ(selection.currentIndex(), selected);

    event.latencyMs = 47;
    model.setEvents({event});
    EXPECT_EQ(model.index(0, 2).data().toString(), QStringLiteral("47 ms"));
    model.setEvents({});
    EXPECT_EQ(model.rowCount(), 0);
}

} // namespace
