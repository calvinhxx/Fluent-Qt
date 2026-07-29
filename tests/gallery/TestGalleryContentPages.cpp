#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>

#include <QAction>
#include <QApplication>
#include <QAbstractScrollArea>
#include <QBoxLayout>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QEvent>
#include <QFrame>
#include <QFile>
#include <QFontDatabase>
#include <QGraphicsOpacityEffect>
#include <QImage>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMargins>
#include <QPoint>
#include <QPixmap>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStringList>
#include <QTest>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include "components/basicinput/Button.h"
#include "components/basicinput/ComboBox.h"
#include "components/basicinput/CompoundButton.h"
#include "compatibility/QtCompat.h"
#include "components/collections/TreeView.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/FontIcon.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/layout/Accordion.h"
#include "components/layout/Card.h"
#include "components/layout/Divider.h"
#include "components/layout/Expander.h"
#include "components/menus_toolbars/CommandBar.h"
#include "components/menus_toolbars/CommandBarFlyout.h"
#include "components/menus_toolbars/Menu.h"
#include "components/scrolling/PipsPager.h"
#include "components/scrolling/ScrollView.h"
#include "components/status_info/Avatar.h"
#include "components/status_info/ToolTip.h"
#include "components/status_info/Toast.h"
#include "components/textfields/EditingCommandRouter.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "components/textfields/TextEdit.h"
#include "design/Spacing.h"
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
using fluent::menus_toolbars::CommandBar;
using fluent::menus_toolbars::CommandBarFlyout;
using fluent::menus_toolbars::FluentMenu;
using fluent::textfields::EditingCommandRouter;
using fluent::textfields::LineEdit;
using fluent::textfields::TextEdit;
using fluent::basicinput::Button;
using fluent::basicinput::ComboBox;

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

