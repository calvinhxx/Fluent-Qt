#include "ComboBox.h"
#include <QApplication>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QProxyStyle>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStringListModel>
#include <QStyle>
#include <QWheelEvent>
#include <QtMath>
#include "compatibility/QtCompat.h"
#include "compatibility/TextPaintCompat.h"
#include "components/collections/ListView.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/private/DpiPaintMetrics_p.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include "components/menus_toolbars/private/TextEditingMenu_p.h"
#include "components/scrolling/ScrollBar.h"
#include "components/textfields/LineEdit.h"
#include "design/Animation.h"
#include "design/CornerRadius.h"

namespace {
static constexpr int kPopupShadowMargin = ::Spacing::Standard;
static constexpr int kPopupContentInset = ::Spacing::XSmall / 2;
static constexpr int kPopupWindowMargin = 4;
static constexpr int kPopupItemOuterInset = 5;
static constexpr int kPopupItemTextLeftInset = 16;
static constexpr int kPopupItemTextRightInset = 8;
static constexpr int kClosedFieldTextFitClearance = ::Spacing::XSmall;
static constexpr qreal kPopupShadowIntensity = 0.18;
static constexpr int kPopupShadowLayerCount = 6;
static constexpr int kPopupShadowVerticalOffset = 1;

// Suppress QStyle's PE_PanelLineEdit native panel — ComboBox paints its own bg
class TransparentLineEditStyle : public QProxyStyle {
public:
    void drawPrimitive(PrimitiveElement pe, const QStyleOption* opt, QPainter* p,
                       const QWidget* w = nullptr) const override
    {
        if (pe == PE_PanelLineEdit)
            return;
        QProxyStyle::drawPrimitive(pe, opt, p, w);
    }
};
} // namespace

namespace fluent::basicinput {

// ─── ComboBox popup window. zh_CN: ComboBox 弹层窗口 ───────────────────────

class ComboBox::ComboBoxPopup : public fluent::dialogs_flyouts::Flyout {
public:
    explicit ComboBoxPopup(ComboBox* comboBox);

