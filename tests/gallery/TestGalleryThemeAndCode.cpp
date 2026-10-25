#include <gtest/gtest.h>
#include <FluentQt/FluentQt.h>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QOpenGLWidget>
#include <QPaintEvent>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStyleFactory>
#include <QTest>
#include <QTimer>
#include <QScrollBar>
#include <QTableView>
#include <QTextEdit>
#include <QTreeView>
#include <QWheelEvent>
#include <QHeaderView>
#include <algorithm>
#include <ctime>
#include <functional>
#include <iostream>
#include "QtTestEnvironment.h"
#include "view/pages/GalleryComponentPage.h"
#include "view/pages/SettingsPage.h"
#include "view/shell/GalleryContentPresenter.h"
#include "view/shell/GallerySpatialController.h"
#include "view/shell/GalleryWindow.h"
#include "view/widgets/GalleryCodeBlock.h"
#include "view/widgets/GallerySampleCard.h"
#include "viewmodel/GallerySettings.h"

using namespace fluent;
using namespace fluent::gallery;

TEST(GalleryThemeAndCodeTest, NativeListDetailSurfaces)
{
    const QString output = qEnvironmentVariable("FLUENT_QT_LIST_DETAIL_EVIDENCE");
    if (output.isEmpty() || tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires an opted-in native Gallery surface review";
    QDir().mkpath(output);
    auto& settings = GallerySettings::instance();
    const auto oldTheme = settings.themeMode();
    const bool oldSpatial = settings.spatialModeEnabled();
    const bool oldIntro = settings.introCompleted();
    const auto restore = qScopeGuard([&] {
        settings.setSpatialModeEnabled(false);
        settings.setThemeMode(oldTheme);
        settings.setIntroCompleted(oldIntro);
        settings.setSpatialModeEnabled(oldSpatial);
        qApp->setStyle(QStringLiteral("Fusion"));
    });
    if (QGuiApplication::platformName() == QLatin1String("cocoa")) {
        for (const auto& name : QStyleFactory::keys())
            if (name.contains("mac", Qt::CaseInsensitive))
                qApp->setStyle(QStyleFactory::create(name));
    }
    settings.setSpatialModeEnabled(false);
    settings.setThemeMode(GallerySettings::ThemeMode::Light);
    settings.setIntroCompleted(true);
    GalleryWindow window;
    window.resize(1240, 850);
    window.show();
    window.raise();
    window.activateWindow();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !window.findChild<QWidget*>("gallerySplashScreen"); }, 60000));
    ASSERT_TRUE(window.selectRoute(QStringLiteral("list-detail-view")));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            auto* page = qobject_cast<GalleryComponentPage*>(window.currentContentPage());
            return page && !page->hasPendingSamples();
        },
        10000));
    auto* view = window.findChild<layout::ListDetailView*>("fileBrowserLayout");
    auto* scroll = window.currentContentPage()->findChild<QScrollArea*>();
    auto* controller = window.findChild<GallerySpatialController*>();
    ASSERT_TRUE(view && scroll && controller);
    QJsonArray results;
    for (bool spatial : {false, true}) {
        settings.setSpatialModeEnabled(spatial);
        for (auto theme : {GallerySettings::ThemeMode::Light, GallerySettings::ThemeMode::Dark}) {
            settings.setThemeMode(theme);
            if (spatial) {
                ASSERT_TRUE(QTest::qWaitFor(
                    [&] {
                        auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
                        return surface && surface->property("presenting").toBool();
                    },
                    10000));
            }
            QTest::qWait(200);
            scroll->ensureWidgetVisible(view, 20, 20);
            controller->cancelTransition();
            QTest::qWait(300);
            const QString state =
                QStringLiteral("cpp-%1-%2")
                    .arg(spatial ? "3d" : "2d")
                    .arg(theme == GallerySettings::ThemeMode::Light ? "light" : "dark");
            auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
            const QImage shot = spatial ? surface->grabFramebuffer() : window.grab().toImage();
            ASSERT_TRUE(shot.save(output + "/" + state + ".png"));
            const auto pixel = [&](QWidget* widget, QPoint local) {
                QPoint p = spatial
                    ? surface->mapFrom(&window, controller->projectedPosition(widget, local))
                    : widget->mapTo(&window, local);
                return shot.pixelColor(
                    QPoint(qRound(p.x() * window.devicePixelRatioF()), qRound(p.y() * window.devicePixelRatioF())));
            };
            const QColor left = pixel(view->listPane(), QPoint(4, 18));
            const QColor right = pixel(view->detailPane(), QPoint(4, 18));
            const QColor expected = view->themeColors().bgLayer;
            results.append(QJsonObject{{"state", state}, {"left", left.name()}, {"right", right.name()},
                {"expected", expected.name()},
                {"panePalette", view->listPane()->palette().color(QPalette::Window).name()}});
            SCOPED_TRACE(state.toStdString());
            EXPECT_LE(qAbs(left.red() - expected.red()), 3);
            EXPECT_LE(qAbs(left.green() - expected.green()), 3);
            EXPECT_LE(qAbs(left.blue() - expected.blue()), 3);
            EXPECT_LE(qAbs(left.red() - right.red()), 3);

            auto* basic = window.findChild<layout::ListDetailView*>("basicListDetailView");
            auto* width = window.findChild<basicinput::ComboBox*>("listDetailPreviewWidth");
            ASSERT_TRUE(basic && width);
            scroll->ensureWidgetVisible(basic->parentWidget(), 20, 20);
            QTest::qWait(100);
            controller->cancelTransition();
            const auto capture = [&](const QString& suffix) {
                QTest::qWait(100);
                const auto image = spatial ? surface->grabFramebuffer() : window.grab().toImage();
                EXPECT_TRUE(image.save(output + "/" + state + suffix + ".png"));
            };
            const auto click = [&](QWidget* control) {
                ASSERT_NE(control, nullptr);
                if (spatial)
                    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                        controller->projectedPosition(control, control->rect().center()));
                else
                    QTest::mouseClick(control, Qt::LeftButton);
            };
            basicinput::Button* open = nullptr;
            basicinput::Button* back = nullptr;
            for (auto* button : basic->findChildren<basicinput::Button*>()) {
                if (button->text() == QStringLiteral("Open meeting note"))
                    open = button;
                if (button->text() == QStringLiteral("Back to notes"))
                    back = button;
            }
            auto* editor = basic->findChild<textfields::LineEdit*>();
            ASSERT_TRUE(open && back && editor);
            width->setCurrentIndex(0);
            basic->showList();
            capture("-basic-wide");
            EXPECT_FALSE(basic->isCompact());
            width->setFocus();
            QTest::keyClick(width, Qt::Key_End);
            ASSERT_TRUE(QTest::qWaitFor([&] { return basic->isCompact(); }, 2000));
            capture("-basic-narrow-list");
            click(open);
            ASSERT_TRUE(QTest::qWaitFor([&] { return basic->detailPane()->isVisible(); }, 2000));
            click(editor);
            QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier);
            QTest::keyClicks(editor, "A draft kept across layouts");
            capture("-basic-narrow-detail");
            click(back);
            EXPECT_EQ(basic->currentPane(), layout::ListDetailView::List);
            click(open);
            width->setCurrentIndex(0);
            ASSERT_TRUE(QTest::qWaitFor([&] { return !basic->isCompact(); }, 2000));
            EXPECT_EQ(editor->text(), QStringLiteral("A draft kept across layouts"));
        }
    }
    QFile result(output + "/cpp-surfaces.json");
    ASSERT_TRUE(result.open(QIODevice::WriteOnly));
    result.write(QJsonDocument(results).toJson());
}

