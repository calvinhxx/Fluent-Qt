#ifndef TABVIEW_H
#define TABVIEW_H

#include <QString>
#include <QHash>
#include <QPoint>
#include <QRect>
#include <QVariant>
#include <QVector>
#include <QWidget>

#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"

class QColor;
class QEvent;
class QFocusEvent;
class QFont;
class QFontMetrics;
class QGraphicsOpacityEffect;
class QKeyEvent;
class QMouseEvent;
class QObject;
class QPainter;
class QPaintEvent;
class QPropertyAnimation;
class QResizeEvent;
class QScrollBar;
class QVariantAnimation;
class QWheelEvent;

namespace view::basicinput {
class Button;
}

namespace view::textfields {
class Label;
}

namespace view::navigation {

struct TabViewItem {
    QString text;
    QString iconGlyph;
    bool closable = true;
    bool enabled = true;
    QVariant data;
    QString accessibleName;

    TabViewItem() = default;

    explicit TabViewItem(const QString& itemText)
        : text(itemText)
    {
    }

    TabViewItem(const QString& itemText,
                const QString& itemIconGlyph,
                bool itemClosable = true,
                bool itemEnabled = true,
                const QVariant& itemData = QVariant(),
                const QString& itemAccessibleName = QString())
        : text(itemText)
        , iconGlyph(itemIconGlyph)
        , closable(itemClosable)
        , enabled(itemEnabled)
        , data(itemData)
        , accessibleName(itemAccessibleName)
    {
    }
};

/**
 * @brief Internal tab strip that paints tab chrome and indicator geometry.
 * zh_CN: 绘制 tab 外观和指示器几何的内部 tab 条。
 *
 * TabStrip keeps tab hit testing, close/add affordances, drag state, and animated
 * indicator rects separate from TabView's page ownership.
 * zh_CN: TabStrip 将 tab 命中测试、关闭/新增入口、拖拽状态和指示器动画矩形与
 * TabView 的页面所有权解耦。
 */
class TabStrip : public QWidget, public FluentElement {
    Q_OBJECT
    /**
     * @brief Current animated indicator rectangle in TabStrip coordinates.
     * zh_CN: TabStrip 坐标系下当前动画指示器矩形。
     */
    Q_PROPERTY(QRect animatedIndicatorRect READ animatedIndicatorRect WRITE setAnimatedIndicatorRect)

public:
    enum class WidthMode {
        Equal,
        SizeToContent,
        Compact
    };

    enum class CloseButtonOverlayMode {
        Auto,
        OnHover,
        Always
    };

    explicit TabStrip(QWidget* parent = nullptr);

    void setItems(const QVector<TabViewItem>& items);
    void revealTab(int index);
    QVector<TabViewItem> items() const { return m_items; }

    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int index);

    WidthMode widthMode() const { return m_widthMode; }
    void setWidthMode(WidthMode mode);

    CloseButtonOverlayMode closeButtonOverlayMode() const { return m_closeButtonOverlayMode; }
    void setCloseButtonOverlayMode(CloseButtonOverlayMode mode);

    bool tabsClosable() const { return m_tabsClosable; }
    bool areTabsClosable() const { return tabsClosable(); }
    void setTabsClosable(bool closable);

    bool addButtonVisible() const { return m_addButtonVisible; }
    bool isAddButtonVisible() const { return addButtonVisible(); }
    void setAddButtonVisible(bool visible);

    bool tabReorderEnabled() const { return m_tabReorderEnabled; }
    bool isTabReorderEnabled() const { return tabReorderEnabled(); }
    void setTabReorderEnabled(bool enabled);

    bool keyboardAcceleratorsEnabled() const { return m_keyboardAcceleratorsEnabled; }
    bool areKeyboardAcceleratorsEnabled() const { return keyboardAcceleratorsEnabled(); }
    void setKeyboardAcceleratorsEnabled(bool enabled);

    QString tabFontRole() const { return m_tabFontRole; }
    void setTabFontRole(const QString& role);

    QString iconFontFamily() const { return m_iconFontFamily; }
    void setIconFontFamily(const QString& family);

