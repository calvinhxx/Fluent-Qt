#include "platform/wasm/WasmSmokeRunner.h"

#include "components/status_info/Toast.h"
#include "components/status_info/ProgressBar.h"

#include <FluentQt/WebAssembly.h>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QContextMenuEvent>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QEvent>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QJsonDocument>
#include <QLabel>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPointer>
#include <QRegion>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QSizePolicy>
#include <QTimer>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QVariantAnimation>
#include <QUrlQuery>
#include "view/pages/GalleryFoundationTopicPage.h"
#include "view/widgets/GallerySampleCard.h"
#include "view/widgets/GallerySampleCatalog.h"

#include <cmath>

#ifdef FLUENT_QT_HAS_SPATIAL
#include <FluentQt/Spatial.h>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#endif

#if defined(FLUENT_QT_HAS_QML_BRIDGE) && defined(FLUENT_QT_HAS_SPATIAL)
#include <FluentQt/QmlBridge.h>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#endif

#include <emscripten/emscripten.h>
#include <emscripten/heap.h>

#include "components/basicinput/Button.h"
#include "components/basicinput/ComboBox.h"
#include "components/basicinput/MultiSelectComboBox.h"
#include "components/basicinput/Slider.h"
#include "components/basicinput/ToggleSwitch.h"
#include "components/collections/DataGrid.h"
#include "components/collections/ListView.h"
#include "components/dialogs_flyouts/Dialog.h"
#include "components/dialogs_flyouts/TeachingTip.h"
#include "components/layout/ParticleBackdrop.h"
#include "components/menus_toolbars/Menu.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/SelectorBar.h"
#include "components/navigation/StackContentHost.h"
#include "components/navigation/TabView.h"
#include "components/scrolling/ScrollView.h"
#include "components/textfields/LineEdit.h"
#include "components/textfields/Label.h"
#include "components/textfields/PasswordBox.h"
#include "components/windowing/TitleBar.h"
#include "components/windowing/Window.h"
#include "compatibility/FontCompat.h"
#include "support/logging/Log.h"
#include "view/pages/GalleryContentPage.h"
#include "view/pages/SettingsPage.h"
#include "view/shell/GallerySpatialController.h"
#include "view/shell/GalleryWindow.h"
#include "view/widgets/GalleryCodeBlock.h"
#include "view/widgets/GallerySampleCard.h"
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

