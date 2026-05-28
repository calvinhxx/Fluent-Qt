#ifndef SELECTORBAR_H
#define SELECTORBAR_H

#include <QRect>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QWidget>

#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"

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

namespace view::navigation {

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
    Q_PROPERTY(OverflowBehavior overflowBehavior READ overflowBehavior WRITE setOverflowBehavior NOTIFY overflowBehaviorChanged)
    /**
     * @brief Typography role used by selector items.
     * zh_CN: 选择器条目使用的排版角色。
     */
    Q_PROPERTY(QString itemFontRole READ itemFontRole WRITE setItemFontRole NOTIFY itemFontRoleChanged)
    /**
     * @brief Iconfont family used for glyph rendering.
     * zh_CN: 图标字符绘制使用的 iconfont 字体族。
     */
    Q_PROPERTY(QString iconFontFamily READ iconFontFamily WRITE setIconFontFamily NOTIFY iconFontFamilyChanged)

public:
    enum class OverflowBehavior {
        ScrollButtons,
        MoreButton
    };
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

    QString itemFontRole() const { return m_itemFontRole; }
    void setItemFontRole(const QString& role);
    QString iconFontFamily() const { return m_iconFontFamily; }
    void setIconFontFamily(const QString& family);

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
    void selectedIndexChanged(int index);
    void currentChanged(int index);
    void selectionChanged(int index, const SelectorBarItem& item);
    void overflowBehaviorChanged(OverflowBehavior behavior);
    void itemFontRoleChanged(const QString& role);
    void iconFontFamilyChanged(const QString& family);
    void overflowActivated(const QVector<int>& hiddenIndexes);

protected:
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
    enum class HitKind {
        None,
        Item,
        OverflowBack,
        OverflowForward,
        OverflowMore
    };

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
    QVector<int> computeVisibleIndexes(const QVector<int>& candidates, int availableWidth, const QVector<int>& widths, bool overflow) const;
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
    void paintOverflowButton(QPainter& painter, const QRect& rect, const QString& glyph, const HitRecord& hit, bool enabled) const;
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
    QString m_itemFontRole = QStringLiteral("Body");
    QString m_iconFontFamily;
    HitRecord m_hoveredHit;
    HitRecord m_pressedHit;
    QRect m_animatedIndicatorRect;
    QVariantAnimation* m_indicatorAnimation = nullptr;
};

} // namespace view::navigation

Q_DECLARE_METATYPE(view::navigation::SelectorBarItem)
Q_DECLARE_METATYPE(view::navigation::SelectorBar::OverflowBehavior)

#endif // SELECTORBAR_H