    void showForComboBox();
    void onThemeUpdated() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    QPoint computePosition() const override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    ComboBox* m_comboBox;
    fluent::collections::ListView* m_listView;
    ComboBoxItemDelegate* m_delegate;
};

// ─── ComboBoxItemDelegate implementation. zh_CN: ComboBoxItemDelegate 实现 ──

ComboBoxItemDelegate::ComboBoxItemDelegate(FluentElement* themeHost, QAbstractItemView* view,
                                           QObject* parent)
    : QStyledItemDelegate(parent), m_themeHost(themeHost), m_view(view)
{}

void ComboBoxItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const
{
    if (!index.isValid())
        return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    FluentElement::Colors colors{};
    FluentElement::Radius radius{};
    if (m_themeHost) {
        colors = m_themeHost->themeColors();
        radius = m_themeHost->themeRadius();
    }

    int itemRightInset = kPopupItemOuterInset;
    if (m_view && m_view->verticalScrollBar() &&
        m_view->verticalScrollBar()->maximum() > m_view->verticalScrollBar()->minimum()) {
        if (auto* listView = qobject_cast<fluent::collections::ListView*>(m_view)) {
            if (auto* scrollBar = listView->verticalFluentScrollBar()) {
                itemRightInset += scrollBar->thickness();
            }
        }
    }
    const QRect logicalBgRect = option.rect.adjusted(kPopupItemOuterInset, 3, -itemRightInset, -3);
    const QRectF bgRect = QStyle::visualRect(option.direction, option.rect, logicalBgRect);
    const int cornerR = radius.control > 0 ? radius.control : 4;

    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isHovered = option.state & QStyle::State_MouseOver;
    const bool isPressed = (option.state & QStyle::State_Sunken) && isHovered;
    const bool isEnabled = option.state & QStyle::State_Enabled;

    QColor bgColor = Qt::transparent;
    QColor textColor = colors.textPrimary;

    if (!isEnabled) {
        textColor = colors.textDisabled;
    } else if (isSelected && isPressed) {
        bgColor = colors.subtleTertiary;
    } else if (isSelected && isHovered) {
        bgColor = colors.subtleSecondary;
    } else if (isSelected) {
        bgColor = colors.subtleSecondary;
    } else if (isPressed) {
        bgColor = colors.subtleTertiary;
    } else if (isHovered) {
        bgColor = colors.subtleSecondary;
    }

    if (bgColor.alpha() > 0) {
        QPainterPath path;
        path.addRoundedRect(bgRect, cornerR, cornerR);
        painter->setPen(Qt::NoPen);
        painter->setBrush(bgColor);
        painter->drawPath(path);
    }

    // ListView owns the selected indicator overlay. The delegate only paints the
    // row background and text; drawing another indicator here creates a double
    // blue pill in ComboBox flyouts.
    const QRect logicalTextRect =
        logicalBgRect.adjusted(kPopupItemTextLeftInset, 0, -kPopupItemTextRightInset, 0);
    const QRectF textSlot = QStyle::visualRect(option.direction, option.rect, logicalTextRect);
    painter->setPen(textColor);
    painter->setFont(option.font);
    const QString text = index.data(Qt::DisplayRole).toString();
    const QFontMetricsF metrics(painter->font());
    const QString elidedText = metrics.elidedText(text, Qt::ElideRight, qRound(textSlot.width()));
    const QRectF textRect =
        fluent::painting::verticallyCenteredTextInkRect(textSlot, metrics, elidedText);
    painter->drawText(textRect,
                      QStyle::visualAlignment(option.direction, Qt::AlignLeft | Qt::AlignVCenter),
                      elidedText);

    painter->restore();
}

QSize ComboBoxItemDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const
{
    return QSize(0, ::Spacing::ControlHeight::Large);
}

// ─── ComboBoxPopup implementation. zh_CN: ComboBoxPopup 实现 ────────────────

ComboBox::ComboBoxPopup::ComboBoxPopup(ComboBox* comboBox) : Flyout(comboBox), m_comboBox(comboBox)
{
    setObjectName("ComboBoxPopup");
    setAnimationEnabled(false);
    setPlacement(fluent::dialogs_flyouts::Flyout::Auto);
    setAnchorOffset(comboBox ? comboBox->popupOffset() : ::Spacing::Small);
    setModal(false);
    setDim(false);
    setClosePolicy(ClosePolicy(CloseOnPressOutside | CloseOnEscape));

    m_listView = new fluent::collections::ListView(this);
    m_listView->setObjectName("ComboBoxPopupListView");
    m_listView->setBorderVisible(false);
    // ComboBoxPopup already paints an opaque bgLayer surface rounded at the
    // overlay radius (8px). A second ListView background would also paint an opaque corner mask
    // rounded at only the control radius (4px) and inset just 2px, so its filled corners poke past
    // the card's 8px-rounded corners as white "dog-ears". Let the card be the single surface.
    // zh_CN: ComboBoxPopup 已绘制按 overlay 圆角(8px)的不透明 bgLayer 表面。再让 ListView
    // 画背景会叠加一层按 control 圆角(4px)、仅内缩 2px 的不透明圆角遮罩,其填充的四角会超出卡片 8px 圆角,
    // 形成白色「狗耳」。让卡片成为唯一表面即可。
    m_listView->setBackgroundVisible(false);
    // The transparent collection viewport must preserve that parent-painted
    // card. On Mica windows its normal stale-pixel cleanup uses Source mode;
    // without this opt-out it erases the locally themed popup surface and
    // exposes the window backdrop (most visible in a dark Gallery preview).
    // zh_CN: 透明列表视口必须保留父级绘制的卡片。Mica 窗口下常规清屏会用
    // Source 模式擦掉局部主题底板，露出窗口背景；暗色 Gallery 预览最明显。
    m_listView->setProperty("fluentPreserveParentSurface", true);
    if (m_listView->viewport())
        m_listView->viewport()->setProperty("fluentPreserveParentSurface", true);
    m_listView->setSelectionMode(fluent::collections::ListView::SelectionMode::Single);
    m_listView->setSpacing(0);

    m_delegate = new ComboBoxItemDelegate(comboBox, m_listView, this);
    m_listView->setItemDelegate(m_delegate);
    m_listView->setFont(comboBox->themeFont(comboBox->fontRole()).toQFont());

    m_listView->setMouseTracking(true);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    connect(m_listView, &fluent::collections::ListView::itemClicked, this, [this](int index) {
        m_comboBox->setCurrentIndex(index);
        m_comboBox->hidePopup();
    });

    connect(this, &ComboBoxPopup::closed, this, [this]() {
        if (m_comboBox)
            m_comboBox->onPopupHidden();
    });

    onThemeUpdated();
}

void ComboBox::ComboBoxPopup::showForComboBox()
{
    m_listView->setModel(m_comboBox->model());
    m_listView->setRootIndex(m_comboBox->rootModelIndex());
    m_listView->setModelColumn(m_comboBox->modelColumn());
    m_listView->setFont(m_comboBox->themeFont(m_comboBox->fontRole()).toQFont());

    if (m_comboBox->currentIndex() >= 0) {
        m_listView->setSelectedIndex(m_comboBox->currentIndex());
    }

    const int itemCount = m_comboBox->count();
    const int itemH = ::Spacing::ControlHeight::Large;
    const int maxVisible = qMin(itemCount, 6);
    const int rowsH = maxVisible * itemH;
    const int sSize = kPopupShadowMargin;
    const int cardInset = kPopupContentInset;
    int widestText = 0;
    const QFontMetrics popupMetrics(m_listView->font());
    for (int index = 0; index < itemCount; ++index)
        widestText = qMax(widestText, popupMetrics.horizontalAdvance(m_comboBox->itemText(index)));

    int scrollClearance = 0;
    if (itemCount > maxVisible) {
        scrollClearance = ::Spacing::XSmall;
        if (auto* scrollBar = m_listView->verticalFluentScrollBar())
            scrollClearance = qMax(scrollClearance, scrollBar->thickness());
    }
    const int textChrome = cardInset * 2 + kPopupItemOuterInset * 2 + kPopupItemTextLeftInset +
                           kPopupItemTextRightInset + scrollClearance + ::Spacing::Standard;
    const int cardW = qMax(qMax(m_comboBox->width(), 120), widestText + textChrome);
    const int cardH = rowsH + cardInset * 2;
    const QSize totalSize = ::fluent::overlay::outerSizeForVisibleCard(QSize(cardW, cardH), sSize);

    setFixedSize(totalSize);
    setAnchorOffset(m_comboBox->m_popupOffset);
    setAnchor(m_comboBox);

    const QRect cardRect = ::fluent::overlay::visibleCardRect(rect(), sSize);
    m_listView->setGeometry(cardRect.adjusted(cardInset, cardInset, -cardInset, -cardInset));
    m_listView->clearMask();
    m_listView->refreshFluentScrollChrome();

    if (isOpen() || isVisible()) {
        move(computePosition());
        show();
        raise();
    } else {
        showAt(m_comboBox);
    }

    if (m_comboBox->currentIndex() >= 0) {
        m_listView->scrollTo(m_listView->model()->index(m_comboBox->currentIndex(), 0),
                             QAbstractItemView::PositionAtCenter);
    }
}

void ComboBox::ComboBoxPopup::onThemeUpdated()
{
    Flyout::onThemeUpdated();
    QPalette pal = palette();
    pal.setColor(QPalette::Window, themeColorsRef().bgLayer);
    setPalette(pal);

    if (m_comboBox) {
        m_listView->setFont(m_comboBox->themeFont(m_comboBox->fontRole()).toQFont());
    }
    if (m_listView && m_listView->viewport())
        m_listView->viewport()->update();
}

void ComboBox::ComboBoxPopup::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // ComboBox dropdowns sit close to their field, so use the compact menu
    // elevation profile instead of Popup's denser floating-card shadow. The
    // smaller spread plus the default 8px anchor gap prevents the shadow from
    // painting back across the closed field in either placement direction.
    // zh_CN: ComboBox 下拉紧邻输入框，使用更轻、更窄的菜单高程，而不是 Popup
    // 的高强度浮层阴影；配合默认 8px 间距，向上或向下弹出时都不会反压输入框。
    const QRect contentRect = ::fluent::overlay::visibleCardRect(rect(), kPopupShadowMargin);
    const int radius = themeRadius().overlay;
    ::fluent::overlay::paintLayeredShadow(painter, contentRect, radius,
                                          themeShadow(Elevation::High), kPopupShadowIntensity,
                                          kPopupShadowLayerCount, kPopupShadowVerticalOffset);

