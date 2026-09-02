#include "platform/wasm/WasmSmokeRunner.h"

#include <FluentQt/WebAssembly.h>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QEvent>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPointer>
#include <QRegion>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QSizePolicy>
#include <QTimer>
#include <QUrlQuery>

#include <emscripten/emscripten.h>
#include <emscripten/heap.h>

#include "components/basicinput/Button.h"
#include "components/basicinput/ComboBox.h"
#include "components/basicinput/MultiSelectComboBox.h"
#include "components/collections/DataGrid.h"
#include "components/collections/ListView.h"
#include "components/dialogs_flyouts/Dialog.h"
#include "components/dialogs_flyouts/TeachingTip.h"
#include "components/menus_toolbars/Menu.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/StackContentHost.h"
#include "components/navigation/TabView.h"
#include "components/textfields/LineEdit.h"
#include "components/textfields/Label.h"
#include "components/textfields/PasswordBox.h"
#include "components/windowing/TitleBar.h"
#include "components/windowing/Window.h"
#include "compatibility/FontCompat.h"
#include "support/logging/Log.h"
#include "view/pages/GalleryContentPage.h"
#include "view/pages/SettingsPage.h"
#include "view/shell/GalleryWindow.h"
#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {
namespace {

QString smokeMode()
{
    const char* rawSearch = emscripten_run_script_string("window.location.search");
    QString search = QString::fromUtf8(rawSearch ? rawSearch : "");
    if (search.startsWith(QLatin1Char('?')))
        search.remove(0, 1);
    return QUrlQuery(search).queryItemValue(QStringLiteral("wasm-smoke")).toLower();
}

void publishSmokeState(const char* state, const QString& detail = {})
{
    const QByteArray encodedDetail = detail.toUtf8();
    // clang-format off
    EM_ASM({
        const state = UTF8ToString($0);
        const detail = UTF8ToString($1);
        document.documentElement.dataset.fluentQtSmoke = state;
        document.documentElement.dataset.fluentQtSmokeDetail = detail;
        console.log(`FLUENT_QT_WASM_SMOKE_${state.toUpperCase()}: ${detail}`);
    }, state, encodedDetail.constData());
    // clang-format on
}

void publishBrowserTextInputProbe(const char* state, const QPoint& globalPosition = QPoint(-1, -1),
                                  const QString& expectedText = {})
{
    const QByteArray encodedText = expectedText.toUtf8();
    // clang-format off
    EM_ASM({
        const root = document.documentElement;
        root.dataset.fluentQtTextInputState = UTF8ToString($0);
        root.dataset.fluentQtTextInputX = String($1);
        root.dataset.fluentQtTextInputY = String($2);
        root.dataset.fluentQtTextInputExpected = UTF8ToString($3);
    }, state, globalPosition.x(), globalPosition.y(), encodedText.constData());
    // clang-format on
}

qint64 heapCapacityMiB()
{
    constexpr size_t bytesPerMiB = 1024U * 1024U;
    return static_cast<qint64>(emscripten_get_heap_size() / bytesPerMiB);
}

qint64 heapBreakMiB()
{
    constexpr uintptr_t bytesPerMiB = 1024U * 1024U;
    const uintptr_t* const heapBreak = emscripten_get_sbrk_ptr();
    return heapBreak ? static_cast<qint64>(*heapBreak / bytesPerMiB) : 0;
}

const QStringList& changedSampleProbeRoutes()
{
    static const QStringList routes{
        QStringLiteral("multi-select-combobox"), QStringLiteral("list-view"),
        QStringLiteral("navigation-view"),       QStringLiteral("tab-view"),
        QStringLiteral("teaching-tip"),
    };
    return routes;
}

bool rejectSmoke(QString* failure, const QString& reason)
{
    if (failure)
        *failure = reason;
    return false;
}

QRect globalWidgetRect(const QWidget* widget)
{
    return widget ? QRect(widget->mapToGlobal(QPoint(0, 0)), widget->size()) : QRect();
}

QString rectSummary(const QRect& rect)
{
    return QStringLiteral("%1,%2 %3x%4")
        .arg(rect.x())
        .arg(rect.y())
        .arg(rect.width())
        .arg(rect.height());
}

bool widgetContainedIn(const QWidget* widget, const QWidget* container, int tolerance = 0)
{
    if (!widget || !container)
        return false;
    return globalWidgetRect(container)
        .adjusted(-tolerance, -tolerance, tolerance, tolerance)
        .contains(globalWidgetRect(widget));
}

QWidget* samplePreviewSurface(QWidget* widget)
{
    for (QWidget* current = widget ? widget->parentWidget() : nullptr; current;
         current = current->parentWidget()) {
        if (current->objectName() == QStringLiteral("gallerySampleCardPreview")) {
            return current;
        }
    }
    return nullptr;
}

class WasmSmokeRunner final : public QObject {
public:
    WasmSmokeRunner(GalleryWindow* window, bool full)
        : QObject(window), m_window(window), m_requireDataGridInteraction(full),
          m_requireChangedSampleProbes(full)
    {
        const QStringList available = window->navigationEntryIds();
        if (full) {
            m_routes = available;
        } else if (!available.isEmpty()) {
            auto appendUnique = [this, &available](int index) {
                if (index < 0 || index >= available.size())
                    return;
                const QString routeId = available.at(index);
                if (!m_routes.contains(routeId))
                    m_routes.append(routeId);
            };
            appendUnique(0);
            appendUnique(available.size() / 3);
            appendUnique((available.size() * 2) / 3);
            appendUnique(available.size() - 1);
        }
    }

