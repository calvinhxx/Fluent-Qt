#include "CollectionSampleDelegates.h"

#include <functional>

#include <QAbstractItemView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPaintDevice>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QtGlobal>

#include "compatibility/QtCompat.h"
#include "compatibility/TextPaintCompat.h"
#include "components/collections/GridView.h"
#include "components/collections/ListView.h"
#include "components/collections/TreeView.h"
#include "components/foundation/FluentElement.h"
#include "design/CornerRadius.h"
#include "design/Spacing.h"
#include "design/Typography.h"

namespace fluent::gallery {

void drawPhotoCaption(QPainter* painter, const QRectF& card,
                      const QString& title, const QString& subtitle,
                      const QFont& baseFont)
{
    if (!painter || title.isEmpty())
        return;

    QFont titleFont = baseFont;
    titleFont.setWeight(QFont::DemiBold);
    QFont subtitleFont = baseFont;
    if (subtitleFont.pixelSize() > 0)
        subtitleFont.setPixelSize(qMax(11, subtitleFont.pixelSize() - 2));
    else if (subtitleFont.pointSizeF() > 0.0)
        subtitleFont.setPointSizeF(qMax<qreal>(8.0, subtitleFont.pointSizeF() - 1.0));

    const QFontMetricsF titleMetrics(titleFont);
    const QFontMetricsF subtitleMetrics(subtitleFont);
    const qreal gap = subtitle.isEmpty() ? 0.0 : 1.0;
    const qreal textHeight = titleMetrics.height()
        + (subtitle.isEmpty() ? 0.0 : subtitleMetrics.height() + gap);
    const qreal barHeight = qMin(card.height(), qMax<qreal>(48.0, textHeight + 10.0));
    const QRectF labelBar(card.left(), card.bottom() - barHeight,
                          card.width(), barHeight);

    QLinearGradient scrim(labelBar.topLeft(), labelBar.bottomLeft());
    scrim.setColorAt(0.0, QColor(0, 0, 0, 20));
    scrim.setColorAt(1.0, QColor(0, 0, 0, 150));
    painter->fillRect(labelBar, scrim);

    const qreal textTop = labelBar.top() + (labelBar.height() - textHeight) / 2.0;
    const QRectF titleRect(labelBar.left() + 10.0, textTop,
                           qMax<qreal>(0.0, labelBar.width() - 20.0),
                           titleMetrics.height());
    painter->setFont(titleFont);
    painter->setPen(QColor(255, 255, 255, 240));
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                      titleMetrics.elidedText(title, Qt::ElideRight,
                                              qRound(titleRect.width())));

    if (!subtitle.isEmpty()) {
        const QRectF subtitleRect(titleRect.left(), titleRect.bottom() + gap,
                                  titleRect.width(), subtitleMetrics.height());
        painter->setFont(subtitleFont);
        painter->setPen(QColor(255, 255, 255, 205));
        painter->drawText(subtitleRect,
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                          subtitleMetrics.elidedText(subtitle, Qt::ElideRight,
                                                     qRound(subtitleRect.width())));
    }
}

using fluent::collections::GridView;
using fluent::collections::ListView;
using fluent::collections::TreeView;