bool actionUsesStandardKey(
    const QAction* action,
    QKeySequence::StandardKey key)
{
    if (!action)
        return false;
    const QList<QKeySequence> bindings =
        QKeySequence::keyBindings(key);
    for (const QKeySequence& shortcut : action->shortcuts()) {
        for (const QKeySequence& binding : bindings) {
            if (shortcut.matches(binding)
                == QKeySequence::ExactMatch) {
                return true;
            }
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

bool isContainedIn(const QWidget* child, const QWidget* parent, int tolerance = 0)
{
    if (!child || !parent)
        return false;
    const QRect bounds = parent->rect().adjusted(
        -tolerance, -tolerance, tolerance, tolerance);
    return bounds.contains(mappedRectInAncestor(child, parent));
}

fluent::FluentElement* firstFluentElement(QWidget* root)
{
    if (!root)
        return nullptr;
    if (auto* element = dynamic_cast<fluent::FluentElement*>(root))
        return element;
    for (QWidget* widget : root->findChildren<QWidget*>()) {
        if (auto* element = dynamic_cast<fluent::FluentElement*>(widget))
            return element;
    }
    return nullptr;
}

QWidget* firstFocusableWidget(QWidget* root)
{
    if (!root)
        return nullptr;
    QList<QWidget*> candidates{root};
    candidates.append(root->findChildren<QWidget*>());
    for (QWidget* candidate : candidates) {
        if (candidate && candidate->isEnabled() && candidate->isVisibleTo(root)
            && candidate->focusPolicy() != Qt::NoFocus) {
            return candidate;
        }
    }
    return nullptr;
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

TEST_F(GalleryContentPagesTest, FoundationLandingOrderMatchesNavigation)
{
    GalleryNavigationViewModel navigationViewModel;
    QStringList navigationRouteIds;
    for (const auto& item : navigationViewModel.items()) {
        if (item.parentId == QStringLiteral("foundation"))
            navigationRouteIds.append(item.id);
    }

    const auto* foundationEntry = galleryContentEntry(QStringLiteral("foundation"));
    ASSERT_NE(foundationEntry, nullptr);
    EXPECT_EQ(navigationRouteIds, foundationEntry->relatedRouteIds);
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
    QStringList referencedHeaders;
    for (const auto& category : galleryComponentCatalog()) {
        for (const auto& component : category.components) {
            const auto reference = galleryComponentReference(component.id);
            ASSERT_TRUE(reference.isValid()) << component.id.toStdString();
            EXPECT_TRUE(reference.header.startsWith(QStringLiteral("<FluentQt/")));
            EXPECT_TRUE(reference.header.endsWith(QStringLiteral(".h>")));
            EXPECT_NE(reference.header, QStringLiteral("<FluentQt/FluentQt.h>"));
            EXPECT_EQ(reference.cmakeTarget, QStringLiteral("FluentQt::FluentQt"));
            const QString expectedNamespace = component.apiNamespace.isEmpty()
                ? QStringLiteral("fluent::%1").arg(category.sourceDirectory)
                : component.apiNamespace;
            EXPECT_TRUE(reference.qualifiedType.startsWith(
                expectedNamespace + QStringLiteral("::")));
            referencedHeaders.append(reference.header);
        }
    }
    referencedHeaders.removeDuplicates();
    EXPECT_EQ(referencedHeaders.size(), galleryComponentCatalog().size());

    EXPECT_EQ(galleryComponentReference(QStringLiteral("menu")).qualifiedType,
              QStringLiteral("fluent::menus_toolbars::FluentMenu"));
    EXPECT_EQ(galleryComponentReference(QStringLiteral("font-icon")).qualifiedType,
              QStringLiteral("fluent::FontIcon"));
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

TEST_F(GalleryContentPagesTest, GalleryAcceptanceMatrixCoversEveryComponentRoute)
{
    auto& settings = fluent::gallery::GallerySettings::instance();
    settings.setIntroCompleted(true);

    GalleryWindow window;
    window.setBackdropEffect(fluent::windowing::BackdropEffect::Solid);
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();

    int reviewedRoutes = 0;
    int focusableRoutes = 0;
    for (const auto& category : galleryComponentCatalog()) {
        for (const auto& component : category.components) {
            SCOPED_TRACE(QStringLiteral("route=%1").arg(component.id).toStdString());
            ASSERT_TRUE(window.selectRoute(component.id));
            auto* page = waitForCurrentPage<GalleryComponentPage>(window);
            ASSERT_NE(page, nullptr);
            ASSERT_FALSE(page->sampleCards().isEmpty());

            for (GallerySampleCard* card : page->sampleCards()) {
                ASSERT_NE(card, nullptr);
                ASSERT_NE(card->previewWidget(), nullptr)
                    << card->sampleId().toStdString();
                card->setPreviewThemeOverride(fluent::FluentElement::Dark);
                card->previewWidget()->setLayoutDirection(Qt::RightToLeft);
                card->previewWidget()->setEnabled(false);
            }
            QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            QApplication::processEvents();

            for (GallerySampleCard* card : page->sampleCards()) {
                QWidget* preview = card->previewWidget();
                auto* surface = card->findChild<QWidget*>(
                    QStringLiteral("gallerySampleCardPreview"));
                ASSERT_NE(surface, nullptr) << card->sampleId().toStdString();
                EXPECT_EQ(preview->layoutDirection(), Qt::RightToLeft)
                    << card->sampleId().toStdString();
                EXPECT_FALSE(preview->isEnabled())
                    << card->sampleId().toStdString();
                EXPECT_TRUE(isContainedIn(preview, surface, 1))
                    << card->sampleId().toStdString();
                EXPECT_TRUE(isContainedIn(surface, card, 1))
                    << card->sampleId().toStdString();

                if (auto* element = firstFluentElement(preview))
                    EXPECT_EQ(element->effectiveTheme(), fluent::FluentElement::Dark)
                        << card->sampleId().toStdString();
            }

            GallerySampleCard* representative = page->sampleCards().first();
            const QPixmap darkRtlDisabled = representative->grab();
            ASSERT_FALSE(darkRtlDisabled.isNull());
            EXPECT_EQ(fluentPixmapLogicalSize(darkRtlDisabled),
                      representative->size());

            QWidget* focusTarget = nullptr;
            for (GallerySampleCard* card : page->sampleCards()) {
                card->clearPreviewThemeOverride();
                card->previewWidget()->setLayoutDirection(Qt::LeftToRight);
                card->previewWidget()->setEnabled(true);
                if (!focusTarget)
                    focusTarget = firstFocusableWidget(card->previewWidget());
            }
            QApplication::processEvents();
            if (focusTarget) {
                ++focusableRoutes;
                focusTarget->setFocus(Qt::TabFocusReason);
                QApplication::processEvents();
                QWidget* focused = QApplication::focusWidget();
                EXPECT_TRUE(focused == focusTarget
                            || (focused && focusTarget->isAncestorOf(focused))
                            || (focused && focused->isAncestorOf(focusTarget)));
            }
            ++reviewedRoutes;
        }
    }

    EXPECT_GT(reviewedRoutes, 50);
    EXPECT_GT(focusableRoutes, 30);
}

TEST_F(GalleryContentPagesTest, GalleryAcceptanceMatrixHonorsProcessScale)
{
    auto& settings = fluent::gallery::GallerySettings::instance();
    settings.setIntroCompleted(true);

    GalleryWindow window;
    window.setBackdropEffect(fluent::windowing::BackdropEffect::Solid);
    window.resize(1180, 760);
    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    window.show();
    QApplication::processEvents();

    auto* page = waitForCurrentPage<GalleryComponentPage>(window);
    ASSERT_NE(page, nullptr);
    GallerySampleCard* card = sampleCardById(
        page, QStringLiteral("button-interaction-state"));
    ASSERT_NE(card, nullptr);
    ASSERT_NE(card->previewWidget(), nullptr);

    card->setPreviewThemeOverride(fluent::FluentElement::Dark);
    card->previewWidget()->setLayoutDirection(Qt::RightToLeft);
    QApplication::processEvents();

    const QPixmap capture = window.grab();
    ASSERT_FALSE(capture.isNull());
    EXPECT_EQ(fluentPixmapLogicalSize(capture), window.size());
    bool hasRequestedScale = false;
    const qreal requestedScale =
        qEnvironmentVariable("QT_SCALE_FACTOR").toDouble(&hasRequestedScale);
    if (hasRequestedScale)
        EXPECT_NEAR(capture.devicePixelRatioF(), requestedScale, 0.01);
    else
        EXPECT_GE(capture.devicePixelRatioF(), 1.0);

    auto* surface = card->findChild<QWidget*>(
        QStringLiteral("gallerySampleCardPreview"));
    ASSERT_NE(surface, nullptr);
    EXPECT_TRUE(isContainedIn(card->previewWidget(), surface, 1));
    EXPECT_TRUE(isContainedIn(surface, card, 1));
}

TEST_F(GalleryContentPagesTest, TreeViewRtlCheckBoxHitTargetUsesLeadingEdge)
{
    fluent::gallery::GallerySample sample;
    ASSERT_TRUE(findSampleById(QStringLiteral("tree-view"),
                               QStringLiteral("tree-view-checkboxes"),
                               &sample));
    GallerySampleCard card(sample);
    card.resize(760, card.sizeHint().height());
    card.previewWidget()->setLayoutDirection(Qt::RightToLeft);
    card.show();
    QApplication::processEvents();

    auto* tree = qobject_cast<TreeView*>(card.previewWidget());
    if (!tree)
        tree = card.previewWidget()->findChild<TreeView*>();
    ASSERT_NE(tree, nullptr);
    ASSERT_NE(tree->model(), nullptr);

    const QModelIndex root = tree->model()->index(0, 0);
    ASSERT_TRUE(root.isValid());
    EXPECT_EQ(root.data(Qt::CheckStateRole).toInt(), int(Qt::PartiallyChecked));

    const QRect rowRect = tree->visualRect(root);
    ASSERT_FALSE(rowRect.isEmpty());
    constexpr int cursorStart = 12;
    constexpr int checkBoxHalfWidth = 11;
    const QPoint rtlCheckBoxCenter(
        rowRect.x() + rowRect.width() - cursorStart - checkBoxHalfWidth,
        rowRect.center().y());
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                      rtlCheckBoxCenter);
    QApplication::processEvents();

    EXPECT_EQ(root.data(Qt::CheckStateRole).toInt(), int(Qt::Checked));
}

TEST_F(GalleryContentPagesTest, ComponentStateMatrixVisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST"))
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Component state review requires a desktop platform";

    if (tests::support::shouldCaptureVisualSnapshot()) {
        fluent::gallery::GallerySample buttonSample;
        ASSERT_TRUE(findSampleById(QStringLiteral("button"),
                                   QStringLiteral("button-interaction-state"),
                                   &buttonSample));
        GallerySampleCard buttonCard(buttonSample);
        buttonCard.resize(760, buttonCard.sizeHint().height());
        QApplication::processEvents();

        tests::support::VisualSnapshotOptions options;
        options.windowSize = buttonCard.size();
        options.variant = QStringLiteral("button-states-light-ltr");
        options.theme = tests::support::VisualSnapshotTheme::Light;
        ASSERT_TRUE(tests::support::captureVisualSnapshot(&buttonCard, options));

        buttonCard.previewWidget()->setLayoutDirection(Qt::RightToLeft);
        options.variant = QStringLiteral("button-states-dark-rtl");
        options.theme = tests::support::VisualSnapshotTheme::Dark;
        ASSERT_TRUE(tests::support::captureVisualSnapshot(&buttonCard, options));

        buttonCard.previewWidget()->setEnabled(false);
        options.variant = QStringLiteral("button-states-dark-rtl-disabled");
        ASSERT_TRUE(tests::support::captureVisualSnapshot(&buttonCard, options));

        fluent::gallery::GallerySample treeSample;
        ASSERT_TRUE(findSampleById(QStringLiteral("tree-view"),
                                   QStringLiteral("tree-view-basic"),
                                   &treeSample));
        GallerySampleCard treeCard(treeSample);
        treeCard.resize(760, treeCard.sizeHint().height());
        treeCard.previewWidget()->setLayoutDirection(Qt::RightToLeft);
        QApplication::processEvents();
        options.windowSize = treeCard.size();
        options.variant = QStringLiteral("tree-view-dark-rtl");
        ASSERT_TRUE(tests::support::captureVisualSnapshot(&treeCard, options));
        return;
    }

    auto& settings = fluent::gallery::GallerySettings::instance();
    settings.setIntroCompleted(true);
    GalleryWindow window;
    window.setBackdropEffect(fluent::windowing::BackdropEffect::Solid);
    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    window.show();
    qApp->exec();
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

TEST_F(GalleryContentPagesTest,
       EntryGridExpandsOnlyRowsThatNeedWrappedDescriptions)
{
    const GalleryEntryGrid::Entry wrappedEntry{
        QStringLiteral("wrapped"),
        QStringLiteral("Wrapped"),
        QStringLiteral(
            "A deliberately long description that wraps across several "
            "lines in one card without stretching every later row in the "
            "catalog grid."),
        QPixmap(),
        QString()};
    const GalleryEntryGrid::Entry compactEntry{
        QStringLiteral("compact"),
        QStringLiteral("Compact"),
        QString(),
        QPixmap(),
        QString()};

    GalleryEntryGrid wrappedRow;
    wrappedRow.resize(1000, 100);
    wrappedRow.setEntries({wrappedEntry});
    const int wrappedRowHeight =
        wrappedRow.sizeHint().height();
    ASSERT_GT(wrappedRowHeight, 86);

    GalleryEntryGrid mixedRows;
    mixedRows.resize(1000, 100);
    mixedRows.setEntries(
        {wrappedEntry,
         compactEntry,
         compactEntry,
         compactEntry,
         compactEntry});

    EXPECT_EQ(
        mixedRows.sizeHint().height(),
        wrappedRowHeight + 12 + 86);
    EXPECT_LT(
        mixedRows.sizeHint().height(),
        wrappedRowHeight * 2 + 12);
}

TEST_F(GalleryContentPagesTest, ComponentCardsUseBundledImagesOrCatalogGlyphs)
{
    const QString placeholder =
        QStringLiteral(":/app/assets/control_images/Placeholder.png");

    for (const auto& category : galleryComponentCatalog()) {
        for (const auto& component : category.components) {
            const QString resource = galleryControlImageResource(component.title);
            if (resource.isEmpty()) {
                EXPECT_FALSE(component.iconGlyph.isEmpty())
                    << component.title.toStdString();
                continue;
            }
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
        {QStringLiteral("compound-button"), QStringLiteral("CompoundButton"), 2},
        {QStringLiteral("accordion"), QStringLiteral("Accordion"), 2},
        {QStringLiteral("card"), QStringLiteral("Card"), 2},
        {QStringLiteral("divider"), QStringLiteral("Divider"), 2},
        {QStringLiteral("expander"), QStringLiteral("Expander"), 2},
        {QStringLiteral("font-icon"), QStringLiteral("FontIcon"), 2},
        {QStringLiteral("avatar"), QStringLiteral("Avatar"), 2},
        {QStringLiteral("toast"), QStringLiteral("Toast"), 3},
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

TEST_F(GalleryContentPagesTest, ExtractedComponentsHaveDedicatedLiveSamples)
{
    struct SampleCase {
        QString routeId;
        QString sampleId;
    };
    const QVector<SampleCase> cases{
        {QStringLiteral("accordion"), QStringLiteral("accordion-single-expansion")},
        {QStringLiteral("avatar"), QStringLiteral("avatar-image-presence")},
        {QStringLiteral("card"), QStringLiteral("card-surface-appearances")},
        {QStringLiteral("compound-button"), QStringLiteral("compound-button-content")},
        {QStringLiteral("divider"), QStringLiteral("divider-vertical-orientation")},
        {QStringLiteral("expander"), QStringLiteral("expander-state-signal")},
        {QStringLiteral("font-icon"), QStringLiteral("font-icon-optical-sizes")},
        {QStringLiteral("toast"), QStringLiteral("toast-severity")},
    };

    for (const SampleCase& sampleCase : cases) {
        fluent::gallery::GallerySample sample;
        ASSERT_TRUE(findSampleById(
            sampleCase.routeId, sampleCase.sampleId, &sample))
            << sampleCase.routeId.toStdString();
        ASSERT_TRUE(static_cast<bool>(sample.createPreview));
        std::unique_ptr<QWidget> preview(sample.createPreview(nullptr));
        ASSERT_NE(preview, nullptr);
        EXPECT_FALSE(sample.codeSnippet.isEmpty());
    }

    fluent::gallery::GallerySample expanderSample;
    ASSERT_TRUE(findSampleById(
        QStringLiteral("expander"),
        QStringLiteral("expander-state-signal"),
        &expanderSample));
    std::unique_ptr<QWidget> expanderPreview(
        expanderSample.createPreview(nullptr));
    auto* expander =
        expanderPreview->findChild<fluent::layout::Expander*>();
    auto* stateLabel = expanderPreview->findChild<fluent::textfields::Label*>(
        QStringLiteral("galleryExpanderStateLabel"));
    ASSERT_NE(expander, nullptr);
    ASSERT_NE(stateLabel, nullptr);
    expander->setExpandedAnimated(true, false);
    EXPECT_EQ(stateLabel->text(), QStringLiteral("Expanded"));

    fluent::gallery::GallerySample accordionSample;
    ASSERT_TRUE(findSampleById(
        QStringLiteral("accordion"),
        QStringLiteral("accordion-single-expansion"),
        &accordionSample));
    std::unique_ptr<QWidget> accordionPreview(
        accordionSample.createPreview(nullptr));
    auto* accordion =
        accordionPreview->findChild<fluent::layout::Accordion*>();
    ASSERT_NE(accordion, nullptr);
    ASSERT_EQ(accordion->count(), 3);
    accordion->itemAt(1)->setExpandedAnimated(true, false);
    EXPECT_FALSE(accordion->itemAt(0)->isExpanded());
    EXPECT_TRUE(accordion->itemAt(1)->isExpanded());

    fluent::gallery::GallerySample avatarSample;
    ASSERT_TRUE(findSampleById(
        QStringLiteral("avatar"),
        QStringLiteral("avatar-image-presence"),
        &avatarSample));
    std::unique_ptr<QWidget> avatarPreview(
        avatarSample.createPreview(nullptr));
    auto* avatar =
        avatarPreview->findChild<fluent::status_info::Avatar*>();
    ASSERT_NE(avatar, nullptr);
    EXPECT_NE(avatar->presence(),
              fluent::status_info::Avatar::PresenceStatus::None);

    fluent::gallery::GallerySample compoundSample;
    ASSERT_TRUE(findSampleById(
        QStringLiteral("compound-button"),
        QStringLiteral("compound-button-content"),
        &compoundSample));
    std::unique_ptr<QWidget> compoundPreview(
        compoundSample.createPreview(nullptr));
    auto* compoundButton =
        compoundPreview->findChild<fluent::basicinput::CompoundButton*>();
    ASSERT_NE(compoundButton, nullptr);
    EXPECT_FALSE(compoundButton->secondaryText().isEmpty());
}

TEST_F(
    GalleryContentPagesTest,
    EditableComboBoxSampleMakesCustomValueContractVisible)
{
    fluent::gallery::GallerySample sample;
    ASSERT_TRUE(findSampleById(
        QStringLiteral("combobox"),
        QStringLiteral("combobox-editable"),
        &sample));
    EXPECT_TRUE(
        sample.description.contains(
            QStringLiteral("Type any value")));
    for (const QString& sourceFragment :
         {QStringLiteral("setEditable(true)"),
          QStringLiteral(
              "setInsertPolicy(QComboBox::NoInsert)"),
          QStringLiteral("QComboBox::editTextChanged"),
          QStringLiteral("findText("),
          QStringLiteral("Suggested"),
          QStringLiteral("Custom")}) {
        EXPECT_TRUE(
            sample.codeSnippet.contains(sourceFragment))
            << sourceFragment.toStdString();
    }

    std::unique_ptr<QWidget> preview(
        sample.createPreview(nullptr));
    ASSERT_NE(preview, nullptr);
    auto* comboBox = preview->findChild<ComboBox*>(
        QStringLiteral("galleryEditableComboBox"));
    auto* status =
        preview->findChild<fluent::textfields::Label*>(
            QStringLiteral(
                "galleryEditableComboBoxStatus"));
    ASSERT_NE(comboBox, nullptr);
    ASSERT_NE(status, nullptr);
    EXPECT_TRUE(comboBox->isEditable());
    EXPECT_EQ(comboBox->width(), 200);
    EXPECT_EQ(status->width(), 200);
    EXPECT_EQ(
        comboBox->insertPolicy(),
        QComboBox::NoInsert);
    EXPECT_EQ(
        status->text(),
        QStringLiteral("Suggested value: 12"));

    const int originalCount = comboBox->count();
    comboBox->setEditText(
        QStringLiteral("13.5"));
    EXPECT_EQ(
        comboBox->currentText(),
        QStringLiteral("13.5"));
    EXPECT_EQ(comboBox->count(), originalCount);
    EXPECT_EQ(
        comboBox->findText(
            QStringLiteral("13.5"),
            Qt::MatchFixedString
                | Qt::MatchCaseSensitive),
        -1);
    EXPECT_EQ(
        status->text(),
        QStringLiteral("Custom value: 13.5"));

    comboBox->setEditText(QStringLiteral("14"));
    EXPECT_EQ(
        status->text(),
        QStringLiteral("Suggested value: 14"));
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
        {QStringLiteral("button"), QStringLiteral("button-interaction-state"), 5, 10},
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

TEST_F(GalleryContentPagesTest, EditingCommandSampleReusesRouterActions)
{
    using Command = EditingCommandRouter::Command;

    fluent::gallery::GallerySample sample;
    ASSERT_TRUE(findSampleById(QStringLiteral("line-edit"),
                               QStringLiteral("line-edit-editing-commands"),
                               &sample));
    ASSERT_TRUE(static_cast<bool>(sample.createPreview));

    GallerySampleCard card(sample);
    card.resize(640, card.sizeHint().height());
    card.show();
    QApplication::processEvents();

    auto* router = card.findChild<EditingCommandRouter*>();
    auto* menu = card.findChild<FluentMenu*>();
    auto* lineEdit = card.findChild<LineEdit*>();
    auto* textEdit = card.findChild<TextEdit*>();
    ASSERT_NE(router, nullptr);
    ASSERT_NE(menu, nullptr);
    ASSERT_NE(lineEdit, nullptr);
    ASSERT_NE(textEdit, nullptr);

    for (QAction* action : router->actions()) {
        ASSERT_NE(action, nullptr);
        EXPECT_TRUE(menu->actions().contains(action));
    }

    lineEdit->selectAll();
    lineEdit->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    EXPECT_TRUE(router->hasActiveTarget());
    EXPECT_TRUE(router->canExecute(Command::Copy));

    textEdit->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    EXPECT_TRUE(router->hasActiveTarget());
    EXPECT_EQ(router->scopeWindow(), card.window());
}

TEST_F(GalleryContentPagesTest,
       EditingCommandSamplesShareOneRouterPerGalleryWindow)
{
    fluent::gallery::GallerySample menuSample;
    fluent::gallery::GallerySample barSample;
    ASSERT_TRUE(findSampleById(
        QStringLiteral("line-edit"),
        QStringLiteral("line-edit-editing-commands"),
        &menuSample));
    ASSERT_TRUE(findSampleById(
        QStringLiteral("command-bar"),
        QStringLiteral("command-bar-editing-router"),
        &barSample));

    QWidget host;
    auto* menuCard =
        new GallerySampleCard(menuSample, &host);
    auto* barCard =
        new GallerySampleCard(barSample, &host);

    const auto routers =
        host.findChildren<EditingCommandRouter*>(
            QStringLiteral("Gallery.WindowEditingCommandRouter"),
            Qt::FindDirectChildrenOnly);
    ASSERT_EQ(routers.size(), 1);
    auto* bar =
        barCard->findChild<CommandBar*>(
            QStringLiteral(
                "Gallery.CommandBar.EditingRouter"));
    auto* menu = menuCard->findChild<FluentMenu*>();
    ASSERT_NE(bar, nullptr);
    ASSERT_NE(menu, nullptr);
    for (QAction* action : routers.first()->actions()) {
        EXPECT_TRUE(
            bar->primaryActions().contains(action)
            || bar->secondaryActions().contains(action));
        EXPECT_TRUE(menu->actions().contains(action));
    }
}

TEST_F(GalleryContentPagesTest,
       ParentedPrewarmSampleUsesGalleryWindowRouter)
{
    fluent::gallery::GallerySample sample;
    ASSERT_TRUE(findSampleById(
        QStringLiteral("command-bar"),
        QStringLiteral("command-bar-editing-router"),
        &sample));

    GalleryWindow window;
    auto* router =
        window.findChild<EditingCommandRouter*>(
            QStringLiteral(
                "Gallery.WindowEditingCommandRouter"),
            Qt::FindDirectChildrenOnly);
    ASSERT_NE(router, nullptr);

    GallerySampleCard prewarmedCard(sample, &window);
    auto* bar =
        prewarmedCard.findChild<CommandBar*>(
            QStringLiteral(
                "Gallery.CommandBar.EditingRouter"));
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(
        prewarmedCard.findChild<EditingCommandRouter*>(),
        nullptr);
    for (QAction* action : router->actions()) {
        EXPECT_TRUE(
            bar->primaryActions().contains(action)
            || bar->secondaryActions().contains(action));
    }
}

TEST_F(GalleryContentPagesTest,
       CommandBarRoutesExposePublicSamplesAndBundledArtwork)
{
    const auto barReference =
        galleryComponentReference(QStringLiteral("command-bar"));
    const auto flyoutReference =
        galleryComponentReference(
            QStringLiteral("command-bar-flyout"));
    ASSERT_TRUE(barReference.isValid());
    ASSERT_TRUE(flyoutReference.isValid());
    EXPECT_EQ(
        barReference.qualifiedType,
        QStringLiteral(
            "fluent::menus_toolbars::CommandBar"));
    EXPECT_EQ(
        flyoutReference.qualifiedType,
        QStringLiteral(
            "fluent::menus_toolbars::CommandBarFlyout"));

    for (const QString& title :
         {QStringLiteral("CommandBar"),
          QStringLiteral("CommandBarFlyout")}) {
        const QString resource =
            galleryControlImageResource(title);
        ASSERT_FALSE(resource.isEmpty());
        ASSERT_TRUE(QFile::exists(resource));
        const QImage image(resource);
        ASSERT_FALSE(image.isNull());
        EXPECT_EQ(image.size(), QSize(72, 72));
        EXPECT_TRUE(image.hasAlphaChannel());
        EXPECT_EQ(image.pixelColor(0, 0).alpha(), 0);
    }

    fluent::gallery::GallerySample responsive;
    ASSERT_TRUE(findSampleById(
        QStringLiteral("command-bar"),
        QStringLiteral(
            "command-bar-responsive-overflow"),
        &responsive));
    EXPECT_TRUE(
        responsive.codeSnippet.contains(
            QStringLiteral("QAction::HighPriority")));
    EXPECT_TRUE(
        responsive.codeSnippet.contains(
            QStringLiteral(":/icons/add.svg")));
    for (const QString& sourceFragment :
         {QStringLiteral("new CommandBar(barHost)"),
          QStringLiteral("barHost->setFixedWidth(536)"),
          QStringLiteral("setBackgroundVisible(false)"),
          QStringLiteral(":/icons/settings.svg"),
          QStringLiteral(":/icons/help.svg")}) {
        EXPECT_TRUE(
            responsive.codeSnippet.contains(sourceFragment))
            << sourceFragment.toStdString();
    }
    GallerySampleCard responsiveCard(responsive);
    responsiveCard.resize(720, responsiveCard.sizeHint().height());
    responsiveCard.show();
    QApplication::processEvents();
    auto* bar = responsiveCard.findChild<CommandBar*>(
        QStringLiteral("Gallery.CommandBar.Responsive"));
    Button* compact =
        buttonWithText(
            &responsiveCard, QStringLiteral("Compact view"));
    Button* labels =
        buttonWithText(
            &responsiveCard, QStringLiteral("Labels: Right"));
    Button* background =
        buttonWithText(
            &responsiveCard, QStringLiteral("Show background"));
    ASSERT_NE(bar, nullptr);
    ASSERT_NE(compact, nullptr);
    ASSERT_NE(labels, nullptr);
    ASSERT_NE(background, nullptr);
    QStringList primaryTexts;
    for (QAction* action : bar->primaryActions()) {
        if (action && !action->isSeparator()) {
            EXPECT_FALSE(action->icon().isNull());
            primaryTexts.append(action->text());
        }
    }
    EXPECT_EQ(
        primaryTexts,
        (QStringList{
            QStringLiteral("Add"),
            QStringLiteral("Edit"),
            QStringLiteral("Share"),
            QStringLiteral("Sync"),
            QStringLiteral("Pin")}));
    QStringList secondaryTexts;
    for (QAction* action : bar->secondaryActions()) {
        ASSERT_NE(action, nullptr);
        secondaryTexts.append(action->text());
    }
    EXPECT_EQ(
        secondaryTexts,
        (QStringList{
            QStringLiteral("Settings"),
            QStringLiteral("Help")}));
    compact->click();
    QApplication::processEvents();
    EXPECT_FALSE(bar->overflowedPrimaryActions().isEmpty());
    labels->click();
    EXPECT_EQ(
        bar->labelPosition(),
        CommandBar::LabelPosition::Collapsed);
    background->click();
    EXPECT_TRUE(bar->backgroundVisible());

    fluent::gallery::GallerySample integration;
    ASSERT_TRUE(findSampleById(
        QStringLiteral("command-bar"),
        QStringLiteral("command-bar-editing-router"),
        &integration));
    EXPECT_TRUE(
        integration.codeSnippet.contains(
            QStringLiteral("EditingCommandRouter")));
    for (const QString& sourceFragment :
         {QStringLiteral(
              "CommandBar::LabelPosition::Right"),
          QStringLiteral(
              "router->action(command)"),
          QStringLiteral(":/icons/undo.svg"),
          QStringLiteral(":/icons/redo.svg"),
          QStringLiteral(":/icons/cut.svg"),
          QStringLiteral(":/icons/copy.svg"),
          QStringLiteral(":/icons/paste.svg"),
          QStringLiteral(":/icons/delete.svg"),
          QStringLiteral(":/icons/select-all.svg"),
          QStringLiteral(
              "QTimer::singleShot(0, editor")}) {
        EXPECT_TRUE(
            integration.codeSnippet.contains(sourceFragment))
            << sourceFragment.toStdString();
    }
    GallerySampleCard integrationCard(integration);
    integrationCard.resize(
        720, integrationCard.sizeHint().height());
    integrationCard.show();
    QApplication::processEvents();
    auto* router =
        integrationCard.findChild<EditingCommandRouter*>();
    auto* integrationBar =
        integrationCard.findChild<CommandBar*>(
            QStringLiteral(
                "Gallery.CommandBar.EditingRouter"));
    auto* editor =
        integrationCard.findChild<LineEdit*>(
            QStringLiteral(
                "Gallery.CommandBar.EditingTarget"));
    Button* selectText =
        buttonWithText(
            &integrationCard, QStringLiteral("Select text"));
    Button* clearSelection =
        buttonWithText(
            &integrationCard, QStringLiteral("Clear selection"));
    Button* readOnly =
        buttonWithText(
            &integrationCard, QStringLiteral("Read-only: Off"));
    ASSERT_NE(router, nullptr);
    ASSERT_NE(integrationBar, nullptr);
    ASSERT_NE(editor, nullptr);
    ASSERT_NE(selectText, nullptr);
    ASSERT_NE(clearSelection, nullptr);
    ASSERT_NE(readOnly, nullptr);
    EXPECT_EQ(
        integrationBar->labelPosition(),
        CommandBar::LabelPosition::Right);
    EXPECT_NE(
        buttonWithText(
            &integrationCard, QStringLiteral("Undo")),
        nullptr);
    EXPECT_NE(
        buttonWithText(
            &integrationCard, QStringLiteral("Redo")),
        nullptr);
    EXPECT_FALSE(integrationBar->backgroundVisible());
    for (QAction* action : router->actions()) {
        EXPECT_TRUE(
            integrationBar->primaryActions().contains(action)
            || integrationBar->secondaryActions().contains(action));
        EXPECT_FALSE(action->icon().isNull());
    }
    QTest::mouseClick(selectText, Qt::LeftButton);
    QTRY_VERIFY(router->canExecute(
        EditingCommandRouter::Command::Cut));
    EXPECT_TRUE(router->canExecute(
        EditingCommandRouter::Command::Copy));
    QTest::mouseClick(readOnly, Qt::LeftButton);
    QTRY_VERIFY(editor->isReadOnly());
    EXPECT_FALSE(router->canExecute(
        EditingCommandRouter::Command::Cut));
    EXPECT_TRUE(router->canExecute(
        EditingCommandRouter::Command::Copy));
    QTest::mouseClick(clearSelection, Qt::LeftButton);
    QTRY_VERIFY(!router->canExecute(
        EditingCommandRouter::Command::Copy));

    fluent::gallery::GallerySample modes;
    ASSERT_TRUE(findSampleById(
        QStringLiteral("command-bar-flyout"),
        QStringLiteral(
            "command-bar-flyout-show-modes"),
        &modes));
    EXPECT_TRUE(
        modes.codeSnippet.contains(
            QStringLiteral(
                "CommandBarFlyout::ShowMode::Transient")));
    for (const QString& sourceFragment :
         {QStringLiteral(":/icons/share.svg"),
          QStringLiteral(":/icons/save.svg"),
          QStringLiteral(":/icons/delete.svg"),
          QStringLiteral(":/icons/resize.svg"),
          QStringLiteral(":/icons/move.svg"),
          QStringLiteral("QAbstractButton::clicked"),
          QStringLiteral(
              "Qt::CustomContextMenu"),
          QStringLiteral(
              "QWidget::customContextMenuRequested"),
          QStringLiteral(
              "CommandBarFlyout::ShowMode::Standard")}) {
        EXPECT_TRUE(modes.codeSnippet.contains(sourceFragment))
            << sourceFragment.toStdString();
    }
    GallerySampleCard flyoutCard(modes);
    flyoutCard.resize(720, flyoutCard.sizeHint().height());
    flyoutCard.show();
    QApplication::processEvents();
    auto* flyout =
        flyoutCard.findChild<CommandBarFlyout*>(
            QStringLiteral("Gallery.CommandBarFlyout"));
    QWidget* tile = flyoutCard.findChild<QWidget*>(
        QStringLiteral(
            "Gallery.CommandBarFlyout.ContextTile"));
    ASSERT_NE(flyout, nullptr);
    ASSERT_NE(tile, nullptr);
    EXPECT_EQ(flyout->primaryActions().size(), 3);
    EXPECT_EQ(flyout->secondaryActions().size(), 2);
    QStringList flyoutPrimaryTexts;
    for (QAction* action : flyout->primaryActions()) {
        ASSERT_NE(action, nullptr);
        flyoutPrimaryTexts.append(action->text());
    }
    EXPECT_EQ(
        flyoutPrimaryTexts,
        (QStringList{
            QStringLiteral("Share"),
            QStringLiteral("Save"),
            QStringLiteral("Delete")}));
    QStringList flyoutSecondaryTexts;
    for (QAction* action : flyout->secondaryActions()) {
        ASSERT_NE(action, nullptr);
        flyoutSecondaryTexts.append(action->text());
    }
    EXPECT_EQ(
        flyoutSecondaryTexts,
        (QStringList{
            QStringLiteral("Resize"),
            QStringLiteral("Move")}));
    for (QAction* action :
         flyout->primaryActions()
             + flyout->secondaryActions()) {
        ASSERT_NE(action, nullptr);
        EXPECT_FALSE(action->icon().isNull());
    }
    flyout->setAnimationEnabled(false);
    const QPoint contextPosition = tile->rect().center();
    QContextMenuEvent contextEvent(
        QContextMenuEvent::Mouse,
        contextPosition,
        tile->mapToGlobal(contextPosition));
    QApplication::sendEvent(tile, &contextEvent);
    QApplication::processEvents();
    EXPECT_TRUE(flyout->isOpen());
    EXPECT_EQ(
        flyout->showMode(),
        CommandBarFlyout::ShowMode::Standard);
    EXPECT_TRUE(flyout->isExpanded());
    flyout->close();
    QTest::mouseClick(
        tile,
        Qt::LeftButton,
        Qt::NoModifier,
        tile->rect().center());
    QApplication::processEvents();
    EXPECT_TRUE(flyout->isOpen());
    EXPECT_EQ(
        flyout->showMode(),
        CommandBarFlyout::ShowMode::Transient);
    EXPECT_FALSE(flyout->isAlwaysExpanded());
    EXPECT_FALSE(flyout->isExpanded());
    flyout->close();

    fluent::gallery::GallerySample alwaysExpandedSample;
    ASSERT_TRUE(findSampleById(
        QStringLiteral("command-bar-flyout"),
        QStringLiteral(
            "command-bar-flyout-always-expanded"),
        &alwaysExpandedSample));
    for (const QString& sourceFragment :
         {QStringLiteral("setAlwaysExpanded(true)"),
          QStringLiteral(
              "CommandBarFlyout::ShowMode::Transient"),
          QStringLiteral("favoriteAction->setCheckable(true)"),
          QStringLiteral(":/icons/link.svg"),
          QStringLiteral(":/icons/favorite.svg"),
          QStringLiteral(":/icons/edit.svg"),
          QStringLiteral(":/icons/info.svg")}) {
        EXPECT_TRUE(
            alwaysExpandedSample.codeSnippet.contains(
                sourceFragment))
            << sourceFragment.toStdString();
    }
    GallerySampleCard alwaysExpandedCard(
        alwaysExpandedSample);
    alwaysExpandedCard.resize(
        720, alwaysExpandedCard.sizeHint().height());
    alwaysExpandedCard.show();
    QApplication::processEvents();
    auto* alwaysExpandedFlyout =
        alwaysExpandedCard.findChild<CommandBarFlyout*>(
            QStringLiteral(
                "Gallery.CommandBarFlyout.AlwaysExpanded"));
    Button* openActions =
        buttonWithText(
            &alwaysExpandedCard,
            QStringLiteral("Open actions"));
    Button* alwaysExpandedToggle =
        buttonWithText(
            &alwaysExpandedCard,
            QStringLiteral("Always expanded: On"));
    ASSERT_NE(alwaysExpandedFlyout, nullptr);
    ASSERT_NE(openActions, nullptr);
    ASSERT_NE(alwaysExpandedToggle, nullptr);
    alwaysExpandedFlyout->setAnimationEnabled(false);
    EXPECT_TRUE(alwaysExpandedFlyout->isAlwaysExpanded());
    openActions->setFocus(Qt::OtherFocusReason);
    openActions->click();
    QApplication::processEvents();
    EXPECT_TRUE(alwaysExpandedFlyout->isOpen());
    EXPECT_EQ(
        alwaysExpandedFlyout->showMode(),
        CommandBarFlyout::ShowMode::Transient);
    EXPECT_TRUE(alwaysExpandedFlyout->isExpanded());
    EXPECT_EQ(QApplication::focusWidget(), openActions);
    alwaysExpandedFlyout->close();
    alwaysExpandedToggle->click();
    EXPECT_FALSE(alwaysExpandedFlyout->isAlwaysExpanded());
    openActions->click();
    QApplication::processEvents();
    EXPECT_TRUE(alwaysExpandedFlyout->isOpen());
    EXPECT_FALSE(alwaysExpandedFlyout->isExpanded());
    alwaysExpandedFlyout->close();
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

TEST_F(GalleryContentPagesTest, EverySampleCodeBlockUsesCppAndNamesItsPreviewComponent)
{
    int auditedSamples = 0;
    for (const auto& category : galleryComponentCatalog()) {
        for (const auto& component : category.components) {
            const auto reference = galleryComponentReference(component.id);
            ASSERT_TRUE(reference.isValid()) << component.id.toStdString();
            const QString expectedType = reference.qualifiedType.section(
                QStringLiteral("::"), -1);
            ASSERT_FALSE(expectedType.isEmpty()) << component.id.toStdString();

            const auto samples = fluent::gallery::gallerySamplesForRoute(component.id);
            ASSERT_FALSE(samples.isEmpty()) << component.id.toStdString();
            for (const auto& sample : samples) {
                SCOPED_TRACE(QStringLiteral("route=%1 sample=%2")
                                 .arg(component.id, sample.id)
                                 .toStdString());
                EXPECT_TRUE(sample.codeSnippet.contains(expectedType))
                    << "The code block must name the component demonstrated by its route: "
                    << expectedType.toStdString();
                EXPECT_TRUE(sample.codeSnippet.contains(QLatin1Char(';')))
                    << "Gallery source blocks are C++ statements, not pseudocode or QML";
                EXPECT_FALSE(sample.codeSnippet.contains(QStringLiteral("import QtQuick")));
                EXPECT_FALSE(sample.codeSnippet.contains(QStringLiteral("import QtQuick.Controls")));

                std::unique_ptr<QWidget> preview(sample.createPreview(nullptr));
                ASSERT_NE(preview, nullptr);
                const QByteArray qualifiedType = reference.qualifiedType.toUtf8();
                bool previewContainsType = preview->inherits(qualifiedType.constData());
                if (!previewContainsType) {
                    const auto descendants = preview->findChildren<QObject*>();
                    previewContainsType = std::any_of(
                        descendants.cbegin(), descendants.cend(),
                        [&qualifiedType](QObject* object) {
                            return object && object->inherits(qualifiedType.constData());
                        });
                }
                // Dialog/flyout/tooltip samples create their transient surface
                // only after the trigger is invoked; the managed Toast stacking
                // sample does the same through Toast::showToast(). Window samples
                // intentionally render an embedded chrome simulation instead of
                // nesting a top-level window. All other routes must carry their
                // public component in the initial live preview tree.
                // zh_CN: 对话框、浮层、提示以及托管 Toast 堆叠示例会在触发后创建瞬态表面；
                // Window 示例使用嵌入式 chrome 模拟，避免嵌套顶层窗口。
                const bool deferredPreview = category.id == QStringLiteral("dialogs-flyouts")
                    || component.id == QStringLiteral("tooltip")
                    || (component.id == QStringLiteral("toast")
                        && sample.id == QStringLiteral("toast-stacking"))
                    || component.id == QStringLiteral("window");
                if (!deferredPreview) {
                    EXPECT_TRUE(previewContainsType)
                        << "The live preview must instantiate the component named by its route";
                }

                GalleryCodeBlock block(sample.codeSnippet);
                auto* language = block.findChild<fluent::textfields::Label*>(
                    QStringLiteral("galleryCodeBlockLang"));
                ASSERT_NE(language, nullptr);
                EXPECT_EQ(language->text(), QStringLiteral("C++"));
                ++auditedSamples;
            }
        }
    }

    EXPECT_GT(auditedSamples, 100)
        << "The audit must cover the complete component sample catalog";
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
    const QString moonGlyph = Typography::Icons::glyph(
        QStringLiteral(
            "ic_fluent_weather_moon_16_regular"));
    ASSERT_FALSE(moonGlyph.isEmpty());
    EXPECT_EQ(themeButton->property("gallerySampleTheme").toString(), QStringLiteral("Light"));
    EXPECT_EQ(
        themeButton->property(
            "gallerySampleThemeGlyph").toString(),
        Typography::Icons::Sunny);
    EXPECT_TRUE(
        themeButton->accessibleName().contains(
            QStringLiteral("Preview theme: Light")));
    EXPECT_TRUE(
        themeButton->accessibleName().contains(
            QStringLiteral("Switch to Dark")));
    EXPECT_EQ(
        themeButton->toolTip(),
        themeButton->accessibleName());

    GallerySampleCard* card = page->sampleCards().first();
    ASSERT_NE(card, nullptr);
    auto* previewSurface = card->findChild<QWidget*>(
        QStringLiteral("gallerySampleCardPreview"));
    ASSERT_NE(previewSurface, nullptr);
    auto* previewCard =
        dynamic_cast<fluent::layout::Card*>(previewSurface);
    ASSERT_NE(previewCard, nullptr);
    EXPECT_FALSE(previewSurface->property("fluentThemeOverride").isValid());
    EXPECT_EQ(previewSurface->property("fluentSurfaceColor").value<QColor>(),
              previewCard->themeColorsRef().bgLayerAlt);
    EXPECT_TRUE(card->styleSheet().contains(QStringLiteral("rgba(255, 255, 255, 255)")));

    auto* sampleButton = previewSurface->findChild<Button*>();
    ASSERT_NE(sampleButton, nullptr);
    EXPECT_EQ(sampleButton->effectiveTheme(), fluent::FluentElement::Light);

    QTest::mouseClick(themeButton, Qt::LeftButton);
    QApplication::processEvents();

    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Light);
    EXPECT_EQ(page->titleLabel()->effectiveTheme(), fluent::FluentElement::Light);
    EXPECT_EQ(themeButton->property("gallerySampleTheme").toString(), QStringLiteral("Dark"));
    EXPECT_EQ(
        themeButton->property(
            "gallerySampleThemeGlyph").toString(),
        moonGlyph);
    EXPECT_TRUE(
        themeButton->accessibleName().contains(
            QStringLiteral("Preview theme: Dark")));
    EXPECT_TRUE(
        themeButton->accessibleName().contains(
            QStringLiteral("Switch to Light")));
    EXPECT_EQ(
        themeButton->toolTip(),
        themeButton->accessibleName());
    EXPECT_EQ(previewSurface->property("fluentThemeOverride").toInt(),
              static_cast<int>(fluent::FluentElement::Dark));
    EXPECT_EQ(previewSurface->property("fluentSurfaceColor").value<QColor>(),
              previewCard->themeColorsRef().bgLayerAlt);
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
    auto* divider = block.findChild<fluent::layout::Divider*>(
        QStringLiteral("fluentExpanderDivider"));
    auto* copyButton = block.findChild<QWidget*>(QStringLiteral("galleryCodeBlockCopyButton"));
    ASSERT_NE(header, nullptr);
    ASSERT_NE(content, nullptr);
    ASSERT_NE(divider, nullptr);
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
    EXPECT_TRUE(divider->isVisible());
    EXPECT_EQ(divider->geometry().top(), header->geometry().bottom() + 1);
    EXPECT_EQ(content->y(), divider->geometry().bottom() + 1);
    EXPECT_EQ(content->geometry().bottom(), block.rect().bottom());

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

TEST_F(GalleryContentPagesTest, CodeBlockUsesFluentReadOnlyContextMenu)
{
    const QString source =
        QStringLiteral("auto value = compute();");
    GalleryCodeBlock block(source);
    block.setExpanded(true, /*animated=*/false);
    block.resize(520, block.sizeHint().height());
    block.show();
    QApplication::processEvents();

    auto* code = block.findChild<QLabel*>(
        QStringLiteral("galleryCodeBlockText"));
    ASSERT_NE(code, nullptr);
    code->setSelection(0, 4);
    ASSERT_TRUE(code->hasSelectedText());

    bool sawFluentMenu = false;
    bool sawCopy = false;
    bool sawSelectAll = false;
    bool sawCopyIcon = false;
    bool sawSelectAllIcon = false;
    QTimer::singleShot(0, [&]() {
        auto* menu =
            qobject_cast<FluentMenu*>(
                QApplication::activePopupWidget());
        sawFluentMenu = menu != nullptr;
        if (!menu)
            return;

        EXPECT_EQ(
            menu->objectName(),
            QStringLiteral("FluentLabel.ContextMenu"));
        EXPECT_EQ(
            menu->fontStyle(),
            Typography::FontRole::Caption);
        EXPECT_EQ(
            menu->font().pixelSize(),
            Typography::FontSize::Caption);
        for (QAction* action : menu->actions()) {
            ASSERT_NE(action, nullptr);
            if (!action->isSeparator()) {
                EXPECT_LT(
                    menu->actionGeometry(action).height(),
                    ::Spacing::ControlHeight::Standard);
            }
            if (!action->icon().isNull()) {
                const QSize iconSize =
                    action->icon().actualSize(QSize(64, 64));
                EXPECT_LE(
                    iconSize.width(),
                    Typography::IconSize::Standard);
                EXPECT_LE(
                    iconSize.height(),
                    Typography::IconSize::Standard);
            }
            if (actionUsesStandardKey(
                    action, QKeySequence::Copy)) {
                sawCopy = true;
                sawCopyIcon = !action->icon().isNull();
                EXPECT_TRUE(action->isEnabled());
                action->trigger();
            } else if (actionUsesStandardKey(
                           action, QKeySequence::SelectAll)) {
                sawSelectAll = true;
                sawSelectAllIcon = !action->icon().isNull();
                EXPECT_TRUE(action->isEnabled());
            }
        }
        menu->close();
    });

    const QPoint localPosition = code->rect().center();
    QContextMenuEvent event(
        QContextMenuEvent::Mouse,
        localPosition,
        code->mapToGlobal(localPosition));
    QApplication::sendEvent(code, &event);

    EXPECT_TRUE(event.isAccepted());
    EXPECT_TRUE(sawFluentMenu);
    EXPECT_TRUE(sawCopy);
    EXPECT_TRUE(sawSelectAll);
    EXPECT_TRUE(sawCopyIcon);
    EXPECT_TRUE(sawSelectAllIcon);
    ASSERT_NE(QApplication::clipboard(), nullptr);
    EXPECT_EQ(
        QApplication::clipboard()->text(),
        QStringLiteral("auto"));
}

TEST_F(
    GalleryContentPagesTest,
    ComponentReferenceValuesUseSharedFluentContextMenu)
{
    const fluent::gallery::GalleryComponentReference reference{
        QStringLiteral("<FluentQt/MenusToolbars.h>"),
        QStringLiteral(
            "fluent::menus_toolbars::CommandBar"),
        QStringLiteral("FluentQt::FluentQt")};
    GalleryComponentReferenceCard card(reference);
    card.resize(620, card.sizeHint().height());
    card.show();
    QApplication::processEvents();

    auto* value =
        card.findChild<fluent::textfields::Label*>(
            QStringLiteral(
                "galleryComponentReferenceHeader"));
    ASSERT_NE(value, nullptr);
    value->setSelection(0, 9);
    ASSERT_TRUE(value->hasSelectedText());

    bool sawFluentMenu = false;
    QTimer::singleShot(0, [&]() {
        auto* menu = qobject_cast<FluentMenu*>(
            QApplication::activePopupWidget());
        sawFluentMenu = menu != nullptr;
        if (!menu)
            return;

        EXPECT_EQ(
            menu->objectName(),
            QStringLiteral("FluentLabel.ContextMenu"));
        EXPECT_EQ(
            menu->font().pixelSize(),
            Typography::FontSize::Caption);
        menu->close();
    });

    const QPoint localPosition = value->rect().center();
    QContextMenuEvent event(
        QContextMenuEvent::Mouse,
        localPosition,
        value->mapToGlobal(localPosition));
    QApplication::sendEvent(value, &event);

    EXPECT_TRUE(event.isAccepted());
    EXPECT_TRUE(sawFluentMenu);
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

    // Event-loop scheduling can coalesce animation ticks on a busy Windows host. The contract is
    // that every geometry sample observed across both transitions stays stable, not that the test
    // runner must wake for a fixed number of frames.
    // zh_CN: Windows 忙碌时事件循环会合并动画 tick；契约是展开/收起期间所有已观测几何保持稳定，而非固定采到 8 帧。
    ASSERT_GE(sampledRadiusHeights.size(), 2);
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

    auto* icon = toast->findChild<fluent::FontIcon*>(
        QStringLiteral("galleryToastIcon"));
    ASSERT_NE(icon, nullptr);
    EXPECT_EQ(icon->size(), QSize(Typography::IconSize::Standard,
                                  Typography::IconSize::Standard));
    EXPECT_EQ(icon->glyph(), Typography::Icons::Success);

    auto* reusableToast =
        qobject_cast<fluent::status_info::Toast*>(toast);
    ASSERT_NE(reusableToast, nullptr);
    EXPECT_EQ(reusableToast->severity(),
              fluent::status_info::Toast::Success);
    EXPECT_EQ(reusableToast->placementMargins(),
              QMargins(16, 36 + 14, 16, 16));

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
