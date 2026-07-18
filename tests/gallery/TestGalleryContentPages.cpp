#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>

#include <QApplication>
#include <QAbstractScrollArea>
#include <QBoxLayout>
#include <QClipboard>
#include <QElapsedTimer>
#include <QEvent>
#include <QFrame>
#include <QFile>
#include <QFontDatabase>
#include <QGraphicsOpacityEffect>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMargins>
#include <QPoint>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStringList>
#include <QTest>
#include <QVector>
#include <QWidget>

#include "components/basicinput/Button.h"
#include "compatibility/QtCompat.h"
#include "components/collections/TreeView.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/scrolling/PipsPager.h"
#include "components/scrolling/ScrollView.h"
#include "components/status_info/ToolTip.h"
#include "components/textfields/Label.h"
#include "components/textfields/TextEdit.h"
#include "design/Typography.h"
#include "model/GalleryComponentCatalog.h"
#include "model/GalleryContentCatalog.h"
#include "view/pages/GalleryCategoryPage.h"
#include "view/widgets/GalleryCodeBlock.h"
#include "view/pages/GalleryComponentPage.h"
#include "view/pages/GalleryContentPage.h"
#include "view/pages/GalleryFoundationTopicPage.h"
#include "view/widgets/GalleryComponentReferenceCard.h"
#include "view/widgets/GalleryEntryGrid.h"
#include "view/widgets/GalleryIconBrowser.h"
#include "view/widgets/GallerySampleCard.h"
#include "view/widgets/GallerySampleCatalog.h"
#include "view/widgets/samples/SampleBuilders.h"
#include "view/shell/GalleryWindow.h"
#include "view/support/GalleryToast.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "viewmodel/GallerySettings.h"
#include "QtTestEnvironment.h"

using fluent::gallery::GalleryCategoryPage;
using fluent::gallery::GalleryCodeBlock;
using fluent::gallery::GalleryComponentPage;
using fluent::gallery::GalleryComponentReferenceCard;
using fluent::gallery::GalleryContentPage;
using fluent::gallery::GalleryEntryGrid;
using fluent::gallery::GalleryFoundationTopicPage;
using fluent::gallery::GalleryIconBrowser;
using fluent::gallery::GalleryNavigationViewModel;
using fluent::gallery::GallerySampleCard;
using fluent::gallery::GalleryWindow;
using fluent::gallery::galleryComponentCatalog;
using fluent::gallery::galleryComponentReference;
using fluent::gallery::galleryControlImageResource;
using fluent::gallery::galleryContentCatalog;
using fluent::gallery::galleryContentEntry;
using fluent::collections::TreeView;
using fluent::textfields::TextEdit;
using fluent::basicinput::Button;

namespace {

class ResizablePreview final : public QWidget {
public:
    explicit ResizablePreview(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    QSize sizeHint() const override
    {
        return QSize(180, m_preferredHeight);
    }

    void setPreferredHeight(int height)
    {
        m_preferredHeight = height;
        updateGeometry();
    }

private:
    int m_preferredHeight = 40;
};

int expectedButtonRowSpacing(int requested)
{
    return fluentAdjacentButtonRowSpacing(requested);
}

QRect mappedRectInAncestor(const QWidget* widget, const QWidget* ancestor)
{
    return QRect(widget->mapTo(const_cast<QWidget*>(ancestor), QPoint(0, 0)), widget->size());
}

GallerySampleCard* sampleCardById(GalleryComponentPage* page, const QString& sampleId)
{
    if (!page)
        return nullptr;
    for (GallerySampleCard* card : page->sampleCards()) {
        if (card && card->sampleId() == sampleId)
            return card;
    }
    return nullptr;
}

Button* buttonWithText(QWidget* root, const QString& text)
{
    if (!root)
        return nullptr;
    const auto buttons = root->findChildren<Button*>();
    for (Button* button : buttons) {
        if (button && button->text() == text)
            return button;
    }
    return nullptr;
}

bool findSampleById(const QString& route,
                    const QString& sampleId,
                    fluent::gallery::GallerySample* outSample)
{
    const auto samples = fluent::gallery::gallerySamplesForRoute(route);
    for (const auto& sample : samples) {
        if (sample.id == sampleId) {
            if (outSample)
                *outSample = sample;
            return true;
        }
    }
    return false;
}

QList<Button*> directButtonsLeftToRight(QWidget* root)
{
    QList<Button*> buttons = root ? root->findChildren<Button*>(QString(), Qt::FindDirectChildrenOnly)
                                  : QList<Button*>();
    std::sort(buttons.begin(), buttons.end(), [root](Button* left, Button* right) {
        return mappedRectInAncestor(left, root).x() < mappedRectInAncestor(right, root).x();
    });
    return buttons;
}

int horizontalGapInAncestor(const QWidget* left, const QWidget* right, const QWidget* ancestor)
{
    const QRect leftRect = mappedRectInAncestor(left, ancestor);
    const QRect rightRect = mappedRectInAncestor(right, ancestor);
    return rightRect.x() - (leftRect.x() + leftRect.width());
}

template <typename PageType>
PageType* waitForCurrentPage(GalleryWindow& window, int timeoutMs = 1000)
{
    QElapsedTimer timer;
    timer.start();
    PageType* page = dynamic_cast<PageType*>(window.currentContentPage());
    while (!page && timer.elapsed() < timeoutMs) {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        QTest::qWait(10);
        page = dynamic_cast<PageType*>(window.currentContentPage());
    }
    return page;
}

} // namespace

class GalleryContentPagesTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    void TearDown() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }
};

// Task 6.1: seeded content routes resolve and stay consistent with navigation routes.
TEST_F(GalleryContentPagesTest, ContentCatalogSeededRoutesMatchNavigation)
{
    GalleryNavigationViewModel navigationViewModel;

    const QStringList seededRouteIds{
        QStringLiteral("home"),
        QStringLiteral("basic-input"),
        QStringLiteral("collections"),
        QStringLiteral("navigation"),
        QStringLiteral("button"),
        QStringLiteral("tree-view"),
        QStringLiteral("tab-view")
    };

    for (const QString& routeId : seededRouteIds) {
        const auto* entry = galleryContentEntry(routeId);
        ASSERT_NE(entry, nullptr) << routeId.toStdString();
        EXPECT_EQ(entry->routeId, routeId);
        EXPECT_NE(navigationViewModel.itemById(routeId), nullptr) << routeId.toStdString();
    }

    // Settings stays footer-owned and out of the content catalog.
    EXPECT_EQ(galleryContentEntry(QStringLiteral("settings")), nullptr);

    // Every catalog entry must correspond to a known navigation route.
    for (const auto& entry : galleryContentCatalog()) {
        EXPECT_NE(navigationViewModel.itemById(entry.routeId), nullptr)
            << entry.routeId.toStdString();
    }
}

// Full coverage: every navigation route except Settings has a content entry, and
// every component route resolves at least one live sample with preview and code.
TEST_F(GalleryContentPagesTest, AllNavigationRoutesHaveContentAndSamples)
{
    GalleryNavigationViewModel navigationViewModel;

    for (const QString& routeId : navigationViewModel.navigationEntryIds()) {
        if (routeId == QStringLiteral("settings"))
            continue;
        const auto* entry = galleryContentEntry(routeId);
        ASSERT_NE(entry, nullptr) << routeId.toStdString();
        EXPECT_FALSE(entry->description.isEmpty()) << routeId.toStdString();

        if (entry->kind != fluent::gallery::GalleryPageKind::Component)
            continue;

        const auto samples = fluent::gallery::gallerySamplesForRoute(routeId);
        ASSERT_GE(samples.size(), 1) << routeId.toStdString();
        for (const auto& sample : samples) {
            EXPECT_TRUE(static_cast<bool>(sample.createPreview)) << sample.id.toStdString();
            EXPECT_FALSE(sample.codeSnippet.isEmpty()) << sample.id.toStdString();
        }
    }
}

