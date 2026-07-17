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
    return fluentIconPixmapForLogicalExtent(icon, extent, painterDevicePixelRatio(painter));
}

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
    painter->drawPixmap(target, pixmap, fluentPixmapSourceRectForDraw(source, pixmap));
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

// Resolved selection/hover fill for a row, branched per design language. Mirrors the
// canonical Button.cpp idiom: Fluent keeps the subtle WinUI fill untouched, Material 3 uses a
// tonal accent wash for selection + a neutral on-surface state layer for hover, and macOS
// (Cupertino) uses the classic solid-accent source-list selection (and sets the returned
// textOnAccent flag so the caller flips the row text/glyph color). The shared row delegates call
// this so List and Tree stay in lockstep.
// zh_CN: 按设计语言分支解析行的选中/悬停填充,沿用 Button.cpp 的规范写法:Fluent 保持原 WinUI subtle 填充不变;
// Material 3 选中用强调色调和淡彩 + 悬停用中性 on-surface state layer;macOS(Cupertino)用经典纯强调色
// 源列表选中(并置位返回的 textOnAccent 标志,由调用方翻转行文字/字形色)。共享行代理调用本函数,使列表与树保持一致。
struct RowSelectionFill {
    QColor color = Qt::transparent;  // Rounded fill brush (transparent = none). zh_CN: 圆角填充色(transparent 表示无)。
    bool textOnAccent = false;       // macOS solid-accent selection → flip text to textOnAccent. zh_CN: macOS 纯强调色选中 → 文字翻转为 textOnAccent。
};

