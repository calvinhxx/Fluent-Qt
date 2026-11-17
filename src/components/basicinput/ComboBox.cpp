#include "compatibility/InputEventCompat.h"
#include "ComboBox.h"
#include "private/ComboBoxAccessibility_p.h"
#include "components/foundation/private/FieldTrailingContent_p.h"
#include <QApplication>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPersistentModelIndex>
#include <QPropertyAnimation>
#include <QProxyStyle>
#include <QResizeEvent>
#include <QScrollBar>
#include <QScopedValueRollback>
#include <QStringListModel>
#include <QStyle>
#include <QTimer>
#include <QWheelEvent>
#include <QtMath>
#include "compatibility/TextPaintCompat.h"
#include "compatibility/private/ComboBoxCompat_p.h"
#include "compatibility/private/LineEditMetricsCompat_p.h"
#include "components/collections/ListView.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/foundation/overlay/OverlayPresentation_p.h"
#include "components/foundation/overlay/FieldPopupGeometry_p.h"
#include "components/foundation/overlay/FieldPopupSurface_p.h"
#include "components/foundation/private/TextInputFrame_p.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include "components/foundation/private/ControlMetrics_p.h"
#include "components/foundation/private/FieldTextGeometry_p.h"
#include "components/menus_toolbars/private/TextEditingMenu_p.h"
#include "components/scrolling/ScrollBar.h"
#include "components/textfields/LineEdit.h"
#include "design/Animation.h"
#include "design/CornerRadius.h"

namespace {
static constexpr int kPopupShadowMargin = ::Spacing::Standard;
static constexpr int kPopupContentInset = ::Spacing::XSmall;
static constexpr int kPopupWindowMargin = 4;
static constexpr int kPopupItemOuterInset = 0;
static constexpr int kPopupItemTextLeftInset = ::Spacing::Small;
static constexpr int kPopupItemTextRightInset = ::Spacing::Small;
static constexpr int kPopupItemVerticalPadding = ::Spacing::XSmall;
static constexpr int kClosedFieldTextFitClearance = ::Spacing::XSmall;

int popupRowHeight(const QFont& font)
{
    return qMax(::Spacing::ControlHeight::Standard, QFontMetrics(font).height() + 2 * kPopupItemVerticalPadding);
}

// Suppress QStyle's native panel since ComboBox paints its own frame.
class TransparentLineEditStyle : public QProxyStyle {
public:
    void drawPrimitive(PrimitiveElement pe, const QStyleOption* opt, QPainter* p, const QWidget* w = nullptr)
        const override
    {
        if (pe == PE_PanelLineEdit)
            return;
        QProxyStyle::drawPrimitive(pe, opt, p, w);
    }
};
} // namespace

namespace fluent::basicinput {

class ComboBox::ComboBoxPopup : public fluent::dialogs_flyouts::Flyout {
public:
    explicit ComboBoxPopup(ComboBox* comboBox);

    void showForComboBox();
    void refreshFont();
    void onThemeUpdated() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    QPoint computePosition() const override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    friend class ComboBox;
    void commitIndex(const QModelIndex& index);
    void updateLayout();
    ComboBox* m_comboBox;
    fluent::collections::ListView* m_listView;
    ComboBoxItemDelegate* m_delegate;
    bool m_geometryUpdatePending = false;
};

ComboBoxItemDelegate::ComboBoxItemDelegate(FluentElement* themeHost, QAbstractItemView* view, QObject* parent)
    : QStyledItemDelegate(parent), m_themeHost(themeHost), m_view(view)
{}

void ComboBoxItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
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
    if (m_view && m_view->verticalScrollBar()
        && m_view->verticalScrollBar()->maximum() > m_view->verticalScrollBar()->minimum()) {
        if (auto* listView = qobject_cast<fluent::collections::ListView*>(m_view)) {
            if (auto* scrollBar = listView->verticalFluentScrollBar()) {
                itemRightInset += scrollBar->thickness();
            }
        }
    }
    const QRect logicalBgRect = option.rect.adjusted(kPopupItemOuterInset, 1, -itemRightInset, -1);
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