TEST_F(GalleryContentPagesTest, FoundationTopicsExposeFullIconCatalogAndSeparateSpacing)
{
    GalleryWindow window;

    ASSERT_TRUE(window.selectRoute(QStringLiteral("foundation-iconography")));
    auto* iconPage = waitForCurrentPage<GalleryFoundationTopicPage>(window);
    ASSERT_NE(iconPage, nullptr);
    auto* browser = iconPage->findChild<GalleryIconBrowser*>(
        QStringLiteral("galleryIconBrowser"));
    ASSERT_NE(browser, nullptr);
    EXPECT_EQ(browser->iconCount(), 9558);
    EXPECT_EQ(browser->visibleIconCount(), browser->iconCount());
    auto* countLabel = browser->findChild<fluent::textfields::Label*>(
        QStringLiteral("galleryIconCount"));
    ASSERT_NE(countLabel, nullptr);
    EXPECT_TRUE(countLabel->text().contains(QStringLiteral("icons")));
    EXPECT_EQ(browser->findChild<QAbstractScrollArea*>(), nullptr);
    auto* iconGrid = browser->findChild<QWidget*>(QStringLiteral("galleryIconGrid"));
    ASSERT_NE(iconGrid, nullptr);
    auto* pagination = browser->findChild<QWidget*>(
        QStringLiteral("galleryIconPagination"));
    auto* pageLabel = browser->findChild<fluent::textfields::Label*>(
        QStringLiteral("galleryIconPageLabel"));
    auto* pager = browser->findChild<fluent::scrolling::PipsPager*>(
        QStringLiteral("galleryIconPager"));
    auto* hoverTip = browser->findChild<fluent::status_info::ToolTip*>(
        QStringLiteral("galleryIconHoverTip"));
    ASSERT_NE(pagination, nullptr);
    ASSERT_NE(pageLabel, nullptr);
    ASSERT_NE(pager, nullptr);
    ASSERT_NE(hoverTip, nullptr);
    EXPECT_FALSE(pagination->isHidden());
    EXPECT_EQ(pager->numberOfPages(), 45);
    EXPECT_EQ(pager->selectedPageIndex(), 0);
    EXPECT_TRUE(pageLabel->text().contains(QStringLiteral("1-216")));
    EXPECT_TRUE(pageLabel->text().contains(QStringLiteral("Page 1 of 45")));

    // The full catalog is split into bounded, dense pages so the gallery keeps
    // one useful outer scrollbar rather than a tiny thumb or nested scroll area.
    // zh_CN: 完整目录按紧凑页分段，页面只保留一个易用的外层滚动条。
    iconGrid->resize(920, iconGrid->heightForWidth(920));
    EXPECT_LT(iconGrid->height(), 700);
    pager->setSelectedPageIndex(1);
    QApplication::processEvents();
    EXPECT_EQ(pager->selectedPageIndex(), 1);
    EXPECT_TRUE(pageLabel->text().contains(QStringLiteral("217-432")));
    EXPECT_TRUE(pageLabel->text().contains(QStringLiteral("Page 2 of 45")));

    auto* search = browser->findChild<QLineEdit*>(QStringLiteral("galleryIconSearch"));
    ASSERT_NE(search, nullptr);
    search->setText(QStringLiteral("ruler 20"));
    QApplication::processEvents();
    EXPECT_GT(browser->visibleIconCount(), 0);
    EXPECT_LT(browser->visibleIconCount(), browser->iconCount());
    EXPECT_FALSE(browser->showingClosestMatches());

    // Name typos fall back only after the deterministic search returns no
    // rows. Structured size terms remain exact during that fallback.
    search->setText(QStringLiteral("calendar 20"));
    QApplication::processEvents();
    const int exactCalendarCount = browser->visibleIconCount();
    ASSERT_GT(exactCalendarCount, 0);
    EXPECT_FALSE(browser->showingClosestMatches());

    search->setText(QStringLiteral("calender 20"));
    QApplication::processEvents();
    EXPECT_EQ(browser->visibleIconCount(), exactCalendarCount);
    EXPECT_TRUE(browser->showingClosestMatches());
    EXPECT_TRUE(countLabel->text().startsWith(QStringLiteral("Closest matches:")));

    // Common design-language aliases rank ahead of edit-distance matches, so
    // a semantic synonym does not pull unrelated spelling-nearby icons in.
    search->setText(QStringLiteral("delete 20"));
    QApplication::processEvents();
    const int exactDeleteCount = browser->visibleIconCount();
    ASSERT_GT(exactDeleteCount, 0);

    search->setText(QStringLiteral("trash 20"));
    QApplication::processEvents();
    EXPECT_EQ(browser->visibleIconCount(), exactDeleteCount);
    EXPECT_TRUE(browser->showingClosestMatches());

    // Very short unknown terms stay strict instead of producing noisy fuzzy
    // result sets.
    search->setText(QStringLiteral("qz"));
    QApplication::processEvents();
    EXPECT_EQ(browser->visibleIconCount(), 0);
    EXPECT_FALSE(browser->showingClosestMatches());

    search->setText(QStringLiteral("U+F109"));
    QApplication::processEvents();
    EXPECT_EQ(browser->visibleIconCount(), 1);
    EXPECT_FALSE(browser->showingClosestMatches());

    search->setText(QStringLiteral("ic_fluent_add_20_regular"));
    QApplication::processEvents();
    ASSERT_EQ(browser->visibleIconCount(), 1);
    EXPECT_FALSE(browser->showingClosestMatches());
    EXPECT_TRUE(pagination->isHidden());
    EXPECT_EQ(pager->numberOfPages(), 1);
    iconGrid->resize(600, iconGrid->heightForWidth(600));
    EXPECT_EQ(iconGrid->height(), 44);

    // Tiles paint only the glyph. The project's Fluent ToolTip supplies the
    // complete name/codepoint/size metadata after the hover delay.
    // zh_CN: 卡片仅绘制图标，悬停后由项目 Fluent ToolTip 展示完整元数据。
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();
    FLUENT_MAKE_MOUSE_EVENT(hoverMove, QEvent::MouseMove, iconGrid, QPoint(22, 22),
                            Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(iconGrid, &hoverMove);
    QTest::qWait(360);
    QApplication::processEvents();
    EXPECT_TRUE(hoverTip->text().contains(QStringLiteral("ic_fluent_add_20_regular")));
    EXPECT_TRUE(hoverTip->text().contains(QStringLiteral("U+")));
    EXPECT_TRUE(hoverTip->text().contains(QStringLiteral("20 px")));

    QGuiApplication::clipboard()->clear();
    QTest::mouseClick(iconGrid, Qt::LeftButton, Qt::NoModifier, QPoint(22, 22));
    const QString copiedLookup = QGuiApplication::clipboard()->text();
    EXPECT_EQ(copiedLookup,
              QStringLiteral("Typography::Icons::glyph(QStringLiteral(\"ic_fluent_add_20_regular\"))"));
    EXPECT_NE(window.findChild<QWidget*>(QStringLiteral("galleryToast")), nullptr);

    // Copy and search are one round trip: the generated C++ expression can be
    // pasted back verbatim instead of forcing users to extract the icon name.
    search->setText(copiedLookup);
    QApplication::processEvents();
    EXPECT_EQ(browser->visibleIconCount(), 1);
    EXPECT_TRUE(pagination->isHidden());

    ASSERT_TRUE(window.selectRoute(QStringLiteral("foundation-spacing")));
    auto* spacingPage = waitForCurrentPage<GalleryFoundationTopicPage>(window);
    ASSERT_NE(spacingPage, nullptr);
    EXPECT_EQ(spacingPage->routeId(), QStringLiteral("foundation-spacing"));
    EXPECT_EQ(spacingPage->title(), QStringLiteral("Spacing"));
}

TEST_F(GalleryContentPagesTest, FoundationVisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST"))
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Foundation visual review requires a desktop platform";

    auto& settings = fluent::gallery::GallerySettings::instance();
    settings.setIntroCompleted(true);
    const auto previousThemeMode = settings.themeMode();
    struct ThemeModeRestore final {
        fluent::gallery::GallerySettings& settings;
        fluent::gallery::GallerySettings::ThemeMode mode;
        ~ThemeModeRestore() { settings.setThemeMode(mode); }
    } restoreThemeMode{settings, previousThemeMode};
    GalleryWindow window;
    // QWidget::grab cannot capture the pixels supplied by DWM Mica, so use the
    // library's solid backdrop during deterministic snapshots.
    // zh_CN: QWidget::grab 无法抓取 DWM Mica 提供的像素，确定性快照改用库内置纯色背景。
    window.setBackdropEffect(fluent::windowing::BackdropEffect::Solid);
    if (tests::support::shouldCaptureVisualSnapshot()) {
        window.setFixedSize(QSize(1440, 900));
        window.show();

        // Let startup prewarm and the splash fade finish before the first capture.
        // zh_CN: 首张截图前等待启动预热和 splash 淡出完成。
        QElapsedTimer startupTimer;
        startupTimer.start();
        while (window.findChild<QWidget*>(QStringLiteral("gallerySplashScreen"))
               && startupTimer.elapsed() < 7000) {
            QApplication::processEvents(QEventLoop::AllEvents, 25);
            QTest::qWait(20);
        }
        ASSERT_EQ(window.findChild<QWidget*>(QStringLiteral("gallerySplashScreen")), nullptr);

        const auto waitForRoute = [&window](const QString& routeId) {
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 5000) {
                auto* page = window.currentContentPage();
                if (page && page->routeId() == routeId)
                    return true;
                QApplication::processEvents(QEventLoop::AllEvents, 25);
                QTest::qWait(20);
            }
            return false;
        };

        struct SnapshotCase {
            QString routeId;
            QString variant;
            tests::support::VisualSnapshotTheme theme;
        };
        const QVector<SnapshotCase> snapshots = {
            {QStringLiteral("foundation-color"), QStringLiteral("color-light"),
             tests::support::VisualSnapshotTheme::Light},
            {QStringLiteral("foundation-geometry"), QStringLiteral("geometry-light"),
             tests::support::VisualSnapshotTheme::Light},
            {QStringLiteral("foundation-iconography"), QStringLiteral("iconography-light"),
             tests::support::VisualSnapshotTheme::Light},
            {QStringLiteral("foundation-spacing"), QStringLiteral("spacing-light"),
             tests::support::VisualSnapshotTheme::Light},
            {QStringLiteral("foundation-typography"), QStringLiteral("typography-light"),
             tests::support::VisualSnapshotTheme::Light},
            {QStringLiteral("foundation-iconography"), QStringLiteral("iconography-dark"),
             tests::support::VisualSnapshotTheme::Dark},
            {QStringLiteral("foundation-typography"), QStringLiteral("typography-dark"),
             tests::support::VisualSnapshotTheme::Dark},
        };

        for (const SnapshotCase& snapshot : snapshots) {
            const bool dark = snapshot.theme == tests::support::VisualSnapshotTheme::Dark;
            settings.setThemeMode(dark
                                      ? fluent::gallery::GallerySettings::ThemeMode::Dark
                                      : fluent::gallery::GallerySettings::ThemeMode::Light);
            ASSERT_TRUE(window.selectRoute(snapshot.routeId));
            ASSERT_TRUE(waitForRoute(snapshot.routeId)) << snapshot.routeId.toStdString();
            QTest::qWait(250);  // Let the navigation selection indicator settle.
            QApplication::processEvents(QEventLoop::AllEvents, 25);
            ASSERT_EQ(fluent::FluentElement::currentTheme(),
                      dark ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
            tests::support::VisualSnapshotOptions options;
            options.windowSize = QSize(1440, 900);
            options.variant = snapshot.variant;
            options.theme = snapshot.theme;
            ASSERT_TRUE(tests::support::captureVisualSnapshot(&window, options));
        }
        return;
    }

    ASSERT_TRUE(window.selectRoute(QStringLiteral("foundation-typography")));
    window.show();
    qApp->exec();
}

TEST_F(GalleryContentPagesTest, ComponentReferencesMatchPublicIntegrationSurface)
{
    for (const auto& category : galleryComponentCatalog()) {
        for (const auto& component : category.components) {
            const auto reference = galleryComponentReference(component.id);
            ASSERT_TRUE(reference.isValid()) << component.id.toStdString();
            EXPECT_EQ(reference.header, QStringLiteral("<FluentQt/FluentQt.h>"));
            EXPECT_EQ(reference.cmakeTarget, QStringLiteral("FluentQt::FluentQt"));
            EXPECT_TRUE(reference.qualifiedType.startsWith(
                QStringLiteral("fluent::%1::").arg(category.sourceDirectory)));
        }
    }

    EXPECT_EQ(galleryComponentReference(QStringLiteral("menu")).qualifiedType,
              QStringLiteral("fluent::menus_toolbars::FluentMenu"));
    EXPECT_FALSE(galleryComponentReference(QStringLiteral("missing-route")).isValid());
}

// Every component route builds its page with live sample previews, exercising
// each preview factory once so a broken sample fails fast here.
TEST_F(GalleryContentPagesTest, EveryComponentRouteBuildsItsPage)
{
    GalleryWindow window;
    GalleryNavigationViewModel navigationViewModel;
    for (const auto& item : navigationViewModel.items()) {
        if (item.kind != fluent::gallery::GalleryNavigationItem::Kind::ComponentRoute)
            continue;
        const auto* entry = galleryContentEntry(item.id);
        if (!entry || entry->kind != fluent::gallery::GalleryPageKind::Component)
            continue;
        ASSERT_TRUE(window.selectRoute(item.id)) << item.id.toStdString();
        auto* page = waitForCurrentPage<GalleryComponentPage>(window);
        ASSERT_NE(page, nullptr) << item.id.toStdString();
        EXPECT_GE(page->sampleCount(), 1) << item.id.toStdString();
        for (GallerySampleCard* card : page->sampleCards()) {
            EXPECT_NE(card->previewWidget(), nullptr)
                << item.id.toStdString() << " " << card->sampleId().toStdString();
        }
    }
}

// The All controls route lists every component as a clickable card.
TEST_F(GalleryContentPagesTest, AllControlsRouteListsEveryComponent)
{
    GalleryWindow window;
    ASSERT_TRUE(window.selectRoute(QStringLiteral("all-controls")));
    auto* page = waitForCurrentPage<GalleryCategoryPage>(window);
    ASSERT_NE(page, nullptr);

    GalleryNavigationViewModel navigationViewModel;
    int componentCount = 0;
    for (const auto& item : navigationViewModel.items()) {
        if (item.kind == fluent::gallery::GalleryNavigationItem::Kind::ComponentRoute)
            ++componentCount;
    }
    EXPECT_EQ(page->componentRouteIds().size(), componentCount);
}

TEST_F(GalleryContentPagesTest, EntryGridExpandsCardsForWrappedDescriptions)
{
    GalleryEntryGrid grid;
    grid.resize(480, 100);
    grid.setEntries({{QStringLiteral("foundation-qmlplus"),
                      QStringLiteral("QML+"),
                      QStringLiteral("QML+ brings anchors, reactive property binding, and named states to plain QWidget controls."),
                      QPixmap(),
                      QString()}});
    grid.show();
    QApplication::processEvents();
    const int wideHeight = grid.sizeHint().height();

    grid.resize(240, 100);
    QApplication::processEvents();
    const int narrowHeight = grid.sizeHint().height();

    EXPECT_GT(narrowHeight, 86);
    EXPECT_GT(narrowHeight, wideHeight);
}

TEST_F(GalleryContentPagesTest, ComponentCardsUseBundledControlImages)
{
    const QString placeholder =
        QStringLiteral(":/app/assets/control_images/Placeholder.png");

    for (const auto& category : galleryComponentCatalog()) {
        for (const auto& component : category.components) {
            const QString resource = galleryControlImageResource(component.title);
            EXPECT_NE(resource, placeholder) << component.title.toStdString();
            EXPECT_TRUE(QFile::exists(resource)) << resource.toStdString();
        }
    }
}

// Task 6.2: category routes build category overview pages with virtualized component grids.
TEST_F(GalleryContentPagesTest, CategoryRoutesCreateCategoryPages)
{
    GalleryWindow window;

    struct CategoryCase {
        QString routeId;
        QString seededComponentRouteId;
    };
    const QVector<CategoryCase> cases{
        {QStringLiteral("basic-input"), QStringLiteral("button")},
        {QStringLiteral("collections"), QStringLiteral("tree-view")},
        {QStringLiteral("navigation"), QStringLiteral("tab-view")}
    };

    for (const CategoryCase& categoryCase : cases) {
        ASSERT_TRUE(window.selectRoute(categoryCase.routeId)) << categoryCase.routeId.toStdString();
        auto* page = waitForCurrentPage<GalleryCategoryPage>(window);
        ASSERT_NE(page, nullptr) << categoryCase.routeId.toStdString();
        EXPECT_EQ(page->routeId(), categoryCase.routeId);
        EXPECT_TRUE(page->componentRouteIds().contains(categoryCase.seededComponentRouteId))
            << categoryCase.routeId.toStdString();

        auto* grid = page->findChild<GalleryEntryGrid*>();
        ASSERT_NE(grid, nullptr) << categoryCase.routeId.toStdString();
        EXPECT_EQ(grid->entryCount(), page->componentRouteIds().size())
            << categoryCase.routeId.toStdString();
    }
}

// Task 6.3: component routes build component pages with expected ids and sample counts.
TEST_F(GalleryContentPagesTest, ComponentRoutesCreateComponentPages)
{
    GalleryWindow window;

    struct ComponentCase {
        QString routeId;
        QString title;
        int minimumSampleCount;
    };
    const QVector<ComponentCase> cases{
        {QStringLiteral("button"), QStringLiteral("Button"), 4},
        {QStringLiteral("tree-view"), QStringLiteral("TreeView"), 1},
        {QStringLiteral("tab-view"), QStringLiteral("TabView"), 1}
    };

    for (const ComponentCase& componentCase : cases) {
        ASSERT_TRUE(window.selectRoute(componentCase.routeId)) << componentCase.routeId.toStdString();
        auto* page = waitForCurrentPage<GalleryComponentPage>(window);
        ASSERT_NE(page, nullptr) << componentCase.routeId.toStdString();
        EXPECT_EQ(page->routeId(), componentCase.routeId);
        EXPECT_EQ(page->title(), componentCase.title);
        EXPECT_FALSE(page->overviewText().isEmpty()) << componentCase.routeId.toStdString();
        ASSERT_NE(page->referenceCard(), nullptr) << componentCase.routeId.toStdString();
        EXPECT_TRUE(page->referenceCard()->reference().isValid());
        EXPECT_GE(page->sampleCount(), componentCase.minimumSampleCount)
            << componentCase.routeId.toStdString();
    }
}

// Task 6.4: sample cards host a live preview widget and expose code snippets where defined.
TEST_F(GalleryContentPagesTest, SampleCardsHostLivePreviewAndCode)
{
    GalleryWindow window;
    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    auto* page = waitForCurrentPage<GalleryComponentPage>(window);
    ASSERT_NE(page, nullptr);
    ASSERT_GE(page->sampleCount(), 4);

    for (GallerySampleCard* card : page->sampleCards()) {
        ASSERT_NE(card, nullptr);
        EXPECT_NE(card->previewWidget(), nullptr) << card->sampleId().toStdString();
        ASSERT_NE(card->codeBlock(), nullptr) << card->sampleId().toStdString();
        EXPECT_FALSE(card->codeBlock()->code().isEmpty()) << card->sampleId().toStdString();
        EXPECT_NE(card->codeBlock()->copyButton(), nullptr) << card->sampleId().toStdString();
    }
}

TEST_F(GalleryContentPagesTest, HorizontalSampleGroupUsesRequestedSpacing)
{
    std::unique_ptr<QWidget> group(fluent::gallery::samples::horizontalGroup(nullptr, 10));

    auto* first = new Button(QStringLiteral("First"), group.get());
    auto* second = new Button(QStringLiteral("Second"), group.get());
    auto* third = new Button(QStringLiteral("Third"), group.get());

    group->layout()->addWidget(first);
    group->layout()->addWidget(second);
    group->layout()->addWidget(third);
    group->resize(group->sizeHint());
    group->layout()->setGeometry(group->rect());
    QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QApplication::processEvents();

    EXPECT_EQ(second->x() - (first->x() + first->width()), expectedButtonRowSpacing(10));
    EXPECT_EQ(third->x() - (second->x() + second->width()), expectedButtonRowSpacing(10));
    ASSERT_EQ(group->layout()->count(), 5);
    ASSERT_NE(group->layout()->itemAt(1)->widget(), nullptr);
    EXPECT_EQ(group->layout()->itemAt(1)->widget()->width(), expectedButtonRowSpacing(10));
    ASSERT_NE(group->layout()->itemAt(3)->widget(), nullptr);
    EXPECT_EQ(group->layout()->itemAt(3)->widget()->width(), expectedButtonRowSpacing(10));
}

TEST_F(GalleryContentPagesTest, HorizontalSampleGroupKeepsSpacingThroughQBoxLayoutApi)
{
    std::unique_ptr<QWidget> group(fluent::gallery::samples::horizontalGroup(nullptr, 10));
    auto* layout = qobject_cast<QBoxLayout*>(group->layout());
    ASSERT_NE(layout, nullptr);

    auto* first = new Button(QStringLiteral("First"), group.get());
    auto* second = new Button(QStringLiteral("Second"), group.get());
    auto* third = new Button(QStringLiteral("Third"), group.get());

    layout->addWidget(first);
    layout->addWidget(second);
    layout->addWidget(third);
    layout->addStretch(1);
    group->resize(group->sizeHint() + QSize(160, 20));
    layout->setGeometry(group->rect());
    QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QApplication::processEvents();

    EXPECT_EQ(first->x(), 0);
    EXPECT_EQ(second->x() - (first->x() + first->width()), expectedButtonRowSpacing(10));
    EXPECT_EQ(third->x() - (second->x() + second->width()), expectedButtonRowSpacing(10));
    ASSERT_EQ(layout->count(), 6);
    ASSERT_NE(layout->itemAt(1)->widget(), nullptr);
    EXPECT_EQ(layout->itemAt(1)->widget()->width(), expectedButtonRowSpacing(10));
    ASSERT_NE(layout->itemAt(3)->widget(), nullptr);
    EXPECT_EQ(layout->itemAt(3)->widget()->width(), expectedButtonRowSpacing(10));
    ASSERT_NE(layout->itemAt(5)->spacerItem(), nullptr);
}

TEST_F(GalleryContentPagesTest, StackViewSampleButtonsUseRequestedSpacing)
{
    fluent::gallery::GallerySample sample;
    ASSERT_TRUE(findSampleById(QStringLiteral("stack-view"),
                               QStringLiteral("stack-view-basic"),
                               &sample));
    ASSERT_TRUE(static_cast<bool>(sample.createPreview));

    GallerySampleCard card(sample);
    card.resize(640, card.sizeHint().height());
    card.show();
    QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QApplication::processEvents();

    Button* pushButton = buttonWithText(card.previewWidget(), QStringLiteral("Push page"));
    Button* popButton = buttonWithText(card.previewWidget(), QStringLiteral("Pop page"));
    ASSERT_NE(pushButton, nullptr);
    ASSERT_NE(popButton, nullptr);

    EXPECT_EQ(popButton->x() - (pushButton->x() + pushButton->width()),
              expectedButtonRowSpacing(8));
}

TEST_F(GalleryContentPagesTest, ButtonLikeSampleRowsPreserveRequestedSpacing)
{
    struct SampleCase {
        QString route;
        QString id;
        int buttonCount;
        int spacing;
    };

    const QVector<SampleCase> cases = {
        {QStringLiteral("button"), QStringLiteral("button-styles"), 3, 10},
        {QStringLiteral("button"), QStringLiteral("button-sizes"), 3, 10},
        {QStringLiteral("button"), QStringLiteral("button-icon-layouts"), 3, 10},
        {QStringLiteral("button"), QStringLiteral("button-interaction-state"), 4, 10},
        {QStringLiteral("repeat-button"), QStringLiteral("repeat-button-timing"), 2, 10},
        {QStringLiteral("split-button"), QStringLiteral("split-button-sizes"), 3, 10},
    };

    for (const SampleCase& sampleCase : cases) {
        fluent::gallery::GallerySample sample;
        ASSERT_TRUE(findSampleById(sampleCase.route, sampleCase.id, &sample))
            << sampleCase.id.toStdString();
        ASSERT_TRUE(static_cast<bool>(sample.createPreview)) << sampleCase.id.toStdString();

        GallerySampleCard card(sample);
        card.resize(720, card.sizeHint().height());
        card.show();
        QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QApplication::processEvents();

        QWidget* preview = card.previewWidget();
        ASSERT_NE(preview, nullptr) << sampleCase.id.toStdString();

        const QList<Button*> buttons = directButtonsLeftToRight(preview);
        ASSERT_EQ(buttons.size(), sampleCase.buttonCount) << sampleCase.id.toStdString();
        for (int i = 0; i + 1 < buttons.size(); ++i) {
            EXPECT_EQ(horizontalGapInAncestor(buttons.at(i), buttons.at(i + 1), preview),
                      expectedButtonRowSpacing(sampleCase.spacing))
                << sampleCase.id.toStdString() << " pair " << i;
        }
    }
}

TEST_F(GalleryContentPagesTest, StackViewTransitionButtonsUseRequestedSpacing)
{
    fluent::gallery::GallerySample sample;
    ASSERT_TRUE(findSampleById(QStringLiteral("stack-view"),
                               QStringLiteral("stack-view-transition-type"),
                               &sample));
    ASSERT_TRUE(static_cast<bool>(sample.createPreview));

    GallerySampleCard card(sample);
    card.resize(640, card.sizeHint().height());
    card.show();
    QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QApplication::processEvents();

    QWidget* preview = card.previewWidget();
    ASSERT_NE(preview, nullptr);

    const QVector<QString> labels = {
        QStringLiteral("ScaleFade"),
        QStringLiteral("SlideFade"),
        QStringLiteral("Push"),
        QStringLiteral("Pop"),
    };
    QVector<Button*> buttons;
    for (const QString& label : labels) {
        Button* button = buttonWithText(preview, label);
        ASSERT_NE(button, nullptr) << label.toStdString();
        buttons.append(button);
    }

    for (int i = 0; i + 1 < buttons.size(); ++i)
        EXPECT_EQ(horizontalGapInAncestor(buttons.at(i), buttons.at(i + 1), preview),
                  expectedButtonRowSpacing(8))
            << "pair " << i;
}

// Regression: the TreeView "Selection indicator motion" sample shares one left-aligned group with a
// controls row whose status label reads "Transition: <none|inward|outward|same level>". The collections
// makeStatusLabel sets no width floor, so without a reservation the label resizes with the text, the
// group (and the tree filling it) resizes too, and the tree's translucent backdrop visibly jumps on every
// selection. The fix reserves the longest transition text's width up front. zh_CN: TreeView「选择指示器动效」
// 示例的 tree 与控制行同处一个左对齐 group,状态标签随过渡文案变宽变窄,若不预留最长文案宽度,group(及填满它的 tree)
// 会随之伸缩,tree 半透明背景在每次选择时跳动。修复为预留最长过渡文案的宽度。
TEST_F(GalleryContentPagesTest, TreeViewIndicatorMotionStatusLabelReservesLongestWidth)
{
    const auto samples = fluent::gallery::gallerySamplesForRoute(QStringLiteral("tree-view"));
    const fluent::gallery::GallerySample* sample = nullptr;
    for (const auto& candidate : samples) {
        if (candidate.id == QStringLiteral("tree-view-indicator-motion")) {
            sample = &candidate;
            break;
        }
    }
    ASSERT_NE(sample, nullptr);
    ASSERT_TRUE(static_cast<bool>(sample->createPreview));

    GallerySampleCard card(*sample);
    card.resize(640, card.sizeHint().height());
    card.show();
    QApplication::processEvents();

    // Find the "Transition: ..." status label.
    QLabel* status = nullptr;
    for (QLabel* label : card.findChildren<QLabel*>()) {
        if (label->text().startsWith(QStringLiteral("Transition:"))) {
            status = label;
            break;
        }
    }
    ASSERT_NE(status, nullptr);
    auto* tree = card.findChild<TreeView*>();
    ASSERT_NE(tree, nullptr);

    const QStringList transitions{
        QStringLiteral("Transition: none"), QStringLiteral("Transition: inward"),
        QStringLiteral("Transition: outward"), QStringLiteral("Transition: same level")};

    // Font-independent fix postcondition: every transition text fits within the reserved floor, so the
    // label never grows the shared row.
    for (const QString& text : transitions) {
        status->setText(text);
        EXPECT_GE(status->minimumWidth(), status->sizeHint().width())
            << text.toStdString();
    }

    // End-to-end: cycling the status text (what a selection does) must not change the tree's width.
    auto settledTreeWidth = [&]() {
        QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        if (card.layout())
            card.layout()->activate();
        QApplication::processEvents();
        return tree->width();
    };
    status->setText(transitions.first());
    const int baselineWidth = settledTreeWidth();
    for (const QString& text : transitions) {
        status->setText(text);
        EXPECT_EQ(settledTreeWidth(), baselineWidth) << text.toStdString();
    }
}

TEST_F(GalleryContentPagesTest, TreeViewIndicatorTargetsDoNotAutoScrollThePreview)
{
    fluent::gallery::GallerySample sample;
    ASSERT_TRUE(findSampleById(QStringLiteral("tree-view"),
                               QStringLiteral("tree-view-indicator-motion"),
                               &sample));

    GallerySampleCard card(sample);
    card.resize(640, card.sizeHint().height());
    card.show();
    QApplication::processEvents();

    auto* tree = card.findChild<TreeView*>();
    ASSERT_NE(tree, nullptr);
    ASSERT_NE(tree->verticalScrollBar(), nullptr);
    tree->verticalScrollBar()->setValue(tree->verticalScrollBar()->minimum());
    QApplication::processEvents();
    const int baseline = tree->verticalScrollBar()->value();

    for (const QString& caption : {QStringLiteral("Parent"),
                                   QStringLiteral("Child"),
                                   QStringLiteral("Sibling")}) {
        Button* button = buttonWithText(&card, caption);
        ASSERT_NE(button, nullptr) << caption.toStdString();
        button->click();
        QApplication::processEvents();
        EXPECT_EQ(tree->verticalScrollBar()->value(), baseline)
            << caption.toStdString();
    }
}

TEST_F(GalleryContentPagesTest, SampleCardRefreshesWhenPreviewSizeHintChanges)
{
    ResizablePreview* preview = nullptr;

    fluent::gallery::GallerySample sample;
    sample.id = QStringLiteral("dynamic-preview");
    sample.title = QStringLiteral("Dynamic preview");
    sample.description = QStringLiteral("Preview content can request a taller card.");
    sample.createPreview = [&preview](QWidget* parent) {
        preview = new ResizablePreview(parent);
        return preview;
    };

    GallerySampleCard card(sample);
    card.resize(640, card.sizeHint().height());
    card.show();
    QApplication::processEvents();

    auto* previewSurface = card.findChild<QWidget*>(QStringLiteral("gallerySampleCardPreview"));
    ASSERT_NE(preview, nullptr);
    ASSERT_NE(previewSurface, nullptr);
    const int initialPreviewSurfaceHeight = previewSurface->height();
    const int initialCardHeight = card.height();

    preview->setPreferredHeight(120);
    QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QApplication::processEvents();
    QApplication::processEvents();

    EXPECT_GT(previewSurface->height(), initialPreviewSurfaceHeight);
    EXPECT_GT(card.height(), initialCardHeight);
}

TEST_F(GalleryContentPagesTest, TextEditSampleReflowsAfterVisibleLineGrowth)
{
    GalleryWindow window;
    window.resize(1180, 760);
    ASSERT_TRUE(window.selectRoute(QStringLiteral("text-edit")));
    window.show();
    QApplication::processEvents();

    auto* page = waitForCurrentPage<GalleryComponentPage>(window);
    ASSERT_NE(page, nullptr);
    GallerySampleCard* card = sampleCardById(page, QStringLiteral("text-edit-visible-lines"));
    ASSERT_NE(card, nullptr);
    ASSERT_NE(card->previewWidget(), nullptr);

    auto* textEdit = card->previewWidget()->findChild<TextEdit*>();
    ASSERT_NE(textEdit, nullptr);
    auto* statusLabel = card->previewWidget()->findChild<fluent::textfields::Label*>(
        QString(), Qt::FindDirectChildrenOnly);
    if (!statusLabel || !statusLabel->text().startsWith(QStringLiteral("Lines:"))) {
        statusLabel = nullptr;
        for (auto* label : card->previewWidget()->findChildren<fluent::textfields::Label*>()) {
            if (label->text().startsWith(QStringLiteral("Lines:"))) {
                statusLabel = label;
                break;
            }
        }
    }
    ASSERT_NE(statusLabel, nullptr);

    const int initialCardHeight = card->height();
    textEdit->setPlainText(QStringLiteral("First line\nSecond line\n\n3123"));
    QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QApplication::processEvents();
    QApplication::processEvents();

    EXPECT_GT(card->height(), initialCardHeight);
    const QRect editRect = mappedRectInAncestor(textEdit, card);
    const QRect statusRect = mappedRectInAncestor(statusLabel, card);
    EXPECT_GT(statusRect.top(), editRect.bottom());
}

// Task 6.4: TreeView and TabView samples produce live hosted preview widgets.
TEST_F(GalleryContentPagesTest, CollectionAndNavigationSamplesHostLivePreviews)
{
    GalleryWindow window;

    ASSERT_TRUE(window.selectRoute(QStringLiteral("tree-view")));
    auto* treePage = waitForCurrentPage<GalleryComponentPage>(window);
    ASSERT_NE(treePage, nullptr);
    ASSERT_GE(treePage->sampleCount(), 1);
    EXPECT_NE(treePage->sampleCards().first()->previewWidget(), nullptr);

    ASSERT_TRUE(window.selectRoute(QStringLiteral("tab-view")));
    auto* tabPage = waitForCurrentPage<GalleryComponentPage>(window);
    ASSERT_NE(tabPage, nullptr);
    ASSERT_GE(tabPage->sampleCount(), 1);
    EXPECT_NE(tabPage->sampleCards().first()->previewWidget(), nullptr);
}

// Task 6.6: content page and sample card refresh their surfaces on theme change.
TEST_F(GalleryContentPagesTest, ContentPageAndSampleCardRefreshOnThemeChange)
{
    GalleryWindow window;
    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    auto* page = waitForCurrentPage<GalleryComponentPage>(window);
    ASSERT_NE(page, nullptr);
    ASSERT_GE(page->sampleCount(), 1);
    GallerySampleCard* card = page->sampleCards().first();
    ASSERT_NE(card, nullptr);
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    page->onThemeUpdated();
    card->onThemeUpdated();
    // The page remains transparent so NavigationView's Mica-backed content frame shows through;
    // opaque cards still refresh to the dark layer token (#2C2C2C).
    EXPECT_FALSE(page->autoFillBackground());
    EXPECT_TRUE(page->styleSheet().contains(QStringLiteral("background: transparent")));
    ASSERT_NE(page->titleLabel(), nullptr);
    EXPECT_TRUE(page->titleLabel()->styleSheet().contains(QStringLiteral("rgba(255, 255, 255, 255)")));
    EXPECT_TRUE(card->styleSheet().contains(QStringLiteral("rgba(44, 44, 44, 255)")));

    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    page->onThemeUpdated();
    card->onThemeUpdated();
    EXPECT_FALSE(page->autoFillBackground());
    EXPECT_TRUE(page->styleSheet().contains(QStringLiteral("background: transparent")));
    EXPECT_TRUE(page->titleLabel()->styleSheet().contains(QStringLiteral("rgba(0, 0, 0, 230)")));
    EXPECT_TRUE(card->styleSheet().contains(QStringLiteral("rgba(255, 255, 255, 255)")));
}

TEST_F(GalleryContentPagesTest, ComponentThemeButtonSwitchesOnlySamplePreviewTheme)
{
    GalleryWindow window;
    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    GalleryComponentPage* page = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (page = dynamic_cast<GalleryComponentPage*>(window.currentContentPage())) != nullptr,
        1000);
    ASSERT_NE(page, nullptr);
    ASSERT_GE(page->sampleCards().size(), 1);

    auto* themeButton = page->findChild<Button*>(
        QStringLiteral("galleryComponentPageThemeButton"));
    ASSERT_NE(themeButton, nullptr);
    EXPECT_EQ(themeButton->property("gallerySampleTheme").toString(), QStringLiteral("Light"));

    GallerySampleCard* card = page->sampleCards().first();
    ASSERT_NE(card, nullptr);
    auto* previewSurface = card->findChild<QWidget*>(
        QStringLiteral("gallerySampleCardPreview"));
    ASSERT_NE(previewSurface, nullptr);
    EXPECT_FALSE(previewSurface->property("fluentThemeOverride").isValid());
    EXPECT_TRUE(card->styleSheet().contains(QStringLiteral("rgba(255, 255, 255, 255)")));

    auto* sampleButton = previewSurface->findChild<Button*>();
    ASSERT_NE(sampleButton, nullptr);
    EXPECT_EQ(sampleButton->effectiveTheme(), fluent::FluentElement::Light);

    QTest::mouseClick(themeButton, Qt::LeftButton);
    QApplication::processEvents();

    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Light);
    EXPECT_EQ(page->titleLabel()->effectiveTheme(), fluent::FluentElement::Light);
    EXPECT_EQ(themeButton->property("gallerySampleTheme").toString(), QStringLiteral("Dark"));
    EXPECT_EQ(previewSurface->property("fluentThemeOverride").toInt(),
              static_cast<int>(fluent::FluentElement::Dark));
    EXPECT_TRUE(previewSurface->styleSheet().contains(QStringLiteral("rgba(61, 61, 61, 255)")));
    EXPECT_TRUE(card->styleSheet().contains(QStringLiteral("rgba(255, 255, 255, 255)")));
    EXPECT_EQ(sampleButton->effectiveTheme(), fluent::FluentElement::Dark);
}

