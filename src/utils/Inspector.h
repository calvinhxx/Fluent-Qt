#ifndef FLUENTQT_INSPECTOR_H
#define FLUENTQT_INSPECTOR_H

#include <QJsonObject>
#include <QSize>

class QWidget;

namespace fluent::diagnostics {

/**
 * @brief Configures the read-only checks performed by Inspector.
 * zh_CN: 配置 Inspector 执行的只读检查。
 *
 * The defaults target desktop application surfaces. Layout-grid checking is
 * opt-in because application shells may intentionally use optical offsets.
 * zh_CN: 默认值面向桌面应用界面。布局网格检查默认为关闭，因为应用壳层可能会
 * 有意采用视觉补偿偏移。
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
 * @brief Produces a versioned quality report for a built QWidget tree.
 * zh_CN: 为已构建的 QWidget 树生成带版本的质量报告。
 *
 * Inspector never changes widgets, layout, focus order, or application data.
 * Call it after the target surface is visible and pending layout events have
 * been processed. The JSON shape is documented by inspector-report.schema.json.
 * zh_CN: Inspector 不会修改控件、布局、焦点顺序或应用数据。请在目标界面可见且
 * 待处理布局事件完成后调用。JSON 结构由 inspector-report.schema.json 说明。
 */
class Inspector final {
public:
    static constexpr int ReportSchemaVersion = 1;

    /**
     * @brief Returns deterministic findings and summary counts for root.
     * zh_CN: 返回 root 的确定性问题记录与汇总计数。
     */
    static QJsonObject report(QWidget* root,
                              const InspectorOptions& options = InspectorOptions{});
};

} // namespace fluent::diagnostics

#endif // FLUENTQT_INSPECTOR_H
