#include <memory>
#include <QApplication>
#include <QImage>
#include <QIcon>
#include <QPainter>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <gtest/gtest.h>
#include "components/collections/FlowView.h"
#include "components/collections/GridView.h"

namespace {
QImage paintItem(QAbstractItemView* view, QAbstractItemDelegate* delegate, const QModelIndex& index)
{
    QImage image(QSize(180, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    QStyleOptionViewItem option;
    option.initFrom(view);
    option.widget = view;
    option.rect = image.rect();
    option.decorationSize = QSize(24, 24);
    option.font = view->font();
    option.state = QStyle::State_Enabled;
    delegate->paint(&painter, option, index);
    return image;
}

TEST(CollectionModelRoles, GridAndFlowRetainStandardRolesAndIndependentRows)
{
    for (const bool grid : {false, true}) {
        for (const auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
            SCOPED_TRACE(::testing::Message() << "grid=" << grid << " theme=" << theme);
            fluent::FluentElement::setTheme(theme);
            std::unique_ptr<QAbstractItemView> view;
            if (grid)
                view = std::make_unique<fluent::collections::GridView>();
            else
                view = std::make_unique<fluent::collections::FlowView>();
            QStandardItemModel model;
            model.appendRow(new QStandardItem(QStringLiteral("First")));
            model.appendRow(new QStandardItem(QStringLiteral("Second")));
            view->setModel(&model);
            view->ensurePolished();
            QStyledItemDelegate standard;
            const auto index = model.index(0, 0);
            const QImage plain = paintItem(view.get(), view->itemDelegate(), index);
            const QImage second = paintItem(view.get(), view->itemDelegate(), model.index(1, 0));
            QPixmap pixmap(24, 24);
            pixmap.fill(Qt::magenta);
            for (const QVariant& decoration : {QVariant::fromValue(QIcon(pixmap)), QVariant::fromValue(pixmap),
                     QVariant::fromValue(pixmap.toImage()), QVariant::fromValue(QColor(Qt::red))}) {
                model.setData(index, decoration, Qt::DecorationRole);
                const QImage rendered = paintItem(view.get(), view->itemDelegate(), index);
                EXPECT_NE(rendered, plain);
                EXPECT_EQ(rendered, paintItem(view.get(), &standard, index));
                EXPECT_EQ(paintItem(view.get(), view->itemDelegate(), model.index(1, 0)), second);
            }
            model.setData(index, QVariant(), Qt::DecorationRole);
            QFont font(view->font());
            font.setPixelSize(24);
            model.setData(index, font, Qt::FontRole);
            model.setData(index, QColor(Qt::red), Qt::ForegroundRole);
            model.setData(index, QColor(Qt::yellow), Qt::BackgroundRole);
            model.setData(index, Qt::Checked, Qt::CheckStateRole);
            model.setData(index, int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
            EXPECT_EQ(paintItem(view.get(), view->itemDelegate(), index), paintItem(view.get(), &standard, index));
            EXPECT_NE(paintItem(view.get(), view->itemDelegate(), index), plain);
            EXPECT_EQ(paintItem(view.get(), view->itemDelegate(), model.index(1, 0)), second);
        }
    }
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
}
} // namespace
