#include "NavigationSamples.h"

#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>

#include "components/collections/ListView.h"
#include "components/navigation/Breadcrumb.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/Pivot.h"
#include "components/navigation/SelectorBar.h"
#include "components/navigation/StackContentHost.h"
#include "components/navigation/TabView.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::collections::ListView;
using fluent::navigation::Breadcrumb;
using fluent::navigation::NavigationView;
using fluent::navigation::Pivot;
using fluent::navigation::SelectorBar;
using fluent::navigation::StackContentHost;
using fluent::navigation::TabView;
using fluent::textfields::Label;
using samples::makeSample;
using samples::verticalGroup;

Label* makeStatusLabel(QWidget* parent, const QString& text)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(Typography::FontRole::Body);
    return label;
}

QVector<GallerySample> breadcrumbSamples()
{
    return {
        makeSample(QStringLiteral("breadcrumb-basic"),
                   QStringLiteral("Breadcrumb trail"),
                   QStringLiteral("Clicking an ancestor truncates the trail back to it."),
                   QStringLiteral("auto* breadcrumb = new Breadcrumb(this);\n"
                                  "breadcrumb->setItems({\"Home\", \"Documents\",\n"
                                  "                      \"Design\", \"Northwind\"});"),
                   [](QWidget* parent) {
                       auto* breadcrumb = new Breadcrumb(parent);
                       breadcrumb->setItems(QStringList{
                           QStringLiteral("Home"), QStringLiteral("Documents"),
                           QStringLiteral("Design"), QStringLiteral("Northwind"),
                           QStringLiteral("Images")});
                       breadcrumb->setMinimumWidth(380);
                       return breadcrumb;
                   })
    };
}

QVector<GallerySample> navigationViewSamples()
{
    return {
        makeSample(QStringLiteral("navigation-view-basic"),
                   QStringLiteral("Left pane with hosted content"),
                   QStringLiteral("Selecting a pane item swaps the page in the content host."),
                   QStringLiteral("auto* navView = new NavigationView(this);\n"
                                  "navView->setDisplayMode(NavigationView::DisplayMode::Left);\n"
                                  "navView->setMainChromeWidget(paneList);\n"
                                  "navView->contentHost()->insertPage(0, page);"),
                   [](QWidget* parent) {
                       auto* navView = new NavigationView(parent);
                       navView->setFixedSize(460, 240);
                       navView->setDisplayMode(NavigationView::DisplayMode::Left);
                       navView->setExpandedPaneWidth(150);
                       navView->setAnimationEnabled(false);

                       auto* paneList = new ListView(navView);
                       paneList->setBorderVisible(false);
                       paneList->setBackgroundVisible(false);
                       auto* paneModel = new QStandardItemModel(paneList);
                       const QStringList paneTitles{
                           QStringLiteral("Home"),
                           QStringLiteral("Apps"),
                           QStringLiteral("Games")};
                       for (const QString& title : paneTitles) {
                           auto* item = new QStandardItem(title);
                           item->setEditable(false);
                           paneModel->appendRow(item);
                       }
                       paneList->setModel(paneModel);
                       navView->setMainChromeWidget(paneList);

                       StackContentHost* contentHost = navView->contentHost();
                       for (int i = 0; i < paneTitles.size(); ++i) {
                           auto* page = new Label(
                               QStringLiteral("%1 page content").arg(paneTitles.at(i)), contentHost);
                           page->setFluentTypography(Typography::FontRole::Body);
                           page->setAlignment(Qt::AlignLeft | Qt::AlignTop);
                           page->setContentsMargins(16, 16, 16, 16);
                           contentHost->insertPage(i, page);
                       }
                       contentHost->setCurrentIndex(0, 0, false);
                       paneList->setSelectedIndex(0);
                       QObject::connect(paneList, &ListView::clicked,
                                        contentHost, [contentHost](const QModelIndex& index) {
                                            if (index.isValid())
                                                contentHost->setCurrentIndex(index.row());
                                        });
                       return navView;
                   })
    };
}

