#include "CollectionSampleDelegates.h"

#include <functional>

#include <QAbstractItemView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>

#include "compatibility/QtCompat.h"
#include "components/collections/GridView.h"
#include "components/collections/ListView.h"
#include "components/collections/TreeView.h"
#include "components/foundation/FluentElement.h"
#include "design/CornerRadius.h"
#include "design/Spacing.h"
#include "design/Typography.h"

namespace fluent::gallery {

using fluent::collections::GridView;
using fluent::collections::ListView;
using fluent::collections::TreeView;

namespace {

void drawCoverPixmap(QPainter* painter, const QRectF& target, const QPixmap& pixmap)
{
    const QSizeF sourceSize = pixmap.size() / pixmap.devicePixelRatio();
    if (sourceSize.isEmpty())
        return;
    const qreal scale = qMax(target.width() / sourceSize.width(),
                             target.height() / sourceSize.height());
    const QSizeF visible(target.width() / scale, target.height() / scale);
    const QRectF source((sourceSize.width() - visible.width()) / 2.0,
                        (sourceSize.height() - visible.height()) / 2.0,
                        visible.width(), visible.height());
    painter->drawPixmap(target, pixmap, source);
}

} // namespace

// ════════════════════════════════════════════════════════════════════════════
// GridPhotoDelegate
// ════════════════════════════════════════════════════════════════════════════

GridPhotoDelegate::GridPhotoDelegate(fluent::FluentElement* themeHost, GridView* view,
                                     QObject* parent)
    : QStyledItemDelegate(parent), m_themeHost(themeHost), m_view(view)
{
}

void GridPhotoDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const
{
    if (!index.isValid())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->setRenderHint(QPainter::TextAntialiasing);

    fluent::FluentElement::Colors colors{};
    if (m_themeHost)
        colors = m_themeHost->themeColors();
    const QColor layer = colors.bgLayerAlt.isValid() ? colors.bgLayerAlt : QColor(250, 250, 250);
    const QColor stroke = colors.strokeDefault.isValid() ? colors.strokeDefault : QColor(220, 220, 220);
    const QColor accent = colors.accentDefault.isValid() ? colors.accentDefault : QColor(0, 120, 212);

    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isHovered = option.state & QStyle::State_MouseOver;
    const bool isEnabled = option.state & QStyle::State_Enabled;
    const bool isMultiSel = m_view
        && (m_view->selectionMode() == GridView::GridSelectionMode::Multiple
            || m_view->selectionMode() == GridView::GridSelectionMode::Extended);

    const QRectF card = QRectF(option.rect).adjusted(2.0, 2.0, -2.0, -2.0);
    const int radius = CornerRadius::Control;
    QPainterPath clip;
    clip.addRoundedRect(card, radius, radius);
    painter->fillPath(clip, layer);

    const QVariant imageData = index.data(PhotoImageRole);
    const QPixmap pixmap = imageData.canConvert<QPixmap>() ? imageData.value<QPixmap>() : QPixmap();
    if (!pixmap.isNull()) {
        painter->setClipPath(clip);
        drawCoverPixmap(painter, card, pixmap);
        if (isHovered)
            painter->fillRect(card, QColor(255, 255, 255, 24));

        const QString title = index.data(Qt::DisplayRole).toString();
        const QString subtitle = index.data(PhotoSubtitleRole).toString();
        if (!title.isEmpty()) {
            const QRectF labelBar(card.left(), card.bottom() - 44.0, card.width(), 44.0);
            QLinearGradient scrim(labelBar.topLeft(), labelBar.bottomLeft());
            scrim.setColorAt(0.0, QColor(0, 0, 0, 20));
            scrim.setColorAt(1.0, QColor(0, 0, 0, 150));
            painter->fillRect(labelBar, scrim);

            QFont titleFont = option.font;
            titleFont.setWeight(QFont::DemiBold);
            painter->setFont(titleFont);
            painter->setPen(QColor(255, 255, 255, 240));
            painter->drawText(labelBar.adjusted(10, 4, -10, -21),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              painter->fontMetrics().elidedText(title, Qt::ElideRight,
                                                                 qRound(labelBar.width()) - 20));
            if (!subtitle.isEmpty()) {
                QFont subtitleFont = option.font;
                subtitleFont.setPixelSize(qMax(10, subtitleFont.pixelSize() - 2));
                painter->setFont(subtitleFont);
                painter->setPen(QColor(255, 255, 255, 205));
                painter->drawText(labelBar.adjusted(10, 22, -10, -4),
                                  Qt::AlignLeft | Qt::AlignVCenter,
                                  painter->fontMetrics().elidedText(subtitle, Qt::ElideRight,
                                                                     qRound(labelBar.width()) - 20));
            }
        }
        painter->setClipping(false);
    }

    // Accent selection border.
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(isSelected ? accent : stroke, isSelected ? 2.0 : 1.0));
    painter->drawPath(clip);

