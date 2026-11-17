#pragma once

// Widget signal signatures, layout, and item-view options.

#include "QtFeatures.h"
#include <QAbstractItemView>
#include <QCheckBox>
#include <QLabel>
#include <QLayoutItem>
#include <QPixmap>
#include <utility>

/**
 * @brief Connects a checkbox state signal with one Qt 5/Qt 6 spelling.
 * zh_CN: 使用统一的 Qt 5/Qt 6 写法连接复选框状态信号。
 */
template <typename CheckBoxType, typename Context, typename Functor>
QMetaObject::Connection fluentConnectCheckStateChanged(CheckBoxType* checkBox, Context* context, Functor&& functor)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    return QObject::connect(checkBox, &CheckBoxType::checkStateChanged, context, std::forward<Functor>(functor));
#else
    return QObject::connect(checkBox, &CheckBoxType::stateChanged, context,
        [slot = std::forward<Functor>(functor)](int state) mutable { slot(static_cast<Qt::CheckState>(state)); });
#endif
}

template <typename Context, typename Functor>
QMetaObject::Connection fluentConnectCheckStateChanged(QCheckBox* checkBox, Context* context, Functor&& functor)
{
    return fluentConnectCheckStateChanged<QCheckBox, Context, Functor>(checkBox, context,
        std::forward<Functor>(functor));
}

inline QWidget* fluentLayoutItemWidget(const QLayoutItem* item)
{
    if (!item)
        return nullptr;
    return const_cast<QLayoutItem*>(item)->widget();
}

inline int fluentAdjacentButtonRowSpacing(int requestedSpacing)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0) && (defined(Q_OS_MACOS) || defined(Q_OS_MAC))
    return requestedSpacing + 10;
#else
    return requestedSpacing;
#endif
}

inline QPixmap fluentLabelPixmapValue(const QLabel* label)
{
    if (!label)
        return {};
    return label->pixmap(Qt::ReturnByValue);
}

/**
 * @brief Container type for the roles parameter in QAbstractItemModel::dataChanged() (QList<int> on Qt 6, QVector<int> on Qt 5).
 * zh_CN: QAbstractItemModel::dataChanged() 中 roles 参数的容器类型（Qt 6 为 QList<int>，Qt 5 为 QVector<int>）。
 */
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using FluentItemDataRoles = QList<int>;
#else
using FluentItemDataRoles = QVector<int>;
#endif

// QAbstractItemView::initViewItemOption compatibility.
// zh_CN: QAbstractItemView::initViewItemOption 兼容封装。
// Qt 6: protected void QAbstractItemView::initViewItemOption(QStyleOptionViewItem*) const.
// Qt 5: protected QStyleOptionViewItem QAbstractItemView::viewOptions() const.
//
// Use inside a QAbstractItemView subclass:
//   QStyleOptionViewItem opt;
//   FLUENT_INIT_VIEW_ITEM_OPTION(&opt);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define FLUENT_INIT_VIEW_ITEM_OPTION(optPtr) initViewItemOption(optPtr)
#else
#define FLUENT_INIT_VIEW_ITEM_OPTION(optPtr)                                                                           \
    do {                                                                                                               \
        *(optPtr) = viewOptions();                                                                                     \
    } while (0)
#endif

/**
 * @brief Returns a stable item-view row height, falling back to view/delegate size hints when visualRect() has no height yet.
 * zh_CN: 返回稳定的列表/表格行高；当 visualRect() 高度尚为 0 时，依次回退到视图或代理的推荐尺寸。
 */
inline int fluentItemViewRowHeight(const QAbstractItemView* view, const QModelIndex& index, const QRect& visualRect)
{
    if (visualRect.height() > 0)
        return visualRect.height();

    if (!view || !index.isValid())
        return qMax(0, visualRect.height());

    const int indexHint = view->sizeHintForIndex(index).height();
    if (indexHint > 0)
        return indexHint;

    const int rowHint = view->sizeHintForRow(index.row());
    if (rowHint > 0)
        return rowHint;

    return qMax(0, view->fontMetrics().height());
}