// Includes normal startup and route caching. Keep timing out of pass/fail assertions.
TEST(GalleryThemeAndCodeTest, NativeGalleryInteractionProbe)
{
    if (qEnvironmentVariableIsEmpty("FLUENT_QT_GALLERY_INTERACTION_BENCHMARK") || tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires an opted-in native desktop and GPU";
    auto& settings = GallerySettings::instance();
    const auto oldTheme = settings.themeMode();
    const auto oldMotion = settings.motionMode();
    const auto oldNavigation = settings.navigationStyle();
    const bool oldSpatial = settings.spatialModeEnabled();
    const bool oldIntro = settings.introCompleted();
    const auto restore = qScopeGuard([&] {
        settings.setSpatialModeEnabled(false);
        settings.setThemeMode(oldTheme);
        settings.setMotionMode(oldMotion);
        settings.setNavigationStyle(oldNavigation);
        settings.setIntroCompleted(oldIntro);
        settings.setSpatialModeEnabled(oldSpatial);
        qApp->setStyle(QStringLiteral("Fusion"));
    });
    if (QGuiApplication::platformName() == QLatin1String("cocoa")) {
        for (const auto& name : QStyleFactory::keys())
            if (name.contains("mac", Qt::CaseInsensitive))
                qApp->setStyle(QStyleFactory::create(name));
    }
    settings.setSpatialModeEnabled(false);
    settings.setThemeMode(GallerySettings::ThemeMode::Light);
    settings.setMotionMode(GallerySettings::MotionMode::Full);
    settings.setNavigationStyle(GallerySettings::NavigationStyle::Left);
    settings.setIntroCompleted(true);
    QElapsedTimer startup;
    startup.start();
    GalleryWindow window;
    window.resize(1240, 850);
    window.show();
    window.raise();
    window.activateWindow();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !window.findChild<QWidget*>("gallerySplashScreen"); }, 60000));
    const auto startupMs = startup.elapsed();
    auto* controller = window.findChild<GallerySpatialController*>();
    ASSERT_NE(controller, nullptr);
    const QString output = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
    const bool resizeOnly = qEnvironmentVariableIntValue("FLUENT_QT_GALLERY_RESIZE_ONLY") != 0;
    const bool componentMatrix = qEnvironmentVariableIsSet("FLUENT_QT_GALLERY_COMPONENT_MATRIX");
    ASSERT_FALSE(output.isEmpty());
    QDir().mkpath(output);
    QJsonArray measurements;
    QWidget* measuredWidget = nullptr;
    const auto record = [&](const QString& phase, const std::function<void(int)>& action = {}) {
        std::cout << "INTERACTION_PHASE " << phase.toStdString() << std::endl;
        class BackingPaintProbe : public QObject {
        public:
            QWidget* window = nullptr;
            QWidget* control = nullptr;
            qint64 pixels = 0;
            int paints = 0, controlPaints = 0;
            bool eventFilter(QObject* object, QEvent* event) override
            {
                if (event->type() == QEvent::Paint && object == control)
                    ++controlPaints;
                if (event->type() == QEvent::Paint && object == window) {
                    ++paints;
                    for (const QRect& rect : static_cast<QPaintEvent*>(event)->region())
                        pixels += qint64(rect.width()) * rect.height();
                }
                return false;
            }
        } backing;
        backing.window = &window;
        backing.control = measuredWidget;
        qApp->installEventFilter(&backing);
        QElapsedTimer elapsed, heartbeat;
        QVector<qint64> gaps, frameGaps;
        int ticks = 0;
        QTimer pulse;
        pulse.setInterval(16);
        pulse.setTimerType(Qt::PreciseTimer);
        QObject::connect(&pulse, &QTimer::timeout, &window, [&] {
            gaps.append(heartbeat.restart());
            if (action)
                action(ticks);
            ++ticks;
        });
        QElapsedTimer presentationClock;
        presentationClock.start();
        if (auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface")) {
            QObject::connect(surface, &QOpenGLWidget::frameSwapped, &pulse, [&] {
                frameGaps.append(presentationClock.restart());
            });
        }
        const auto before = controller->renderingStatistics();
        const auto cpu = std::clock();
        elapsed.start();
        heartbeat.start();
        pulse.start();
        QTest::qWait(qEnvironmentVariableIntValue("FLUENT_QT_GALLERY_PROBE_MS") > 0
                ? qEnvironmentVariableIntValue("FLUENT_QT_GALLERY_PROBE_MS")
                : 2000);
        pulse.stop();
        auto stats = controller->renderingStatistics();
        for (auto it = stats.begin(); it != stats.end(); ++it)
            if (it.key().endsWith("Captures") || it.key().endsWith("CaptureMs") || it.key() == "paints"
                || it.key() == "paintMs")
                it.value() = it.value().toDouble() - before.value(it.key()).toDouble();
        std::sort(gaps.begin(), gaps.end());
        stats["phase"] = phase;
        stats["cpuMs"] = 1000.0 * (std::clock() - cpu) / CLOCKS_PER_SEC;
        stats["elapsedMs"] = elapsed.elapsed();
        stats["ticks"] = ticks;
        stats["p95EventGapMs"] = gaps.isEmpty() ? 0 : gaps.at(qMin(gaps.size() - 1, gaps.size() * 95 / 100));
        stats["maxEventGapMs"] = gaps.isEmpty() ? 0 : gaps.last();
        std::sort(frameGaps.begin(), frameGaps.end());
        stats["frameSwaps"] = frameGaps.size();
        stats["p95FrameGapMs"] =
            frameGaps.isEmpty() ? 0 : frameGaps.at(qMin(frameGaps.size() - 1, frameGaps.size() * 95 / 100));
        stats["maxFrameGapMs"] = frameGaps.isEmpty() ? 0 : frameGaps.last();
        stats["widgetPaints"] = backing.controlPaints;
        stats["backingPaints"] = backing.paints;
        stats["backingPaintPixels"] = backing.pixels;
        stats["widgets"] = window.findChildren<QWidget*>().size();
        stats["startupMs"] = startupMs;
        measurements.append(QJsonObject::fromVariantMap(stats));
        QFile result(output + "/interactions.json");
        ASSERT_TRUE(result.open(QIODevice::WriteOnly));
        result.write(QJsonDocument(measurements).toJson());
    };
    const auto select = [&](const QString& route) {
        EXPECT_TRUE(window.selectRoute(route));
        EXPECT_TRUE(QTest::qWaitFor(
            [&] {
                const auto* page = qobject_cast<GalleryComponentPage*>(window.currentContentPage());
                return page && page->routeId() == route && !page->hasPendingSamples();
            },
            10000));
        QTest::qWait(400);
    };
    for (const bool spatial : {false, true}) {
        settings.setSpatialModeEnabled(spatial);
        if (spatial)
            ASSERT_TRUE(QTest::qWaitFor(
                [&] {
                    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
                    return surface && surface->property("presenting").toBool() && !controller->transitionRunning();
                },
                10000));
        const QString mode = spatial ? QStringLiteral("3d-") : QStringLiteral("2d-");
        if (componentMatrix) {
            const auto reveal = [&](QWidget* control) {
                EXPECT_NE(control, nullptr);
                auto* scroll = window.currentContentPage()->findChild<QScrollArea*>();
                EXPECT_NE(scroll, nullptr);
                if (control && scroll)
                    scroll->ensureWidgetVisible(control, 20, 90);
                QTest::qWait(300);
                controller->cancelTransition();
            };
            const auto capture = [&](const QString& name) {
                QTest::qWait(100);
                auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
                const auto shot = spatial && surface ? surface->grabFramebuffer() : window.grab().toImage();
                EXPECT_TRUE(shot.save(output + "/" + mode + name + ".png"));
                if (spatial) {
                    const auto stats = controller->renderingStatistics();
                    EXPECT_GE(stats["cacheDpr"].toDouble(), window.devicePixelRatioF());
                    EXPECT_LE(stats["cacheEstimatedBytes"].toLongLong(), stats["cacheBudgetBytes"].toLongLong());
                }
            };
            const auto roundTrip = [&](const std::function<void()>& verify) {
                if (!spatial)
                    return;
                settings.setSpatialModeEnabled(false);
                ASSERT_TRUE(QTest::qWaitFor([&] { return !controller->transitionRunning(); }, 3000));
                verify();
                settings.setSpatialModeEnabled(true);
                ASSERT_TRUE(QTest::qWaitFor([&] { return !controller->transitionRunning(); }, 3000));
                verify();
            };
            const auto drag = [&](QWidget* source, const QPoint& start, const QString& name,
                                  const std::function<QPoint(int)>& pointAt, const std::function<void()>& observe) {
                measuredWidget = source;
                QWidget* receiver = source->childAt(start);
                if (!receiver)
                    receiver = source;
                QWidget* target = spatial ? static_cast<QWidget*>(&window) : receiver;
                const auto map = [&](const QPoint& point) {
                    return spatial ? controller->projectedPosition(source, point) : receiver->mapFrom(source, point);
                };
                QPoint point = map(start);
                QTest::mousePress(target, Qt::LeftButton, Qt::NoModifier, point);
                record(mode + name, [&](int tick) {
                    point = map(pointAt(tick));
                    QMouseEvent move(QEvent::MouseMove, point, target->mapToGlobal(point), Qt::NoButton, Qt::LeftButton,
                        Qt::NoModifier);
                    QApplication::sendEvent(target, &move);
                    observe();
                });
                QTest::mouseRelease(target, Qt::LeftButton, Qt::NoModifier, point);
            };
            const auto wheel = [&](QAbstractScrollArea* view, const QString& name) {
                ASSERT_NE(view, nullptr);
                reveal(view);
                auto* outer = window.currentContentPage()->findChild<QScrollArea*>();
                const int outerBefore = outer->verticalScrollBar()->value();
                measuredWidget = view->viewport();
                auto* bar = view->verticalScrollBar();
                ASSERT_GT(bar->maximum(), 0);
                bar->setValue(0);
                bool down = true;
                int reached = 0;
                record(mode + name, [&](int) {
                    if (bar->value() == bar->maximum())
                        down = false;
                    else if (bar->value() == 0)
                        down = true;
                    const QPoint center = view->viewport()->rect().center();
                    const QPoint point = spatial ? controller->projectedPosition(view->viewport(), center) : center;
                    QWidget* target = spatial ? static_cast<QWidget*>(&window) : view->viewport();
                    QWheelEvent event(point, target->mapToGlobal(point), QPoint(0, down ? -12 : 12),
                        QPoint(0, down ? -120 : 120), Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false);
                    QApplication::sendEvent(target, &event);
                    reached = qMax(reached, bar->value());
                });
                EXPECT_GT(reached, 0);
                EXPECT_EQ(outer->verticalScrollBar()->value(), outerBefore);
                const int value = bar->value();
                roundTrip([&] { EXPECT_EQ(bar->value(), value); });
                capture(name);
            };

            select("slider");
            auto* slider = window.currentContentPage()->findChild<basicinput::Slider*>();
            ASSERT_NE(slider, nullptr);
            reveal(slider);
            slider->setValue(50);
            int minimum = 50, maximum = 50;
            drag(
                slider, slider->rect().center(), "slider-drag",
                [&](int tick) { return QPoint(25 + (tick % 50) * 4, slider->height() / 2); },
                [&] {
                    minimum = qMin(minimum, slider->value());
                    maximum = qMax(maximum, slider->value());
                });
            EXPECT_LT(minimum, 20);
            EXPECT_GT(maximum, 70);
            EXPECT_FALSE(slider->isSliderDown());
            const int sliderValue = slider->value();
            roundTrip([&] { EXPECT_EQ(slider->value(), sliderValue); });
            capture("slider-drag");

            select("split-view");
            auto* split = window.currentContentPage()->findChild<collections::SplitView*>();
            ASSERT_NE(split, nullptr);
            reveal(split);
            const auto splitState = split->saveState();
            const QPoint handle = split->handleGeometry(0).center();
            const int initialWidth = split->paneGeometry(0).width();
            int widest = initialWidth;
            drag(
                split, handle, "split-view-drag", [&](int tick) { return handle + QPoint((tick % 30) * 2, 0); },
                [&] { widest = qMax(widest, split->paneGeometry(0).width()); });
            EXPECT_GT(widest, initialWidth + 20);
            EXPECT_FALSE(split->resizing());
            const int splitWidth = split->paneGeometry(0).width();
            roundTrip([&] { EXPECT_EQ(split->paneGeometry(0).width(), splitWidth); });
            capture("split-view-drag");
            EXPECT_TRUE(split->restoreState(splitState));

            select("list-view");
            auto* list = window.currentContentPage()->findChild<collections::ListView*>();
            ASSERT_NE(list, nullptr);
            const auto listIndex = list->currentIndex();
            wheel(list, "list-view-wheel");
            EXPECT_EQ(list->currentIndex(), listIndex);

            select("tree-view");
            auto* tree = window.currentContentPage()->findChild<collections::TreeView*>();
            ASSERT_NE(tree, nullptr);
            tree->expandAll();
            tree->setFixedHeight(160);
            wheel(tree, "tree-view-wheel");

            select("text-edit");
            textfields::TextEdit* text = nullptr;
            for (auto* candidate : window.currentContentPage()->findChildren<textfields::TextEdit*>())
                if (candidate->accessibleName() == "Scrollable notes")
                    text = candidate;
            ASSERT_NE(text, nullptr);
            const QString originalText = text->toPlainText();
            wheel(text->findChild<QTextEdit*>(), "text-edit-wheel");
            EXPECT_EQ(text->toPlainText(), originalText);

            select("expander");
            auto* expander = window.currentContentPage()->findChild<layout::Expander*>("galleryExpanderTextContent");
            ASSERT_NE(expander, nullptr);
            reveal(expander);
            int toggles = 0;
            measuredWidget = expander;
            record(mode + "expander-animation", [&](int tick) {
                if (tick % 16)
                    return;
                auto* button = expander->headerButton();
                const QPoint point =
                    spatial ? controller->projectedPosition(button, button->rect().center()) : button->rect().center();
                QTest::mouseClick(spatial ? static_cast<QWidget*>(&window) : button, Qt::LeftButton, Qt::NoModifier,
                    point);
                ++toggles;
            });
            EXPECT_GT(toggles, 2);
            expander->setExpandedAnimated(true, false);
            roundTrip([&] { EXPECT_TRUE(expander->isExpanded()); });
            capture("expander-animation");

            select("data-grid");
            auto* code = window.currentContentPage()->findChild<GalleryCodeBlock*>();
            ASSERT_NE(code, nullptr);
            reveal(code->headerButton());
            auto* sourceScroll = window.currentContentPage()->findChild<QScrollArea*>();
            ASSERT_NE(sourceScroll, nullptr);
            sourceScroll->verticalScrollBar()->setValue(code->mapTo(sourceScroll->widget(), QPoint()).y() - 100);
            QTest::qWait(100);
            measuredWidget = code;
            record(mode + "source-expansion", [&](int tick) {
                if (tick % 25 == 0)
                    code->setExpanded(!code->isExpanded());
            });
            code->setExpanded(true, false);
            roundTrip([&] { EXPECT_TRUE(code->isExpanded()); });
            capture("source-expansion");

            select("line-chart");
            auto* chart = window.currentContentPage()->findChild<charts::ChartView*>();
            ASSERT_NE(chart, nullptr);
            reveal(chart);
            auto* model = chart->model();
            ASSERT_NE(model, nullptr);
            const auto original = model->dataSnapshot();
            measuredWidget = chart;
            record(mode + "chart-stream", [&](int tick) {
                model->setPoints({{0, 10. + tick % 20}, {1, 35. - tick % 20}, {2, 18}, {3, 45}});
            });
            const int frameRate = chart->maximumFrameRate();
            chart->setMaximumFrameRate(60);
            record(mode + "chart-stream-60", [&](int tick) {
                model->setPoints({{0, 10. + tick % 20}, {1, 35. - tick % 20}, {2, 18}, {3, 45}});
            });
            chart->setMaximumFrameRate(frameRate);
            EXPECT_EQ(chart->model(), model);
            roundTrip([&] {
                EXPECT_EQ(chart->model(), model);
                EXPECT_EQ(model->rowCount(), 4);
            });
            capture("chart-stream");
            EXPECT_TRUE(model->setDataSnapshot(original));
            QTest::qWait(600);
            record(mode + "settled-idle");
            continue;
        }
        select("data-grid");
        auto* page = window.currentContentPage();
        auto* scroll = page->findChild<QScrollArea*>();
        ASSERT_NE(scroll, nullptr);
        auto grids = page->findChildren<collections::DataGrid*>();
        ASSERT_FALSE(grids.isEmpty());
        auto* grid = grids.first();
        scroll->verticalScrollBar()->setValue(grid->mapTo(scroll->widget(), QPoint()).y() - 150);
        QTest::qWait(400);
        if (!resizeOnly) {
            record(mode + "grid-idle");
            record(mode + "grid-horizontal", [&](int tick) {
                const int maximum = grid->horizontalScrollBar()->maximum();
                grid->horizontalScrollBar()->setValue(
                    (tick / 30) % 2 ? maximum - (tick % 30) * maximum / 30 : (tick % 30) * maximum / 30);
            });
        }
        grid->horizontalScrollBar()->setValue(0);
        for (auto* table : grids) {
            const int rows = table->model() ? table->model()->rowCount() : 0;
            if (rows != 2 && rows != 100000)
                continue;
            scroll->verticalScrollBar()->setValue(table->mapTo(scroll->widget(), QPoint()).y() - 150);
            QTest::qWait(300);
            const int originalWidth = table->columnWidth(0);
            record(mode + QStringLiteral("header-resize-%1-rows").arg(rows), [&](int tick) {
                const int offset = (tick / 30) % 2 ? 120 - (tick % 30) * 4 : (tick % 30) * 4;
                table->horizontalHeader()->resizeSection(0, originalWidth + offset);
            });
            table->setColumnWidth(0, originalWidth);
            QTest::qWait(100);
            auto* header = table->horizontalHeader();
            const QPoint handle(header->sectionViewportPosition(0) + originalWidth - 1, header->height() / 2);
            QWidget* target = spatial ? static_cast<QWidget*>(&window) : header->viewport();
            const auto mapHandle = [&](const QPoint& point) {
                return spatial ? controller->projectedPosition(header->viewport(), point) : point;
            };
            QPoint pointer = mapHandle(handle);
            int widest = originalWidth;
            QTest::mousePress(target, Qt::LeftButton, Qt::NoModifier, pointer);
            record(mode + QStringLiteral("pointer-resize-%1-rows").arg(rows), [&](int tick) {
                const int offset = (tick / 30) % 2 ? 120 - (tick % 30) * 4 : (tick % 30) * 4;
                pointer = mapHandle(handle + QPoint(offset, 0));
                QMouseEvent move(QEvent::MouseMove, pointer, target->mapToGlobal(pointer), Qt::NoButton, Qt::LeftButton,
                    Qt::NoModifier);
                QApplication::sendEvent(target, &move);
                widest = qMax(widest, table->columnWidth(0));
            });
            QTest::mouseRelease(target, Qt::LeftButton, Qt::NoModifier, pointer);
            EXPECT_GT(widest, originalWidth + 80) << "Pointer input must reach the resize handle";
            table->setColumnWidth(0, originalWidth);
        }
        if (resizeOnly)
            continue;
        record(mode + "page-scroll", [&](int tick) {
            const int maximum = qMin(900, scroll->verticalScrollBar()->maximum());
            scroll->verticalScrollBar()->setValue(
                (tick / 60) % 2 ? maximum - (tick % 60) * maximum / 60 : (tick % 60) * maximum / 60);
        });
        scroll->verticalScrollBar()->setValue(grid->mapTo(scroll->widget(), QPoint()).y() - 150);
        QTest::qWait(400);
        record(mode + "theme", [&](int tick) {
            if (tick % 30 == 0)
                settings.setThemeMode(
                    (tick / 30) % 2 ? GallerySettings::ThemeMode::Light : GallerySettings::ThemeMode::Dark);
        });
        if (auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface"); spatial && surface)
            surface->grabFramebuffer().save(output + "/" + mode + "grid.png");
        else
            window.grab().save(output + "/" + mode + "grid.png");
        select("button");
        record(mode + "button-idle");
        if (spatial) {
            for (int cycle = 0; cycle < 4; ++cycle) {
                const auto theme = cycle % 2 ? GallerySettings::ThemeMode::Dark : GallerySettings::ThemeMode::Light;
                settings.setThemeMode(theme);
                auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
                ASSERT_NE(surface, nullptr);
                surface->grabFramebuffer().save(output + "/immediate-theme-" + QString::number(cycle) + ".png");
                QTest::qWait(600);
                surface->grabFramebuffer().save(output + "/settled-theme-" + QString::number(cycle) + ".png");
            }
        }
    }
}