    void start()
    {
        qApp->setProperty("fluentqtGalleryAutomated", true);
        m_initialHeapMiB = heapCapacityMiB();
        m_initialHeapBreakMiB = heapBreakMiB();
        m_totalTimer.start();
        publishSmokeState("running", QStringLiteral("route traversal started"));
        QTimer::singleShot(0, this, [this]() { visitNextRoute(); });
    }

private:
    bool currentRouteReady(const QString& routeId) const
    {
        if (!m_window || m_window->currentRouteId() != routeId)
            return false;
        if (routeId == QStringLiteral("settings")) {
            SettingsPage* page = m_window->currentSettingsPage();
            return page && page->routeId() == routeId && page->isVisible();
        }
        GalleryContentPage* page = m_window->currentContentPage();
        return page && page->routeId() == routeId && page->isVisible();
    }

    void visitNextRoute()
    {
        if (!m_window)
            return fail(QStringLiteral("Gallery window was destroyed"));
        if (m_routeIndex >= m_routes.size())
            return runRuntimeChecks();

        m_currentRoute = m_routes.at(m_routeIndex++);
        m_routeTimer.restart();
        if (!m_window->selectRoute(m_currentRoute))
            return fail(QStringLiteral("Could not select route %1").arg(m_currentRoute));
        waitForCurrentRoute();
    }

    void waitForCurrentRoute()
    {
        if (currentRouteReady(m_currentRoute)) {
            QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            QApplication::processEvents();
            if (m_currentRoute == QStringLiteral("settings")) {
                SettingsPage* settingsPage = m_window->currentSettingsPage();
                auto* motionChoice = settingsPage
                                         ? settingsPage->findChild<fluent::basicinput::ComboBox*>(
                                               QStringLiteral("gallerySettingsMotionChoice"))
                                         : nullptr;
                if (!motionChoice || motionChoice->count() != 3 ||
                    motionChoice->currentIndex() !=
                        static_cast<int>(GallerySettings::instance().motionMode())) {
                    return fail(QStringLiteral(
                        "Settings route is missing the synchronized Motion policy choice"));
                }
            }
            if (m_currentRoute == QStringLiteral("data-grid")) {
                QString failure;
                if (!verifyDataGridRoute(&failure))
                    return fail(failure);
                m_dataGridInteractionPassed = true;
            }
            if (changedSampleProbeRoutes().contains(m_currentRoute)) {
                QString failure;
                if (m_currentRoute == QStringLiteral("teaching-tip")) {
                    if (!beginTeachingTipRouteProbe(&failure))
                        return fail(failure);
                    return;
                }
                if (!verifyChangedSampleRoute(&failure))
                    return fail(failure);
                m_changedSampleRoutesPassed.insert(m_currentRoute);
            }
            finishCurrentRoute();
            return;
        }
        if (m_routeTimer.elapsed() > 30000)
            return fail(QStringLiteral("Timed out waiting for route %1").arg(m_currentRoute));
        QTimer::singleShot(25, this, [this]() { waitForCurrentRoute(); });
    }

    void finishCurrentRoute()
    {
        const qint64 routeMs = m_routeTimer.elapsed();
        if (routeMs > m_slowestRouteMs) {
            m_slowestRouteMs = routeMs;
            m_slowestRoute = m_currentRoute;
        }
        LOG_INFO(QStringLiteral("WasmSmoke route ready id=%1 index=%2 total=%3 elapsedMs=%4")
                     .arg(m_currentRoute)
                     .arg(m_routeIndex)
                     .arg(m_routes.size())
                     .arg(routeMs));
        QTimer::singleShot(0, this, [this]() { visitNextRoute(); });
    }

