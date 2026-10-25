#include "compatibility/InputEventCompat.h"
#include "DataGrid.h"

#include "private/DataGridAccessibility_p.h"

#include <QAbstractItemModel>
#include <QAccessible>
#include <QApplication>
#include <QBrush>
#include <QCursor>
#include <QRegularExpressionValidator>
#include <QFocusEvent>
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
#include <QVariantAnimation>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QWheelEvent>
#include <cmath>
#include <limits>

#include "components/collections/CollectionViewBackdrop_p.h"
#include "components/collections/private/CollectionItemState_p.h"
#include "components/scrolling/OverlayScrollChrome.h"
#include "components/scrolling/ScrollBar.h"
#include "design/CornerRadius.h"
#include "design/Spacing.h"
#include "design/IconCatalog.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/textfields/LineEdit.h"
#include "components/textfields/NumberBox.h"
#include "compatibility/private/VariantCompat_p.h"
#include "compatibility/private/AccessibleTableCompat_p.h"
#include "compatibility/private/ItemViewSelectionCompat_p.h"
#include "compatibility/private/ForwardingItemDelegate_p.h"

namespace fluent::collections {

namespace {

constexpr int kScrollBarInset = ::Spacing::XSmall / 2;
constexpr int kHeaderHeight = ::Spacing::ControlHeight::Standard + ::Spacing::XSmall;
constexpr int kRowHeight = ::Spacing::ControlHeight::Standard + ::Spacing::XSmall;
constexpr int kCellHorizontalInset = ::Spacing::Medium;

QRect clearOfVerticalScrollBar(const DataGrid* grid, const QWidget* viewport, QRect rect)
{
    const auto* bar = grid->verticalFluentScrollBar();
    if (!viewport || !bar || grid->verticalScrollBar()->maximum() <= grid->verticalScrollBar()->minimum())
        return rect;
    const QRect barRect = bar->geometry().translated(-viewport->mapTo(grid, QPoint()));
    if (!barRect.intersects(rect))
        return rect;
    // Reserve space by scroll range, so text does not move as the thumb fades.
    // zh_CN: 按滚动范围预留空间，避免滑块淡入淡出时文字跳动。
    if (grid->layoutDirection() == Qt::RightToLeft)
        rect.setLeft(qMin(rect.right(), qMax(rect.left(), barRect.right() + ::Spacing::Gap::Normal + 1)));
    else
        rect.setRight(qMax(rect.left(), qMin(rect.right(), barRect.left() - ::Spacing::Gap::Normal - 1)));
    return rect;
}

void paintViewportBackground(DataGrid* grid, QWidget* viewport)
{
    QPainter painter(viewport);
    if (grid->isBackgroundVisible()) {
        painter.fillRect(viewport->rect(), grid->themeColorsRef().bgLayer);
    } else if (detail::shouldClearCompositedViewport(grid)) {
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(viewport->rect(), Qt::transparent);
    }
}

QList<int> visibleRows(const QTableView& view)
{
    QList<int> rows;
    for (int y = 0; y < view.viewport()->height();) {
        const int row = view.rowAt(y);
        if (row < 0)
            break;
        rows.append(row);
        y = qMax(y + 1, view.rowViewportPosition(row) + view.rowHeight(row));
    }
    return rows;
}

void notifyAccessibleModelReset(DataGrid* grid)
{
#if QT_CONFIG(accessibility)
    if (!grid)
        return;

    QAccessibleTableModelChangeEvent event(grid, QAccessibleTableModelChangeEvent::ModelReset);
    if (QAccessibleInterface* interface = QAccessible::queryAccessibleInterface(grid)) {
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

class DataGridPinnedView final : public QTableView {
public:
    explicit DataGridPinnedView(DataGrid* grid) : QTableView(grid), m_grid(grid)
    {
        viewport()->setAutoFillBackground(false);
    }
    bool editCell(const QModelIndex& index, EditTrigger trigger, QEvent* event)
    {
        return QTableView::edit(index, trigger, event);
    }
    void setSelectionModel(QItemSelectionModel* selection) override
    {
        ::compatibility::detail::setPassiveTableSelectionModel(this, selection);
    }

protected:
    QModelIndex moveCursor(CursorAction action, Qt::KeyboardModifiers modifiers) override
    {
        return m_grid->moveCursor(action, modifiers);
    }
    bool edit(const QModelIndex& index, EditTrigger trigger, QEvent* event) override
    {
        return m_grid->edit(index, trigger, event);
    }
    void paintEvent(QPaintEvent* event) override
    {
        // Match the main viewport, including empty rows and transparent composition.
        // zh_CN: 与主视口共用背景绘制，覆盖空行区域和透明合成。
        paintViewportBackground(m_grid, viewport());
        QTableView::paintEvent(event);
    }

private:
    DataGrid* m_grid;
};

class DataGridHeaderView final : public QHeaderView {
public:
    DataGridHeaderView(Qt::Orientation orientation, DataGrid* grid) : QHeaderView(orientation, grid), m_grid(grid)
    {
        setMouseTracking(true);
        viewport()->setMouseTracking(true);
        viewport()->setAutoFillBackground(false);
        setSectionsClickable(true);
        setHighlightSections(false);
        m_sortAngle = sortIndicatorOrder() == Qt::AscendingOrder ? 0.0 : 180.0;
        m_sortedSection = sortIndicatorSection();
        m_sortAnimation = new QVariantAnimation(this);
        connect(m_sortAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_sortAngle = value.toReal();
            viewport()->update();
        });
        connect(this, &QHeaderView::sortIndicatorChanged, this, [this](int section, Qt::SortOrder order) {
            const qreal target = order == Qt::AscendingOrder ? 0.0 : 180.0;
            m_sortAnimation->stop();
            m_sortAnimation->setStartValue(section == m_sortedSection ? m_sortAngle : target);
            m_sortAnimation->setEndValue(target);
            m_sortAnimation->setEasingCurve(m_grid->themeAnimation().decelerate);
            m_sortedSection = section;
            ::fluent::detail::startMotionTransition(m_sortAnimation, m_grid->themeAnimation().fast,
                m_grid->isAnimationEnabled() && m_grid->isVisible());
        });
        const auto settleMotion = [this] {
            if (!m_grid->isAnimationEnabled() || MotionPolicy::instance().mode() == MotionPolicy::Mode::Disabled) {
                m_sortAnimation->stop();
                m_sortAngle = sortIndicatorOrder() == Qt::AscendingOrder ? 0.0 : 180.0;
                viewport()->update();
            }
        };
        connect(grid, &DataGrid::animationEnabledChanged, this, settleMotion);
        connect(&MotionPolicy::instance(), &MotionPolicy::modeChanged, this, settleMotion);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        if (!m_grid)
            return QHeaderView::paintEvent(event);

        QPainter background(viewport());
        background.fillRect(viewport()->rect(), m_grid->themeColorsRef().bgLayer);
        background.end();
        QHeaderView::paintEvent(event);
    }

    void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override
    {
        if (!painter || !rect.isValid() || !m_grid || !model())
            return;

        const auto& colors = m_grid->themeColorsRef();
        const QPoint pointer = viewport()->mapFromGlobal(QCursor::pos());
        const bool hovered = isEnabled() && underMouse() && rect.contains(pointer);
        const bool pressed = hovered && QApplication::mouseButtons().testFlag(Qt::LeftButton);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->fillRect(rect, colors.bgLayer);
        if (pressed)
            painter->fillRect(rect, colors.subtleTertiary);
        else if (hovered)
            painter->fillRect(rect, colors.subtleSecondary);

        const bool horizontal = orientation() == Qt::Horizontal;
        painter->setPen(QPen(colors.strokeDivider, 1.0));
        if (horizontal) {
            painter->drawLine(rect.bottomLeft(), rect.bottomRight());
            const bool rtl = layoutDirection() == Qt::RightToLeft;
            const int edge = sectionViewportPosition(logicalIndex) + (rtl ? 0 : sectionSize(logicalIndex) - 1);
            const int adjacent = logicalIndexAt(edge + (rtl ? -1 : 1));
            if (adjacent >= 0 && adjacent != logicalIndex) {
                const qreal x = rtl ? rect.left() + 0.5 : rect.right() - 0.5;
                painter->drawLine(QPointF(x, rect.top() + ::Spacing::Small),
                    QPointF(x, rect.bottom() - ::Spacing::Small));
            }
        } else {
            painter->drawLine(rect.topRight(), rect.bottomRight());
            painter->drawLine(rect.bottomLeft(), rect.bottomRight());
        }

        QColor foreground = isEnabled() ? colors.textSecondary : colors.textDisabled;
        const QVariant foregroundRole = model()->headerData(logicalIndex, orientation(), Qt::ForegroundRole);
        if (foregroundRole.canConvert<QBrush>()) {
            const QBrush brush = qvariant_cast<QBrush>(foregroundRole);
            if (brush.style() != Qt::NoBrush)
                foreground = brush.color();
        }
        painter->setPen(foreground);

        QFont headerFont = font();
        const QVariant fontRole = model()->headerData(logicalIndex, orientation(), Qt::FontRole);
        if (fontRole.canConvert<QFont>())
            headerFont = qvariant_cast<QFont>(fontRole);
        painter->setFont(headerFont);

        Qt::Alignment alignment = horizontal ? Qt::AlignLeft | Qt::AlignVCenter : Qt::AlignCenter;
        const QVariant alignmentRole = model()->headerData(logicalIndex, orientation(), Qt::TextAlignmentRole);
        if (alignmentRole.isValid())
            alignment = static_cast<Qt::Alignment>(alignmentRole.toInt());

        const bool sorted = horizontal && isSortIndicatorShown() && sortIndicatorSection() == logicalIndex;
        if (sorted && isEnabled())
            painter->setPen(colors.textPrimary);
        const QRect contentRect = horizontal ? clearOfVerticalScrollBar(m_grid, viewport(), rect) : rect;
        QRect textRect = contentRect.adjusted(horizontal ? kCellHorizontalInset : ::Spacing::XSmall, 0,
            horizontal ? -(sorted ? ::Spacing::XLarge : kCellHorizontalInset) : -::Spacing::XSmall, 0);
        const QString text = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();
        const QFontMetrics metrics(headerFont);
        painter->drawText(textRect, alignment, metrics.elidedText(text, Qt::ElideRight, textRect.width()));

        if (sorted) {
            const bool rtl = layoutDirection() == Qt::RightToLeft;
            const int textWidth = qMin(metrics.horizontalAdvance(text), textRect.width());
            const qreal centerX = rtl
                ? textRect.right() - textWidth - ::Spacing::Standard
                : (alignment.testFlag(Qt::AlignRight)
                          ? contentRect.right() - ::Spacing::Medium
                          : textRect.left() + textWidth + ::Spacing::Standard);
            const qreal centerY = rect.center().y() + 0.5;
            painter->translate(centerX, centerY);
            painter->rotate(m_sortAngle);
            Typography::Icons::paintGlyph(*painter, QRectF(-8, -8, 16, 16),
                QStringLiteral("ic_fluent_arrow_up_16_regular"), 16);
        }
        painter->restore();
    }

private:
    DataGrid* m_grid = nullptr;
    QVariantAnimation* m_sortAnimation = nullptr;
    int m_sortedSection = -1;
    qreal m_sortAngle = 0;
};

class DataGridFrameOverlay final : public QWidget {
public:
    explicit DataGridFrameOverlay(DataGrid* grid) : QWidget(grid), m_grid(grid)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setAutoFillBackground(false);
        setFocusPolicy(Qt::NoFocus);
    }

    void setPinnedView(QTableView* view) { m_pinnedView = view; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!m_grid || !m_grid->isBorderVisible())
            return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::NoBrush);
        if (m_pinnedView && m_pinnedView->isVisible()) {
            // Mark the boundary where scrolling columns pass behind the fixed pane.
            // zh_CN: 标明滚动列从固定区域下方经过的边界。
            const QRect pane = m_pinnedView->geometry();
            const qreal edge = layoutDirection() == Qt::RightToLeft ? pane.left() : pane.right() + 1;
            painter.setPen(QPen(m_grid->themeColorsRef().strokeDivider, 1.0));
            painter.drawLine(QPointF(edge, pane.top() + 1), QPointF(edge, pane.bottom()));
        }
        painter.setPen(QPen(m_grid->themeColorsRef().strokeCard, 1.0));
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), m_grid->themeRadius().control,
            m_grid->themeRadius().control);
    }