    // Top-right check overlay for multi-selection grids (WinUI 3 affordance).
    if (isMultiSel)
        drawCheckOverlay(painter, card, isSelected, isEnabled);

    painter->restore();
}

void GridPhotoDelegate::drawCheckOverlay(QPainter* painter, const QRectF& card,
                                         bool selected, bool enabled) const
{
    fluent::FluentElement::Colors colors{};
    if (m_themeHost)
        colors = m_themeHost->themeColors();
    const QColor accent = colors.accentDefault.isValid() ? colors.accentDefault : QColor(0, 120, 212);

    constexpr qreal kSize = 22.0;
    constexpr qreal kMargin = 7.0;
    const QRectF checkRect(card.right() - kSize - kMargin, card.top() + kMargin, kSize, kSize);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (selected && enabled) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(accent);
        painter->drawEllipse(checkRect);

        QFont checkFont(Typography::FontFamily::SegoeFluentIcons);
        checkFont.setPixelSize(12);
        painter->setFont(checkFont);
        painter->setPen(Qt::white);
        painter->drawText(checkRect, Qt::AlignCenter, Typography::Icons::CheckMark);
    } else {
        painter->setPen(QPen(QColor(255, 255, 255, 200), 1.5));
        painter->setBrush(QColor(0, 0, 0, 60));
        painter->drawEllipse(checkRect.adjusted(0.75, 0.75, -0.75, -0.75));
    }
    painter->restore();
}

QSize GridPhotoDelegate::sizeHint(const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const
{
    if (m_view)
        return m_view->gridSize();
    const QVariant size = index.data(Qt::SizeHintRole);
    if (size.canConvert<QSize>())
        return size.toSize();
    return QStyledItemDelegate::sizeHint(option, index);
}

// ════════════════════════════════════════════════════════════════════════════
// ListRowDelegate
// ════════════════════════════════════════════════════════════════════════════

ListRowDelegate::ListRowDelegate(fluent::FluentElement* themeHost, ListView* view,
                                 QObject* parent)
    : QStyledItemDelegate(parent), m_themeHost(themeHost), m_view(view)
{
}

void ListRowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const
{
    if (!index.isValid())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    fluent::FluentElement::Colors colors{};
    fluent::FluentElement::Radius radius{};
    if (m_themeHost) {
        colors = m_themeHost->themeColors();
        radius = m_themeHost->themeRadius();
    }
    const int cornerR = radius.control > 0 ? radius.control : CornerRadius::Control;

    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isHovered = option.state & QStyle::State_MouseOver;
    const bool isPressed = (option.state & QStyle::State_Sunken) && isHovered;
    const bool isEnabled = option.state & QStyle::State_Enabled;

    // Background rect matches the container's indicator base rect so the accent pill,
    // painted by ListView on top, lines up with the rounded fill we draw here.
    const QRectF bgRect = QRectF(option.rect).adjusted(2.0, 1.0, -2.0, -1.0);

    QColor bgColor = Qt::transparent;
    if (isEnabled) {
        if (isPressed)
            bgColor = colors.subtleTertiary;
        else if (isSelected || isHovered)
            bgColor = colors.subtleSecondary;
    }
    if (bgColor.alpha() > 0) {
        QPainterPath path;
        path.addRoundedRect(bgRect, cornerR, cornerR);
        painter->setPen(Qt::NoPen);
        painter->setBrush(bgColor);
        painter->drawPath(path);
    }

    // Left padding clears the accent indicator pill (drawn by ListView at bgRect.left()+4).
    qreal cursorX = bgRect.left() + 14.0;

    // Resolve the row icon. QStandardItem::setIcon stores a QIcon in DecorationRole;
    // a QPixmap variant also converts cleanly, so the QIcon path covers both. The avatar /
    // glyph pixmaps already carry their own (circular or rounded-tile) alpha, so draw them
    // as-is rather than re-clipping.
    QSize extent = option.decorationSize;
    if (!extent.isValid() || extent.isEmpty())
        extent = m_view ? m_view->iconSize() : QSize(24, 24);
    if (!extent.isValid() || extent.isEmpty())
        extent = QSize(24, 24);
    const QVariant decoration = index.data(Qt::DecorationRole);
    QPixmap iconPixmap;
    if (decoration.canConvert<QIcon>()) {
        const QIcon icon = decoration.value<QIcon>();
        if (!icon.isNull())
            iconPixmap = icon.pixmap(extent);
    } else if (decoration.canConvert<QPixmap>()) {
        iconPixmap = decoration.value<QPixmap>();
    }
    if (!iconPixmap.isNull()) {
        const QRect iconRect(qRound(cursorX), qRound(bgRect.center().y() - extent.height() / 2.0),
                             extent.width(), extent.height());
        painter->drawPixmap(iconRect, iconPixmap);
        cursorX = iconRect.right() + 12.0;
    }

    const QString text = index.data(Qt::DisplayRole).toString();
    const QRectF textRect(cursorX, bgRect.top(), bgRect.right() - cursorX - 8.0, bgRect.height());
    painter->setPen(isEnabled ? colors.textPrimary : colors.textDisabled);
    QFont font = option.font;
    if (isSelected)
        font.setWeight(QFont::DemiBold);
    painter->setFont(font);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(text, Qt::ElideRight, int(textRect.width())));

    painter->restore();
}

