#include "DataGrid.h"

#include "private/DataGridAccessibility_p.h"

#include <QAbstractItemModel>
#include <QAccessible>
#include <QApplication>
#include <QBrush>
#include <QCursor>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPointer>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QStyleOptionHeader>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QWheelEvent>

#include "components/collections/CollectionViewBackdrop_p.h"
#include "components/scrolling/OverlayScrollChrome.h"
#include "components/scrolling/ScrollBar.h"
#include "compatibility/QtCompat.h"
#include "design/CornerRadius.h"
#include "design/Spacing.h"

namespace fluent::collections {

namespace {

constexpr int kScrollBarInset = ::Spacing::XSmall / 2;
constexpr int kHeaderHeight = ::Spacing::ControlHeight::Standard
    + ::Spacing::XSmall;
constexpr int kRowHeight = ::Spacing::ControlHeight::Standard
    + ::Spacing::XSmall;
constexpr int kCellHorizontalInset = ::Spacing::Medium;

void notifyAccessibleModelReset(DataGrid* grid)
{
#if QT_CONFIG(accessibility)
    if (!grid)
        return;

    QAccessibleTableModelChangeEvent event(
        grid, QAccessibleTableModelChangeEvent::ModelReset);
    if (QAccessibleInterface* interface =
            QAccessible::queryAccessibleInterface(grid)) {
        if (QAccessibleTableInterface* table = interface->tableInterface())
            table->modelChange(&event);
    }
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(grid);
#endif
}

} // namespace

namespace detail {

class DataGridHeaderView final : public QHeaderView {
public:
    DataGridHeaderView(Qt::Orientation orientation, DataGrid* grid)
        : QHeaderView(orientation, grid)
        , m_grid(grid)
    {
        setMouseTracking(true);
        viewport()->setMouseTracking(true);
        viewport()->setAutoFillBackground(false);
        setHighlightSections(false);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        if (!m_grid)
            return QHeaderView::paintEvent(event);

        QPainter background(viewport());
        background.fillRect(viewport()->rect(),
                            m_grid->themeColorsRef().bgLayerAlt);
        background.end();
        QHeaderView::paintEvent(event);
    }

