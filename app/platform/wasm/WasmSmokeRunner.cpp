#include "platform/wasm/WasmSmokeRunner.h"

#include <FluentQt/WebAssembly.h>

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QRegion>
#include <QSettings>
#include <QTimer>
#include <QUrlQuery>

#include <emscripten/emscripten.h>
#include <emscripten/heap.h>

#include "components/dialogs_flyouts/Dialog.h"
#include "components/menus_toolbars/Menu.h"
#include "components/textfields/LineEdit.h"
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
    EM_ASM({
        const state = UTF8ToString($0);
        const detail = UTF8ToString($1);
        document.documentElement.dataset.fluentQtSmoke = state;
        document.documentElement.dataset.fluentQtSmokeDetail = detail;
        console.log(`FLUENT_QT_WASM_SMOKE_${state.toUpperCase()}: ${detail}`);
    }, state, encodedDetail.constData());
}

void publishBrowserTextInputProbe(const char* state,
                                  const QPoint& globalPosition = QPoint(-1, -1),
                                  const QString& expectedText = {})
{
    const QByteArray encodedText = expectedText.toUtf8();
    EM_ASM({
        const root = document.documentElement;
        root.dataset.fluentQtTextInputState = UTF8ToString($0);
        root.dataset.fluentQtTextInputX = String($1);
        root.dataset.fluentQtTextInputY = String($2);
        root.dataset.fluentQtTextInputExpected = UTF8ToString($3);
    }, state, globalPosition.x(), globalPosition.y(), encodedText.constData());
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
    return heapBreak
        ? static_cast<qint64>(*heapBreak / bytesPerMiB)
        : 0;
}

