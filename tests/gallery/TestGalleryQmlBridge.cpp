#include "compatibility/private/QuickCompat_p.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsEffect>
#include <QLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>
#include <QPalette>
#include <QOpenGLWidget>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSignalSpy>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSGRendererInterface>
#include <QScrollBar>
#include <QScreen>
#include <QTest>
#include <QVBoxLayout>
#include <QXmlStreamReader>

#include "components/basicinput/Button.h"
#include "components/basicinput/ComboBox.h"
#include "components/status_info/ProgressBar.h"
#include "components/navigation/SelectorBar.h"
#include "components/qmlbridge/QmlSession.h"
#include "components/qmlbridge/QmlTextureLayer.h"
#include "components/qmlbridge/QmlView.h"
#include "components/scrolling/ScrollView.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "model/GalleryContentCatalog.h"
#include "platform/GalleryPlatform.h"
#include "view/pages/GalleryComponentPage.h"
#include "view/shell/GalleryWindow.h"
#include "view/widgets/GalleryCodeBlock.h"
#include "view/widgets/GallerySampleCard.h"
#include "view/widgets/GallerySampleCatalog.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "viewmodel/GallerySettings.h"

#ifdef FLUENT_QT_HAS_SPATIAL
#include "SpatialTestEnvironment.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/StackContentHost.h"
#include "view/shell/GallerySpatialController.h"
#include "view/support/GalleryDepth.h"
#endif

namespace {
using fluent::basicinput::Button;
using fluent::status_info::ProgressBar;
using fluent::gallery::GalleryCodeLanguage;
using fluent::gallery::GalleryComponentPage;
using fluent::gallery::GalleryComponentPageOptions;
using fluent::gallery::GalleryNavigationViewModel;
using fluent::gallery::GallerySampleCard;
using fluent::gallery::GallerySettings;
using fluent::gallery::GalleryWindow;
using fluent::qmlbridge::QmlSession;
using fluent::qmlbridge::QmlView;
using fluent::scrolling::ScrollView;
using fluent::textfields::Label;
using fluent::textfields::LineEdit;

constexpr char kDirectComposition[] = "fluentqtDirectCompositionRequired";
const QUrl kPreviewSource(QStringLiteral("qrc:/app/qml-bridge/Basic.qml"));

QRect rectIn(const QWidget* widget, QWidget* ancestor)
{
    return QRect(widget->mapTo(ancestor, QPoint()), widget->size());
}

const QStringList kSampleIds{
    QStringLiteral("qml-view-load"),      QStringLiteral("qml-view-theme"),
    QStringLiteral("qml-view-context"),   QStringLiteral("qml-view-action"),
    QStringLiteral("qml-view-lifecycle"), QStringLiteral("qml-view-error")};
const QStringList kViewNames{
    QStringLiteral("qmlBridgeLoadView"),      QStringLiteral("qmlBridgeThemeView"),
    QStringLiteral("qmlBridgeContextView"),   QStringLiteral("qmlBridgeActionView"),
    QStringLiteral("qmlBridgeLifecycleView"), QStringLiteral("qmlBridgeErrorView")};

GallerySampleCard* qmlCard(GalleryComponentPage* page,
                           const QString& sampleId = QStringLiteral("qml-view-action"))
{
    for (auto* card : page->sampleCards())
        if (card->sampleId() == sampleId)
            return card;
    return nullptr;
}

testing::AssertionResult atomicSourceContract(const fluent::gallery::GallerySample& sample)
{
    const int index = kSampleIds.indexOf(sample.id);
    if (index < 0)
        return testing::AssertionFailure() << "Unknown atomic sample";
    const QStringList files{QStringLiteral("Basic.qml"),   QStringLiteral("Theme.qml"),
                            QStringLiteral("Context.qml"), QStringLiteral("Action.qml"),
                            QStringLiteral("Basic.qml"),   QStringLiteral("Basic.qml")};
    const QStringList keyTokens[]{
        QStringLiteral("activate|createView|load(source)").split(QLatin1Char('|')),
        QStringLiteral("Theme.qml|load(source)").split(QLatin1Char('|')),
        QStringLiteral("setContextObject|widgetInput|A shared title").split(QLatin1Char('|')),
        QStringLiteral("setContextObject|widgetProgress|setRange(0, 100)|setValue(40)")
            .split(QLatin1Char('|')),
        QStringLiteral("deactivate|activate|unload()|load(source)|statusChanged")
            .split(QLatin1Char('|')),
        QStringLiteral("QmlView::Error|lastError|Missing.qml|restore|load(source)")
            .split(QLatin1Char('|'))};
    const QString& key = sample.usageSnippet;
    const QString& full = sample.codeSnippet;
    if (key.trimmed().isEmpty() || key.trimmed() == full.trimmed() ||
        key.contains(QStringLiteral("int main(")) ||
        key.contains(QStringLiteral("import QtQuick")) ||
        key.contains(QStringLiteral("Rectangle {")))
        return testing::AssertionFailure() << "Key usage must be a distinct feature excerpt";
    for (const auto& token : keyTokens[index]) {
        if (!key.contains(token))
            return testing::AssertionFailure() << "Key usage omits " << token.toStdString();
    }
    if (index == 1 && key.contains(QStringLiteral("createView")))
        return testing::AssertionFailure() << "Theme Key usage must focus on live QML tokens";
    for (const auto& token :
         {QStringLiteral("#include <FluentQt/FluentQt.h>"),
          QStringLiteral("#include <FluentQt/QmlBridge.h>"), QStringLiteral("QVBoxLayout"),
          QStringLiteral("makePanel"), QStringLiteral("setContentWidget(makePanel("),
          QStringLiteral("session->activate"), QStringLiteral("session->createView"),
          QStringLiteral("new Label"), QStringLiteral("return panel;"),
          QStringLiteral("application.exec()")}) {
        if (!full.contains(token))
            return testing::AssertionFailure()
                   << "Standalone Full example omits " << token.toStdString();
    }
    if (full.count(QStringLiteral("int main(")) != 1 || full.contains(QStringLiteral("qrc:/app/")))
        return testing::AssertionFailure()
               << "Full example must have one main and independent QML files";
    const auto main = full.mid(full.indexOf(QStringLiteral("int main(")));
    const int prepared = main.indexOf(QStringLiteral("QmlSession::prepareApplication()"));
    const int application = main.indexOf(QStringLiteral("QApplication application"));
    if (prepared < 0 || application < prepared)
        return testing::AssertionFailure()
               << "Full example must prepare Quick before its application";
    for (const auto& forbidden :
         {QStringLiteral("import QtQuick"), QStringLiteral("Rectangle {"),
          QStringLiteral("QTemporaryDir"), QStringLiteral("const QByteArray qml"),
          QStringLiteral("QUrl::fromLocalFile"), QStringLiteral(".write(qml)")}) {
        if (full.contains(forbidden))
            return testing::AssertionFailure()
                   << "C++ host source must load a separate QML file: " << forbidden.toStdString();
    }
    if (sample.sourceFiles.size() != 4)
        return testing::AssertionFailure() << "Full example requires four independent source files";
    const auto& host = sample.sourceFiles[0];
    const auto& qml = sample.sourceFiles[1];
    const auto& resource = sample.sourceFiles[2];
    const auto& cmake = sample.sourceFiles[3];
    if (host.name != QStringLiteral("main.cpp") || host.language != QStringLiteral("cpp") ||
        host.code != full || host.usageSnippet != key || qml.name != files[index] ||
        qml.language != QStringLiteral("qml") || !qml.usageSnippet.isEmpty() ||
        resource.name != QStringLiteral("qml.qrc") || resource.language != QStringLiteral("xml") ||
        !resource.usageSnippet.isEmpty() || cmake.name != QStringLiteral("CMakeLists.txt") ||
        cmake.language != QStringLiteral("cmake") || !cmake.usageSnippet.isEmpty())
        return testing::AssertionFailure() << "Source file names, languages or excerpts differ";
    QFile asset(QStringLiteral(":/app/qml-bridge/") + files[index]);
    if (!asset.open(QIODevice::ReadOnly) || qml.code.toUtf8() != asset.readAll())
        return testing::AssertionFailure() << "QML source must match the canonical asset bytes";
    if (!full.contains(QStringLiteral("qrc:/") + qml.name))
        return testing::AssertionFailure() << "C++ must load the supplied QML resource";
    QXmlStreamReader xml(resource.code);
    QStringList registered;
    QString resourcePrefix;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("qresource"))
            resourcePrefix = xml.attributes().value(QStringLiteral("prefix")).toString();
        if (xml.isStartElement() && xml.name() == QLatin1String("file"))
            registered.append(xml.readElementText());
    }
    if (xml.hasError() || registered != QStringList{qml.name} ||
        resourcePrefix != QStringLiteral("/"))
        return testing::AssertionFailure()
               << "qml.qrc must register this card's QML at its loaded path";
    for (const auto& token : {QStringLiteral("CMAKE_AUTORCC ON"),
                              QStringLiteral("COMPONENTS QmlBridge"), QStringLiteral("main.cpp"),
                              QStringLiteral("qml.qrc"), QStringLiteral("FluentQt::QmlBridge")}) {
        if (!cmake.code.contains(token))
            return testing::AssertionFailure() << "CMake example omits " << token.toStdString();
    }
    const QStringList qmlTokens[]{
        QStringLiteral("Hello from QML").split(QLatin1Char('|')),
        QStringLiteral(
            "fluentTheme.accent|fluentTheme.dark|fluentTheme.text|fluentTheme.fontFamily")
            .split(QLatin1Char('|')),
        QStringLiteral("widgetInput.text|previewTitle").split(QLatin1Char('|')),
        QStringLiteral("widgetProgress.value|Math.min(100|function advance()|onClicked")
            .split(QLatin1Char('|')),
        QStringLiteral("Hello from QML").split(QLatin1Char('|')),
        QStringLiteral("Hello from QML").split(QLatin1Char('|'))};
    for (const auto& token : qmlTokens[index]) {
        if (!qml.code.contains(token))
            return testing::AssertionFailure() << "QML page omits " << token.toStdString();
    }
    if (index == 4 && (!full.contains(QStringLiteral("view->unload()")) ||
                       !full.contains(QStringLiteral("session->deactivate()"))))
        return testing::AssertionFailure() << "Full lifecycle must force a fresh scene on Reload";
    return testing::AssertionSuccess();
}

