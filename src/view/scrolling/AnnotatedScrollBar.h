#ifndef ANNOTATEDSCROLLBAR_H
#define ANNOTATEDSCROLLBAR_H

#include <functional>

#include <QColor>
#include <QMetaObject>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QVector>
#include <QWidget>

#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QWheelEvent;

namespace view::status_info {
class ToolTip;
}

namespace view::scrolling {

class ScrollView;

/**
 * @brief Label metadata rendered beside an AnnotatedScrollBar offset.
 * zh_CN: 渲染在 AnnotatedScrollBar 偏移位置旁的标签元数据。
 *
 * The offset is expressed in the same scroll-value coordinate space used by
 * the scrollbar range. detailText can override the detail provider result.
 * zh_CN: offset 使用与滚动条范围相同的滚动值坐标；detailText 可覆盖详情回调结果。
 */
struct AnnotatedScrollBarLabel {
    QString text;
    int offset = 0;
    QString detailText;

    AnnotatedScrollBarLabel() = default;
    AnnotatedScrollBarLabel(const QString& labelText, int scrollOffset,
                            const QString& detail = QString())
        : text(labelText),
          offset(scrollOffset),
          detailText(detail) {}
};

inline bool operator==(const AnnotatedScrollBarLabel& lhs, const AnnotatedScrollBarLabel& rhs)
{
    return lhs.text == rhs.text &&
           lhs.offset == rhs.offset &&
           lhs.detailText == rhs.detailText;
}

inline bool operator!=(const AnnotatedScrollBarLabel& lhs, const AnnotatedScrollBarLabel& rhs)
{
    return !(lhs == rhs);
}

/**
 * @brief Vertical Fluent scrollbar with offset labels and optional detail tooltip.
 * zh_CN: 带偏移标签和可选详情提示的垂直 Fluent 滚动条。
 *
 * AnnotatedScrollBar can mirror a ScrollView or be driven directly through range
 * and value properties, while keeping label layout and tooltip behavior explicit.
 * zh_CN: AnnotatedScrollBar 可镜像 ScrollView，也可直接由范围和值属性驱动，
 * 并显式管理标签布局和提示行为。
 */
class AnnotatedScrollBar : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Minimum scroll value mirrored from the connected ScrollView or set directly.
     * zh_CN: 从连接的 ScrollView 镜像或直接设置的最小滚动值。
     */
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum NOTIFY rangeChanged)
    /**
     * @brief Maximum scroll value mirrored from the connected ScrollView or set directly.
     * zh_CN: 从连接的 ScrollView 镜像或直接设置的最大滚动值。
     */
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum NOTIFY rangeChanged)
    /**
     * @brief Current scroll value that drives thumb and annotation positions.
     * zh_CN: 驱动滑块和注释位置的当前滚动值。
     */
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    /**
     * @brief Page step used for scroll or range movement.
     * zh_CN: 滚动或范围移动使用的页步长。
     */
    Q_PROPERTY(int pageStep READ pageStep WRITE setPageStep NOTIFY pageStepChanged)
    /**
     * @brief Preferred visual size of the control.
     * zh_CN: 控件首选视觉尺寸。
     */
    Q_PROPERTY(QSize preferredSize READ preferredSize WRITE setPreferredSize NOTIFY layoutMetricsChanged)
    /**
     * @brief Minimum rendered scrollbar size.
     * zh_CN: 滚动条最小绘制尺寸。
     */
    Q_PROPERTY(QSize minimumBarSize READ minimumBarSize WRITE setMinimumBarSize NOTIFY layoutMetricsChanged)
    /**
     * @brief Vertical padding inside the control.
     * zh_CN: 控件内部垂直内边距。
     */
    Q_PROPERTY(int verticalPadding READ verticalPadding WRITE setVerticalPadding NOTIFY layoutMetricsChanged)
    /**
     * @brief Caret glyph size used by scroll affordances.
     * zh_CN: 滚动入口使用的箭头图形尺寸。
     */
    Q_PROPERTY(QSize caretSize READ caretSize WRITE setCaretSize NOTIFY layoutMetricsChanged)
    /**
     * @brief Width reserved for annotated labels.
     * zh_CN: 为标注标签保留的列宽。
     */
    Q_PROPERTY(int labelColumnWidth READ labelColumnWidth WRITE setLabelColumnWidth NOTIFY layoutMetricsChanged)
    /**
     * @brief Line height used by annotated labels.
     * zh_CN: 标注标签使用的行高。
     */
    Q_PROPERTY(int labelLineHeight READ labelLineHeight WRITE setLabelLineHeight NOTIFY layoutMetricsChanged)
    /**
     * @brief Minimum spacing kept between visible labels.
     * zh_CN: 可见标签之间保留的最小间距。
     */
    Q_PROPERTY(int minimumLabelSpacing READ minimumLabelSpacing WRITE setMinimumLabelSpacing NOTIFY layoutMetricsChanged)
    /**
     * @brief Width of the scroll position indicator area.
     * zh_CN: 滚动位置指示器区域宽度。
     */
    Q_PROPERTY(int indicatorWidth READ indicatorWidth WRITE setIndicatorWidth NOTIFY layoutMetricsChanged)
    /**
     * @brief Painted thickness of the scroll position indicator.
     * zh_CN: 滚动位置指示器绘制厚度。
     */
    Q_PROPERTY(int indicatorThickness READ indicatorThickness WRITE setIndicatorThickness NOTIFY layoutMetricsChanged)
    /**
     * @brief Fallback line step when no connected scroll view provides one.
     * zh_CN: 未连接滚动视图时使用的备用单行步长。
     */
    Q_PROPERTY(int lineStepFallback READ lineStepFallback WRITE setLineStepFallback NOTIFY layoutMetricsChanged)