    void paintSection(QPainter* painter, const QRect& rect,
                      int logicalIndex) const override
    {
        if (!painter || !rect.isValid() || !m_grid || !model())
            return;

        const auto& colors = m_grid->themeColorsRef();
        const QPoint pointer = viewport()->mapFromGlobal(QCursor::pos());
        const bool hovered = isEnabled() && underMouse()
            && rect.contains(pointer);
        const bool pressed = hovered
            && QApplication::mouseButtons().testFlag(Qt::LeftButton);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->fillRect(rect, colors.bgLayerAlt);
        if (pressed)
            painter->fillRect(rect, colors.subtleTertiary);
        else if (hovered)
            painter->fillRect(rect, colors.subtleSecondary);

        const bool horizontal = orientation() == Qt::Horizontal;
        painter->setPen(QPen(colors.strokeDivider, 1.0));
        if (horizontal) {
            painter->drawLine(rect.bottomLeft(), rect.bottomRight());
            if (logicalIndex != count() - 1) {
                painter->drawLine(rect.right(), rect.top() + 10,
                                  rect.right(), rect.bottom() - 10);
            }
        } else {
            painter->drawLine(rect.topRight(), rect.bottomRight());
            painter->drawLine(rect.bottomLeft(), rect.bottomRight());
        }

        QColor foreground = isEnabled()
            ? colors.textSecondary : colors.textDisabled;
        const QVariant foregroundRole = model()->headerData(
            logicalIndex, orientation(), Qt::ForegroundRole);
        if (foregroundRole.canConvert<QBrush>()) {
            const QBrush brush = qvariant_cast<QBrush>(foregroundRole);
            if (brush.style() != Qt::NoBrush)
                foreground = brush.color();
        }
        painter->setPen(foreground);

        QFont headerFont = font();
        const QVariant fontRole = model()->headerData(
            logicalIndex, orientation(), Qt::FontRole);
        if (fontRole.canConvert<QFont>())
            headerFont = qvariant_cast<QFont>(fontRole);
        painter->setFont(headerFont);

        Qt::Alignment alignment = horizontal
            ? Qt::AlignLeft | Qt::AlignVCenter
            : Qt::AlignCenter;
        const QVariant alignmentRole = model()->headerData(
            logicalIndex, orientation(), Qt::TextAlignmentRole);
        if (alignmentRole.isValid())
            alignment = static_cast<Qt::Alignment>(alignmentRole.toInt());

        const bool sorted = horizontal && isSortIndicatorShown()
            && sortIndicatorSection() == logicalIndex;
        QRect textRect = rect.adjusted(
            horizontal ? kCellHorizontalInset : ::Spacing::XSmall,
            0,
            horizontal ? -(sorted ? ::Spacing::XLarge
                                   : kCellHorizontalInset)
                       : -::Spacing::XSmall,
            0);
        const QString text = model()->headerData(
            logicalIndex, orientation(), Qt::DisplayRole).toString();
        const QFontMetrics metrics(headerFont);
        painter->drawText(
            textRect,
            alignment,
            metrics.elidedText(text, Qt::ElideRight, textRect.width()));

        if (sorted) {
            const bool rtl = layoutDirection() == Qt::RightToLeft;
            const qreal centerX = rtl
                ? rect.left() + ::Spacing::Medium
                : rect.right() - ::Spacing::Medium;
            const qreal centerY = rect.center().y() + 0.5;
            const qreal direction = sortIndicatorOrder() == Qt::AscendingOrder
                ? -1.0 : 1.0;
            QPainterPath chevron;
            chevron.moveTo(centerX - 3.5, centerY - direction * 1.5);
            chevron.lineTo(centerX, centerY + direction * 2.0);
            chevron.lineTo(centerX + 3.5, centerY - direction * 1.5);
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(foreground, 1.5,
                                 Qt::SolidLine, Qt::RoundCap,
                                 Qt::RoundJoin));
            painter->drawPath(chevron);
        }
        painter->restore();
    }

private:
    DataGrid* m_grid = nullptr;
};

class DataGridFrameOverlay final : public QWidget {
public:
    explicit DataGridFrameOverlay(DataGrid* grid)
        : QWidget(grid)
        , m_grid(grid)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setAutoFillBackground(false);
        setFocusPolicy(Qt::NoFocus);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!m_grid || !m_grid->isBorderVisible())
            return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(m_grid->themeColorsRef().strokeCard, 1.0));
        painter.drawRoundedRect(
            QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
            m_grid->themeRadius().control,
            m_grid->themeRadius().control);
    }

private:
    DataGrid* m_grid = nullptr;
};

class DataGridCellDelegate final : public QStyledItemDelegate {
public:
    explicit DataGridCellDelegate(DataGrid* grid)
        : QStyledItemDelegate(grid)
        , m_grid(grid)
    {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        if (!painter || !m_grid || !index.isValid())
            return;

        const auto& colors = m_grid->themeColorsRef();
        const bool enabled = option.state & QStyle::State_Enabled;
        const bool selected = option.state & QStyle::State_Selected;
        const bool rowSelected = m_grid->selectionModel()
            && m_grid->selectionBehavior() == QAbstractItemView::SelectRows
            && m_grid->selectionModel()->isRowSelected(
                index.row(), index.parent());
        const bool hovered = enabled && m_grid->m_hoveredRow == index.row();
        const bool current = index == m_grid->currentIndex();

        QColor background = m_grid->isBackgroundVisible()
            ? colors.bgLayer : Qt::transparent;
        const QVariant backgroundRole = index.data(Qt::BackgroundRole);
        if (backgroundRole.canConvert<QBrush>()) {
            const QBrush brush = qvariant_cast<QBrush>(backgroundRole);
            if (brush.style() != Qt::NoBrush)
                background = brush.color();
        }
        QColor foreground = enabled ? colors.textPrimary : colors.textDisabled;
        const QVariant foregroundRole = index.data(Qt::ForegroundRole);
        if (foregroundRole.canConvert<QBrush>()) {
            const QBrush brush = qvariant_cast<QBrush>(foregroundRole);
            if (brush.style() != Qt::NoBrush)
                foreground = brush.color();
        }

        painter->save();
        painter->fillRect(option.rect, background);
        if (enabled && (selected || rowSelected))
            painter->fillRect(option.rect, colors.subtleSecondary);
        else if (hovered)
            painter->fillRect(option.rect, colors.subtleSecondary);

        painter->setPen(QPen(colors.strokeDivider, 1.0));
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());