RowSelectionFill rowSelectionFill(const QStyleOptionViewItem& option,
                                  const fluent::FluentElement::Colors& colors,
                                  fluent::FluentElement::DesignLanguage lang,
                                  bool dark)
{
    using DL = fluent::FluentElement;
    RowSelectionFill out;
    if (!(option.state & QStyle::State_Enabled))
        return out;  // Disabled / at rest → transparent. zh_CN: 禁用/静止 → 透明。

    const bool hovered = option.state & QStyle::State_MouseOver;
    const bool pressed = (option.state & QStyle::State_Sunken) && hovered;
    const bool selected = option.state & QStyle::State_Selected;

    // Theme-aware neutral interaction veil (darkens light surfaces, lightens dark). zh_CN: 主题感知中性交互薄层(浅色变暗、深色变亮)。
    const auto veil = [dark](int a) { return dark ? QColor(255, 255, 255, a) : QColor(0, 0, 0, a); };

    if (lang == DL::DesignMaterial) {
        // Material 3: SELECTED = tonal secondary-container-like accent wash (subtleSecondary reads
        // too faint), text stays textPrimary; HOVER (not selected) = neutral on-surface state layer.
        // zh_CN: Material 3:选中 = 类 secondary-container 的强调色调和淡彩(subtleSecondary 太淡),文字保持 textPrimary;
        // 悬停(未选中)= 中性 on-surface state layer。
        if (selected) {
            QColor wash = colors.accentDefault;
            if (wash.isValid()) {
                wash.setAlphaF(dark ? 0.28 : 0.16);
                out.color = wash;
            }
        } else if (hovered) {
            out.color = veil(0x14);  // 8% on-surface state layer. zh_CN: 8% on-surface state layer。
        }
        return out;
    }

    if (lang == DL::DesignCupertino) {
        // macOS: SELECTED = solid accentDefault source-list fill, text flips to textOnAccent;
        // HOVER (not selected) = faint neutral veil.
        // zh_CN: macOS:选中 = 纯 accentDefault 源列表填充,文字翻转为 textOnAccent;悬停(未选中)= 淡中性薄层。
        if (selected && colors.accentDefault.isValid()) {
            out.color = colors.accentDefault;
            out.textOnAccent = true;
        } else if (hovered) {
            out.color = veil(dark ? 0x12 : 0x10);
        }
        return out;
    }

    // DesignFluent (default): unchanged WinUI subtle fill — pressed > selected/hover. zh_CN: 默认 Fluent:原 WinUI subtle 填充不变——按下 > 选中/悬停。
    if (pressed)
        out.color = colors.subtleTertiary;
    else if (selected || hovered)
        out.color = colors.subtleSecondary;
    return out;
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

    // Design language steers how the SELECTED tile reads. The accent border + check affordance are
    // kept across all languages (accent resolves correctly everywhere); Material 3 just makes the
    // selected border slightly heavier and adds a faint accent scrim over the image so the tile
    // reads as selected even when the src view suppresses its own pill. zh_CN: 设计语言决定选中图块的表现。
    // 强调边框 + 勾选标记在所有语言下保留(强调色到处都正确);Material 3 仅让选中边框略粗并在图上加一层淡强调蒙版,
    // 使其在 src 视图抑制自身指示条时仍清晰可读。
    const auto lang = m_themeHost ? m_themeHost->themeDesignLanguage()
                                  : fluent::FluentElement::DesignFluent;

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

    // Material 3 selected scrim: a faint accent wash over the whole tile (clipped to the rounded
    // card), giving the M3 tonal "selected" read without an opaque black overlay. Guarded with
    // isValid()+alpha so an unassigned accent never paints solid black. zh_CN: Material 3 选中蒙版:
    // 整块图块覆一层淡强调色(裁剪到圆角卡片),呈现 M3 调和选中观感而非不透明黑色蒙版。以 isValid()+alpha 守护,
    // 避免未赋值的强调色绘成纯黑。
    if (lang == fluent::FluentElement::DesignMaterial && isSelected && isEnabled
        && colors.accentDefault.isValid()) {
        QColor scrim = colors.accentDefault;
        scrim.setAlpha(0x24);  // ~14% accent wash. zh_CN: 约 14% 强调色蒙版。
        if (scrim.isValid() && scrim.alpha() > 0)
            painter->fillPath(clip, scrim);
    }

    // Accent selection border. Material 3 uses a slightly heavier selected stroke to its taste;
    // Fluent and macOS keep the 2px accent border. zh_CN: 强调选中边框。Material 3 选中描边略粗;Fluent 与 macOS 保持 2px。
    const qreal selectedBorderW = (lang == fluent::FluentElement::DesignMaterial) ? 2.5 : 2.0;
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(isSelected ? accent : stroke, isSelected ? selectedBorderW : 1.0));
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

        QFont checkFont(Typography::FontFamily::FluentIcons);
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

    const fluent::FluentElement::Colors& colors =
        m_themeHost ? m_themeHost->themeColorsRef() : emptyColors();
    fluent::FluentElement::Radius radius{};
    if (m_themeHost)
        radius = m_themeHost->themeRadius();
    const int cornerR = radius.control > 0 ? radius.control : CornerRadius::Control;

    // Design language + effective theme drive the per-language selection/hover treatment.
    // zh_CN: 设计语言 + 有效主题决定各语言的选中/悬停表现。
    const auto lang = m_themeHost ? m_themeHost->themeDesignLanguage()
                                  : fluent::FluentElement::DesignFluent;
    const bool dark = m_themeHost
                      && m_themeHost->effectiveTheme() == fluent::FluentElement::Dark;

    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isEnabled = option.state & QStyle::State_Enabled;

    // Background rect matches the container's indicator base rect so the accent pill,
    // painted by ListView on top, lines up with the rounded fill we draw here.
    const QRectF bgRect = QRectF(option.rect).adjusted(2.0, 1.0, -2.0, -1.0);

    const RowSelectionFill fill = rowSelectionFill(option, colors, lang, dark);
    fillRoundedBackground(painter, bgRect, fill.color, cornerR);

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
    const QRectF textRect(cursorX, bgRect.top(), bgRect.right() - cursorX - 8.0, bgRect.height());
    // macOS solid-accent selection flips the row text to textOnAccent so it stays legible. zh_CN: macOS 纯强调色选中时,行文字翻转为 textOnAccent 以保证可读。
    QColor rowTextColor = isEnabled ? colors.textPrimary : colors.textDisabled;
    if (isEnabled && fill.textOnAccent && colors.textOnAccent.isValid())
        rowTextColor = colors.textOnAccent;
    painter->setPen(rowTextColor);
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
    // Span from this row's indented content edge to the viewport's right, so the rounded
    // highlight tracks the item's hierarchy depth (like the accent pill) instead of bleeding
    // a stray fill across the indentation gutter on the left.
    // zh_CN: 从本行缩进后的内容左缘延伸到视口右缘，使圆角高亮随层级缩进（与强调指示条一致），
    // 而非在左侧缩进留白处渗出一截多余填充。
    const int vpRight = m_view && m_view->viewport() ? m_view->viewport()->width() - 2
                                                     : option.rect.right();
    return QRectF(option.rect.left() + 2, option.rect.top() + 2,
                  qMax<qreal>(0.0, vpRight - (option.rect.left() + 2)),
                  option.rect.height() - 4);
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

    const fluent::FluentElement::Colors& colors =
        m_themeHost ? m_themeHost->themeColorsRef() : emptyColors();
    fluent::FluentElement::Radius radius{};
    if (m_themeHost)
        radius = m_themeHost->themeRadius();
    const int cornerR = radius.control > 0 ? radius.control : 4;
    const QRectF bgRect = bgRectForOption(option);

    // Design language + effective theme drive the per-language selection/hover treatment
    // (mirrors ListRowDelegate). zh_CN: 设计语言 + 有效主题决定各语言的选中/悬停表现(与 ListRowDelegate 一致)。
    const auto lang = m_themeHost ? m_themeHost->themeDesignLanguage()
                                  : fluent::FluentElement::DesignFluent;
    const bool dark = m_themeHost
                      && m_themeHost->effectiveTheme() == fluent::FluentElement::Dark;

    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isEnabled = option.state & QStyle::State_Enabled;

    const RowSelectionFill fill = rowSelectionFill(option, colors, lang, dark);

    QColor textColor = colors.textPrimary;
    if (!isEnabled)
        textColor = colors.textDisabled;
    // macOS solid-accent selection flips row text/glyph/chevron to textOnAccent for legibility. zh_CN: macOS 纯强调色选中时,行文字/字形/chevron 翻转为 textOnAccent 以保证可读。
    else if (fill.textOnAccent && colors.textOnAccent.isValid())
        textColor = colors.textOnAccent;

    fillRoundedBackground(painter, bgRect, fill.color, cornerR);

    // Animated accent indicator (single-select). When TreeView owns the overlay
    // indicator, skip the delegate bar so examples do not show two indicators.
    // Under Material/macOS the full-row fill IS the only selection cue, so the Fluent-style
    // accent bar is suppressed (it would be a double cue on M3's wash, invisible on macOS's
    // solid-accent row). zh_CN: 在 Material/macOS 下整行填充即唯一选中提示,故抑制 Fluent 式强调指示条
    // (在 M3 淡彩上是双重提示,在 macOS 纯强调行上不可见)。
    const bool treeOverlayIndicatorVisible = m_view && m_view->selectionIndicatorVisible();
    const bool accentBarAllowed = (lang == fluent::FluentElement::DesignFluent);
    if (accentBarAllowed && !treeOverlayIndicatorVisible && !m_checkBoxVisible && isSelected
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
            QFont glyphFont(Typography::FontFamily::FluentIcons);
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
        QFont iconFont(Typography::FontFamily::FluentIcons);
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
        // Honor the per-row glyph color, EXCEPT on a macOS solid-accent selected row where a custom
        // hue would clash with the accent fill — there textColor is already textOnAccent.
        // zh_CN: 采用行自定义字形色,但在 macOS 纯强调色选中行除外(自定义色会与强调填充冲突)——此时 textColor 已是 textOnAccent。
        if (colorVar.canConvert<QColor>() && colorVar.value<QColor>().isValid() && isEnabled
            && !fill.textOnAccent)
            glyphColor = colorVar.value<QColor>();
        QFont iconFont(Typography::FontFamily::FluentIcons);
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
