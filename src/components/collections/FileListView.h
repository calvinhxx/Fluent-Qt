#ifndef FLUENTQT_COMPONENTS_COLLECTIONS_FILELISTVIEW_H
#define FLUENTQT_COMPONENTS_COLLECTIONS_FILELISTVIEW_H

#include <memory>

#include "ListView.h"

namespace fluent::collections {

class FileListItemDelegate;

/**
 * @brief List view displaying file names, transfer status, and per-row remove/retry actions.
 * Driven by a Qt item model without disk I/O; Delete requests removal and Ctrl/Cmd+R requests retry.
 * zh_CN: 展示文件名、进度状态及移除/重试按钮的列表视图。通过数据模型驱动（不读写磁盘文件），Delete 请求移除、Ctrl/Cmd+R 请求重试。
 */
class FileListView : public ListView {
    Q_OBJECT
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)

public:
    /**
     * @brief Display status of a file row; actual transfers are handled by the application.
     * zh_CN: 文件行的显示状态；实际传输逻辑由应用处理。
     */
    enum Status { Ready = 0, Uploading = 1, Rejected = 2 };
    Q_ENUM(Status)

    /**
     * @brief Model roles read in addition to Qt::DisplayRole.
     * MetadataRole (QString), StatusRole (Status, default Ready), ProgressRole ([0, 1]),
     * RemovableRole (bool, default true), and RetryableRole (bool, default false for Rejected rows).
     * zh_CN: 除 Qt::DisplayRole 外支持的模型角色。
     * 包含 MetadataRole（辅助文本）、StatusRole（默认 Ready）、ProgressRole（[0, 1] 进度）、
     * RemovableRole（默认 true）与 RetryableRole（默认 false，仅 Rejected 状态生效）。
     */
    enum DataRole { MetadataRole = Qt::UserRole + 1, StatusRole, ProgressRole, RemovableRole, RetryableRole };
    Q_ENUM(DataRole)

    explicit FileListView(QWidget* parent = nullptr);
    ~FileListView() override;

    /**
     * @brief Sets the item model (still managed by the caller).
     * zh_CN: 设置数据模型（仍由应用管理，不会随视图销毁）。
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
