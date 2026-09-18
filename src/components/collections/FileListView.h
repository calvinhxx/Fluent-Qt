#ifndef FLUENTQT_COMPONENTS_COLLECTIONS_FILELISTVIEW_H
#define FLUENTQT_COMPONENTS_COLLECTIONS_FILELISTVIEW_H

#include <memory>

#include "components/collections/ListView.h"

namespace fluent::collections {

class FileListItemDelegate;

/**
 * @brief Presents file names, transfer status, and commands from a caller-owned model.
 * zh_CN: 从调用方持有的模型展示文件名、传输状态和操作。
 *
 * Qt::DisplayRole supplies the full file name. MetadataRole supplies supporting
 * text; both wrap without elision. This view performs no I/O and never changes
 * the model. Connect its request signals to the application's file controller.
 * Delete/Backspace requests removal; Ctrl+R requests retry for the current row
 * (Cmd+R on macOS with Qt's default modifier mapping).
 * zh_CN: Qt::DisplayRole 提供完整文件名，MetadataRole 提供辅助说明，均换行而不
 * 截断。视图不执行 I/O 或修改模型；应用自行处理操作信号。Delete/Backspace 请求
 * 移除当前行，Ctrl+R 请求重试（macOS 默认 Qt 修饰键映射下为 Cmd+R）。
 * Row-body clicks retain ListView's pressed/clicked signals. The remove and
 * retry hit regions emit only their command request, not a row click.
 * zh_CN: 行主体点击保留 ListView 的 pressed/clicked 信号；移除和重试热区只发送
 * 对应操作请求，不再触发行点击。
 *
 * Rows are painted by a private delegate; no widgets are allocated per file.
 * Long rows are measured as they enter the viewport. Compact height metadata
 * is retained for visited rows; text layout and motion only use visible rows.
 * zh_CN: 行由私有 delegate 绘制，不为每个文件分配控件。长行进入视口时才测量；
 * 已访问行仅保留紧凑的高度信息，文字排版和动效只处理可见行。
 *
 * The view owns its file delegate. Replacing it through inherited delegate
 * setters is unsupported; use ListView for application-defined row rendering.
 * zh_CN: 视图持有文件 delegate，不支持通过继承的 setter 替换；需要自定义行绘制
 * 时应使用 ListView。
 */
class FileListView : public ListView {
    Q_OBJECT
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY
                   animationEnabledChanged)

public:
    /**
     * @brief File presentation status; transfer work remains application-owned.
     * zh_CN: 文件展示状态；实际传输由应用负责。
     */
    enum Status { Ready = 0, Uploading = 1, Rejected = 2 };
    Q_ENUM(Status)

    /**
     * @brief Model roles used by the file delegate, in addition to Qt::DisplayRole.
     * zh_CN: 文件 delegate 在 Qt::DisplayRole 之外使用的模型角色。
     *
     * MetadataRole is QString; StatusRole is Status (Ready by default);
     * ProgressRole is a finite fraction clamped to [0, 1]; RemovableRole is
     * bool (true by default); RetryableRole is bool (false by default).
     * Retry is available only for Rejected rows. Standard ItemIsEnabled flags
     * gate all commands. Qt::AccessibleTextRole/AccessibleDescriptionRole may
     * override the generated accessible name and supporting description.
     * zh_CN: MetadataRole 为 QString，StatusRole 默认 Ready，ProgressRole 为
     * 限定在 [0, 1] 的有限小数；RemovableRole 默认 true，RetryableRole 默认
     * false。仅 Rejected 行可以重试，所有操作遵守 ItemIsEnabled。标准无障碍
     * 文本角色可覆盖自动生成的名称和说明。
     */
    enum DataRole {
        MetadataRole = Qt::UserRole + 1,
        StatusRole,
        ProgressRole,
        RemovableRole,
        RetryableRole
    };
    Q_ENUM(DataRole)

    explicit FileListView(QWidget* parent = nullptr);
    ~FileListView() override;

    /**
     * @brief Borrows a model without reparenting or modifying it.
     * zh_CN: 借用模型，不改变其父对象或修改其内容。
     */
    void setModel(QAbstractItemModel* model) override;

    bool isAnimationEnabled() const;
    /**
     * @brief Enables short hover and progress transitions, subject to MotionPolicy.
     * zh_CN: 在 MotionPolicy 允许时启用短暂的悬停和进度过渡。
     */
    void setAnimationEnabled(bool enabled);

signals:
    void animationEnabledChanged(bool enabled);
    /**
     * @brief Requests removal; the application decides whether to change its model.
     * zh_CN: 请求移除；由应用决定是否修改模型。
     */
    void removeRequested(const QModelIndex& index);
    /**
     * @brief Requests retry without starting a transfer or changing file status.
     * zh_CN: 请求重试，不启动传输或更改文件状态。
     */
    void retryRequested(const QModelIndex& index);

protected:
    void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight,
                     const QVector<int>& roles = QVector<int>()) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool event(QEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void onThemeUpdated() override;

private:
    friend class FileListItemDelegate;
    friend class FileListViewInteraction;
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace fluent::collections

#endif // FLUENTQT_COMPONENTS_COLLECTIONS_FILELISTVIEW_H
