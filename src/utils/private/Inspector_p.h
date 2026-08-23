#ifndef FLUENTQT_INSPECTOR_P_H
#define FLUENTQT_INSPECTOR_P_H

#include <utils/Inspector.h>

#include <QJsonObject>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QVector>

class QWidget;

namespace fluent::diagnostics {

enum class InspectorSeverity {
    Info,
    Warning,
    Error,
};

enum class InspectorCategory {
    Text,
    Accessibility,
    Input,
    Focus,
    Layout,
    Actions,
    Scrolling,
};

struct InspectorFinding {
    QString code;
    InspectorCategory category = InspectorCategory::Layout;
    InspectorSeverity severity = InspectorSeverity::Warning;
    QString path;
    QRect rect;
    QString message;
    QJsonObject details;
    QPointer<QWidget> widget;

    QJsonObject toJson() const;
};

QVector<InspectorFinding> inspectFindings(
    QWidget* root,
    const InspectorOptions& options = InspectorOptions{});

} // namespace fluent::diagnostics

#endif // FLUENTQT_INSPECTOR_P_H
