#include <gtest/gtest.h>

#include <QApplication>
#include <QFrame>
#include <QFile>
#include <QGraphicsOpacityEffect>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QMargins>
#include <QStringList>
#include <QTest>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/textfields/Label.h"
#include "model/GalleryComponentCatalog.h"
#include "model/GalleryContentCatalog.h"
#include "view/pages/GalleryCategoryPage.h"
#include "view/widgets/GalleryCodeBlock.h"
#include "view/pages/GalleryComponentPage.h"
#include "view/pages/GalleryContentPage.h"
#include "view/widgets/GalleryEntryCard.h"
#include "view/widgets/GallerySampleCard.h"
#include "view/widgets/GallerySampleCatalog.h"
#include "view/shell/GalleryWindow.h"
#include "view/support/GalleryToast.h"
#include "viewmodel/GalleryNavigationViewModel.h"

using fluent::gallery::GalleryCategoryPage;
using fluent::gallery::GalleryCodeBlock;
using fluent::gallery::GalleryComponentPage;
using fluent::gallery::GalleryContentPage;
using fluent::gallery::GalleryEntryCard;
using fluent::gallery::GalleryNavigationViewModel;
using fluent::gallery::GallerySampleCard;
using fluent::gallery::GalleryWindow;
using fluent::gallery::galleryComponentCatalog;
using fluent::gallery::galleryControlImageResource;
using fluent::gallery::galleryContentCatalog;
using fluent::gallery::galleryContentEntry;

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

        const auto* item = navigationViewModel.itemById(routeId);
        ASSERT_NE(item, nullptr) << routeId.toStdString();
        if (item->kind != fluent::gallery::GalleryNavigationItem::Kind::ComponentRoute)
            continue;

        EXPECT_EQ(entry->kind, fluent::gallery::GalleryPageKind::Component)
            << routeId.toStdString();
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
        ASSERT_TRUE(window.selectRoute(item.id)) << item.id.toStdString();
        auto* page = dynamic_cast<GalleryComponentPage*>(window.currentContentPage());
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
    auto* page = dynamic_cast<GalleryCategoryPage*>(window.currentContentPage());
    ASSERT_NE(page, nullptr);

    GalleryNavigationViewModel navigationViewModel;
    int componentCount = 0;
    for (const auto& item : navigationViewModel.items()) {
        if (item.kind == fluent::gallery::GalleryNavigationItem::Kind::ComponentRoute)
            ++componentCount;
    }
    EXPECT_EQ(page->componentRouteIds().size(), componentCount);
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

// Task 6.2: category routes build category overview pages with component cards.
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
        auto* page = dynamic_cast<GalleryCategoryPage*>(window.currentContentPage());
        ASSERT_NE(page, nullptr) << categoryCase.routeId.toStdString();
        EXPECT_EQ(page->routeId(), categoryCase.routeId);
        EXPECT_TRUE(page->componentRouteIds().contains(categoryCase.seededComponentRouteId))
            << categoryCase.routeId.toStdString();

        bool cardForComponent = false;
        for (auto* card : page->findChildren<GalleryEntryCard*>()) {
            if (card->targetRouteId() == categoryCase.seededComponentRouteId)
                cardForComponent = true;
        }
        EXPECT_TRUE(cardForComponent) << categoryCase.seededComponentRouteId.toStdString();
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
        auto* page = dynamic_cast<GalleryComponentPage*>(window.currentContentPage());
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
    auto* page = dynamic_cast<GalleryComponentPage*>(window.currentContentPage());
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

// Task 6.4: TreeView and TabView samples produce live hosted preview widgets.
TEST_F(GalleryContentPagesTest, CollectionAndNavigationSamplesHostLivePreviews)
{
    GalleryWindow window;

    ASSERT_TRUE(window.selectRoute(QStringLiteral("tree-view")));
    auto* treePage = dynamic_cast<GalleryComponentPage*>(window.currentContentPage());
    ASSERT_NE(treePage, nullptr);
    ASSERT_GE(treePage->sampleCount(), 1);
    EXPECT_NE(treePage->sampleCards().first()->previewWidget(), nullptr);

    ASSERT_TRUE(window.selectRoute(QStringLiteral("tab-view")));
    auto* tabPage = dynamic_cast<GalleryComponentPage*>(window.currentContentPage());
    ASSERT_NE(tabPage, nullptr);
    ASSERT_GE(tabPage->sampleCount(), 1);
    EXPECT_NE(tabPage->sampleCards().first()->previewWidget(), nullptr);
}

// Task 6.6: content page and sample card refresh their surfaces on theme change.
TEST_F(GalleryContentPagesTest, ContentPageAndSampleCardRefreshOnThemeChange)
{
    GalleryWindow window;
    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    auto* page = dynamic_cast<GalleryComponentPage*>(window.currentContentPage());
    ASSERT_NE(page, nullptr);
    ASSERT_GE(page->sampleCount(), 1);
    GallerySampleCard* card = page->sampleCards().first();
    ASSERT_NE(card, nullptr);

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    page->onThemeUpdated();
    card->onThemeUpdated();
    // Dark canvas (#202020) is applied to the page surface and dark layer (#2C2C2C) to the card.
    EXPECT_TRUE(page->styleSheet().contains(QStringLiteral("32, 32, 32")));
    EXPECT_TRUE(card->styleSheet().contains(QStringLiteral("44, 44, 44")));

    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    page->onThemeUpdated();
    card->onThemeUpdated();
    // Light canvas (#F3F3F3) returns to the page surface.
    EXPECT_TRUE(page->styleSheet().contains(QStringLiteral("243, 243, 243")));
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

    auto* page = dynamic_cast<GalleryComponentPage*>(window.currentContentPage());
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