testing::AssertionResult sampleGeometry(GalleryComponentPage* page)
{
    if (page->sampleCards().size() != kSampleIds.size())
        return testing::AssertionFailure() << "Expected six atomic QmlView sample cards";
    int liveHeaders = 0;
    for (auto* label : page->findChildren<Label*>(QStringLiteral("galleryContentSectionHeader")))
        liveHeaders += label->text() == QStringLiteral("Live examples");
    if (liveHeaders != 1)
        return testing::AssertionFailure() << "Expected one Live examples heading";
    for (int index = 0; index < kSampleIds.size(); ++index) {
        auto* card = page->sampleCards()[index];
        if (card->sampleId() != kSampleIds[index])
            return testing::AssertionFailure() << "Atomic sample order differs";
        auto* panel = card->previewWidget();
        auto* surface = card->findChild<QWidget*>(QStringLiteral("gallerySampleCardPreview"));
        if (!panel || !surface || !card->codeBlock())
            return testing::AssertionFailure() << "Missing preview or source host";
        if (!surface->rect().adjusted(20, 20, -20, -20).contains(rectIn(panel, surface)) ||
            surface->height() < panel->height() + 40)
            return testing::AssertionFailure() << "Atomic preview exceeds its surface";
        auto* view = panel->findChild<QmlView*>(kViewNames[index]);
        if (!view || !view->parentWidget()->rect().contains(view->geometry()))
            return testing::AssertionFailure() << "QmlView exceeds its atomic card";
        for (auto* control : panel->findChildren<QWidget*>()) {
            if (control->objectName().startsWith(QStringLiteral("qmlBridge")) &&
                !panel->rect().contains(rectIn(control, panel)))
                return testing::AssertionFailure()
                       << "Atomic control clipped: " << control->objectName().toStdString();
        }
        auto* block = card->codeBlock();
        if (block->y() < surface->geometry().bottom() + 12 ||
            card->height() < block->geometry().bottom() + 18)
            return testing::AssertionFailure() << "Source overlaps an atomic preview";
    }
    auto* scroll = page->findChild<ScrollView*>(QStringLiteral("galleryContentScrollArea"));
    if (!scroll || !scroll->widget() || !scroll->widget()->layout())
        return testing::AssertionFailure() << "Missing scroll content";
    const int required =
        qMax(scroll->viewport()->height(), scroll->widget()->layout()->minimumSize().height());
    if (scroll->widget()->height() < required)
        return testing::AssertionFailure()
               << "Scroll content truncates its layout: actual=" << scroll->widget()->height()
               << " required=" << required;
    return testing::AssertionSuccess();
}

bool waitForStableGeometry(GalleryComponentPage* page, QmlView* view, ScrollView* scroll)
{
    QRect previousView;
    QSize previousViewport;
    QSize previousContent;
    int previousMaximum = -1;
    QElapsedTimer unchanged;
    unchanged.start();
    return QTest::qWaitFor(
        [&] {
            const QRect currentView = rectIn(view, scroll->widget());
            const QSize viewport = scroll->viewport()->size();
            const QSize content = scroll->widget()->size();
            const int maximum = scroll->verticalScrollBar()->maximum();
            if (!sampleGeometry(page) || currentView != previousView ||
                viewport != previousViewport || content != previousContent ||
                maximum != previousMaximum) {
                previousView = currentView;
                previousViewport = viewport;
                previousContent = content;
                previousMaximum = maximum;
                unchanged.restart();
                return false;
            }
            return unchanged.elapsed() >= 40;
        },
        3000);
}

class CompositionOrderProbe final : public QObject {
public:
    bool declaredBeforeQuick = false;
    bool clearedAfterQuickHidden = false;
    int declarations = 0;
    int clearances = 0;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() != QEvent::DynamicPropertyChange ||
            static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName() != kDirectComposition)
            return false;
        auto* view = qobject_cast<QmlView*>(watched);
        if (!view)
            return false;
        const auto quickWidgets = view->findChildren<QQuickWidget*>();
        if (view->property(kDirectComposition).toBool()) {
            ++declarations;
            declaredBeforeQuick = quickWidgets.isEmpty();
        } else {
            ++clearances;
            clearedAfterQuickHidden =
                std::all_of(quickWidgets.cbegin(), quickWidgets.cend(),
                            [](QQuickWidget* quick) { return quick->isHidden(); });
        }
        return false;
    }
};

class GalleryQmlBridgeTest : public testing::Test {
protected:
    static void SetUpTestSuite() { QmlSession::prepareApplication(); }
    void SetUp() override
    {
        auto& settings = GallerySettings::instance();
        m_theme = settings.themeMode();
        m_motion = settings.motionMode();
        m_spatial = settings.spatialModeEnabled();
        m_intro = settings.introCompleted();
        m_available = settings.spatialAvailable();
        m_pending = settings.spatialAvailabilityPending();
        m_reason = settings.spatialUnavailableReason();
        settings.setIntroCompleted(true);
        settings.setSpatialModeEnabled(false);
        settings.setThemeMode(GallerySettings::ThemeMode::Light);
        settings.setMotionMode(GallerySettings::MotionMode::Full);
    }
    void TearDown() override
    {
        auto& settings = GallerySettings::instance();
        settings.setSpatialModeEnabled(false);
        settings.setThemeMode(m_theme);
        settings.setMotionMode(m_motion);
        settings.setIntroCompleted(m_intro);
        if (m_pending)
            settings.beginSpatialAvailabilityCheck();
        else
            settings.setSpatialAvailability(m_available, m_reason);
        settings.setSpatialModeEnabled(m_spatial);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QApplication::processEvents();
    }

private:
    GallerySettings::ThemeMode m_theme;
    GallerySettings::MotionMode m_motion;
    bool m_spatial, m_intro, m_available, m_pending;
    QString m_reason;
};
} // namespace

TEST_F(GalleryQmlBridgeTest, Contract_NativeSourceSwitchKeepsPaintGeometryConsistent)
{
    GalleryWindow window;
    window.resize(1160, 760);
    ASSERT_TRUE(window.selectRoute(QStringLiteral("qml-view")));
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            return window.currentContentPage() &&
                   window.currentContentPage()->routeId() == QStringLiteral("qml-view") &&
                   !window.findChild<QWidget*>(QStringLiteral("gallerySplashScreen"));
        },
        60000));
    auto* page = qobject_cast<GalleryComponentPage*>(window.currentContentPage());
    ASSERT_NE(page, nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !page->hasPendingSamples(); }));
    auto* card = qmlCard(page, QStringLiteral("qml-view-load"));
    auto* view = page->findChild<QmlView*>(QStringLiteral("qmlBridgeLoadView"));
    auto* scroll = page->findChild<ScrollView*>(QStringLiteral("galleryContentScrollArea"));
    ASSERT_TRUE(card && view && scroll);
    auto* code = card->codeBlock();
    auto* selector = code->findChild<fluent::navigation::SelectorBar*>(
        QStringLiteral("galleryCodeBlockSourceSelector"));
    auto* label = code->findChild<Label*>(QStringLiteral("galleryCodeBlockText"));
    ASSERT_TRUE(selector && label);
    code->setExpanded(true, false);
    ASSERT_TRUE(QTest::qWaitFor([&] { return view->status() == QmlView::Ready; }));
    ASSERT_TRUE(waitForStableGeometry(page, view, scroll));
    const QPointer<QObject> root(view->rootObject());
    QSignalSpy expansion(code, &fluent::layout::Expander::expandedChanged);

    class PaintProbe : public QObject {
    public:
        fluent::gallery::GalleryCodeBlock* code = nullptr;
        GallerySampleCard* card = nullptr;
        Label* label = nullptr;
        QStringList failures;
        int paints = 0;
        bool eventFilter(QObject*, QEvent* event) override
        {
            if (event->type() != QEvent::Paint)
                return false;
            ++paints;
            const int needed = label->heightForWidth(label->width());
            if (label->height() < needed)
                failures.append(QStringLiteral("Code painted before height settled: %1 < %2")
                                    .arg(label->height())
                                    .arg(needed));
            if (!card->rect().contains(rectIn(code, card)))
                failures.append(QStringLiteral("Code painted outside its sample card"));
            return false;
        }
    } probe;
    probe.code = code;
    probe.card = card;
    probe.label = label;
    label->installEventFilter(&probe);
    code->installEventFilter(&probe);
    card->installEventFilter(&probe);

    const QString evidence = qEnvironmentVariable("FLUENTQT_SWITCH_EVIDENCE_DIR");
    if (!evidence.isEmpty())
        ASSERT_TRUE(QDir().mkpath(evidence));
    int frame = 0;
    for (auto theme : {GallerySettings::ThemeMode::Dark, GallerySettings::ThemeMode::Light}) {
        GallerySettings::instance().setThemeMode(theme);
        for (int width : {1160, 760}) {
            window.resize(width, 760);
            ASSERT_TRUE(waitForStableGeometry(page, view, scroll));
            scroll->scrollTo(0,
                             rectIn(selector, scroll->widget()).center().y() -
                                 scroll->viewport()->height() / 2,
                             false);
            ASSERT_TRUE(waitForStableGeometry(page, view, scroll));
            ASSERT_TRUE(scroll->viewport()->rect().contains(rectIn(selector, scroll->viewport())));
            const QRect previewRect = rectIn(view, scroll->viewport());
            const int offset = scroll->verticalScrollBar()->value();
            for (int pass = 0; pass < 12; ++pass) {
                SCOPED_TRACE(QStringLiteral("theme=%1 width=%2 pass=%3")
                                 .arg(int(theme))
                                 .arg(width)
                                 .arg(pass)
                                 .toStdString());
                const int index = (pass + 1) % 2;
                QTest::mouseClick(selector, Qt::LeftButton, Qt::NoModifier,
                                  selector->itemGeometry(index).center());
                for (int tick = 0; tick < 4; ++tick) {
                    QTest::qWait(16);
                    EXPECT_EQ(rectIn(view, scroll->viewport()), previewRect);
                    EXPECT_EQ(scroll->verticalScrollBar()->value(), offset);
                    EXPECT_TRUE(code->isExpanded());
                    EXPECT_EQ(view->rootObject(), root);
                    if (!evidence.isEmpty()) {
                        const QImage screen = window.screen()->grabWindow(window.winId()).toImage();
                        ASSERT_FALSE(screen.isNull());
                        ASSERT_TRUE(screen.save(
                            evidence +
                            QStringLiteral("/frame-%1.png").arg(frame++, 4, 10, QLatin1Char('0'))));
                    }
                }
            }
            // Also cover clicks arriving faster than the display's frame cadence.
            for (int pass = 0; pass < 32; ++pass) {
                QTest::mouseClick(selector, Qt::LeftButton, Qt::NoModifier,
                                  selector->itemGeometry((pass + 1) % 2).center());
                EXPECT_GE(label->height(), label->heightForWidth(label->width()));
                EXPECT_EQ(rectIn(view, scroll->viewport()), previewRect);
                EXPECT_EQ(scroll->verticalScrollBar()->value(), offset);
                QTest::qWait(8);
            }
        }
    }
    EXPECT_GT(probe.paints, 0);
    EXPECT_TRUE(probe.failures.isEmpty()) << probe.failures.join(QLatin1Char('\n')).toStdString();
    EXPECT_EQ(expansion.count(), 0);
}

