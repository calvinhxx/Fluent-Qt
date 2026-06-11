#include "CollectionsSamples.h"

#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/collections/DrawerView.h"
#include "components/collections/FlipView.h"
#include "components/collections/FlowView.h"
#include "components/collections/GridView.h"
#include "components/collections/ListView.h"
#include "components/collections/SplitView.h"
#include "components/collections/StackView.h"
#include "components/collections/TreeView.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::collections::DrawerView;
using fluent::collections::FlipView;
using fluent::collections::FlowView;
using fluent::collections::GridView;
using fluent::collections::ListView;
using fluent::collections::SplitView;
using fluent::collections::StackView;
using fluent::collections::TreeView;
using fluent::textfields::Label;
using samples::horizontalGroup;
using samples::makeSample;
using samples::verticalGroup;

/**
 * @brief Centered pastel page used by paged-container previews.
 * zh_CN: 翻页类容器预览使用的居中淡彩页面。
 */
Label* makePastelPage(QWidget* parent, const QString& text, const QString& cssBackground)
{
    auto* page = new Label(text, parent);
    page->setFluentTypography(Typography::FontRole::BodyStrong);
    page->setAlignment(Qt::AlignCenter);
    page->setStyleSheet(QStringLiteral("background: %1; color: #444; border-radius: 8px;")
                            .arg(cssBackground));
    return page;
}

QStandardItemModel* makeStringListModel(QObject* parent, const QStringList& texts)
{
    auto* model = new QStandardItemModel(parent);
    for (const QString& text : texts) {
        auto* item = new QStandardItem(text);
        item->setEditable(false);
        model->appendRow(item);
    }
    return model;
}

QVector<GallerySample> drawerViewSamples()
{
    return {
        makeSample(QStringLiteral("drawer-view-basic"),
                   QStringLiteral("Left-edge drawer"),
                   QStringLiteral("The drawer slides over the hosting surface and dismisses on outside click."),
                   QStringLiteral("auto* drawer = new DrawerView(host);\n"
                                  "drawer->setEdge(DrawerView::DrawerEdge::Left);\n"
                                  "drawer->setDrawerLength(200);\n"
                                  "drawer->setIsOpen(true);"),
                   [](QWidget* parent) {
                       auto* host = new QWidget(parent);
                       host->setFixedSize(420, 220);
                       host->setStyleSheet(QStringLiteral(
                           "background: rgba(128, 128, 128, 18); border-radius: 8px;"));

                       auto* drawer = new DrawerView(host);
                       drawer->setEdge(DrawerView::DrawerEdge::Left);
                       drawer->setDrawerLength(200);

                       auto* drawerContent = new Label(QStringLiteral("Drawer content"), nullptr);
                       drawerContent->setFluentTypography(Typography::FontRole::BodyStrong);
                       drawerContent->setAlignment(Qt::AlignCenter);
                       drawer->setContentWidget(drawerContent);

                       auto* openButton = new Button(QStringLiteral("Open drawer"), host);
                       openButton->setFluentStyle(Button::Accent);
                       openButton->move(16, 16);
                       QObject::connect(openButton, &Button::clicked,
                                        drawer, [drawer]() { drawer->setIsOpen(true); });
                       return host;
                   })
    };
}

QVector<GallerySample> flipViewSamples()
{
    return {
        makeSample(QStringLiteral("flip-view-basic"),
                   QStringLiteral("FlipView with pages"),
                   QStringLiteral("Use the arrows or swipe to flip between pages."),
                   QStringLiteral("auto* flipView = new FlipView(this);\n"
                                  "flipView->addPage(firstPage);\n"
                                  "flipView->addPage(secondPage);\n"
                                  "flipView->setShowPageIndicator(true);"),
                   [](QWidget* parent) {
                       auto* flipView = new FlipView(parent);
                       flipView->setFixedSize(400, 200);
                       flipView->setShowPageIndicator(true);
                       flipView->addPage(makePastelPage(flipView, QStringLiteral("Page 1"),
                                                        QStringLiteral("#DCE8F8")));
                       flipView->addPage(makePastelPage(flipView, QStringLiteral("Page 2"),
                                                        QStringLiteral("#DFF3E3")));
                       flipView->addPage(makePastelPage(flipView, QStringLiteral("Page 3"),
                                                        QStringLiteral("#FBE8DC")));
                       return flipView;
                   })
    };
}