private:
    DataGrid* m_grid = nullptr;
    QTableView* m_pinnedView = nullptr;
};

class DataGridCellDelegate final : public QStyledItemDelegate {
public:
    explicit DataGridCellDelegate(DataGrid* grid) : QStyledItemDelegate(grid), m_grid(grid) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const int type = index.data(Qt::EditRole).userType();
        QWidget* editor = nullptr;
        if (type == QMetaType::Int || type == QMetaType::UInt || type == QMetaType::Double
            || type == QMetaType::Float) {
            auto* number = new fluent::textfields::NumberBox(parent);
            number->setObjectName(QStringLiteral("fluentDataGridNumberEditor"));
            number->setSpinButtonPlacementMode(fluent::textfields::NumberBox::SpinButtonPlacementMode::Compact);
            number->setAcceptsExpression(false);
            const bool integer = type == QMetaType::Int || type == QMetaType::UInt;
            const double minimum = type == QMetaType::UInt ? 0.0
                : type == QMetaType::Int                   ? std::numeric_limits<int>::lowest()
                : type == QMetaType::Float
                ? std::numeric_limits<float>::lowest()
                : std::numeric_limits<double>::lowest();
            const double maximum = type == QMetaType::UInt ? std::numeric_limits<unsigned int>::max()
                : type == QMetaType::Int                   ? std::numeric_limits<int>::max()
                : type == QMetaType::Float
                ? std::numeric_limits<float>::max()
                : std::numeric_limits<double>::max();
            number->setRange(minimum, maximum);
            if (integer)
                number->setDisplayPrecision(0);
            // Validate syntax here; NumberBox owns numeric parsing and bounds.
            // zh_CN: 此处只验证语法，数值解析和范围约束由 NumberBox 统一负责。
            const QString pattern = integer
                ? QStringLiteral("[+-]?[0-9]+")
                : QStringLiteral("[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?");
            number->setValidator(new QRegularExpressionValidator(QRegularExpression(pattern), number));
            number->setProperty("fluentDataGridValueType", type);
            editor = number;
        } else if (type == QMetaType::QString || type == QMetaType::LongLong || type == QMetaType::ULongLong) {
            editor = new fluent::textfields::LineEdit(parent);
            editor->setProperty("fluentDataGridValueType", type);
        } else {
            editor = QStyledItemDelegate::createEditor(parent, option, index);
        }
        if (!editor)
            return nullptr;
        const QString columnName = index.model()->headerData(index.column(), Qt::Horizontal).toString();
        editor->setAccessibleName(columnName.isEmpty() ? tr("Cell value") : columnName);
        if (auto* text = qobject_cast<fluent::textfields::LineEdit*>(editor)) {
            text->setClearButtonEnabled(false);
            text->setFrameVisible(false);
            const int padding = kCellHorizontalInset - ::Spacing::XSmall;
            QMargins margins = text->contentMargins();
            margins.setLeft(padding);
            if (!qobject_cast<fluent::textfields::NumberBox*>(text))
                margins.setRight(padding);
            margins.setTop(0);
            margins.setBottom(0);
            text->setContentMargins(margins);
        }
        const QPersistentModelIndex persistent(index);
        m_editors.insert(persistent, editor);
        connect(editor, &QObject::destroyed, this, [this, persistent, editor] {
            if (!m_editors.value(persistent) || m_editors.value(persistent) == editor)
                m_editors.remove(persistent);
        });
        connect(editor, &QObject::destroyed, parent, [parent] { parent->update(); });
        return editor;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        if (auto* number = qobject_cast<fluent::textfields::NumberBox*>(editor)) {
            const QPointer<fluent::textfields::NumberBox> guard(number);
            number->setValue(index.data(Qt::EditRole).toDouble());
            if (guard)
                number->selectAll();
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
    {
        if (auto* number = qobject_cast<fluent::textfields::NumberBox*>(editor)) {
            // Tab and focus-out reach the delegate before the editor's handlers.
            // zh_CN: Tab 和失焦先进入委托，必须先提交输入文本再读取数值。
            const QPointer<fluent::textfields::NumberBox> guard(number);
            const QPointer<QAbstractItemModel> modelGuard(model);
            const QPersistentModelIndex persistent(index);
            number->commitInput();
            if (!guard || !modelGuard || !persistent.isValid() || !std::isfinite(number->value()))
                return;
            const int type = number->property("fluentDataGridValueType").toInt();
            if ((type == QMetaType::Int || type == QMetaType::UInt) && std::trunc(number->value()) != number->value())
                return;
            QVariant value(number->value());
            if (::compatibility::detail::convertVariantToType(value, type))
                modelGuard->setData(persistent, value, Qt::EditRole);
            return;
        }
        const int type = editor->property("fluentDataGridValueType").toInt();
        if (type == QMetaType::LongLong || type == QMetaType::ULongLong) {
            const auto* text = qobject_cast<QLineEdit*>(editor);
            bool valid = false;
            const QVariant value = type == QMetaType::LongLong
                ? QVariant::fromValue(text->text().toLongLong(&valid))
                : QVariant::fromValue(text->text().toULongLong(&valid));
            if (valid)
                model->setData(index, value, Qt::EditRole);
            return;
        }
        QStyledItemDelegate::setModelData(editor, model, index);
    }

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index)
        const override
    {
        QStyledItemDelegate::updateEditorGeometry(editor, option, index);
        if (qobject_cast<fluent::textfields::LineEdit*>(editor)) {
            const int horizontal = ::Spacing::XSmall;
            const int vertical = ::Spacing::XSmall / 2;
            editor->setGeometry(clearOfVerticalScrollBar(m_grid, editor->parentWidget(), option.rect)
                    .adjusted(horizontal, vertical, -horizontal, -vertical));
        }
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (!painter || !m_grid || !index.isValid())
            return;

        const auto& colors = m_grid->themeColorsRef();
        const bool enabled = option.state & QStyle::State_Enabled;
        const bool selected = option.state & QStyle::State_Selected;
        const bool rowSelected = m_grid->selectionModel()
            && m_grid->selectionBehavior() == QAbstractItemView::SelectRows
            && m_grid->selectionModel()->isRowSelected(index.row(), index.parent());
        qreal hoverOpacity = enabled && m_grid->m_hoveredRow == index.row() ? m_grid->m_hoverProgress : 0.0;
        if (enabled && m_grid->m_previousHoveredRow == index.row())
            hoverOpacity = m_grid->m_previousHoverOpacity * (1.0 - m_grid->m_hoverProgress);
        const bool current = index == m_grid->currentIndex();

        QColor background = m_grid->isBackgroundVisible() ? colors.bgLayer : Qt::transparent;
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
        painter->setRenderHint(QPainter::Antialiasing);
        painter->fillRect(option.rect, background);
        const auto editor = m_editors.isEmpty() ? QPointer<QWidget>() : m_editors.value(index);
        if (editor && editor->isVisible()) {
            // The transparent editor owns the text. Keep one focus cue and no
            // selection/hover fill underneath it, including in the pinned pane.
            // zh_CN: 透明编辑器负责文字；固定列也只保留一条焦点提示，不叠加选中或悬停底色。
            if (editor->hasFocus()) {
                painter->setPen(QPen(colors.accentDefault, 2.0));
                const QRect inset = editor->geometry();
                painter->drawLine(QPointF(inset.left(), inset.bottom()), QPointF(inset.right(), inset.bottom()));
            }
            painter->restore();
            return;
        }
        const qreal selectionOpacity = enabled ? m_grid->selectionOpacity(index, selected || rowSelected) : 0.0;
        if (selectionOpacity > 0.0) {
            QColor tint = collectionItemVisualStyle(QStyle::State_Enabled | QStyle::State_Selected, colors).background;
            tint.setAlphaF(tint.alphaF() * selectionOpacity);
            painter->fillRect(option.rect, tint);
        } else if (hoverOpacity > 0.0) {
            QColor tint = colors.subtleTertiary;
            tint.setAlphaF(tint.alphaF() * hoverOpacity);
            painter->fillRect(option.rect, tint);
        }

        QStyleOptionViewItem content = option;
        initStyleOption(&content, index);
        content.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus);
        content.backgroundBrush = Qt::NoBrush;
        content.palette.setColor(QPalette::Text, foreground);
        content.palette.setColor(QPalette::WindowText, foreground);
        const int nativeTextMargin = m_grid->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, &content, m_grid) + 1;
        const int inset = qMax(0, kCellHorizontalInset - nativeTextMargin);
        const auto* view = qobject_cast<const QAbstractItemView*>(option.widget);
        content.rect = clearOfVerticalScrollBar(m_grid, view ? view->viewport() : m_grid->viewport(), content.rect);
        content.rect.adjust(inset, 0, -inset, 0);
        QStyledItemDelegate::paint(painter, content, index);

        if (selectionOpacity > 0.0 && m_grid->selectionBehavior() == QAbstractItemView::SelectRows
            && isFirstVisibleColumn(index.column())) {
            const bool rtl = m_grid->layoutDirection() == Qt::RightToLeft;
            const qreal indicatorX = rtl ? option.rect.right() - 6.0 : option.rect.left() + 4.0;
            const QRectF indicator(indicatorX, option.rect.center().y() - 8.0, 3.0, 16.0);
            painter->setPen(Qt::NoPen);
            QColor indicatorColor = colors.accentDefault;
            indicatorColor.setAlphaF(indicatorColor.alphaF() * selectionOpacity);
            painter->setBrush(indicatorColor);
            painter->drawRoundedRect(indicator, ::CornerRadius::Indicator, ::CornerRadius::Indicator);
        }

        const bool rowFocus = m_grid->selectionBehavior() == QAbstractItemView::SelectRows
            && index.row() == m_grid->currentIndex().row();
        if ((current || rowFocus) && m_grid->hasFocus() && m_grid->m_keyboardFocusVisible && enabled) {
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(colors.accentDefault, 1.5));
            QRectF focusRect = QRectF(option.rect).adjusted(1.5, 1.5, -1.5, -1.5);
            if (rowFocus) {
                const auto* owner = qobject_cast<const QAbstractItemView*>(option.widget);
                const auto* header = m_grid->horizontalHeader();
                QRect columns;
                for (int column = 0; column < header->count(); ++column) {
                    if (!m_grid->isColumnHidden(column))
                        columns = columns.united(
                            QRect(header->sectionViewportPosition(column), 0, header->sectionSize(column), 1));
                }
                columns = columns.intersected(QRect(0, 0, m_grid->viewport()->width(), 1));
                const int offset = owner
                    ? m_grid->viewport()->mapTo(m_grid, QPoint()).x() - owner->viewport()->mapTo(m_grid, QPoint()).x()
                    : 0;
                // One row outline ends at the visible columns, including across
                // the pinned pane. Each cell paints only its part of that outline.
                // zh_CN: 整行焦点框截止于可见列边缘，固定列也绘制同一外框的对应部分。
                focusRect.setLeft(columns.left() + offset + 1.5);
                focusRect.setRight(columns.right() + 1 + offset - 1.5);
                painter->setClipRect(option.rect, Qt::IntersectClip);
            }
            painter->drawRoundedRect(focusRect, qMax<qreal>(2.0, m_grid->themeRadius().control - 1.0),
                qMax<qreal>(2.0, m_grid->themeRadius().control - 1.0));
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QSize hint = QStyledItemDelegate::sizeHint(option, index);
        hint.setHeight(qMax(hint.height(), kRowHeight));
        return hint;
    }