QVector<GallerySample> pivotSamples()
{
    return {
        makeSample(QStringLiteral("pivot-basic"),
                   QStringLiteral("Pivot headers with live selection"),
                   QStringLiteral("The underline indicator slides between the selected headers."),
                   QStringLiteral("auto* pivot = new Pivot(this);\n"
                                  "pivot->addItem(\"All\");\n"
                                  "pivot->addItem(\"Unread\");\n"
                                  "pivot->setSelectedIndex(0);"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 10);
                       auto* pivot = new Pivot(group);
                       pivot->addItem(QStringLiteral("All"));
                       pivot->addItem(QStringLiteral("Unread"));
                       pivot->addItem(QStringLiteral("Flagged"));
                       pivot->setSelectedIndex(0);
                       Label* status = makeStatusLabel(group, QStringLiteral("Showing: All"));
                       QObject::connect(pivot, &Pivot::selectedIndexChanged,
                                        status, [status, pivot](int index) {
                                            status->setText(QStringLiteral("Showing: %1")
                                                                .arg(pivot->itemAt(index).header));
                                        });
                       group->layout()->addWidget(pivot);
                       group->layout()->addWidget(status);
                       return group;
                   })
    };
}

QVector<GallerySample> selectorBarSamples()
{
    return {
        makeSample(QStringLiteral("selector-bar-basic"),
                   QStringLiteral("SelectorBar with live selection"),
                   QStringLiteral("A lightweight row of selectable items for view switching."),
                   QStringLiteral("auto* selector = new SelectorBar(this);\n"
                                  "selector->addItem(\"Recent\");\n"
                                  "selector->addItem(\"Shared\");\n"
                                  "selector->setSelectedIndex(0);"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 10);
                       auto* selector = new SelectorBar(group);
                       selector->addItem(QStringLiteral("Recent"));
                       selector->addItem(QStringLiteral("Shared"));
                       selector->addItem(QStringLiteral("Favorites"));
                       selector->setSelectedIndex(0);
                       Label* status = makeStatusLabel(group, QStringLiteral("View: Recent"));
                       QObject::connect(selector, &SelectorBar::selectedIndexChanged,
                                        status, [status, selector](int index) {
                                            status->setText(QStringLiteral("View: %1")
                                                                .arg(selector->itemAt(index).text));
                                        });
                       group->layout()->addWidget(selector);
                       group->layout()->addWidget(status);
                       return group;
                   })
    };
}

QVector<GallerySample> tabViewSamples()
{
    return {
        makeSample(QStringLiteral("tab-view-basic"),
                   QStringLiteral("Tabbed content"),
                   QStringLiteral("Selecting a tab swaps the hosted content below the strip."),
                   QStringLiteral("auto* tabView = new TabView(this);\n"
                                  "tabView->addTab(\"Overview\");\n"
                                  "connect(tabView, &TabView::currentChanged,\n"
                                  "        stack, &QStackedWidget::setCurrentIndex);"),
                   [](QWidget* parent) {
                       auto* container = verticalGroup(parent, 0);

                       auto* tabView = new TabView(container);
                       tabView->setAddTabButtonVisible(false);
                       tabView->setTabsClosable(false);

                       auto* contentHost = new StackContentHost(container);
                       const QStringList tabTitles{
                           QStringLiteral("Overview"),
                           QStringLiteral("Details"),
                           QStringLiteral("Activity")};
                       int pageIndex = 0;
                       for (const QString& title : tabTitles) {
                           tabView->addTab(title);
                           auto* page = new Label(
                               QStringLiteral("%1 content hosted by the selected tab.").arg(title),
                               contentHost);
                           page->setFluentTypography(Typography::FontRole::Body);
                           page->setAlignment(Qt::AlignLeft | Qt::AlignTop);
                           page->setWordWrap(true);
                           page->setContentsMargins(4, 12, 4, 4);
                           contentHost->insertPage(pageIndex, page);
                           ++pageIndex;
                       }
                       tabView->setSelectedIndex(0);
                       contentHost->setCurrentIndex(0, 0, false);
                       QObject::connect(tabView, &TabView::currentChanged,
                                        contentHost, [contentHost](int index) {
                                            if (index >= 0 && index < contentHost->count())
                                                contentHost->setCurrentIndex(index);
                                        });

                       container->layout()->addWidget(tabView);
                       container->layout()->addWidget(contentHost);
                       return container;
                   })
    };
}

} // namespace

QVector<GallerySample> navigationSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("breadcrumb"))
        return breadcrumbSamples();
    if (routeId == QStringLiteral("navigation-view"))
        return navigationViewSamples();
    if (routeId == QStringLiteral("pivot"))
        return pivotSamples();
    if (routeId == QStringLiteral("selector-bar"))
        return selectorBarSamples();
    if (routeId == QStringLiteral("tab-view"))
        return tabViewSamples();
    return {};
}

} // namespace fluent::gallery