    const auto& colors = themeColorsRef();
    fluent::painting::RoundedSurfacePaint surface;
    surface.fill = colors.bgLayer;
    surface.radius = radius;
    surface.border = colors.strokeCard;
    fluent::painting::paintRoundedSurface(painter, QRectF(contentRect), surface);
}

QPoint ComboBox::ComboBoxPopup::computePosition() const
{
    if (!m_comboBox || !m_comboBox->window())
        return Flyout::computePosition();

    QWidget* top = m_comboBox->window();
    const int shadow = kPopupShadowMargin;
    const QSize cardSize = ::fluent::overlay::visibleCardSize(size(), shadow);
    const int cardH = cardSize.height();
    const QRect anchor(m_comboBox->mapTo(top, QPoint(0, 0)), m_comboBox->size());

    const QRect surface = ::fluent::overlay::overlaySurfaceRect(top);
    const int spaceBelow = surface.bottom() - anchor.bottom();
    const int spaceAbove = anchor.top() - surface.top();
    // Include the anchor gap and the surface safety margin in the fit test. If
    // only the card height is considered, a popup that is a few pixels too tall
    // is first placed below and then clamped upward across its owning field.
    // zh_CN: 适配判断必须计入锚点间距与表面安全边距；只比较卡片高度会让仅差数像素的
    // 弹层先向下打开，再被钳制回输入框上方并与其重叠。
    const int requiredSpace = anchorOffset() + cardH + kPopupWindowMargin;
    const bool fitsBelow = spaceBelow >= requiredSpace;
    const bool fitsAbove = spaceAbove >= requiredSpace;
    const bool placeAbove = !fitsBelow && (fitsAbove || spaceAbove > spaceBelow);

    QPoint cardTopLeft(anchor.left(), placeAbove ? anchor.top() - anchorOffset() - cardH
                                                 : anchor.bottom() + 1 + anchorOffset());

    if (clampToWindow()) {
        cardTopLeft =
            ::fluent::overlay::clampCardTopLeft(cardTopLeft, cardSize, surface, kPopupWindowMargin);
    }

    return ::fluent::overlay::outerTopLeftForVisibleCard(cardTopLeft, shadow);
}