TEST_F(GalleryQmlBridgeTest, Contract_QuickRequirementPrecedesCreationAndClearsAfterHide)
{
    QWidget host;
    auto* column = new QVBoxLayout(&host);
    const auto samples = fluent::gallery::gallerySamplesForRoute(QStringLiteral("qml-view"));
    ASSERT_EQ(samples.size(), 6);
    QmlSession session(&host);
    ASSERT_TRUE(session.activate());
    auto* view = session.createView(&host);
    ASSERT_NE(view, nullptr);
    column->addWidget(view);
    CompositionOrderProbe probe;
    view->installEventFilter(&probe);
    EXPECT_FALSE(view->property(kDirectComposition).toBool());
    ASSERT_TRUE(view->load(kPreviewSource));
    ASSERT_EQ(view->status(), QmlView::Ready);
    EXPECT_EQ(probe.declarations, 1);
    EXPECT_TRUE(probe.declaredBeforeQuick);
    host.resize(760, 680);
    host.show();
    ASSERT_TRUE(QTest::qWaitFor([&] { return view->isVisible(); }));
    const QPointer<QObject> root(view->rootObject());
    ASSERT_FALSE(root.isNull());
    view->unload();
    EXPECT_FALSE(view->property(kDirectComposition).toBool());
    EXPECT_EQ(view->rootObject(), nullptr);
    EXPECT_EQ(probe.clearances, 1);
    EXPECT_TRUE(probe.clearedAfterQuickHidden);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(root.isNull());
    ASSERT_TRUE(view->load(kPreviewSource));
    ASSERT_EQ(view->status(), QmlView::Ready);
    const int clearances = probe.clearances;
    delete view;
    EXPECT_EQ(probe.clearances, clearances + 1);
    EXPECT_TRUE(probe.clearedAfterQuickHidden);
}

TEST_F(GalleryQmlBridgeTest, Contract_NativeQmlPageKeepsCppOnlyPresentation)
{
    EXPECT_FALSE(fluent::gallery::platform::capabilities().showsBilingualDocumentation);
    GalleryWindow window;
    ASSERT_TRUE(window.selectRoute(QStringLiteral("qml-view")));
    window.show();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        auto* current = qobject_cast<GalleryComponentPage*>(window.currentContentPage());
        return current && current->routeId() == QStringLiteral("qml-view") &&
               !current->hasPendingSamples();
    }));
    auto* page = qobject_cast<GalleryComponentPage*>(window.currentContentPage());
    ASSERT_NE(page, nullptr);
    EXPECT_FALSE(page->bilingualDocumentationEnabled());
    ASSERT_EQ(page->sampleCards().size(), 6);
    for (auto* card : page->sampleCards()) {
        auto* code = card->codeBlock();
        ASSERT_NE(code, nullptr);
        EXPECT_EQ(code->languageSelector(), nullptr);
        EXPECT_FALSE(code->hasPythonCode());
        EXPECT_EQ(code->codeLanguage(), GalleryCodeLanguage::Cpp);
        const auto files = code->sourceFileBundle();
        ASSERT_EQ(files.size(), 4);
        EXPECT_EQ(files[0].name, QStringLiteral("main.cpp"));
        EXPECT_EQ(files[0].language, QStringLiteral("cpp"));
        EXPECT_EQ(files[1].language, QStringLiteral("qml"));
    }
}

