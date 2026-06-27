#include "GalleryNavigationDelegate.h"

#include <QAbstractItemView>
#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QPainter>
#include <QPair>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QVariant>

#include "components/collections/TreeView.h"
#include "components/foundation/FluentElement.h"
#include "design/Typography.h"
#include "GalleryNavigationMetrics.h"
#include "model/GalleryNavigationItem.h"

namespace fluent::gallery {

namespace {

int viewportWidthForOption(const QStyleOptionViewItem& option)
{
    const auto* itemView = qobject_cast<const QAbstractItemView*>(option.widget);
    return itemView && itemView->viewport() ? itemView->viewport()->width() : option.rect.width();
}

bool compactForOption(const QStyleOptionViewItem& option)
{
    for (const QWidget* widget = option.widget; widget; widget = widget->parentWidget()) {
        const QVariant compact = widget->property("galleryCompact");
        if (compact.isValid())
            return compact.toBool();
    }
    return false;
}

qreal compactVisualProgressForOption(const QStyleOptionViewItem& option)
{
    for (const QWidget* widget = option.widget; widget; widget = widget->parentWidget()) {
        const QVariant progress = widget->property("galleryCompactVisualProgress");
        if (progress.isValid())
            return qBound<qreal>(0.0, progress.toDouble(), 1.0);
    }
    return compactForOption(option) ? 1.0 : 0.0;
}

qreal settingsIconRotationForOption(const QStyleOptionViewItem& option)
{
    for (const QWidget* widget = option.widget; widget; widget = widget->parentWidget()) {
        const QVariant rotation = widget->property("gallerySettingsIconRotation");
        if (rotation.isValid())
            return rotation.toDouble();
    }
    return 0.0;
}

QRectF rowBackgroundRectForOption(const QStyleOptionViewItem& option)
{
    const bool compact = compactForOption(option);
    const qreal compactProgress = compactVisualProgressForOption(option);
    const bool fullyCompact = compact && compactProgress >= 0.999;
    const qreal availableWidth = fullyCompact ? kCompactPaneWidth : viewportWidthForOption(option);
    const qreal rightInset = compact ? kRowLeftInset : kRowRightInset;
    return QRectF(kRowLeftInset,
                  option.rect.top() + kRowVerticalInset,
                  qMax<qreal>(0.0, availableWidth - kRowLeftInset - rightInset),
                  option.rect.height() - 2.0 * kRowVerticalInset);
}

QRectF chevronRectForOption(const QStyleOptionViewItem& option)
{
    const QRectF backgroundRect = rowBackgroundRectForOption(option);
    return QRectF(backgroundRect.right() - kChevronRightInset - kChevronAreaWidth,
                  backgroundRect.top(),
                  kChevronAreaWidth,
                  backgroundRect.height());
}

class GalleryNavigationDelegate : public QStyledItemDelegate {
public:
    explicit GalleryNavigationDelegate(fluent::FluentElement* themeHost, QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
        , m_themeHost(themeHost)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const auto kind = static_cast<GalleryNavigationItem::Kind>(index.data(KindRole).toInt());
        if (kind == GalleryNavigationItem::Kind::SectionHeader) {
            const qreal compactProgress = compactVisualProgressForOption(option);
            return QSize(1, qRound(kSectionHeight * (1.0 - compactProgress)));
        }
        return QSize(1, kRouteHeight);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (!index.isValid() || !m_themeHost)
            return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);

        const auto& colors = m_themeHost->themeColorsRef();
        const auto radius = m_themeHost->themeRadius();
        const auto kind = static_cast<GalleryNavigationItem::Kind>(index.data(KindRole).toInt());
        const QString text = index.data(Qt::DisplayRole).toString();
        const bool compact = compactForOption(option);
        const qreal compactProgress = compactVisualProgressForOption(option);
        const qreal expandedOpacity = qBound<qreal>(0.0, 1.0 - compactProgress, 1.0);

        if (kind == GalleryNavigationItem::Kind::SectionHeader) {
            if (expandedOpacity <= 0.01) {
                painter->restore();
                return;
            }
            painter->setFont(m_themeHost->themeFont(Typography::FontRole::Caption).toQFont());
            painter->setPen(colors.textSecondary);
            painter->setOpacity(expandedOpacity);
            painter->drawText(option.rect.adjusted(qRound(16 - 6 * compactProgress), 6, -8, 0),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              text);
            painter->restore();
            return;
        }

        const QRectF backgroundRect = rowBackgroundRectForOption(option);

        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const bool pressed = (option.state & QStyle::State_Sunken) && hovered;

        QColor background = Qt::transparent;
        if (pressed)
            background = colors.subtleTertiary;
        else if (selected || hovered)
            background = colors.subtleSecondary;

        if (background.alpha() > 0) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(background);
            painter->drawRoundedRect(backgroundRect, radius.control, radius.control);
        }

        // The selected indicator pill is painted by the TreeView overlay so it can
        // slide between rows with direction-aware motion.
        // zh_CN: 选中指示器药丸由 TreeView 覆盖层绘制，可在行间带方向感地滑动。