void publishBrowserTextInputProbe(const char* state,
    const QPoint& globalPosition = QPoint(-1, -1),
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
        QStringLiteral("multi-select-combobox"),
        QStringLiteral("list-view"),
        QStringLiteral("navigation-view"),
        QStringLiteral("tab-view"),
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
    return QStringLiteral("%1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
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
    for (QWidget* current = widget ? widget->parentWidget() : nullptr; current; current = current->parentWidget()) {
        if (current->objectName() == QStringLiteral("gallerySampleCardPreview")) {
            return current;
        }
    }
    return nullptr;
}

class WasmSmokeRunner final : public QObject {
public:
    WasmSmokeRunner(GalleryWindow* window, bool full)
        : QObject(window), m_window(window), m_requireDataGridInteraction(full), m_requireChangedSampleProbes(full)
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
                if (!motionChoice || motionChoice->count() != 3
                    || motionChoice->currentIndex() != static_cast<int>(GallerySettings::instance().motionMode())) {
                    return fail(QStringLiteral("Settings route is missing the synchronized Motion policy choice"));
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
            return reject(
                QStringLiteral("DataGrid Gallery scenarios did not expose scale, selection, and editing views"));
        if (!largeGrid->isScrollChainingEnabled() || !selectionGrid->isScrollChainingEnabled()
            || !editingGrid->isScrollChainingEnabled()) {
            return reject(QStringLiteral("DataGrid Gallery samples do not share boundary scroll chaining"));
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
            return rejectSmoke(failure, QStringLiteral("TeachingTip changed-sample probe must run asynchronously"));
        }
        return rejectSmoke(failure,
            QStringLiteral("No changed-sample probe is registered for route %1").arg(m_currentRoute));
    }

    bool verifyMultiSelectRoute(QString* failure) const
    {
        GalleryContentPage* page = m_window ? m_window->currentContentPage() : nullptr;
        if (!page) {
            return rejectSmoke(failure, QStringLiteral("MultiSelectComboBox Gallery page is unavailable"));
        }

        fluent::basicinput::MultiSelectComboBox* box = nullptr;
        const auto boxes = page->findChildren<fluent::basicinput::MultiSelectComboBox*>();
        for (auto* candidate : boxes) {
            if (candidate && candidate->accessibleName() == QStringLiteral("Teams")) {
                box = candidate;
                break;
            }
        }
        if (!box || !box->model() || box->model()->rowCount() != 4 || box->selectedCount() != 2
            || box->width() != 280) {
            return rejectSmoke(failure, QStringLiteral("MultiSelectComboBox primary sample state is incomplete"));
        }

        auto* scrollArea = page->findChild<QScrollArea*>(QStringLiteral("galleryContentScrollArea"));
        if (!scrollArea) {
            return rejectSmoke(failure, QStringLiteral("MultiSelectComboBox page scroll host is unavailable"));
        }
        scrollArea->ensureWidgetVisible(box, 24, 48);
        QApplication::processEvents();
        box->open();
        QApplication::processEvents();

        QWidget* popupHost = box->window();
        auto* popup = popupHost ? popupHost->findChild<QWidget*>(QStringLiteral("MultiSelectComboBox.Popup")) : nullptr;
        auto* listView =
            popup ? popup->findChild<QAbstractItemView*>(QStringLiteral("MultiSelectComboBox.ListView")) : nullptr;
        const bool wasOpen = box->isOpen();
        const bool popupWasVisible = popup && popup->isVisible();
        const bool valid = wasOpen && popup && popupWasVisible && listView && listView->model()
            && listView->model()->rowCount() == 4 && popupHost && widgetContainedIn(popup, popupHost, 1)
            && widgetContainedIn(listView, popup, 1);
        box->close();
        QApplication::processEvents();
        if (!valid) {
            return rejectSmoke(failure,
                QStringLiteral(
                    "MultiSelectComboBox browser popup state or geometry is invalid "
                    "open=%1 popup=%2 visible=%3 list=%4 rows=%5 "
                    "popupRect=%6 windowRect=%7 listRect=%8")
                    .arg(wasOpen)
                    .arg(popup != nullptr)
                    .arg(popupWasVisible)
                    .arg(listView != nullptr)
                    .arg(listView && listView->model() ? listView->model()->rowCount() : -1)
                    .arg(popup ? rectSummary(globalWidgetRect(popup)) : QStringLiteral("<none>"))
                    .arg(popupHost ? rectSummary(globalWidgetRect(popupHost)) : QStringLiteral("<none>"))
                    .arg(listView ? rectSummary(globalWidgetRect(listView)) : QStringLiteral("<none>")));
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
        for (const ListCase& listCase :
            {ListCase{QStringLiteral("Contacts"), 12, 1}, ListCase{QStringLiteral("Message filters"), 9, 2}}) {
            fluent::collections::ListView* listView = nullptr;
            for (auto* candidate : views) {
                if (candidate && candidate->accessibleName() == listCase.accessibleName) {
                    listView = candidate;
                    break;
                }
            }
            QWidget* previewSurface = samplePreviewSurface(listView);
            if (!listView || !previewSurface || !listView->model() || listView->model()->rowCount() != listCase.rowCount
                || listView->size() != QSize(320, 234)
                || listView->selectionModel()->selectedRows().size() != listCase.selectedRows
                || !widgetContainedIn(listView, previewSurface, 1)) {
                return rejectSmoke(failure,
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
                    return rejectSmoke(failure,
                        QStringLiteral("ListView browser viewport clips a visible row: %1/%2")
                            .arg(listCase.accessibleName)
                            .arg(row));
                }
            }
            if (visibleRows != 5) {
                return rejectSmoke(failure,
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
            return rejectSmoke(failure, QStringLiteral("NavigationView Gallery page is unavailable"));
        }

        struct NavigationCase {
            QString objectName;
            int height;
        };
        for (const NavigationCase& navigationCase :
            {NavigationCase{QStringLiteral("navigationViewChromeSlotsPreview"), 340},
                NavigationCase{QStringLiteral("navigationViewDisplayModesPreview"), 340},
                NavigationCase{QStringLiteral("navigationViewContentHostPreview"), 320}}) {
            auto* navigation = page->findChild<fluent::navigation::NavigationView*>(navigationCase.objectName);
            QWidget* previewSurface = samplePreviewSurface(navigation);
            QWidget* contentHost = navigation ? navigation->contentHost() : nullptr;
            if (!navigation || !previewSurface || !contentHost || navigation->width() < 440 || navigation->width() > 620
                || navigation->height() != navigationCase.height
                || navigation->sizePolicy().horizontalPolicy() != QSizePolicy::Expanding
                || !widgetContainedIn(navigation, previewSurface, 1)
                || !widgetContainedIn(contentHost, navigation, 1)) {
                return rejectSmoke(failure,
                    QStringLiteral("NavigationView browser sample is clipped or incorrectly sized: %1")
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
        auto* tabs = page->findChild<fluent::navigation::TabView*>(QStringLiteral("tabViewHostedPagesTabs"));
        auto* host = page->findChild<QWidget*>(QStringLiteral("tabViewHostedPagesHost"));
        QWidget* previewSurface = samplePreviewSurface(surface);
        if (!surface || !tabs || !host || !previewSurface || surface->width() < 360 || surface->width() > 560
            || surface->height() != 186 || tabs->height() != 40 || host->height() != 146 || tabs->tabCount() != 3
            || tabs->selectedIndex() != 0 || tabs->visibleTabIndexes().size() != 3
            || !widgetContainedIn(surface, previewSurface, 1) || !widgetContainedIn(tabs, surface, 1)
            || !widgetContainedIn(host, surface, 1)) {
            return rejectSmoke(failure, QStringLiteral("TabView browser sample state or geometry is invalid"));
        }
        for (int index = 0; index < tabs->tabCount(); ++index) {
            const QRect tabRect = tabs->tabGeometry(index);
            if (tabRect.isEmpty() || !tabs->rect().contains(tabRect)) {
                return rejectSmoke(failure, QStringLiteral("TabView browser tab geometry is clipped: %1").arg(index));
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
                QStringLiteral("TabView browser sample exposed %1/%2 labels").arg(labels.size()).arg(tabs->tabCount()));
        }
        for (const auto* label : labels) {
            if (!label || !label->isVisible() || label->isTextElided() || !expectedLabels.remove(label->text())) {
                return rejectSmoke(failure,
                    QStringLiteral("TabView browser label is missing, hidden, duplicated, or elided: %1")
                        .arg(label ? label->text() : QStringLiteral("<null>")));
            }
        }
        if (!expectedLabels.isEmpty()) {
            const QStringList missingLabels(expectedLabels.values());
            return rejectSmoke(failure,
                QStringLiteral("TabView browser labels are incomplete: %1")
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

        auto* anchor = page->findChild<fluent::basicinput::Button*>(QStringLiteral("teachingTipTopAnchor"));
        auto* scrollArea = page->findChild<QScrollArea*>(QStringLiteral("galleryContentScrollArea"));
        if (!anchor || !scrollArea) {
            return rejectSmoke(failure, QStringLiteral("TeachingTip placement sample is incomplete"));
        }

        scrollArea->ensureWidgetVisible(anchor, 24, 160);
        QApplication::processEvents();
        anchor->click();
        QApplication::processEvents();

        QWidget* tipHost = anchor->window();
        auto* tip = tipHost
            ? tipHost->findChild<fluent::dialogs_flyouts::TeachingTip*>(QStringLiteral("teachingTipPlacementPreview"))
            : nullptr;
        if (!tip || !tipHost) {
            return rejectSmoke(failure, QStringLiteral("TeachingTip browser placement instance is unavailable"));
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
        QTimer::singleShot(400, this, [this, tipGuard, anchorGuard, hostGuard, pageGuard]() mutable {
            if (!tipGuard || !anchorGuard || !hostGuard || !pageGuard) {
                fail(QStringLiteral("TeachingTip browser placement lifetime ended before settle"));
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
            const bool valid = m_teachingTipOpened && settled && statusUpdated && tipGuard->isOpen()
                && tipGuard->isVisible() && tipGuard->target() == anchorGuard
                && tipGuard->preferredPlacement() == fluent::dialogs_flyouts::TeachingTip::Top
                && tipGuard->cardSize() == QSize(300, 136) && tipGuard->isTailVisible()
                && tipGuard->isLightDismissEnabled()
                && tipGuard->accessibleName() == QStringLiteral("Top placement tip") && tipGuard->contentHost()
                && widgetContainedIn(tipGuard, hostGuard, 1) && widgetContainedIn(tipGuard->contentHost(), tipGuard, 1);

            tipGuard->setExitAnimationEnabled(false);
            tipGuard->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            const bool deletedAfterClose = tipGuard.isNull();
            if (!valid || !deletedAfterClose) {
                fail(QStringLiteral(
                    "TeachingTip browser state, settle, geometry, status, or "
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
        if (!browserSurface || browserSurface->windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus)
            || browserSurface->testAttribute(Qt::WA_ShowWithoutActivating) || !m_window->customWindowChromeEnabled()
            || !m_window->titleBar() || !m_window->titleBar()->isVisible() || !m_window->titleBar()->isWindowActive()
            || !m_window->titleBar()->testAttribute(Qt::WA_OpaquePaintEvent)
            || m_window->property("fluentPaintedSurfaceCacheGeneration").toInt() <= 0
            || !m_window->findChild<QWidget*>(QStringLiteral("fluentWindowFrameHost"))) {
            return fail(QStringLiteral(
                "Browser Gallery host is non-focusable or missing "
                "opaque/cached Fluent window chrome"));
        }

        auto& settings = GallerySettings::instance();
        settings.setThemeMode(GallerySettings::ThemeMode::Light);
        QSettings storage(QSettings::WebLocalStorageFormat, QSettings::UserScope, QCoreApplication::organizationName(),
            QCoreApplication::applicationName());
        storage.sync();
        if (storage.value(QStringLiteral("settings/themeMode"), -1).toInt()
            != static_cast<int>(GallerySettings::ThemeMode::Light)) {
            return fail(QStringLiteral("WebLocalStorage theme persistence failed"));
        }

        const QStringList hanFallbacks = QFontDatabase::applicationFallbackFontFamilies(QChar::Script_Han);
        if (!hanFallbacks.contains(fluent::fontcompat::UISimplifiedChineseFamily)) {
            return fail(QStringLiteral("Simplified Chinese application font fallback was not registered"));
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
        const QRect normalGeometry(m_window->geometry().center() - QPoint(320, 260), QSize(640, 520));
        fluent::webassembly::showWindow(window, normalGeometry);

        QTimer::singleShot(150, window, [this, window, normalGeometry]() {
            const bool valid = window->isVisible() && window->customWindowChromeEnabled() && window->titleBar()
                && window->titleBar()->isVisible()
                && window->findChild<QWidget*>(QStringLiteral("fluentWindowFrameHost"))
                && window->width() >= window->minimumWidth() && window->height() >= window->minimumHeight();
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
        auto* menu = new fluent::menus_toolbars::FluentMenu(QStringLiteral("Asynchronous menu smoke"), m_window);
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
                actionGeometryValid = actionGeometryValid && !actionRect.isEmpty()
                    && menu->rect().contains(actionRect.center());
            }
            const QRegion surfaceMask = menu->mask();
            const bool roundedMaskValid = !surfaceMask.isEmpty() && !surfaceMask.contains(menu->rect().topLeft())
                && !surfaceMask.contains(menu->rect().topRight()) && !surfaceMask.contains(menu->rect().bottomRight())
                && !surfaceMask.contains(menu->rect().bottomLeft()) && surfaceMask.contains(menu->rect().center());
            if (!menu->isVisible() || menu->testAttribute(Qt::WA_TranslucentBackground) || !actionGeometryValid
                || !roundedMaskValid) {
                menu->close();
                fail(QStringLiteral(
                    "Opaque browser menu surface/geometry check failed "
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
            publishBrowserTextInputProbe("ready", guard->mapToGlobal(guard->rect().center()), expectedText);
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
        QContextMenuEvent event(QContextMenuEvent::Mouse, localPosition, password->mapToGlobal(localPosition));
        QApplication::sendEvent(password, &event);
        if (!event.isAccepted()) {
            password->deleteLater();
            return fail(QStringLiteral("PasswordBox did not route its browser context menu through FluentMenu"));
        }
        QTimer::singleShot(0, this, [this, password]() {
            auto* menu = qobject_cast<fluent::menus_toolbars::FluentMenu*>(QApplication::activePopupWidget());
            bool actionGeometryValid = menu != nullptr;
            bool sourceMenusHidden = menu != nullptr;
            int visibleActionCount = 0;
            QStringList actionGeometrySummary;
            if (menu) {
                const QList<QMenu*> sourceMenus = menu->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly);
                sourceMenusHidden = !sourceMenus.isEmpty();
                for (QMenu* sourceMenu : sourceMenus) {
                    sourceMenusHidden = sourceMenusHidden && sourceMenu && sourceMenu->isHidden()
                        && sourceMenu->testAttribute(Qt::WA_DontShowOnScreen);
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
                    actionGeometryValid = actionGeometryValid && !actionRect.isEmpty()
                        && menu->rect().contains(actionRect.center());
                }
            }
            actionGeometryValid = actionGeometryValid && visibleActionCount > 0;
            if (!menu || menu->objectName() != QStringLiteral("FluentLineEdit.ContextMenu")
                || menu->testAttribute(Qt::WA_TranslucentBackground) || !actionGeometryValid || !sourceMenusHidden
                || menu->width() >= m_window->width() || menu->height() >= m_window->height()) {
                if (menu)
                    menu->close();
                password->deleteLater();
                fail(QStringLiteral(
                    "PasswordBox browser context menu surface/geometry check failed "
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
                return fail(QStringLiteral("Full smoke did not exercise changed Gallery samples: %1")
                        .arg(missing.join(QStringLiteral(", "))));
            }
        }
        const qint64 totalMs = m_totalTimer.elapsed();
        const qint64 finalHeapMiB = heapCapacityMiB();
        const qint64 finalHeapBreakMiB = heapBreakMiB();
        QString changedSampleSummary;
        if (m_requireChangedSampleProbes) {
            changedSampleSummary = QStringLiteral("changed sample probes passed for all 5 routes; ");
        } else if (!m_changedSampleRoutesPassed.isEmpty()) {
            QStringList passedRoutes;
            for (const QString& routeId : changedSampleProbeRoutes()) {
                if (m_changedSampleRoutesPassed.contains(routeId))
                    passedRoutes.append(routeId);
            }
            changedSampleSummary =
                QStringLiteral("changed sample probes passed for %1; ").arg(passedRoutes.join(QStringLiteral(", ")));
        }
        publishSmokeState("pass",
            QStringLiteral(
                "%1 routes, storage, window, dialog, menu, browser text input, "
                "and text menu passed in %2 ms; "
                "%3%4"
                "CJK fallback passed; "
                "slowest route %5 took %6 ms; heap %7 -> %8 MiB; "
                "break %9 -> %10 MiB")
                .arg(m_routes.size())
                .arg(totalMs)
                .arg(m_dataGridInteractionPassed
                        ? QStringLiteral("DataGrid scroll, keyboard selection, and editing passed; ")
                        : QString())
                .arg(changedSampleSummary)
                .arg(m_slowestRoute)
                .arg(m_slowestRouteMs)
                .arg(m_initialHeapMiB)
                .arg(finalHeapMiB)
                .arg(m_initialHeapBreakMiB)
                .arg(finalHeapBreakMiB));
        LOG_INFO(QStringLiteral(
            "WasmSmoke completed routes=%1 elapsedMs=%2 slowestRoute=%3 "
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

// Read-only geometry for physical browser input. Qt accessibility rectangles are
// unprojected in 3D, so they cannot locate the visible source text on the canvas.
// This URL-only probe neither expands the source nor changes focus/selection.
class WasmQmlPlusProbe final : public QObject {
public:
    explicit WasmQmlPlusProbe(GalleryWindow* window) : QObject(window), m_window(window)
    {
        qApp->setProperty("fluentqtGalleryAutomated", true);
        GallerySettings::instance().setSpatialModeEnabled(false);
        window->selectRoute(QStringLiteral("foundation-qmlplus"));
        m_clock.start();
        m_tick.setInterval(50);
        connect(&m_tick, &QTimer::timeout, this, [this] { tick(); });
        m_tick.start();
    }

private:
    static QJsonObject fingerprint(const QString& source)
    {
        const auto bytes = source.toUtf8();
        return {{"bytes", int(bytes.size())},
            {"sha256", QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())}};
    }

    QList<GallerySampleCard*> cards(GalleryFoundationTopicPage* page)
    {
        QList<GallerySampleCard*> result;
        for (auto* widget : page->findChildren<QWidget*>())
            if (auto* sample = dynamic_cast<GallerySampleCard*>(widget))
                result.append(sample);
        return result;
    }

    GallerySampleCard* card(GalleryFoundationTopicPage* page, const QString& id)
    {
        for (auto* candidate : cards(page))
            if (candidate->sampleId() == id)
                return candidate;
        return nullptr;
    }

    void tick()
    {
        auto* page = m_window ? qobject_cast<GalleryFoundationTopicPage*>(m_window->currentContentPage()) : nullptr;
        if (!page || !page->isVisible() || m_window->currentRouteId() != QStringLiteral("foundation-qmlplus")) {
            if (m_clock.elapsed() > 60000)
                publishSmokeState("fail", QStringLiteral("QMLPlus page did not load"));
            return;
        }
        // clang-format off
        const char* input = reinterpret_cast<const char*>(EM_ASM_PTR({
            const value = document.documentElement.dataset.fluentQtQmlPlusCommand || '{}';
            const size = lengthBytesUTF8(value) + 1;
            const ptr = _malloc(size); stringToUTF8(value, ptr, size); return ptr;
        }));
        // clang-format on
        const auto request = QJsonDocument::fromJson(QByteArray(input)).object();
        free(const_cast<char*>(input));
        const int sequence = request["sequence"].toInt();
        if (sequence > m_sequence) {
            m_sequence = sequence;
            const auto action = request["action"].toString();
            const auto target = request["target"].toString();
            if (action == QStringLiteral("value")) {
                auto* widget = page->findChild<QWidget*>(target);
                const auto property = request["property"].toString().toUtf8();
                if (!widget || !widget->setProperty(property, request["value"].toVariant()))
                    m_failure = QStringLiteral("QMLPlus control/property is missing: %1").arg(target);
            } else if (action == QStringLiteral("show-source")) {
                m_selectedSource = target;
                if (auto* sample = card(page, target)) {
                    sample->codeBlock()->setExpanded(true, false);
                    if (auto* scroll = page->findChild<QScrollArea*>())
                        scroll->ensureWidgetVisible(sample->codeBlock(), 0, 20);
                } else {
                    m_failure = QStringLiteral("QMLPlus source card is missing: %1").arg(target);
                }
            } else if (action == QStringLiteral("finish")) {
                m_finished = true;
            }
        }
        QJsonObject sources;
        for (auto* sample : cards(page)) {
            auto* block = sample->codeBlock();
            if (!block->hasPythonCode() || !block->languageSelector()) {
                m_failure = QStringLiteral("QMLPlus card has no bilingual source: %1").arg(sample->sampleId());
                break;
            }
            sources[sample->sampleId()] = QJsonObject{{"cpp", fingerprint(block->cppCode())},
                {"python", fingerprint(block->pythonCode())}};
        }
        QJsonObject values;
        QJsonObject rectangles;
        for (auto* widget : page->findChildren<QWidget*>()) {
            const auto name = widget->objectName();
            if (!name.startsWith(QStringLiteral("qmlplus")))
                continue;
            QJsonObject properties;
            for (const auto* property : {"value", "isOn", "text", "enabled"}) {
                const auto value = widget->property(property);
                if (value.isValid())
                    properties[QString::fromLatin1(property)] = QJsonValue::fromVariant(value);
            }
            values[name] = properties;
            rectangles[name] = QJsonArray{widget->x(), widget->y(), widget->width(), widget->height()};
        }
        QJsonObject positions;
        QString selectedLanguage;
        QJsonObject selectedCode;
        if (auto* sample = card(page, m_selectedSource)) {
            auto* block = sample->codeBlock();
            selectedLanguage =
                block->codeLanguage() == GalleryCodeLanguage::Python ? QStringLiteral("python") : QStringLiteral("cpp");
            selectedCode = fingerprint(block->code());
            for (const auto* language : {"Cpp", "Python"}) {
                const QString name = QStringLiteral("galleryLanguage%1Button").arg(QString::fromLatin1(language));
                if (auto* button = block->findChild<QWidget*>(name)) {
                    QPoint point = button->mapToGlobal(button->rect().center());
#ifdef FLUENT_QT_HAS_SPATIAL
                    if (auto* controller = m_window->findChild<GallerySpatialController*>())
                        point = m_window->mapToGlobal(controller->projectedPosition(button, button->rect().center()));
#endif
                    positions[QString::fromLatin1(language)] = QJsonArray{point.x(), point.y()};
                }
            }
        }
        if (positions != m_positions) {
            m_positions = positions;
            m_geometryClock.restart();
        }
        bool transitioning = false;
#ifdef FLUENT_QT_HAS_SPATIAL
        if (auto* controller = m_window->findChild<GallerySpatialController*>())
            transitioning = controller->transitionRunning();
#endif
        const QJsonObject frame{
            {"state",
                !m_failure.isEmpty() ? "fail"
                    : m_finished     ? "pass"
                                     : "observing"},
            {"error", m_failure}, {"sequence", m_sequence}, {"ready", sources.size() == 12},
            {"geometryStable", m_geometryClock.isValid() && m_geometryClock.elapsed() >= 150},
            {"transitioning", transitioning}, {"sources", sources}, {"values", values}, {"rectangles", rectangles},
            {"positions", positions}, {"language", selectedLanguage}, {"code", selectedCode}};
        const auto output = QJsonDocument(frame).toJson(QJsonDocument::Compact);
        // clang-format off
        EM_ASM({ document.documentElement.dataset.fluentQtQmlPlus = UTF8ToString($0); }, output.constData());
        // clang-format on
        if (m_finished || !m_failure.isEmpty()) {
            publishSmokeState(m_failure.isEmpty() ? "pass" : "fail", m_failure);
            m_tick.stop();
            deleteLater();
        }
    }

    QPointer<GalleryWindow> m_window;
    QTimer m_tick;
    QElapsedTimer m_clock;
    QElapsedTimer m_geometryClock;
    QJsonObject m_positions;
    int m_sequence = 0;
    QString m_selectedSource = QStringLiteral("qmlplus-binding-one-way");
    QString m_failure;
    bool m_finished = false;
};

class WasmSourceInputProbe final : public QObject {
public:
    explicit WasmSourceInputProbe(GalleryWindow* window)
        : QObject(window), m_window(window), m_route(window->currentRouteId())
    {
        m_clock.start();
        m_tick.setInterval(50);
        connect(&m_tick, &QTimer::timeout, this, [this] { tick(); });
        m_tick.start();
    }

private:
    QJsonArray position(const QWidget* widget, const QPoint& point) const
    {
        const auto* controller = m_window->findChild<GallerySpatialController*>();
        const QPoint global = controller
            ? m_window->mapToGlobal(controller->projectedPosition(widget, point))
            : widget->mapToGlobal(point);
        return {global.x(), global.y()};
    }

    void tick()
    {
        // clang-format off
        const bool done = EM_ASM_INT({
            return document.documentElement.dataset.fluentQtSourceInputDone === 'true';
        });
        // clang-format on
        if (m_clock.elapsed() > 45000 || done) {
            deleteLater();
            return;
        }
        if (m_window->findChild<QWidget*>(QStringLiteral("gallerySplashScreen")))
            return;
        auto* controller = m_window->findChild<GallerySpatialController*>();
        if (GallerySettings::instance().spatialAvailabilityPending() || (controller && controller->transitionRunning()))
            return;
        auto* page = m_window->currentContentPage();
        if (!page || page->routeId() != m_route || !page->isVisible())
            return;
        if (!m_block) {
            m_block = page->findChild<GalleryCodeBlock*>();
            if (!m_block)
                return;
            connect(m_block, &GalleryCodeBlock::expansionTransitionStarted, this, [this] { m_expanding = true; });
            connect(m_block, &GalleryCodeBlock::expansionTransitionFinished, this, [this] { m_expanding = false; });
        }
        const auto* surface = m_window->findChild<QWidget*>(QStringLiteral("gallerySpatialSurface"));
        QJsonObject frame{{"state", QStringLiteral("ready")}, {"route", m_route},
            {"header", position(m_block->headerButton(), m_block->headerButton()->rect().center())},
            {"spatial", surface && surface->isVisible() && surface->property("presenting").toBool()}};
        frame["copyEnabled"] = m_block->copyButton()->isEnabled();
        QJsonArray toasts;
        for (const auto* toast : m_window->window()->findChildren<status_info::Toast*>()) {
            if (toast->isOpen())
                toasts.append(QJsonObject{{"message", toast->message()}, {"severity", int(toast->severity())}});
        }
        frame["toasts"] = toasts;
        if (m_block->isExpanded() && !m_expanding) {
            auto* label = m_block->findChild<textfields::Label*>(QStringLiteral("galleryCodeBlockText"));
            if (!label || !label->isVisible())
                return;
            const QRect content = label->contentsRect().adjusted(label->margin(), label->margin(), -label->margin(),
                -label->margin());
            QTextDocument document;
            document.setDefaultFont(label->font());
            document.setDocumentMargin(0);
            document.setHtml(label->text());
            document.setTextWidth(content.width());
            document.documentLayout()->documentSize();
            const auto* layout = document.firstBlock().layout();
            if (!layout || layout->lineCount() < 2)
                return;
            QJsonArray lines;
            for (int index = 0; index < 2; ++index) {
                const auto line = layout->lineAt(index);
                const int y = qRound(content.y() + layout->position().y() + line.y() + line.height() / 2);
                lines.append(QJsonArray{position(label, QPoint(content.left() + 1, y)),
                    position(label, QPoint(content.right() - 1, y))});
            }
            frame["state"] = QStringLiteral("expanded");
            frame["copy"] = position(m_block->copyButton(), m_block->copyButton()->rect().center());
            frame["lines"] = lines;
        }
        const auto json = QJsonDocument(frame).toJson(QJsonDocument::Compact);
        EM_ASM({ document.documentElement.dataset.fluentQtSourceInput = UTF8ToString($0); }, json.constData());
    }

    GalleryWindow* m_window;
    const QString m_route;
    QPointer<GalleryCodeBlock> m_block;
    QTimer m_tick;
    QElapsedTimer m_clock;
    bool m_expanding = false;
};

#if defined(FLUENT_QT_HAS_QML_BRIDGE) && defined(FLUENT_QT_HAS_SPATIAL)
// Explicit URL-only observation of the real Gallery page. Browser commands may
// navigate or scroll; all preview/form/button input comes from the browser canvas.
class WasmQmlBridgeProbe final : public QObject {
public:
    explicit WasmQmlBridgeProbe(GalleryWindow* window) : QObject(window), m_window(window)
    {
        qApp->setProperty("fluentqtGalleryAutomated", true);
        auto& settings = GallerySettings::instance();
        settings.setHomeParticlesEnabled(false);
        settings.setMotionMode(MotionPolicy::Mode::Full);
        settings.setNavigationStyle(GallerySettings::NavigationStyle::Auto);
        settings.setSpatialModeEnabled(true);
        window->selectRoute(QStringLiteral("button"));
        publishSmokeState("running", QStringLiteral("Waiting for QmlBridge Gallery input"));
        m_clock.start();
        m_tick.setInterval(50);
        connect(&m_tick, &QTimer::timeout, this, [this] { tick(); });
        m_tick.start();
    }

private:
    QJsonObject position(QWidget* widget, const QPoint& local) const
    {
        if (!widget)
            return {};
        const QPoint nativeGlobal = widget->mapToGlobal(local);
        auto* controller = m_window->findChild<GallerySpatialController*>();
        const QPoint global =
            controller ? m_window->mapToGlobal(controller->projectedPosition(widget, local)) : nativeGlobal;
        QRect visible = globalWidgetRect(widget);
        for (auto* parent = widget->parentWidget(); parent; parent = parent->parentWidget())
            visible &= globalWidgetRect(parent);
        return {{"point", QJsonArray{global.x(), global.y()}},
            {"visible", widget->isVisible() && visible.contains(nativeGlobal)}, {"enabled", widget->isEnabled()}};
    }

    GalleryCodeBlock* actionSource(qmlbridge::QmlView* view) const
    {
        for (auto* parent = view ? view->parentWidget() : nullptr; parent; parent = parent->parentWidget())
            if (auto* card = dynamic_cast<GallerySampleCard*>(parent))
                return card->codeBlock();
        return nullptr;
    }

    static QJsonObject sourceFingerprint(const QString& source)
    {
        const auto bytes = source.toUtf8();
        const int lines =
            source.isEmpty() ? 0 : int(source.count(QLatin1Char('\n'))) + (source.endsWith(QLatin1Char('\n')) ? 0 : 1);
        return {{"sha256", QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())},
            {"lines", lines}, {"bytes", int(bytes.size())}};
    }

    void captureSourceContract(GalleryContentPage* page)
    {
        if (!m_sourceContract.isEmpty() && !m_sourceFileContract.isEmpty())
            return;
        const QStringList ids{QStringLiteral("qml-view-load"), QStringLiteral("qml-view-theme"),
            QStringLiteral("qml-view-context"), QStringLiteral("qml-view-action"), QStringLiteral("qml-view-lifecycle"),
            QStringLiteral("qml-view-error")};
        QJsonObject sources;
        QJsonObject sourceFiles;
        QJsonObject pythonFiles;
        for (auto* widget : page->findChildren<QWidget*>()) {
            auto* card = dynamic_cast<GallerySampleCard*>(widget);
            if (!card)
                continue;
            const auto id = card->sampleId();
            auto* block = card->codeBlock();
            if (!ids.contains(id) || sources.contains(id) || !block
                || block->codeLanguage() != GalleryCodeLanguage::Cpp) {
                m_failure = QStringLiteral("Cannot capture the default C++ source for QML card: %1").arg(id);
                return;
            }
            // Read the actual card's full source and its initially selected Key usage.
            // zh_CN: 从真实卡片读取完整源码和初始选中的核心用法，避免另建源码副本。
            sources[id] = QJsonObject{{"full", sourceFingerprint(block->cppCode())},
                {"key", sourceFingerprint(block->code())}};
            QJsonArray files;
            for (const auto& file : block->sourceFileBundle()) {
                files.append(QJsonObject{{"name", file.name}, {"language", file.language},
                    {"code", sourceFingerprint(file.code)}, {"usage", sourceFingerprint(file.usageSnippet)}});
            }
            sourceFiles[id] = files;
            if (!block->hasPythonCode() || !block->languageSelector()) {
                m_failure = QStringLiteral("QML card is missing its Python source selector: %1").arg(id);
                return;
            }
            block->setCodeLanguage(GalleryCodeLanguage::Python);
            QJsonArray python;
            for (const auto& file : block->sourceFileBundle()) {
                python.append(QJsonObject{{"name", file.name}, {"language", file.language},
                    {"code", sourceFingerprint(file.code)}, {"usage", sourceFingerprint(file.usageSnippet)}});
            }
            if (python.size() != 2 || python[0].toObject()["name"].toString() != "main.py") {
                m_failure = QStringLiteral("QML card has an incomplete Python file bundle: %1").arg(id);
                return;
            }
            pythonFiles[id] = python;
            block->setCodeLanguage(GalleryCodeLanguage::Cpp);
        }
        if (sources.size() != ids.size()) {
            m_failure = QStringLiteral("The QmlView page does not expose all six source cards");
            return;
        }
        m_sourceContract = sources;
        m_sourceFileContract = sourceFiles;
        m_pythonSourceFileContract = pythonFiles;
    }

    QJsonObject viewState(qmlbridge::QmlView* view)
    {
        if (!view)
            return {};
        if (!m_readyCounts.contains(view)) {
            m_readyCounts.insert(view, view->status() == qmlbridge::QmlView::Ready ? 1 : 0);
            connect(view, &qmlbridge::QmlView::statusChanged, this, [this, view](qmlbridge::QmlView::Status status) {
                if (status == qmlbridge::QmlView::Ready)
                    ++m_readyCounts[view];
            });
            connect(view, &QObject::destroyed, this, [this, view] { m_readyCounts.remove(view); });
        }
        QJsonObject state{{"active", view->isActive()}, {"ready", view->status() == qmlbridge::QmlView::Ready},
            {"readyCount", m_readyCounts.value(view)}, {"status", int(view->status())},
            {"source", view->source().toString()}, {"lastError", view->lastError()},
            {"rootPresent", view->rootObject() != nullptr}, {"textureEnabled", view->isTextureRenderingEnabled()},
            {"textureId", double(view->textureId())}, {"textureRevision", double(view->textureRevision())},
            {"quickWidgets", view->findChildren<QQuickWidget*>().size()}};
        if (auto* root = view->rootObject()) {
            state["rootIdentity"] = QString::number(quintptr(root), 16);
            state["engineIdentity"] = QString::number(quintptr(qmlEngine(root)), 16);
        }
        return state;
    }

    QPoint advancePosition(qmlbridge::QmlView* view) const
    {
        auto* root = view ? view->rootObject() : nullptr;
        auto* item = root ? root->findChild<QQuickItem*>(QStringLiteral("qmlBridgeAdvanceButton")) : nullptr;
        if (!item)
            return QPoint(-1, -1);
        return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
    }

    bool command(GalleryContentPage* page, qmlbridge::QmlView* view)
    {
        // clang-format off
        const char* raw = emscripten_run_script_string(R"JS((() => {
            const root = document.documentElement;
            const command = root.dataset.fluentQtQmlGalleryCommand || '';
            delete root.dataset.fluentQtQmlGalleryCommand;
            return command;
        })())JS");
        // clang-format on
        if (!raw || !*raw)
            return true;
        const QJsonObject request = QJsonDocument::fromJson(QByteArray(raw)).object();
        const QString action = request["action"].toString();
        m_commandSequence = request["sequence"].toInt();
        if (action == QStringLiteral("open-qml") || action == QStringLiteral("return-qml")) {
            if (!m_initial3d) {
                m_failure = QStringLiteral("QML was requested before the Button page presented 3D");
                return false;
            }
            if (!m_window->selectRoute(QStringLiteral("qml-view")))
                m_failure = QStringLiteral("The QmlView Gallery route is missing");
        } else if (action == QStringLiteral("leave-qml")) {
            if (!m_window->selectRoute(QStringLiteral("button")))
                m_failure = QStringLiteral("The Button Gallery route is missing");
        } else if (action == QStringLiteral("source-language")) {
            auto* source = actionSource(view);
            if (!source || !source->hasPythonCode()) {
                m_failure = QStringLiteral("Cannot select the Python QML host source");
                return false;
            }
            source->setCodeLanguage(request["target"].toString() == QStringLiteral("python")
                    ? GalleryCodeLanguage::Python
                    : GalleryCodeLanguage::Cpp);
        } else if (action == QStringLiteral("source-detail")) {
            auto* source = actionSource(view);
            auto* selector = source
                ? source->findChild<navigation::SelectorBar*>(QStringLiteral("galleryCodeBlockSourceSelector"))
                : nullptr;
            if (!selector || selector->isHidden()) {
                m_failure = QStringLiteral("Cannot select the host source detail");
                return false;
            }
            selector->setSelectedIndex(request["target"].toString() == "full" ? 1 : 0);
        } else if (action == QStringLiteral("source-file")) {
            auto* source = actionSource(view);
            auto* selector = source
                ? source->findChild<basicinput::ComboBox*>(QStringLiteral("galleryCodeBlockFileSelector"))
                : nullptr;
            const QString filename = request["target"].toString();
            const int index = selector ? selector->findText(filename) : -1;
            if (!page || page->routeId() != QStringLiteral("qml-view") || index < 0) {
                m_failure = QStringLiteral("Cannot select the QML source file: %1").arg(filename);
                return false;
            }
            selector->setCurrentIndex(index);
        } else if (action == QStringLiteral("reveal")) {
            if (!page || page->routeId() != QStringLiteral("qml-view")) {
                m_failure = QStringLiteral("Cannot scroll a QML control outside its page");
                return false;
            }
            const QString target = request["target"].toString();
            QWidget* widget = nullptr;
            QPoint point;
            if (target == QStringLiteral("advance")) {
                widget = view;
                point = advancePosition(view);
            } else {
                const QString name = target == QStringLiteral("title") ? QStringLiteral("qmlBridgeTitleEdit")
                    : target == QStringLiteral("deactivate")           ? QStringLiteral("qmlBridgeDeactivate")
                    : target == QStringLiteral("reload")               ? QStringLiteral("qmlBridgeReload")
                    : target == QStringLiteral("missing")              ? QStringLiteral("qmlBridgeLoadMissing")
                    : target == QStringLiteral("restore")              ? QStringLiteral("qmlBridgeRestore")
                    : target == QStringLiteral("lifecycle")
                    ? QStringLiteral("qmlBridgeLifecycleView")
                    : QString();
                if (target == QStringLiteral("source") || target == QStringLiteral("sourcePython")
                    || target == QStringLiteral("sourceCpp")) {
                    auto* source = actionSource(view);
                    widget = source
                        ? source->findChild<QWidget*>(
                              target == QStringLiteral("sourcePython") ? QStringLiteral("galleryLanguagePythonButton")
                                  : target == QStringLiteral("sourceCpp")
                                  ? QStringLiteral("galleryLanguageCppButton")
                                  : QStringLiteral("galleryCodeBlockHeader"))
                        : nullptr;
                } else {
                    widget = name.isEmpty() ? nullptr : page->findChild<QWidget*>(name);
                }
                if (widget)
                    point = widget->rect().center();
            }
            auto* scroll = page->findChild<QScrollArea*>();
            if (!widget || point.x() < 0 || point.y() < 0 || !scroll || !scroll->widget()) {
                m_failure = QStringLiteral("Cannot locate the requested QML Gallery control: %1").arg(target);
                return false;
            }
            const QPoint inContent = widget->mapTo(scroll->widget(), point);
            scroll->ensureVisible(inContent.x(), inContent.y(), 32, 72);
        } else if (action == QStringLiteral("clip-advance")) {
            auto* scroll = page ? page->findChild<scrolling::ScrollView*>() : nullptr;
            if (!view || !scroll || !scroll->widget()) {
                m_failure = QStringLiteral("Cannot clip the QML action without its scroll page");
                return false;
            }
            if (auto* focused = QApplication::focusWidget())
                focused->clearFocus();
            auto* item = view->rootObject()
                ? view->rootObject()->findChild<QQuickItem*>(QStringLiteral("qmlBridgeAdvanceButton"))
                : nullptr;
            if (!item) {
                m_failure = QStringLiteral("Cannot clip an unloaded QML action");
                return false;
            }
            const QPoint local = advancePosition(view);
            const QPoint accent = item->mapToScene(QPointF(8, item->height() / 2)).toPoint();
            const int actionY = view->mapTo(scroll->widget(), local).y();
            const int maximum = scroll->verticalScrollBar()->maximum();
            const bool clipTop = actionY + 12 <= maximum;
            const QRect appRect = globalWidgetRect(m_window);
            bool placed = false;
            for (int inset : {12, 4, 1}) {
                const int requested = clipTop ? actionY + inset : actionY - scroll->viewport()->height() - inset;
                if (requested < 0 || requested > maximum)
                    continue;
                scroll->scrollTo(scroll->horizontalScrollBar()->value(), requested, false);
                const auto actionPoint = position(view, local);
                const auto accentPoint = position(view, accent);
                const auto inApp = [&appRect](const QJsonObject& observation) {
                    const auto point = observation["point"].toArray();
                    return point.size() == 2 && appRect.contains(QPoint(point[0].toInt(), point[1].toInt()));
                };
                const QRect viewInViewport(view->mapTo(scroll->viewport(), QPoint()), view->size());
                if (!actionPoint["visible"].toBool() && !accentPoint["visible"].toBool() && inApp(actionPoint)
                    && inApp(accentPoint) && viewInViewport.intersects(scroll->viewport()->rect())) {
                    placed = true;
                    break;
                }
            }
            if (!placed) {
                m_failure = QStringLiteral("Cannot place a clipped QML action inside the Qt window");
                return false;
            }
            m_geometryClock.restart();
        } else if (action == QStringLiteral("mode-2d")) {
            GallerySettings::instance().setSpatialModeEnabled(false);
        } else if (action == QStringLiteral("mode-3d")) {
            GallerySettings::instance().setSpatialModeEnabled(true);
        } else if (action == QStringLiteral("finish")) {
            m_finished = true;
        } else {
            m_failure = QStringLiteral("Unsupported QML Gallery probe command: %1").arg(action);
        }
        return m_failure.isEmpty();
    }

    void tick()
    {
        if (!m_window || m_clock.elapsed() > 90000) {
            m_failure = QStringLiteral("QML Gallery browser input timed out or lost its window");
        }
        auto* page = m_window ? m_window->currentContentPage() : nullptr;
        auto* view = page ? page->findChild<qmlbridge::QmlView*>(QStringLiteral("qmlBridgeActionView")) : nullptr;
        if (m_failure.isEmpty())
            command(page, view);
        // A route command may have changed the current page synchronously.
        page = m_window ? m_window->currentContentPage() : nullptr;
        view = page ? page->findChild<qmlbridge::QmlView*>(QStringLiteral("qmlBridgeActionView")) : nullptr;
        auto* controller = m_window ? m_window->findChild<GallerySpatialController*>() : nullptr;
        auto* surface = m_window ? m_window->findChild<QWidget*>("gallerySpatialSurface") : nullptr;
        auto* navigationView = m_window ? m_window->findChild<navigation::NavigationView*>() : nullptr;
        const auto& settings = GallerySettings::instance();
        if (!settings.spatialAvailabilityPending() && !settings.spatialAvailable()
            && !settings.spatialUnavailableReason().isEmpty()) {
            m_failure =
                QStringLiteral("QML Gallery probe requires hardware 3D: %1").arg(settings.spatialUnavailableReason());
        }
        const bool presenting = surface && surface->isVisible() && surface->property("presenting").toBool();
        const bool transitioning = controller && controller->transitionRunning();
        const QVariantMap stats = controller ? controller->renderingStatistics() : QVariantMap();
        if (page && page->routeId() == QStringLiteral("button") && presenting && !transitioning
            && settings.spatialModeEnabled() && !stats["directCompositionRequired"].toBool())
            m_initial3d = true;
        QJsonObject frame{{"state", m_failure.isEmpty() ? (m_finished ? "pass" : "observing") : "fail"},
            {"error", m_failure}, {"commandSequence", m_commandSequence}, {"initial3d", m_initial3d},
            {"route", m_window ? m_window->currentRouteId() : QString()}, {"pageVisible", page && page->isVisible()},
            {"spatialRequested", settings.spatialModeEnabled()}, {"spatialAvailable", settings.spatialAvailable()},
            {"availabilityPending", settings.spatialAvailabilityPending()},
            {"spatialUnavailableReason", settings.spatialUnavailableReason()}, {"presenting", presenting},
            {"transitioning", transitioning},
            {"directCompositionRequired", stats["directCompositionRequired"].toBool()},
            {"navigationEffect", navigationView && navigationView->graphicsEffect()},
            {"contentEffect", navigationView && navigationView->contentHost()->graphicsEffect()},
            {"navigationMode", navigationView ? int(navigationView->effectiveDisplayMode()) : -1},
            {"windowWidth", m_window ? m_window->width() : 0}, {"windowHeight", m_window ? m_window->height() : 0},
            {"dpr", m_window ? m_window->devicePixelRatioF() : 0}, {"statistics", QJsonObject::fromVariantMap(stats)}};
        int allQuickCount = 0;
        int cachedReadyCount = 0;
        if (m_window) {
            allQuickCount = m_window->findChildren<QQuickWidget*>().size();
            for (auto* cachedView : m_window->findChildren<qmlbridge::QmlView*>())
                if (cachedView->status() == qmlbridge::QmlView::Ready && (!page || !page->isAncestorOf(cachedView)))
                    ++cachedReadyCount;
        }
        frame["allQuickWidgets"] = allQuickCount;
        frame["cachedReadyViews"] = cachedReadyCount;
        frame["quickWidgets"] = page ? page->findChildren<QQuickWidget*>().size() : 0;
        int visibleQuickCount = 0;
        if (page)
            for (auto* quick : page->findChildren<QQuickWidget*>())
                visibleQuickCount += quick->isVisible();
        frame["visibleQuickWidgets"] = visibleQuickCount;
        QJsonArray effects;
        if (view) {
            for (auto* ancestor = static_cast<QWidget*>(view); ancestor; ancestor = ancestor->parentWidget())
                if (ancestor->graphicsEffect())
                    effects.append(QString::fromLatin1(ancestor->metaObject()->className()) + QLatin1Char(':')
                        + ancestor->objectName());
            frame["active"] = view->isActive();
            frame["ready"] = view->status() == qmlbridge::QmlView::Ready;
            frame["status"] = int(view->status());
            frame["source"] = view->source().toString();
            frame["rootPresent"] = view->rootObject() != nullptr;
            frame["qmlErrors"] = QJsonArray::fromStringList(view->errors());
            frame["viewDirectCompositionRequired"] = view->property("fluentqtDirectCompositionRequired").toBool();
            frame["themeDark"] = view->themeObject()->property("dark").toBool();
            frame["textureEnabled"] = view->isTextureRenderingEnabled();
            frame["textureId"] = double(view->textureId());
            frame["textureRevision"] = double(view->textureRevision());
            frame["textureSize"] = QJsonArray{view->textureSize().width(), view->textureSize().height()};
            frame["textureStatistics"] = QJsonObject::fromVariantMap(view->textureStatistics());
            if (auto* root = view->rootObject()) {
                frame["rootIdentity"] = QString::number(quintptr(root), 16);
                frame["engineIdentity"] = QString::number(quintptr(qmlEngine(root)), 16);
                frame["qmlTitle"] = root->property("previewTitle").toString();
                frame["qmlProgress"] = root->property("previewProgress").toInt();
                if (m_progressProbe < 0)
                    m_progressProbe = (root->property("previewProgress").toInt() + 5) / 100.;
            }
        }
        frame["ancestorEffects"] = effects;
        if (page && page->routeId() == QStringLiteral("qml-view")) {
            if (auto* source = actionSource(view)) {
                auto* files = source->findChild<basicinput::ComboBox*>(QStringLiteral("galleryCodeBlockFileSelector"));
                auto* modes = source->findChild<navigation::SelectorBar*>(
                    QStringLiteral("galleryCodeBlockSourceSelector"));
                frame["sourceSelection"] = QJsonObject{{"currentFileName", files ? files->currentText() : QString()},
                    {"sourceLanguage", source->sourceLanguage()},
                    {"codeFingerprint", sourceFingerprint(source->code())},
                    {"sourceMode",
                        !modes || modes->isHidden() ? QStringLiteral("file")
                            : modes->selectedIndex() == 0
                            ? QStringLiteral("key")
                            : QStringLiteral("full")}};
                auto* text = source->findChild<QLabel*>("galleryCodeBlockText");
                auto* clip = source->findChild<QWidget*>("galleryCodeBlockContent");
                auto* body = source->contentWidget();
                frame["sourceContentFits"] = text && clip && body
                    && text->height() >= text->heightForWidth(text->width()) && clip->height() >= body->height();
            }
            auto* title = page->findChild<QLineEdit*>(QStringLiteral("qmlBridgeTitleEdit"));
            auto* progress = page->findChild<status_info::ProgressBar*>("qmlBridgeProgressIndicator");
            frame["widgetTitle"] = title ? title->text() : QString();
            frame["widgetProgress"] = progress ? progress->value() : -1;
            auto* context = page->findChild<qmlbridge::QmlView*>(QStringLiteral("qmlBridgeContextView"));
            frame["qmlTitle"] = context && context->rootObject()
                ? context->rootObject()->property("previewTitle").toString()
                : QString();
            QJsonObject states;
            const auto names = {qMakePair("load", "qmlBridgeLoadView"), qMakePair("theme", "qmlBridgeThemeView"),
                qMakePair("context", "qmlBridgeContextView"), qMakePair("action", "qmlBridgeActionView"),
                qMakePair("lifecycle", "qmlBridgeLifecycleView"), qMakePair("error", "qmlBridgeErrorView")};
            bool allReady = true;
            for (const auto& name : names) {
                const auto state = viewState(page->findChild<qmlbridge::QmlView*>(QString::fromLatin1(name.second)));
                allReady &= state["ready"].toBool();
                states[name.first] = state;
            }
            if (allReady)
                captureSourceContract(page);
            frame["views"] = states;
            frame["pageDirectCompositionRequired"] = page->property("fluentqtDirectCompositionRequired").toBool();
            if (auto* source = actionSource(view))
                frame["sourceExpanded"] = source->isExpanded();
            QJsonObject positions;
            if (view && view->rootObject()) {
                positions["advance"] = position(view, advancePosition(view));
                auto* action = view->rootObject()->findChild<QQuickItem*>(QStringLiteral("qmlBridgeAdvanceButton"));
                if (action) {
                    const QPoint sample = action->mapToScene(QPointF(8, action->height() / 2)).toPoint();
                    positions["accentSample"] = position(view, sample);
                    positions["backgroundSample"] = position(view, QPoint(view->width() - 24, sample.y()));
                    const QColor accent = view->themeObject()->property("accent").value<QColor>();
                    const QColor background = view->themeObject()->property("surface").value<QColor>();
                    frame["accentRgb"] = QJsonArray{accent.red(), accent.green(), accent.blue()};
                    frame["backgroundRgb"] = QJsonArray{background.red(), background.green(), background.blue()};
                    for (auto* item : view->rootObject()->findChildren<QQuickItem*>()) {
                        if (!qFuzzyCompare(item->height(), 6.) || item->width() < view->width() * .7)
                            continue;
                        positions["progressSample"] = position(view,
                            item->mapToScene(QPointF(item->width() * m_progressProbe, 3)).toPoint());
                        QColor progressColor = progress && progress->value() >= m_progressProbe * 100
                            ? accent
                            : view->themeObject()->property("border").value<QColor>();
                        const qreal opacity = progressColor.alphaF();
                        progressColor = QColor(qRound(progressColor.red() * opacity + background.red() * (1 - opacity)),
                            qRound(progressColor.green() * opacity + background.green() * (1 - opacity)),
                            qRound(progressColor.blue() * opacity + background.blue() * (1 - opacity)));
                        frame["progressRgb"] = QJsonArray{progressColor.red(), progressColor.green(),
                            progressColor.blue()};
                        break;
                    }
                }
            }
            if (title)
                positions["title"] = position(title, title->rect().center());
            for (const auto& target :
                {qMakePair("deactivate", "qmlBridgeDeactivate"), qMakePair("reload", "qmlBridgeReload"),
                    qMakePair("missing", "qmlBridgeLoadMissing"), qMakePair("restore", "qmlBridgeRestore")}) {
                if (auto* widget = page->findChild<QWidget*>(QString::fromLatin1(target.second)))
                    positions[target.first] = position(widget, widget->rect().center());
            }
            if (auto* source = actionSource(view)) {
                if (auto* header = source->findChild<QWidget*>(QStringLiteral("galleryCodeBlockHeader")))
                    positions["source"] = position(header, header->rect().center());
                for (const auto& language : {qMakePair("sourceCpp", "galleryLanguageCppButton"),
                         qMakePair("sourcePython", "galleryLanguagePythonButton")}) {
                    if (auto* button = source->findChild<QWidget*>(QString::fromLatin1(language.second)))
                        positions[language.first] = position(button, button->rect().center());
                }
            }
            if (auto* lifecycle = page->findChild<qmlbridge::QmlView*>(QStringLiteral("qmlBridgeLifecycleView")))
                positions["lifecycle"] = position(lifecycle, lifecycle->rect().center());
            if (auto* scroll = page->findChild<QScrollArea*>()) {
                positions["scrollViewport"] = position(scroll->viewport(),
                    QPoint(qMax(0, scroll->viewport()->width() - 8), scroll->viewport()->height() / 2));
                frame["scrollValue"] = scroll->verticalScrollBar()->value();
                frame["scrollMaximum"] = scroll->verticalScrollBar()->maximum();
                if (view && scroll->widget()) {
                    const QPoint origin = view->mapTo(scroll->widget(), QPoint());
                    auto* source = actionSource(view);
                    const QJsonArray geometry{origin.x(), origin.y(), view->width(), view->height(),
                        scroll->viewport()->width(), scroll->viewport()->height(), scroll->widget()->width(),
                        scroll->widget()->height(), scroll->verticalScrollBar()->value(),
                        scroll->verticalScrollBar()->maximum(), source ? source->height() : 0,
                        source && source->isExpanded()};
                    if (geometry != m_geometry) {
                        m_geometry = geometry;
                        m_geometryClock.restart();
                    }
                    bool sourceAnimating = false;
                    if (source)
                        for (auto* animation : source->findChildren<QVariantAnimation*>())
                            sourceAnimating |= animation->state() == QAbstractAnimation::Running;
                    frame["geometry"] = geometry;
                    frame["geometryStable"] = !sourceAnimating && m_geometryClock.elapsed() >= 100;
                }
            }
            frame["positions"] = positions;
        }
        frame["sourceContract"] = m_sourceContract;
        frame["sourceFileContract"] = m_sourceFileContract;
        frame["pythonSourceFileContract"] = m_pythonSourceFileContract;
        if (!m_failure.isEmpty()) {
            frame["state"] = QStringLiteral("fail");
            frame["error"] = m_failure;
        }
        const auto json = QJsonDocument(frame).toJson(QJsonDocument::Compact);
        // clang-format off
        EM_ASM({ document.documentElement.dataset.fluentQtQmlGallery = UTF8ToString($0); },
               json.constData());
        // clang-format on
        if (!m_failure.isEmpty() || m_finished) {
            publishSmokeState(m_failure.isEmpty() ? "pass" : "fail", m_failure);
            m_tick.stop();
            deleteLater();
        }
    }

    QPointer<GalleryWindow> m_window;
    QTimer m_tick;
    QElapsedTimer m_clock;
    QElapsedTimer m_geometryClock;
    QJsonArray m_geometry;
    QJsonObject m_sourceContract;
    QJsonObject m_sourceFileContract;
    QJsonObject m_pythonSourceFileContract;
    QHash<qmlbridge::QmlView*, int> m_readyCounts;
    QString m_failure;
    qreal m_progressProbe = -1;
    int m_commandSequence = 0;
    bool m_initial3d = false;
    bool m_finished = false;
};
#endif

} // namespace

#ifdef FLUENT_QT_HAS_SPATIAL
// Capture the same real page through both rendering paths, including the flat GPU
// endpoint. The browser acknowledges each frame after saving its actual canvas.
class WasmSpatialQualityProbe final : public QObject {
public:
    explicit WasmSpatialQualityProbe(GalleryWindow* window) : QObject(window), m_window(window)
    {
        auto& settings = GallerySettings::instance();
        settings.setHomeParticlesEnabled(false);
        settings.setSpatialModeEnabled(false);
        settings.setNavigationStyle(GallerySettings::NavigationStyle::Left);
        window->selectRoute(QStringLiteral("popup"));
        publishSmokeState("running", QStringLiteral("Waiting for quality comparison"));
        m_clock.start();
        m_tick.setInterval(50);
        connect(&m_tick, &QTimer::timeout, this, [this] { tick(); });
        m_tick.start();
    }

private:
    void tick()
    {
        if (m_clock.elapsed() > 30000) {
            publishSmokeState("fail", QStringLiteral("Quality capture timed out"));
            deleteLater();
            return;
        }
        auto* controller = m_window->findChild<GallerySpatialController*>();
        auto* surface = m_window->findChild<QOpenGLWidget*>("gallerySpatialSurface");
        if (m_window->findChild<QWidget*>("gallerySplashScreen") || !controller)
            return;
        if (m_ready) {
            const int acknowledged = EM_ASM_INT({
                return Number(document.documentElement.dataset.fluentQtQualityAcknowledged || -1);
            });
            if (acknowledged != m_stage)
                return;
            m_ready = false;
            if (m_stage == 0) {
                GallerySettings::instance().setSpatialModeEnabled(true);
            } else if (m_stage == 1) {
                controller->cancelTransition();
            } else if (m_stage == 2) {
                GallerySettings::instance().setSpatialModeEnabled(false);
            } else {
                publishSmokeState("pass", QStringLiteral("2D, flat GPU, 3D and 2D return captured"));
                deleteLater();
                return;
            }
            ++m_stage;
            m_clock.restart();
            return;
        }
        if (controller->transitionRunning() || m_clock.elapsed() < 800)
            return;
        if (!m_button) {
            auto* page = m_window->currentContentPage();
            if (!page || page->routeId() != "popup")
                return;
            for (auto* candidate : page->findChildren<basicinput::Button*>())
                if (candidate->text() == "Show popup")
                    m_button = candidate;
            if (!m_button)
                return;
            if (auto* scroll = page->findChild<QScrollArea*>())
                scroll->ensureWidgetVisible(m_button, 0, 60);
            m_clock.restart();
            return;
        }
        if (m_stage == 1 && !m_flat) {
            if (!surface || !surface->property("presenting").toBool())
                return;
            controller->cancelTransition();
            auto* motion = controller->findChild<QVariantAnimation*>("galleryAssemblyAnimation");
            motion->setStartValue(0.0);
            motion->setEndValue(1.0);
            motion->setCurrentTime(motion->duration() / 2);
            motion->setCurrentTime(0);
            m_flat = true;
            m_clock.restart();
            return;
        }
        QJsonArray corners;
        const QRect rect = m_button->rect();
        for (const auto& point : {rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()}) {
            const QPoint position = m_window->mapToGlobal(controller->projectedPosition(m_button, point));
            corners.append(QJsonArray{position.x(), position.y()});
        }
        QJsonObject frame{{"stage", m_stage}, {"dpr", m_window->devicePixelRatioF()}, {"button", corners},
            {"cache", QJsonObject::fromVariantMap(controller->renderingStatistics())}};
        const auto json = QJsonDocument(frame).toJson(QJsonDocument::Compact);
        EM_ASM({ document.documentElement.dataset.fluentQtQualityFrame = UTF8ToString($0); }, json.constData());
        m_ready = true;
    }
    GalleryWindow* m_window;
    QPointer<basicinput::Button> m_button;
    QTimer m_tick;
    QElapsedTimer m_clock;
    int m_stage = 0;
    bool m_ready = false, m_flat = false;
};

// An explicit diagnostic URL drives a repeatable workload; normal visits create no probe.
class WasmSpatialProbe final : public QObject {
public:
    explicit WasmSpatialProbe(GalleryWindow* window, bool expectFallback)
        : QObject(window), m_window(window), m_expectFallback(expectFallback)
    {
        auto& settings = GallerySettings::instance();
        settings.setHomeParticlesEnabled(false);
        settings.setMotionMode(MotionPolicy::Mode::Full);
        settings.setSpatialModeEnabled(true);
        window->selectRoute(QStringLiteral("settings"));
        publishSmokeState("running", QStringLiteral("Waiting for the Spatial renderer"));
        m_clock.start();
        m_tick.setInterval(16);
        connect(&m_tick, &QTimer::timeout, this, [this] { tick(); });
        m_tick.start();
    }

private:
    void tick()
    {
        auto& settings = GallerySettings::instance();
        auto* controller = m_window->findChild<GallerySpatialController*>();
        if (m_stage == 0) {
            if (m_clock.elapsed() > 12000) {
                finish(false,
                    QStringLiteral("Spatial initialization timed out: %1").arg(settings.spatialUnavailableReason()));
                return;
            }
            if (settings.spatialAvailabilityPending()
                || m_window->findChild<QWidget*>(QStringLiteral("gallerySplashScreen")))
                return;
            if (!settings.spatialAvailable()) {
                m_result["fallback"] = true;
                if (!m_expectFallback) {
                    finish(false, settings.spatialUnavailableReason());
                    return;
                }
                if (!m_window->selectRoute(QStringLiteral("button"))) {
                    finish(false, QStringLiteral("The 2D fallback could not select a route"));
                    return;
                }
                m_clock.restart();
                m_stage = -1; // Route creation/presentation is deferred to the event loop.
                return;
            }
            if (m_expectFallback) {
                finish(false, QStringLiteral("Expected software rendering to select 2D"));
                return;
            }
            if (!controller || controller->transitionRunning())
                return;
            m_surface = m_window->findChild<QOpenGLWidget*>(QStringLiteral("gallerySpatialSurface"));
            if (!m_surface || !m_surface->property("presenting").toBool())
                return;
            m_surface->makeCurrent();
            const auto* renderer = m_surface->context()->functions()->glGetString(GL_RENDERER);
            m_result["renderer"] = QString::fromLatin1(reinterpret_cast<const char*>(renderer));
            m_surface->doneCurrent();
            m_result["width"] = m_window->width();
            m_result["height"] = m_window->height();
            m_result["dpr"] = m_window->devicePixelRatioF();
            connect(m_surface, &QOpenGLWidget::frameSwapped, this, [this] {
                if (m_stage == 1 || m_stage == 3)
                    ++m_frames;
            });
            m_clock.restart();
            m_result["statistics_before_motion"] = QJsonObject::fromVariantMap(controller->renderingStatistics());
            m_stage = 1;
        } else if (m_stage == -1) {
            auto* page = m_window->currentContentPage();
            if (!page || page->routeId() != QStringLiteral("button") || !page->isVisible()) {
                if (m_clock.elapsed() > 5000)
                    finish(false, QStringLiteral("The 2D fallback route did not become visible"));
                return;
            }
            auto* surface = m_window->findChild<QWidget*>(QStringLiteral("gallerySpatialSurface"));
            auto* navigationView = m_window->findChild<navigation::NavigationView*>();
            bool clicked = false;
            for (auto* button : page->findChildren<basicinput::Button*>()) {
                if (button->text() != QStringLiteral("Standard") || !button->isVisible() || !button->isEnabled())
                    continue;
                for (auto* parent = button->parentWidget(); parent; parent = parent->parentWidget())
                    if (auto* scroll = qobject_cast<QScrollArea*>(parent))
                        scroll->ensureWidgetVisible(button);
                const QPoint local = button->rect().center();
                const QPoint global = button->mapToGlobal(local);
                auto* hit = m_window->childAt(m_window->mapFromGlobal(global));
                if (!hit || (hit != button && !button->isAncestorOf(hit)))
                    continue;
                const auto connection = connect(button, &basicinput::Button::clicked, this, [&clicked] {
                    clicked = true;
                });
                QMouseEvent press(QEvent::MouseButtonPress, local, local, global, Qt::LeftButton, Qt::LeftButton,
                    Qt::NoModifier);
                QMouseEvent release(QEvent::MouseButtonRelease, local, local, global, Qt::LeftButton, Qt::NoButton,
                    Qt::NoModifier);
                QApplication::sendEvent(button, &press);
                QApplication::sendEvent(button, &release);
                disconnect(connection);
                break;
            }
            // Keep the saved 3D preference: the effective presentation must be 2D.
            const bool usable = !settings.spatialAvailable() && !settings.spatialUnavailableReason().isEmpty()
                && !surface && navigationView && !navigationView->graphicsEffect()
                && !navigationView->contentHost()->graphicsEffect() && clicked;
            m_result["two_dimensional_input"] = clicked;
            m_result["two_dimensional_usable"] = usable;
            finish(usable, settings.spatialUnavailableReason());
        } else if (m_stage == 1) {
            const qreal angle = m_clock.elapsed() * .003;
            const QPointF position(m_window->width() * (.5 + .32 * std::sin(angle)),
                m_window->height() * (.55 + .25 * std::cos(angle)));
            QMouseEvent move(QEvent::MouseMove, position, position, m_window->mapToGlobal(position.toPoint()),
                Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(m_window, &move);
            if (m_clock.elapsed() >= 3000) {
                m_result["moving_frames"] = m_frames;
                m_result["moving_ms"] = double(m_clock.elapsed());
                m_result["moving_fps"] = m_frames * 1000.0 / m_clock.elapsed();
                m_result["statistics_after_motion"] = QJsonObject::fromVariantMap(controller->renderingStatistics());
                const auto before = m_result["statistics_before_motion"].toObject();
                const auto after = m_result["statistics_after_motion"].toObject();
                if (after["backdropUploads"] != before["backdropUploads"]) {
                    finish(false, QStringLiteral("Pointer motion reuploaded an unchanged backdrop"));
                    return;
                }
                if (m_frames < 3) {
                    finish(false, QStringLiteral("Pointer following did not repaint the scene"));
                    return;
                }
                m_clock.restart();
                m_stage = 2;
            }
        } else if (m_stage == 2 && m_clock.elapsed() >= 600) {
            m_frames = 0;
            m_clock.restart();
            m_stage = 3;
        } else if (m_stage == 3 && m_clock.elapsed() >= 1500) {
            m_result["idle_frames"] = m_frames;
            m_result["idle_ms"] = double(m_clock.elapsed());
            auto* toggle = m_window->findChild<basicinput::ToggleSwitch*>(
                QStringLiteral("gallerySettingsSpatialModeToggle"));
            if (!toggle || !controller) {
                finish(false, QStringLiteral("The 3D switch was not created"));
                return;
            }
            const QPoint local = controller->projectedPosition(toggle, QPoint(20, toggle->height() / 2));
            const QPoint global = m_window->mapToGlobal(local);
            QMouseEvent press(QEvent::MouseButtonPress, local, local, global, Qt::LeftButton, Qt::LeftButton,
                Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease, local, local, global, Qt::LeftButton, Qt::NoButton,
                Qt::NoModifier);
            QApplication::sendEvent(m_window, &press);
            QApplication::sendEvent(m_window, &release);
            m_result["projected_click"] = !settings.spatialModeEnabled();
            if (settings.spatialModeEnabled()) {
                finish(false, QStringLiteral("The projected 3D switch did not receive its click"));
                return;
            }
            m_clock.restart();
            m_stage = 4;
        } else if (m_stage == 4 && !controller->transitionRunning()) {
            m_result["return_to_2d_ms"] = double(m_clock.elapsed());
            const auto statistics = controller->renderingStatistics();
            m_result["statistics_after_return_to_2d"] = QJsonObject::fromVariantMap(statistics);
            if (statistics["backdropTextureBytes"].toLongLong() || statistics["cacheEstimatedBytes"].toLongLong()) {
                finish(false, QStringLiteral("The 2D return retained GPU frame caches"));
                return;
            }
            if (m_surface->property("presenting").toBool()) {
                finish(false, QStringLiteral("The native 2D layout was not restored"));
                return;
            }
            settings.setSpatialModeEnabled(true);
            m_stage = 5;
        } else if (m_stage == 5 && !controller->transitionRunning()) {
            if (!m_surface || !m_surface->property("presenting").toBool()
                || !m_window->selectRoute(QStringLiteral("spatial-view"))) {
                finish(false, QStringLiteral("Could not open the Spatial component example"));
                return;
            }
            m_clock.restart();
            m_stage = 6;
        } else if (m_stage == 6) {
            if (m_clock.elapsed() > 6000) {
                finish(false, QStringLiteral("The Spatial component example did not initialize"));
                return;
            }
            auto* page = m_window->currentContentPage();
            auto* view = page ? page->findChild<spatial::SpatialView*>("spatialPreviewView") : nullptr;
            auto* scroll = page ? page->findChild<QScrollArea*>() : nullptr;
            auto* distance = page ? page->findChild<basicinput::Slider*>("spatialViewDistance") : nullptr;
            if (!view || !scroll || !distance || view->itemCount() != 2)
                return;
            scroll->ensureWidgetVisible(view);
            distance->setValue(900);
            m_preview = view;
            m_clock.restart();
            m_stage = 7;
        } else if (m_stage == 7 && m_clock.elapsed() >= 600) {
            const bool valid = m_preview && m_preview->isSpatialEnabled() && m_preview->cameraDistance() == 900
                && !m_preview->items().first()->projectedPolygon().isEmpty();
            m_result["spatial_example"] = valid;
            if (!valid) {
                finish(false, QStringLiteral("The Spatial example did not retain its state"));
                return;
            }
            settings.setHomeParticlesEnabled(true);
            settings.setSpatialModeEnabled(false);
            m_window->selectRoute(QStringLiteral("home"));
            m_clock.restart();
            m_stage = 8;
        } else if (m_stage == 8) {
            auto* particles = m_window->findChild<layout::ParticleBackdrop*>("galleryHomeParticles");
            if (!particles || !particles->isVisible() || m_clock.elapsed() < 600) {
                if (m_clock.elapsed() > 6000)
                    finish(false, QStringLiteral("The particle Home page did not become visible"));
                return;
            }
            const auto statistics = controller->renderingStatistics();
            if (statistics["particleLayers"].toInt() != 0) {
                finish(false, QStringLiteral("The 2D particle path retained a GPU layer"));
                return;
            }
            m_result["particle_cpu"] = QJsonObject::fromVariantMap(statistics);
            settings.setSpatialModeEnabled(true);
            m_clock.restart();
            m_stage = 9;
        } else if (m_stage == 9 && m_clock.elapsed() >= 800) {
            m_result["particle_before"] = QJsonObject::fromVariantMap(controller->renderingStatistics());
            m_clock.restart();
            m_stage = 10;
        } else if (m_stage == 10 && m_clock.elapsed() >= 600) {
            const auto statistics = controller->renderingStatistics();
            const bool accelerated = statistics["particleLayers"].toInt() > 0;
            m_result["particle_accelerated"] = accelerated;
            m_result["particle_after"] = QJsonObject::fromVariantMap(statistics);
            const auto before = m_result["particle_before"].toObject();
            if (accelerated && statistics["particleFrames"].toDouble() <= before["particleFrames"].toDouble()) {
                finish(false, QStringLiteral("The GPU particle texture stopped advancing"));
                return;
            }
            if (statistics["cacheEstimatedBytes"].toLongLong() > statistics["cacheBudgetBytes"].toLongLong()) {
                finish(false, QStringLiteral("Particle targets exceeded the shared cache budget"));
                return;
            }
            // High-DPI/large panels may legitimately retain CPU particles when
            // no auxiliary textures fit. Report that path instead of calling it GPU.
            settings.setSpatialModeEnabled(false);
            m_clock.restart();
            m_stage = 11;
        } else if (m_stage == 11 && !controller->transitionRunning()) {
            const auto statistics = controller->renderingStatistics();
            m_result["particle_return_to_2d"] = QJsonObject::fromVariantMap(statistics);
            if (statistics["particleLayers"].toInt() || statistics["particleBytes"].toLongLong()
                || statistics["cacheEstimatedBytes"].toLongLong()) {
                finish(false, QStringLiteral("Returning to 2D retained particle GPU resources"));
                return;
            }
            finish(true,
                QStringLiteral(
                    "3D input, idle, roundtrip, Spatial example and particle "
                    "CPU/automatic/release paths checked"));
        }
    }
    void finish(bool passed, const QString& detail)
    {
        m_tick.stop();
        m_result["detail"] = detail;
        const auto json = QJsonDocument(m_result).toJson(QJsonDocument::Compact);
        EM_ASM({ document.documentElement.dataset.fluentQtSpatialMetrics = UTF8ToString($0); }, json.constData());
        publishSmokeState(passed ? "pass" : "fail", detail);
        deleteLater();
    }
    GalleryWindow* m_window;
    QPointer<QOpenGLWidget> m_surface;
    QPointer<spatial::SpatialView> m_preview;
    QTimer m_tick;
    QElapsedTimer m_clock;
    QJsonObject m_result;
    int m_stage = 0;
    int m_frames = 0;
    bool m_expectFallback = false;
};
#endif

void startWasmSmokeIfRequested(GalleryWindow* window)
{
    const QString mode = smokeMode();
    if (window && mode == QStringLiteral("qmlplus")) {
        new WasmQmlPlusProbe(window);
        return;
    }
#if defined(FLUENT_QT_HAS_QML_BRIDGE) && defined(FLUENT_QT_HAS_SPATIAL)
    if (window && mode == QStringLiteral("qml-bridge")) {
        new WasmQmlBridgeProbe(window);
        return;
    }
#endif
    if (window && mode == QStringLiteral("source-input")) {
        new WasmSourceInputProbe(window);
        return;
    }
#ifdef FLUENT_QT_HAS_SPATIAL
    if (window && mode == QStringLiteral("spatial-quality")) {
        new WasmSpatialQualityProbe(window);
        return;
    }
    if (window && (mode == QStringLiteral("spatial") || mode == QStringLiteral("spatial-fallback"))) {
        new WasmSpatialProbe(window, mode == QStringLiteral("spatial-fallback"));
        return;
    }
#endif
    if (!window || (mode != QStringLiteral("fast") && mode != QStringLiteral("full")))
        return;
    (new WasmSmokeRunner(window, mode == QStringLiteral("full")))->start();
}

void prepareWasmSmokeIfRequested()
{
    if (smokeMode() == QStringLiteral("spatial"))
        qputenv("FLUENT_QT_SPATIAL_BENCHMARK", "1");
}

} // namespace fluent::gallery
