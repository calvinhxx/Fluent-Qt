#ifndef BREADCRUMB_H
#define BREADCRUMB_H

#include "compatibility/InputEventCompat.h"
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;

namespace fluent::navigation {

namespace detail {
class BreadcrumbAccessible;
}

struct BreadcrumbItem {
    QString text;
    QVariant data;
    bool enabled = true;
    QString accessibleName;

    BreadcrumbItem() = default;
    explicit BreadcrumbItem(const QString& itemText);
    BreadcrumbItem(const QString& itemText,
        const QVariant& itemData,
        bool itemEnabled = true,
        const QString& itemAccessibleName = QString());
};

/**
 * @brief Path navigation bar with configurable item sizing and overflow collapsing.
 * Emits activation signals on item click or keyboard trigger and collapses excess segments when width is limited.
 * zh_CN: 路径面包屑控件，按顺序展示路径并在点击或按键时发出激活信号，宽度不足时按所选模式折叠多余层级。
 */
class Breadcrumb : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Number of items in the breadcrumb.
     * zh_CN: 面包屑条目数量。
     */
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemsChanged)
    /**
     * @brief Visual density used by breadcrumb items.
     * zh_CN: 面包屑条目的视觉密度。
     */
    Q_PROPERTY(BreadcrumbSize breadcrumbSize READ breadcrumbSize WRITE setBreadcrumbSize NOTIFY breadcrumbSizeChanged)
    /**
     * @brief Overflow behavior used by breadcrumb items.
     * zh_CN: 面包屑条目的溢出行为。
     */
    Q_PROPERTY(OverflowMode overflowMode READ overflowMode WRITE setOverflowMode NOTIFY overflowModeChanged)
    /**
     * @brief Whether activating an item removes all trailing items after it.
     * zh_CN: 激活条目时是否自动移除其后的所有条目。
     */
    Q_PROPERTY(bool autoTruncateOnItemClick
        READ autoTruncateOnItemClick
        WRITE setAutoTruncateOnItemClick
        NOTIFY autoTruncateOnItemClickChanged)
    /**
     * @brief Font role used by standard breadcrumb items.
     * zh_CN: 标准面包屑条目使用的字号样式。
     */
    Q_PROPERTY(Typography::FontRole standardFontRole
        READ standardFontRole
        WRITE setStandardFontRole
        NOTIFY standardFontRoleChanged)
    /**
     * @brief Font role used by large breadcrumb items.
     * zh_CN: 大尺寸面包屑条目使用的字号样式。
     */
    Q_PROPERTY(Typography::FontRole largeFontRole READ largeFontRole WRITE setLargeFontRole NOTIFY largeFontRoleChanged)

public:
    enum class BreadcrumbSize { Standard, Large };
    Q_ENUM(BreadcrumbSize)

    enum class OverflowMode { None, Beginning, Middle };
    Q_ENUM(OverflowMode)

    explicit Breadcrumb(QWidget* parent = nullptr);

    int itemCount() const { return m_items.size(); }
    QVector<BreadcrumbItem> items() const { return m_items; }
    BreadcrumbItem itemAt(int index) const;

    void setItems(const QStringList& items);
    void setItems(const QVector<BreadcrumbItem>& items);
    void appendItem(const BreadcrumbItem& item);
    void appendItem(const QString& text);
    void insertItem(int index, const BreadcrumbItem& item);
    bool removeItemAt(int index);
    void clearItems();

    BreadcrumbSize breadcrumbSize() const { return m_breadcrumbSize; }
    void setBreadcrumbSize(BreadcrumbSize size);

    OverflowMode overflowMode() const { return m_overflowMode; }
    void setOverflowMode(OverflowMode mode);

    bool autoTruncateOnItemClick() const { return m_autoTruncateOnItemClick; }
    void setAutoTruncateOnItemClick(bool enabled);

    Typography::FontRole standardFontRole() const { return m_standardFontRole; }
    void setStandardFontRole(Typography::FontRole role);
    Typography::FontRole largeFontRole() const { return m_largeFontRole; }
    void setLargeFontRole(Typography::FontRole role);

    QRect itemGeometry(int itemIndex) const;
    QRect separatorGeometry(int separatorIndex) const;
    QRect overflowGeometry() const;
    QVector<int> hiddenItemIndexes() const;
    int visibleItemCount() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

