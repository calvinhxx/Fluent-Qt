#include <gtest/gtest.h>

#include <QApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QFrame>
#include <QFile>
#include <QGraphicsOpacityEffect>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QMargins>
#include <QPoint>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStringList>
#include <QTest>
#include <QWidget>

#include "components/basicinput/Button.h"
#include "components/collections/TreeView.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/scrolling/ScrollView.h"
#include "components/textfields/Label.h"
#include "components/textfields/TextEdit.h"
#include "model/GalleryComponentCatalog.h"
#include "model/GalleryContentCatalog.h"
#include "view/pages/GalleryCategoryPage.h"
#include "view/widgets/GalleryCodeBlock.h"
#include "view/pages/GalleryComponentPage.h"
#include "view/pages/GalleryContentPage.h"
#include "view/widgets/GalleryEntryGrid.h"
#include "view/widgets/GallerySampleCard.h"
#include "view/widgets/GallerySampleCatalog.h"
#include "view/shell/GalleryWindow.h"
#include "view/support/GalleryToast.h"
#include "viewmodel/GalleryNavigationViewModel.h"

using fluent::gallery::GalleryCategoryPage;
using fluent::gallery::GalleryCodeBlock;
using fluent::gallery::GalleryComponentPage;
using fluent::gallery::GalleryContentPage;
using fluent::gallery::GalleryEntryGrid;
using fluent::gallery::GalleryNavigationViewModel;
using fluent::gallery::GallerySampleCard;
using fluent::gallery::GalleryWindow;
using fluent::gallery::galleryComponentCatalog;
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

    GallerySampleCard* card = page->sampleCards().first();
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

    const QRect titleGeometry = card->titleLabel()->geometry();
    const QRect previewGeometry = preview->geometry();
    const QRect collapsedCodeGeometry = codeBlock->geometry();

    codeBlock->setExpanded(true);
    const int targetContentHeight = contentInner->height();
    EXPECT_GT(targetContentHeight, 0);
    for (int i = 0; i < 6; ++i) {
        QTest::qWait(40);
        QApplication::processEvents();
        EXPECT_EQ(card->titleLabel()->geometry(), titleGeometry);
        EXPECT_EQ(preview->geometry(), previewGeometry);
        EXPECT_EQ(codeBlock->geometry().topLeft(), collapsedCodeGeometry.topLeft());
        EXPECT_EQ(codeBlock->geometry().width(), collapsedCodeGeometry.width());
        EXPECT_EQ(contentInner->geometry().topLeft(), QPoint(0, 0));
        EXPECT_EQ(contentInner->height(), targetContentHeight);
        EXPECT_LE(content->height(), targetContentHeight);
    }

    QTest::qWait(320);
    QApplication::processEvents();
    EXPECT_EQ(card->titleLabel()->geometry(), titleGeometry);
    EXPECT_EQ(preview->geometry(), previewGeometry);
    EXPECT_EQ(codeBlock->geometry().topLeft(), collapsedCodeGeometry.topLeft());
    EXPECT_EQ(codeBlock->geometry().width(), collapsedCodeGeometry.width());
    EXPECT_EQ(contentInner->geometry().topLeft(), QPoint(0, 0));
    EXPECT_EQ(contentInner->height(), targetContentHeight);
    EXPECT_GT(codeBlock->height(), collapsedCodeGeometry.height());
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
