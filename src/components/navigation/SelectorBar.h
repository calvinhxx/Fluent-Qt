#ifndef SELECTORBAR_H
#define SELECTORBAR_H

#include "compatibility/InputEventCompat.h"
#include <QRect>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QFocusEvent;
class QColor;
class QFont;
class QFontMetrics;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QResizeEvent;
class QVariantAnimation;

namespace fluent::navigation {

namespace detail {
class SelectorBarAccessible;
}

struct SelectorBarItem {
    QString text;
    QString iconGlyph;
    bool enabled = true;
    bool visible = true;
    bool selected = false;
    QVariant data;
    QString accessibleName;

    SelectorBarItem() = default;
    explicit SelectorBarItem(const QString& itemText);
    SelectorBarItem(const QString& itemText,
        const QString& itemIconGlyph,
        bool itemEnabled = true,
        bool itemVisible = true,
        const QVariant& itemData = QVariant(),
        const QString& itemAccessibleName = QString());
};

/**
 * @brief Horizontal selector bar for mutually exclusive navigation items.
 * zh_CN: 用于互斥导航项的水平选择栏。
 *
 * SelectorBar manages selected index, overflow behavior, item typography, icon
 * font settings, and activation signals without owning application pages.
 * zh_CN: SelectorBar 管理选中索引、溢出行为、item 排版、图标字体设置和激活信号，
 * 不拥有应用页面。
 */
class SelectorBar : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Number of items exposed by the navigation control.
     * zh_CN: 导航控件暴露的条目数量。
     */
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged)
    /**
     * @brief Currently selected item index.
     * zh_CN: 当前选中条目索引。
     */
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    /**
     * @brief How the control handles items that exceed available width.
     * zh_CN: 控件如何处理超出可用宽度的条目。
     */
    Q_PROPERTY(OverflowBehavior overflowBehavior
        READ overflowBehavior
        WRITE setOverflowBehavior
        NOTIFY overflowBehaviorChanged)
    /**
     * @brief Typography role used by selector items.
     * zh_CN: 选择器条目使用的排版角色。
     */
    Q_PROPERTY(Typography::FontRole itemFontRole READ itemFontRole WRITE setItemFontRole NOTIFY itemFontRoleChanged)
    /**
     * @brief Iconfont family used for glyph rendering.
     * zh_CN: 图标字符绘制使用的 iconfont 字体族。
     */
    Q_PROPERTY(QString iconFontFamily READ iconFontFamily WRITE setIconFontFamily NOTIFY iconFontFamilyChanged)
    /**
     * @brief Whether the bar paints its layer background.
     * zh_CN: 是否绘制选择栏的图层背景。
     *
     * Disable it to let the bar blend into a host surface that already paints
     * its own background, such as a navigation chrome or a custom title bar.
     * zh_CN: 关闭后选择栏融入自带背景的宿主表面，例如导航 chrome 或自定义标题栏。
     */
    Q_PROPERTY(bool backgroundVisible
        READ isBackgroundVisible
        WRITE setBackgroundVisible
        NOTIFY backgroundVisibleChanged)

public:
    /**
     * @brief Presentation used when items exceed the available width.
     * zh_CN: 条目超出可用宽度时的呈现方式。
     *
     * ScrollButtons pages a window of items. MoreButton keeps the leading items
     * plus the selected and keyboard-focused items visible, and reports the rest
     * through overflowActivated(). If both do not fit, the focused item takes priority.
     * zh_CN: ScrollButtons 按窗口翻页。MoreButton 保持前导条目以及选中、键盘聚焦条目可见，
     * 其余条目经 overflowActivated() 报告。空间不足以同时显示时，优先显示聚焦条目。
     */
    enum class OverflowBehavior { ScrollButtons, MoreButton };
    Q_ENUM(OverflowBehavior)

    explicit SelectorBar(QWidget* parent = nullptr);
    ~SelectorBar() override;

    int itemCount() const { return m_items.size(); }
    QVector<SelectorBarItem> items() const { return m_items; }
    SelectorBarItem itemAt(int index) const;

    int addItem(const QString& text);
    int addItem(const SelectorBarItem& item);
    bool insertItem(int index, const QString& text);
    bool insertItem(int index, const SelectorBarItem& item);
    bool removeItem(int index);
    void clearItems();

    bool setItemText(int index, const QString& text);
    bool setItemIconGlyph(int index, const QString& glyph);
    bool setItemEnabled(int index, bool enabled);
    bool setItemVisible(int index, bool visible);
    bool setItemData(int index, const QVariant& data);
    bool setItemAccessibleName(int index, const QString& accessibleName);
    bool setItemSelected(int index, bool selected);

    int selectedIndex() const { return m_selectedIndex; }
    SelectorBarItem selectedItem() const;
    void setSelectedIndex(int index);
    void clearSelection();

    OverflowBehavior overflowBehavior() const { return m_overflowBehavior; }
    void setOverflowBehavior(OverflowBehavior behavior);

    Typography::FontRole itemFontRole() const { return m_itemFontRole; }
    void setItemFontRole(Typography::FontRole role);
    QString iconFontFamily() const { return m_iconFontFamily; }
    void setIconFontFamily(const QString& family);
    bool isBackgroundVisible() const { return m_backgroundVisible; }
    bool backgroundVisible() const { return isBackgroundVisible(); }
    void setBackgroundVisible(bool visible);

    QRect selectorRowGeometry() const;
    QRect itemGeometry(int index) const;
    QRect selectedIndicatorGeometry(int index) const;
    QRect overflowBackGeometry() const;
    QRect overflowForwardGeometry() const;
    QRect overflowGeometry() const;
    QVector<int> visibleItemIndexes() const;
    QVector<int> hiddenItemIndexes() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