    bool verifyDataGridRoute(QString* failure) const
    {
        auto reject = [failure](const QString& reason) {
            if (failure)
                *failure = reason;
            return false;
        };
        GalleryContentPage* page = m_window ? m_window->currentContentPage() : nullptr;
        if (!page)
            return reject(QStringLiteral("DataGrid Gallery page is unavailable"));

        const auto grids = page->findChildren<fluent::collections::DataGrid*>();
        if (grids.size() < 3)
            return reject(QStringLiteral("DataGrid Gallery samples are incomplete"));

        fluent::collections::DataGrid* largeGrid = nullptr;
        fluent::collections::DataGrid* selectionGrid = nullptr;
        fluent::collections::DataGrid* editingGrid = nullptr;
        for (auto* grid : grids) {
            if (!grid || !grid->model())
                continue;
            if (grid->model()->rowCount() >= 100000)
                largeGrid = grid;
            if (grid->selectionMode() == fluent::collections::DataGrid::SelectionMode::Extended) {
                selectionGrid = grid;
            }
            if (grid->editTriggers() != QAbstractItemView::NoEditTriggers)
                editingGrid = grid;
        }
        if (!largeGrid || !selectionGrid || !editingGrid)
            return reject(QStringLiteral(
                "DataGrid Gallery scenarios did not expose scale, selection, and editing views"));
        if (!largeGrid->isScrollChainingEnabled() || !selectionGrid->isScrollChainingEnabled() ||
            !editingGrid->isScrollChainingEnabled()) {
            return reject(
                QStringLiteral("DataGrid Gallery samples do not share boundary scroll chaining"));
        }

        QScrollBar* scrollBar = largeGrid->verticalScrollBar();
        if (!scrollBar || scrollBar->maximum() <= scrollBar->minimum())
            return reject(QStringLiteral("DataGrid large-model scrollbar is not usable"));
        scrollBar->setValue(scrollBar->maximum());
        QApplication::processEvents();
        if (scrollBar->value() != scrollBar->maximum())
            return reject(QStringLiteral("DataGrid large-model scrolling did not reach the end"));

        const QModelIndex first = selectionGrid->model()->index(0, 0);
        selectionGrid->setCurrentIndex(first);
        selectionGrid->setFocus(Qt::OtherFocusReason);
        QKeyEvent downEvent(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QApplication::sendEvent(selectionGrid, &downEvent);
        if (selectionGrid->currentIndex().row() != 1)
            return reject(QStringLiteral("DataGrid keyboard selection did not advance"));

        const QModelIndex editableIndex = editingGrid->model()->index(0, 1);
        editingGrid->setCurrentIndex(editableIndex);
        editingGrid->edit(editableIndex);
        QApplication::processEvents();
        QLineEdit* editor = editingGrid->findChild<QLineEdit*>();
        if (!editor || !editor->isVisible())
            return reject(QStringLiteral("DataGrid delegate editor did not open"));
        const QString committedValue = QStringLiteral("browser-edit");
        editor->setText(committedValue);
        QKeyEvent commitEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(editor, &commitEvent);
        QApplication::processEvents();
        if (editableIndex.data(Qt::EditRole).toString() != committedValue)
            return reject(QStringLiteral("DataGrid delegate editor did not commit"));

        return true;
    }

    bool verifyChangedSampleRoute(QString* failure) const
    {
        if (m_currentRoute == QStringLiteral("multi-select-combobox"))
            return verifyMultiSelectRoute(failure);
        if (m_currentRoute == QStringLiteral("list-view"))
            return verifyListViewRoute(failure);
        if (m_currentRoute == QStringLiteral("navigation-view"))
            return verifyNavigationViewRoute(failure);
        if (m_currentRoute == QStringLiteral("tab-view"))
            return verifyTabViewRoute(failure);
        if (m_currentRoute == QStringLiteral("teaching-tip")) {
            return rejectSmoke(
                failure,
                QStringLiteral("TeachingTip changed-sample probe must run asynchronously"));
        }
        return rejectSmoke(failure,
                           QStringLiteral("No changed-sample probe is registered for route %1")
                               .arg(m_currentRoute));
    }

    bool verifyMultiSelectRoute(QString* failure) const
    {
        GalleryContentPage* page = m_window ? m_window->currentContentPage() : nullptr;
        if (!page) {
            return rejectSmoke(failure,
                               QStringLiteral("MultiSelectComboBox Gallery page is unavailable"));
        }

        fluent::basicinput::MultiSelectComboBox* box = nullptr;
        const auto boxes = page->findChildren<fluent::basicinput::MultiSelectComboBox*>();
        for (auto* candidate : boxes) {
            if (candidate && candidate->accessibleName() == QStringLiteral("Teams")) {
                box = candidate;
                break;
            }
        }
        if (!box || !box->model() || box->model()->rowCount() != 4 || box->selectedCount() != 2 ||
            box->width() != 280) {
            return rejectSmoke(
                failure, QStringLiteral("MultiSelectComboBox primary sample state is incomplete"));
        }

        auto* scrollArea =
            page->findChild<QScrollArea*>(QStringLiteral("galleryContentScrollArea"));
        if (!scrollArea) {
            return rejectSmoke(
                failure, QStringLiteral("MultiSelectComboBox page scroll host is unavailable"));
        }
        scrollArea->ensureWidgetVisible(box, 24, 48);
        QApplication::processEvents();
        box->open();
        QApplication::processEvents();

        QWidget* popupHost = box->window();
        auto* popup =
            popupHost ? popupHost->findChild<QWidget*>(QStringLiteral("MultiSelectComboBox.Popup"))
                      : nullptr;
        auto* listView = popup ? popup->findChild<QAbstractItemView*>(
                                     QStringLiteral("MultiSelectComboBox.ListView"))
                               : nullptr;
        const bool wasOpen = box->isOpen();
        const bool popupWasVisible = popup && popup->isVisible();
        const bool valid = wasOpen && popup && popupWasVisible && listView && listView->model() &&
                           listView->model()->rowCount() == 4 && popupHost &&
                           widgetContainedIn(popup, popupHost, 1) &&
                           widgetContainedIn(listView, popup, 1);
        box->close();
        QApplication::processEvents();
        if (!valid) {
            return rejectSmoke(
                failure,
                QStringLiteral("MultiSelectComboBox browser popup state or geometry is invalid "
                               "open=%1 popup=%2 visible=%3 list=%4 rows=%5 "
                               "popupRect=%6 windowRect=%7 listRect=%8")
                    .arg(wasOpen)
                    .arg(popup != nullptr)
                    .arg(popupWasVisible)
                    .arg(listView != nullptr)
                    .arg(listView && listView->model() ? listView->model()->rowCount() : -1)
                    .arg(popup ? rectSummary(globalWidgetRect(popup)) : QStringLiteral("<none>"))
                    .arg(popupHost ? rectSummary(globalWidgetRect(popupHost))
                                   : QStringLiteral("<none>"))
                    .arg(listView ? rectSummary(globalWidgetRect(listView))
                                  : QStringLiteral("<none>")));
        }
        return true;
    }

    bool verifyListViewRoute(QString* failure) const
    {
        GalleryContentPage* page = m_window ? m_window->currentContentPage() : nullptr;
        if (!page)
            return rejectSmoke(failure, QStringLiteral("ListView Gallery page is unavailable"));

        struct ListCase {
            QString accessibleName;
            int rowCount;
            int selectedRows;
        };
        const auto views = page->findChildren<fluent::collections::ListView*>();
        for (const ListCase& listCase : {ListCase{QStringLiteral("Contacts"), 12, 1},
                                         ListCase{QStringLiteral("Message filters"), 9, 2}}) {
            fluent::collections::ListView* listView = nullptr;
            for (auto* candidate : views) {
                if (candidate && candidate->accessibleName() == listCase.accessibleName) {
                    listView = candidate;
                    break;
                }
            }
            QWidget* previewSurface = samplePreviewSurface(listView);
            if (!listView || !previewSurface || !listView->model() ||
                listView->model()->rowCount() != listCase.rowCount ||
                listView->size() != QSize(320, 234) ||
                listView->selectionModel()->selectedRows().size() != listCase.selectedRows ||
                !widgetContainedIn(listView, previewSurface, 1)) {
                return rejectSmoke(
                    failure,
                    QStringLiteral("ListView sample state or containing surface is invalid: %1")
                        .arg(listCase.accessibleName));
            }

            listView->doItemsLayout();
            const QRect viewportRect = listView->viewport()->rect();
            int visibleRows = 0;
            for (int row = 0; row < listView->model()->rowCount(); ++row) {
                const QRect rowRect = static_cast<QAbstractItemView*>(listView)->visualRect(
                    listView->model()->index(row, 0));
                if (!viewportRect.intersects(rowRect))
                    continue;
                ++visibleRows;
                if (!viewportRect.contains(rowRect)) {
                    return rejectSmoke(
                        failure,
                        QStringLiteral("ListView browser viewport clips a visible row: %1/%2")
                            .arg(listCase.accessibleName)
                            .arg(row));
                }
            }
            if (visibleRows != 5) {
                return rejectSmoke(
                    failure,
                    QStringLiteral("ListView browser viewport expected 5 complete rows: %1/%2")
                        .arg(listCase.accessibleName)
                        .arg(visibleRows));
            }
        }
        return true;
    }

    bool verifyNavigationViewRoute(QString* failure) const
    {
        GalleryContentPage* page = m_window ? m_window->currentContentPage() : nullptr;
        if (!page) {
            return rejectSmoke(failure,
                               QStringLiteral("NavigationView Gallery page is unavailable"));
        }

        struct NavigationCase {
            QString objectName;
            int height;
        };
        for (const NavigationCase& navigationCase :
             {NavigationCase{QStringLiteral("navigationViewChromeSlotsPreview"), 340},
              NavigationCase{QStringLiteral("navigationViewDisplayModesPreview"), 340},
              NavigationCase{QStringLiteral("navigationViewContentHostPreview"), 320}}) {
            auto* navigation =
                page->findChild<fluent::navigation::NavigationView*>(navigationCase.objectName);
            QWidget* previewSurface = samplePreviewSurface(navigation);
            QWidget* contentHost = navigation ? navigation->contentHost() : nullptr;
            if (!navigation || !previewSurface || !contentHost || navigation->width() < 440 ||
                navigation->width() > 620 || navigation->height() != navigationCase.height ||
                navigation->sizePolicy().horizontalPolicy() != QSizePolicy::Expanding ||
                !widgetContainedIn(navigation, previewSurface, 1) ||
                !widgetContainedIn(contentHost, navigation, 1)) {
                return rejectSmoke(
                    failure,
                    QStringLiteral(
                        "NavigationView browser sample is clipped or incorrectly sized: %1")
                        .arg(navigationCase.objectName));
            }
        }
        return true;
    }

    bool verifyTabViewRoute(QString* failure) const
    {
        GalleryContentPage* page = m_window ? m_window->currentContentPage() : nullptr;
        if (!page)
            return rejectSmoke(failure, QStringLiteral("TabView Gallery page is unavailable"));

        auto* surface = page->findChild<QWidget*>(QStringLiteral("tabViewHostedPagesSurface"));
        auto* tabs =
            page->findChild<fluent::navigation::TabView*>(QStringLiteral("tabViewHostedPagesTabs"));
        auto* host = page->findChild<QWidget*>(QStringLiteral("tabViewHostedPagesHost"));
        QWidget* previewSurface = samplePreviewSurface(surface);
        if (!surface || !tabs || !host || !previewSurface || surface->width() < 360 ||
            surface->width() > 560 || surface->height() != 186 || tabs->height() != 40 ||
            host->height() != 146 || tabs->tabCount() != 3 || tabs->selectedIndex() != 0 ||
            tabs->visibleTabIndexes().size() != 3 ||
            !widgetContainedIn(surface, previewSurface, 1) ||
            !widgetContainedIn(tabs, surface, 1) || !widgetContainedIn(host, surface, 1)) {
            return rejectSmoke(
                failure, QStringLiteral("TabView browser sample state or geometry is invalid"));
        }
        for (int index = 0; index < tabs->tabCount(); ++index) {
            const QRect tabRect = tabs->tabGeometry(index);
            if (tabRect.isEmpty() || !tabs->rect().contains(tabRect)) {
                return rejectSmoke(
                    failure,
                    QStringLiteral("TabView browser tab geometry is clipped: %1").arg(index));
            }
        }
        const auto labels = tabs->findChildren<fluent::textfields::Label*>();
        QSet<QString> expectedLabels{
            QStringLiteral("Home"),
            QStringLiteral("Details"),
            QStringLiteral("Activity"),
        };
        if (labels.size() != tabs->tabCount()) {
            return rejectSmoke(failure,
                               QStringLiteral("TabView browser sample exposed %1/%2 labels")
                                   .arg(labels.size())
                                   .arg(tabs->tabCount()));
        }
        for (const auto* label : labels) {
            if (!label || !label->isVisible() || label->isTextElided() ||
                !expectedLabels.remove(label->text())) {
                return rejectSmoke(
                    failure,
                    QStringLiteral(
                        "TabView browser label is missing, hidden, duplicated, or elided: %1")
                        .arg(label ? label->text() : QStringLiteral("<null>")));
            }
        }
        if (!expectedLabels.isEmpty()) {
            const QStringList missingLabels(expectedLabels.values());
            return rejectSmoke(failure, QStringLiteral("TabView browser labels are incomplete: %1")
                                            .arg(missingLabels.join(QStringLiteral(", "))));
        }
        return true;
    }

    bool beginTeachingTipRouteProbe(QString* failure)
    {
        GalleryContentPage* page = m_window ? m_window->currentContentPage() : nullptr;
        if (!page) {
            return rejectSmoke(failure, QStringLiteral("TeachingTip Gallery page is unavailable"));
        }

        auto* anchor =
            page->findChild<fluent::basicinput::Button*>(QStringLiteral("teachingTipTopAnchor"));
        auto* scrollArea =
            page->findChild<QScrollArea*>(QStringLiteral("galleryContentScrollArea"));
        if (!anchor || !scrollArea) {
            return rejectSmoke(failure,
                               QStringLiteral("TeachingTip placement sample is incomplete"));
        }

        scrollArea->ensureWidgetVisible(anchor, 24, 160);
        QApplication::processEvents();
        anchor->click();
        QApplication::processEvents();

        QWidget* tipHost = anchor->window();
        auto* tip = tipHost ? tipHost->findChild<fluent::dialogs_flyouts::TeachingTip*>(
                                  QStringLiteral("teachingTipPlacementPreview"))
                            : nullptr;
        if (!tip || !tipHost) {
            return rejectSmoke(
                failure, QStringLiteral("TeachingTip browser placement instance is unavailable"));
        }

        m_teachingTipOpened = qFuzzyCompare(tip->popupProgress(), 1.0);
        QPointer<fluent::dialogs_flyouts::TeachingTip> tipGuard(tip);
        QPointer<fluent::basicinput::Button> anchorGuard(anchor);
        QPointer<QWidget> hostGuard(tipHost);
        QPointer<GalleryContentPage> pageGuard(page);
        connect(tip, &fluent::dialogs_flyouts::Popup::opened, this, [this, tipGuard]() {
            if (tipGuard)
                m_teachingTipOpened = true;
        });
        QTimer::singleShot(
            400, this, [this, tipGuard, anchorGuard, hostGuard, pageGuard]() mutable {
                if (!tipGuard || !anchorGuard || !hostGuard || !pageGuard) {
                    fail(QStringLiteral(
                        "TeachingTip browser placement lifetime ended before settle"));
                    return;
                }

                bool statusUpdated = false;
                const auto statusLabels = pageGuard->findChildren<fluent::textfields::Label*>();
                for (const auto* label : statusLabels) {
                    if (label && label->text() == QStringLiteral("Placement: Top, tail on")) {
                        statusUpdated = true;
                        break;
                    }
                }
                const bool settled = qFuzzyCompare(tipGuard->popupProgress(), 1.0);
                const bool valid =
                    m_teachingTipOpened && settled && statusUpdated && tipGuard->isOpen() &&
                    tipGuard->isVisible() && tipGuard->target() == anchorGuard &&
                    tipGuard->preferredPlacement() == fluent::dialogs_flyouts::TeachingTip::Top &&
                    tipGuard->cardSize() == QSize(300, 136) && tipGuard->isTailVisible() &&
                    tipGuard->isLightDismissEnabled() &&
                    tipGuard->accessibleName() == QStringLiteral("Top placement tip") &&
                    tipGuard->contentHost() && widgetContainedIn(tipGuard, hostGuard, 1) &&
                    widgetContainedIn(tipGuard->contentHost(), tipGuard, 1);

                tipGuard->setExitAnimationEnabled(false);
                tipGuard->close();
                QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                QApplication::processEvents();
                QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                const bool deletedAfterClose = tipGuard.isNull();
                if (!valid || !deletedAfterClose) {
                    fail(QStringLiteral("TeachingTip browser state, settle, geometry, status, or "
                                        "lifetime is invalid "
                                        "opened=%1 settled=%2 status=%3 deleted=%4")
                             .arg(m_teachingTipOpened)
                             .arg(settled)
                             .arg(statusUpdated)
                             .arg(deletedAfterClose));
                    return;
                }
                m_changedSampleRoutesPassed.insert(m_currentRoute);
                finishCurrentRoute();
            });
        return true;
    }

    void runRuntimeChecks()
    {
        QWidget* browserSurface = m_window->window();
        if (!browserSurface ||
            browserSurface->windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus) ||
            browserSurface->testAttribute(Qt::WA_ShowWithoutActivating) ||
            !m_window->customWindowChromeEnabled() || !m_window->titleBar() ||
            !m_window->titleBar()->isVisible() || !m_window->titleBar()->isWindowActive() ||
            !m_window->titleBar()->testAttribute(Qt::WA_OpaquePaintEvent) ||
            m_window->property("fluentPaintedSurfaceCacheGeneration").toInt() <= 0 ||
            !m_window->findChild<QWidget*>(QStringLiteral("fluentWindowFrameHost"))) {
            return fail(QStringLiteral("Browser Gallery host is non-focusable or missing "
                                       "opaque/cached Fluent window chrome"));
        }

        auto& settings = GallerySettings::instance();
        settings.setThemeMode(GallerySettings::ThemeMode::Light);
        QSettings storage(QSettings::WebLocalStorageFormat, QSettings::UserScope,
                          QCoreApplication::organizationName(),
                          QCoreApplication::applicationName());
        storage.sync();
        if (storage.value(QStringLiteral("settings/themeMode"), -1).toInt() !=
            static_cast<int>(GallerySettings::ThemeMode::Light)) {
            return fail(QStringLiteral("WebLocalStorage theme persistence failed"));
        }

        const QStringList hanFallbacks =
            QFontDatabase::applicationFallbackFontFamilies(QChar::Script_Han);
        if (!hanFallbacks.contains(fluent::fontcompat::UISimplifiedChineseFamily)) {
            return fail(
                QStringLiteral("Simplified Chinese application font fallback was not registered"));
        }
        const QFontMetrics fallbackMetrics{QFont(fluent::fontcompat::UISimplifiedChineseFamily)};
        if (!fallbackMetrics.inFont(QChar(0x6708)) || !fallbackMetrics.inFont(QChar(0x5468))) {
            return fail(QStringLiteral("Simplified Chinese fallback is missing calendar glyphs"));
        }

        runWindowCheck();
    }