        QStyleOptionViewItem content = option;
        initStyleOption(&content, index);
        content.state &= ~(QStyle::State_Selected
                           | QStyle::State_MouseOver
                           | QStyle::State_HasFocus);
        content.backgroundBrush = Qt::NoBrush;
        content.palette.setColor(QPalette::Text, foreground);
        content.palette.setColor(QPalette::WindowText, foreground);
        content.rect.adjust(kCellHorizontalInset, 0,
                            -kCellHorizontalInset, 0);
        QStyledItemDelegate::paint(painter, content, index);

        if (rowSelected && isFirstVisibleColumn(index.column())) {
            const bool rtl = m_grid->layoutDirection() == Qt::RightToLeft;
            const qreal indicatorX = rtl
                ? option.rect.right() - 6.0
                : option.rect.left() + 4.0;
            const QRectF indicator(
                indicatorX,
                option.rect.center().y() - 8.0,
                3.0,
                16.0);
            painter->setPen(Qt::NoPen);
            painter->setBrush(colors.accentDefault);
            painter->drawRoundedRect(indicator,
                                     ::CornerRadius::Indicator,
                                     ::CornerRadius::Indicator);
        }

        if (current && m_grid->hasFocus() && enabled
            && m_grid->selectionBehavior()
                == QAbstractItemView::SelectItems) {
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(colors.accentDefault, 1.5));
            painter->drawRoundedRect(
                QRectF(option.rect).adjusted(1.5, 1.5, -1.5, -1.5),
                qMax<qreal>(2.0, m_grid->themeRadius().control - 1.0),
                qMax<qreal>(2.0, m_grid->themeRadius().control - 1.0));
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override
    {
        QSize hint = QStyledItemDelegate::sizeHint(option, index);
        hint.setHeight(qMax(hint.height(), kRowHeight));
        return hint;
    }

private:
    bool isFirstVisibleColumn(int logicalIndex) const
    {
        if (!m_grid || !m_grid->horizontalHeader())
            return false;
        const QHeaderView* header = m_grid->horizontalHeader();
        for (int visualIndex = 0; visualIndex < header->count(); ++visualIndex) {
            const int candidate = header->logicalIndex(visualIndex);
            if (!m_grid->isColumnHidden(candidate))
                return candidate == logicalIndex;
        }
        return false;
    }

    DataGrid* m_grid = nullptr;
};

} // namespace detail

DataGrid::DataGrid(QWidget* parent)
    : QTableView(parent)
{
    detail::ensureDataGridAccessibilityFactory();
    setObjectName(QStringLiteral("FluentQtDataGrid"));
    setHorizontalHeader(
        new detail::DataGridHeaderView(Qt::Horizontal, this));
    setVerticalHeader(
        new detail::DataGridHeaderView(Qt::Vertical, this));
    setFrameStyle(QFrame::NoFrame);
    setAlternatingRowColors(false);
    setCornerButtonEnabled(false);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setSelectionBehavior(QAbstractItemView::SelectItems);
    QTableView::setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    viewport()->setAutoFillBackground(false);
    setShowGrid(false);
    setWordWrap(false);

    horizontalHeader()->setDefaultSectionSize(120);
    horizontalHeader()->setMinimumSectionSize(56);
    horizontalHeader()->setFixedHeight(kHeaderHeight);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    horizontalHeader()->setSectionsMovable(true);

    verticalHeader()->setDefaultSectionSize(kRowHeight);
    verticalHeader()->setMinimumSectionSize(::Spacing::ControlHeight::Standard);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setFixedWidth(::Spacing::ControlHeight::Large);
    verticalHeader()->hide();

    QTableView::setItemDelegate(new detail::DataGridCellDelegate(this));

    m_verticalFluentScrollBar = fluent::scrolling::createOverlayScrollBar(
        Qt::Vertical, this, verticalScrollBar(),
        QStringLiteral("fluentDataGridVerticalScrollBar"));
    m_horizontalFluentScrollBar = fluent::scrolling::createOverlayScrollBar(
        Qt::Horizontal, this, horizontalScrollBar(),
        QStringLiteral("fluentDataGridHorizontalScrollBar"));
    connect(verticalScrollBar(), &QScrollBar::rangeChanged,
            this, [this] { syncFluentScrollBars(); });
    connect(horizontalScrollBar(), &QScrollBar::rangeChanged,
            this, [this] { syncFluentScrollBars(); });

    m_borderOverlay = new detail::DataGridFrameOverlay(this);

    applyThemePalette();
    syncFluentScrollBars();
}