    const int markerWidth = Typography::IconSize::Compact + ::Spacing::Small;
    const QRect logicalTextRect = logicalBgRect.adjusted(kPopupItemTextLeftInset, 0,
        -kPopupItemTextRightInset - markerWidth, 0);
    const QRectF textSlot = QStyle::visualRect(option.direction, option.rect, logicalTextRect);
    painter->setPen(textColor);
    painter->setFont(option.font);
    const QString text = index.data(Qt::DisplayRole).toString();
    const QFontMetricsF metrics(painter->font());
    const QString elidedText = metrics.elidedText(text, Qt::ElideRight, qRound(textSlot.width()));
    fluent::painting::drawVerticallyCenteredText(*painter, textSlot, elidedText,
        QStyle::visualAlignment(option.direction, Qt::AlignLeft));

    if (isSelected) {
        const QRect logicalMarker(logicalBgRect.right() + 1 - kPopupItemTextRightInset - Typography::IconSize::Compact,
            logicalBgRect.top(), Typography::IconSize::Compact, logicalBgRect.height());
        painter->setPen(isEnabled ? colors.accentDefault : colors.textDisabled);
        Typography::Icons::paintGlyph(*painter, QStyle::visualRect(option.direction, option.rect, logicalMarker),
            Typography::Icons::CheckMark, Typography::IconSize::Compact, Qt::AlignCenter);
    }

    painter->restore();
}

QSize ComboBoxItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const
{
    return QSize(0, popupRowHeight(option.font));
}

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
    m_listView->setAccessibleName(tr("Options"));
    m_listView->setBorderVisible(false);
    // Let ComboBoxPopup draw the single card surface; ListView's smaller radius would poke through 8px corners.
    m_listView->setBackgroundVisible(false);
    // Opt out of Source-mode viewport clear so transparent ListView keeps the popup card background on Mica windows.
    m_listView->setProperty("fluentPreserveParentSurface", true);
    if (m_listView->viewport())
        m_listView->viewport()->setProperty("fluentPreserveParentSurface", true);
    m_listView->setSelectionMode(fluent::collections::ListView::SelectionMode::Single);
    m_listView->setSelectionIndicatorVisible(false);
    m_listView->setSpacing(0);

    m_delegate = new ComboBoxItemDelegate(comboBox, m_listView, this);
    m_listView->setItemDelegate(m_delegate);
    m_listView->setFont(comboBox->font());
    m_listView->installEventFilter(this);

    m_listView->setMouseTracking(true);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    connect(m_listView, &QAbstractItemView::clicked, this, &ComboBoxPopup::commitIndex);

    connect(this, &ComboBoxPopup::closed, this, [this]() {
        if (m_comboBox)
            m_comboBox->onPopupHidden();
    });

    onThemeUpdated();
}