TEST_F(GalleryContentPagesTest, ComponentThemeButtonUpdatesTreeViewPreviewTheme)
{
    GalleryWindow window;
    ASSERT_TRUE(window.selectRoute(QStringLiteral("tree-view")));
    GalleryComponentPage* page = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (page = dynamic_cast<GalleryComponentPage*>(window.currentContentPage())) != nullptr,
        1000);
    ASSERT_NE(page, nullptr);

    auto* themeButton = page->findChild<Button*>(
        QStringLiteral("galleryComponentPageThemeButton"));
    ASSERT_NE(themeButton, nullptr);
    EXPECT_EQ(themeButton->property("gallerySampleTheme").toString(), QStringLiteral("Light"));

    auto* treeView = page->findChild<TreeView*>();
    ASSERT_NE(treeView, nullptr);
    EXPECT_EQ(treeView->effectiveTheme(), fluent::FluentElement::Light);

    QTest::mouseClick(themeButton, Qt::LeftButton);
    QApplication::processEvents();

    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Light);
    EXPECT_EQ(themeButton->property("gallerySampleTheme").toString(), QStringLiteral("Dark"));
    EXPECT_EQ(treeView->effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(treeView->themeColors().bgLayer, QColor("#2C2C2C"));
    EXPECT_EQ(page->titleLabel()->effectiveTheme(), fluent::FluentElement::Light);

    QTest::mouseClick(themeButton, Qt::LeftButton);
    QApplication::processEvents();

    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Light);
    EXPECT_EQ(themeButton->property("gallerySampleTheme").toString(), QStringLiteral("Light"));
    EXPECT_EQ(treeView->effectiveTheme(), fluent::FluentElement::Light);
    EXPECT_EQ(treeView->themeColors().bgLayer, QColor("#FFFFFF"));
    EXPECT_EQ(page->titleLabel()->effectiveTheme(), fluent::FluentElement::Light);
}