signals:
    void itemsChanged();
    void itemClicked(int index);
    void itemActivated(int index, const BreadcrumbItem& item);
    void overflowActivated(const QVector<int>& hiddenIndexes);
    void breadcrumbSizeChanged(BreadcrumbSize size);
    void overflowModeChanged(OverflowMode mode);
    void autoTruncateOnItemClickChanged(bool enabled);
    void standardFontRoleChanged(Typography::FontRole role);
    void largeFontRoleChanged(Typography::FontRole role);

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
    friend class detail::BreadcrumbAccessible;

    enum class RecordType { Item, Separator, Overflow };

    struct Metrics {
        int rowHeight = 20;
        int textHeight = 20;
        int separatorWidth = 16;
        int overflowWidth = 16;
        int itemHorizontalPadding = 4;
        int chevronPixelSize = 12;
        int overflowPixelSize = 16;
        int cornerRadius = 4;
    };

    struct DisplayRecord {
        RecordType type = RecordType::Item;
        int itemIndex = -1;
        QRect rect;
        QRect contentRect;
        int naturalWidth = 0;
    };

    bool isValidIndex(int index) const;
    Metrics metrics() const;
    QFont contentFont() const;
    void invalidateLayout(bool updateGeometryHint = true);
    void ensureLayout() const;
    void updateLayout();
    int naturalItemWidth(int index, const QFontMetrics& fontMetrics) const;
    int requiredWidthForItems(const QVector<int>& visibleItems, bool hasOverflow) const;
    QVector<int> visibleItemsForCurrentMode() const;
    void buildRecords(const QVector<int>& visibleItems, bool hasOverflow, const QVector<int>& hiddenItems);
    void clampFocusedRecord();
    int focusedItemIndex() const;
    int recordAt(const QPoint& position) const;
    int firstInteractiveRecord() const;
    int lastInteractiveRecord() const;
    int nextInteractiveRecord(int from, int direction) const;
    bool isRecordInteractive(int recordIndex) const;
    void setHoveredRecord(int recordIndex);
    void resetPressedRecord();
    void activateRecord(int recordIndex);
    void activateItem(int itemIndex);
    void truncateAfter(int itemIndex);
    void updateAccessibleText();
    void paintItem(QPainter& painter, const DisplayRecord& record, int recordIndex);
    void paintIconRecord(QPainter& painter, const DisplayRecord& record, int recordIndex);
    QColor textColorForRecord(const DisplayRecord& record, int recordIndex) const;
    void paintIconGlyph(QPainter& painter,
        const QRect& rect,
        const QString& glyph,
        const QColor& color,
        int pixelSize,
        int lineHeight);

    QVector<BreadcrumbItem> m_items;
    mutable QVector<DisplayRecord> m_records;
    mutable QVector<int> m_hiddenItemIndexes;
    mutable bool m_layoutDirty = true;
    BreadcrumbSize m_breadcrumbSize = BreadcrumbSize::Standard;
    OverflowMode m_overflowMode = OverflowMode::None;
    bool m_autoTruncateOnItemClick = false;
    int m_hoveredRecord = -1;
    int m_pressedRecord = -1;
    int m_focusedRecord = -1;
    QString m_autoAccessibleName;
    Typography::FontRole m_standardFontRole = Typography::FontRole::Body;
    Typography::FontRole m_largeFontRole = Typography::FontRole::Title;
};

} // namespace fluent::navigation

Q_DECLARE_METATYPE(fluent::navigation::BreadcrumbItem)
Q_DECLARE_METATYPE(fluent::navigation::Breadcrumb::BreadcrumbSize)
Q_DECLARE_METATYPE(fluent::navigation::Breadcrumb::OverflowMode)

#endif // BREADCRUMB_H