// Opt-in native evidence. Timing is reported, never made a machine-dependent CI threshold.
TEST(GalleryThemeAndCodeTest, NativeGlobalThemeAndSourceExpansionProbe)
{
    if (qEnvironmentVariableIsEmpty("FLUENT_QT_THEME_CODE_BENCHMARK") || tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires an opted-in native desktop and GPU";
    auto& settings = GallerySettings::instance();
    const auto oldTheme = settings.themeMode();
    const auto oldMotion = settings.motionMode();
    const auto oldNavigation = settings.navigationStyle();
    const auto oldEffect = settings.windowEffect();
    const bool oldSpatial = settings.spatialModeEnabled();
    const bool oldParticles = settings.homeParticlesEnabled();
    const bool oldIntro = settings.introCompleted();
    const auto restore = qScopeGuard([&] {
        settings.setSpatialModeEnabled(false);
        settings.setThemeMode(oldTheme);
        settings.setMotionMode(oldMotion);
        settings.setNavigationStyle(oldNavigation);
        settings.setWindowEffect(oldEffect);
        settings.setHomeParticlesEnabled(oldParticles);
        settings.setIntroCompleted(oldIntro);
        settings.setSpatialModeEnabled(oldSpatial);
        qApp->setStyle(QStringLiteral("Fusion"));
    });
    if (QGuiApplication::platformName() == QLatin1String("cocoa")) {
        for (const auto& name : QStyleFactory::keys()) {
            if (name.contains("mac", Qt::CaseInsensitive))
                qApp->setStyle(QStyleFactory::create(name));
        }
    }
    settings.setSpatialModeEnabled(false);
    settings.setThemeMode(GallerySettings::ThemeMode::Light);
    settings.setMotionMode(GallerySettings::MotionMode::Full);
    settings.setNavigationStyle(GallerySettings::NavigationStyle::Left);
    settings.setHomeParticlesEnabled(false);
    settings.setIntroCompleted(true);
    GalleryWindow window;
    auto* presenter = window.findChild<GalleryContentPresenter*>();
    presenter->setPrewarmPaused(true);
    presenter->prewarmFinished();
    window.resize(1200, 850);
    window.show();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !window.findChild<QWidget*>("gallerySplashScreen"); }, 60000));
    auto* controller = window.findChild<GallerySpatialController*>();
    ASSERT_NE(controller, nullptr);
    const QString output = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
    ASSERT_FALSE(output.isEmpty());
    QDir().mkpath(output);
    QJsonArray measurements;
    const auto capture = [&](const QString& name) {
        if (auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
            surface && surface->property("presenting").toBool())
            surface->grabFramebuffer().save(output + "/" + name + ".png");
        else
            window.grab().save(output + "/" + name + ".png");
    };
    for (const bool spatial : {false, true}) {
        settings.setSpatialModeEnabled(spatial);
        if (spatial) {
            ASSERT_TRUE(QTest::qWaitFor(
                [&] {
                    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
                    return surface && surface->property("presenting").toBool() && !controller->transitionRunning();
                },
                10000));
        }
        const QString mode = spatial ? QStringLiteral("3d") : QStringLiteral("2d");
        for (const auto& route : {QStringLiteral("data-grid"), QStringLiteral("data-grid-tool-bar")}) {
            ASSERT_TRUE(window.selectRoute(route));
            ASSERT_TRUE(QTest::qWaitFor(
                [&] {
                    const auto* page = qobject_cast<GalleryComponentPage*>(window.currentContentPage());
                    return page && page->routeId() == route && !page->hasPendingSamples();
                },
                10000));
            auto* page = window.currentContentPage();
            auto blocks = page->findChildren<GalleryCodeBlock*>();
            ASSERT_FALSE(blocks.isEmpty());
            auto* block = blocks.first();
            if (qEnvironmentVariableIsSet("FLUENT_QT_THEME_CODE_FULL")) {
                auto* detail = block->findChild<navigation::SelectorBar*>("galleryCodeBlockSourceSelector");
                ASSERT_NE(detail, nullptr);
                detail->setSelectedIndex(1);
            }
            auto* scroll = page->findChild<QScrollArea*>();
            ASSERT_NE(scroll, nullptr);
            for (const auto theme : {GallerySettings::ThemeMode::Light, GallerySettings::ThemeMode::Dark}) {
                ASSERT_TRUE(window.selectRoute(QStringLiteral("settings")));
                ASSERT_TRUE(QTest::qWaitFor([&] { return window.currentSettingsPage() != nullptr; }, 10000));
                auto* choice = window.currentSettingsPage()->findChild<QComboBox*>("gallerySettingsThemeChoice");
                ASSERT_NE(choice, nullptr);
                choice->setCurrentIndex(static_cast<int>(theme));
                ASSERT_TRUE(QTest::qWaitFor([&] { return settings.themeMode() == theme; }, 3000));
                QTest::qWait(200);
                capture(mode + "-settings-" + QString::number(int(theme)));
                ASSERT_TRUE(window.selectRoute(route));
                ASSERT_TRUE(QTest::qWaitFor([&] { return window.currentContentPage() == page; }, 10000));
                QTest::qWait(250);
                const QString scene = mode + "-" + route + "-" + QString::number(int(theme));
                scroll->verticalScrollBar()->setValue(0);
                QTest::qWait(50);
                capture(scene + "-collapsed");
                block->setExpanded(false, false);
                scroll->ensureWidgetVisible(block->headerButton(), 0, 40);
                scroll->verticalScrollBar()->setValue(block->mapTo(scroll->widget(), QPoint()).y() - 80);
                QTest::qWait(100);
                const auto before = controller->renderingStatistics();
                QElapsedTimer clock, heartbeat;
                qint64 longestGap = 0;
                QTimer pulse;
                pulse.setInterval(8);
                QObject::connect(&pulse, &QTimer::timeout, &window, [&] {
                    longestGap = qMax(longestGap, heartbeat.restart());
                });
                heartbeat.start();
                pulse.start();
                QSignalSpy finished(block, &layout::Expander::expansionTransitionFinished);
                clock.start();
                block->headerButton()->click();
                const qint64 handlerMs = clock.elapsed();
                ASSERT_TRUE(QTest::qWaitFor([&] { return !finished.isEmpty(); }, 10000));
                const qint64 transitionMs = clock.elapsed();
                QTest::qWait(200);
                pulse.stop();
                auto stats = controller->renderingStatistics();
                for (auto it = stats.begin(); it != stats.end(); ++it) {
                    if (it.key().endsWith("Captures") || it.key().endsWith("CaptureMs") || it.key() == "paints"
                        || it.key() == "paintMs")
                        it.value() = it.value().toDouble() - before.value(it.key()).toDouble();
                }
                stats["scene"] = scene;
                stats["sourceDetail"] = qEnvironmentVariableIsSet("FLUENT_QT_THEME_CODE_FULL") ? "full" : "key";
                stats["handlerMs"] = handlerMs;
                stats["transitionMs"] = transitionMs;
                stats["longestEventGapMs"] = longestGap;
                stats["codeCharacters"] = block->code().size();
                stats["expandedHeight"] = block->height();
                stats["dpr"] = window.devicePixelRatioF();
                measurements.append(QJsonObject::fromVariantMap(stats));
                capture(scene + "-expanded");
                block->setExpanded(false, false);
            }
        }
    }
    QFile result(output + "/measurements.json");
    ASSERT_TRUE(result.open(QIODevice::WriteOnly));
    result.write(QJsonDocument(measurements).toJson());
}