    void runWindowCheck()
    {
        auto* window = new fluent::windowing::Window();
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->setWindowTitle(QStringLiteral("Web window smoke"));
        const QRect normalGeometry(m_window->geometry().center() - QPoint(320, 260),
                                   QSize(640, 520));
        fluent::webassembly::showWindow(window, normalGeometry);

        QTimer::singleShot(150, window, [this, window, normalGeometry]() {
            const bool valid =
                window->isVisible() && window->customWindowChromeEnabled() && window->titleBar() &&
                window->titleBar()->isVisible() &&
                window->findChild<QWidget*>(QStringLiteral("fluentWindowFrameHost")) &&
                window->width() >= window->minimumWidth() &&
                window->height() >= window->minimumHeight();
            const bool geometryMatches = window->geometry() == normalGeometry;
            window->close();
            if (!valid || !geometryMatches) {
                fail(QStringLiteral("Secondary Fluent Window chrome/geometry check failed"));
                return;
            }
            runDialogCheck();
        });
    }

    void runDialogCheck()
    {
        auto* dialog = new fluent::dialogs_flyouts::Dialog(m_window);
        dialog->setWindowTitle(QStringLiteral("Asynchronous dialog smoke"));
        dialog->setAnimationEnabled(false);
        connect(dialog, &QDialog::finished, this, [this, dialog](int result) {
            dialog->deleteLater();
            if (result != QDialog::Accepted) {
                fail(QStringLiteral("Asynchronous dialog did not complete"));
                return;
            }
            runMenuCheck();
        });
        QTimer::singleShot(25, dialog, &QDialog::accept);
        dialog->open();
    }