TEST_F(GalleryQmlBridgeTest, Contract_AtomicSamplesKeepIndependentState)
{
    GalleryNavigationViewModel navigation;
    const auto* entry = fluent::gallery::galleryContentEntry(QStringLiteral("qml-view"));
    ASSERT_NE(entry, nullptr);
    GalleryComponentPage page(*entry, navigation, GalleryComponentPageOptions{true, false});
    page.resize(1160, 760);
    page.show();
    auto* context = page.findChild<QmlView*>(QStringLiteral("qmlBridgeContextView"));
    auto* action = page.findChild<QmlView*>(QStringLiteral("qmlBridgeActionView"));
    auto* theme = page.findChild<QmlView*>(QStringLiteral("qmlBridgeThemeView"));
    auto* lifecycle = page.findChild<QmlView*>(QStringLiteral("qmlBridgeLifecycleView"));
    auto* error = page.findChild<QmlView*>(QStringLiteral("qmlBridgeErrorView"));
    auto* title = page.findChild<LineEdit*>(QStringLiteral("qmlBridgeTitleEdit"));
    auto* progress = page.findChild<ProgressBar*>(QStringLiteral("qmlBridgeProgressIndicator"));
    auto* deactivate = page.findChild<Button*>(QStringLiteral("qmlBridgeDeactivate"));
    auto* reload = page.findChild<Button*>(QStringLiteral("qmlBridgeReload"));
    auto* missing = page.findChild<Button*>(QStringLiteral("qmlBridgeLoadMissing"));
    auto* restore = page.findChild<Button*>(QStringLiteral("qmlBridgeRestore"));
    auto* scroll = page.findChild<ScrollView*>(QStringLiteral("galleryContentScrollArea"));
    ASSERT_TRUE(context && action && theme && lifecycle && error && title && progress &&
                deactivate && reload && missing && restore && scroll);
    const auto views = page.findChildren<QmlView*>();
    ASSERT_EQ(views.size(), 6);
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return std::all_of(views.cbegin(), views.cend(),
                           [](QmlView* view) { return view->status() == QmlView::Ready; });
    }));
    EXPECT_EQ(title->text(), QStringLiteral("A shared title"));
    EXPECT_DOUBLE_EQ(progress->value(), 40);
    const QPointer<QObject> contextRoot(context->rootObject());
    const QPointer<QObject> actionRoot(action->rootObject());
    for (int width : {1160, 620}) {
        SCOPED_TRACE(width);
        page.resize(width, 760);
        ASSERT_TRUE(QTest::qWaitFor([&] { return bool(sampleGeometry(&page)); }));
        scroll->ensureWidgetVisible(title);
        QTest::mouseClick(title, Qt::LeftButton);
        title->selectAll();
        QTest::keyClicks(title, "Edited in Widgets");
        EXPECT_EQ(context->rootObject()->property("previewTitle").toString(), title->text());
        const double beforeAdvance = progress->value();
        ASSERT_TRUE(
            QMetaObject::invokeMethod(action->rootObject(), "advance", Qt::DirectConnection));
        EXPECT_DOUBLE_EQ(progress->value(), qMin(100., beforeAdvance + 10));
        EXPECT_EQ(action->rootObject()->property("previewProgress").toInt(),
                  int(progress->value()));
        EXPECT_EQ(title->text(), QStringLiteral("Edited in Widgets"));
        for (auto mode : {fluent::FluentElement::Dark, fluent::FluentElement::Light}) {
            auto* card = qmlCard(&page, QStringLiteral("qml-view-theme"));
            card->setPreviewThemeOverride(mode);
            EXPECT_EQ(theme->themeObject()->property("dark").toBool(),
                      mode == fluent::FluentElement::Dark);
            EXPECT_EQ(theme->rootObject()->property("color").value<QColor>(),
                      theme->themeObject()->property("surface").value<QColor>());
            scroll->ensureWidgetVisible(theme);
            auto* themeRoot = qobject_cast<QQuickItem*>(theme->rootObject());
            ASSERT_NE(themeRoot, nullptr);
            auto* swatch =
                themeRoot->findChild<QQuickItem*>(QStringLiteral("qmlBridgeThemeSwatch"));
            auto* caption =
                themeRoot->findChild<QQuickItem*>(QStringLiteral("qmlBridgeThemeCaption"));
            ASSERT_TRUE(swatch && caption);
            const auto center = [](QQuickItem* item) {
                return item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
            };
            const auto itemGeometry = [&](QQuickItem* item) {
                const QPointF sceneCenter = center(item);
                return QJsonObject{{QStringLiteral("class"), item->metaObject()->className()},
                                   {QStringLiteral("name"), item->objectName()},
                                   {QStringLiteral("width"), item->width()},
                                   {QStringLiteral("height"), item->height()},
                                   {QStringLiteral("x"), item->x()},
                                   {QStringLiteral("y"), item->y()},
                                   {QStringLiteral("sceneCenterX"), sceneCenter.x()},
                                   {QStringLiteral("sceneCenterY"), sceneCenter.y()}};
            };
            ASSERT_TRUE(QTest::qWaitFor([&] {
                return caption->width() > 0 && caption->height() > 0 &&
                       qAbs(center(swatch).y() - center(caption).y()) < .25;
            })) << QJsonDocument(QJsonObject{
                                     {QStringLiteral("mode"), int(mode)},
                                     {QStringLiteral("swatch"), itemGeometry(swatch)},
                                     {QStringLiteral("caption"), itemGeometry(caption)},
                                     {QStringLiteral("row"), itemGeometry(swatch->parentItem())},
                                     {QStringLiteral("root"), itemGeometry(themeRoot)},
                                     {QStringLiteral("scrollValue"),
                                      scroll->verticalScrollBar()->value()},
                                     {QStringLiteral("viewVisibleRegionEmpty"),
                                      theme->visibleRegion().isEmpty()}})
                       .toJson(QJsonDocument::Compact)
                       .toStdString();
            EXPECT_NEAR(center(swatch).y(), center(caption).y(), .25);
            EXPECT_NEAR(center(swatch).y(), themeRoot->height() / 2, .25);
            auto* nativeQuick = theme->findChild<QQuickWidget*>();
            ASSERT_NE(nativeQuick, nullptr);
            const QColor backdrop = theme->themeObject()->property("surfaceAlt").value<QColor>();
            const auto grabFrame = [nativeQuick] {
#if !FLUENT_QUICK_USES_RHI
                // Qt 5's software widget paints its own cached image; read
                // that presentation instead of reentering render-control grab.
                if (nativeQuick->quickWindow()->rendererInterface()->graphicsApi() ==
                    QSGRendererInterface::Software)
                    return nativeQuick->grab().toImage();
#endif
                return nativeQuick->grabFramebuffer();
            };
            QImage themeFrame;
            const bool backdropRendered = QTest::qWaitFor([&] {
                themeFrame = grabFrame();
                const auto& frame = themeFrame;
                return !frame.isNull() && frame.pixelColor(0, 0) == backdrop &&
                       frame.pixelColor(frame.width() - 1, frame.height() - 1) == backdrop &&
                       backdrop.alpha() == 255;
            });
            if (const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
                !dir.isEmpty()) {
                QDir().mkpath(dir);
                themeFrame.save(dir +
                                QStringLiteral("/qml-theme-%1-%2.png").arg(width).arg(int(mode)));
            }
            ASSERT_TRUE(backdropRendered)
                << "The Gallery Theme preview must render its explicit LayerAlt backdrop";
        }
        qmlCard(&page, QStringLiteral("qml-view-theme"))->clearPreviewThemeOverride();
        const auto samples = fluent::gallery::gallerySamplesForRoute(QStringLiteral("qml-view"));
        for (int sampleIndex = 0; sampleIndex < samples.size(); ++sampleIndex) {
            const auto& sample = samples[sampleIndex];
            auto* card = page.sampleCards()[sampleIndex];
            SCOPED_TRACE(sample.id.toStdString());
            EXPECT_TRUE(atomicSourceContract(sample));
            auto* code = card->codeBlock();
            code->setExpanded(true, false);
            auto* selector = code->findChild<fluent::navigation::SelectorBar*>(
                QStringLiteral("galleryCodeBlockSourceSelector"));
            ASSERT_NE(selector, nullptr);
            auto* fileSelector = code->findChild<fluent::basicinput::ComboBox*>(
                QStringLiteral("galleryCodeBlockFileSelector"));
            ASSERT_NE(fileSelector, nullptr);
            ASSERT_EQ(fileSelector->count(), sample.sourceFiles.size());
            EXPECT_EQ(fileSelector->currentText(), QStringLiteral("main.cpp"));
            EXPECT_EQ(code->sourceLanguage(), QStringLiteral("cpp"));
            EXPECT_FALSE(selector->isHidden());
            selector->setSelectedIndex(0);
            EXPECT_EQ(code->code(), sample.usageSnippet);
            QApplication::clipboard()->clear();
            code->copyButton()->click();
            ASSERT_TRUE(QTest::qWaitFor(
                [&] { return QApplication::clipboard()->text() == sample.usageSnippet; }));
            selector->setSelectedIndex(1);
            EXPECT_EQ(code->code(), sample.codeSnippet);
            QApplication::clipboard()->clear();
            code->copyButton()->click();
            ASSERT_TRUE(QTest::qWaitFor(
                [&] { return QApplication::clipboard()->text() == sample.codeSnippet; }));
            for (int fileIndex = 1; fileIndex < sample.sourceFiles.size(); ++fileIndex) {
                const auto& source = sample.sourceFiles[fileIndex];
                EXPECT_EQ(fileSelector->itemText(fileIndex), source.name);
                fileSelector->setCurrentIndex(fileIndex);
                EXPECT_TRUE(selector->isHidden());
                EXPECT_EQ(code->sourceLanguage(), source.language);
                EXPECT_EQ(code->code(), source.code);
                QApplication::clipboard()->clear();
                code->copyButton()->click();
                ASSERT_TRUE(QTest::qWaitFor(
                    [&] { return QApplication::clipboard()->text() == source.code; }));
            }
            fileSelector->setCurrentIndex(0);
            EXPECT_FALSE(selector->isHidden());
            EXPECT_EQ(selector->selectedIndex(), 1);
            EXPECT_EQ(code->code(), sample.codeSnippet);
            ASSERT_TRUE(code->hasPythonCode());
            ASSERT_NE(code->languageSelector(), nullptr);
            if (code->hasPythonCode()) {
                code->setCodeLanguage(GalleryCodeLanguage::Python);
                EXPECT_EQ(fileSelector->currentText(), QStringLiteral("main.py"));
                EXPECT_EQ(fileSelector->count(), 2);
                EXPECT_EQ(code->sourceLanguage(), QStringLiteral("python"));
                const auto pythonFiles = code->sourceFileBundle();
                ASSERT_EQ(pythonFiles.size(), 2);
                EXPECT_EQ(code->code(), pythonFiles[0].code);
                selector->setSelectedIndex(0);
                EXPECT_EQ(code->code(), code->pythonCode());
                EXPECT_NE(code->code(), pythonFiles[0].code);
                selector->setSelectedIndex(1);
                QApplication::clipboard()->clear();
                code->copyButton()->click();
                ASSERT_TRUE(QTest::qWaitFor(
                    [&] { return QApplication::clipboard()->text() == pythonFiles[0].code; }));
                EXPECT_FALSE(code->code().contains(QStringLiteral("import QtQuick")));
                EXPECT_FALSE(selector->isHidden());
                fileSelector->setCurrentIndex(1);
                EXPECT_EQ(fileSelector->currentText(), sample.sourceFiles[1].name);
                EXPECT_EQ(code->code(), sample.sourceFiles[1].code);
                EXPECT_EQ(code->sourceLanguage(), QStringLiteral("qml"));
                EXPECT_TRUE(selector->isHidden());
                QApplication::clipboard()->clear();
                code->copyButton()->click();
                ASSERT_TRUE(QTest::qWaitFor([&] {
                    return QApplication::clipboard()->text() == sample.sourceFiles[1].code;
                }));
                code->setCodeLanguage(GalleryCodeLanguage::Cpp);
                EXPECT_EQ(fileSelector->currentText(), QStringLiteral("main.cpp"));
                EXPECT_EQ(code->sourceLanguage(), QStringLiteral("cpp"));
                EXPECT_EQ(code->code(), sample.codeSnippet);
                EXPECT_FALSE(selector->isHidden());
            }
            selector->setSelectedIndex(0);
            EXPECT_EQ(code->code(), sample.usageSnippet);
            code->setExpanded(false, false);
        }
        scroll->ensureWidgetVisible(deactivate);
        const QPointer<QObject> retiredRoot(lifecycle->rootObject());
        const QPointer<QQuickWidget> retiredQuick(lifecycle->findChild<QQuickWidget*>());
        ASSERT_FALSE(retiredQuick.isNull());
        const QPointer<QQmlEngine> retiredEngine(retiredQuick->engine());
        QTest::mouseClick(deactivate, Qt::LeftButton);
        EXPECT_EQ(lifecycle->status(), QmlView::Unloaded);
        EXPECT_EQ(lifecycle->rootObject(), nullptr);
        EXPECT_EQ(lifecycle->textureId(), 0u);
        EXPECT_TRUE(retiredQuick.isNull() || retiredQuick->isHidden());
        ASSERT_TRUE(QTest::qWaitFor(
            [&] {
                QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                return retiredQuick.isNull() && retiredRoot.isNull() && retiredEngine.isNull() &&
                       page.findChildren<QQuickWidget*>().size() == 5;
            },
            1000));
        EXPECT_EQ(page.findChildren<QQuickWidget*>().size(), 5);
        EXPECT_EQ(context->rootObject(), contextRoot);
        EXPECT_EQ(action->rootObject(), actionRoot);
        QTest::mouseClick(reload, Qt::LeftButton);
        ASSERT_TRUE(QTest::qWaitFor([&] { return lifecycle->status() == QmlView::Ready; }));
        const QPointer<QObject> readyRoot(lifecycle->rootObject());
        const QPointer<QQuickWidget> readyQuick(lifecycle->findChild<QQuickWidget*>());
        ASSERT_FALSE(readyQuick.isNull());
        const QPointer<QQmlEngine> readyEngine(readyQuick->engine());
        struct SceneSnapshot {
            QmlView* view;
            QPointer<QObject> root;
            QPointer<QQmlEngine> engine;
        };
        QVector<SceneSnapshot> unchanged;
        for (auto* other : views) {
            if (other != lifecycle) {
                auto* otherQuick = other->findChild<QQuickWidget*>();
                ASSERT_NE(otherQuick, nullptr);
                unchanged.append({other, other->rootObject(), otherQuick->engine()});
            }
        }
        QTest::mouseClick(reload, Qt::LeftButton);
        ASSERT_TRUE(QTest::qWaitFor([&] { return lifecycle->status() == QmlView::Ready; }));
        EXPECT_NE(lifecycle->rootObject(), readyRoot);
        ASSERT_TRUE(QTest::qWaitFor(
            [&] {
                QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                return readyRoot.isNull() && readyQuick.isNull() && readyEngine.isNull();
            },
            1000));
        EXPECT_EQ(page.findChildren<QQuickWidget*>().size(), 6);
        for (const auto& original : unchanged) {
            EXPECT_EQ(original.view->rootObject(), original.root);
            auto* originalQuick = original.view->findChild<QQuickWidget*>();
            ASSERT_NE(originalQuick, nullptr);
            EXPECT_EQ(originalQuick->engine(), original.engine);
        }
        scroll->ensureWidgetVisible(missing);
        QTest::mouseClick(missing, Qt::LeftButton);
        EXPECT_EQ(error->status(), QmlView::Error);
        EXPECT_FALSE(error->lastError().isEmpty());
        EXPECT_EQ(error->rootObject(), nullptr);
        EXPECT_EQ(context->rootObject(), contextRoot);
        EXPECT_EQ(action->rootObject(), actionRoot);
        QTest::mouseClick(restore, Qt::LeftButton);
        ASSERT_TRUE(QTest::qWaitFor([&] { return error->status() == QmlView::Ready; }));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        EXPECT_EQ(page.findChildren<QQuickWidget*>().size(), 6);
        EXPECT_TRUE(sampleGeometry(&page));
    }
}