TEST_F(GalleryContentPagesTest, NavigationViewDisplayModeButtonsKeepContentScrollPosition)
{
    GalleryWindow window;
    window.resize(1180, 760);
    ASSERT_TRUE(window.selectRoute(QStringLiteral("navigation-view")));
    window.show();
    QApplication::processEvents();

    auto* page = waitForCurrentPage<GalleryComponentPage>(window);
    ASSERT_NE(page, nullptr);
    auto* scrollView = page->findChild<fluent::scrolling::ScrollView*>(
        QStringLiteral("galleryContentScrollArea"));
    ASSERT_NE(scrollView, nullptr);
    ASSERT_NE(scrollView->verticalScrollBar(), nullptr);

    GallerySampleCard* card = sampleCardById(
        page, QStringLiteral("navigation-view-display-modes"));
    ASSERT_NE(card, nullptr);
    ASSERT_NE(card->previewWidget(), nullptr);

    const int cardTop = card->mapTo(scrollView->widget(), QPoint(0, 0)).y();
    scrollView->verticalScrollBar()->setValue(
        qBound(scrollView->verticalScrollBar()->minimum(),
               cardTop - 28,
               scrollView->verticalScrollBar()->maximum()));
    QApplication::processEvents();

    const QStringList modeButtons{
        QStringLiteral("Compact"),
        QStringLiteral("Minimal"),
        QStringLiteral("Top"),
        QStringLiteral("Left")
    };

    for (const QString& buttonText : modeButtons) {
        Button* button = buttonWithText(card->previewWidget(), buttonText);
        ASSERT_NE(button, nullptr) << buttonText.toStdString();
        const int before = scrollView->verticalScrollBar()->value();
        QTest::mouseClick(button, Qt::LeftButton, Qt::NoModifier,
                          button->rect().center());
        QTest::qWait(360);
        QApplication::processEvents();
        EXPECT_LE(qAbs(scrollView->verticalScrollBar()->value() - before), 2)
            << buttonText.toStdString();
        EXPECT_LT(scrollView->verticalScrollBar()->value(),
                  scrollView->verticalScrollBar()->maximum())
            << buttonText.toStdString();
    }
}