    void runMenuCheck()
    {
        auto* menu = new fluent::menus_toolbars::FluentMenu(
            QStringLiteral("Asynchronous menu smoke"), m_window);
        menu->addAction(QStringLiteral("Asynchronous menu smoke"));
        menu->addAction(QStringLiteral("Second menu action"));
        menu->addAction(QStringLiteral("Third menu action"));
        connect(menu, &QMenu::aboutToHide, this, [this, menu]() {
            menu->deleteLater();
            QTimer::singleShot(0, this, [this]() { runBrowserTextInputCheck(); });
        });
        menu->popup(m_window->mapToGlobal(QPoint(24, 24)));
        QTimer::singleShot(0, menu, [this, menu]() {
            bool actionGeometryValid = !menu->actions().isEmpty();
            QStringList geometrySummary;
            for (QAction* action : menu->actions()) {
                if (!action || action->isSeparator() || !action->isVisible())
                    continue;
                const QRect actionRect = menu->actionGeometry(action);
                geometrySummary.append(QStringLiteral("%1:%2,%3,%4x%5")
                                           .arg(action->text())
                                           .arg(actionRect.x())
                                           .arg(actionRect.y())
                                           .arg(actionRect.width())
                                           .arg(actionRect.height()));
                actionGeometryValid = actionGeometryValid && !actionRect.isEmpty() &&
                                      menu->rect().contains(actionRect.center());
            }
            const QRegion surfaceMask = menu->mask();
            const bool roundedMaskValid = !surfaceMask.isEmpty() &&
                                          !surfaceMask.contains(menu->rect().topLeft()) &&
                                          !surfaceMask.contains(menu->rect().topRight()) &&
                                          !surfaceMask.contains(menu->rect().bottomRight()) &&
                                          !surfaceMask.contains(menu->rect().bottomLeft()) &&
                                          surfaceMask.contains(menu->rect().center());
            if (!menu->isVisible() || menu->testAttribute(Qt::WA_TranslucentBackground) ||
                !actionGeometryValid || !roundedMaskValid) {
                menu->close();
                fail(QStringLiteral("Opaque browser menu surface/geometry check failed "
                                    "size=%1x%2 hint=%3x%4 geometry=%5 roundedMask=%6")
                         .arg(menu->width())
                         .arg(menu->height())
                         .arg(menu->sizeHint().width())
                         .arg(menu->sizeHint().height())
                         .arg(geometrySummary.join(QLatin1Char('|')))
                         .arg(roundedMaskValid));
                return;
            }
            QTimer::singleShot(25, menu, &QMenu::close);
        });
    }