void ComboBox::ComboBoxPopup::showForComboBox()
{
    QPointer<ComboBoxPopup> guard(this);
    m_listView->setModel(m_comboBox->model());
    m_listView->setRootIndex(m_comboBox->rootModelIndex());
    m_listView->setModelColumn(m_comboBox->modelColumn());
    m_listView->setFont(m_comboBox->font());

    const QModelIndex current = m_comboBox->model()->index(m_comboBox->currentIndex(), m_comboBox->modelColumn(),
        m_comboBox->rootModelIndex());
    m_listView->selectionModel()->setCurrentIndex(current,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    updateLayout();
    if (isOpen() || isVisible()) {
        show();
        raise();
    } else {
        showAt(m_comboBox);
    }
    if (!guard || !isOpen())
        return;

    // Hand keyboard navigation to the list while keeping pending row distinct from committed value.
    m_listView->setFocus(Qt::PopupFocusReason);
    if (!guard)
        return;

    if (m_comboBox->currentIndex() >= 0) {
        m_listView->scrollTo(
            m_listView->model()->index(m_comboBox->currentIndex(), m_comboBox->modelColumn(),
                m_comboBox->rootModelIndex()),
            QAbstractItemView::PositionAtCenter);
    } else {
        // Do not keep a stale selected row when currentIndex is -1.
        m_listView->selectionModel()->clear();
    }
}

void ComboBox::ComboBoxPopup::commitIndex(const QModelIndex& index)
{
    if (!isOpen() || !m_comboBox || !m_comboBox->m_popupVisible || !m_comboBox->isEnabled() || !index.isValid()
        || index.model() != m_comboBox->model() || index.parent() != m_comboBox->rootModelIndex()
        || index.column() != m_comboBox->modelColumn() || !index.flags().testFlag(Qt::ItemIsEnabled)
        || !index.flags().testFlag(Qt::ItemIsSelectable)) {
        return;
    }

    QPointer<ComboBox> comboBox(m_comboBox);
    const QPersistentModelIndex selected(index);
    // Clear m_popupVisible before Flyout closes so reentrant commits are rejected while closing.
    comboBox->hidePopup();
    if (!comboBox || !comboBox->isEnabled() || !selected.isValid() || selected.model() != comboBox->model()
        || selected.parent() != comboBox->rootModelIndex() || selected.column() != comboBox->modelColumn()
        || !selected.flags().testFlag(Qt::ItemIsEnabled) || !selected.flags().testFlag(Qt::ItemIsSelectable)) {
        return;
    }

    const int row = selected.row();
    const QString text = comboBox->itemText(row);
    comboBox->setCurrentIndex(row);
    if (!comboBox)
        return;
    emit comboBox->activated(row);
    if (!comboBox)
        return;
    ::compatibility::detail::emitComboBoxTextActivated(comboBox.data(), text);
}

void ComboBox::ComboBoxPopup::refreshFont()
{
    m_listView->setFont(m_comboBox->font());
    if (isOpen())
        updateLayout();
    m_listView->viewport()->update();
}

void ComboBox::ComboBoxPopup::updateLayout()
{
    setLayoutDirection(m_comboBox->layoutDirection());
    const int itemCount = m_comboBox->count();
    const int itemH = popupRowHeight(m_listView->font());
    const int maxVisible = qMin(itemCount, 6);
    int rowsH = maxVisible * itemH;
    const int sSize = kPopupShadowMargin;
    const int cardInset = kPopupContentInset;
    QWidget* top = ::fluent::overlay::presentedTopLevel(m_comboBox);
    const QRect surface = ::fluent::overlay::overlaySurfaceRect(top);
    const QRect anchor = ::fluent::overlay::presentedRectInTopLevel(m_comboBox);
    const auto space = ::fluent::overlay::fieldPopupSpace(anchor, surface, m_comboBox->popupOffset(),
        kPopupWindowMargin);
    rowsH = qMin(rowsH, qMax(1, space.capacity(rowsH + 2 * cardInset) - 2 * cardInset));
    int widestText = 0;
    const QFontMetricsF popupMetrics(m_listView->font());
    for (int index = 0; index < itemCount; ++index)
        widestText = qMax(widestText, qCeil(popupMetrics.horizontalAdvance(m_comboBox->itemText(index))) + 2);

    int scrollClearance = 0;
    if (itemCount > rowsH / itemH) {
        scrollClearance = ::Spacing::XSmall;
        if (auto* scrollBar = m_listView->verticalFluentScrollBar())
            scrollClearance = qMax(scrollClearance, scrollBar->thickness());
    }
    const int textChrome = cardInset * 2 + kPopupItemOuterInset * 2 + kPopupItemTextLeftInset + kPopupItemTextRightInset
        + scrollClearance + Typography::IconSize::Compact + ::Spacing::Small;
    const int cardW = space.boundedWidth(qMax(qMax(m_comboBox->width(), 120), widestText + textChrome));
    const int cardH = rowsH + cardInset * 2;
    const QSize totalSize = ::fluent::overlay::outerSizeForVisibleCard(QSize(cardW, cardH), sSize);

    setFixedSize(totalSize);
    setAnchorOffset(m_comboBox->m_popupOffset);
    setAnchor(m_comboBox);

    const QRect cardRect = ::fluent::overlay::visibleCardRect(rect(), sSize);
    m_listView->setGeometry(cardRect.adjusted(cardInset, cardInset, -cardInset, -cardInset));
    m_listView->clearMask();
    m_listView->doItemsLayout();
    m_listView->refreshFluentScrollChrome();
    if (isOpen() || isVisible())
        move(computePosition());
}

void ComboBox::ComboBoxPopup::onThemeUpdated()
{
    Flyout::onThemeUpdated();
    QPalette pal = palette();
    pal.setColor(QPalette::Window, themeColorsRef().bgLayer);
    setPalette(pal);

    if (m_comboBox) {
        refreshFont();
    }
    if (m_listView && m_listView->viewport())
        m_listView->viewport()->update();
}

void ComboBox::ComboBoxPopup::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    ::fluent::overlay::paintFieldPopupSurface(painter, rect(), *this);
}