    QRect tabGeometry(int index) const;
    QRect closeButtonGeometry(int index) const;
    QRect addButtonGeometry() const;
    QRect overflowBackGeometry() const;
    QRect overflowForwardGeometry() const;
    QVector<int> visibleTabIndexes() const;
    int rowHeight() const;
    bool handleForwardedMouseEvent(QMouseEvent* event);
    bool handleForwardedKeyEvent(QKeyEvent* event);
    bool handleForwardedWheelEvent(QWheelEvent* event);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

signals:
    void selectedIndexRequested(int index);
    void tabCloseRequested(int index);
    void addTabRequested();
    void tabMoveRequested(int from, int to);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    enum class HitKind {
        None,
        Tab,
        Close,
        Add,
        OverflowBack,
        OverflowForward
    };

    struct Metrics {
        int rowHeight = 40;
        int tabHeight = 32;
        int buttonWidth = 40;
        int iconSlot = 24;
        int closeButtonSize = 24;
        int horizontalPadding = 12;
        int textGap = 8;
        int minTabWidth = 48;
        int preferredTabWidth = 160;
        int maxTabWidth = 220;
        int compactInactiveWidth = 48;
        int cornerRadius = 4;
        int selectionIndicatorHeight = 3;
        int iconPixelSize = 16;
        int closeIconPixelSize = 12;
    };

    struct TabRecord {
        int tabIndex = -1;
        QRect tabRect;
        QRect iconRect;
        QRect textRect;
        QRect closeRect;
        bool closeVisible = false;
    };

    struct TabHeaderWidgets {
        int tabIndex = -1;
        view::textfields::Label* label = nullptr;
        view::basicinput::Button* closeButton = nullptr;
        QGraphicsOpacityEffect* labelOpacity = nullptr;
        QGraphicsOpacityEffect* closeOpacity = nullptr;
    };

    struct HitRecord {
        HitKind kind = HitKind::None;
        int tabIndex = -1;
    };

    bool isValidIndex(int index) const;
    bool isSelectableIndex(int index) const;
    bool isCloseableIndex(int index) const;
    Metrics metrics() const;
    QFont tabFont() const;
    QFont iconFont(int pixelSize) const;
    QString normalizedString(const QString& value, const QString& fallback) const;
    void invalidateLayout();
    void ensureLayout() const;
    void updateLayout();
    int dropTargetIndexForPosition(const QPoint& position) const;
    int dragOffsetForRecord(const TabRecord& record) const;
    int targetDragOffsetForRecord(const TabRecord& record, int targetIndex) const;
    QHash<int, qreal> dragOffsetMapForTarget(int targetIndex) const;
    void setDragTargetIndex(int index);
    void animateDragOffsets(const QHash<int, qreal>& startOffsets, const QHash<int, qreal>& endOffsets);
    void stopDragOffsetAnimation();
    QRect dragInsertionIndicatorRect() const;
    TabRecord visualRecordForRecord(const TabRecord& record) const;
    void clearDragState();
    bool compactExpansionTarget(int index) const;
    qreal compactExpansionProgress(int index) const;
    void animateCompactExpansion(int index, qreal start, qreal end);
    void stopCompactExpansionAnimation(int index);
    int naturalTabWidth(int index, const QFontMetrics& fontMetrics, bool reserveClose) const;
    bool shouldReserveCloseSpace(int index) const;
    bool shouldShowCloseButton(int index) const;
    QVector<int> computeVisibleIndexes(int stripWidth, const QVector<int>& widths, bool overflow) const;
    void ensureSelectedTabVisible();
    int nextEnabledIndex(int from, int direction) const;
    int firstEnabledIndex() const;
    int lastEnabledIndex() const;
    TabRecord* recordForTab(int index);
    const TabRecord* recordForTab(int index) const;
    QRect indicatorRectForTab(int index) const;
    QRect animatedIndicatorRect() const { return m_animatedIndicatorRect; }
    void setAnimatedIndicatorRect(const QRect& rect);
    void animateIndicator(const QRect& from, const QRect& to);
    void setOverflowScrollValue(int value);
    void syncOverflowScrollBar(bool overflow);
    void updateHeaderWidgets();
    TabHeaderWidgets* widgetsForTabHeader(int index);
    qreal tabRevealOpacity(int index) const;
    void setTabRevealOpacity(int index, qreal opacity);
    void stopRevealAnimation(int index);
    void setHeaderWidgetOpacity(TabHeaderWidgets& widgets, qreal opacity);
    view::basicinput::Button* createIconButton(const QString& glyph, int pixelSize);
    void updateIconButton(view::basicinput::Button* button, const QRect& rect, const QString& glyph, int pixelSize, bool enabled);
    HitRecord hitTest(const QPoint& position) const;
    void setHoveredHit(const HitRecord& hit);
    void clearPressedHit();
    bool sameHit(const HitRecord& lhs, const HitRecord& rhs) const;
    bool isInteractiveHit(const HitRecord& hit) const;
    void activateHit(const HitRecord& hit);
    void selectFromUser(int index);
    void emitCloseRequested(int index);
    void scrollOverflow(int direction);
    void paintRow(QPainter& painter);
    void paintTab(QPainter& painter, const TabRecord& record);
    void paintDragInsertionIndicator(QPainter& painter);
    void paintSelectedIndicator(QPainter& painter);
    void paintButton(QPainter& painter, const QRect& rect, const QString& glyph, const HitRecord& hit, bool enabled);
    void paintFocus(QPainter& painter, const QRect& rect);
    QColor tabFillColor(const TabRecord& record) const;
    QColor textColorForTab(int index) const;
    QColor fillForHit(const HitRecord& hit) const;