    void runBrowserTextInputCheck()
    {
        static const QString expectedText = QStringLiteral("INPUT42");
        auto* input = new fluent::textfields::LineEdit(m_window);
        input->setObjectName(QStringLiteral("WasmBrowserTextInputSmoke"));
        input->setGeometry(24, 80, 240, input->sizeHint().height());
        input->show();
        input->raise();
        // Headless Chromium has no OS window activation step. Establish the
        // Qt-side focus explicitly; the browser driver still has to focus the
        // hidden HTML input and deliver physical keys for the probe to pass.
        // zh_CN: 无头 Chromium 没有 OS 窗口激活步骤，先显式建立 Qt 侧焦点；
        // 浏览器驱动仍必须聚焦隐藏 HTML input 并发送物理按键才能通过探针。
        input->setFocus(Qt::OtherFocusReason);

        const QPointer<fluent::textfields::LineEdit> guard(input);
        connect(input, &QLineEdit::textChanged, this, [this, guard](const QString& text) {
            if (!guard || text != expectedText)
                return;
            publishBrowserTextInputProbe("pass");
            guard->deleteLater();
            QTimer::singleShot(0, this, [this]() { runTextEditingMenuCheck(); });
        });

        // Publish only after QWidget geometry has settled. The Python browser
        // smoke clicks this real screen coordinate and emits physical key
        // events, covering the Qt WASM hidden-input bridge rather than merely
        // synthesizing QKeyEvent inside C++.
        // zh_CN: 等 QWidget 几何稳定后再发布坐标。Python 浏览器 smoke 会点击
        // 真实屏幕位置并发送物理按键，从而覆盖 Qt WASM 隐藏 input 桥接，而不是
        // 只在 C++ 内合成 QKeyEvent。
        QTimer::singleShot(0, input, [guard]() {
            if (!guard)
                return;
            publishBrowserTextInputProbe("ready", guard->mapToGlobal(guard->rect().center()),
                                         expectedText);
        });
        QTimer::singleShot(10000, this, [this, guard]() {
            if (!guard)
                return;
            publishBrowserTextInputProbe("fail");
            guard->deleteLater();
            fail(QStringLiteral("Browser keyboard input did not reach the hosted LineEdit"));
        });
    }

