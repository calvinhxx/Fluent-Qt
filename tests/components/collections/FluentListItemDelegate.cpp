#include "FluentListItemDelegate.h"

#include <QPainter>
#include <QPainterPath>
#include <QAbstractItemView>
#include <QStyle>

#include "design/Spacing.h"
#include "components/foundation/FluentElement.h"

namespace listview_test {

FluentListItemDelegate::FluentListItemDelegate(fluent::FluentElement* themeHost, int rowHeight,
                                               QAbstractItemView* view,
                                               QObject* parent)
    : QStyledItemDelegate(parent), m_themeHost(themeHost), m_rowHeight(rowHeight) {
    Q_UNUSED(view);
}

void FluentListItemDelegate::setThemeHost(fluent::FluentElement* host) {
    m_themeHost = host;
}

void FluentListItemDelegate::setRowHeight(int height) {
    m_rowHeight = height;
}

void FluentListItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const {
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

    const int hPad = Spacing::Padding::ListItemHorizontal;
    const int cornerR = radius.control > 0 ? radius.control : 4;

    QRectF bgRect = QRectF(option.rect).adjusted(2, 1, -2, -1);

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

    QRectF textRect = bgRect.adjusted(hPad + 8, 0, -hPad, 0);
    painter->setPen(textColor);
    painter->setFont(option.font);
    const QString text = index.data(Qt::DisplayRole).toString();
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(text, Qt::ElideRight, int(textRect.width())));

    painter->restore();
}

QSize FluentListItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const {
    const int h = m_rowHeight > 0 ? m_rowHeight
                                  : (Spacing::ControlHeight::Standard + Spacing::Gap::Tight);
    const int hPad = Spacing::Padding::ListItemHorizontal;
    const int hGap  = Spacing::Gap::Tight;  // 水平 item 之间的间距
    const QString text = index.data(Qt::DisplayRole).toString();
    const QFontMetrics fm(option.font);
    const int w = fm.horizontalAdvance(text) + hPad * 2 + 12 + hGap;
    return QSize(w, h);
}

} // namespace listview_test