    QVector<TabViewItem> m_items;
    mutable QVector<TabRecord> m_tabRecords;
    mutable QVector<int> m_visibleTabIndexes;
    mutable QRect m_addButtonRect;
    mutable QRect m_overflowBackRect;
    mutable QRect m_overflowForwardRect;
    QScrollBar* m_overflowScrollBar = nullptr;
    mutable bool m_layoutDirty = true;
    bool m_updatingOverflowScrollBar = false;
    mutable int m_firstVisibleTab = 0;
    int m_selectedIndex = -1;
    WidthMode m_widthMode = WidthMode::Equal;
    CloseButtonOverlayMode m_closeButtonOverlayMode = CloseButtonOverlayMode::Auto;
    bool m_tabsClosable = true;
    bool m_addButtonVisible = true;
    bool m_tabReorderEnabled = false;
    bool m_keyboardAcceleratorsEnabled = true;
    int m_wheelScrollRemainder = 0;
    QString m_tabFontRole = QStringLiteral("Body");
    QString m_iconFontFamily = QStringLiteral("Segoe Fluent Icons");
    HitRecord m_hoveredHit;
    HitRecord m_pressedHit;
    HitRecord m_focusedHit;
    QPoint m_pressPosition;
    QPoint m_dragPosition;
    int m_dragStartIndex = -1;
    int m_dragTargetIndex = -1;
    int m_dragMouseOffset = 0;
    bool m_dragActive = false;
    bool m_focusVisualVisible = false;
    QHash<int, qreal> m_dragAnimatedOffsets;
    QVariantAnimation* m_dragOffsetAnimation = nullptr;
    QRect m_animatedIndicatorRect;
    QPropertyAnimation* m_indicatorAnimation = nullptr;
    QHash<int, qreal> m_tabRevealProgress;
    QHash<int, QVariantAnimation*> m_tabRevealAnimations;
    QHash<int, qreal> m_compactExpansionProgress;
    QHash<int, QVariantAnimation*> m_compactExpansionAnimations;
    QVector<TabHeaderWidgets> m_headerWidgets;
    view::basicinput::Button* m_addButton = nullptr;
    view::basicinput::Button* m_overflowBackButton = nullptr;
    view::basicinput::Button* m_overflowForwardButton = nullptr;
};