private:
    mutable QHash<QPersistentModelIndex, QPointer<QWidget>> m_editors;

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

DataGrid::DataGrid(QWidget* parent) : QTableView(parent)
{
    detail::ensureDataGridAccessibilityFactory();
    setObjectName(QStringLiteral("FluentQtDataGrid"));
    setHorizontalHeader(new detail::DataGridHeaderView(Qt::Horizontal, this));
    setVerticalHeader(new detail::DataGridHeaderView(Qt::Vertical, this));
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

    m_verticalFluentScrollBar = fluent::scrolling::createOverlayScrollBar(Qt::Vertical, this, verticalScrollBar(),
        QStringLiteral("fluentDataGridVerticalScrollBar"));
    m_horizontalFluentScrollBar = fluent::scrolling::createOverlayScrollBar(Qt::Horizontal, this, horizontalScrollBar(),
        QStringLiteral("fluentDataGridHorizontalScrollBar"));
    connect(verticalScrollBar(), &QScrollBar::rangeChanged, this, [this] { syncFluentScrollBars(); });
    connect(horizontalScrollBar(), &QScrollBar::rangeChanged, this, [this] { syncFluentScrollBars(); });

    m_borderOverlay = new detail::DataGridFrameOverlay(this);

    m_hoverAnimation = new QVariantAnimation(this);
    m_selectionAnimation = new QVariantAnimation(this);
    for (auto* animation : {m_hoverAnimation, m_selectionAnimation}) {
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);
        animation->setEasingCurve(themeAnimation().decelerate);
    }
    connect(m_hoverAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_hoverProgress = value.toReal();
        viewport()->update();
        if (m_pinnedView)
            m_pinnedView->viewport()->update();
    });
    connect(m_selectionAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_selectionProgress = value.toReal();
        viewport()->update();
        if (m_pinnedView)
            m_pinnedView->viewport()->update();
    });
    connect(m_selectionAnimation, &QVariantAnimation::finished, this, [this] { m_selectionTransitions.clear(); });
    const auto layoutChanged = [this] {
        if (m_syncingColumns)
            return;
        m_pinnedStructureDirty = true;
        syncFluentScrollBars();
        emit columnLayoutChanged();
    };
    connect(horizontalHeader(), &QHeaderView::sectionMoved, this, layoutChanged);
    connect(horizontalHeader(), &QHeaderView::geometriesChanged, this, layoutChanged);
    connect(horizontalHeader(), &QHeaderView::sectionResized, this,
        [this, layoutChanged](int c, int before, int width) {
            if (m_changingColumns)
                return;
            if (width > 0)
                m_columnWidths.insert(c, width);
            else if (!m_columnWidths.contains(c) && before > 0)
                m_columnWidths.insert(c, before);
            layoutChanged();
        });
    connect(horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, layoutChanged);
    connect(verticalHeader(), &QHeaderView::sectionResized, this, [this](int row, int, int height) {
        if (m_pinnedView) {
            // Qt emits the zero-size change before updating the hidden flag.
            m_pinnedView->setRowHidden(row, height == 0);
            if (height > 0)
                m_pinnedView->setRowHeight(row, height);
        }
    });
    connect(&MotionPolicy::instance(), &MotionPolicy::modeChanged, this, [this] {
        if (MotionPolicy::instance().mode() == MotionPolicy::Mode::Disabled) {
            m_hoverAnimation->stop();
            m_selectionAnimation->stop();
            m_hoverProgress = m_selectionProgress = 1.0;
            m_selectionTransitions.clear();
            viewport()->update();
            if (m_pinnedView)
                m_pinnedView->viewport()->update();
        }
    });

    applyThemePalette();
    syncFluentScrollBars();
}