namespace {

// Shared all-invalid color set, bound by const-ref when a delegate has no theme host, so the paint
// hot paths can read colors via themeColorsRef() without copying the ~50-QColor Colors struct per
// item. The .isValid() guards at each use site fall back to literals for every field.
// zh_CN: 共享的全无效色板;delegate 无主题宿主时按 const 引用绑定，使绘制热路径可经 themeColorsRef() 读色而不必每项
// 拷贝整个 ~50 个 QColor 的 Colors 结构体。各使用点的 .isValid() 守卫会对每个字段回退到字面量。
const fluent::FluentElement::Colors& emptyColors()
{
    static const fluent::FluentElement::Colors kEmpty{};
    return kEmpty;
}

qreal painterDevicePixelRatio(const QPainter* painter)
{
    if (painter && painter->device())
        return qMax<qreal>(1.0, painter->device()->devicePixelRatioF());
    return 1.0;
}

QPixmap iconPixmapForExtent(const QIcon& icon, const QSize& extent, const QPainter* painter)
{
    QWidget* targetWidget = painter && painter->device()
        ? dynamic_cast<QWidget*>(painter->device())
        : nullptr;
    QWindow* targetWindow = targetWidget && targetWidget->window()
        ? targetWidget->window()->windowHandle()
        : nullptr;
    return fluentIconPixmapForLogicalExtent(
        icon, extent, painterDevicePixelRatio(painter), targetWindow);
}

void drawCoverPixmap(QPainter* painter, const QRectF& target, const QPixmap& pixmap)
{
    if (!painter)
        return;
    fluentDrawCoverPixmapInLogicalRect(*painter, target, pixmap);
}

// Fills a rounded-rect background when the color is visible. zh_CN: 颜色可见时填充圆角矩形背景。
void fillRoundedBackground(QPainter* painter, const QRectF& rect, const QColor& color, qreal radius)
{
    if (!color.isValid() || color.alpha() <= 0)
        return;
    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawPath(path);
}

QColor rowSelectionFill(const QStyleOptionViewItem& option,
                        const fluent::FluentElement::Colors& colors)
{
    if (!(option.state & QStyle::State_Enabled))
        return Qt::transparent;

    const bool hovered = option.state & QStyle::State_MouseOver;
    const bool pressed = (option.state & QStyle::State_Sunken) && hovered;
    const bool selected = option.state & QStyle::State_Selected;
    if (pressed)
        return colors.subtleTertiary;
    if (selected || hovered)
        return colors.subtleSecondary;
    return Qt::transparent;
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

    const fluent::FluentElement::Colors& colors =
        m_themeHost ? m_themeHost->themeColorsRef() : emptyColors();
    const QColor layer = colors.bgLayerAlt.isValid() ? colors.bgLayerAlt : QColor(250, 250, 250);
    const QColor stroke = colors.strokeDefault.isValid() ? colors.strokeDefault : QColor(220, 220, 220);
    const QColor accent = colors.accentDefault.isValid() ? colors.accentDefault : QColor(0, 120, 212);

    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isHovered = option.state & QStyle::State_MouseOver;
    const bool isEnabled = option.state & QStyle::State_Enabled;
    const bool isMultiSel = m_view
        && (m_view->selectionMode() == GridView::SelectionMode::Multiple
            || m_view->selectionMode() == GridView::SelectionMode::Extended);

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
        drawPhotoCaption(painter, card, title, subtitle, option.font);
        painter->setClipping(false);
    }
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
    const fluent::FluentElement::Colors& colors =
        m_themeHost ? m_themeHost->themeColorsRef() : emptyColors();
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

        QFont checkFont = Typography::Icons::font(Typography::IconSize::Compact);
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

    const fluent::FluentElement::Colors& colors =
        m_themeHost ? m_themeHost->themeColorsRef() : emptyColors();
    fluent::FluentElement::Radius radius{};
    if (m_themeHost)
        radius = m_themeHost->themeRadius();
    const int cornerR = radius.control > 0 ? radius.control : CornerRadius::Control;

    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isEnabled = option.state & QStyle::State_Enabled;

    // Background rect matches the container's indicator base rect so the accent pill,
    // painted by ListView on top, lines up with the rounded fill we draw here.
    const QRectF bgRect = QRectF(option.rect).adjusted(2.0, 1.0, -2.0, -1.0);

    const QColor fill = rowSelectionFill(option, colors);
    fillRoundedBackground(painter, bgRect, fill, cornerR);

    // Left padding clears the accent indicator pill (drawn by ListView at bgRect.left()+4).
    qreal cursorX = bgRect.left() + 14.0;

    // Resolve the row icon. Prefer a direct QPixmap so Qt5 keeps the pixmap DPR instead of asking
    // QIcon to synthesize a low-DPI variant; the QIcon path is still kept for ordinary icon models.
    // zh_CN: 优先使用直接的 QPixmap，让 Qt5 保留 DPR，避免 QIcon 重新合成低分辨率版本；普通图标模型仍走 QIcon。
    QSize extent = option.decorationSize;
    if (!extent.isValid() || extent.isEmpty())
        extent = m_view ? m_view->iconSize() : QSize(24, 24);
    if (!extent.isValid() || extent.isEmpty())
        extent = QSize(24, 24);
    const QVariant decoration = index.data(Qt::DecorationRole);
    QPixmap iconPixmap;
    if (decoration.canConvert<QPixmap>()) {
        iconPixmap = decoration.value<QPixmap>();
    } else if (decoration.canConvert<QIcon>()) {
        const QIcon icon = decoration.value<QIcon>();
        if (!icon.isNull())
            iconPixmap = iconPixmapForExtent(icon, extent, painter);
    }
    if (!iconPixmap.isNull()) {
        const QRect iconRect(qRound(cursorX), qRound(bgRect.center().y() - extent.height() / 2.0),
                             extent.width(), extent.height());
        painter->drawPixmap(iconRect, iconPixmap);
        cursorX = iconRect.right() + 12.0;
    }

