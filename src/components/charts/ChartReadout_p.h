#pragma once

#include <QVector>
#include "components/dialogs_flyouts/Popup.h"

namespace fluent::textfields {
class Label;
}

namespace fluent::charts::detail {

struct ReadoutRow {
    QString name, text;
    QColor color;
    bool selected = false;
};

// One passive, reusable surface per chart; never one widget per data point.
class ChartReadout final : public dialogs_flyouts::Popup {
public:
    explicit ChartReadout(QWidget* chart);
    void present(const QString& heading, const QVector<ReadoutRow>& rows, const QPointF& anchor);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Row {
        textfields::Label *marker, *name, *value;
        QColor color;
        qreal dpr = 0;
    };
    QPointer<QWidget> m_chart;
    textfields::Label* m_heading;
    textfields::Label* m_more;
    QVector<Row> m_rows;
};

} // namespace fluent::charts::detail