TEST_F(GalleryQmlBridgeTest, Contract_AtomicSourcesStayDistinct)
{
    const auto samples = fluent::gallery::gallerySamplesForRoute(QStringLiteral("qml-view"));
    ASSERT_EQ(samples.size(), kSampleIds.size());
    for (int index = 0; index < samples.size(); ++index) {
        const auto& sample = samples[index];
        SCOPED_TRACE(sample.id.toStdString());
        EXPECT_EQ(sample.id, kSampleIds[index]);
        EXPECT_TRUE(atomicSourceContract(sample));
        ::testing::Test::RecordProperty((sample.id + QStringLiteral("-full")).toStdString(),
                                        sample.codeSnippet.toStdString());
        ::testing::Test::RecordProperty((sample.id + QStringLiteral("-key")).toStdString(),
                                        sample.usageSnippet.toStdString());
        QJsonArray files;
        for (const auto& source : sample.sourceFiles) {
            files.append(QJsonObject{{QStringLiteral("name"), source.name},
                                     {QStringLiteral("language"), source.language},
                                     {QStringLiteral("code"), source.code},
                                     {QStringLiteral("usage_snippet"), source.usageSnippet}});
        }
        ::testing::Test::RecordProperty(
            (sample.id + QStringLiteral("-files")).toStdString(),
            QJsonDocument(files).toJson(QJsonDocument::Compact).toStdString());
    }
}