    void runTextEditingMenuCheck()
    {
        auto* password = new fluent::textfields::PasswordBox(m_window);
        password->setObjectName(QStringLiteral("WasmPasswordContextMenuSmoke"));
        password->setPassword(QStringLiteral("browser-secret"));
        password->setGeometry(24, 80, 240, password->sizeHint().height());
        password->show();
        password->selectAll();

        const QPoint localPosition = password->rect().center();
        QContextMenuEvent event(QContextMenuEvent::Mouse, localPosition,
                                password->mapToGlobal(localPosition));
        QApplication::sendEvent(password, &event);
        if (!event.isAccepted()) {
            password->deleteLater();
            return fail(QStringLiteral(
                "PasswordBox did not route its browser context menu through FluentMenu"));
        }
        QTimer::singleShot(0, this, [this, password]() {
            auto* menu = qobject_cast<fluent::menus_toolbars::FluentMenu*>(
                QApplication::activePopupWidget());
            bool actionGeometryValid = menu != nullptr;
            bool sourceMenusHidden = menu != nullptr;
            int visibleActionCount = 0;
            QStringList actionGeometrySummary;
            if (menu) {
                const QList<QMenu*> sourceMenus =
                    menu->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly);
                sourceMenusHidden = !sourceMenus.isEmpty();
                for (QMenu* sourceMenu : sourceMenus) {
                    sourceMenusHidden = sourceMenusHidden && sourceMenu && sourceMenu->isHidden() &&
                                        sourceMenu->testAttribute(Qt::WA_DontShowOnScreen);
                }
                for (QAction* action : menu->actions()) {
                    if (!action || action->isSeparator() || !action->isVisible())
                        continue;
                    ++visibleActionCount;
                    const QRect actionRect = menu->actionGeometry(action);
                    actionGeometrySummary.append(QStringLiteral("%1:%2,%3,%4x%5")
                                                     .arg(action->text())
                                                     .arg(actionRect.x())
                                                     .arg(actionRect.y())
                                                     .arg(actionRect.width())
                                                     .arg(actionRect.height()));
                    actionGeometryValid = actionGeometryValid && !actionRect.isEmpty() &&
                                          menu->rect().contains(actionRect.center());
                }
            }
            actionGeometryValid = actionGeometryValid && visibleActionCount > 0;
            if (!menu || menu->objectName() != QStringLiteral("FluentLineEdit.ContextMenu") ||
                menu->testAttribute(Qt::WA_TranslucentBackground) || !actionGeometryValid ||
                !sourceMenusHidden || menu->width() >= m_window->width() ||
                menu->height() >= m_window->height()) {
                if (menu)
                    menu->close();
                password->deleteLater();
                fail(
                    QStringLiteral("PasswordBox browser context menu surface/geometry check failed "
                                   "menu=%1 object=%2 translucent=%3 actions=%4 visible=%5 "
                                   "sourceHidden=%6 menuSize=%7x%8 hint=%9x%10 "
                                   "windowSize=%11x%12 geometry=%13")
                        .arg(menu != nullptr)
                        .arg(menu ? menu->objectName() : QStringLiteral("<none>"))
                        .arg(menu && menu->testAttribute(Qt::WA_TranslucentBackground))
                        .arg(actionGeometryValid)
                        .arg(visibleActionCount)
                        .arg(sourceMenusHidden)
                        .arg(menu ? menu->width() : 0)
                        .arg(menu ? menu->height() : 0)
                        .arg(menu ? menu->sizeHint().width() : 0)
                        .arg(menu ? menu->sizeHint().height() : 0)
                        .arg(m_window->width())
                        .arg(m_window->height())
                        .arg(actionGeometrySummary.join(QLatin1Char('|'))));
                return;
            }

            connect(menu, &QMenu::aboutToHide, this, [this, password]() {
                password->deleteLater();
                complete();
            });
            QTimer::singleShot(25, menu, &QMenu::close);
        });
    }