bool ComboBox::ComboBoxPopup::eventFilter(QObject* watched, QEvent* event)
{
    if (event && event->type() == QEvent::MouseButtonPress && m_comboBox) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const QPoint comboLocal = m_comboBox->mapFromGlobal(fluentMouseGlobalPos(mouseEvent));
        const bool pressOnOwner = m_comboBox->rect().contains(comboLocal);
        const bool pressInsidePopup = ::fluent::overlay::visibleCardContains(
            rect(), mapFromGlobal(fluentMouseGlobalPos(mouseEvent)), kPopupShadowMargin);
        if (pressOnOwner && !pressInsidePopup) {
            m_comboBox->m_ignoreNextPopupPress = true;
        }
    }

    return Flyout::eventFilter(watched, event);
}

// ─── ComboBox implementation. zh_CN: ComboBox 主体实现 ─────────────────────

ComboBox::ComboBox(QWidget* parent) : QComboBox(parent)
{
    setAttribute(Qt::WA_Hover);
    // QComboBox defaults to WheelFocus, which grants focus before wheelEvent and changes the
    // selection merely by hovering and scrolling. Fluent ComboBox accepts wheel selection only
    // after deliberate click/tab focus. zh_CN: QComboBox 默认 WheelFocus，会在 wheelEvent 前先抢
    // 焦点，导致仅悬停滚轮就切换选项；Fluent ComboBox 只接受点击/Tab 明确取得焦点后的滚轮输入。
    setFocusPolicy(Qt::StrongFocus);
    setFont(themeFont(m_fontRole).toQFont());
    setFixedHeight(::Spacing::ControlHeight::Standard);

    initAnimation();
    onThemeUpdated();
}

ComboBox::~ComboBox()
{
    delete m_popup.data();
}

void ComboBox::initAnimation()
{
    m_pressAnimation = new QPropertyAnimation(this, "pressProgress", this);
    m_pressAnimation->setDuration(themeAnimation().slow);
    m_pressAnimation->setEasingCurve(themeAnimation().decelerate);
}

void ComboBox::setFontRole(Typography::FontRole role)
{
    if (m_fontRole == role)
        return;
    m_fontRole = role;
    setFont(themeFont(m_fontRole).toQFont());
    updateGeometry();
    emit fontRoleChanged();
    update();
}

void ComboBox::setContentPaddingH(int px)
{
    if (m_contentPaddingH == px)
        return;
    m_contentPaddingH = px;
    updateGeometry();
    emit layoutChanged();
    update();
}

void ComboBox::setContentPaddingV(int px)
{
    if (m_contentPaddingV == px)
        return;
    m_contentPaddingV = px;
    updateGeometry();
    emit layoutChanged();
    update();
}

void ComboBox::setChevronGlyph(const QString& glyph)
{
    if (m_chevronGlyph == glyph)
        return;
    m_chevronGlyph = glyph;
    emit chevronChanged();
    update();
}

void ComboBox::setChevronSize(int size)
{
    if (m_chevronSize == size)
        return;
    m_chevronSize = size;
    emit chevronChanged();
    update();
}

void ComboBox::setChevronOffset(const QPoint& offset)
{
    if (m_chevronOffset == offset)
        return;
    m_chevronOffset = offset;
    emit chevronChanged();
    update();
}