TEST_F(GalleryQmlBridgeTest, Contract_NativeCornersFollowPreviewTheme)
{
    auto& settings = GallerySettings::instance();
    settings.setSpatialModeEnabled(false);
    settings.setThemeMode(GallerySettings::ThemeMode::Dark);
    GalleryWindow window;
    window.resize(1160, 760);
    ASSERT_TRUE(window.selectRoute(QStringLiteral("qml-view")));
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            auto* current = window.currentContentPage();
            return current && current->routeId() == QStringLiteral("qml-view") &&
                   !window.findChild<QWidget*>(QStringLiteral("gallerySplashScreen"));
        },
        60000));
    auto* page = qobject_cast<GalleryComponentPage*>(window.currentContentPage());
    ASSERT_NE(page, nullptr);
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return page->findChildren<QmlView*>().size() == 6 && !page->hasPendingSamples(); }));
    auto* load = page->findChild<QmlView*>(QStringLiteral("qmlBridgeLoadView"));
    auto* theme = page->findChild<QmlView*>(QStringLiteral("qmlBridgeThemeView"));
    auto* scroll = page->findChild<ScrollView*>(QStringLiteral("galleryContentScrollArea"));
    auto* loadCard = qmlCard(page, QStringLiteral("qml-view-load"));
    auto* themeCard = qmlCard(page, QStringLiteral("qml-view-theme"));
    auto* themeButton = page->findChild<Button*>(QStringLiteral("galleryComponentPageThemeButton"));
    ASSERT_TRUE(load && theme && scroll && loadCard && themeCard && themeButton);
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return load->status() == QmlView::Ready && theme->status() == QmlView::Ready; }));
    const QPointer<QObject> loadRoot(load->rootObject());
    const QPointer<QObject> themeRoot(theme->rootObject());
    const QString evidence =
        QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath())
            .absoluteFilePath(QStringLiteral("../../build/qmlview-corner-validation/")) +
        QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    ASSERT_TRUE(QDir().mkpath(evidence));
    SCOPED_TRACE(evidence.toStdString());
    const auto rgbDifference = [](const QColor& a, const QColor& b) {
        return qMax(qAbs(a.red() - b.red()),
                    qMax(qAbs(a.green() - b.green()), qAbs(a.blue() - b.blue())));
    };
    const auto colorName = [](const QColor& color) { return color.name(QColor::HexArgb); };
    const auto capture = [&](GallerySampleCard* card, QmlView* view, const QString& stem) {
        SCOPED_TRACE(stem.toStdString());
        if (auto* focused = QApplication::focusWidget())
            focused->clearFocus();
        ASSERT_TRUE(waitForStableGeometry(page, view, scroll));
        const QRect target = rectIn(view, scroll->widget());
        const int targetY = qBound(scroll->verticalScrollBar()->minimum(),
                                   target.center().y() - scroll->viewport()->height() / 2,
                                   scroll->verticalScrollBar()->maximum());
        scroll->scrollTo(0, targetY, false);
        QRect previousVisible;
        int previousOffset = -1;
        QElapsedTimer revealed;
        revealed.start();
        ASSERT_TRUE(QTest::qWaitFor(
            [&] {
                const QRect visible = rectIn(view, scroll->viewport());
                const int offset = scroll->verticalScrollBar()->value();
                if (visible != previousVisible || offset != previousOffset) {
                    previousVisible = visible;
                    previousOffset = offset;
                    revealed.restart();
                }
                return offset == targetY && scroll->viewport()->rect().contains(visible) &&
                       revealed.elapsed() >= 60;
            },
            3000))
            << "The entire preview and all four corners must remain revealed at the requested "
               "offset";
        auto* quick = view->findChild<QQuickWidget*>();
        ASSERT_NE(quick, nullptr);
        ASSERT_FALSE(quick->isHidden());
        ASSERT_EQ(quick->quickWindow()->rendererInterface()->graphicsApi(),
                  QSGRendererInterface::OpenGL);
        ASSERT_FALSE(view->isTextureRenderingEnabled());
        ASSERT_FALSE(settings.spatialModeEnabled());
        const QImage native = quick->grabFramebuffer();
        ASSERT_FALSE(native.isNull());
        const QPoint origin = quick->mapTo(&window, QPoint());
        const qreal quickScale = qreal(native.width()) / quick->width();
        const QPoint landmark(quick->width() / 4, quick->height() / 2);
        const QColor landmarkColor = view->rootObject()->property("color").value<QColor>();
        ASSERT_LE(rgbDifference(native.pixelColor(landmark * quickScale), landmarkColor), 1);
        ASSERT_NE(window.screen(), nullptr);
        // QWidget/page readiness can precede the native splash-to-page frame.
        // Wait only for a blank interior landmark; corner colors never gate
        // readiness. Capture before the separate QWidget::grab diagnostic.
        window.raise();
        window.activateWindow();
        window.repaint();
        QCoreApplication::processEvents();
        QImage screen;
        QPoint screenLandmark;
        const bool pagePresented = QTest::qWaitFor(
            [&] {
                if (scroll->verticalScrollBar()->value() != targetY ||
                    rectIn(view, scroll->viewport()) != previousVisible)
                    return false;
                screen = window.screen()->grabWindow(window.winId()).toImage();
                if (screen.isNull())
                    return false;
                screenLandmark = (origin + landmark) * (qreal(screen.width()) / window.width());
                return screen.rect().contains(screenLandmark) &&
                       rgbDifference(screen.pixelColor(screenLandmark), landmarkColor) <= 3;
            },
            3000);
        const QImage painted = window.grab().toImage();
        ASSERT_FALSE(painted.isNull());
        ASSERT_TRUE(native.save(evidence + QLatin1Char('/') + stem + QStringLiteral("-quick.png")));
        ASSERT_TRUE(
            painted.save(evidence + QLatin1Char('/') + stem + QStringLiteral("-widget.png")));
        if (!screen.isNull())
            ASSERT_TRUE(
                screen.save(evidence + QLatin1Char('/') + stem + QStringLiteral("-screen.png")));
        auto* surface = card->findChild<QWidget*>(QStringLiteral("gallerySampleCardPreview"));
        auto* element = dynamic_cast<fluent::FluentElement*>(surface);
        ASSERT_NE(element, nullptr);
        const QColor hostColor = element->themeColorsRef().bgLayerAlt;
        QJsonArray ancestors;
        for (auto* ancestor = static_cast<QWidget*>(quick); ancestor;
             ancestor = ancestor->parentWidget()) {
            ancestors.append(QJsonObject{
                {QStringLiteral("class"), ancestor->metaObject()->className()},
                {QStringLiteral("name"), ancestor->objectName()},
                {QStringLiteral("windowColor"),
                 colorName(ancestor->palette().color(QPalette::Window))},
                {QStringLiteral("baseColor"), colorName(ancestor->palette().color(QPalette::Base))},
                {QStringLiteral("surfaceColor"),
                 colorName(ancestor->property("fluentSurfaceColor").value<QColor>())},
                {QStringLiteral("autoFillBackground"), ancestor->autoFillBackground()},
                {QStringLiteral("opaquePaintEvent"),
                 ancestor->testAttribute(Qt::WA_OpaquePaintEvent)},
                {QStringLiteral("noSystemBackground"),
                 ancestor->testAttribute(Qt::WA_NoSystemBackground)},
                {QStringLiteral("translucentBackground"),
                 ancestor->testAttribute(Qt::WA_TranslucentBackground)},
                {QStringLiteral("nativeWindow"), ancestor->testAttribute(Qt::WA_NativeWindow)},
                {QStringLiteral("alwaysStackOnTop"),
                 ancestor->testAttribute(Qt::WA_AlwaysStackOnTop)}});
        }
        QJsonObject state{
            {QStringLiteral("globalTheme"), QStringLiteral("Dark")},
            {QStringLiteral("localThemeDark"), view->themeObject()->property("dark").toBool()},
            {QStringLiteral("qmlRootColor"),
             colorName(view->rootObject()->property("color").value<QColor>())},
            {QStringLiteral("hostLayerAlt"), colorName(hostColor)},
            {QStringLiteral("ancestors"), ancestors},
            {QStringLiteral("sourceExpanded"), loadCard->codeBlock()->isExpanded()},
            {QStringLiteral("sourceFull"),
             loadCard->codeBlock()->code().contains(QStringLiteral("int main("))},
            {QStringLiteral("screenCaptureAvailable"), !screen.isNull()},
            {QStringLiteral("pagePresented"), pagePresented},
            {QStringLiteral("landmarkExpected"), colorName(landmarkColor)},
            {QStringLiteral("landmarkActual"),
             !screen.isNull() && screen.rect().contains(screenLandmark)
                 ? colorName(screen.pixelColor(screenLandmark))
                 : QStringLiteral("unavailable")},
            {QStringLiteral("captureScope"),
             QStringLiteral("own native window compositor; widget and Quick images are separate "
                            "diagnostics")}};
        state[QStringLiteral("windowWidth")] = window.width();
        state[QStringLiteral("windowHeight")] = window.height();
        state[QStringLiteral("quickWindowX")] = origin.x();
        state[QStringLiteral("quickWindowY")] = origin.y();
        state[QStringLiteral("quickScale")] = quickScale;
        const auto pixels = [&](const QImage& image, const QString& scope) {
            SCOPED_TRACE(scope.toStdString());
            QJsonArray samples;
            if (image.isNull())
                return samples;
            const qreal scale = qreal(image.width()) / window.width();
            EXPECT_NEAR(qreal(image.height()) / window.height(), scale, .01);
            for (int inset : {0, 1}) {
                for (bool right : {false, true}) {
                    for (bool bottom : {false, true}) {
                        const QPoint corner(right ? native.width() - inset - 1 : inset,
                                            bottom ? native.height() - inset - 1 : inset);
                        const QPoint point = origin * scale + corner * (scale / quickScale);
                        const QPoint outside =
                            origin * scale + QPoint(right ? qRound(quick->width() * scale) + 1 : -2,
                                                    qRound(corner.y() * scale / quickScale));
                        EXPECT_TRUE(image.rect().contains(point));
                        EXPECT_TRUE(image.rect().contains(outside));
                        if (!image.rect().contains(point) || !image.rect().contains(outside))
                            continue;
                        const QColor rgba = native.pixelColor(corner);
                        const QColor host = image.pixelColor(outside);
                        const QColor actual = image.pixelColor(point);
                        const qreal alpha = rgba.alphaF();
                        const QColor expected(
                            qRound(rgba.red() * alpha + host.red() * (1 - alpha)),
                            qRound(rgba.green() * alpha + host.green() * (1 - alpha)),
                            qRound(rgba.blue() * alpha + host.blue() * (1 - alpha)));
                        samples.append(
                            QJsonObject{{QStringLiteral("nativeX"), corner.x()},
                                        {QStringLiteral("nativeY"), corner.y()},
                                        {QStringLiteral("windowX"), point.x()},
                                        {QStringLiteral("windowY"), point.y()},
                                        {QStringLiteral("neighborX"), outside.x()},
                                        {QStringLiteral("neighborY"), outside.y()},
                                        {QStringLiteral("quickRgba"), colorName(rgba)},
                                        {QStringLiteral("neighborRgb"), colorName(host)},
                                        {QStringLiteral("actualRgb"), colorName(actual)},
                                        {QStringLiteral("expectedRgb"), colorName(expected)}});
                        EXPECT_EQ(rgba.alpha(), 255);
                        if (inset == 0)
                            EXPECT_LE(rgbDifference(rgba, hostColor), 1)
                                << "The QML sample must explicitly render LayerAlt behind its "
                                   "rounded surface";
                        EXPECT_LE(rgbDifference(host, hostColor), 3)
                            << "The neighboring native preview surface must use LayerAlt";
                        EXPECT_LE(rgbDifference(actual, expected), 3)
                            << "The rounded corner must composite over its local preview surface";
                    }
                }
            }
            return samples;
        };
        state[QStringLiteral("screenPixels")] = pixels(screen, QStringLiteral("native compositor"));
        state[QStringLiteral("widgetPixels")] =
            pixels(painted, QStringLiteral("QWidget diagnostic"));
        QFile json(evidence + QLatin1Char('/') + stem + QStringLiteral(".json"));
        ASSERT_TRUE(json.open(QIODevice::WriteOnly));
        ASSERT_GT(json.write(QJsonDocument(state).toJson()), 0);
        ASSERT_TRUE(pagePresented)
            << "The native compositor must present the current page before sampling corners";
        ASSERT_FALSE(screen.isNull())
            << "Cannot capture the native window compositor; diagnostic grabs cannot replace it";
    };
    QTest::qWait(400); // Let the existing first-show scroll restoration complete.
    auto* source = loadCard->codeBlock();
    ASSERT_NE(source, nullptr);
    source->setExpanded(true, false);
    auto* selector = source->findChild<fluent::navigation::SelectorBar*>(
        QStringLiteral("galleryCodeBlockSourceSelector"));
    ASSERT_NE(selector, nullptr);
    selector->setSelectedIndex(1);
    ASSERT_TRUE(source->code().contains(QStringLiteral("int main(")));
    ASSERT_FALSE(source->code().contains(QStringLiteral("import QtQuick")));
    const auto clickTheme = [&] {
        if (auto* focused = QApplication::focusWidget())
            focused->clearFocus();
        scroll->scrollTo(0, scroll->verticalScrollBar()->minimum(), false);
        if (!QTest::qWaitFor(
                [&] {
                    return scroll->verticalScrollBar()->value() ==
                               scroll->verticalScrollBar()->minimum() &&
                           themeButton->isVisible() &&
                           scroll->viewport()->rect().contains(
                               rectIn(themeButton, scroll->viewport())) &&
                           window.rect().contains(rectIn(themeButton, &window));
                },
                1000))
            return false;
        QTest::mouseClick(themeButton, Qt::LeftButton);
        return true;
    };
    for (int width : {1160, 620}) {
        window.resize(width, 760);
        ASSERT_TRUE(waitForStableGeometry(page, load, scroll));
        if (themeButton->property("gallerySampleTheme").toString() != QStringLiteral("Dark"))
            ASSERT_TRUE(clickTheme());
        ASSERT_EQ(themeButton->property("gallerySampleTheme").toString(), QStringLiteral("Dark"));
        int pass = 0;
        for (auto mode : {fluent::FluentElement::Light, fluent::FluentElement::Dark,
                          fluent::FluentElement::Light}) {
            ASSERT_TRUE(clickTheme());
            ASSERT_EQ(themeButton->property("gallerySampleTheme").toString(),
                      mode == fluent::FluentElement::Dark ? QStringLiteral("Dark")
                                                          : QStringLiteral("Light"));
            ASSERT_EQ(load->themeObject()->property("dark").toBool(),
                      mode == fluent::FluentElement::Dark);
            ASSERT_EQ(theme->themeObject()->property("dark").toBool(),
                      mode == fluent::FluentElement::Dark);
            const QString prefix = QStringLiteral("%1-%2-%3").arg(width).arg(pass++).arg(int(mode));
            capture(themeCard, theme, prefix + QStringLiteral("-theme"));
            capture(loadCard, load, prefix + QStringLiteral("-load-return"));
            EXPECT_EQ(load->rootObject(), loadRoot);
            EXPECT_EQ(theme->rootObject(), themeRoot);
            EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Dark);
        }
    }
}

