#include "FluentGridItemDelegate.h"

#include <QAbstractItemModel>
#include <QPainter>
#include <QPainterPath>
#include <QListView>
#include <QStyle>
#include <QDateTime>
#include <QTimer>

#include "design/Animation.h"
#include "design/CornerRadius.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include "components/foundation/FluentElement.h"
#include "components/status_info/Shimmer.h"

namespace gridview_test {
namespace {

QVector<fluent::status_info::ShimmerPainter::Element> gridImageLoadingElements(
    const QRectF& bounds,
    qreal radius)
{
    using Element = fluent::status_info::ShimmerPainter::Element;
    using Shape = fluent::status_info::ShimmerPainter::Shape;

    const QRectF surface = bounds.adjusted(8.0, 8.0, -8.0, -8.0);
    if (!surface.isValid() || surface.isEmpty())
        return {};

    const qreal captionHeight = qBound<qreal>(18.0, surface.height() * 0.24, 34.0);
    const QRectF mediaRect(surface.left(),
                           surface.top(),
                           surface.width(),
                           qMax<qreal>(20.0, surface.height() - captionHeight - 8.0));
    const qreal textTop = mediaRect.bottom() + 8.0;
    const qreal lineHeight = qMin<qreal>(10.0, qMax<qreal>(6.0, captionHeight * 0.42));

    QVector<Element> elements{
        Element(Shape::RoundedRect, mediaRect, radius),
        Element(Shape::Line, QRectF(surface.left(), textTop, surface.width() * 0.62, lineHeight))
    };
    if (captionHeight >= 24.0) {
        elements.append(Element(Shape::Line,
                                QRectF(surface.left(),
                                       textTop + lineHeight + 6.0,
                                       surface.width() * 0.42,
                                       lineHeight)));
    }
    return elements;
}

} // namespace

FluentGridItemDelegate::FluentGridItemDelegate(fluent::FluentElement* themeHost,
                                               QListView* view,
                                               QObject* parent)
    : QStyledItemDelegate(parent), m_themeHost(themeHost), m_view(view) {
    auto* repaintTimer = new QTimer(this);
    repaintTimer->setInterval(16);
    connect(repaintTimer, &QTimer::timeout, this, [this]() {
        if (!m_view || !m_view->isVisible() || !hasLoadingRows())
            return;
        m_view->viewport()->update();
    });
    repaintTimer->start();
}

void FluentGridItemDelegate::setThemeHost(fluent::FluentElement* host) {
    m_themeHost = host;
}

void FluentGridItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const {
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

    // 绘制区域：在 gridSize 内缩进一点
    QRectF bgRect = QRectF(option.rect).adjusted(2, 2, -2, -2);

    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isHovered  = option.state & QStyle::State_MouseOver;
    const bool isPressed  = (option.state & QStyle::State_Sunken) && isHovered;
    const bool isEnabled  = option.state & QStyle::State_Enabled;
    const bool isMultiSel = m_view &&
        (m_view->selectionMode() == QAbstractItemView::MultiSelection ||
         m_view->selectionMode() == QAbstractItemView::ExtendedSelection);

    // --- 尝试获取图片 (ImageRole) ---
    const QVariant imgVar = index.data(ImageRole);
    const bool hasImage = imgVar.isValid() && imgVar.canConvert<QPixmap>() &&
                          !imgVar.value<QPixmap>().isNull();
    const bool isLoadingImage = !hasImage && index.data(ImageLoadingRole).toBool();

    if (hasImage) {
        // ── 图片模式 ──────────────────────────────────────────────────────
        const QPixmap pixmap = imgVar.value<QPixmap>();

        // 裁剪路径（圆角）
        QPainterPath clipPath;
        clipPath.addRoundedRect(bgRect, cornerR, cornerR);
        painter->setClipPath(clipPath);

        // Cover 模式绘制图片
        QSizeF imgSize = pixmap.size();
        QSizeF viewSize = bgRect.size();
        qreal scale = qMax(viewSize.width() / imgSize.width(),
                           viewSize.height() / imgSize.height());
        QSizeF scaled = imgSize * scale;
        QRectF src((scaled.width() - viewSize.width()) / 2.0 / scale,
                   (scaled.height() - viewSize.height()) / 2.0 / scale,
                   viewSize.width() / scale, viewSize.height() / scale);
        painter->drawPixmap(bgRect.toRect(), pixmap, src.toRect());

        // 底部文本遮罩 + 文本
        const QString text = index.data(Qt::DisplayRole).toString();
        const QString subText = index.data(Qt::ToolTipRole).toString();
        if (!text.isEmpty()) {
            const int barH = subText.isEmpty() ? 28 : 40;
            QRectF labelBar(bgRect.left(), bgRect.bottom() - barH,
                            bgRect.width(), barH);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0, 0, 0, 140));
            painter->drawRect(labelBar);

            painter->setPen(Qt::white);
            if (subText.isEmpty()) {
                painter->setFont(option.font);
                painter->drawText(labelBar.adjusted(8, 0, -8, 0),
                                  Qt::AlignVCenter | Qt::AlignLeft, text);
            } else {
                // 两行：标题 + 副标题
                QFont titleFont = option.font;
                titleFont.setWeight(QFont::DemiBold);
                painter->setFont(titleFont);
                QRectF titleRect(labelBar.left() + 8, labelBar.top() + 2,
                                 labelBar.width() - 16, 20);
                painter->drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft, text);

                QFont subFont = option.font;
                subFont.setPixelSize(subFont.pixelSize() > 2 ? subFont.pixelSize() - 2 : 10);
                painter->setFont(subFont);
                painter->setPen(QColor(220, 220, 220));
                QRectF subRect(labelBar.left() + 8, titleRect.bottom(),
                               labelBar.width() - 16, 16);
                painter->drawText(subRect, Qt::AlignVCenter | Qt::AlignLeft, subText);
            }
        }

        painter->setClipping(false);

        // 选中态 accent 边框
        if (isSelected && isEnabled && colors.accentDefault.isValid()) {
            QPainterPath borderPath;
            borderPath.addRoundedRect(bgRect.adjusted(0.5, 0.5, -0.5, -0.5), cornerR, cornerR);
            painter->setPen(QPen(colors.accentDefault, 2.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(borderPath);
        }

        // 多选 check 浮层
        if (isMultiSel) {
            drawCheckOverlay(painter, bgRect, isSelected, isEnabled);
        }

    } else if (isLoadingImage) {
        auto shimmerPalette = fluent::status_info::ShimmerPainter::paletteFromTheme(colors, isEnabled);
        fluent::status_info::ShimmerPainter::paint(
            painter,
            gridImageLoadingElements(bgRect, cornerR),
            shimmerPalette,
            shimmerProgress(),
            true);

        if (isSelected && isEnabled && colors.accentDefault.isValid()) {
            QPainterPath borderPath;
            borderPath.addRoundedRect(bgRect.adjusted(0.5, 0.5, -0.5, -0.5), cornerR, cornerR);
            painter->setPen(QPen(colors.accentDefault, 2.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(borderPath);
        }

        if (isMultiSel) {
            drawCheckOverlay(painter, bgRect, isSelected, isEnabled);
        }
    } else {
        // ── 无图片模式：icon glyph + text 卡片 ──────────────────────────
        QColor bgColor = colors.bgLayerAlt;
        QColor textColor = colors.textPrimary;

        if (!isEnabled) {
            textColor = colors.textDisabled;
        } else if (isPressed) {
            bgColor = colors.subtleTertiary;
        } else if (isHovered) {
            bgColor = colors.subtleSecondary;
        }

        // 绘制卡片背景
        {
            QPainterPath path;
            path.addRoundedRect(bgRect, cornerR, cornerR);
            painter->setPen(Qt::NoPen);
            painter->setBrush(bgColor);
            painter->drawPath(path);
        }

        // 选中态 accent 边框
        if (isSelected && isEnabled && colors.accentDefault.isValid()) {
            QPainterPath borderPath;
            borderPath.addRoundedRect(bgRect.adjusted(0.5, 0.5, -0.5, -0.5), cornerR, cornerR);
            painter->setPen(QPen(colors.accentDefault, 2.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(borderPath);
        }

        // icon glyph (DecorationRole)
        const QVariant iconVar = index.data(Qt::DecorationRole);
        const QString iconGlyph = iconVar.toString();
        const int textAreaH = 24;
        QRectF iconRect = bgRect.adjusted(4, 4, -4, -4 - textAreaH);

        if (!iconGlyph.isEmpty()) {
            QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
            iconFont.setPixelSize(qMin(static_cast<int>(iconRect.width()),
                                       static_cast<int>(iconRect.height())) / 2);
            painter->setFont(iconFont);
            painter->setPen(colors.textPrimary);
            painter->drawText(iconRect, Qt::AlignCenter, iconGlyph);
        }

        // 文本
        const QString text = index.data(Qt::DisplayRole).toString();
        if (!text.isEmpty()) {
            QRectF textRect(bgRect.left(), bgRect.bottom() - textAreaH,
                            bgRect.width(), textAreaH);
            painter->setPen(isEnabled ? colors.textPrimary : colors.textDisabled);
            painter->setFont(option.font);
            painter->drawText(textRect, Qt::AlignCenter,
                              painter->fontMetrics().elidedText(text, Qt::ElideRight,
                                                               static_cast<int>(textRect.width()) - 8));
        }

        // 多选 check 浮层
        if (isMultiSel) {
            drawCheckOverlay(painter, bgRect, isSelected, isEnabled);
        }
    }

    painter->restore();
}

// ── 右上角 Check 浮层（WinUI 3 多选样式） ────────────────────────────────────

void FluentGridItemDelegate::drawCheckOverlay(QPainter* painter, const QRectF& cellRect,
                                              bool selected, bool enabled) const {
    fluent::FluentElement::Colors colors{};
    if (m_themeHost) colors = m_themeHost->themeColors();

    constexpr int kSize = 20;
    constexpr int kMargin = 6;
    const QRectF checkRect(cellRect.right() - kSize - kMargin,
                           cellRect.top() + kMargin,
                           kSize, kSize);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    if (selected && enabled) {
        // 选中：accent 填充圆 + 白色对勾
        painter->setPen(Qt::NoPen);
        painter->setBrush(colors.accentDefault);
        painter->drawEllipse(checkRect);

        QFont checkFont(Typography::FontFamily::SegoeFluentIcons);
        checkFont.setPixelSize(12);
        painter->setFont(checkFont);
        painter->setPen(Qt::white);
        painter->drawText(checkRect, Qt::AlignCenter,
                          Typography::Icons::CheckMark);
    } else {
        // 未选中：半透明背景圆 + 细描边
        painter->setPen(QPen(QColor(255, 255, 255, 180), 1.5));
        painter->setBrush(QColor(0, 0, 0, 60));
        painter->drawEllipse(checkRect.adjusted(0.75, 0.75, -0.75, -0.75));
    }

    painter->restore();
}

QSize FluentGridItemDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                                       const QModelIndex& /*index*/) const {
    if (m_view) {
        return m_view->gridSize();
    }
    return QSize(112, 112);
}

bool FluentGridItemDelegate::hasLoadingRows() const
{
    if (!m_view || !m_view->model())
        return false;

    const QAbstractItemModel* model = m_view->model();
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->index(row, 0).data(ImageLoadingRole).toBool())
            return true;
    }
    return false;
}

qreal FluentGridItemDelegate::shimmerProgress() const
{
    constexpr qint64 kCycleMs = Animation::Duration::VerySlow + Animation::Duration::VerySlow;
    return static_cast<qreal>(QDateTime::currentMSecsSinceEpoch() % kCycleMs)
        / static_cast<qreal>(kCycleMs);
}

} // namespace gridview_test