        const bool hasChildren = index.model() && index.model()->hasChildren(index);
        QRectF chevronRect;
        const qreal chevronOpacity = compact ? 0.0 : expandedOpacity;
        if (hasChildren && chevronOpacity > 0.01) {
            const auto* treeView = qobject_cast<const fluent::collections::TreeView*>(option.widget);
            const qreal progress = treeView
                ? treeView->chevronRotation(index)
                : (qobject_cast<const QTreeView*>(option.widget)
                       && qobject_cast<const QTreeView*>(option.widget)->isExpanded(index) ? 1.0 : 0.0);
            chevronRect = chevronRectForOption(option);
            QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
            iconFont.setPixelSize(9);
            painter->setFont(iconFont);
            painter->setPen(colors.textSecondary);
            painter->save();
            painter->setOpacity(chevronOpacity);
            painter->translate(chevronRect.center());
            painter->rotate(180.0 * qBound<qreal>(0.0, progress, 1.0));
            painter->translate(-chevronRect.center());
            painter->drawText(chevronRect,
                              Qt::AlignCenter,
                              Typography::Icons::ChevronDownMed);
            painter->restore();
        }

        const QString iconGlyph = index.data(IconGlyphRole).toString();
        const bool hasIcon = !iconGlyph.isEmpty();
        const qreal contentLeft = backgroundRect.left() + kContentStart;
        const qreal compactIconLeft = qMax<qreal>(0.0, (kCompactPaneWidth - kIconAreaWidth) / 2.0);
        const qreal iconLeft = contentLeft + (compactIconLeft - contentLeft) * compactProgress;
        qreal textX = kind == GalleryNavigationItem::Kind::ComponentRoute
            ? backgroundRect.left() + kTextStart
            : contentLeft;
        if (!iconGlyph.isEmpty()) {
            QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
            iconFont.setPixelSize(15);
            painter->setFont(iconFont);
            painter->setPen(selected ? colors.textPrimary : colors.textSecondary);
            const QRectF iconRect(iconLeft,
                                  backgroundRect.top(),
                                  kIconAreaWidth,
                                  backgroundRect.height());
            const qreal iconRotation = index.data(RouteIdRole).toString() == QStringLiteral("settings")
                ? settingsIconRotationForOption(option)
                : 0.0;
            if (!qFuzzyIsNull(iconRotation)) {
                painter->save();
                painter->translate(iconRect.center());
                painter->rotate(iconRotation);
                painter->translate(-iconRect.center());
                painter->drawText(iconRect,
                                  Qt::AlignCenter,
                                  iconGlyph);
                painter->restore();
            } else {
                painter->drawText(iconRect,
                                  Qt::AlignCenter,
                                  iconGlyph);
            }
            textX = backgroundRect.left() + kTextStart;
        } else if (!hasIcon && kind != GalleryNavigationItem::Kind::ComponentRoute) {
            textX = backgroundRect.left() + kTextStart;
        }

        // Selected rows keep the regular text weight — no bold highlight.
        // zh_CN: 选中行保持常规字重，不做字体加粗高亮。
        QFont textFont = m_themeHost->themeFont(Typography::FontRole::Body).toQFont();
        textFont.setPixelSize(kRouteTextPixelSize);
        painter->setFont(textFont);
        painter->setPen(colors.textPrimary);
        const qreal textRight = hasChildren && !compact
            ? chevronRect.left() - kTextRightGap
            : backgroundRect.right() - kTextRightGap;
        const QRectF textRect(textX - 6.0 * compactProgress,
                              backgroundRect.top(),
                              qMax<qreal>(0.0, textRight - textX),
                              backgroundRect.height());
        if (expandedOpacity > 0.01) {
            painter->save();
            painter->setOpacity(expandedOpacity);
            painter->drawText(textRect,
                              Qt::AlignLeft | Qt::AlignVCenter,
                              cachedElidedText(painter->fontMetrics(), text, qRound(textRect.width())));
            painter->restore();
        }
        painter->restore();
    }

private:
    // Eliding measures text layout, which is comparatively expensive on Windows
    // (DirectWrite) and was being redone every animation frame for every visible
    // row. The labels and their widths are stable, so memoise the result.
    // zh_CN: 省略号计算要做文本排版测量，在 Windows（DirectWrite）上偏贵，且原先每帧每行都重算。
    // 标签文本与宽度是稳定的，缓存结果即可。
    QString cachedElidedText(const QFontMetrics& metrics, const QString& text, int width) const
    {
        const auto key = qMakePair(text, width);
        const auto cached = m_elideCache.constFind(key);
        if (cached != m_elideCache.constEnd())
            return cached.value();
        const QString elided = metrics.elidedText(text, Qt::ElideRight, width);
        m_elideCache.insert(key, elided);
        return elided;
    }

    fluent::FluentElement* m_themeHost = nullptr;
    mutable QHash<QPair<QString, int>, QString> m_elideCache;
};

} // namespace

QAbstractItemDelegate* makeGalleryNavigationDelegate(fluent::FluentElement* themeHost, QObject* parent)
{
    return new GalleryNavigationDelegate(themeHost, parent);
}

} // namespace fluent::gallery