QPoint ComboBox::ComboBoxPopup::computePosition() const
{
    if (!::fluent::overlay::presentedTopLevel(m_comboBox))
        return Flyout::computePosition();

    QWidget* top = ::fluent::overlay::presentedTopLevel(m_comboBox);
    const int shadow = kPopupShadowMargin;
    const QSize cardSize = ::fluent::overlay::visibleCardSize(size(), shadow);
    const int cardH = cardSize.height();
    const QRect anchor = ::fluent::overlay::presentedRectInTopLevel(m_comboBox);

    const QRect surface = ::fluent::overlay::overlaySurfaceRect(top);
    const auto space = ::fluent::overlay::fieldPopupSpace(anchor, surface, anchorOffset(), kPopupWindowMargin);
    const QPoint cardTopLeft = ::fluent::overlay::fieldPopupCardTopLeft(anchor, cardSize, surface,
        m_comboBox->layoutDirection(), space.opensAbove(cardH), anchorOffset(), kPopupWindowMargin, clampToWindow());

    return ::fluent::overlay::outerTopLeftForVisibleCard(cardTopLeft, shadow);
}

bool ComboBox::ComboBoxPopup::eventFilter(QObject* watched, QEvent* event)
{
    if (isOpen() && !m_geometryUpdatePending
        && ::fluent::overlay::fieldPopupAnchorGeometryMayChange(watched, event, m_comboBox, this)) {
        m_geometryUpdatePending = true;
        QTimer::singleShot(0, this, [this] {
            m_geometryUpdatePending = false;
            if (isOpen())
                updateLayout();
        });
    }
    if (watched == m_listView && event->type() == QEvent::KeyPress && isOpen()) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            keyEvent->accept();
            commitIndex(m_listView->currentIndex());
            return true;
        }
    }
    // ListView tracks theme updates separately; keep the owning ComboBox's font regardless of broadcast order.
    if (watched == m_listView && event->type() == QEvent::FontChange && m_listView->font() != m_comboBox->font()) {
        m_listView->setFont(m_comboBox->font());
    }
    if (event && event->type() == QEvent::MouseButtonPress && m_comboBox) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const QPoint comboLocal = ::fluent::overlay::localPointFromPresentedGlobal(m_comboBox,
            fluentMouseGlobalPos(mouseEvent));
        const bool pressOnOwner = m_comboBox->rect().contains(comboLocal);
        const bool pressInsidePopup = ::fluent::overlay::visibleCardContains(rect(),
            mapFromGlobal(fluentMouseGlobalPos(mouseEvent)), kPopupShadowMargin);
        if (pressOnOwner && !pressInsidePopup) {
            m_comboBox->m_ignoreNextPopupPress = true;
        }
    }

    return Flyout::eventFilter(watched, event);
}

ComboBox::ComboBox(QWidget* parent) : QComboBox(parent)
{
    detail::ensureComboBoxAccessibilityFactory();
    setAttribute(Qt::WA_LayoutUsesWidgetRect);
    setAttribute(Qt::WA_Hover);
    // Use StrongFocus instead of WheelFocus so hovering and scrolling doesn't steal focus and switch items.
    setFocusPolicy(Qt::StrongFocus);
    m_trailingContent = std::make_unique<::fluent::detail::FieldTrailingContent>(this);
    connect(m_trailingContent.get(), &::fluent::detail::FieldTrailingContent::geometryChanged, this,
        &ComboBox::synchronizeFont);
    connect(m_trailingContent.get(), &::fluent::detail::FieldTrailingContent::textChanged, this,
        &ComboBox::trailingTextChanged);
    connect(m_trailingContent.get(), &::fluent::detail::FieldTrailingContent::widgetChanged, this,
        &ComboBox::trailingWidgetChanged);
    connect(m_trailingContent.get(), &::fluent::detail::FieldTrailingContent::ownershipChanged, this,
        &ComboBox::trailingWidgetOwnershipChanged);
    connect(this, &ComboBox::trailingWidgetChanged, this, [this] { detail::notifyComboBoxContentChanged(this); });
    connect(this, &ComboBox::trailingTextChanged, this, [this] { detail::notifyComboBoxContentChanged(this); });
    applyFontRole();
    setFixedHeight(::Spacing::ControlHeight::Standard);

    initAnimation();
    onThemeUpdated();
}

