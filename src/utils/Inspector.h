#ifndef FLUENTQT_INSPECTOR_H
#define FLUENTQT_INSPECTOR_H

#include <QJsonObject>
#include <QSize>

class QWidget;

namespace fluent::diagnostics {

/**
 * @brief Configures the read-only UI checks performed by Inspector.
 * Defaults suit standard desktop windows; layout-grid checking is off by default because shells often use intentional visual offsets.
 * zh_CN: 配置 Inspector 执行的只读界面检查项。默认适用于桌面应用界面；布局网格检查默认关闭，以免误报有意微调的视觉对齐。
 */
struct InspectorOptions {
    QSize minimumHitArea{24, 24};
    int spacingGrid = 4;
    bool checkClippedText = true;
    bool checkAccessibilityNames = true;
    bool checkHitAreas = true;
    bool checkFocusOrder = true;
    bool checkDuplicateActions = true;
    bool checkNestedScrolling = true;
    bool checkLayoutGrid = false;
};

/**
 * @brief Produces a read-only JSON inspection report for a QWidget tree.
 * Call after the window is shown and pending layout events have settled (schema in inspector-report.schema.json).
 * zh_CN: 为 QWidget 树生成只读的 JSON 检查报告。请在窗口显示且待处理的布局事件完成后调用（字段格式见 inspector-report.schema.json）。
 */
class Inspector final {
public:
    static constexpr int ReportSchemaVersion = 1;

    /**
     * @brief Inspects @p root and returns deterministic findings and summary counts.
     * zh_CN: 检查 @p root 并返回检查结果列表与统计计数。
     */
    static QJsonObject report(QWidget* root, const InspectorOptions& options = InspectorOptions{});
};

} // namespace fluent::diagnostics

#endif // FLUENTQT_INSPECTOR_H