void ComboBox::setPopupOffset(int offset)
{
    if (m_popupOffset == offset)
        return;
    m_popupOffset = offset;
    emit layoutChanged();
}

void ComboBox::setPressProgress(qreal p)
{
    m_pressProgress = p;
    update();
}

void ComboBox::onThemeUpdated()
{
    setFont(themeFont(m_fontRole).toQFont());
    if (m_popup) {
        m_popup->onThemeUpdated();
    }
    synchronizeLineEdit();
    if (m_observedLineEdit) {
        applyLineEditStyle();
    }
    update();
}

QSize ComboBox::sizeHint() const
{
    const auto& sp = themeSpacing();
    QFontMetrics fm(font());
    // Find widest item
    int maxTextW = 80; // Figma: min width 80px
    for (int i = 0; i < count(); ++i) {
        int w = fm.horizontalAdvance(itemText(i));
        maxTextW = qMax(maxTextW, w);
    }
    // chevron area: offset.x + icon size + gap
    const int chevronArea = m_chevronOffset.x() + m_chevronSize + ::Spacing::Gap::Tight;
    // QFontMetrics::horizontalAdvance() is the logical glyph advance. Leave a
    // small clearance so integer layout/raster rounding cannot make an item
    // that nominally fits cross the elision boundary on another platform.
    // zh_CN: 为字形逻辑宽度预留少量余量，避免不同平台的整数布局和栅格化舍入触发省略号。
    const int w = m_contentPaddingH + maxTextW + kClosedFieldTextFitClearance + chevronArea;
    const int h = sp.controlHeight.standard;
    return QSize(w, h);
}

// ── Editable ─────────────────────────────────────────────────────────────────

void ComboBox::setEditable(bool editable)
{
    if (editable == QComboBox::isEditable())
        return;

    if (editable) {
        m_editorMutationInProgress = true;
        QComboBox::setEditable(true);
        auto* editor = new fluent::textfields::LineEdit(this);
        editor->setClearButtonEnabled(false);
        editor->setFontRole(m_fontRole);
        editor->setContentMargins(QMargins(0, 0, 0, 0));
        editor->setFrameVisible(false);
        auto* style = new TransparentLineEditStyle();
        style->setParent(editor);
        editor->setStyle(style);
        editor->setFocusPolicy(Qt::ClickFocus);
        const Qt::ContextMenuPolicy contextMenuPolicy = editor->contextMenuPolicy();
        QComboBox::setLineEdit(editor);
        // QComboBox forces its editor to NoContextMenu. Restore the editor's
        // policy so the Fluent LineEdit keeps its standard editing surface.
        editor->setContextMenuPolicy(contextMenuPolicy);
        m_editorMutationInProgress = false;
        synchronizeLineEdit();
        setMouseTracking(true);
        applyLineEditStyle();
        layoutLineEdit();
        editor->show();
        connect(editor, &fluent::textfields::LineEdit::returnPressed, this,
                [this]() { hidePopup(); });
    } else {
        QPointer<QLineEdit> previousEditor = m_observedLineEdit;
        if (m_observedLineEdit)
            m_observedLineEdit->removeEventFilter(this);
        m_observedLineEdit = nullptr;
        m_editorMutationInProgress = true;
        QComboBox::setEditable(false);
        if (previousEditor)
            delete previousEditor.data();
        m_editorMutationInProgress = false;
        setMouseTracking(false);
        m_chevronHovered = false;
    }
    update();
}

void ComboBox::setLineEdit(QLineEdit* edit)
{
    if (!edit)
        return;
    const Qt::ContextMenuPolicy contextMenuPolicy = edit->contextMenuPolicy();
    m_editorMutationInProgress = true;
    if (!QComboBox::isEditable())
        QComboBox::setEditable(true);
    QComboBox::setLineEdit(edit);
    // Preserve caller intent across QComboBox's internal NoContextMenu
    // assignment. Only DefaultContextMenu is adapted by eventFilter below.
    edit->setContextMenuPolicy(contextMenuPolicy);
    m_editorMutationInProgress = false;
    synchronizeLineEdit();
    applyLineEditStyle();
    layoutLineEdit();
    update();
}

fluent::textfields::LineEdit* ComboBox::fluentLineEdit() const
{
    return qobject_cast<fluent::textfields::LineEdit*>(QComboBox::lineEdit());
}

void ComboBox::setModel(QAbstractItemModel* model)
{
    QComboBox::setModel(model);
    if (m_popupVisible && m_popup)
        m_popup->showForComboBox();
}