ComboBox::~ComboBox()
{
    delete m_popup.data();
}

QWidget* ComboBox::accessiblePopupWidget() const
{
    return m_popup.data();
}

QString ComboBox::trailingText() const
{
    return m_trailingContent->text();
}
void ComboBox::setTrailingText(const QString& text)
{
    m_trailingContent->setText(text);
}
QWidget* ComboBox::trailingWidget() const
{
    return m_trailingContent->widget();
}
WidgetOwnership ComboBox::trailingWidgetOwnership() const
{
    return m_trailingContent->ownership();
}
bool ComboBox::setTrailingWidget(QWidget* widget, WidgetOwnership ownership)
{
    if (widget && widget == lineEdit())
        return false;
    return m_trailingContent->setWidget(widget, ownership);
}
QWidget* ComboBox::takeTrailingWidget()
{
    return m_trailingContent->takeWidget();
}
void ComboBox::releaseTrailingWidget()
{
    m_trailingContent->releaseWidget();
}

void ComboBox::initAnimation()
{
    m_pressAnimation = new QPropertyAnimation(this, "pressProgress", this);
    m_pressAnimation->setDuration(themeAnimation().slow);
    m_pressAnimation->setEasingCurve(themeAnimation().decelerate);
}

void ComboBox::setFontRole(Typography::FontRole role)
{
    const bool roleChanged = m_fontRole != role;
    if (!roleChanged && !m_hasExplicitFont)
        return;
    m_fontRole = role;
    m_hasExplicitFont = false;
    applyFontRole();
    synchronizeFont();
    if (roleChanged)
        emit fontRoleChanged();
}

void ComboBox::setFont(const QFont& font)
{
    m_hasExplicitFont = true;
    QComboBox::setFont(font);
}

void ComboBox::applyFontRole()
{
    const QScopedValueRollback<bool> applying(m_applyingFontRole, true);
    QComboBox::setFont(themeFont(m_fontRole).toQFont());
}

void ComboBox::changeEvent(QEvent* event)
{
    QComboBox::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        if (!m_applyingFontRole)
            m_hasExplicitFont = true;
        synchronizeFont();
    }
}

void ComboBox::synchronizeFont()
{
    // Only auto-adjust height when the caller hasn't overridden fixed/min/max height.
    if (minimumHeight() == m_autoHeight && maximumHeight() == m_autoHeight) {
        m_autoHeight = sizeHint().height();
        setFixedHeight(m_autoHeight);
    }
    applyLineEditStyle();
    layoutLineEdit();
    if (m_popup)
        m_popup->refreshFont();
    updateGeometry();
    update();
}

void ComboBox::setContentPaddingH(int px)
{
    if (m_contentPaddingH == px)
        return;
    m_contentPaddingH = px;
    layoutLineEdit();
    updateGeometry();
    emit layoutChanged();
    update();
}

void ComboBox::setContentPaddingV(int px)
{
    if (m_contentPaddingV == px)
        return;
    m_contentPaddingV = px;
    synchronizeFont();
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
    layoutLineEdit();
    updateGeometry();
    emit chevronChanged();
    update();
}

void ComboBox::setChevronOffset(const QPoint& offset)
{
    if (m_chevronOffset == offset)
        return;
    m_chevronOffset = offset;
    layoutLineEdit();
    updateGeometry();
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
    if (!m_hasExplicitFont)
        applyFontRole();
    synchronizeFont();
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
    int maxTextW = 80;
    for (int i = 0; i < count(); ++i) {
        int w = fm.horizontalAdvance(itemText(i));
        maxTextW = qMax(maxTextW, w);
    }
    const int chevronArea = chevronAreaWidth();
    // Leave small clearance past horizontalAdvance() so integer layout/raster rounding won't clip/elide.
    const QSize trailing = m_trailingContent ? m_trailingContent->sizeHint() : QSize(0, 0);
    const int w = m_contentPaddingH + maxTextW + kClosedFieldTextFitClearance + chevronArea
        + (trailing.width() > 0 ? trailing.width() + ::Spacing::XSmall : 0);
    const int h = qMax(trailing.height() + 2 * m_contentPaddingV,
        qMax(::fluent::detail::standardControlHeight(fm),
            qMax(sp.controlHeight.standard, fm.height() + 2 * m_contentPaddingV)));
    return QSize(w, h);
}

