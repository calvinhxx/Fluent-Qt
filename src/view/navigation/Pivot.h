#ifndef PIVOT_H
#define PIVOT_H

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

namespace view::navigation {

struct PivotItem {
    QString header;
    QString iconGlyph;
    bool enabled = true;
    QVariant data;
    QString accessibleName;

    PivotItem() = default;
    explicit PivotItem(const QString& itemHeader);
    PivotItem(const QString& itemHeader,
              const QString& itemIconGlyph,
              bool itemEnabled = true,
              const QVariant& itemData = QVariant(),
              const QString& itemAccessibleName = QString());
};

/**
 * @brief Pivot-style horizontal navigation selector.
 * zh_CN: Pivot 风格的水平导航选择器。
 *
 * Pivot shares item overflow and selection concepts with SelectorBar while
 * presenting the more text-centric Pivot surface expected by tabbed sections.
 * zh_CN: Pivot 与 SelectorBar 共享 item 溢出和选择概念，但呈现更偏文本型的 Pivot 表面，
 * 适合分段内容导航。
 */
class Pivot : public QWidget, public FluentElement, public QMLPlus {
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

    explicit Pivot(QWidget* parent = nullptr);
    ~Pivot() override;

    int itemCount() const { return m_items.size(); }
    QVector<PivotItem> items() const { return m_items; }
    PivotItem itemAt(int index) const;

    int addItem(const QString& header);
    int addItem(const PivotItem& item);
    bool insertItem(int index, const QString& header);
    bool insertItem(int index, const PivotItem& item);
    bool removeItem(int index);
    void clearItems();

    bool setItemHeader(int index, const QString& header);
    bool setItemIconGlyph(int index, const QString& glyph);
    bool setItemEnabled(int index, bool enabled);
    bool setItemData(int index, const QVariant& data);
    bool setItemAccessibleName(int index, const QString& accessibleName);

    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int index);
    void clearSelection();

    OverflowBehavior overflowBehavior() const { return m_overflowBehavior; }
    void setOverflowBehavior(OverflowBehavior behavior);

    QString itemFontRole() const { return m_itemFontRole; }
    void setItemFontRole(const QString& role);
    QString iconFontFamily() const { return m_iconFontFamily; }
    void setIconFontFamily(const QString& family);

    QRect headerRowGeometry() const;
    QRect itemHeaderGeometry(int index) const;
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
    void itemActivated(int index, const PivotItem& item);
    void selectedIndexChanged(int index);
    void currentChanged(int index);
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
        Header,
        OverflowBack,
        OverflowForward,
        OverflowMore
    };

    struct HeaderRecord {
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
        int headerRowHeight = 44;
        int headerVisualHeight = 36;
        int horizontalPadding = 16;
        int iconSize = 16;
        int iconGap = 8;
        int minHeaderWidth = 48;
        int maxHeaderWidth = 220;
        int overflowButtonWidth = 40;
        int selectedIndicatorHeight = 3;
    };

    bool isValidIndex(int index) const;
    bool isSelectableIndex(int index) const;
    QString normalizedString(const QString& value, const QString& fallback) const;
    QFont itemFont() const;
    QFont iconFont(int pixelSize) const;
    Metrics metrics() const;

    void invalidateLayout(bool updateGeometryHint = true);
    void ensureLayout() const;
    void updateLayout();
    int naturalHeaderWidth(int index, const QFontMetrics& fontMetrics) const;
    QVector<int> computeVisibleIndexes(int availableWidth, const QVector<int>& widths, bool overflow) const;
    void ensureSelectedHeaderVisible();
    void clampSelectedIndexAfterMutation(int preferredIndex, int previousIndex, bool emitSelection);
    int firstEnabledIndex() const;
    int lastEnabledIndex() const;
    int nextEnabledIndex(int from, int direction) const;
    void setSelectedIndexInternal(int index, bool emitSignals, bool animated);
    void updateAccessibleText();

    const HeaderRecord* recordForItem(int index) const;
    HitRecord hitTest(const QPoint& position) const;
    bool sameHit(const HitRecord& lhs, const HitRecord& rhs) const;
    bool isInteractiveHit(const HitRecord& hit) const;
    void setHoveredHit(const HitRecord& hit);
    void clearPressedHit();
    void focusItem(int index);
    void activateHit(const HitRecord& hit);
    void activateHeader(int index);
    void scrollOverflow(int direction);

    void paintBackground(QPainter& painter) const;
    void paintHeader(QPainter& painter, const HeaderRecord& record) const;
    void paintOverflowButton(QPainter& painter, const QRect& rect, const QString& glyph, const HitRecord& hit, bool enabled) const;
    QColor headerTextColor(int index) const;

    QVector<PivotItem> m_items;
    mutable QVector<HeaderRecord> m_headerRecords;
    mutable QVector<int> m_visibleIndexes;
    mutable QVector<int> m_hiddenIndexes;
    mutable QRect m_headerRowRect;
    mutable QRect m_overflowBackRect;
    mutable QRect m_overflowForwardRect;
    mutable QRect m_overflowMoreRect;
    mutable bool m_layoutDirty = true;
    mutable int m_firstVisibleIndex = 0;
    int m_selectedIndex = -1;
    int m_focusedIndex = -1;
    OverflowBehavior m_overflowBehavior = OverflowBehavior::ScrollButtons;
    QString m_itemFontRole = QStringLiteral("Body");
    QString m_iconFontFamily = QStringLiteral("Segoe Fluent Icons");
    HitRecord m_hoveredHit;
    HitRecord m_pressedHit;
};

} // namespace view::navigation

Q_DECLARE_METATYPE(view::navigation::PivotItem)
Q_DECLARE_METATYPE(view::navigation::Pivot::OverflowBehavior)

#endif // PIVOT_H