DataGrid::~DataGrid()
{
    for (const QMetaObject::Connection& connection : m_modelConnections)
        disconnect(connection);
    // Headers can emit geometriesChanged from QWidget teardown, after our
    // column containers have been destroyed. Disconnect owned senders first.
    // zh_CN: QWidget 清理子控件时表头仍可能发出几何通知，先断开以免访问已析构的列状态。
    for (auto* child : findChildren<QObject*>())
        QObject::disconnect(child, nullptr, this, nullptr);
    QObject::disconnect(&MotionPolicy::instance(), nullptr, this, nullptr);
    m_syncingColumns = true;
    auto* pinned = m_pinnedView;
    m_pinnedView = nullptr;
    delete pinned;
}

void DataGrid::setModel(QAbstractItemModel* model)
{
    if (QTableView::model() == model)
        return;

    for (const QMetaObject::Connection& connection : m_modelConnections)
        disconnect(connection);
    m_modelConnections.clear();
    m_pinnedColumns.clear();
    m_columnWidths.clear();
    m_columnWidthsBeforeChange.clear();
    m_changingColumns = false;
    m_pinnedStructureDirty = true;
    m_selectionTransitions.clear();
    if (m_pinnedView)
        m_pinnedView->setModel(nullptr);

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
    syncPinnedColumns();
    emit modelChanged(model);
}

void DataGrid::setSelectionModel(QItemSelectionModel* selectionModel)
{
    QTableView::setSelectionModel(selectionModel);
    m_selectionTransitions.clear();
    if (m_pinnedView && selectionModel && m_pinnedView->model() == model())
        m_pinnedView->setSelectionModel(selectionModel);
    if (viewport())
        viewport()->update();
}