    void complete()
    {
        if (m_requireDataGridInteraction && !m_dataGridInteractionPassed) {
            return fail(QStringLiteral("Full smoke did not exercise the DataGrid Gallery route"));
        }
        if (m_requireChangedSampleProbes) {
            QStringList missing;
            for (const QString& routeId : changedSampleProbeRoutes()) {
                if (!m_changedSampleRoutesPassed.contains(routeId))
                    missing.append(routeId);
            }
            if (!missing.isEmpty()) {
                return fail(
                    QStringLiteral("Full smoke did not exercise changed Gallery samples: %1")
                        .arg(missing.join(QStringLiteral(", "))));
            }
        }
        const qint64 totalMs = m_totalTimer.elapsed();
        const qint64 finalHeapMiB = heapCapacityMiB();
        const qint64 finalHeapBreakMiB = heapBreakMiB();
        QString changedSampleSummary;
        if (m_requireChangedSampleProbes) {
            changedSampleSummary =
                QStringLiteral("changed sample probes passed for all 5 routes; ");
        } else if (!m_changedSampleRoutesPassed.isEmpty()) {
            QStringList passedRoutes;
            for (const QString& routeId : changedSampleProbeRoutes()) {
                if (m_changedSampleRoutesPassed.contains(routeId))
                    passedRoutes.append(routeId);
            }
            changedSampleSummary = QStringLiteral("changed sample probes passed for %1; ")
                                       .arg(passedRoutes.join(QStringLiteral(", ")));
        }
        publishSmokeState(
            "pass", QStringLiteral("%1 routes, storage, window, dialog, menu, browser text input, "
                                   "and text menu passed in %2 ms; "
                                   "%3%4"
                                   "CJK fallback passed; "
                                   "slowest route %5 took %6 ms; heap %7 -> %8 MiB; "
                                   "break %9 -> %10 MiB")
                        .arg(m_routes.size())
                        .arg(totalMs)
                        .arg(m_dataGridInteractionPassed
                                 ? QStringLiteral(
                                       "DataGrid scroll, keyboard selection, and editing passed; ")
                                 : QString())
                        .arg(changedSampleSummary)
                        .arg(m_slowestRoute)
                        .arg(m_slowestRouteMs)
                        .arg(m_initialHeapMiB)
                        .arg(finalHeapMiB)
                        .arg(m_initialHeapBreakMiB)
                        .arg(finalHeapBreakMiB));
        LOG_INFO(QStringLiteral("WasmSmoke completed routes=%1 elapsedMs=%2 slowestRoute=%3 "
                                "slowestRouteMs=%4 heapMiB=%5->%6 heapBreakMiB=%7->%8")
                     .arg(m_routes.size())
                     .arg(totalMs)
                     .arg(m_slowestRoute)
                     .arg(m_slowestRouteMs)
                     .arg(m_initialHeapMiB)
                     .arg(finalHeapMiB)
                     .arg(m_initialHeapBreakMiB)
                     .arg(finalHeapBreakMiB));
        deleteLater();
    }

    void fail(const QString& reason)
    {
        LOG_CRITICAL(QStringLiteral("WasmSmoke failed reason=%1").arg(reason));
        publishSmokeState("fail", reason);
        deleteLater();
    }

    GalleryWindow* m_window = nullptr;
    bool m_requireDataGridInteraction = false;
    bool m_dataGridInteractionPassed = false;
    bool m_requireChangedSampleProbes = false;
    bool m_teachingTipOpened = false;
    QSet<QString> m_changedSampleRoutesPassed;
    QStringList m_routes;
    QString m_currentRoute;
    int m_routeIndex = 0;
    qint64 m_slowestRouteMs = 0;
    qint64 m_initialHeapMiB = 0;
    qint64 m_initialHeapBreakMiB = 0;
    QString m_slowestRoute;
    QElapsedTimer m_totalTimer;
    QElapsedTimer m_routeTimer;
};

} // namespace

void startWasmSmokeIfRequested(GalleryWindow* window)
{
    const QString mode = smokeMode();
    if (!window || (mode != QStringLiteral("fast") && mode != QStringLiteral("full")))
        return;
    (new WasmSmokeRunner(window, mode == QStringLiteral("full")))->start();
}

} // namespace fluent::gallery
