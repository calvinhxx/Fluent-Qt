#ifndef BREADCRUMB_H
#define BREADCRUMB_H

#include <QRect>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QWidget>

#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"

class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;

namespace view::navigation {

struct BreadcrumbItem {
    QString text;
    QVariant data;
    bool enabled = true;
    QString accessibleName;

    BreadcrumbItem() = default;
    explicit BreadcrumbItem(const QString& itemText);
    BreadcrumbItem(const QString& itemText, const QVariant& itemData, bool itemEnabled = true, const QString& itemAccessibleName = QString());
};

/**
 * @brief Breadcrumb navigation control with overflow handling and item sizing.
 * zh_CN: 支持溢出处理和尺寸模式的面包屑导航控件。
 *
 * Breadcrumb represents a path-like item list, emits navigation intent on item
 * activation, and can truncate or overflow segments according to the selected mode.
 * zh_CN: Breadcrumb 表示路径式 item 列表，在激活项时发出导航意图，并按所选模式截断或溢出片段。
 */
class Breadcrumb : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Number of items exposed by the navigation control.
     * zh_CN: 导航控件暴露的条目数量。
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
     * @brief Whether breadcrumb items collapse after item activation.
     * zh_CN: 条目激活后面包屑是否自动收缩。
     */
    Q_PROPERTY(bool autoTruncateOnItemClick READ autoTruncateOnItemClick WRITE setAutoTruncateOnItemClick NOTIFY autoTruncateOnItemClickChanged)
    /**
     * @brief Typography role used by standard breadcrumb items.
     * zh_CN: 标准面包屑条目使用的排版角色。
     */
    Q_PROPERTY(QString standardFontRole READ standardFontRole WRITE setStandardFontRole NOTIFY standardFontRoleChanged)
    /**
     * @brief Typography role used by large breadcrumb items.
     * zh_CN: 大尺寸面包屑条目使用的排版角色。
     */
    Q_PROPERTY(QString largeFontRole READ largeFontRole WRITE setLargeFontRole NOTIFY largeFontRoleChanged)

public:
    enum class BreadcrumbSize {
        Standard,
        Large
    };
    Q_ENUM(BreadcrumbSize)

    enum class OverflowMode {
        None,
        Beginning,
        Middle
    };
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

    QString standardFontRole() const { return m_standardFontRole; }
    void setStandardFontRole(const QString& role);
    QString largeFontRole() const { return m_largeFontRole; }
    void setLargeFontRole(const QString& role);

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
    void standardFontRoleChanged(const QString& role);
    void largeFontRoleChanged(const QString& role);

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
    enum class RecordType {
        Item,
        Separator,
        Overflow
    };

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
    QString normalizedFontRole(const QString& role, const QString& fallback) const;
    void invalidateLayout(bool updateGeometryHint = true);
    void ensureLayout() const;
    void updateLayout();
    int naturalItemWidth(int index, const QFontMetrics& fontMetrics) const;
    int requiredWidthForItems(const QVector<int>& visibleItems, bool hasOverflow) const;
    QVector<int> visibleItemsForCurrentMode() const;
    void buildRecords(const QVector<int>& visibleItems, bool hasOverflow, const QVector<int>& hiddenItems);
    void clampFocusedRecord();
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
    void paintIconGlyph(QPainter& painter, const QRect& rect, const QString& glyph, const QColor& color, int pixelSize, int lineHeight);

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
    QString m_standardFontRole = QStringLiteral("Body");
    QString m_largeFontRole = QStringLiteral("Title");
};

} // namespace view::navigation

Q_DECLARE_METATYPE(view::navigation::BreadcrumbItem)
Q_DECLARE_METATYPE(view::navigation::Breadcrumb::BreadcrumbSize)
Q_DECLARE_METATYPE(view::navigation::Breadcrumb::OverflowMode)

#endif // BREADCRUMB_H