void DataGrid::keyPressEvent(QKeyEvent* event)
{
    m_keyboardFocusVisible = true;
    updateFocusPresentation();
    if (event && event->key() == Qt::Key_F2 && editTriggers().testFlag(QAbstractItemView::EditKeyPressed)
        && currentIndex().isValid() && edit(currentIndex(), QAbstractItemView::EditKeyPressed, event)) {
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

void DataGrid::setSelectionMode(fluent::collections::SelectionMode mode)
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
    return !m_placeholderText.isEmpty() && (!model() || model()->rowCount() == 0 || model()->columnCount() == 0);
}

void DataGrid::setBorderVisible(bool visible)
{
    if (m_borderVisible == visible)
        return;
    m_borderVisible = visible;
    updateSurfaceMask();
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
    updateSurfaceMask();
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
    updateSurfaceMask();
    if (viewport())
        viewport()->update();
    if (horizontalHeader())
        horizontalHeader()->viewport()->update();
    if (verticalHeader())
        verticalHeader()->viewport()->update();
    if (m_borderOverlay)
        m_borderOverlay->update();
}

void DataGrid::updateSurfaceMask()
{
    // Clip headers, frozen panes and editors together, only when geometry or
    // presentation changes. A border alone cannot hide their square backgrounds.
    // zh_CN: 在几何或外观变化时统一裁剪表头、固定列和编辑器，避免方形背景盖住圆角。
    const QRegion surface = (m_borderVisible || m_backgroundVisible)
        ? fluent::overlay::roundedRectRegion(rect(), themeRadius().control)
        : QRegion();
    if (mask() != surface)
        setMask(surface);
}

void DataGrid::paintEvent(QPaintEvent* event)
{
    syncPinnedColumns();
    const auto& colors = themeColorsRef();
    paintViewportBackground(this, viewport());
    QTableView::paintEvent(event);

    if (isShowingPlaceholder()) {
        QPainter placeholder(viewport());
        placeholder.setRenderHint(QPainter::Antialiasing);
        placeholder.setPen(colors.textTertiary);
        placeholder.setFont(themeFont(Typography::FontRole::Body).toQFont());
        placeholder.drawText(viewport()->rect().adjusted(32, 32, -32, -32), Qt::AlignCenter | Qt::TextWordWrap,
            m_placeholderText);
    }
}

void DataGrid::resizeEvent(QResizeEvent* event)
{
    QTableView::resizeEvent(event);
    updateSurfaceMask();
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
    QTableView::wheelEvent(event);
    const bool scrollable = verticalScrollBar()->maximum() > verticalScrollBar()->minimum()
        || horizontalScrollBar()->maximum() > horizontalScrollBar()->minimum();
    // Match ScrollView: a scrollable container owns boundary and momentum
    // events unless chaining was requested. A fitted view stays transparent.
    // zh_CN: 与 ScrollView 一致：默认持有滚动边界和惯性事件，无可滚动内容时放行。
    if (!m_scrollChainingEnabled && scrollable)
        event->accept();
    syncFluentScrollBars();
}

void DataGrid::updateFocusPresentation()
{
    viewport()->update();
    if (m_pinnedView)
        m_pinnedView->viewport()->update();
}

void DataGrid::focusInEvent(QFocusEvent* event)
{
    QTableView::focusInEvent(event);
    if (event->reason() == Qt::MouseFocusReason)
        m_keyboardFocusVisible = false;
    else if (event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason
        || event->reason() == Qt::ShortcutFocusReason)
        m_keyboardFocusVisible = true;
    updateFocusPresentation();
}

void DataGrid::focusOutEvent(QFocusEvent* event)
{
    QTableView::focusOutEvent(event);
    updateFocusPresentation();
}

void DataGrid::mousePressEvent(QMouseEvent* event)
{
    m_keyboardFocusVisible = false;
    updateFocusPresentation();
    QTableView::mousePressEvent(event);
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
    m_previousHoverOpacity = m_hoverProgress;
    m_previousHoveredRow = previous;
    m_hoveredRow = row;
    if (!viewport())
        return;

    QRect dirty;
    const auto includeRow = [this, &dirty](int candidate) {
        if (!model() || candidate < 0 || candidate >= model()->rowCount())
            return;
        const QRect rowRect(0, rowViewportPosition(candidate), viewport()->width(), rowHeight(candidate));
        dirty = dirty.isNull() ? rowRect : dirty.united(rowRect);
    };
    includeRow(previous);
    includeRow(m_hoveredRow);
    if (!dirty.isNull())
        viewport()->update(dirty.adjusted(0, -1, 0, 1));
    if (m_hoverAnimation) {
        m_hoverAnimation->stop();
        ::fluent::detail::startMotionTransition(m_hoverAnimation, themeAnimation().fast,
            m_animationEnabled && isVisible());
    }
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
    viewPalette.setColor(QPalette::Disabled, QPalette::WindowText, colors.textDisabled);
    viewPalette.setColor(QPalette::Disabled, QPalette::Highlight, colors.accentDisabled);
    setPalette(viewPalette);
    viewport()->setPalette(viewPalette);

    QPalette headerPalette = viewPalette;
    headerPalette.setColor(QPalette::Button, colors.bgLayerAlt);
    headerPalette.setColor(QPalette::ButtonText, colors.textSecondary);
    horizontalHeader()->setPalette(headerPalette);
    verticalHeader()->setPalette(headerPalette);

    setFont(themeFont(Typography::FontRole::Body).toQFont());
    QFont headerFont = themeFont(Typography::FontRole::Caption).toQFont();
    headerFont.setStyleName(QString());
    headerFont.setWeight(QFont::DemiBold);
    horizontalHeader()->setFont(headerFont);
    verticalHeader()->setFont(headerFont);
    if (m_borderOverlay)
        m_borderOverlay->update();
    if (m_pinnedView) {
        m_pinnedView->setPalette(viewPalette);
        m_pinnedView->viewport()->setPalette(viewPalette);
        m_pinnedView->viewport()->update();
        m_pinnedView->horizontalHeader()->setPalette(headerPalette);
        m_pinnedView->horizontalHeader()->viewport()->update();
        m_pinnedView->setFont(font());
        m_pinnedView->horizontalHeader()->setFont(headerFont);
    }
}

void DataGrid::connectModelSignals(QAbstractItemModel* model)
{
    if (!model)
        return;

    const auto refresh = [this] { refreshModelPresentation(); };
    const auto beforeColumns = [this](const QModelIndex& parent, int, int) {
        if (parent == rootIndex()) {
            m_changingColumns = true;
            m_columnWidthsBeforeChange = m_columnWidths;
        }
    };
    m_modelConnections.append(connect(model, &QAbstractItemModel::columnsAboutToBeInserted, this, beforeColumns));
    m_modelConnections.append(connect(model, &QAbstractItemModel::columnsAboutToBeRemoved, this, beforeColumns));
    m_modelConnections.append(connect(model, &QAbstractItemModel::modelReset, this, [this, refresh] {
        m_pinnedColumns.clear();
        m_columnWidths.clear();
        m_columnWidthsBeforeChange.clear();
        m_changingColumns = false;
        m_pinnedStructureDirty = true;
        m_selectionTransitions.clear();
        refresh();
        emit columnLayoutChanged();
    }));
    m_modelConnections.append(connect(model, &QAbstractItemModel::layoutChanged, this, [this, refresh] {
        refresh();
        if (isVisible())
            notifyAccessibleModelReset(this);
    }));
    m_modelConnections.append(connect(model, &QAbstractItemModel::rowsInserted, this, refresh));
    m_modelConnections.append(connect(model, &QAbstractItemModel::rowsRemoved, this, refresh));
    m_modelConnections.append(connect(model, &QAbstractItemModel::columnsInserted, this,
        [this, refresh](const QModelIndex& parent, int first, int last) {
            if (parent == rootIndex()) {
                QSet<int> pins;
                for (int c : m_pinnedColumns)
                    pins.insert(c >= first ? c + last - first + 1 : c);
                m_pinnedColumns = pins;
                m_columnWidths.clear();
                for (auto it = m_columnWidthsBeforeChange.cbegin(); it != m_columnWidthsBeforeChange.cend(); ++it)
                    m_columnWidths.insert(it.key() >= first ? it.key() + last - first + 1 : it.key(), it.value());
                m_columnWidthsBeforeChange.clear();
                m_changingColumns = false;
                m_pinnedStructureDirty = true;
            }
            refresh();
            emit columnLayoutChanged();
        }));
    m_modelConnections.append(connect(model, &QAbstractItemModel::columnsRemoved, this,
        [this, refresh](const QModelIndex& parent, int first, int last) {
            if (parent == rootIndex()) {
                QSet<int> pins;
                for (int c : m_pinnedColumns)
                    if (c < first || c > last)
                        pins.insert(c > last ? c - (last - first + 1) : c);
                m_pinnedColumns = pins;
                m_columnWidths.clear();
                for (auto it = m_columnWidthsBeforeChange.cbegin(); it != m_columnWidthsBeforeChange.cend(); ++it)
                    if (it.key() < first || it.key() > last)
                        m_columnWidths.insert(it.key() > last ? it.key() - (last - first + 1) : it.key(), it.value());
                m_columnWidthsBeforeChange.clear();
                m_changingColumns = false;
                m_pinnedStructureDirty = true;
            }
            refresh();
            emit columnLayoutChanged();
        }));
    m_modelConnections.append(connect(model, &QAbstractItemModel::dataChanged, this,
        [this, refresh](const QModelIndex& first, const QModelIndex& last) {
            refresh();
            if (isVisible() && first.parent() == rootIndex())
                ::compatibility::detail::refreshAccessibleTableData(this, first, last);
        }));
    m_modelConnections.append(connect(model, &QAbstractItemModel::headerDataChanged, this, refresh));
    m_modelConnections.append(connect(model, &QObject::destroyed, this, refresh));
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
    syncPinnedColumns();
}

void DataGrid::updateAutomaticAccessibleDescription()
{
    const QString current = accessibleDescription();
    if (!current.isEmpty() && current != m_automaticAccessibleDescription)
        return;

    const QString automatic = isShowingPlaceholder() ? m_placeholderText : QString();
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
    syncPinnedColumns();

    if (m_borderOverlay)
        m_borderOverlay->raise();

    fluent::scrolling::suppressNativeScrollBars(verticalScrollBar(), horizontalScrollBar());
    const QRect viewportRect = viewport()->geometry();
    if (fluent::scrolling::mirrorNativeScrollBar(m_verticalFluentScrollBar, verticalScrollBar())) {
        fluent::scrolling::placeVerticalScrollBar(m_verticalFluentScrollBar, rect(),
            viewportRect.top() + kScrollBarInset, kScrollBarInset, kScrollBarInset);
        m_verticalFluentScrollBar->setGeometry(
            QStyle::visualRect(layoutDirection(), rect(), m_verticalFluentScrollBar->geometry()));
    }
    if (fluent::scrolling::mirrorNativeScrollBar(m_horizontalFluentScrollBar, horizontalScrollBar())) {
        const int pinnedWidth = m_pinnedView && m_pinnedView->isVisible() ? m_pinnedView->width() : 0;
        const bool rtl = layoutDirection() == Qt::RightToLeft;
        if (horizontalScrollMode() == QAbstractItemView::ScrollPerPixel)
            m_horizontalFluentScrollBar->setPageStep(qMax(1, horizontalScrollBar()->pageStep() - pinnedWidth));
        fluent::scrolling::placeHorizontalScrollBar(m_horizontalFluentScrollBar, rect(),
            viewportRect.left() + kScrollBarInset + (rtl ? 0 : pinnedWidth),
            width() - viewportRect.right() - 1 + kScrollBarInset + (rtl ? pinnedWidth : 0), kScrollBarInset);
    }
    m_verticalFluentScrollBar->raise();
    m_horizontalFluentScrollBar->raise();
}

void DataGrid::setAnimationEnabled(bool enabled)
{
    if (m_animationEnabled == enabled)
        return;
    m_animationEnabled = enabled;
    if (!enabled) {
        m_hoverAnimation->stop();
        m_selectionAnimation->stop();
        m_hoverProgress = m_selectionProgress = 1.0;
        m_selectionTransitions.clear();
        viewport()->update();
        if (m_pinnedView)
            m_pinnedView->viewport()->update();
    }
    emit animationEnabledChanged();
}

qreal DataGrid::selectionOpacity(const QModelIndex& index, bool selected) const
{
    if (m_selectionTransitions.isEmpty())
        return selected ? 1.0 : 0.0;
    const QModelIndex key = selectionBehavior() == SelectRows ? index.sibling(index.row(), 0) : index;
    const auto it = m_selectionTransitions.constFind(QPersistentModelIndex(key));
    if (it == m_selectionTransitions.cend())
        return selected ? 1.0 : 0.0;
    return it->first + (it->second - it->first) * m_selectionProgress;
}

void DataGrid::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    QTableView::selectionChanged(selected, deselected);
    if (!m_selectionAnimation || !model() || !isVisible())
        return;
    QHash<QPersistentModelIndex, QPair<qreal, qreal>> changes;
    for (int row : visibleRows(*this)) {
        if (changes.size() >= 256)
            break;
        const int columns = selectionBehavior() == SelectRows ? 1 : horizontalHeader()->count();
        for (int c = 0; c < columns && changes.size() < 256; ++c) {
            const int column = selectionBehavior() == SelectRows ? 0 : horizontalHeader()->logicalIndex(c);
            if (selectionBehavior() != SelectRows
                && (isColumnHidden(column) || columnViewportPosition(column) + columnWidth(column) < 0
                    || columnViewportPosition(column) > viewport()->width()))
                continue;
            const QModelIndex index = model()->index(row, column, rootIndex());
            if (!selected.contains(index) && !deselected.contains(index))
                continue;
            const qreal from = selectionOpacity(index, deselected.contains(index));
            changes.insert(QPersistentModelIndex(index), qMakePair(from, selected.contains(index) ? 1.0 : 0.0));
        }
    }
    m_selectionAnimation->stop();
    m_selectionTransitions = changes;
    ::fluent::detail::startMotionTransition(m_selectionAnimation, themeAnimation().fast, m_animationEnabled);
}