#ifdef FLUENT_QT_HAS_SPATIAL
TEST_F(GalleryQmlBridgeTest, Contract_NativeQmlFailureKeepsTheCompletedFrameUntilFallback)
{
    const QString unavailable = tests::support::nativeOpenGLUnavailableReason();
    ASSERT_TRUE(unavailable.isEmpty()) << unavailable.toStdString();
    auto& settings = GallerySettings::instance();
    GalleryWindow window;
    window.resize(1160, 760);
    ASSERT_TRUE(window.selectRoute(QStringLiteral("qml-view")));
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            auto* page = qobject_cast<GalleryComponentPage*>(window.currentContentPage());
            return page && page->routeId() == QStringLiteral("qml-view") &&
                   !page->hasPendingSamples() &&
                   !window.findChild<QWidget*>(QStringLiteral("gallerySplashScreen"));
        },
        60000));
    auto* page = qobject_cast<GalleryComponentPage*>(window.currentContentPage());
    auto* controller = window.findChild<fluent::gallery::GallerySpatialController*>();
    auto* view = page->findChild<QmlView*>(QStringLiteral("qmlBridgeActionView"));
    auto* scroll = page->findChild<ScrollView*>(QStringLiteral("galleryContentScrollArea"));
    auto* model = page->findChild<ProgressBar*>(QStringLiteral("qmlBridgeProgressIndicator"));
    ASSERT_TRUE(controller && view && scroll && model);
    QTest::qWait(400);
    scroll->ensureWidgetVisible(view);
    const QPointer<QObject> root(view->rootObject());
    ASSERT_FALSE(root.isNull());
    auto* action = root->findChild<QQuickItem*>(QStringLiteral("qmlBridgeAdvanceButton"));
    ASSERT_NE(action, nullptr);
    QSignalSpy requested(&settings, &GallerySettings::spatialModeEnabledChanged);
    for (const bool composeFailure : {false, true}) {
        SCOPED_TRACE(composeFailure ? "composition target failure" : "QML frame GL failure");
        settings.setSpatialModeEnabled(true);
        QPointer<QOpenGLWidget> surface;
        ASSERT_TRUE(QTest::qWaitFor(
            [&] {
                surface = window.findChild<QOpenGLWidget*>(QStringLiteral("gallerySpatialSurface"));
                return surface && surface->property("presenting").toBool() &&
                       !controller->transitionRunning() && view->textureId();
            },
            5000));
        auto* layer = surface->findChild<fluent::qmlbridge::QmlTextureLayer*>();
        ASSERT_NE(layer, nullptr);
        model->setValue(40);
        view->setFocus();
        action->forceActiveFocus();
        const QPoint actionPoint =
            action->mapToScene(QPointF(action->width() / 2, action->height() / 2)).toPoint();
        const QPoint projected = controller->projectedPosition(view, actionPoint);
        QSignalSpy failures(layer, &fluent::qmlbridge::QmlTextureLayer::renderingFailed);
        const QImage before = surface->grabFramebuffer();
        ASSERT_FALSE(before.isNull());
        const int requestsBeforeFailure = requested.count();
        QMetaObject::Connection injection;
        bool injected = false;
        if (composeFailure) {
            surface->makeCurrent();
            EXPECT_EQ(layer->compose(view->textureId(), QSize(1, 1), 0), 0u);
            surface->doneCurrent();
        } else {
            auto* item = qobject_cast<QQuickItem*>(root.data());
            ASSERT_TRUE(item && item->window());
#if FLUENT_QUICK_USES_RHI
            const auto frameFinished = &QQuickWindow::afterFrameEnd;
#else
            const auto frameFinished = &QQuickWindow::afterRendering;
#endif
            injection = QObject::connect(
                item->window(), frameFinished, layer,
                [&] {
                    injected = true;
                    QOpenGLContext::currentContext()->functions()->glEnable(GLenum(0xffffffffu));
                },
                Qt::DirectConnection);
            ASSERT_TRUE(root->setProperty("color", QColor(Qt::magenta)));
        }
        // grabFramebuffer renders synchronously, before queued 2D restoration.
        const QImage failedFrame = surface->grabFramebuffer();
        QObject::disconnect(injection);
        EXPECT_TRUE(composeFailure || injected);
        ASSERT_EQ(failures.count(), 1);
        EXPECT_EQ(failedFrame, before)
            << "The failed auxiliary frame must not clear the completed Gallery scene";
        EXPECT_TRUE(surface->property("presenting").toBool());
        EXPECT_TRUE(view->isTextureRenderingEnabled());
        EXPECT_EQ(requested.count(), requestsBeforeFailure);
        EXPECT_TRUE(surface->property("galleryQmlFailurePending").toBool());
        // Send synchronously: QTest's input delay would process the queued fallback.
        QMouseEvent press(QEvent::MouseButtonPress, projected, window.mapToGlobal(projected),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, projected, window.mapToGlobal(projected),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &press);
        QCoreApplication::sendEvent(&window, &release);
        action->forceActiveFocus();
        QKeyEvent keyPress(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QKeyEvent keyRelease(QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier);
        QCoreApplication::sendEvent(view, &keyPress);
        QCoreApplication::sendEvent(view, &keyRelease);
        EXPECT_EQ(model->value(), 40)
            << "Input cannot act on a scene that differs from the retained frame";

        // A recreated FBO cannot retain old pixels. It must still contain the
        // ordinary panels, rather than a fully transparent intermediate frame.
        surface->resize(surface->size() + QSize(24, 16));
        const QImage resized = surface->grabFramebuffer();
        ASSERT_FALSE(resized.isNull());
        ASSERT_NE(resized.size(), before.size());
        int visible = 0;
        for (int y = 0; y < resized.height(); y += 16)
            for (int x = 0; x < resized.width(); x += 16)
                visible += resized.pixelColor(x, y).alpha() > 0;
        EXPECT_GT(visible, resized.width() * resized.height() / (16 * 16 * 4));
        ASSERT_TRUE(QTest::qWaitFor([&] {
            auto* native = view->findChild<QQuickWidget*>();
            return surface->isHidden() && !view->isTextureRenderingEnabled() && native &&
                   native->isVisible() && native->geometry() == view->contentsRect();
        }));
        EXPECT_EQ(view->rootObject(), root);
        EXPECT_TRUE(view->findChild<QQuickWidget*>()->isVisible());
        EXPECT_TRUE(settings.spatialModeEnabled());
        EXPECT_EQ(requested.count(), requestsBeforeFailure);
        EXPECT_FALSE(surface->property("galleryQmlFailurePending").toBool());
        auto* quick = view->findChild<QQuickWidget*>();
        QTest::mouseClick(quick, Qt::LeftButton, Qt::NoModifier, actionPoint);
        EXPECT_EQ(model->value(), 50);
        QTest::keyClick(quick, Qt::Key_Return);
        EXPECT_EQ(model->value(), 60);
        settings.setSpatialModeEnabled(false);
    }
}

