#ifndef SPLITVIEW_H
#define SPLITVIEW_H

#include <QPointer>
#include <QWidget>
#include <QVector>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/WidgetOwnership.h"

class QMouseEvent;
class QPaintEvent;
class QResizeEvent;

namespace fluent::collections {

struct SplitViewPaneOptions {
    int minimumSize = 48;
    int preferredSize = 160;
    int maximumSize = 16777215;
    bool fill = false;

    SplitViewPaneOptions(int minimumPaneSize = 48,
                         int preferredPaneSize = 160,
                         int maximumPaneSize = 16777215,
                         bool fillPane = false)
        : minimumSize(minimumPaneSize)
        , preferredSize(preferredPaneSize)
        , maximumSize(maximumPaneSize)
        , fill(fillPane)
    {
    }
};

/**
 * @brief Resizable multi-pane container with Fluent splitter handles.
 * zh_CN: 带 Fluent 分隔条的可调整多窗格容器。
 *
 * SplitView manages pane sizes, minimum lengths, fill-pane behavior, orientation,
 * and splitter visuals as one reusable layout component.
 * zh_CN: SplitView 将窗格尺寸、最小长度、填充窗格、方向和分隔条视觉统一封装为可复用布局组件。
 */
class SplitView : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Primary layout or motion orientation.
     * zh_CN: 主要布局或运动方向。
     */
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    /**
     * @brief Whether a split handle is actively being dragged.
     * zh_CN: 分隔手柄当前是否正在拖拽调整。
     */
    Q_PROPERTY(bool resizing READ resizing NOTIFY resizingChanged)
    /**
     * @brief Interactive width of the split handle.
     * zh_CN: 分隔手柄的可交互宽度。
     */
    Q_PROPERTY(int handleWidth READ handleWidth WRITE setHandleWidth NOTIFY handleWidthChanged)
    /**
     * @brief Painted thickness of the split handle indicator.
     * zh_CN: 分隔手柄指示器的绘制厚度。
     */
    Q_PROPERTY(int handleVisualThickness READ handleVisualThickness WRITE setHandleVisualThickness NOTIFY handleVisualThicknessChanged)
    /**
     * @brief Pane index that consumes remaining SplitView space.
     * zh_CN: 占用 SplitView 剩余空间的面板索引。
     */
    Q_PROPERTY(int fillPaneIndex READ fillPaneIndex WRITE setFillPaneIndex NOTIFY fillPaneIndexChanged)
    /**
     * @brief Fallback pane size hint when a pane does not provide one.
     * zh_CN: 面板未提供尺寸时使用的默认 sizeHint。
     */
    Q_PROPERTY(QSize defaultSizeHint READ defaultSizeHint WRITE setDefaultSizeHint NOTIFY defaultSizeHintChanged)
    /**
     * @brief Fallback pane minimum size hint.
     * zh_CN: 面板默认最小尺寸提示。
     */
    Q_PROPERTY(QSize defaultMinimumSizeHint READ defaultMinimumSizeHint WRITE setDefaultMinimumSizeHint NOTIFY defaultMinimumSizeHintChanged)

public:
    explicit SplitView(QWidget* parent = nullptr);
    ~SplitView() override;

    void onThemeUpdated() override;

    int addPane(QWidget* pane, const SplitViewPaneOptions& options = SplitViewPaneOptions());
    /**
     * @brief Appends a pane with an explicit release policy.
     * zh_CN: 使用显式释放策略追加窗格。
     */
    int addPane(QWidget* pane,
                WidgetOwnership ownership,
                const SplitViewPaneOptions& options = SplitViewPaneOptions());
    int insertPane(int index, QWidget* pane, const SplitViewPaneOptions& options = SplitViewPaneOptions());
    /**
     * @brief Inserts a pane with an explicit release policy.
     * zh_CN: 使用显式释放策略插入窗格。
     */
    int insertPane(int index,
                   QWidget* pane,
                   WidgetOwnership ownership,
                   const SplitViewPaneOptions& options = SplitViewPaneOptions());
    /**
     * @brief Legacy removal that transfers the pane to the caller.
     * zh_CN: 兼容旧行为，移除窗格并将其转交给调用方。
     */
    bool removePane(QWidget* pane);
    /**
     * @brief Legacy indexed removal that transfers the pane to the caller.
     * zh_CN: 兼容旧行为，按索引移除窗格并将其转交给调用方。
     */
    QWidget* removePaneAt(int index);
    /**
     * @brief Removes a pane and applies its configured ownership policy.
     * zh_CN: 移除窗格并执行其已配置的所有权策略。
     */
    bool releasePane(QWidget* pane);
    /**
     * @brief Removes an indexed pane and applies its ownership policy.
     * zh_CN: 按索引移除窗格并执行其所有权策略。
     */
    bool releasePaneAt(int index);
    /**
     * @brief Removes a pane without deleting it and transfers it to the caller.
     * zh_CN: 移除窗格但不删除，并将其转交给调用方。
     */
    QWidget* takePaneAt(int index);