bool DataGrid::isColumnPinned(int column) const
{
    return m_pinnedColumns.contains(column);
}

QList<int> DataGrid::pinnedColumns() const
{
    QList<int> result;
    for (int visual = 0; visual < horizontalHeader()->count(); ++visual) {
        const int logical = horizontalHeader()->logicalIndex(visual);
        if (m_pinnedColumns.contains(logical))
            result.append(logical);
    }
    return result;
}

void DataGrid::setColumnPinned(int column, bool pinned)
{
    if (!model() || column < 0 || column >= model()->columnCount(rootIndex()) || isColumnPinned(column) == pinned)
        return;
    if (pinned)
        m_pinnedColumns.insert(column);
    else
        m_pinnedColumns.remove(column);
    m_pinnedStructureDirty = true;
    syncFluentScrollBars();
    emit columnLayoutChanged();
}

bool DataGrid::isColumnVisible(int column) const
{
    return model() && column >= 0 && column < model()->columnCount(rootIndex()) && !isColumnHidden(column);
}

void DataGrid::setColumnVisible(int column, bool visible)
{
    if (!model() || column < 0 || column >= model()->columnCount(rootIndex()) || isColumnVisible(column) == visible)
        return;
    setColumnHidden(column, !visible);
    m_pinnedStructureDirty = true;
    syncFluentScrollBars();
    emit columnLayoutChanged();
}

QByteArray DataGrid::saveColumnState() const
{
    if (!model())
        return {};
    QJsonArray columns;
    for (int visual = 0; visual < horizontalHeader()->count(); ++visual) {
        const int c = horizontalHeader()->logicalIndex(visual);
        columns.append(QJsonObject{{"column", c}, {"title", model()->headerData(c, Qt::Horizontal).toString()},
            {"width",
                isColumnHidden(c) ? m_columnWidths.value(c, horizontalHeader()->defaultSectionSize()) : columnWidth(c)},
            {"visible", !isColumnHidden(c)}, {"pinned", isColumnPinned(c)}});
    }
    return QJsonDocument(
        QJsonObject{{"version", 1}, {"columns", columns}, {"sortColumn", horizontalHeader()->sortIndicatorSection()},
            {"sortOrder", int(horizontalHeader()->sortIndicatorOrder())}, {"sorting", isSortingEnabled()}})
        .toJson(QJsonDocument::Compact);
}