signals:
    void itemCountChanged(int count);
    void itemsChanged();
    void itemActivated(int index, const SelectorBarItem& item);
    /**
     * @brief Emitted for every pointer, keyboard, or accessibility invocation, including the selected item.
     * zh_CN: 每次指针、键盘或辅助功能调用条目时发出，包括再次调用已选中条目。
     *
     * It precedes itemActivated() and the selection signals, which fire only
     * when the selection changes.
     * zh_CN: 它先于 itemActivated() 与选择信号发出；后两者仅在选中变化时发出。
     */
    void itemInvoked(int index, const SelectorBarItem& item);
    void selectedIndexChanged(int index);
    void currentChanged(int index);
    void selectionChanged(int index, const SelectorBarItem& item);
    void overflowBehaviorChanged(OverflowBehavior behavior);
    void itemFontRoleChanged(Typography::FontRole role);
    void iconFontFamilyChanged(const QString& family);
    void backgroundVisibleChanged(bool visible);
    void overflowActivated(const QVector<int>& hiddenIndexes);

protected:
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    friend class detail::SelectorBarAccessible;

    enum class HitKind { None, Item, OverflowBack, OverflowForward, OverflowMore };

    struct ItemRecord {
        int itemIndex = -1;
        QRect rect;
        QRect iconRect;
        QRect textRect;
        QRect indicatorRect;
        bool enabled = true;
    };

    struct HitRecord {
        HitKind kind = HitKind::None;
        int itemIndex = -1;
    };

    struct Metrics {
        int rowHeight = 44;
        int itemVisualHeight = 36;
        int horizontalPadding = 16;
        int iconSize = 16;
        int iconGap = 8;
        int minItemWidth = 48;
        int maxItemWidth = 220;
        int overflowButtonWidth = 40;
        int selectedIndicatorHeight = 3;
        int selectedIndicatorWidth = 20;
    };

    bool isValidIndex(int index) const;
    bool isSelectableIndex(int index) const;
    QVector<int> allVisibleItemIndexes() const;
    QString normalizedString(const QString& value, const QString& fallback) const;
    QFont itemFont() const;
    QFont iconFont(int pixelSize) const;
    Metrics metrics() const;

    void invalidateLayout(bool updateGeometryHint = true);
    void ensureLayout() const;
    void updateLayout();
    int naturalItemWidth(int index, const QFontMetrics& fontMetrics) const;
    QVector<int> computeVisibleIndexes(const QVector<int>& candidates,
        int availableWidth,
        const QVector<int>& widths,
        bool overflow) const;
    QVector<int>
    computePinnedVisibleIndexes(const QVector<int>& candidates, int availableWidth, const QVector<int>& widths) const;
    void ensureIndexVisible(int index);
    int nearestSelectableIndex(int preferredIndex) const;
    int firstSelectableIndex() const;
    int lastSelectableIndex() const;
    int nextSelectableIndex(int from, int direction) const;
    void clampSelectionAfterMutation(int preferredIndex, bool emitSignals);
    void setSelectedIndexInternal(int index, bool emitSignals);
    void syncSelectedFlags();
    void updateAccessibleText();
    QRect indicatorRectForItem(int index) const;
    void setAnimatedIndicatorRect(const QRect& rect);
    void animateIndicator(const QRect& from, const QRect& to);

    const ItemRecord* recordForItem(int index) const;
    HitRecord hitTest(const QPoint& position) const;
    bool sameHit(const HitRecord& lhs, const HitRecord& rhs) const;
    bool isInteractiveHit(const HitRecord& hit) const;
    void setHoveredHit(const HitRecord& hit);
    void clearPressedHit();
    void focusItem(int index);
    void activateHit(const HitRecord& hit);
    void activateItem(int index);
    void scrollOverflow(int direction);

    void paintBackground(QPainter& painter) const;
    void paintItem(QPainter& painter, const ItemRecord& record) const;
    void paintSelectedIndicator(QPainter& painter) const;
    void
    paintOverflowButton(QPainter& painter, const QRect& rect, const QString& glyph, const HitRecord& hit, bool enabled)
        const;
    QColor itemTextColor(int index) const;

    QVector<SelectorBarItem> m_items;
    mutable QVector<ItemRecord> m_itemRecords;
    mutable QVector<int> m_visibleIndexes;
    mutable QVector<int> m_hiddenIndexes;
    mutable QRect m_rowRect;
    mutable QRect m_overflowBackRect;
    mutable QRect m_overflowForwardRect;
    mutable QRect m_overflowMoreRect;
    mutable bool m_layoutDirty = true;
    mutable int m_firstVisibleIndex = 0;
    int m_selectedIndex = -1;
    int m_focusedIndex = -1;
    OverflowBehavior m_overflowBehavior = OverflowBehavior::ScrollButtons;
    Typography::FontRole m_itemFontRole = Typography::FontRole::Body;
    QString m_iconFontFamily;
    bool m_backgroundVisible = true;
    bool m_keyboardFocusVisible = false;
    HitRecord m_hoveredHit;
    HitRecord m_pressedHit;
    QRect m_animatedIndicatorRect;
    QVariantAnimation* m_indicatorAnimation = nullptr;
    QString m_autoAccessibleDescription;
};

} // namespace fluent::navigation

Q_DECLARE_METATYPE(fluent::navigation::SelectorBarItem)
Q_DECLARE_METATYPE(fluent::navigation::SelectorBar::OverflowBehavior)

#endif // SELECTORBAR_H