    const QString text = index.data(Qt::DisplayRole).toString();
    const QRectF textSlot(cursorX, bgRect.top(), bgRect.right() - cursorX - 8.0, bgRect.height());
    const QColor rowTextColor = isEnabled ? colors.textPrimary : colors.textDisabled;
    painter->setPen(rowTextColor);
    QFont font = option.font;
    if (isSelected)
        font.setWeight(QFont::DemiBold);
    painter->setFont(font);
    const QFontMetricsF metrics(painter->font());
    const QString elidedText = metrics.elidedText(
        text, Qt::ElideRight, qRound(textSlot.width()));
    const QRectF textRect = fluent::painting::verticallyCenteredTextInkRect(
        textSlot, metrics, elidedText);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                      elidedText);

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

qreal treeRowLeadingEdge(const QStyleOptionViewItem& option)
{
    return option.direction == Qt::RightToLeft
        ? qreal(option.rect.x() + option.rect.width())
        : qreal(option.rect.x());
}

QRectF treeRowRectFromLeading(const QStyleOptionViewItem& option, qreal offset,
                              qreal width, qreal top, qreal height)
{
    const qreal leading = treeRowLeadingEdge(option);
    const qreal x = option.direction == Qt::RightToLeft
        ? leading - offset - width
        : leading + offset;
    return QRectF(x, top, width, height);
}
} // namespace

TreeRowDelegate::TreeRowDelegate(fluent::FluentElement* themeHost, int rowHeight,
                                 TreeView* view, QObject* parent)
    : QStyledItemDelegate(parent), m_themeHost(themeHost), m_rowHeight(rowHeight), m_view(view)
{
}

QRectF TreeRowDelegate::bgRectForOption(const QStyleOptionViewItem& option) const
{
    // Span from the indented logical-leading edge to the opposite viewport edge, so the
    // rounded highlight follows hierarchy depth in both LTR and RTL layouts.
    // zh_CN: 从缩进后的逻辑起始边延伸到视口另一侧，使圆角高亮在 LTR 与 RTL
    // 布局中都随层级正确缩进。
    const qreal viewportWidth = m_view && m_view->viewport()
        ? qreal(m_view->viewport()->width())
        : qreal(option.rect.x() + option.rect.width());
    const qreal top = option.rect.top() + 2.0;
    const qreal height = option.rect.height() - 4.0;
    if (option.direction == Qt::RightToLeft) {
        const qreal right = qreal(option.rect.x() + option.rect.width()) - 2.0;
        return QRectF(2.0, top, qMax<qreal>(0.0, right - 2.0), height);
    }

    const qreal left = option.rect.left() + 2.0;
    return QRectF(left, top, qMax<qreal>(0.0, viewportWidth - 2.0 - left), height);
}

QRectF TreeRowDelegate::checkBoxRectForOption(const QStyleOptionViewItem& option) const
{
    if (!m_checkBoxVisible)
        return {};
    const QRectF bg = bgRectForOption(option);
    return treeRowRectFromLeading(option, kCursorStart, kCheckBoxAreaW,
                                  bg.top(), bg.height());
}

QRectF TreeRowDelegate::chevronRectForOption(const QStyleOptionViewItem& option) const
{
    const QRectF bg = bgRectForOption(option);
    const qreal offset = kCursorStart
        + (m_checkBoxVisible ? kCheckBoxAreaW + kGap : 0.0);
    return treeRowRectFromLeading(option, offset, kChevronAreaW,
                                  bg.top(), bg.height());
}

void TreeRowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const
{
    if (!index.isValid())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const fluent::FluentElement::Colors& colors =
        m_themeHost ? m_themeHost->themeColorsRef() : emptyColors();
    fluent::FluentElement::Radius radius{};
    if (m_themeHost)
        radius = m_themeHost->themeRadius();
    const int cornerR = radius.control > 0 ? radius.control : 4;
    const QRectF bgRect = bgRectForOption(option);

    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isEnabled = option.state & QStyle::State_Enabled;

    const QColor fill = rowSelectionFill(option, colors);

    QColor textColor = colors.textPrimary;
    if (!isEnabled)
        textColor = colors.textDisabled;

    fillRoundedBackground(painter, bgRect, fill, cornerR);

    // Animated accent indicator (single-select). When TreeView owns the overlay
    // indicator, skip the delegate bar so examples do not show two indicators.
    const bool treeOverlayIndicatorVisible = m_view && m_view->selectionIndicatorVisible();
    if (!treeOverlayIndicatorVisible && !m_checkBoxVisible && isSelected
        && isEnabled && colors.accentDefault.isValid()) {
        const qreal accentT = m_view ? qBound(0.0, m_view->selectedIndicatorProgress(index), 1.0) : 1.0;
        const bool activeMotion = m_view && m_view->isIndicatorMotionActiveForIndex(index);
        const auto direction = activeMotion ? m_view->indicatorMotionDirection()
                                            : TreeView::IndicatorVerticalDirection::None;
        const auto hierarchy = activeMotion ? m_view->indicatorHierarchyTransition()
                                            : TreeView::IndicatorHierarchyTransition::None;
        const qreal indicatorW = 3.0;
        const qreal fullH = 16.0;
        const qreal indicatorH = fullH * (0.35 + 0.65 * accentT);
        const bool rtl = option.direction == Qt::RightToLeft;
        const qreal settledX = rtl
            ? qreal(option.rect.x() + option.rect.width()) - 4.0 - indicatorW
            : qreal(option.rect.left()) + 4.0;
        const qreal settledY = bgRect.center().y() - fullH / 2.0;
        const qreal remaining = 1.0 - accentT;

        qreal indicatorX = settledX;
        if (hierarchy == TreeView::IndicatorHierarchyTransition::Inward)
            indicatorX += (rtl ? -1.0 : 1.0) * remaining * 4.0;
        else if (hierarchy == TreeView::IndicatorHierarchyTransition::Outward)
            indicatorX += (rtl ? 1.0 : -1.0) * remaining * 3.0;

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

    const bool rtl = option.direction == Qt::RightToLeft;
    qreal cursorX = treeRowLeadingEdge(option) + (rtl ? -kCursorStart : kCursorStart);
    const auto takeLeadingRect = [&](qreal width) {
        const qreal x = rtl ? cursorX - width : cursorX;
        const QRectF rect(x, bgRect.top(), width, bgRect.height());
        cursorX += (rtl ? -1.0 : 1.0) * (width + kGap);
        return rect;
    };

    // Tri-state checkbox (multi-select).
    if (m_checkBoxVisible) {
        const QRectF cbArea = takeLeadingRect(kCheckBoxAreaW);
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
            QFont glyphFont = Typography::Icons::font(Typography::IconSize::Compact);
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
    }

    // Rotating chevron for parents.
    const QAbstractItemModel* m = index.model();
    const bool hasChildren = m && m->hasChildren(index);
    const QRectF chevronRect = takeLeadingRect(kChevronAreaW);
    if (hasChildren) {
        const qreal rotation = m_view ? m_view->chevronRotation(index) : 0.0;
        QFont iconFont = Typography::Icons::font(Typography::IconSize::Compact);
        painter->setFont(iconFont);
        painter->setPen(textColor);
        painter->save();
        painter->translate(chevronRect.center());
        painter->rotate((rtl ? -1.0 : 1.0) * rotation * 90.0);
        painter->translate(-chevronRect.center());
        painter->drawText(chevronRect, Qt::AlignCenter,
                          rtl ? Typography::Icons::ChevronLeftMed
                              : Typography::Icons::ChevronRightMed);
        painter->restore();
    }

    // Per-row icon glyph.
    const QString glyph = index.data(TreeIconGlyphRole).toString();
    if (!glyph.isEmpty()) {
        QColor glyphColor = textColor;
        const QVariant colorVar = index.data(TreeIconColorRole);
        if (colorVar.canConvert<QColor>() && colorVar.value<QColor>().isValid() && isEnabled)
            glyphColor = colorVar.value<QColor>();
        QFont iconFont = Typography::Icons::font(Typography::IconSize::Standard);
        painter->setFont(iconFont);
        painter->setPen(glyphColor);
        painter->drawText(takeLeadingRect(kIconAreaW), Qt::AlignCenter, glyph);
    }

    // Text.
    const QRectF textSlot = rtl
        ? QRectF(bgRect.left() + 8.0, bgRect.top(),
                 qMax<qreal>(0.0, cursorX - bgRect.left() - 8.0), bgRect.height())
        : QRectF(cursorX, bgRect.top(),
                 qMax<qreal>(0.0, bgRect.right() - cursorX - 8.0), bgRect.height());
    painter->setPen(textColor);
    painter->setFont(option.font);
    const QString text = index.data(Qt::DisplayRole).toString();
    const QFontMetricsF metrics(painter->font());
    const QString elidedText = metrics.elidedText(
        text, rtl ? Qt::ElideLeft : Qt::ElideRight, qRound(textSlot.width()));
    const QRectF textRect = fluent::painting::verticallyCenteredTextInkRect(
        textSlot, metrics, elidedText);
    painter->drawText(textRect,
                      (rtl ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter,
                      elidedText);

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