void ComboBox::synchronizeLineEdit()
{
    if (m_editorMutationInProgress)
        return;
    QLineEdit* editor = QComboBox::isEditable() ? QComboBox::lineEdit() : nullptr;
    if (m_observedLineEdit == editor)
        return;
    if (m_observedLineEdit)
        m_observedLineEdit->removeEventFilter(this);
    m_observedLineEdit = editor;
    if (m_observedLineEdit)
        m_observedLineEdit->installEventFilter(this);
}

void ComboBox::layoutLineEdit()
{
    synchronizeLineEdit();
    QLineEdit* editor = m_observedLineEdit.data();
    if (!editor)
        return;
    const int chevronAreaW = m_chevronOffset.x() + m_chevronSize + ::Spacing::Gap::Tight;
    const int gap = ::Spacing::Gap::Tight;
    const QRect logicalTextRect = rect().adjusted(m_contentPaddingH, m_contentPaddingV,
                                                  -(chevronAreaW + gap), -m_contentPaddingV);
    const QRect textRect = QStyle::visualRect(layoutDirection(), rect(), logicalTextRect);
    editor->setGeometry(textRect);
}

void ComboBox::applyLineEditStyle()
{
    synchronizeLineEdit();
    QLineEdit* editor = m_observedLineEdit.data();
    if (!editor)
        return;
    editor->setFont(themeFont(m_fontRole).toQFont());
    if (auto* fluentEditor = qobject_cast<fluent::textfields::LineEdit*>(editor)) {
        fluentEditor->setFontRole(m_fontRole);
        fluentEditor->onThemeUpdated();
    }
}

bool ComboBox::event(QEvent* event)
{
    const bool handled = QComboBox::event(event);
    synchronizeLineEdit();
    if (event && event->type() == QEvent::LayoutDirectionChange) {
        layoutLineEdit();
        update();
    }
    return handled;
}

void ComboBox::resizeEvent(QResizeEvent* event)
{
    QComboBox::resizeEvent(event);
    layoutLineEdit();
}

// ── Popup ────────────────────────────────────────────────────────────────────

void ComboBox::showPopup()
{
    if (m_popupVisible && m_popup)
        return;
    m_popupVisible = true;

    if (!m_popup)
        m_popup = new ComboBoxPopup(this);

    m_popup->showForComboBox();
    update();
}

void ComboBox::hidePopup()
{
    if (!m_popupVisible)
        return;
    m_popupVisible = false;
    m_pressed = false;

    if (m_popup)
        m_popup->close();

    update();
    QComboBox::hidePopup();
}

// Private helper called from the popup close lifecycle.
void ComboBox::onPopupHidden()
{
    const bool needsUpdate = m_popupVisible || m_pressed;
    m_popupVisible = false;
    m_pressed = false;
    if (needsUpdate)
        update();
}

// ── Input events. zh_CN: 输入事件 ────────────────────────────────────────────

void ComboBox::enterEvent(FluentEnterEvent* event)
{
    m_hovered = true;
    update();
    QComboBox::enterEvent(event);
}

void ComboBox::leaveEvent(QEvent* event)
{
    m_hovered = false;
    m_chevronHovered = false;
    update();
    QComboBox::leaveEvent(event);
}

void ComboBox::wheelEvent(QWheelEvent* event)
{
    synchronizeLineEdit();
    const bool ownsFocus = hasFocus() || (m_observedLineEdit && m_observedLineEdit->hasFocus());
    if (!ownsFocus) {
        event->ignore();
        return;
    }
    QComboBox::wheelEvent(event);
}

void ComboBox::keyPressEvent(QKeyEvent* event)
{
    synchronizeLineEdit();
    const bool ownsFocus = hasFocus() || (m_observedLineEdit && m_observedLineEdit->hasFocus());
    if (!ownsFocus) {
        event->ignore();
        return;
    }
    QComboBox::keyPressEvent(event);
}

void ComboBox::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (!hasFocus())
            setFocus(Qt::MouseFocusReason);
        if (m_ignoreNextPopupPress) {
            m_ignoreNextPopupPress = false;
            m_pressed = false;
            update();
            event->accept();
            return;
        }

        m_pressed = true;
        // Fire-and-forget bounce animation (0→1, qSin gives 0→peak→0)
        m_pressAnimation->stop();
        m_pressAnimation->setStartValue(0.0);
        m_pressAnimation->setEndValue(1.0);
        m_pressAnimation->start();

        // Toggle popup ourselves — base class has its own popup management
        // that conflicts with our custom popup
        if (m_popupVisible)
            hidePopup();
        else
            showPopup();
    }
    event->accept();
}