TEST_F(GalleryContentPagesTest, ContentScrollSurfaceStaysTransparentAcrossThemeRefresh)
{
    GalleryContentPage page(QStringLiteral("test"), QStringLiteral("Test"));
    auto* scrollView = page.findChild<fluent::scrolling::ScrollView*>(
        QStringLiteral("galleryContentScrollArea"));
    ASSERT_NE(scrollView, nullptr);
    ASSERT_NE(scrollView->viewport(), nullptr);

    EXPECT_FALSE(scrollView->viewport()->autoFillBackground());
    EXPECT_FALSE(scrollView->viewport()->testAttribute(Qt::WA_TranslucentBackground));

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    QApplication::processEvents();

    EXPECT_FALSE(scrollView->viewport()->autoFillBackground());
    EXPECT_FALSE(scrollView->viewport()->testAttribute(Qt::WA_TranslucentBackground));
}

// The "Source code" block starts collapsed and toggles its code + copy affordance.
TEST_F(GalleryContentPagesTest, CodeBlockCollapsesAndExpands)
{
    GalleryCodeBlock block(QStringLiteral("auto* button = makeButton();"));
    block.resize(520, block.sizeHint().height());
    block.show();
    QApplication::processEvents();

    auto* header = block.findChild<QWidget*>(QStringLiteral("galleryCodeBlockHeader"));
    auto* content = block.findChild<QWidget*>(QStringLiteral("galleryCodeBlockContent"));
    auto* copyButton = block.findChild<QWidget*>(QStringLiteral("galleryCodeBlockCopyButton"));
    ASSERT_NE(header, nullptr);
    ASSERT_NE(content, nullptr);
    ASSERT_NE(copyButton, nullptr);
    // Copy now lives inside the collapsible content (top-right of the code area), so it is
    // revealed/clipped together with the code rather than fading independently.
    // zh_CN: Copy 现在位于可折叠内容里（代码区右上角），随代码一起被揭示/裁剪，而非独立淡入淡出。
    EXPECT_EQ(copyButton->parentWidget()->objectName(), QStringLiteral("galleryCodeBlockContentInner"));

    // Collapsed by default: the code area remains in the layout but is clipped to zero height.
    // zh_CN: 默认折叠时内容区保留在布局中，但被裁剪到 0 高，避免 show/hide 带来的布局抖动。
    EXPECT_FALSE(block.isExpanded());
    EXPECT_FALSE(content->isHidden());
    EXPECT_EQ(content->height(), 0);
    const QRect collapsedHeaderGeometry = header->geometry();

    // Expanding (non-animated, for determinism) reveals the code.
    block.setExpanded(true, /*animated=*/false);
    block.resize(520, block.sizeHint().height());
    QApplication::processEvents();
    EXPECT_TRUE(block.isExpanded());
    EXPECT_FALSE(content->isHidden());
    EXPECT_GT(content->height(), 0);
    EXPECT_EQ(content->minimumHeight(), content->maximumHeight());
    EXPECT_EQ(header->geometry(), collapsedHeaderGeometry);
    EXPECT_EQ(content->y(), header->geometry().bottom() + 1);

    // Collapsing clips the code again without removing the content widget from the layout.
    block.setExpanded(false, /*animated=*/false);
    block.resize(520, block.sizeHint().height());
    QApplication::processEvents();
    EXPECT_FALSE(block.isExpanded());
    EXPECT_FALSE(content->isHidden());
    EXPECT_EQ(content->height(), 0);
    EXPECT_EQ(header->geometry(), collapsedHeaderGeometry);

    // toggleExpanded flips the state.
    block.toggleExpanded();
    EXPECT_TRUE(block.isExpanded());
}