QSize ComboBox::minimumSizeHint() const
{
    QSize hint = QComboBox::minimumSizeHint();
    if (m_trailingContent && m_trailingContent->sizeHint().width() > 0) {
        const int width =
            trailingWidget() ? m_trailingContent->sizeHint().width() : qMin(24, m_trailingContent->sizeHint().width());
        hint.setWidth(qMax(hint.width(),
            m_contentPaddingH + chevronAreaWidth() + width + ::Spacing::XSmall
                + fontMetrics().horizontalAdvance(QLatin1Char('M')) * 4));
        hint.setHeight(qMax(hint.height(), sizeHint().height()));
    }
    return hint;
}

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
        if (!placeholderText().isEmpty())
            editor->setPlaceholderText(placeholderText());
        auto* style = new TransparentLineEditStyle();
        style->setParent(editor);
        editor->setStyle(style);
        editor->setFocusPolicy(Qt::ClickFocus);
        const Qt::ContextMenuPolicy contextMenuPolicy = editor->contextMenuPolicy();
        QComboBox::setLineEdit(editor);
        // Restore editor's policy since QComboBox::setLineEdit forces NoContextMenu.
        editor->setContextMenuPolicy(contextMenuPolicy);
        m_editorMutationInProgress = false;
        synchronizeLineEdit();
        setMouseTracking(true);
        applyLineEditStyle();
        layoutLineEdit();
        editor->show();
        connect(editor, &fluent::textfields::LineEdit::returnPressed, this, [this]() { hidePopup(); });
    } else {
        const QPointer<QObject> previousEditor = m_observedLineEdit;
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

QLineEdit* ComboBox::observedLineEdit() const
{
    // Qt can deliver teardown events after ~QLineEdit() while QObject is still valid.
    return m_editorMutationInProgress ? nullptr : qobject_cast<QLineEdit*>(m_observedLineEdit.data());
}

void ComboBox::layoutLineEdit()
{
    if (m_editorLayoutInProgress)
        return;
    const QPointer<ComboBox> guard(this);
    m_editorLayoutInProgress = true;
    synchronizeLineEdit();
    const int chevronAreaW = chevronAreaWidth();
    if (m_trailingContent) {
        m_trailingContent->refreshFont();
        const QRect available = ::fluent::detail::fieldTextRect(rect(), layoutDirection(),
            QMargins(m_contentPaddingH, m_contentPaddingV, chevronAreaW, m_contentPaddingV));
        const int reserved = m_trailingContent->layout(available, layoutDirection(),
            fontMetrics().horizontalAdvance(QLatin1Char('M')) * 4);
        if (!guard)
            return;
        m_trailingContentWidth = reserved;
    }
    QLineEdit* editor = observedLineEdit();
    if (!editor) {
        m_editorLayoutInProgress = false;
        return;
    }
    const QRect textRect = ::fluent::detail::fieldTextRect(rect(), layoutDirection(),
        QMargins(m_contentPaddingH, m_contentPaddingV, chevronAreaW + m_trailingContentWidth, m_contentPaddingV));
    // Clamp the trailing edge to textRect so the editor never overlaps the chevron hit area.
    QRect geometry = compatibility::detail::lineEditGeometryForTextRect(*editor, textRect);
    if (layoutDirection() == Qt::RightToLeft)
        geometry.setLeft(qMax(geometry.left(), textRect.left()));
    else
        geometry.setRight(qMin(geometry.right(), textRect.right()));
    if (editor->geometry() != geometry)
        editor->setGeometry(geometry);
    if (guard)
        m_editorLayoutInProgress = false;
}

void ComboBox::applyLineEditStyle()
{
    synchronizeLineEdit();
    QLineEdit* editor = observedLineEdit();
    if (!editor)
        return;
    if (auto* fluentEditor = qobject_cast<fluent::textfields::LineEdit*>(editor)) {
        fluentEditor->setFontRole(m_fontRole);
        fluentEditor->onThemeUpdated();
    }
    editor->setFont(font());
}

bool ComboBox::event(QEvent* event)
{
    const QPointer<ComboBox> guard(this);
    const bool handled = QComboBox::event(event);
    if (!guard)
        return handled;
    synchronizeLineEdit();
    if (event && event->type() == QEvent::LayoutDirectionChange) {
        layoutLineEdit();
        if (m_popup && m_popup->isOpen())
            m_popup->updateLayout();
        update();
    }
    if (event && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut))
        update();
    return handled;
}