/**
 * @brief Fluent tabbed page host with closable, reorderable, and overflow-aware tabs.
 * zh_CN: 支持关闭、重排和溢出处理的 Fluent 标签页宿主。
 *
 * TabView owns tab metadata, selected index, add-tab affordance, keyboard accelerators,
 * and page hosting while delegating tab-strip painting to TabStrip.
 * zh_CN: TabView 管理 tab 元数据、选中索引、新增入口、键盘加速键和页面承载，
 * tab 条绘制交给 TabStrip。
 */
class TabView : public QWidget, public FluentElement, public view::QMLPlus {
    Q_OBJECT
    /**
     * @brief Number of tabs currently managed by TabView.
     * zh_CN: TabView 当前管理的标签数量。
     */
    Q_PROPERTY(int tabCount READ tabCount NOTIFY tabsChanged)
    /**
     * @brief Currently selected tab index.
     * zh_CN: 当前选中的标签索引。
     */
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    /**
     * @brief Width sizing mode used by tabs.
     * zh_CN: 标签页使用的宽度计算模式。
     */
    Q_PROPERTY(TabWidthMode tabWidthMode READ tabWidthMode WRITE setTabWidthMode NOTIFY tabWidthModeChanged)
    /**
     * @brief Visibility behavior for tab close buttons.
     * zh_CN: 标签关闭按钮的可见性行为。
     */
    Q_PROPERTY(CloseButtonOverlayMode closeButtonOverlayMode READ closeButtonOverlayMode WRITE setCloseButtonOverlayMode NOTIFY closeButtonOverlayModeChanged)
    /**
     * @brief Whether tabs expose close affordances.
     * zh_CN: 标签是否显示关闭入口。
     */
    Q_PROPERTY(bool tabsClosable READ tabsClosable WRITE setTabsClosable NOTIFY tabsClosableChanged)
    /**
     * @brief Whether the add-tab button is visible.
     * zh_CN: 添加标签按钮是否可见。
     */
    Q_PROPERTY(bool addTabButtonVisible READ addTabButtonVisible WRITE setAddTabButtonVisible NOTIFY addTabButtonVisibleChanged)
    /**
     * @brief Whether users can reorder tabs.
     * zh_CN: 用户是否可以重排标签。
     */
    Q_PROPERTY(bool tabReorderEnabled READ tabReorderEnabled WRITE setTabReorderEnabled NOTIFY tabReorderEnabledChanged)
    /**
     * @brief Whether tab keyboard accelerators are enabled.
     * zh_CN: 是否启用标签页键盘快捷操作。
     */
    Q_PROPERTY(bool keyboardAcceleratorsEnabled READ keyboardAcceleratorsEnabled WRITE setKeyboardAcceleratorsEnabled NOTIFY keyboardAcceleratorsEnabledChanged)
    /**
     * @brief Typography role used by tab text.
     * zh_CN: 标签文本使用的排版角色。
     */
    Q_PROPERTY(QString tabFontRole READ tabFontRole WRITE setTabFontRole NOTIFY tabFontRoleChanged)
    /**
     * @brief Icon font family used by tab glyphs.
     * zh_CN: 标签图标字符使用的图标字体族。
     */
    Q_PROPERTY(QString iconFontFamily READ iconFontFamily WRITE setIconFontFamily NOTIFY iconFontFamilyChanged)

public:
    enum class TabWidthMode {
        Equal,
        SizeToContent,
        Compact
    };
    Q_ENUM(TabWidthMode)

    enum class CloseButtonOverlayMode {
        Auto,
        OnHover,
        Always
    };
    Q_ENUM(CloseButtonOverlayMode)

    explicit TabView(QWidget* parent = nullptr);
    ~TabView() override;

    int tabCount() const { return m_items.size(); }
    QVector<TabViewItem> tabs() const { return m_items; }
    TabViewItem tabAt(int index) const;

    int addTab(const QString& text);
    int addTab(const TabViewItem& item);
    bool insertTab(int index, const QString& text);
    bool insertTab(int index, const TabViewItem& item);
    bool removeTab(int index);
    bool closeTab(int index);
    void clearTabs();
    bool moveTab(int from, int to);