DataGrid::~DataGrid()
{
    for (const QMetaObject::Connection& connection : m_modelConnections)
        disconnect(connection);
}

void DataGrid::setModel(QAbstractItemModel* model)
{
    if (QTableView::model() == model)
        return;

    for (const QMetaObject::Connection& connection : m_modelConnections)
        disconnect(connection);
    m_modelConnections.clear();

    const bool notifyVisibleReset = isVisible();
    QTableView::setModel(model);
    setHoveredRow(-1);
    if (notifyVisibleReset) {
        const QPointer<QAbstractItemModel> expectedModel(model);
        QTimer::singleShot(0, this, [this, expectedModel] {
            if (this->model() == expectedModel.data())
                notifyAccessibleModelReset(this);
        });
    }
    connectModelSignals(model);
    refreshModelPresentation();
}

void DataGrid::setSelectionModel(QItemSelectionModel* selectionModel)
{
    QTableView::setSelectionModel(selectionModel);
    if (viewport())
        viewport()->update();
}

void DataGrid::keyPressEvent(QKeyEvent* event)
{
    if (event && event->key() == Qt::Key_F2
        && editTriggers().testFlag(QAbstractItemView::EditKeyPressed)
        && currentIndex().isValid()
        && edit(currentIndex(), QAbstractItemView::EditKeyPressed, event)) {
        event->accept();
        return;
    }

    QTableView::keyPressEvent(event);
}

void DataGrid::leaveEvent(QEvent* event)
{
    setHoveredRow(-1);
    QTableView::leaveEvent(event);
}

void DataGrid::mouseMoveEvent(QMouseEvent* event)
{
    if (event)
        setHoveredRow(indexAt(fluentMousePos(event)).row());
    QTableView::mouseMoveEvent(event);
}

void DataGrid::setSelectionMode(
    fluent::collections::SelectionMode mode)
{
    if (m_selectionMode == mode)
        return;
    m_selectionMode = mode;
    switch (mode) {
    case fluent::collections::SelectionMode::None:
        QTableView::setSelectionMode(QAbstractItemView::NoSelection);
        break;
    case fluent::collections::SelectionMode::Single:
        QTableView::setSelectionMode(QAbstractItemView::SingleSelection);
        break;
    case fluent::collections::SelectionMode::Multiple:
        QTableView::setSelectionMode(QAbstractItemView::MultiSelection);
        break;
    case fluent::collections::SelectionMode::Extended:
        QTableView::setSelectionMode(QAbstractItemView::ExtendedSelection);
        break;
    }
    emit selectionModeChanged();
}

void DataGrid::setPlaceholderText(const QString& text)
{
    if (m_placeholderText == text)
        return;
    m_placeholderText = text;
    updateAutomaticAccessibleDescription();
    viewport()->update();
    emit placeholderTextChanged();
}

bool DataGrid::isShowingPlaceholder() const
{
    return !m_placeholderText.isEmpty()
        && (!model() || model()->rowCount() == 0 || model()->columnCount() == 0);
}

void DataGrid::setBorderVisible(bool visible)
{
    if (m_borderVisible == visible)
        return;
    m_borderVisible = visible;
    if (m_borderOverlay) {
        m_borderOverlay->setVisible(visible);
        m_borderOverlay->update();
    }
    emit borderVisibleChanged();
}

void DataGrid::setBackgroundVisible(bool visible)
{
    if (m_backgroundVisible == visible)
        return;
    m_backgroundVisible = visible;
    applyThemePalette();
    viewport()->update();
    emit backgroundVisibleChanged();
}

void DataGrid::setScrollChainingEnabled(bool enabled)
{
    if (m_scrollChainingEnabled == enabled)
        return;
    m_scrollChainingEnabled = enabled;
    emit scrollChainingEnabledChanged();
}