void ComboBox::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = false;
        update();
    }
    event->accept();
}

void ComboBox::mouseMoveEvent(QMouseEvent* event)
{
    synchronizeLineEdit();
    if (m_observedLineEdit) {
        const int chevronAreaW = m_chevronOffset.x() + m_chevronSize + ::Spacing::Gap::Tight;
        const QRect logicalChevronRect(width() - chevronAreaW, 0, chevronAreaW, height());
        const QRect chevronRect = QStyle::visualRect(layoutDirection(), rect(), logicalChevronRect);
        const bool over = chevronRect.contains(event->pos());
        if (over != m_chevronHovered) {
            m_chevronHovered = over;
            update();
        }
    }
    QComboBox::mouseMoveEvent(event);
}

bool ComboBox::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_observedLineEdit) {
        if (event->type() == QEvent::FocusIn) {
            m_observedLineEdit->selectAll();
            update();
        } else if (event->type() == QEvent::FocusOut) {
            update();
        } else if (event->type() == QEvent::ContextMenu &&
                   !qobject_cast<fluent::textfields::LineEdit*>(m_observedLineEdit.data()) &&
                   m_observedLineEdit->contextMenuPolicy() == Qt::DefaultContextMenu) {
            auto* contextEvent = static_cast<QContextMenuEvent*>(event);
            if (fluent::menus_toolbars::detail::showTextEditingContextMenu(
                    m_observedLineEdit.data(), m_observedLineEdit->createStandardContextMenu(),
                    contextEvent->globalPos(),
                    QStringLiteral("FluentComboBox.LineEdit.ContextMenu"))) {
                contextEvent->accept();
                return true;
            }

            // Leave an unavailable shared menu unhandled so the original
            // editor can continue through its native context-menu path.
            // zh_CN: 共享菜单不可用时不吞掉事件，让原始 editor
            // 继续执行其原生右键菜单路径。
            contextEvent->ignore();
        }
    }
    return QComboBox::eventFilter(watched, event);
}

// ── Painting. zh_CN: 绘制 ────────────────────────────────────────────────────