    QWidget* paneAt(int index) const;
    int paneCount() const { return m_panes.size(); }
    int indexOf(QWidget* pane) const;
    /**
     * @brief Returns the configured release policy for a hosted pane.
     * zh_CN: 返回托管窗格已配置的释放策略。
     */
    WidgetOwnership paneOwnershipAt(int index) const;

    Qt::Orientation orientation() const { return m_orientation; }
    void setOrientation(Qt::Orientation orientation);

    bool resizing() const { return m_resizing; }

    int handleWidth() const { return m_handleWidth; }
    void setHandleWidth(int width);

    int handleVisualThickness() const { return m_handleVisualThickness; }
    void setHandleVisualThickness(int thickness);

    int fillPaneIndex() const;
    void setFillPaneIndex(int index);

    QSize defaultSizeHint() const { return m_defaultSizeHint; }
    void setDefaultSizeHint(const QSize& size);

    QSize defaultMinimumSizeHint() const { return m_defaultMinimumSizeHint; }
    void setDefaultMinimumSizeHint(const QSize& size);

    void setPaneMinimumSize(QWidget* pane, int size);
    void setPaneMinimumSize(int index, int size);
    int paneMinimumSize(QWidget* pane) const;
    int paneMinimumSize(int index) const;

    void setPanePreferredSize(QWidget* pane, int size);
    void setPanePreferredSize(int index, int size);
    int panePreferredSize(QWidget* pane) const;
    int panePreferredSize(int index) const;

    void setPaneMaximumSize(QWidget* pane, int size);
    void setPaneMaximumSize(int index, int size);
    int paneMaximumSize(QWidget* pane) const;
    int paneMaximumSize(int index) const;

    void setPaneFill(QWidget* pane, bool fill);
    void setPaneFill(int index, bool fill);
    bool isPaneFill(QWidget* pane) const;
    bool isPaneFill(int index) const;

    QRect paneGeometry(int index) const;
    QRect handleGeometry(int index) const;
    int handleCount() const { return m_handleRects.size(); }

    QByteArray saveState() const;
    bool restoreState(const QByteArray& state);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void orientationChanged(Qt::Orientation orientation);
    void resizingChanged(bool resizing);
    void handleWidthChanged(int width);
    void handleVisualThicknessChanged(int thickness);
    void fillPaneIndexChanged(int index);
    void defaultSizeHintChanged(const QSize& size);
    void defaultMinimumSizeHintChanged(const QSize& size);
    void paneCountChanged(int count);
    void paneAdded(int index, QWidget* pane);
    void paneRemoved(int index, QWidget* pane);
    void paneConfigurationChanged(int index);
    void paneSizeChanged(int index, int size);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    struct PaneRecord {
        QPointer<QWidget> widget;
        QWidget* rawWidget = nullptr;
        QPointer<QWidget> originalParent;
        WidgetOwnership ownership = WidgetOwnership::Owned;
        int minimumSize = 48;
        int preferredSize = 160;
        int maximumSize = 16777215;
        bool fill = false;
        QRect geometry;
        QMetaObject::Connection destroyedConnection;
    };

    struct HandlePair {
        int leadingPane = -1;
        int trailingPane = -1;
    };

    struct DragState {
        int handleIndex = -1;
        int leadingPane = -1;
        int trailingPane = -1;
        int startPosition = 0;
        int leadingStartSize = 0;
        int trailingStartSize = 0;
        bool changed = false;
    };

    int axisPosition(const QPoint& point) const;
    int axisLength(const QSize& size) const;
    int dragStartLength(int index) const;
    int crossLength(const QSize& size) const;
    QRect makePaneRect(int origin, int length) const;
    QRect makeHandleRect(int origin) const;

    static QSize normalizedHintSize(const QSize& size);
    static SplitViewPaneOptions normalizedOptions(const SplitViewPaneOptions& options);
    void normalizePane(PaneRecord& pane);
    bool isPaneExplicitlyVisible(const PaneRecord& pane) const;
    QVector<int> visiblePaneIndexes() const;
    int effectiveFillPaneIndex(const QVector<int>& visibleIndexes) const;
    int effectiveFillPaneIndex() const;
    void updateLayout();
    void updateCursorForHover();
    int hitTestHandle(const QPoint& position) const;
    void setResizing(bool resizing);
    void setHoveredHandle(int index);
    void clearDragState();
    void emitPaneSizeIfChanged(int index, int oldSize);
    PaneRecord extractPaneRecord(int index);
    void finishPaneRemoval(int index, QWidget* pane);
    void handlePaneDestroyed(QWidget* pane);
    bool isValidPaneIndex(int index) const;

    QVector<PaneRecord> m_panes;
    QVector<QRect> m_handleRects;
    QVector<HandlePair> m_handlePairs;
    Qt::Orientation m_orientation = Qt::Horizontal;
    int m_handleWidth = 8;
    int m_handleVisualThickness = 2;
    QSize m_defaultSizeHint = QSize(560, 320);
    QSize m_defaultMinimumSizeHint = QSize(160, 96);
    int m_hoveredHandle = -1;
    int m_pressedHandle = -1;
    bool m_resizing = false;
    bool m_layingOut = false;
    DragState m_drag;
};

} // namespace fluent::collections

#endif // SPLITVIEW_H