void DataGrid::onThemeUpdated()
{
    applyThemePalette();
    if (viewport())
        viewport()->update();
    if (horizontalHeader())
        horizontalHeader()->viewport()->update();
    if (verticalHeader())
        verticalHeader()->viewport()->update();
    if (m_borderOverlay)
        m_borderOverlay->update();
}

void DataGrid::paintEvent(QPaintEvent* event)
{
    const auto& colors = themeColorsRef();
    if (m_backgroundVisible) {
        QPainter background(viewport());
        background.fillRect(viewport()->rect(), colors.bgLayer);
    } else if (fluent::collections::detail::shouldClearCompositedViewport(this)) {
        QPainter clear(viewport());
        clear.setCompositionMode(QPainter::CompositionMode_Source);
        clear.fillRect(viewport()->rect(), Qt::transparent);
    }

    QTableView::paintEvent(event);

    if (isShowingPlaceholder()) {
        QPainter placeholder(viewport());
        placeholder.setRenderHint(QPainter::Antialiasing);
        placeholder.setPen(colors.textTertiary);
        placeholder.setFont(themeFont(Typography::FontRole::Body).toQFont());
        placeholder.drawText(
            viewport()->rect().adjusted(32, 32, -32, -32),
            Qt::AlignCenter | Qt::TextWordWrap,
            m_placeholderText);
    }

}

void DataGrid::resizeEvent(QResizeEvent* event)
{
    QTableView::resizeEvent(event);
    if (m_borderOverlay) {
        m_borderOverlay->setGeometry(rect());
        m_borderOverlay->raise();
    }
    syncFluentScrollBars();
}

void DataGrid::showEvent(QShowEvent* event)
{
    QTableView::showEvent(event);
    syncFluentScrollBars();
    QTimer::singleShot(0, this, [this] { syncFluentScrollBars(); });
}

void DataGrid::wheelEvent(QWheelEvent* event)
{
    if (m_scrollChainingEnabled && event && event->angleDelta().y() != 0) {
        const int delta = event->angleDelta().y();
        const QScrollBar* scrollBar = verticalScrollBar();
        const bool beyondStart = delta > 0
            && scrollBar->value() <= scrollBar->minimum();
        const bool beyondEnd = delta < 0
            && scrollBar->value() >= scrollBar->maximum();
        if (beyondStart || beyondEnd) {
            event->ignore();
            return;
        }
    }

    QTableView::wheelEvent(event);
    syncFluentScrollBars();
}

void DataGrid::scrollContentsBy(int dx, int dy)
{
    QTableView::scrollContentsBy(dx, dy);
    syncFluentScrollBars();
}

void DataGrid::setHoveredRow(int row)
{
    if (m_hoveredRow == row)
        return;

    const int previous = m_hoveredRow;
    m_hoveredRow = row;
    if (!viewport())
        return;

    QRect dirty;
    const auto includeRow = [this, &dirty](int candidate) {
        if (!model() || candidate < 0 || candidate >= model()->rowCount())
            return;
        const QRect rowRect(
            0,
            rowViewportPosition(candidate),
            viewport()->width(),
            rowHeight(candidate));
        dirty = dirty.isNull() ? rowRect : dirty.united(rowRect);
    };
    includeRow(previous);
    includeRow(m_hoveredRow);
    if (!dirty.isNull())
        viewport()->update(dirty.adjusted(0, -1, 0, 1));
}