QVector<GallerySample> flowViewSamples()
{
    return {
        makeSample(QStringLiteral("flow-view-basic"),
                   QStringLiteral("Wrapping tag layout"),
                   QStringLiteral("Items flow into rows and wrap at the viewport edge."),
                   QStringLiteral("auto* flowView = new FlowView(this);\n"
                                  "flowView->setModel(model);\n"
                                  "flowView->setSelectionMode(FlowView::FlowSelectionMode::Single);"),
                   [](QWidget* parent) {
                       auto* flowView = new FlowView(parent);
                       flowView->setFixedSize(420, 170);
                       flowView->setSelectionMode(FlowView::FlowSelectionMode::Single);
                       flowView->setModel(makeStringListModel(
                           flowView,
                           {QStringLiteral("Design"), QStringLiteral("Development"),
                            QStringLiteral("Fluent"), QStringLiteral("Qt Widgets"),
                            QStringLiteral("Gallery"), QStringLiteral("Components"),
                            QStringLiteral("Theming"), QStringLiteral("Animation"),
                            QStringLiteral("Accessibility")}));
                       return flowView;
                   })
    };
}

QVector<GallerySample> gridViewSamples()
{
    return {
        makeSample(QStringLiteral("grid-view-basic"),
                   QStringLiteral("GridView with uniform cells"),
                   QStringLiteral("Items align to a grid of equally sized cells."),
                   QStringLiteral("auto* gridView = new GridView(this);\n"
                                  "gridView->setModel(model);\n"
                                  "gridView->setCellSize(QSize(96, 72));\n"
                                  "gridView->setMaxColumns(4);"),
                   [](QWidget* parent) {
                       auto* gridView = new GridView(parent);
                       gridView->setFixedSize(440, 190);
                       gridView->setCellSize(QSize(96, 72));
                       gridView->setMaxColumns(4);
                       QStringList items;
                       for (int i = 1; i <= 8; ++i)
                           items.append(QStringLiteral("Item %1").arg(i));
                       gridView->setModel(makeStringListModel(gridView, items));
                       gridView->setSelectedIndex(0);
                       return gridView;
                   })
    };
}

QVector<GallerySample> listViewSamples()
{
    return {
        makeSample(QStringLiteral("list-view-basic"),
                   QStringLiteral("ListView with selection"),
                   QStringLiteral("A vertical list with single selection and an animated indicator."),
                   QStringLiteral("auto* listView = new ListView(this);\n"
                                  "listView->setModel(model);\n"
                                  "listView->setHeaderText(\"Contacts\");"),
                   [](QWidget* parent) {
                       auto* listView = new ListView(parent);
                       listView->setFixedSize(320, 220);
                       listView->setHeaderText(QStringLiteral("Contacts"));
                       listView->setModel(makeStringListModel(
                           listView,
                           {QStringLiteral("Kendall Collins"), QStringLiteral("Henry Ross"),
                            QStringLiteral("Nicole Wagner"), QStringLiteral("Adam Wolfe"),
                            QStringLiteral("Stephanie Meyer")}));
                       return listView;
                   })
    };
}

QVector<GallerySample> splitViewSamples()
{
    return {
        makeSample(QStringLiteral("split-view-basic"),
                   QStringLiteral("Resizable panes"),
                   QStringLiteral("Drag the handle between the panes to resize them."),
                   QStringLiteral("auto* splitView = new SplitView(this);\n"
                                  "splitView->addPane(firstPane);\n"
                                  "splitView->addPane(secondPane);\n"
                                  "splitView->setPanePreferredSize(0, 160);"),
                   [](QWidget* parent) {
                       auto* splitView = new SplitView(parent);
                       splitView->setFixedSize(420, 160);
                       splitView->addPane(makePastelPage(splitView, QStringLiteral("Pane 1"),
                                                         QStringLiteral("#DCE8F8")));
                       splitView->addPane(makePastelPage(splitView, QStringLiteral("Pane 2"),
                                                         QStringLiteral("#DFF3E3")));
                       splitView->setPanePreferredSize(0, 160);
                       splitView->setPaneFill(1, true);
                       return splitView;
                   })
    };
}