bool DataGrid::restoreColumnState(const QByteArray& state)
{
    if (!model() || state.size() > 1024 * 1024)
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(state);
    const QJsonObject root = doc.object();
    const QJsonArray columns = root.value("columns").toArray();
    const int count = model()->columnCount(rootIndex());
    const int sort = root.value("sortColumn").toInt(-2);
    const int order = root.value("sortOrder").toInt(-1);
    if (!doc.isObject() || root.value("version").toInt() != 1 || columns.size() != count || sort < -1 || sort >= count
        || (order != 0 && order != 1) || !root.value("sorting").isBool())
        return false;
    QSet<int> seen;
    bool sawUnpinned = false;
    for (const auto& value : columns) {
        const auto item = value.toObject();
        const int c = item.value("column").toInt(-1);
        const int width = item.value("width").toInt(-1);
        if (c < 0 || c >= count || seen.contains(c) || width < 0 || width > 16777215 || !item.value("visible").isBool()
            || !item.value("pinned").isBool()
            || item.value("title").toString() != model()->headerData(c, Qt::Horizontal).toString()
            || (sawUnpinned && item.value("pinned").toBool()))
            return false;
        sawUnpinned |= !item.value("pinned").toBool();
        seen.insert(c);
    }
    if (saveColumnState() == state)
        return true;
    {
        QScopedValueRollback<bool> syncing(m_syncingColumns, true);
        const QSignalBlocker blocker(this);
        m_pinnedColumns.clear();
        for (int visual = 0; visual < columns.size(); ++visual) {
            const auto item = columns.at(visual).toObject();
            const int c = item.value("column").toInt();
            horizontalHeader()->moveSection(horizontalHeader()->visualIndex(c), visual);
            if (item.value("width").toInt() > 0) {
                setColumnWidth(c, item.value("width").toInt());
                m_columnWidths.insert(c, item.value("width").toInt());
            }
            setColumnHidden(c, !item.value("visible").toBool());
            if (item.value("pinned").toBool())
                m_pinnedColumns.insert(c);
        }
        // A saved disabled-sort layout must not sort the caller's model.
        // zh_CN: 恢复已关闭排序的布局时，不应触发调用方模型的排序。
        setSortingEnabled(false);
        horizontalHeader()->setSortIndicator(sort, static_cast<Qt::SortOrder>(order));
        setSortingEnabled(root.value("sorting").toBool());
    }
    m_pinnedStructureDirty = true;
    syncFluentScrollBars();
    emit columnLayoutChanged();
    return true;
}

void DataGrid::ensurePinnedView()
{
    if (m_pinnedView)
        return;
    m_pinnedView = new detail::DataGridPinnedView(this);
    m_pinnedView->setObjectName(QStringLiteral("fluentDataGridPinnedColumns"));
    m_pinnedView->setAccessibleName(tr("Pinned columns"));
    m_pinnedView->setFocusPolicy(Qt::NoFocus);
    m_pinnedView->setFocusProxy(this);
    m_pinnedView->setFrameStyle(QFrame::NoFrame);
    m_pinnedView->setShowGrid(false);
    m_pinnedView->setHorizontalHeader(new detail::DataGridHeaderView(Qt::Horizontal, this));
    m_pinnedView->horizontalHeader()->setParent(m_pinnedView);
    m_pinnedView->horizontalHeader()->setFixedHeight(kHeaderHeight);
    m_pinnedView->horizontalHeader()->setSectionsClickable(true);
    m_pinnedView->horizontalHeader()->setMinimumSectionSize(horizontalHeader()->minimumSectionSize());
    m_pinnedView->verticalHeader()->hide();
    m_pinnedView->verticalHeader()->setDefaultSectionSize(kRowHeight);
    m_pinnedView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pinnedView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pinnedView->setVerticalScrollMode(ScrollPerPixel);
    m_pinnedView->setHorizontalScrollMode(ScrollPerPixel);
    m_pinnedView->setMouseTracking(true);
    m_pinnedView->viewport()->setMouseTracking(true);
    m_pinnedView->viewport()->installEventFilter(this);
    m_pinnedDefaultDelegate = new detail::DataGridCellDelegate(this);
    m_pinnedView->setItemDelegate(new ::compatibility::detail::ForwardingItemDelegate(
        [this](const QModelIndex& index) -> QAbstractItemDelegate* {
            auto* delegate = itemDelegateForRow(index.row());
            if (!delegate)
                delegate = itemDelegateForColumn(index.column());
            if (!delegate)
                delegate = itemDelegate();
            return dynamic_cast<detail::DataGridCellDelegate*>(delegate) ? m_pinnedDefaultDelegate : delegate;
        },
        m_pinnedView));
    connect(verticalScrollBar(), &QScrollBar::valueChanged, m_pinnedView->verticalScrollBar(), &QScrollBar::setValue);
    connect(m_pinnedView->verticalScrollBar(), &QScrollBar::valueChanged, verticalScrollBar(), &QScrollBar::setValue);
    connect(m_pinnedView, &QTableView::pressed, this, &DataGrid::pressed);
    connect(m_pinnedView, &QTableView::clicked, this, &DataGrid::clicked);
    connect(m_pinnedView, &QTableView::doubleClicked, this, &DataGrid::doubleClicked);
    connect(m_pinnedView, &QTableView::activated, this, &DataGrid::activated);
    connect(m_pinnedView->horizontalHeader(), &QHeaderView::sectionClicked, this, [this](int column) {
        if (isSortingEnabled())
            sortByColumn(column,
                horizontalHeader()->sortIndicatorSection() == column
                        && horizontalHeader()->sortIndicatorOrder() == Qt::AscendingOrder
                    ? Qt::DescendingOrder
                    : Qt::AscendingOrder);
    });
    connect(m_pinnedView->horizontalHeader(), &QHeaderView::sectionResized, this, [this](int c, int, int width) {
        if (!m_syncingColumns)
            setColumnWidth(c, width);
    });
    // Fix the stacking order once. Raising this pane during painting hides the
    // border/scrollbars and invalidates the ancestor's composition again.
    // zh_CN: 仅在创建时确定层级，绘制时提层会遮住边框和滚动条并再次触发上层合成。
    if (m_borderOverlay) {
        static_cast<detail::DataGridFrameOverlay*>(m_borderOverlay)->setPinnedView(m_pinnedView);
        m_pinnedView->stackUnder(m_borderOverlay);
    }
    m_verticalFluentScrollBar->raise();
    m_horizontalFluentScrollBar->raise();
}