TEST_F(GalleryContentPagesTest, CodeBlockUsesBodySizedNativeMonospaceFont)
{
    GalleryCodeBlock block(QStringLiteral("auto value = compute();"));
    auto* code = block.findChild<QLabel*>(QStringLiteral("galleryCodeBlockText"));
    ASSERT_NE(code, nullptr);
    EXPECT_EQ(code->font().family(),
              QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
    EXPECT_EQ(code->font().pixelSize(), Typography::FontSize::Body);
}

TEST_F(GalleryContentPagesTest, CodeBlockExpansionKeepsFoundationPageGeometryStable)
{
    GalleryWindow window;
    window.resize(1200, 790);
    ASSERT_TRUE(window.selectRoute(QStringLiteral("foundation-geometry")));
    window.show();
    QApplication::processEvents();

    auto* page = waitForCurrentPage<GalleryFoundationTopicPage>(window);
    ASSERT_NE(page, nullptr);
    auto* codeBlock = page->findChild<GalleryCodeBlock*>();
    ASSERT_NE(codeBlock, nullptr);
    auto* codeHeader = codeBlock->findChild<QWidget*>(
        QStringLiteral("galleryCodeBlockHeader"));
    ASSERT_NE(codeHeader, nullptr);
    auto* scrollView = page->findChild<fluent::scrolling::ScrollView*>();
    ASSERT_NE(scrollView, nullptr);
    ASSERT_NE(scrollView->viewport(), nullptr);
    QWidget* scrollContent = scrollView->widget();
    ASSERT_NE(scrollContent, nullptr);
    QLayout* pageLayout = scrollContent->layout();
    ASSERT_NE(pageLayout, nullptr);

    fluent::textfields::Label* cornerHeader = nullptr;
    fluent::textfields::Label* strokeHeader = nullptr;
    for (auto* label : page->findChildren<fluent::textfields::Label*>(
             QStringLiteral("galleryContentSectionHeader"))) {
        if (label->text() == QStringLiteral("Corner radius"))
            cornerHeader = label;
        else if (label->text() == QStringLiteral("Stroke widths"))
            strokeHeader = label;
    }
    ASSERT_NE(cornerHeader, nullptr);
    ASSERT_NE(strokeHeader, nullptr);

    QWidget* radiusCard = nullptr;
    for (int i = 0; i + 1 < pageLayout->count(); ++i) {
        if (pageLayout->itemAt(i)->widget() != cornerHeader)
            continue;
        for (int candidate = i + 1; candidate < pageLayout->count(); ++candidate) {
            if (QWidget* widget = pageLayout->itemAt(candidate)->widget()) {
                radiusCard = widget;
                break;
            }
        }
        break;
    }
    ASSERT_NE(radiusCard, nullptr);

    const QRect radiusGeometry = radiusCard->geometry();
    const QRect strokeGeometry = strokeHeader->geometry();
    const auto codeHeaderViewportY = [&]() {
        return scrollView->viewport()->mapFromGlobal(
            codeHeader->mapToGlobal(QPoint(0, 0))).y();
    };
    const int anchoredHeaderY = codeHeaderViewportY();

    QVector<int> sampledRadiusHeights;
    QVector<int> sampledStrokeTops;
    QVector<int> sampledHeaderYs;
    QVector<int> sampledContentDeficits;
    const auto captureGeometry = [&]() {
        sampledRadiusHeights.append(radiusCard->height());
        sampledStrokeTops.append(strokeHeader->y());
        sampledHeaderYs.append(codeHeaderViewportY());
        const int requiredHeight = qMax(scrollView->viewport()->height(),
                                        pageLayout->minimumSize().height());
        sampledContentDeficits.append(requiredHeight - scrollContent->height());
    };

    int finishedTransitions = 0;
    QObject::connect(codeBlock, &GalleryCodeBlock::expansionTransitionFinished, &window,
                     [&finishedTransitions]() { ++finishedTransitions; });
    const auto waitForTransition = [&](int expectedCount) {
        QElapsedTimer timer;
        timer.start();
        while (finishedTransitions < expectedCount && timer.elapsed() < 1000) {
            QApplication::processEvents(QEventLoop::AllEvents, 5);
            captureGeometry();
            QTest::qWait(2);
        }
        QApplication::processEvents(QEventLoop::AllEvents, 5);
        QTest::qWait(2);
        QApplication::processEvents(QEventLoop::AllEvents, 5);
        captureGeometry();
        ASSERT_EQ(finishedTransitions, expectedCount);
    };

    codeBlock->setExpanded(true);
    waitForTransition(1);
    codeBlock->setExpanded(false);
    waitForTransition(2);

    ASSERT_GE(sampledRadiusHeights.size(), 8);
    for (int height : sampledRadiusHeights)
        EXPECT_EQ(height, radiusGeometry.height());
    for (int top : sampledStrokeTops)
        EXPECT_EQ(top, strokeGeometry.top());
    for (int headerY : sampledHeaderYs)
        EXPECT_NEAR(headerY, anchoredHeaderY, 1);
    for (int deficit : sampledContentDeficits)
        EXPECT_LE(deficit, 0);
    EXPECT_EQ(radiusCard->geometry(), radiusGeometry);
    EXPECT_EQ(strokeHeader->geometry(), strokeGeometry);
}

TEST_F(GalleryContentPagesTest, CodeBlockExpansionKeepsSampleChromeStable)
{
    GalleryWindow window;
    window.resize(1180, 760);
    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    window.show();
    QApplication::processEvents();

    auto* page = waitForCurrentPage<GalleryComponentPage>(window);
    ASSERT_NE(page, nullptr);
    ASSERT_GE(page->sampleCards().size(), 1);

    GallerySampleCard* card = page->sampleCards().last();
    ASSERT_NE(card, nullptr);
    EXPECT_NE(qobject_cast<fluent::AnchorLayout*>(card->layout()), nullptr);
    ASSERT_NE(card->titleLabel(), nullptr);
    auto* preview = card->findChild<QWidget*>(QStringLiteral("gallerySampleCardPreview"));
    ASSERT_NE(preview, nullptr);
    GalleryCodeBlock* codeBlock = card->codeBlock();
    ASSERT_NE(codeBlock, nullptr);
    auto* content = codeBlock->findChild<QWidget*>(QStringLiteral("galleryCodeBlockContent"));
    ASSERT_NE(content, nullptr);
    auto* contentInner = codeBlock->findChild<QWidget*>(QStringLiteral("galleryCodeBlockContentInner"));
    ASSERT_NE(contentInner, nullptr);
    auto* header = codeBlock->findChild<QWidget*>(QStringLiteral("galleryCodeBlockHeader"));
    ASSERT_NE(header, nullptr);
    auto* scrollView = page->findChild<fluent::scrolling::ScrollView*>();
    ASSERT_NE(scrollView, nullptr);
    QScrollBar* verticalBar = scrollView->verticalScrollBar();
    ASSERT_NE(verticalBar, nullptr);

    QWidget* followingWidget = nullptr;
    QLayout* pageLayout = card->parentWidget() ? card->parentWidget()->layout() : nullptr;
    ASSERT_NE(pageLayout, nullptr);
    int cardIndex = -1;
    for (int i = 0; i < pageLayout->count(); ++i) {
        if (pageLayout->itemAt(i)->widget() == card) {
            cardIndex = i;
            break;
        }
    }
    ASSERT_GE(cardIndex, 0);
    for (int i = cardIndex + 1; i < pageLayout->count(); ++i) {
        QWidget* candidate = pageLayout->itemAt(i)->widget();
        if (candidate && !candidate->isHidden()) {
            followingWidget = candidate;
            break;
        }
    }
    ASSERT_NE(followingWidget, nullptr);

    // Reproduce the user-visible case: the page is at its old maximum when the
    // final source block starts growing. Its header must stay under the pointer.
    // zh_CN: 复现页面位于旧最大滚动值时展开末尾源码块的场景，标题需保持在指针下方。
    verticalBar->setValue(verticalBar->maximum());
    QApplication::processEvents();
    const int anchoredScrollValue = verticalBar->value();
    EXPECT_GT(anchoredScrollValue, 0);

    const auto headerViewportY = [&]() {
        return scrollView->viewport()->mapFromGlobal(
            header->mapToGlobal(QPoint(0, 0))).y();
    };
    const int anchoredHeaderY = headerViewportY();
    const int followingGap = followingWidget->geometry().top()
        - (card->geometry().bottom() + 1);

    const QRect titleGeometry = card->titleLabel()->geometry();
    const QRect previewGeometry = preview->geometry();
    const QRect collapsedCodeGeometry = codeBlock->geometry();

    QVector<int> sampledHeaderYs;
    QVector<int> sampledBlockHeights;
    QVector<int> sampledCardHeights;
    QVector<int> sampledFollowingGaps;
    auto capturePaintableGeometry = [&]() {
        sampledHeaderYs.append(headerViewportY());
        sampledFollowingGaps.append(followingWidget->geometry().top()
                                    - (card->geometry().bottom() + 1));
    };
    auto captureAnimationHeight = [&]() {
        sampledBlockHeights.append(codeBlock->height());
        sampledCardHeights.append(card->height());
    };
    class PaintGeometryProbe final : public QObject {
    public:
        std::function<void()> capture;

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event && event->type() == QEvent::Paint && capture)
                capture();
            return QObject::eventFilter(watched, event);
        }
    } paintProbe;
    paintProbe.capture = capturePaintableGeometry;
    card->installEventFilter(&paintProbe);
    followingWidget->installEventFilter(&paintProbe);
    const auto samplesText = [](const QVector<int>& samples) {
        QStringList values;
        values.reserve(samples.size());
        for (int value : samples)
            values.append(QString::number(value));
        return values.join(QLatin1Char(',')).toStdString();
    };
    QObject::connect(codeBlock, &GalleryCodeBlock::layoutHeightChanged, &window,
                     [&captureAnimationHeight]() { captureAnimationHeight(); });

    int finishedTransitions = 0;
    QObject::connect(codeBlock, &GalleryCodeBlock::expansionTransitionFinished, &window,
                     [&finishedTransitions]() { ++finishedTransitions; });
    const auto waitForTransition = [&]() {
        QElapsedTimer timer;
        timer.start();
        while (finishedTransitions == 0 && timer.elapsed() < 1000) {
            QApplication::processEvents(QEventLoop::AllEvents, 5);
            capturePaintableGeometry();
            QTest::qWait(2);
        }
        QApplication::processEvents(QEventLoop::AllEvents, 5);
        QTest::qWait(2);  // run the card's queued final anchor correction
        QApplication::processEvents(QEventLoop::AllEvents, 5);
        capturePaintableGeometry();
        ASSERT_EQ(finishedTransitions, 1);
    };

    codeBlock->setExpanded(true);
    const int targetContentHeight = contentInner->height();
    EXPECT_GT(targetContentHeight, 0);
    waitForTransition();

    ASSERT_GE(sampledHeaderYs.size(), 4);
    for (int value : sampledHeaderYs)
        EXPECT_NEAR(value, anchoredHeaderY, 1);
    for (int gap : sampledFollowingGaps)
        EXPECT_EQ(gap, followingGap)
            << "gaps=" << samplesText(sampledFollowingGaps)
            << " blockHeights=" << samplesText(sampledBlockHeights)
            << " cardHeights=" << samplesText(sampledCardHeights);
    EXPECT_TRUE(std::is_sorted(sampledBlockHeights.cbegin(), sampledBlockHeights.cend()))
        << "block heights must grow monotonically: " << samplesText(sampledBlockHeights);
    EXPECT_TRUE(std::is_sorted(sampledCardHeights.cbegin(), sampledCardHeights.cend()))
        << "card heights must grow monotonically: " << samplesText(sampledCardHeights);

    EXPECT_EQ(card->titleLabel()->geometry(), titleGeometry);
    EXPECT_EQ(preview->geometry(), previewGeometry);
    EXPECT_EQ(codeBlock->geometry().topLeft(), collapsedCodeGeometry.topLeft());
    EXPECT_EQ(codeBlock->geometry().width(), collapsedCodeGeometry.width());
    EXPECT_EQ(contentInner->geometry().topLeft(), QPoint(0, 0));
    EXPECT_EQ(contentInner->height(), targetContentHeight);
    EXPECT_GT(codeBlock->height(), collapsedCodeGeometry.height());
    EXPECT_EQ(verticalBar->value(), anchoredScrollValue);

    const int expandedBlockHeight = codeBlock->height();
    sampledHeaderYs.clear();
    sampledBlockHeights.clear();
    sampledCardHeights.clear();
    sampledFollowingGaps.clear();
    finishedTransitions = 0;
    codeBlock->setExpanded(false);
    waitForTransition();

    ASSERT_GE(sampledHeaderYs.size(), 4);
    for (int value : sampledHeaderYs)
        EXPECT_NEAR(value, anchoredHeaderY, 1);
    for (int gap : sampledFollowingGaps)
        EXPECT_EQ(gap, followingGap) << samplesText(sampledFollowingGaps);
    EXPECT_TRUE(std::is_sorted(sampledBlockHeights.crbegin(), sampledBlockHeights.crend()))
        << "block heights must shrink monotonically: " << samplesText(sampledBlockHeights);
    EXPECT_TRUE(std::is_sorted(sampledCardHeights.crbegin(), sampledCardHeights.crend()))
        << "card heights must shrink monotonically: " << samplesText(sampledCardHeights);
    EXPECT_LT(codeBlock->height(), expandedBlockHeight);
    EXPECT_EQ(codeBlock->height(), collapsedCodeGeometry.height());
    EXPECT_EQ(verticalBar->value(), anchoredScrollValue);
}