public:
    /**
     * @brief Callback that resolves detail text for a scroll offset.
     * zh_CN: 根据滚动偏移生成详情文本的回调。
     */
    using DetailLabelProvider = std::function<QString(int offset)>;

    explicit AnnotatedScrollBar(QWidget* parent = nullptr);
    ~AnnotatedScrollBar() override;

    int minimum() const { return m_minimum; }
    int maximum() const { return m_maximum; }
    int value() const { return m_value; }
    int pageStep() const { return m_pageStep; }

    QSize preferredSize() const { return m_preferredSize; }
    QSize minimumBarSize() const { return m_minimumBarSize; }
    int verticalPadding() const { return m_verticalPadding; }
    QSize caretSize() const { return m_caretSize; }
    int labelColumnWidth() const { return m_labelColumnWidth; }
    int labelLineHeight() const { return m_labelLineHeight; }
    int minimumLabelSpacing() const { return m_minimumLabelSpacing; }
    int indicatorWidth() const { return m_indicatorWidth; }
    int indicatorThickness() const { return m_indicatorThickness; }
    int lineStepFallback() const { return m_lineStepFallback; }

    void setMinimum(int minimum);
    void setMaximum(int maximum);
    void setRange(int minimum, int maximum);
    void setValue(int value);
    void setPageStep(int pageStep);

    void setPreferredSize(const QSize& size);
    void setMinimumBarSize(const QSize& size);
    void setVerticalPadding(int padding);
    void setCaretSize(const QSize& size);
    void setLabelColumnWidth(int width);
    void setLabelLineHeight(int height);
    void setMinimumLabelSpacing(int spacing);
    void setIndicatorWidth(int width);
    void setIndicatorThickness(int thickness);
    void setLineStepFallback(int step);

    /**
     * @brief Returns all authored labels, including labels hidden by collision filtering.
     * zh_CN: 返回所有设置过的标签，包括因碰撞过滤而未显示的标签。
     */
    QVector<AnnotatedScrollBarLabel> labels() const { return m_labels; }

    /**
     * @brief Replaces all labels and invalidates visible-label layout.
     * zh_CN: 替换全部标签，并使可见标签布局失效。
     */
    void setLabels(const QVector<AnnotatedScrollBarLabel>& labels);
    void addLabel(const AnnotatedScrollBarLabel& label);
    void clearLabels();

    /**
     * @brief Installs a provider used when a hovered label needs detail text.
     * zh_CN: 设置标签 hover 时用于生成详情文本的 provider。
     */
    void setDetailLabelProvider(DetailLabelProvider provider);
    void clearDetailLabelProvider();
    bool hasDetailLabelProvider() const { return static_cast<bool>(m_detailLabelProvider); }

    /**
     * @brief Mirrors range/value from a ScrollView and sends scroll requests back to it.
     * zh_CN: 从 ScrollView 同步 range/value，并将滚动请求回传给该 ScrollView。
     */
    void connectToScrollView(ScrollView* scrollView);
    void disconnectScrollView();
    ScrollView* connectedScrollView() const { return m_scrollView.data(); }

    /**
     * @brief Returns the collision-filtered labels currently eligible for painting.
     * zh_CN: 返回当前可绘制的、经过碰撞过滤后的标签。
     */
    QVector<AnnotatedScrollBarLabel> visibleLabels() const;
    int visibleLabelCount() const;
    bool isDetailLabelVisible() const { return m_detailLabelVisible; }
    QString detailLabelText() const { return m_detailLabelText; }
    QPointF indicatorCenter() const;
    int offsetAtPosition(const QPoint& position) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    // Property and interaction notifications.
    // zh_CN: 属性与交互通知信号。
    void rangeChanged(int minimum, int maximum);
    void valueChanged(int value);
    void pageStepChanged(int pageStep);
    void labelsChanged();
    void layoutMetricsChanged();
    void detailLabelRequested(int offset);

    /**
     * @brief Emitted when user interaction requests scrolling to an offset.
     * zh_CN: 用户交互请求滚动到某个偏移位置时发出。
     */
    void scrollRequested(int offset);
    void labelActivated(int offset, const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;
    void onThemeUpdated() override;

private:
    struct VisibleLabel {
        AnnotatedScrollBarLabel label;
        int originalIndex = -1;
        qreal centerY = 0.0;
        QRect rect;
    };

    void init();
    void updateColors();
    void invalidateLabelLayout();
    void ensureLabelLayout() const;
    void syncFromScrollBar();
    void setValueFromExternal(int value);
    bool setValueInternal(int value, bool emitChange);
    void requestScrollTo(int offset);
    void updateDetailForPosition(const QPoint& position);
    void showDetail(const QString& text, const QPoint& localAnchor);
    void hideDetail();
    int lineStep() const;
    QRectF trackRect() const;
    QRect labelColumnRect() const;
    QRect topCaretRect() const;
    QRect bottomCaretRect() const;
    QRectF indicatorRectForValue(int value) const;
    qreal yForOffset(int offset) const;
    int labelIndexAt(const QPoint& position) const;
    int clampedValue(int value) const;
    QFont labelFont() const;
    QFont iconFont(int pixelSize) const;
    void applyLayoutMetricChange(bool updateSizeHints = false);

    int m_minimum = 0;
    int m_maximum = 0;
    int m_value = 0;
    int m_pageStep = 0;

    QSize m_preferredSize = QSize(97, 535);
    QSize m_minimumBarSize = QSize(48, 96);
    int m_verticalPadding = 17;
    QSize m_caretSize = QSize(14, 16);
    int m_labelColumnWidth = 31;
    int m_labelLineHeight = 20;
    int m_minimumLabelSpacing = 20;
    int m_indicatorWidth = 29;
    int m_indicatorThickness = 3;
    int m_lineStepFallback = 16;

    QVector<AnnotatedScrollBarLabel> m_labels;
    mutable QVector<VisibleLabel> m_visibleLabels;
    mutable bool m_labelLayoutDirty = true;
    mutable QSize m_labelLayoutSize;

    DetailLabelProvider m_detailLabelProvider;
    view::status_info::ToolTip* m_detailPopup = nullptr;
    bool m_detailLabelVisible = false;
    QString m_detailLabelText;

    bool m_hovered = false;
    bool m_pressed = false;
    bool m_dragging = false;
    int m_hoveredLabelIndex = -1;

    QPointer<ScrollView> m_scrollView;
    QVector<QMetaObject::Connection> m_scrollConnections;

    QColor m_labelColor;
    QColor m_caretColor;
    QColor m_indicatorColor;
    QColor m_indicatorHoverColor;
    QColor m_disabledColor;
};

} // namespace view::scrolling

#endif // ANNOTATEDSCROLLBAR_H
