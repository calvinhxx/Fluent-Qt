#ifndef FLUENTQT_COMPONENTS_COLLECTIONS_FILELISTVIEW_H
#define FLUENTQT_COMPONENTS_COLLECTIONS_FILELISTVIEW_H

#include <memory>

#include "components/collections/ListView.h"

namespace fluent::collections {

class FileListItemDelegate;

/**
 * @brief List view for displaying file names, transfer status, and per-row remove/retry actions.
 * Uses an external model without taking ownership and performs no I/O itself. Qt::DisplayRole supplies
 * the file name and MetadataRole supplies supporting text (both wrap without elision). Row clicks preserve
 * ListView's pressed/clicked signals, while the remove/retry buttons emit only removeRequested() or
 * retryRequested(). Delete/Backspace requests removal and Ctrl+R (Cmd+R on macOS) requests retry for
 * Rejected rows. Uses a private delegate without per-row widgets; replacing the delegate is unsupported
 * (use ListView for custom row rendering).
 * zh_CN: 用于展示文件名、传输状态及移除/重试操作的列表视图。
 * 关联外部数据模型（不接管所有权），组件自身不执行文件 I/O。Qt::DisplayRole 提供文件名，MetadataRole 提供辅助说明，
 * 两者均自动换行不截断。点击行主体保持 ListView 的 pressed/clicked 信号，点击移除/重试按钮仅发出 removeRequested()
 * 或 retryRequested()；Delete/Backspace 请求移除当前行，Ctrl+R（macOS 下为 Cmd+R）请求重试 Rejected 行。
 * 内部使用私有代理绘制且不逐行创建控件；不支持替换代理，自定义行样式请改用 ListView。
 */
class FileListView : public ListView {
    Q_OBJECT
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)

public:
    /**
     * @brief Display status of a file row; actual transfers are handled by the application.
     * zh_CN: 文件行的展示状态；实际传输逻辑由应用外部处理。
     */
    enum Status { Ready = 0, Uploading = 1, Rejected = 2 };
    Q_ENUM(Status)

    /**
     * @brief Model roles read by FileListView in addition to Qt::DisplayRole.
     * MetadataRole takes QString, StatusRole defaults to Ready, ProgressRole is clamped to [0, 1],
     * RemovableRole defaults to true, and RetryableRole defaults to false (effective only when Status is
     * Rejected). Standard Qt::ItemIsEnabled flags gate actions, and Qt::AccessibleTextRole /
     * Qt::AccessibleDescriptionRole override the generated accessibility text.
     * zh_CN: 除 Qt::DisplayRole 外使用的自定义模型角色。
     * MetadataRole 为 QString；StatusRole 默认 Ready；ProgressRole 为限制在 [0, 1] 的进度值；
     * RemovableRole 默认 true；RetryableRole 默认 false（仅 Status 为 Rejected 时可重试）。
     * 所有操作受 Qt::ItemIsEnabled 约束，可用 Qt::AccessibleTextRole / AccessibleDescriptionRole 覆盖默认无障碍文本。
     */
    enum DataRole { MetadataRole = Qt::UserRole + 1, StatusRole, ProgressRole, RemovableRole, RetryableRole };
    Q_ENUM(DataRole)

    explicit FileListView(QWidget* parent = nullptr);
    ~FileListView() override;

    /**
     * @brief Associates an external model without taking ownership or modifying its data.
     * zh_CN: 关联外部数据模型，不改变其父对象也不修改其数据。
     */
    void setModel(QAbstractItemModel* model) override;

    bool isAnimationEnabled() const;
    /**
     * @brief Enables or disables hover and progress transition animations (subject to MotionPolicy).
     * zh_CN: 启用或关闭悬停与进度条过渡动画（同时受 MotionPolicy 控制）。
     */
    void setAnimationEnabled(bool enabled);

signals:
    void animationEnabledChanged(bool enabled);
    /**
     * @brief Emitted when the user triggers removal for a row; the caller updates the model.
     * zh_CN: 用户请求移除指定行时发出；是否修改模型由调用方决定。
     */
    void removeRequested(const QModelIndex& index);
    /**
     * @brief Emitted when the user triggers retry for a Rejected row.
     * zh_CN: 用户请求重试 Rejected 状态的行时发出。
     */
    void retryRequested(const QModelIndex& index);

protected:
    void dataChanged(const QModelIndex& topLeft,
        const QModelIndex& bottomRight,
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