QVector<GallerySample> stackViewSamples()
{
    return {
        makeSample(QStringLiteral("stack-view-basic"),
                   QStringLiteral("Push and pop pages"),
                   QStringLiteral("StackView keeps a navigation stack of pages with transitions."),
                   QStringLiteral("auto* stackView = new StackView(this);\n"
                                  "stackView->setInitialItem(firstPage);\n"
                                  "stackView->push(nextPage);\n"
                                  "stackView->pop();"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 12);

                       auto* stackView = new StackView(group);
                       stackView->setFixedSize(360, 150);
                       stackView->setInitialItem(makePastelPage(stackView, QStringLiteral("Page 1"),
                                                                QStringLiteral("#DCE8F8")));

                       QWidget* buttons = horizontalGroup(group, 8);
                       auto* pushButton = new Button(QStringLiteral("Push page"), buttons);
                       pushButton->setFluentStyle(Button::Accent);
                       auto* popButton = new Button(QStringLiteral("Pop page"), buttons);
                       QObject::connect(pushButton, &Button::clicked,
                                        stackView, [stackView]() {
                                            static const QStringList palette{
                                                QStringLiteral("#DFF3E3"),
                                                QStringLiteral("#FBE8DC"),
                                                QStringLiteral("#F2E3F8")};
                                            const int depth = stackView->depth();
                                            stackView->push(makePastelPage(
                                                stackView,
                                                QStringLiteral("Page %1").arg(depth + 1),
                                                palette.at(depth % palette.size())));
                                        });
                       QObject::connect(popButton, &Button::clicked,
                                        stackView, [stackView]() { stackView->pop(); });
                       buttons->layout()->addWidget(pushButton);
                       buttons->layout()->addWidget(popButton);

                       group->layout()->addWidget(stackView);
                       group->layout()->addWidget(buttons);
                       return group;
                   })
    };
}

QVector<GallerySample> treeViewSamples()
{
    return {
        makeSample(QStringLiteral("tree-view-basic"),
                   QStringLiteral("Basic hierarchy"),
                   QStringLiteral("Expandable nodes reveal nested items on demand."),
                   QStringLiteral("auto* tree = new TreeView(this);\n"
                                  "tree->setModel(model);\n"
                                  "tree->expand(model->index(0, 0));"),
                   [](QWidget* parent) {
                       auto* tree = new TreeView(parent);
                       tree->setHeaderHidden(true);
                       tree->setMinimumHeight(220);

                       auto* model = new QStandardItemModel(tree);
                       auto branch = [](const QString& text, const QStringList& children) {
                           auto* node = new QStandardItem(text);
                           node->setEditable(false);
                           for (const QString& child : children) {
                               auto* leaf = new QStandardItem(child);
                               leaf->setEditable(false);
                               node->appendRow(leaf);
                           }
                           return node;
                       };
                       model->appendRow(branch(QStringLiteral("Work documents"),
                                               {QStringLiteral("Proposal.docx"),
                                                QStringLiteral("Budget.xlsx"),
                                                QStringLiteral("Notes.txt")}));
                       model->appendRow(branch(QStringLiteral("Photos"),
                                               {QStringLiteral("Trip.png"),
                                                QStringLiteral("Family.png")}));
                       tree->setModel(model);
                       tree->expand(model->index(0, 0));
                       return tree;
                   })
    };
}

} // namespace

QVector<GallerySample> collectionsSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("drawer-view"))
        return drawerViewSamples();
    if (routeId == QStringLiteral("flip-view"))
        return flipViewSamples();
    if (routeId == QStringLiteral("flow-view"))
        return flowViewSamples();
    if (routeId == QStringLiteral("grid-view"))
        return gridViewSamples();
    if (routeId == QStringLiteral("list-view"))
        return listViewSamples();
    if (routeId == QStringLiteral("split-view"))
        return splitViewSamples();
    if (routeId == QStringLiteral("stack-view"))
        return stackViewSamples();
    if (routeId == QStringLiteral("tree-view"))
        return treeViewSamples();
    return {};
}

} // namespace fluent::gallery