QSize ListRowDelegate::sizeHint(const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    QSize hint = QStyledItemDelegate::sizeHint(option, index);
    const int minRow = Spacing::ControlHeight::Standard + Spacing::Gap::Tight;
    hint.setHeight(qMax(hint.height(), minRow));
    // Account for the indicator padding + icon gap added in paint().
    hint.setWidth(hint.width() + 26);
    return hint;
}

// ════════════════════════════════════════════════════════════════════════════
// TreeRowDelegate
// ════════════════════════════════════════════════════════════════════════════

namespace {
constexpr qreal kChevronAreaW = 20.0;
constexpr qreal kCheckBoxAreaW = 22.0;
constexpr qreal kIconAreaW = 22.0;
constexpr qreal kGap = 4.0;
constexpr qreal kCursorStart = 12.0;
}

TreeRowDelegate::TreeRowDelegate(fluent::FluentElement* themeHost, int rowHeight,
                                 TreeView* view, QObject* parent)
    : QStyledItemDelegate(parent), m_themeHost(themeHost), m_rowHeight(rowHeight), m_view(view)
{
}

QRectF TreeRowDelegate::bgRectForOption(const QStyleOptionViewItem& option) const
{
    const int vpWidth = m_view && m_view->viewport() ? m_view->viewport()->width()
                                                     : option.rect.right();
    return QRectF(2, option.rect.top() + 2, vpWidth - 4, option.rect.height() - 4);
}

QRectF TreeRowDelegate::checkBoxRectForOption(const QStyleOptionViewItem& option) const
{
    if (!m_checkBoxVisible)
        return {};
    const QRectF bg = bgRectForOption(option);
    return QRectF(qreal(option.rect.left()) + kCursorStart, bg.top(), kCheckBoxAreaW, bg.height());
}

QRectF TreeRowDelegate::chevronRectForOption(const QStyleOptionViewItem& option) const
{
    qreal x = qreal(option.rect.left()) + kCursorStart;
    if (m_checkBoxVisible)
        x += kCheckBoxAreaW + kGap;
    const QRectF bg = bgRectForOption(option);
    return QRectF(x, bg.top(), kChevronAreaW, bg.height());
}

void TreeRowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const
{
    if (!index.isValid())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    fluent::FluentElement::Colors colors{};
    fluent::FluentElement::Radius radius{};
    if (m_themeHost) {
        colors = m_themeHost->themeColors();
        radius = m_themeHost->themeRadius();
    }
    const int cornerR = radius.control > 0 ? radius.control : 4;
    const QRectF bgRect = bgRectForOption(option);

    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isHovered = option.state & QStyle::State_MouseOver;
    const bool isPressed = (option.state & QStyle::State_Sunken) && isHovered;
    const bool isEnabled = option.state & QStyle::State_Enabled;

    QColor bgColor = Qt::transparent;
    QColor textColor = colors.textPrimary;
    if (!isEnabled)
        textColor = colors.textDisabled;
    else if (isPressed)
        bgColor = colors.subtleTertiary;
    else if (isSelected || isHovered)
        bgColor = colors.subtleSecondary;

    if (bgColor.alpha() > 0) {
        QPainterPath path;
        path.addRoundedRect(bgRect, cornerR, cornerR);
        painter->setPen(Qt::NoPen);
        painter->setBrush(bgColor);
        painter->drawPath(path);
    }

    // Animated accent indicator (single-select). In checkbox mode the box is the affordance.
    if (!m_checkBoxVisible && isSelected && isEnabled && colors.accentDefault.isValid()) {
        const qreal accentT = m_view ? qBound(0.0, m_view->selectedIndicatorProgress(index), 1.0) : 1.0;
        const bool activeMotion = m_view && m_view->isIndicatorMotionActiveForIndex(index);
        const auto direction = activeMotion ? m_view->indicatorMotionDirection()
                                            : TreeView::IndicatorVerticalDirection::None;
        const auto hierarchy = activeMotion ? m_view->indicatorHierarchyTransition()
                                            : TreeView::IndicatorHierarchyTransition::None;
        const qreal indicatorW = 3.0;
        const qreal fullH = 16.0;
        const qreal indicatorH = fullH * (0.35 + 0.65 * accentT);
        const qreal settledX = qreal(option.rect.left()) + 4.0;
        const qreal settledY = bgRect.center().y() - fullH / 2.0;
        const qreal remaining = 1.0 - accentT;

        qreal indicatorX = settledX;
        if (hierarchy == TreeView::IndicatorHierarchyTransition::Inward)
            indicatorX += remaining * 4.0;
        else if (hierarchy == TreeView::IndicatorHierarchyTransition::Outward)
            indicatorX -= remaining * 3.0;

        qreal indicatorY = bgRect.center().y() - indicatorH / 2.0;
        if (direction == TreeView::IndicatorVerticalDirection::Down)
            indicatorY = settledY - remaining * 6.0;
        else if (direction == TreeView::IndicatorVerticalDirection::Up)
            indicatorY = settledY + (fullH - indicatorH) + remaining * 6.0;

        QPainterPath indicatorPath;
        indicatorPath.addRoundedRect(QRectF(indicatorX, indicatorY, indicatorW, indicatorH),
                                     indicatorW / 2.0, indicatorW / 2.0);
        QColor ac = colors.accentDefault;
        ac.setAlphaF(ac.alphaF() * accentT);
        painter->setPen(Qt::NoPen);
        painter->setBrush(ac);
        painter->drawPath(indicatorPath);
    }

    qreal cursorX = qreal(option.rect.left()) + kCursorStart;

    // Tri-state checkbox (multi-select).
    if (m_checkBoxVisible) {
        const QRectF cbArea(cursorX, bgRect.top(), kCheckBoxAreaW, bgRect.height());
        const QVariant checkData = index.data(Qt::CheckStateRole);
        const auto state = checkData.isValid() ? static_cast<Qt::CheckState>(checkData.toInt())
                                               : Qt::Unchecked;
        const qreal box = 18.0;
        const QRectF boxRect(cbArea.center().x() - box / 2.0, cbArea.center().y() - box / 2.0, box, box);
        QPainterPath boxPath;
        boxPath.addRoundedRect(boxRect, 3.0, 3.0);
        if (state == Qt::Checked || state == Qt::PartiallyChecked) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(colors.accentDefault);
            painter->drawPath(boxPath);
            QFont glyphFont(Typography::FontFamily::SegoeFluentIcons);
            glyphFont.setPixelSize(12);
            painter->setFont(glyphFont);
            painter->setPen(Qt::white);
            painter->drawText(boxRect, Qt::AlignCenter,
                              state == Qt::Checked ? Typography::Icons::CheckMark
                                                   : Typography::Icons::Hyphen);
        } else {
            painter->setPen(QPen(colors.strokeDefault, 1.5));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(boxPath);
        }
        cursorX += kCheckBoxAreaW + kGap;
    }

    // Rotating chevron for parents.
    const QAbstractItemModel* m = index.model();
    const bool hasChildren = m && m->hasChildren(index);
    const qreal chevronLeft = cursorX;
    if (hasChildren) {
        const qreal rotation = m_view ? m_view->chevronRotation(index) : 0.0;
        QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
        iconFont.setPixelSize(12);
        painter->setFont(iconFont);
        painter->setPen(textColor);
        const QRectF chevronRect(chevronLeft, bgRect.top(), kChevronAreaW, bgRect.height());
        painter->save();
        painter->translate(chevronRect.center());
        painter->rotate(rotation * 90.0);
        painter->translate(-chevronRect.center());
        painter->drawText(chevronRect, Qt::AlignCenter, Typography::Icons::ChevronRightMed);
        painter->restore();
    }
    cursorX = chevronLeft + kChevronAreaW + kGap;

    // Per-row icon glyph.
    const QString glyph = index.data(TreeIconGlyphRole).toString();
    if (!glyph.isEmpty()) {
        QColor glyphColor = textColor;
        const QVariant colorVar = index.data(TreeIconColorRole);
        if (colorVar.canConvert<QColor>() && colorVar.value<QColor>().isValid() && isEnabled)
            glyphColor = colorVar.value<QColor>();
        QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
        iconFont.setPixelSize(16);
        painter->setFont(iconFont);
        painter->setPen(glyphColor);
        painter->drawText(QRectF(cursorX, bgRect.top(), kIconAreaW, bgRect.height()),
                          Qt::AlignCenter, glyph);
        cursorX += kIconAreaW + kGap;
    }

    // Text.
    const QRectF textRect(cursorX, bgRect.top(), bgRect.right() - cursorX - 8.0, bgRect.height());
    painter->setPen(textColor);
    painter->setFont(option.font);
    const QString text = index.data(Qt::DisplayRole).toString();
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(text, Qt::ElideRight, int(textRect.width())));

    painter->restore();
}