void DataGrid::applyThemePalette()
{
    const auto& colors = themeColorsRef();
    const QColor transparent = Qt::transparent;
    const QColor base = m_backgroundVisible ? colors.bgLayer : transparent;
    const QColor alternate = base;

    QPalette viewPalette = palette();
    viewPalette.setColor(QPalette::Base, base);
    viewPalette.setColor(QPalette::AlternateBase, alternate);
    viewPalette.setColor(QPalette::Window, base);
    viewPalette.setColor(QPalette::Text, colors.textPrimary);
    viewPalette.setColor(QPalette::WindowText, colors.textPrimary);
    viewPalette.setColor(QPalette::Highlight, colors.subtleSecondary);
    viewPalette.setColor(QPalette::HighlightedText, colors.textPrimary);
    viewPalette.setColor(QPalette::Disabled, QPalette::Text, colors.textDisabled);
    viewPalette.setColor(QPalette::Disabled, QPalette::WindowText,
                         colors.textDisabled);
    viewPalette.setColor(QPalette::Disabled, QPalette::Highlight,
                         colors.accentDisabled);
    setPalette(viewPalette);
    viewport()->setPalette(viewPalette);

    QPalette headerPalette = viewPalette;
    headerPalette.setColor(QPalette::Button, colors.bgLayerAlt);
    headerPalette.setColor(QPalette::ButtonText, colors.textSecondary);
    horizontalHeader()->setPalette(headerPalette);
    verticalHeader()->setPalette(headerPalette);

    setFont(themeFont(Typography::FontRole::Body).toQFont());
    QFont headerFont = themeFont(Typography::FontRole::Caption).toQFont();
    headerFont.setWeight(QFont::DemiBold);
    horizontalHeader()->setFont(headerFont);
    verticalHeader()->setFont(headerFont);
    if (m_borderOverlay)
        m_borderOverlay->update();
}

void DataGrid::connectModelSignals(QAbstractItemModel* model)
{
    if (!model)
        return;

    const auto refresh = [this] { refreshModelPresentation(); };
    m_modelConnections.append(connect(
        model, &QAbstractItemModel::modelReset, this, refresh));
    m_modelConnections.append(connect(
        model, &QAbstractItemModel::layoutChanged, this, refresh));
    m_modelConnections.append(connect(
        model, &QAbstractItemModel::rowsInserted, this, refresh));
    m_modelConnections.append(connect(
        model, &QAbstractItemModel::rowsRemoved, this, refresh));
    m_modelConnections.append(connect(
        model, &QAbstractItemModel::columnsInserted, this, refresh));
    m_modelConnections.append(connect(
        model, &QAbstractItemModel::columnsRemoved, this, refresh));
    m_modelConnections.append(connect(
        model, &QAbstractItemModel::dataChanged, this, refresh));
    m_modelConnections.append(connect(
        model, &QAbstractItemModel::headerDataChanged, this, refresh));
    m_modelConnections.append(connect(
        model, &QObject::destroyed, this, refresh));
}

void DataGrid::refreshModelPresentation()
{
    updateAutomaticAccessibleDescription();
    if (viewport())
        viewport()->update();
    if (horizontalHeader())
        horizontalHeader()->viewport()->update();
    if (verticalHeader())
        verticalHeader()->viewport()->update();
    QTimer::singleShot(0, this, [this] { syncFluentScrollBars(); });
}

void DataGrid::updateAutomaticAccessibleDescription()
{
    const QString current = accessibleDescription();
    if (!current.isEmpty() && current != m_automaticAccessibleDescription)
        return;

    const QString automatic = isShowingPlaceholder()
        ? m_placeholderText
        : QString();
    if (current == automatic) {
        m_automaticAccessibleDescription = automatic;
        return;
    }

    setAccessibleDescription(automatic);
    m_automaticAccessibleDescription = automatic;
    QAccessibleEvent event(this, QAccessible::DescriptionChanged);
    QAccessible::updateAccessibility(&event);
}

void DataGrid::syncFluentScrollBars()
{
    if (!m_verticalFluentScrollBar || !m_horizontalFluentScrollBar)
        return;

    if (m_borderOverlay)
        m_borderOverlay->raise();

    fluent::scrolling::suppressNativeScrollBars(
        verticalScrollBar(), horizontalScrollBar());
    const QRect viewportRect = viewport()->geometry();
    if (fluent::scrolling::mirrorNativeScrollBar(
            m_verticalFluentScrollBar, verticalScrollBar())) {
        fluent::scrolling::placeVerticalScrollBar(
            m_verticalFluentScrollBar, rect(),
            viewportRect.top() + kScrollBarInset,
            kScrollBarInset, kScrollBarInset);
    }
    if (fluent::scrolling::mirrorNativeScrollBar(
            m_horizontalFluentScrollBar, horizontalScrollBar())) {
        fluent::scrolling::placeHorizontalScrollBar(
            m_horizontalFluentScrollBar, rect(),
            viewportRect.left() + kScrollBarInset,
            kScrollBarInset, kScrollBarInset);
    }
    m_verticalFluentScrollBar->raise();
    m_horizontalFluentScrollBar->raise();
}

} // namespace fluent::collections