void ComboBox::resizeEvent(QResizeEvent* event)
{
    QComboBox::resizeEvent(event);
    layoutLineEdit();
}

void ComboBox::showPopup()
{
    if (!isEnabled() || count() == 0 || (m_popupVisible && m_popup))
        return;
    m_popupVisible = true;

    if (!m_popup)
        m_popup = new ComboBoxPopup(this);

    QPointer<ComboBox> guard(this);
    m_popup->showForComboBox();
    if (guard) {
        update();
        detail::notifyComboBoxPopupChanged(this);
    }
}

void ComboBox::hidePopup()
{
    if (!m_popupVisible)
        return;
    m_popupVisible = false;
    m_pressed = false;

    QPointer<ComboBox> guard(this);
    if (m_popup)
        m_popup->close();
    if (!guard)
        return;

    update();
    QComboBox::hidePopup();
    if (guard)
        detail::notifyComboBoxPopupChanged(this);
}

void ComboBox::onPopupHidden()
{
    const bool needsUpdate = m_popupVisible || m_pressed;
    m_popupVisible = false;
    m_pressed = false;
    if (needsUpdate) {
        update();
        detail::notifyComboBoxPopupChanged(this);
    }
}

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
    auto* editor = observedLineEdit();
    const bool ownsFocus = hasFocus() || (editor && editor->hasFocus());
    if (!ownsFocus) {
        event->ignore();
        return;
    }
    QComboBox::wheelEvent(event);
}

void ComboBox::keyPressEvent(QKeyEvent* event)
{
    synchronizeLineEdit();
    auto* editor = observedLineEdit();
    const bool ownsFocus = hasFocus() || (editor && editor->hasFocus());
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
        m_pressAnimation->stop();
        m_pressAnimation->setStartValue(0.0);
        m_pressAnimation->setEndValue(1.0);
        ::fluent::detail::startMotionTransition(m_pressAnimation, themeAnimation().slow);

        // Manage popup directly because QComboBox's built-in popup conflicts with ComboBoxPopup.
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
        const bool over = chevronButtonRect().contains(event->pos());
        if (over != m_chevronHovered) {
            m_chevronHovered = over;
            update();
        }
    }
    QComboBox::mouseMoveEvent(event);
}

bool ComboBox::eventFilter(QObject* watched, QEvent* event)
{
    if (auto* editor = watched == m_observedLineEdit ? observedLineEdit() : nullptr) {
        if ((event->type() == QEvent::Move || event->type() == QEvent::Resize) && !m_editorLayoutInProgress) {
            // QComboBox resets editor geometry on selection/model updates; reapply ours and drop the stale event.
            const QPointer<ComboBox> guard(this);
            const QPointer<QLineEdit> editorGuard(editor);
            layoutLineEdit();
            if (!guard || !editorGuard)
                return true;
            // Discard stale Move/Resize if layoutLineEdit() already moved/resized the editor.
            if ((event->type() == QEvent::Move && static_cast<QMoveEvent*>(event)->pos() != editorGuard->pos())
                || (event->type() == QEvent::Resize
                    && static_cast<QResizeEvent*>(event)->size() != editorGuard->size()))
                return true;
        } else if (event->type() == QEvent::FontChange && editor->font() != font()) {
            // Embedded Fluent LineEdit also updates on theme changes; keep ComboBox's font.
            editor->setFont(font());
        } else if (event->type() == QEvent::StyleChange || event->type() == QEvent::ContentsRectChange) {
            layoutLineEdit();
        } else if (event->type() == QEvent::FocusIn) {
            editor->selectAll();
            update();
        } else if (event->type() == QEvent::FocusOut) {
            update();
        } else if (event->type() == QEvent::ContextMenu && !qobject_cast<fluent::textfields::LineEdit*>(editor)
            && editor->contextMenuPolicy() == Qt::DefaultContextMenu) {
            auto* contextEvent = static_cast<QContextMenuEvent*>(event);
            if (fluent::menus_toolbars::detail::showTextEditingContextMenu(editor, editor->createStandardContextMenu(),
                    contextEvent->globalPos(), QStringLiteral("FluentComboBox.LineEdit.ContextMenu"))) {
                contextEvent->accept();
                return true;
            }

            // Leave unhandled if the shared menu is unavailable so native context menu runs.
            contextEvent->ignore();
        }
    }
    return QComboBox::eventFilter(watched, event);
}