void DataGrid::syncPinnedColumns()
{
    if (m_syncingColumns || m_changingColumns)
        return;
    QScopedValueRollback<bool> syncing(m_syncingColumns, true);
    if (!model() || m_pinnedColumns.isEmpty()) {
        if (m_pinnedView)
            m_pinnedView->hide();
        return;
    }
    ensurePinnedView();
    if (m_pinnedView->model() != model()) {
        m_pinnedView->setModel(model());
        m_pinnedView->verticalHeader()->restoreState(verticalHeader()->saveState());
        m_pinnedView->verticalHeader()->hide();
        m_pinnedStructureDirty = true;
    }
    if (m_pinnedView->verticalHeader()->defaultSectionSize() != verticalHeader()->defaultSectionSize())
        m_pinnedView->verticalHeader()->setDefaultSectionSize(verticalHeader()->defaultSectionSize());
    if (m_pinnedView->selectionModel() != selectionModel()) {
        auto* previous = m_pinnedView->selectionModel();
        m_pinnedView->setSelectionModel(selectionModel());
        if (previous && previous->parent() == m_pinnedView)
            delete previous;
    }
    if (m_pinnedView->rootIndex() != rootIndex()) {
        m_pinnedView->setRootIndex(rootIndex());
        m_pinnedStructureDirty = true;
    }
    if (m_pinnedView->selectionBehavior() != selectionBehavior())
        m_pinnedView->setSelectionBehavior(selectionBehavior());
    if (m_pinnedView->selectionMode() != QTableView::selectionMode())
        m_pinnedView->setSelectionMode(QTableView::selectionMode());
    if (m_pinnedView->editTriggers() != editTriggers())
        m_pinnedView->setEditTriggers(editTriggers());
    if (m_pinnedView->palette() != palette())
        m_pinnedView->setPalette(palette());
    if (m_pinnedView->font() != font())
        m_pinnedView->setFont(font());
    auto* pinnedHeader = m_pinnedView->horizontalHeader();
    if (pinnedHeader->font() != horizontalHeader()->font())
        pinnedHeader->setFont(horizontalHeader()->font());
    if (pinnedHeader->isSortIndicatorShown() != isSortingEnabled())
        pinnedHeader->setSortIndicatorShown(isSortingEnabled());
    if (pinnedHeader->sortIndicatorSection() != horizontalHeader()->sortIndicatorSection()
        || pinnedHeader->sortIndicatorOrder() != horizontalHeader()->sortIndicatorOrder())
        pinnedHeader->setSortIndicator(horizontalHeader()->sortIndicatorSection(),
            horizontalHeader()->sortIndicatorOrder());
    // Structural changes are signalled by the header/model. A paint or scroll
    // only checks scalar options; the forwarding delegate resolves row/column
    // delegates directly, including calls through inherited nonvirtual setters.
    // zh_CN: 表头和模型通知结构变化，绘制/滚动只同步标量状态；转发代理直接解析行列代理，
    // 保留通过继承的非虚 setter 修改代理的行为，无需每帧扫描所有列和可见行。
    if (m_pinnedHiddenCount != horizontalHeader()->hiddenSectionCount())
        m_pinnedStructureDirty = true;
    if (m_pinnedStructureDirty) {
        const int count = horizontalHeader()->count();
        for (auto it = m_pinnedColumns.begin(); it != m_pinnedColumns.end();) {
            if (*it >= count)
                it = m_pinnedColumns.erase(it);
            else
                ++it;
        }
        const auto pinned = pinnedColumns();
        for (int i = 0; i < pinned.size(); ++i) {
            const int from = horizontalHeader()->visualIndex(pinned.at(i));
            if (from != i)
                horizontalHeader()->moveSection(from, i);
        }
        m_pinnedWidth = 0;
        for (int visual = 0; visual < count; ++visual) {
            const int column = horizontalHeader()->logicalIndex(visual);
            const int from = pinnedHeader->visualIndex(column);
            if (from != visual)
                pinnedHeader->moveSection(from, visual);
            const bool hidden = !m_pinnedColumns.contains(column) || isColumnHidden(column);
            if (m_pinnedView->isColumnHidden(column) != hidden)
                m_pinnedView->setColumnHidden(column, hidden);
            if (!hidden) {
                if (m_pinnedView->columnWidth(column) != columnWidth(column))
                    m_pinnedView->setColumnWidth(column, columnWidth(column));
                m_pinnedWidth += columnWidth(column);
            }
        }
        m_pinnedHiddenCount = horizontalHeader()->hiddenSectionCount();
        m_pinnedStructureDirty = false;
    }
    const QRect area = viewport()->geometry();
    const int width = qMin(m_pinnedWidth, qMax(0, area.width() - ::Spacing::ControlHeight::Large));
    const int x = layoutDirection() == Qt::RightToLeft ? area.right() - width + 1 : area.left();
    const int headerHeight = horizontalHeader()->isVisible() ? horizontalHeader()->height() : 0;
    const int height = headerHeight + (model()->rowCount(rootIndex()) > 0 ? area.height() : 0);
    const QRect geometry(x, horizontalHeader()->isVisible() ? horizontalHeader()->y() : area.y(), width, height);
    const bool visible = width > 0 && height > 0;
    const bool geometryChanged = m_pinnedView->geometry() != geometry;
    const bool visibilityChanged = m_pinnedView->isHidden() == visible;
    if (geometryChanged)
        m_pinnedView->setGeometry(geometry);
    if (pinnedHeader->isHidden() == horizontalHeader()->isVisible())
        pinnedHeader->setVisible(horizontalHeader()->isVisible());
    if (m_pinnedView->verticalScrollBar()->value() != verticalScrollBar()->value())
        m_pinnedView->verticalScrollBar()->setValue(verticalScrollBar()->value());
    if (visibilityChanged)
        m_pinnedView->setVisible(visible);
    if ((geometryChanged || visibilityChanged) && m_borderOverlay)
        m_borderOverlay->update();
}

bool DataGrid::eventFilter(QObject* watched, QEvent* event)
{
    if (m_pinnedView && watched == m_pinnedView->viewport()) {
        if (event->type() == QEvent::Wheel) {
            wheelEvent(static_cast<QWheelEvent*>(event));
            return true;
        }
        if (event->type() == QEvent::MouseButtonPress) {
            m_keyboardFocusVisible = false;
            updateFocusPresentation();
        } else if (event->type() == QEvent::MouseMove)
            setHoveredRow(m_pinnedView->indexAt(fluentMousePos(static_cast<QMouseEvent*>(event))).row());
        else if (event->type() == QEvent::Leave)
            setHoveredRow(-1);
    }
    return QTableView::eventFilter(watched, event);
}

void DataGrid::scrollTo(const QModelIndex& index, ScrollHint hint)
{
    const int previous = horizontalScrollBar()->value();
    QTableView::scrollTo(index, hint);
    if (!m_pinnedView || !m_pinnedView->isVisible())
        return;
    if (isColumnPinned(index.column())) {
        horizontalScrollBar()->setValue(previous);
        m_pinnedView->scrollTo(index, hint);
    } else {
        const QRect cell = QTableView::visualRect(index);
        if (layoutDirection() == Qt::LeftToRight && cell.left() < m_pinnedView->width())
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() + cell.left() - m_pinnedView->width());
        else if (layoutDirection() == Qt::RightToLeft && cell.right() > viewport()->width() - m_pinnedView->width())
            horizontalScrollBar()->setValue(
                horizontalScrollBar()->value() + viewport()->width() - m_pinnedView->width() - cell.right());
    }
}

QRect DataGrid::visualRect(const QModelIndex& index) const
{
    if (m_pinnedView && m_pinnedView->isVisible() && isColumnPinned(index.column())) {
        return m_pinnedView->visualRect(index).translated(
            m_pinnedView->viewport()->mapTo(this, QPoint()) - viewport()->pos());
    }
    return QTableView::visualRect(index);
}

QModelIndex DataGrid::indexAt(const QPoint& point) const
{
    if (m_pinnedView && m_pinnedView->isVisible()) {
        const QPoint mapped = point + viewport()->pos() - m_pinnedView->viewport()->mapTo(this, QPoint());
        if (m_pinnedView->viewport()->rect().contains(mapped))
            return m_pinnedView->indexAt(mapped);
    }
    return QTableView::indexAt(point);
}

void DataGrid::setRootIndex(const QModelIndex& index)
{
    QTableView::setRootIndex(index);
    m_pinnedStructureDirty = true;
    syncPinnedColumns();
}

void DataGrid::setItemDelegate(QAbstractItemDelegate* delegate)
{
    QTableView::setItemDelegate(delegate);
    syncPinnedColumns();
}

bool DataGrid::edit(const QModelIndex& index, EditTrigger trigger, QEvent* event)
{
    if (m_pinnedView && m_pinnedView->isVisible() && isColumnPinned(index.column())) {
        // Qt distinguishes explicit edit() from user triggers and forwards the
        // initiating key to the editor. Preserve both in the visible view.
        // zh_CN: 保留 Qt 的程序编辑语义和首个输入事件，在实际可见的视图中执行。
        m_pinnedView->setEditTriggers(editTriggers());
        return static_cast<detail::DataGridPinnedView*>(m_pinnedView)->editCell(index, trigger, event);
    }
    return QTableView::edit(index, trigger, event);
}

} // namespace fluent::collections