class WasmSmokeRunner final : public QObject {
public:
    WasmSmokeRunner(GalleryWindow* window, bool full)
        : QObject(window)
        , m_window(window)
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
            const qint64 routeMs = m_routeTimer.elapsed();
            if (routeMs > m_slowestRouteMs) {
                m_slowestRouteMs = routeMs;
                m_slowestRoute = m_currentRoute;
            }
            LOG_INFO(QStringLiteral(
                         "WasmSmoke route ready id=%1 index=%2 total=%3 elapsedMs=%4")
                         .arg(m_currentRoute)
                         .arg(m_routeIndex)
                         .arg(m_routes.size())
                         .arg(routeMs));
            QTimer::singleShot(0, this, [this]() { visitNextRoute(); });
            return;
        }
        if (m_routeTimer.elapsed() > 30000)
            return fail(QStringLiteral("Timed out waiting for route %1").arg(m_currentRoute));
        QTimer::singleShot(25, this, [this]() { waitForCurrentRoute(); });
    }

    void runRuntimeChecks()
    {
        QWidget* browserSurface = m_window->window();
        if (!browserSurface
            || browserSurface->windowFlags().testFlag(
                Qt::WindowDoesNotAcceptFocus)
            || browserSurface->testAttribute(Qt::WA_ShowWithoutActivating)
            || !m_window->customWindowChromeEnabled()
            || !m_window->titleBar()
            || !m_window->titleBar()->isVisible()
            || !m_window->titleBar()->isWindowActive()
            || !m_window->titleBar()->testAttribute(Qt::WA_OpaquePaintEvent)
            || m_window->property(
                   "fluentPaintedSurfaceCacheGeneration").toInt() <= 0
            || !m_window->findChild<QWidget*>(
                QStringLiteral("fluentWindowFrameHost"))) {
            return fail(QStringLiteral(
                "Browser Gallery host is non-focusable or missing opaque/cached Fluent window chrome"));
        }

        auto& settings = GallerySettings::instance();
        settings.setThemeMode(GallerySettings::ThemeMode::Light);
        QSettings storage(QSettings::WebLocalStorageFormat,
                          QSettings::UserScope,
                          QCoreApplication::organizationName(),
                          QCoreApplication::applicationName());
        storage.sync();
        if (storage.value(QStringLiteral("settings/themeMode"), -1).toInt()
            != static_cast<int>(GallerySettings::ThemeMode::Light)) {
            return fail(QStringLiteral("WebLocalStorage theme persistence failed"));
        }

        const QStringList hanFallbacks =
            QFontDatabase::applicationFallbackFontFamilies(QChar::Script_Han);
        if (!hanFallbacks.contains(
                fluent::fontcompat::UISimplifiedChineseFamily)) {
            return fail(QStringLiteral(
                "Simplified Chinese application font fallback was not registered"));
        }
        const QFontMetrics fallbackMetrics{
            QFont(fluent::fontcompat::UISimplifiedChineseFamily)};
        if (!fallbackMetrics.inFont(QChar(0x6708))
            || !fallbackMetrics.inFont(QChar(0x5468))) {
            return fail(QStringLiteral(
                "Simplified Chinese fallback is missing calendar glyphs"));
        }

        runWindowCheck();
    }

    void runWindowCheck()
    {
        auto* window = new fluent::windowing::Window();
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->setWindowTitle(QStringLiteral("Web window smoke"));
        const QRect normalGeometry(
            m_window->geometry().center() - QPoint(320, 260),
            QSize(640, 520));
        fluent::webassembly::showWindow(window, normalGeometry);

        QTimer::singleShot(150, window, [this, window, normalGeometry]() {
            const bool valid = window->isVisible()
                && window->customWindowChromeEnabled()
                && window->titleBar()
                && window->titleBar()->isVisible()
                && window->findChild<QWidget*>(
                    QStringLiteral("fluentWindowFrameHost"))
                && window->width() >= window->minimumWidth()
                && window->height() >= window->minimumHeight();
            const bool geometryMatches = window->geometry() == normalGeometry;
            window->close();
            if (!valid || !geometryMatches) {
                fail(QStringLiteral(
                    "Secondary Fluent Window chrome/geometry check failed"));
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
        connect(dialog, &QDialog::finished, this,
                [this, dialog](int result) {
                    dialog->deleteLater();
                    if (result != QDialog::Accepted) {
                        fail(QStringLiteral(
                            "Asynchronous dialog did not complete"));
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
        connect(menu, &QMenu::aboutToHide, this,
                [this, menu]() {
                    menu->deleteLater();
                    QTimer::singleShot(0, this,
                                       [this]() { runBrowserTextInputCheck(); });
        });
        menu->popup(m_window->mapToGlobal(QPoint(24, 24)));
        QTimer::singleShot(0, menu, [this, menu]() {
            bool actionGeometryValid = !menu->actions().isEmpty();
            QStringList geometrySummary;
            for (QAction* action : menu->actions()) {
                if (!action || action->isSeparator() || !action->isVisible())
                    continue;
                const QRect actionRect = menu->actionGeometry(action);
                geometrySummary.append(
                    QStringLiteral("%1:%2,%3,%4x%5")
                        .arg(action->text())
                        .arg(actionRect.x())
                        .arg(actionRect.y())
                        .arg(actionRect.width())
                        .arg(actionRect.height()));
                actionGeometryValid = actionGeometryValid
                    && !actionRect.isEmpty()
                    && menu->rect().contains(actionRect.center());
            }
            const QRegion surfaceMask = menu->mask();
            const bool roundedMaskValid = !surfaceMask.isEmpty()
                && !surfaceMask.contains(menu->rect().topLeft())
                && !surfaceMask.contains(menu->rect().topRight())
                && !surfaceMask.contains(menu->rect().bottomRight())
                && !surfaceMask.contains(menu->rect().bottomLeft())
                && surfaceMask.contains(menu->rect().center());
            if (!menu->isVisible()
                || menu->testAttribute(Qt::WA_TranslucentBackground)
                || !actionGeometryValid
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
        connect(input, &QLineEdit::textChanged, this,
                [this, guard](const QString& text) {
                    if (!guard || text != expectedText)
                        return;
                    publishBrowserTextInputProbe("pass");
                    guard->deleteLater();
                    QTimer::singleShot(
                        0, this,
                        [this]() { runTextEditingMenuCheck(); });
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
            publishBrowserTextInputProbe(
                "ready",
                guard->mapToGlobal(guard->rect().center()),
                expectedText);
        });
        QTimer::singleShot(10000, this, [this, guard]() {
            if (!guard)
                return;
            publishBrowserTextInputProbe("fail");
            guard->deleteLater();
            fail(QStringLiteral(
                "Browser keyboard input did not reach the hosted LineEdit"));
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
        QContextMenuEvent event(QContextMenuEvent::Mouse,
                                localPosition,
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
                    menu->findChildren<QMenu*>(
                        QString(), Qt::FindDirectChildrenOnly);
                sourceMenusHidden = !sourceMenus.isEmpty();
                for (QMenu* sourceMenu : sourceMenus) {
                    sourceMenusHidden = sourceMenusHidden
                        && sourceMenu
                        && sourceMenu->isHidden()
                        && sourceMenu->testAttribute(
                            Qt::WA_DontShowOnScreen);
                }
                for (QAction* action : menu->actions()) {
                    if (!action || action->isSeparator() || !action->isVisible())
                        continue;
                    ++visibleActionCount;
                    const QRect actionRect = menu->actionGeometry(action);
                    actionGeometrySummary.append(
                        QStringLiteral("%1:%2,%3,%4x%5")
                            .arg(action->text())
                            .arg(actionRect.x())
                            .arg(actionRect.y())
                            .arg(actionRect.width())
                            .arg(actionRect.height()));
                    actionGeometryValid = actionGeometryValid
                        && !actionRect.isEmpty()
                        && menu->rect().contains(actionRect.center());
                }
            }
            actionGeometryValid = actionGeometryValid
                && visibleActionCount > 0;
            if (!menu
                || menu->objectName()
                    != QStringLiteral("FluentLineEdit.ContextMenu")
                || menu->testAttribute(Qt::WA_TranslucentBackground)
                || !actionGeometryValid
                || !sourceMenusHidden
                || menu->width() >= m_window->width()
                || menu->height() >= m_window->height()) {
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
                         .arg(menu && menu->testAttribute(
                             Qt::WA_TranslucentBackground))
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

            connect(menu, &QMenu::aboutToHide, this,
                    [this, password]() {
                        password->deleteLater();
                        complete();
                    });
            QTimer::singleShot(25, menu, &QMenu::close);
        });
    }

    void complete()
    {
        const qint64 totalMs = m_totalTimer.elapsed();
        const qint64 finalHeapMiB = heapCapacityMiB();
        const qint64 finalHeapBreakMiB = heapBreakMiB();
        publishSmokeState("pass",
                          QStringLiteral(
                              "%1 routes, storage, window, dialog, menu, browser text input, and text menu passed in %2 ms; "
                              "CJK fallback passed; "
                              "slowest route %3 took %4 ms; heap %5 -> %6 MiB; "
                              "break %7 -> %8 MiB")
                              .arg(m_routes.size())
                              .arg(totalMs)
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
