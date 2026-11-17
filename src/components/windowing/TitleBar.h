#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QPointer>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::windowing {

/**
 * @brief Custom title bar container with leading and trailing regions reserved for system window controls.
 * zh_CN: 为系统窗口按钮预留两侧空间的自定义标题栏容器，用于承载自定义标题内容。
 */
class TitleBar : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Widget hosted in the title-bar content area.
     * zh_CN: 标题栏内容区承载的控件。
     */
    Q_PROPERTY(QWidget* contentWidget READ contentWidget WRITE setContentWidget NOTIFY contentWidgetChanged)
    /**
     * @brief Leading width reserved for platform window controls.
     * zh_CN: 为平台窗口控件保留的前置宽度。
     */
    Q_PROPERTY(int systemReservedLeadingWidth
        READ systemReservedLeadingWidth
        WRITE setSystemReservedLeadingWidth
        NOTIFY systemReservedLeadingWidthChanged)
    /**
     * @brief Trailing width reserved for platform window controls.
     * zh_CN: 为平台窗口控件保留的尾部宽度。
     */
    Q_PROPERTY(int systemReservedTrailingWidth
        READ systemReservedTrailingWidth
        WRITE setSystemReservedTrailingWidth
        NOTIFY systemReservedTrailingWidthChanged)
    /**
     * @brief Title bar height in pixels.
     * zh_CN: 标题栏高度，单位为像素。
     */
    Q_PROPERTY(int titleBarHeight READ titleBarHeight WRITE setTitleBarHeight NOTIFY titleBarHeightChanged)
    /**
     * @brief Whether the owning top-level window is currently active.
     * zh_CN: 所属顶层窗口当前是否处于激活状态。
     */
    Q_PROPERTY(bool windowActive READ isWindowActive NOTIFY windowActiveChanged)

public:
    explicit TitleBar(QWidget* parent = nullptr);

    static constexpr int defaultTitleBarHeight() { return kDefaultHeight; }

    QWidget* contentHost() const { return const_cast<TitleBar*>(this); }
    QWidget* contentWidget() const { return m_contentWidget; }
    /**
     * @brief Sets the title-bar content widget without taking ownership.
     * zh_CN: 设置标题栏内容区控件（不负责销毁）。
     */
    void setContentWidget(QWidget* widget);

    int systemReservedLeadingWidth() const { return m_systemReservedLeadingWidth; }
    void setSystemReservedLeadingWidth(int width);

    int systemReservedTrailingWidth() const { return m_systemReservedTrailingWidth; }
    void setSystemReservedTrailingWidth(int width);

    int titleBarHeight() const { return m_titleBarHeight; }
    void setTitleBarHeight(int height);

    bool isWindowActive() const { return m_windowActive; }

    QVector<QRect> dragExclusionRects() const;

    /**
     * @brief Re-publishes non-draggable child control rects to the window hit-test region.
     * zh_CN: 当标题栏内控件移动或更改启用状态且未触发尺寸变化时调用，以刷新窗口可点击区域。
     */
    void refreshChromeExclusions();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

signals:
    void contentWidgetChanged(QWidget* widget);
    void systemReservedLeadingWidthChanged(int width);
    void systemReservedTrailingWidthChanged(int width);
    void titleBarHeightChanged(int height);
    void windowActiveChanged(bool active);
    void dragStarted(const QPoint& globalPos);
    void dragMoved(const QPoint& globalPos);
    void dragFinished();
    void doubleClicked(const QPoint& globalPos);
    void contextMenuRequested(const QPoint& globalPos);
    void chromeGeometryChanged();

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    static constexpr int kDefaultHeight = 36;

    bool isDragExcludedWidget(const QWidget* widget) const;
    void updateContentWidgetAnchor();

    fluent::AnchorLayout* m_anchorLayout = nullptr;
    QPointer<QWidget> m_contentWidget;
    int m_systemReservedLeadingWidth = 0;
    int m_systemReservedTrailingWidth = 0;
    int m_titleBarHeight = kDefaultHeight;
    bool m_windowActive = false;
    bool m_dragging = false;
};

} // namespace fluent::windowing

#endif // TITLEBAR_H
