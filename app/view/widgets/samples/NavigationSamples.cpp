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
using fluent::navigation::SelectorBarItem;
using fluent::navigation::StackContentHost;
using fluent::navigation::TabView;
using fluent::textfields::Label;
using samples::glyphPixmap;
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
                   QStringLiteral("Clicking an ancestor truncates the trail back to it; the label echoes the click."),
                   QStringLiteral("auto* breadcrumb = new Breadcrumb(this);\n"
                                  "breadcrumb->setItems({\"Home\", \"Documents\",\n"
                                  "                      \"Design\", \"Northwind\", \"Images\"});\n"
                                  "connect(breadcrumb, &Breadcrumb::itemClicked,\n"
                                  "        this, [](int index) { /* navigate */ });"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 10);
                       auto* breadcrumb = new Breadcrumb(group);
                       const QStringList trail{
                           QStringLiteral("Home"), QStringLiteral("Documents"),
                           QStringLiteral("Design"), QStringLiteral("Northwind"),
                           QStringLiteral("Images")};
                       breadcrumb->setItems(trail);
                       breadcrumb->setMinimumWidth(380);
                       Label* status = makeStatusLabel(
                           group, QStringLiteral("Current folder: Images"));
                       QObject::connect(breadcrumb, &Breadcrumb::itemClicked,
                                        status, [status, breadcrumb](int index) {
                                            status->setText(QStringLiteral("Current folder: %1")
                                                                .arg(breadcrumb->items().value(index).text));
                                        });
                       group->layout()->addWidget(breadcrumb);
                       group->layout()->addWidget(status);
                       return group;
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
                       paneList->setIconSize(QSize(20, 20));
                       auto* paneModel = new QStandardItemModel(paneList);
                       struct PaneEntry { QString title; QString glyph; };
                       const QVector<PaneEntry> paneEntries{
                           {QStringLiteral("Home"), Typography::Icons::Home},
                           {QStringLiteral("Apps"), Typography::Icons::AllApps},
                           {QStringLiteral("Games"), Typography::Icons::Controller}};
                       const QColor paneIconColor(0x00, 0x78, 0xD4);
                       for (const PaneEntry& entry : paneEntries) {
                           auto* item = new QStandardItem(entry.title);
                           item->setEditable(false);
                           item->setIcon(glyphPixmap(entry.glyph, paneIconColor, 20));
                           paneModel->appendRow(item);
                       }
                       paneList->setModel(paneModel);
                       navView->setMainChromeWidget(paneList);

                       StackContentHost* contentHost = navView->contentHost();
                       for (int i = 0; i < paneEntries.size(); ++i) {
                           auto* page = new Label(
                               QStringLiteral("%1 page content").arg(paneEntries.at(i).title),
                               contentHost);
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
                                  "pivot->addItem(\"Flagged\");\n"
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
                   QStringLiteral("SelectorBar with icon items"),
                   QStringLiteral("A lightweight row of selectable items for view switching."),
                   QStringLiteral("auto* selector = new SelectorBar(this);\n"
                                  "selector->addItem({\"Recent\", Typography::Icons::History});\n"
                                  "selector->addItem({\"Shared\", Typography::Icons::Share});\n"
                                  "selector->addItem({\"Favorites\", Typography::Icons::FavoriteStar});\n"
                                  "selector->setSelectedIndex(0);"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 10);
                       auto* selector = new SelectorBar(group);
                       selector->addItem(SelectorBarItem(QStringLiteral("Recent"),
                                                         Typography::Icons::History));
                       selector->addItem(SelectorBarItem(QStringLiteral("Shared"),
                                                         Typography::Icons::Share));
                       selector->addItem(SelectorBarItem(QStringLiteral("Favorites"),
                                                         Typography::Icons::FavoriteStar));
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
                   QStringLiteral("Closable tabs with an add button"),
                   QStringLiteral("Selecting a tab swaps the hosted content; tabs can be closed and added."),
                   QStringLiteral("auto* tabView = new TabView(this);\n"
                                  "tabView->setTabsClosable(true);\n"
                                  "tabView->setAddTabButtonVisible(true);\n"
                                  "connect(tabView, &TabView::addTabRequested,\n"
                                  "        this, [tabView] { tabView->addTab(\"New tab\"); });\n"
                                  "connect(tabView, &TabView::tabCloseRequested,\n"
                                  "        this, [tabView](int i) { tabView->closeTab(i); });"),
                   [](QWidget* parent) {
                       auto* container = verticalGroup(parent, 0);

                       auto* tabView = new TabView(container);
                       tabView->setAddTabButtonVisible(true);
                       tabView->setTabsClosable(true);

                       auto* contentHost = new StackContentHost(container);
                       auto makePage = [contentHost](const QString& title) {
                           auto* page = new Label(
                               QStringLiteral("%1 content hosted by the selected tab.").arg(title),
                               contentHost);
                           page->setFluentTypography(Typography::FontRole::Body);
                           page->setAlignment(Qt::AlignLeft | Qt::AlignTop);
                           page->setWordWrap(true);
                           page->setContentsMargins(4, 12, 4, 4);
                           return page;
                       };
                       const QStringList tabTitles{
                           QStringLiteral("Overview"),
                           QStringLiteral("Details"),
                           QStringLiteral("Activity")};
                       int pageIndex = 0;
                       for (const QString& title : tabTitles) {
                           tabView->addTab(title);
                           contentHost->insertPage(pageIndex, makePage(title));
                           ++pageIndex;
                       }
                       tabView->setSelectedIndex(0);
                       contentHost->setCurrentIndex(0, 0, false);
                       QObject::connect(tabView, &TabView::currentChanged,
                                        contentHost, [contentHost](int index) {
                                            if (index >= 0 && index < contentHost->count())
                                                contentHost->setCurrentIndex(index);
                                        });
                       // The add button appends a fresh document tab plus its page.
                       // zh_CN: 加号按钮追加一个新的文档页签及其页面。
                       QObject::connect(tabView, &TabView::addTabRequested,
                                        contentHost, [tabView, contentHost, makePage]() {
                                            static int documentNumber = 0;
                                            const QString title = QStringLiteral("Document %1")
                                                                      .arg(++documentNumber);
                                            const int index = tabView->addTab(title);
                                            contentHost->insertPage(index, makePage(title));
                                            tabView->setSelectedIndex(index);
                                        });
                       // Closing a tab removes its hosted page as well.
                       // zh_CN: 关闭页签时同步移除承载的页面。
                       QObject::connect(tabView, &TabView::tabCloseRequested,
                                        contentHost, [tabView, contentHost](int index) {
                                            tabView->closeTab(index);
                                            if (QWidget* page = contentHost->takePage(index))
                                                page->deleteLater();
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
