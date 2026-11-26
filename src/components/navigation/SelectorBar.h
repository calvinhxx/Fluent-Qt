#ifndef SELECTORBAR_H
#define SELECTORBAR_H

#include "../../compatibility/InputEventCompat.h"
#include <QRect>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QWidget>

#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"

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
 * @brief Horizontal selector bar for mutually exclusive navigation options.
 * Manages selection, overflow behavior, and activation signals while page widgets remain managed by the caller.
 * zh_CN: 水平互斥导航选择栏，管理选中项、横向溢出与激活信号，页面控件由外部管理。
 */
class SelectorBar : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Number of items in the selector bar.
     * zh_CN: 选择栏条目数量。
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
     * @brief Font role used by selector items.
     * zh_CN: 选项条目使用的字号样式。
     */
    Q_PROPERTY(Typography::FontRole itemFontRole READ itemFontRole WRITE setItemFontRole NOTIFY itemFontRoleChanged)
    /**
     * @brief Icon font family used for glyph rendering.
     * zh_CN: 图标字符绘制使用的字体族。
     */
    Q_PROPERTY(QString iconFontFamily READ iconFontFamily WRITE setIconFontFamily NOTIFY iconFontFamilyChanged)
    /**
     * @brief Whether the bar paints its background fill.
     * Disable to blend into a parent container that already provides a background, such as navigation chrome or a custom title bar.
     * zh_CN: 是否绘制选择栏背景；关闭后可融入导航栏或自定义标题栏等自带背景的容器。
     */
    Q_PROPERTY(bool backgroundVisible
        READ isBackgroundVisible
        WRITE setBackgroundVisible
        NOTIFY backgroundVisibleChanged)

public:
    /**
     * @brief Presentation used when items exceed the available width.
     * ScrollButtons pages through items; MoreButton pins leading, selected, and focused items and reports hidden ones via overflowActivated().
     * zh_CN: 条目超出可用宽度时的显示方式：ScrollButtons 按页切换；MoreButton 优先保留前导与选中/聚焦项，其余通过 overflowActivated() 报告。
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
     * @brief Emitted for every pointer, keyboard, or accessibility invocation, even on the already selected item.
     * Precedes itemActivated() and selection signals, which fire only when selection changes.
     * zh_CN: 每次指针、键盘或无障碍触发条目时发出（包括再次触发当前选中项）；先于 itemActivated() 和选择变更信号发出，后者仅在选中项变化时触发。
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