TEST_F(GalleryContentPagesTest, GalleryToastUsesOverlayMarginAndSuccessBadge)
{
    QWidget host;
    host.resize(800, 600);

    fluent::gallery::showGalleryToast(&host, QStringLiteral("Copied to clipboard"));

    auto* toast = host.findChild<QWidget*>(QStringLiteral("galleryToast"));
    ASSERT_NE(toast, nullptr);
    ASSERT_NE(toast->layout(), nullptr);
    EXPECT_EQ(toast->layout()->contentsMargins(),
              fluent::overlay::uniformShadowMargins());

    auto* card = toast->findChild<QFrame*>(QStringLiteral("galleryToastCard"));
    ASSERT_NE(card, nullptr);
    EXPECT_EQ(toast->size(),
              fluent::overlay::outerSizeForVisibleCard(card->sizeHint()));

    auto* icon = toast->findChild<QLabel*>(QStringLiteral("galleryToastIcon"));
    ASSERT_NE(icon, nullptr);
    EXPECT_EQ(icon->size(), QSize(26, 26));
    EXPECT_TRUE(icon->styleSheet().contains(QStringLiteral("background")));

    auto* opacity = qobject_cast<QGraphicsOpacityEffect*>(toast->graphicsEffect());
    ASSERT_NE(opacity, nullptr);
    opacity->setOpacity(1.0);

    QImage rendered(toast->size(), QImage::Format_ARGB32_Premultiplied);
    rendered.fill(Qt::transparent);
    toast->render(&rendered);

    const QRect cardRect = fluent::overlay::visibleCardRect(toast->rect());
    const auto alphaAt = [&rendered](const QPoint& point) {
        return QColor::fromRgba(rendered.pixel(point)).alpha();
    };
    const int topHaloAlpha = alphaAt(QPoint(cardRect.center().x(), cardRect.top() - 4));
    const int bottomShadowAlpha = alphaAt(QPoint(cardRect.center().x(), cardRect.bottom() + 8));
    EXPECT_LT(topHaloAlpha, 10);
    EXPECT_GT(bottomShadowAlpha, topHaloAlpha);
    EXPECT_LT(bottomShadowAlpha, 48);
}