    bool setTabText(int index, const QString& text);
    bool setTabIconGlyph(int index, const QString& glyph);
    bool setTabClosable(int index, bool closable);
    bool setTabEnabled(int index, bool enabled);
    bool setTabData(int index, const QVariant& data);
    bool setTabAccessibleName(int index, const QString& accessibleName);

    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int index);

    TabWidthMode tabWidthMode() const { return m_tabWidthMode; }
    void setTabWidthMode(TabWidthMode mode);

    CloseButtonOverlayMode closeButtonOverlayMode() const { return m_closeButtonOverlayMode; }
    void setCloseButtonOverlayMode(CloseButtonOverlayMode mode);

    bool tabsClosable() const { return m_tabsClosable; }
    bool areTabsClosable() const { return tabsClosable(); }
    void setTabsClosable(bool closable);

    bool addTabButtonVisible() const { return m_addTabButtonVisible; }
    bool isAddTabButtonVisible() const { return addTabButtonVisible(); }
    void setAddTabButtonVisible(bool visible);

    bool tabReorderEnabled() const { return m_tabReorderEnabled; }
    bool isTabReorderEnabled() const { return tabReorderEnabled(); }
    void setTabReorderEnabled(bool enabled);

    bool keyboardAcceleratorsEnabled() const { return m_keyboardAcceleratorsEnabled; }
    bool areKeyboardAcceleratorsEnabled() const { return keyboardAcceleratorsEnabled(); }
    void setKeyboardAcceleratorsEnabled(bool enabled);

    QString tabFontRole() const { return m_tabFontRole; }
    void setTabFontRole(const QString& role);
    QString iconFontFamily() const { return m_iconFontFamily; }
    void setIconFontFamily(const QString& family);

    QRect tabGeometry(int index) const;
    QRect closeButtonGeometry(int index) const;
    QRect addButtonGeometry() const;
    QRect overflowBackGeometry() const;
    QRect overflowForwardGeometry() const;
    QVector<int> visibleTabIndexes() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

signals:
    void tabsChanged();
    void selectedIndexChanged(int index);
    void currentChanged(int index);
    void tabWidthModeChanged(TabWidthMode mode);
    void closeButtonOverlayModeChanged(CloseButtonOverlayMode mode);
    void tabsClosableChanged(bool closable);
    void addTabButtonVisibleChanged(bool visible);
    void tabReorderEnabledChanged(bool enabled);
    void keyboardAcceleratorsEnabledChanged(bool enabled);
    void tabFontRoleChanged(const QString& role);
    void iconFontFamilyChanged(const QString& family);
    void tabCloseRequested(int index);
    void addTabRequested();
    void tabMoved(int from, int to);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool isValidIndex(int index) const;
    bool isSelectableIndex(int index) const;
    bool isCloseableIndex(int index) const;
    QString normalizedString(const QString& value, const QString& fallback) const;
    void clampSelectedIndex();
    int firstEnabledIndex() const;
    int lastEnabledIndex() const;
    bool handleShortcutKey(QKeyEvent* event);
    void setActiveShortcutOwner();
    void syncTabStrip();
    void updateChildGeometry();
    QRect translateFromStrip(const QRect& rect) const;
    void updateAccessibleText();

    QVector<TabViewItem> m_items;
    TabStrip* m_tabStrip = nullptr;
    int m_selectedIndex = -1;
    TabWidthMode m_tabWidthMode = TabWidthMode::Equal;
    CloseButtonOverlayMode m_closeButtonOverlayMode = CloseButtonOverlayMode::Auto;
    bool m_tabsClosable = true;
    bool m_addTabButtonVisible = true;
    bool m_tabReorderEnabled = false;
    bool m_keyboardAcceleratorsEnabled = true;
    QString m_tabFontRole = QStringLiteral("Body");
    QString m_iconFontFamily = QStringLiteral("Segoe Fluent Icons");
};

} // namespace view::navigation

Q_DECLARE_METATYPE(view::navigation::TabViewItem)
Q_DECLARE_METATYPE(view::navigation::TabView::TabWidthMode)
Q_DECLARE_METATYPE(view::navigation::TabView::CloseButtonOverlayMode)

#endif // TABVIEW_H