int ComboBox::chevronAreaWidth() const
{
    const int slot = qMax(::Spacing::ControlHeight::Small, m_chevronSize + ::Spacing::Small);
    return m_chevronOffset.x() + m_chevronSize / 2 + slot / 2 + ::Spacing::Gap::Tight;
}

QRect ComboBox::chevronButtonRect() const
{
    const int slot = qMax(::Spacing::ControlHeight::Small, m_chevronSize + ::Spacing::Small);
    const QRect logical(width() - chevronAreaWidth() + ::Spacing::Gap::Tight,
        (height() - slot) / 2 + m_chevronOffset.y(), slot, slot);
    return QStyle::visualRect(layoutDirection(), rect(), logical);
}

void ComboBox::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto& colors = themeColorsRef();
    const auto& radius = themeRadius();

    const bool enabled = isEnabled();
    const QRectF r(rect());
    synchronizeLineEdit();
    auto* editor = observedLineEdit();
    ::fluent::detail::TextInputFrameState state;
    state.enabled = enabled;
    state.readOnly = editor && editor->isReadOnly();
    state.focused = hasFocus() || (editor && editor->hasFocus()) || m_popupVisible;
    state.hovered = m_hovered;
    state.pressed = m_pressed;
    ::fluent::detail::paintTextInputFrame(painter, r, colors, radius.control, state);

    QColor textColor = enabled ? colors.textPrimary : colors.textDisabled;

    const int chevronAreaW = chevronAreaWidth();
    const QRectF textSlot = ::fluent::detail::fieldTextRect(rect(), layoutDirection(),
        QMargins(m_contentPaddingH, m_contentPaddingV, chevronAreaW + m_trailingContentWidth, m_contentPaddingV));

    if (!editor) {
        const QString text = currentText();
        const bool showingPlaceholder = text.isEmpty() && !placeholderText().isEmpty();
        const QString displayText = showingPlaceholder ? placeholderText() : text;
        const QColor placeholderColor = enabled ? colors.textSecondary : colors.textDisabled;
        painter.setPen(showingPlaceholder ? placeholderColor : textColor);
        painter.setFont(font());
        const QFontMetricsF metrics(painter.font());
        const QString elidedText = metrics.elidedText(displayText, Qt::ElideRight, qRound(textSlot.width()));
        fluent::painting::drawVerticallyCenteredText(painter, textSlot, elidedText,
            QStyle::visualAlignment(layoutDirection(), Qt::AlignLeft));
    }

    // Use sin(progress*pi) so chevron opacity recovers to 1.0 even while the popup remains open at progress=1.
    QColor chevronColor = enabled ? colors.textSecondary : colors.textDisabled;
    const qreal pressEffect = qSin(m_pressProgress * M_PI);
    if (pressEffect > 0.0 && enabled) {
        const qreal alphaFactor = 1.0 - 0.5 * pressEffect;
        chevronColor.setAlphaF(chevronColor.alphaF() * alphaFactor);
    }

    const qreal maxBounce = 3.0;
    const qreal pressOffset = qRound(maxBounce * pressEffect);
    QRectF chevronSlot = chevronButtonRect();
    chevronSlot.translate(0, pressOffset);

    if (editor && enabled) {
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
    Typography::Icons::paintGlyph(painter, chevronSlot, m_chevronGlyph, m_chevronSize, Qt::AlignCenter);
}

} // namespace fluent::basicinput