void ComboBox::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto& colors = themeColorsRef();
    const auto& radius = themeRadius();

    const bool enabled = isEnabled();
    const QRectF r(rect());
    const fluent::painting::DpiPaintMetrics paintMetrics(painter);

    // ── Background ───────────────────────────────────────────────────────
    // Figma: Outer 1px padding + 4px radius, inner base 3px radius
    // Outer wrapper
    QColor outerBg = Qt::transparent;
    synchronizeLineEdit();
    const bool lineEditFocused = m_observedLineEdit && m_observedLineEdit->hasFocus();
    if (!enabled) {
        outerBg = colors.controlDisabled;
    } else if (m_popupVisible) {
        outerBg = colors.controlTertiary;
    } else if (m_pressed) {
        outerBg = colors.controlTertiary;
    } else if (lineEditFocused) {
        outerBg = colors.controlDefault;
    } else if (m_hovered) {
        outerBg = colors.controlSecondary;
    } else {
        outerBg = colors.controlDefault;
    }

    // Fluent treatment. zh_CN: Fluent 样式。
    // Draw the control background with 1px inset for border
    const qreal outerR = radius.control; // 4px
    const qreal innerR = outerR - 1;     // 3px

    // Fill background
    const auto borderStroke = paintMetrics.alignedStroke(r, 1.0);
    QPainterPath bgPath;
    bgPath.addRoundedRect(borderStroke.rect, outerR, outerR);
    painter.setPen(Qt::NoPen);
    painter.setBrush(outerBg);
    painter.drawPath(bgPath);

    // ── Border ───────────────────────────────────────────────────────────
    // Figma: border rgba(0,0,0,0.06) → strokeDefault
    const QColor borderColor = colors.strokeDefault;

    // Bottom accent stroke when focused/open (WinUI 3 pattern)
    if (lineEditFocused && enabled) {
        // Draw normal border first
        painter.setPen(QPen(borderColor, borderStroke.width));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(borderStroke.rect, outerR, outerR);

        // Accent bottom border (2px)
        const qreal accentH = 2.0;
        QRectF bottomRect(r.left() + 0.5, r.bottom() - accentH - 0.5, r.width() - 1.0, accentH);
        QPainterPath bp;
        bp.addRoundedRect(bottomRect, innerR, innerR);
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.accentDefault);
        painter.drawPath(bp);
    } else {
        // Normal border
        painter.setPen(QPen(borderColor, borderStroke.width));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(borderStroke.rect, outerR, outerR);

        // Bottom edge gradient (WinUI 3 ControlElevation): slightly darker at bottom
        if (enabled && !m_pressed) {
            const qreal accentH = 1.0;
            QRectF bottomRect(r.left() + 1, r.bottom() - accentH - 0.5, r.width() - 2, accentH);
            QPainterPath bp;
            bp.addRoundedRect(bottomRect, 1.0, 1.0);
            painter.setPen(Qt::NoPen);
            painter.setBrush(colors.strokeSecondary);
            painter.drawPath(bp);
        }
    }

    // ── Text ─────────────────────────────────────────────────────────────
    // Figma: text 14px, color rgba(0,0,0,0.9) → textPrimary
    QColor textColor = enabled ? colors.textPrimary : colors.textDisabled;

    // Chevron area calculation
    const int chevronAreaW = m_chevronOffset.x() + m_chevronSize + ::Spacing::Gap::Tight;
    const QRect logicalTextRect =
        rect().adjusted(m_contentPaddingH, m_contentPaddingV, -chevronAreaW, -m_contentPaddingV);
    const QRectF textSlot = QStyle::visualRect(layoutDirection(), rect(), logicalTextRect);

    // In editable mode, QLineEdit handles text display
    if (!m_observedLineEdit) {
        painter.setPen(textColor);
        painter.setFont(font());
        const QString text = currentText();
        const QFontMetricsF metrics(painter.font());
        const QString elidedText =
            metrics.elidedText(text, Qt::ElideRight, qRound(textSlot.width()));
        const QRectF textRect =
            fluent::painting::verticallyCenteredTextInkRect(textSlot, metrics, elidedText);
        painter.drawText(
            textRect, QStyle::visualAlignment(layoutDirection(), Qt::AlignLeft | Qt::AlignVCenter),
            elidedText);
    }

    // ── Chevron ──────────────────────────────────────────────────────────
    // Figma: 12 px symbol, color rgba(0,0,0,0.61) → textSecondary
    // Press fade/bounce must use sin(progress·π) like DropDownButton: the click
    // animation ends at progress=1 while the popup stays open — a linear fade
    // would leave the chevron at 50% alpha until the next cycle.
    // zh_CN: 按下淡化/下沉须与 DropDownButton 一样用 sin(progress·π)：点击动画结束在
    // progress=1 且 popup 仍打开，若线性淡化会把箭头一直留在 50% 透明度。
    QColor chevronColor = enabled ? colors.textSecondary : colors.textDisabled;
    const qreal pressEffect = qSin(m_pressProgress * M_PI);
    if (pressEffect > 0.0 && enabled) {
        const qreal alphaFactor = 1.0 - 0.5 * pressEffect;
        chevronColor.setAlphaF(chevronColor.alphaF() * alphaFactor);
    }

    // Shared trailing chip slot (hover + glyph), SplitButton-style AlignCenter.
    // zh_CN: 悬停底与字形共用尾缘芯片槽，按 SplitButton 方式 AlignCenter。
    const qreal maxBounce = 3.0;
    const qreal pressOffset = qRound(maxBounce * pressEffect);
    const qreal pad = 4.0;
    const qreal btnW = m_chevronSize + pad * 2;
    const qreal btnH = m_chevronSize + pad * 2;
    const QRect logicalChevronSlot(qRound(r.right() - m_chevronOffset.x() - m_chevronSize - pad),
                                   qRound(r.center().y() - btnH / 2.0), qRound(btnW), qRound(btnH));
    QRectF chevronSlot = QStyle::visualRect(layoutDirection(), rect(), logicalChevronSlot);
    chevronSlot.translate(0, pressOffset + m_chevronOffset.y());

    // Editable mode: draw chevron button hover/press background
    if (m_observedLineEdit && enabled) {
        QColor chevronBg = Qt::transparent;
        if (m_chevronHovered && m_pressed) {
            chevronBg = colors.subtleTertiary;
        } else if (m_chevronHovered) {
            chevronBg = colors.subtleSecondary;
        }
        if (chevronBg.alpha() > 0) {
            const qreal chipR = qMax<qreal>(0.0, radius.control - 1.0);
            QPainterPath bp;
            bp.addRoundedRect(chevronSlot, chipR, chipR);
            painter.setPen(Qt::NoPen);
            painter.setBrush(chevronBg);
            painter.drawPath(bp);
        }
    }

    painter.setPen(chevronColor);
    Typography::Icons::paintGlyph(painter, chevronSlot, m_chevronGlyph, m_chevronSize,
                                  Qt::AlignCenter);
}

} // namespace fluent::basicinput