TEST_F(GalleryQmlBridgeTest, Contract_NativeRequestedSpatialModeRoundtrip)
{
    const QString unavailable = tests::support::nativeOpenGLUnavailableReason();
    ASSERT_TRUE(unavailable.isEmpty()) << unavailable.toStdString();
    auto& settings = GallerySettings::instance();
    GalleryWindow window;
    window.resize(1160, 760);
    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            auto* page = window.currentContentPage();
            return page && page->routeId() == QStringLiteral("button") &&
                   !window.findChild<QWidget*>(QStringLiteral("gallerySplashScreen"));
        },
        60000));
    auto* controller = window.findChild<fluent::gallery::GallerySpatialController*>();
    ASSERT_NE(controller, nullptr);
    settings.setSpatialModeEnabled(true);
    QPointer<QOpenGLWidget> surface;
    const auto presenting = [&] {
        if (!surface)
            surface = window.findChild<QOpenGLWidget*>(QStringLiteral("gallerySpatialSurface"));
        return surface && surface->property("presenting").toBool() &&
               !controller->transitionRunning();
    };
    ASSERT_TRUE(QTest::qWaitFor(presenting, 5000));
    auto* textureLayer = surface->findChild<fluent::qmlbridge::QmlTextureLayer*>();
    ASSERT_NE(textureLayer, nullptr);
    QSignalSpy renderingFailures(textureLayer,
                                 &fluent::qmlbridge::QmlTextureLayer::renderingFailed);
    QSignalSpy requested(&settings, &GallerySettings::spatialModeEnabledChanged);
    ASSERT_TRUE(window.selectRoute(QStringLiteral("qml-view")));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            auto* page = window.currentContentPage();
            return page && page->routeId() == QStringLiteral("qml-view");
        },
        60000));
    auto* page = qobject_cast<GalleryComponentPage*>(window.currentContentPage());
    ASSERT_NE(page, nullptr);
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return page->findChildren<QmlView*>().size() == 6 && !page->hasPendingSamples(); }));
    auto* view = page->findChild<QmlView*>(QStringLiteral("qmlBridgeActionView"));
    auto* scroll = page->findChild<ScrollView*>(QStringLiteral("galleryContentScrollArea"));
    auto* title = page->findChild<LineEdit*>(QStringLiteral("qmlBridgeTitleEdit"));
    auto* model = page->findChild<ProgressBar*>(QStringLiteral("qmlBridgeProgressIndicator"));
    auto* contextView = page->findChild<QmlView*>(QStringLiteral("qmlBridgeContextView"));
    auto* lifecycle = page->findChild<QmlView*>(QStringLiteral("qmlBridgeLifecycleView"));
    ASSERT_TRUE(view && scroll && title && model && contextView && lifecycle);
    ASSERT_EQ(page->findChildren<QmlView*>().size(), 6);
    const auto diagnostics = [&] {
        QJsonObject state = QJsonObject::fromVariantMap(controller->renderingStatistics());
        state[QStringLiteral("qmlTexture")] =
            QJsonObject::fromVariantMap(view->textureStatistics());
        QStringList errors;
        for (const auto& arguments : renderingFailures)
            errors.append(arguments.front().toString());
        state[QStringLiteral("qmlRenderingFailures")] = QJsonArray::fromStringList(errors);
        return QJsonDocument(state).toJson(QJsonDocument::Compact).toStdString();
    };
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return presenting() && view->status() == QmlView::Ready &&
               view->isTextureRenderingEnabled();
    })) << diagnostics();
    QTest::qWait(400); // Allow the page's existing first-show scroll resets to finish.
    scroll->ensureWidgetVisible(view);
    ASSERT_TRUE(waitForStableGeometry(page, view, scroll));
    ASSERT_TRUE(QTest::qWaitFor([&] { return presenting() && view->textureId() != 0; }))
        << diagnostics();
    const QPointer<QObject> root(view->rootObject());
    auto* quick = view->findChild<QQuickWidget*>(QStringLiteral("qmlQuickWidget"));
    ASSERT_NE(quick, nullptr);
    const QPointer<QQmlEngine> engine(quick->engine());
    auto* action = root->findChild<QQuickItem*>(QStringLiteral("qmlBridgeAdvanceButton"));
    ASSERT_NE(action, nullptr);
    const auto assertProjected = [&] {
        EXPECT_TRUE(settings.spatialModeEnabled());
        EXPECT_TRUE(presenting());
        EXPECT_TRUE(fluent::gallery::depth::enabled(&window));
        EXPECT_TRUE(view->isTextureRenderingEnabled());
        EXPECT_TRUE(quick->isHidden());
        EXPECT_FALSE(view->property(kDirectComposition).toBool());
        EXPECT_FALSE(page->property(kDirectComposition).toBool());
        EXPECT_GT(view->textureId(), 0u);
        EXPECT_GT(view->textureRevision(), 0u);
        EXPECT_FALSE(view->textureSize().isEmpty());
        EXPECT_EQ(view->rootObject(), root);
        EXPECT_EQ(quick->engine(), engine);
        EXPECT_EQ(surface,
                  window.findChild<QOpenGLWidget*>(QStringLiteral("gallerySpatialSurface")));
    };
    assertProjected();
    const auto scenePoint = [&](const QPointF& point) {
        return action->mapToScene(point).toPoint();
    };
    const auto projected = [&](QWidget* widget, const QPoint& point) {
        return controller->projectedPosition(widget, point);
    };
    const auto colorAt = [&](const QPoint& local) {
        const QImage frame = surface->grabFramebuffer();
        const QPoint pixel =
            surface->mapFrom(&window, projected(view, local)) * surface->devicePixelRatioF();
        return frame.rect().contains(pixel) ? frame.pixelColor(pixel) : QColor();
    };
    const auto nearColor = [](const QColor& actual, const QColor& expected) {
        return actual.isValid() && qAbs(actual.red() - expected.red()) <= 12 &&
               qAbs(actual.green() - expected.green()) <= 12 &&
               qAbs(actual.blue() - expected.blue()) <= 12;
    };
    const QColor accent = view->themeObject()->property("accent").value<QColor>();
    const QColor background = view->themeObject()->property("surface").value<QColor>();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return nearColor(colorAt(scenePoint(QPointF(8, action->height() / 2))), accent) &&
               nearColor(colorAt(QPoint(view->width() - 24, view->height() - 24)), background);
    })) << "The final Spatial framebuffer must contain the actual QML scene";

    scroll->ensureWidgetVisible(title);
    ASSERT_TRUE(waitForStableGeometry(page, view, scroll));
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                      projected(title, title->rect().center()));
    title->selectAll();
    QTest::keyClicks(title, "Projected context input");
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return contextView->rootObject()->property("previewTitle").toString() == title->text();
    }));
    scroll->ensureWidgetVisible(view);
    ASSERT_TRUE(waitForStableGeometry(page, view, scroll));
    const int progress = int(model->value());
    QTest::mouseClick(
        &window, Qt::LeftButton, Qt::NoModifier,
        projected(view, scenePoint(QPointF(action->width() / 2, action->height() / 2))));
    ASSERT_TRUE(QTest::qWaitFor([&] { return int(model->value()) == progress + 10; }));
    EXPECT_EQ(title->text(), QStringLiteral("Projected context input"));
    assertProjected();
    EXPECT_TRUE(action->hasActiveFocus());
    auto* code = qmlCard(page)->codeBlock();
    if (auto* focused = QApplication::focusWidget())
        focused->clearFocus();
    code->setExpanded(true, false);
    ASSERT_TRUE(waitForStableGeometry(page, view, scroll));
    const QPoint center = scenePoint(QPointF(action->width() / 2, action->height() / 2));
    const int clippedScroll = view->mapTo(scroll->widget(), center).y() + 12;
    ASSERT_LE(clippedScroll, scroll->verticalScrollBar()->maximum());
    scroll->scrollTo(0, clippedScroll, false);
    ASSERT_TRUE(waitForStableGeometry(page, view, scroll));
    EXPECT_FALSE(scroll->viewport()->rect().contains(view->mapTo(scroll->viewport(), center)));
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, projected(view, center));
    QTest::qWait(80);
    EXPECT_EQ(int(model->value()), progress + 10)
        << "The clipped portion of a projected QML action must not receive input";
    EXPECT_FALSE(nearColor(colorAt(scenePoint(QPointF(8, action->height() / 2))), accent))
        << "A projected QML texture must not escape the scroll viewport";
    code->setExpanded(false, false);
    scroll->ensureWidgetVisible(view);
    ASSERT_TRUE(waitForStableGeometry(page, view, scroll));
    const QPointer<QObject> focused(action);
    for (int pass = 0; pass < 2; ++pass) {
        settings.setSpatialModeEnabled(false);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return !surface->property("presenting").toBool() &&
                   !view->isTextureRenderingEnabled() && !quick->isHidden();
        }));
        EXPECT_EQ(view->rootObject(), root);
        EXPECT_EQ(quick->engine(), engine);
        EXPECT_EQ(root->findChild<QQuickItem*>(QStringLiteral("qmlBridgeAdvanceButton")), focused);
        EXPECT_EQ(int(model->value()), progress + 10);
        settings.setSpatialModeEnabled(true);
        ASSERT_TRUE(QTest::qWaitFor([&] { return presenting() && view->textureId() != 0; }));
        assertProjected();
    }
    EXPECT_EQ(requested.count(), 4);

    QTest::qWait(400);
    const quint64 revision = view->textureRevision();
    const auto frames = view->textureStatistics()[QStringLiteral("renderedFrames")];
    EXPECT_GT(frames.toULongLong(), 0u);
    QTest::mouseMove(&window, QPoint(window.width() / 2, 24));
    QTest::qWait(150);
    EXPECT_EQ(view->textureRevision(), revision);
    EXPECT_EQ(view->textureStatistics()[QStringLiteral("renderedFrames")], frames)
        << "Perspective motion must reuse the QML GPU texture";

    auto* deactivate = page->findChild<Button*>(QStringLiteral("qmlBridgeDeactivate"));
    auto* reload = page->findChild<Button*>(QStringLiteral("qmlBridgeReload"));
    ASSERT_TRUE(deactivate && reload);
    const QPointer<QObject> lifecycleRoot(lifecycle->rootObject());
    const QPointer<QQuickWidget> lifecycleQuick(lifecycle->findChild<QQuickWidget*>());
    ASSERT_FALSE(lifecycleQuick.isNull());
    const QPointer<QQmlEngine> lifecycleEngine(lifecycleQuick->engine());
    scroll->ensureWidgetVisible(deactivate);
    ASSERT_TRUE(waitForStableGeometry(page, view, scroll));
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                      projected(deactivate, deactivate->rect().center()));
    ASSERT_EQ(lifecycle->status(), QmlView::Unloaded);
    EXPECT_EQ(lifecycle->rootObject(), nullptr);
    EXPECT_EQ(lifecycle->textureId(), 0u);
    EXPECT_TRUE(lifecycleQuick.isNull() || lifecycleQuick->isHidden());
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            return lifecycleQuick.isNull() && lifecycleRoot.isNull() && lifecycleEngine.isNull() &&
                   page->findChildren<QQuickWidget*>().size() == 5;
        },
        1000));
    EXPECT_EQ(page->findChildren<QQuickWidget*>().size(), 5);
    EXPECT_EQ(view->rootObject(), root);
    EXPECT_EQ(quick->engine(), engine);
    EXPECT_TRUE(settings.spatialModeEnabled());
    ASSERT_TRUE(QTest::qWaitFor(presenting));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(lifecycleRoot.isNull());
    EXPECT_TRUE(lifecycleEngine.isNull());
    title->setText(QStringLiteral("Other cards stay editable"));
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                      projected(reload, reload->rect().center()));
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return presenting() && lifecycle->status() == QmlView::Ready &&
               lifecycle->isTextureRenderingEnabled();
    })) << diagnostics();
    EXPECT_EQ(page->findChildren<QQuickWidget*>().size(), 6);
    EXPECT_EQ(view->rootObject(), root);
    EXPECT_EQ(contextView->rootObject()->property("previewTitle").toString(), title->text());
    scroll->ensureWidgetVisible(view);
    ASSERT_TRUE(waitForStableGeometry(page, view, scroll));
    ASSERT_TRUE(QTest::qWaitFor([&] { return presenting() && view->textureId() != 0; }));
    const QPointer<QObject> reloadedRoot(view->rootObject());
    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    ASSERT_TRUE(QTest::qWaitFor(presenting));
    EXPECT_FALSE(view->isVisible());
    EXPECT_EQ(view->rootObject(), reloadedRoot);
    const auto cachedFrames = view->textureStatistics()[QStringLiteral("renderedFrames")];
    QTest::qWait(150);
    EXPECT_EQ(view->textureStatistics()[QStringLiteral("renderedFrames")], cachedFrames);
    ASSERT_TRUE(window.selectRoute(QStringLiteral("qml-view")));
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return presenting() && view->status() == QmlView::Ready &&
               view->isTextureRenderingEnabled();
    })) << diagnostics();
    QTest::qWait(400);
    scroll->ensureWidgetVisible(view);
    ASSERT_TRUE(waitForStableGeometry(page, view, scroll));
    ASSERT_TRUE(QTest::qWaitFor([&] { return presenting() && view->textureId() != 0; }));
    EXPECT_EQ(view->rootObject(), reloadedRoot);
    EXPECT_EQ(int(model->value()), progress + 10);
    EXPECT_TRUE(view->findChild<QQuickWidget*>()->isHidden());
    EXPECT_TRUE(sampleGeometry(page));
    EXPECT_EQ(requested.count(), 4);
}
#endif