bool TreeRowDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                  const QStyleOptionViewItem& option, const QModelIndex& index)
{
    if (event->type() == QEvent::MouseButtonPress && m_checkBoxVisible) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (checkBoxRectForOption(option).contains(fluentMousePos(me)))
            return true;  // Swallow press so the row doesn't also select.
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        const QPointF pos = fluentMousePos(me);

        if (m_checkBoxVisible && checkBoxRectForOption(option).contains(pos)) {
            const QVariant checkData = index.data(Qt::CheckStateRole);
            const auto cur = checkData.isValid() ? static_cast<Qt::CheckState>(checkData.toInt())
                                                 : Qt::Unchecked;
            const Qt::CheckState next = (cur == Qt::Checked) ? Qt::Unchecked : Qt::Checked;

            model->setData(index, next, Qt::CheckStateRole);
            // Cascade down to every descendant.
            std::function<void(const QModelIndex&)> cascade = [&](const QModelIndex& parent) {
                for (int r = 0; r < model->rowCount(parent); ++r) {
                    const QModelIndex child = model->index(r, 0, parent);
                    model->setData(child, next, Qt::CheckStateRole);
                    cascade(child);
                }
            };
            cascade(index);
            // Roll the tri-state up through ancestors.
            std::function<void(const QModelIndex&)> rollUp = [&](const QModelIndex& child) {
                const QModelIndex parent = child.parent();
                if (!parent.isValid())
                    return;
                int checked = 0, unchecked = 0;
                const int rows = model->rowCount(parent);
                for (int r = 0; r < rows; ++r) {
                    const QVariant v = model->index(r, 0, parent).data(Qt::CheckStateRole);
                    const auto st = v.isValid() ? static_cast<Qt::CheckState>(v.toInt()) : Qt::Unchecked;
                    if (st == Qt::Checked)
                        ++checked;
                    else if (st == Qt::Unchecked)
                        ++unchecked;
                }
                Qt::CheckState parentState = Qt::PartiallyChecked;
                if (checked == rows)
                    parentState = Qt::Checked;
                else if (unchecked == rows)
                    parentState = Qt::Unchecked;
                model->setData(parent, parentState, Qt::CheckStateRole);
                rollUp(parent);
            };
            rollUp(index);
            return true;
        }

        // A click anywhere on a parent row toggles its expansion (not only the chevron),
        // matching file-explorer behavior. Leaf rows fall through to normal selection; the
        // checkbox area was already handled above, and a real reorder drag is consumed by
        // TreeView before it reaches the delegate, so this only fires on genuine clicks.
        if (index.model() && index.model()->hasChildren(index)
            && bgRectForOption(option).contains(pos)) {
            if (m_view)
                m_view->toggleExpanded(index);
            return true;
        }
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QSize TreeRowDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                                const QModelIndex& /*index*/) const
{
    return QSize(0, m_rowHeight);
}

} // namespace fluent::gallery
