#include <gtest/gtest.h>

#include <QApplication>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/collections/StackView.h"
#include "components/scrolling/PipsPager.h"
#include "compatibility/QtCompat.h"

using fluent::collections::StackView;
using fluent::scrolling::PipsPager;

namespace {

using Edge = fluent::AnchorLayout::Edge;

class StackViewTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, themeColors().bgCanvas);
        setPalette(pal);
        setAutoFillBackground(true);
    }
};

class StackPage : public QWidget {
public:
    explicit StackPage(const QString& title, const QColor& color = QColor("#DDEAF7"), QWidget* parent = nullptr)
        : QWidget(parent),
          m_title(title),
          m_color(color)
    {
        setAutoFillBackground(true);
        QPalette pal = palette();
        pal.setColor(QPalette::Window, m_color);
        setPalette(pal);
        setObjectName(title);

        auto* layout = new fluent::AnchorLayout();
        setLayout(layout);

        auto* label = new QLabel(title, this);
        label->setAlignment(Qt::AlignCenter);
        QFont font = label->font();
        font.setPixelSize(24);
        font.setBold(true);
        label->setFont(font);

        fluent::AnchorLayout::Anchors anchors;
        anchors.fill = true;
        anchors.fillMargins = QMargins(12, 12, 12, 12);
        layout->addAnchoredWidget(label, anchors);
    }

    QSize sizeHint() const override { return QSize(320, 220); }
    QSize minimumSizeHint() const override { return QSize(120, 80); }

private:
    QString m_title;
    QColor m_color;
};

void showAndProcess(QWidget& widget)
{
    widget.show();
    QApplication::processEvents();
}

void sendKey(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QKeyEvent press(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(widget, &press);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(widget, &release);
}

void processDeferredDeletes()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
}

StackView::StackViewItemStatus statusAt(const QSignalSpy& spy, int index)
{
    return static_cast<StackView::StackViewItemStatus>(spy.at(index).at(1).value<StackView::StackViewItemStatus>());
}

} // namespace

class StackViewTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        fluentRegisterMetaTypeNames<fluent::collections::StackView::StackViewItemStatus>(
            "fluent::collections::StackView::StackViewItemStatus",
            "StackViewItemStatus");
        fluentRegisterMetaTypeNames<fluent::collections::StackView::StackViewTransitionOperation>(
            "fluent::collections::StackView::StackViewTransitionOperation",
            "StackViewTransitionOperation");
        fluentRegisterMetaTypeNames<fluent::collections::StackView::StackViewTransitionType>(
            "fluent::collections::StackView::StackViewTransitionType",
            "StackViewTransitionType");
        fluentRegisterMetaTypeNames<fluent::collections::StackView::StackViewItemOwnership>(
            "fluent::collections::StackView::StackViewItemOwnership",
            "StackViewItemOwnership");
        qRegisterMetaType<Qt::Orientation>("Qt::Orientation");
    }

    void SetUp() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    void TearDown() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }
};

TEST_F(StackViewTest, DefaultsExposeExpectedApi)
{
    StackView stack;

    EXPECT_EQ(stack.depth(), 0);
    EXPECT_EQ(stack.currentItem(), nullptr);
    EXPECT_EQ(stack.initialItem(), nullptr);
    EXPECT_FALSE(stack.canPop());
    EXPECT_FALSE(stack.busy());
    EXPECT_EQ(stack.orientation(), Qt::Horizontal);
    EXPECT_TRUE(stack.transitionAnimationEnabled());
    EXPECT_GT(stack.transitionDuration(), 0);
    EXPECT_EQ(stack.transitionType(), StackView::StackViewTransitionType::SlideFade);
    EXPECT_FALSE(stack.sizeHint().isEmpty());
    EXPECT_NE(dynamic_cast<QStackedWidget*>(&stack), nullptr);
    EXPECT_NE(dynamic_cast<fluent::FluentElement*>(&stack), nullptr);
    EXPECT_NE(dynamic_cast<fluent::QMLPlus*>(&stack), nullptr);
}

TEST_F(StackViewTest, TransitionTypeIsConfigurable)
{
    StackView stack;
    QSignalSpy spy(&stack, &StackView::transitionTypeChanged);

    stack.setTransitionType(StackView::StackViewTransitionType::ScaleFade);
    EXPECT_EQ(stack.transitionType(), StackView::StackViewTransitionType::ScaleFade);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).value<StackView::StackViewTransitionType>(), StackView::StackViewTransitionType::ScaleFade);

    stack.setTransitionType(StackView::StackViewTransitionType::ScaleFade);
    EXPECT_EQ(spy.count(), 1);

    stack.setTransitionType(StackView::StackViewTransitionType::Immediate);
    EXPECT_EQ(stack.transitionType(), StackView::StackViewTransitionType::Immediate);
    EXPECT_EQ(spy.count(), 2);
}

TEST_F(StackViewTest, PushPopReplaceAndClearUpdateStackState)
{
    StackView stack;
    stack.setTransitionAnimationEnabled(false);
    QSignalSpy depthSpy(&stack, &StackView::depthChanged);
    QSignalSpy currentSpy(&stack, &StackView::currentItemChanged);
    QSignalSpy pushedSpy(&stack, &StackView::itemPushed);
    QSignalSpy poppedSpy(&stack, &StackView::itemPopped);
    QSignalSpy replacedSpy(&stack, &StackView::itemReplaced);

    auto* first = new StackPage("First");
    auto* second = new StackPage("Second");
    QPointer<QWidget> secondPointer = second;
    auto* third = new StackPage("Third");

    ASSERT_TRUE(stack.push(first));
    EXPECT_EQ(stack.depth(), 1);
    EXPECT_EQ(stack.currentItem(), first);
    EXPECT_EQ(stack.initialItem(), first);
    EXPECT_EQ(stack.itemStatus(first), StackView::StackViewItemStatus::Active);
    EXPECT_EQ(pushedSpy.count(), 1);

    ASSERT_TRUE(stack.push(second));
    EXPECT_EQ(stack.depth(), 2);
    EXPECT_EQ(stack.currentItem(), second);
    EXPECT_TRUE(stack.canPop());
    EXPECT_EQ(stack.itemStatus(first), StackView::StackViewItemStatus::Inactive);
    EXPECT_EQ(stack.itemStatus(second), StackView::StackViewItemStatus::Active);

    ASSERT_TRUE(stack.replace(third));
    EXPECT_EQ(stack.depth(), 2);
    EXPECT_EQ(stack.currentItem(), third);
    EXPECT_EQ(replacedSpy.count(), 1);
    processDeferredDeletes();
    EXPECT_TRUE(secondPointer.isNull());

    ASSERT_TRUE(stack.pop());
    EXPECT_EQ(stack.depth(), 1);
    EXPECT_EQ(stack.currentItem(), first);
    EXPECT_EQ(poppedSpy.count(), 1);
    EXPECT_FALSE(stack.pop());

    ASSERT_TRUE(stack.clear());
    EXPECT_EQ(stack.depth(), 0);
    EXPECT_EQ(stack.currentItem(), nullptr);
    EXPECT_EQ(stack.initialItem(), nullptr);
    EXPECT_GE(depthSpy.count(), 4);
    EXPECT_GE(currentSpy.count(), 4);
}

TEST_F(StackViewTest, SetInitialItemResetsStackAndRespectsOwnership)
{
    StackView stack;
    stack.setTransitionAnimationEnabled(false);
    QSignalSpy initialSpy(&stack, &StackView::initialItemChanged);
    auto* first = new StackPage("First");
    QPointer<QWidget> firstPointer = first;
    auto* second = new StackPage("Second");

    ASSERT_TRUE(stack.setInitialItem(first, StackView::StackViewItemOwnership::DoesNotOwnItem));
    EXPECT_EQ(stack.depth(), 1);
    EXPECT_EQ(stack.currentItem(), first);
    EXPECT_EQ(stack.initialItem(), first);
    EXPECT_EQ(first->parent(), &stack);

    ASSERT_TRUE(stack.setInitialItem(second, StackView::StackViewItemOwnership::DoesNotOwnItem));
    processDeferredDeletes();
    ASSERT_FALSE(firstPointer.isNull());
    EXPECT_EQ(first->parent(), nullptr);
    EXPECT_EQ(stack.depth(), 1);
    EXPECT_EQ(stack.currentItem(), second);
    EXPECT_EQ(stack.initialItem(), second);
    EXPECT_GE(initialSpy.count(), 2);

    ASSERT_TRUE(stack.clear());
    EXPECT_EQ(second->parent(), nullptr);
    delete first;
    delete second;
}

TEST_F(StackViewTest, MultiItemPushKeepsOnlyLastItemCurrent)
{
    StackView stack;
    stack.setTransitionAnimationEnabled(false);
    auto* first = new StackPage("First");
    auto* second = new StackPage("Second");
    auto* third = new StackPage("Third");

    ASSERT_TRUE(stack.push(QVector<QWidget*>{first, second, third}));
    EXPECT_EQ(stack.depth(), 3);
    EXPECT_EQ(stack.initialItem(), first);
    EXPECT_EQ(stack.currentItem(), third);
    EXPECT_EQ(stack.itemAt(0), first);
    EXPECT_EQ(stack.itemAt(1), second);
    EXPECT_EQ(stack.itemAt(2), third);
    EXPECT_EQ(stack.itemStatus(first), StackView::StackViewItemStatus::Inactive);
    EXPECT_EQ(stack.itemStatus(second), StackView::StackViewItemStatus::Inactive);
    EXPECT_EQ(stack.itemStatus(third), StackView::StackViewItemStatus::Active);
}

TEST_F(StackViewTest, PopToRootPopToItemAndGoBackNavigatePredictably)
{
    StackView stack;
    stack.setTransitionAnimationEnabled(false);
    auto* first = new StackPage("First");
    auto* second = new StackPage("Second");
    auto* third = new StackPage("Third");
    auto* fourth = new StackPage("Fourth");

    ASSERT_TRUE(stack.push(first));
    ASSERT_TRUE(stack.push(second));
    ASSERT_TRUE(stack.push(third));
    ASSERT_TRUE(stack.push(fourth));

    EXPECT_TRUE(stack.popToItem(second));
    EXPECT_EQ(stack.depth(), 2);
    EXPECT_EQ(stack.currentItem(), second);

    ASSERT_TRUE(stack.push(new StackPage("Third again")));
    ASSERT_TRUE(stack.push(new StackPage("Fourth again")));
    EXPECT_TRUE(stack.popToRoot());
    EXPECT_EQ(stack.depth(), 1);
    EXPECT_EQ(stack.currentItem(), first);

    ASSERT_TRUE(stack.push(new StackPage("Second again")));
    EXPECT_TRUE(stack.goBack());
    EXPECT_EQ(stack.currentItem(), first);
    EXPECT_FALSE(stack.goBack());
}

TEST_F(StackViewTest, StatusSignalsPreserveLifecycleOrderWithoutAnimation)
{
    StackView stack;
    stack.setTransitionAnimationEnabled(false);
    auto* first = new StackPage("First");
    auto* second = new StackPage("Second");
    ASSERT_TRUE(stack.push(first));

    QSignalSpy statusSpy(&stack, &StackView::itemStatusChanged);
    ASSERT_TRUE(stack.push(second));

    ASSERT_GE(statusSpy.count(), 4);
    EXPECT_EQ(statusSpy.at(0).at(0).value<QWidget*>(), first);
    EXPECT_EQ(statusAt(statusSpy, 0), StackView::StackViewItemStatus::Deactivating);
    EXPECT_EQ(statusSpy.at(1).at(0).value<QWidget*>(), second);
    EXPECT_EQ(statusAt(statusSpy, 1), StackView::StackViewItemStatus::Activating);
    EXPECT_EQ(statusSpy.at(2).at(0).value<QWidget*>(), second);
    EXPECT_EQ(statusAt(statusSpy, 2), StackView::StackViewItemStatus::Active);
    EXPECT_EQ(statusSpy.at(3).at(0).value<QWidget*>(), first);
    EXPECT_EQ(statusAt(statusSpy, 3), StackView::StackViewItemStatus::Inactive);
}

TEST_F(StackViewTest, BusyRejectsConcurrentStackOperations)
{
    StackView stack;
    stack.resize(320, 220);
    stack.setTransitionDuration(160);
    showAndProcess(stack);

    ASSERT_TRUE(stack.push(new StackPage("First")));
    auto* second = new StackPage("Second");
    auto* rejected = new StackPage("Rejected");
    ASSERT_TRUE(stack.push(second));
    EXPECT_TRUE(stack.busy());
    EXPECT_FALSE(stack.push(rejected));
    EXPECT_FALSE(stack.pop());
    EXPECT_EQ(stack.depth(), 2);

    QTest::qWait(stack.transitionDuration() + 80);
    QApplication::processEvents();
    EXPECT_FALSE(stack.busy());
    EXPECT_EQ(stack.currentItem(), second);
    EXPECT_EQ(rejected->parent(), nullptr);
    delete rejected;
}

TEST_F(StackViewTest, OwnershipAndExternalDestructionAreHandled)
{
    StackView stack;
    stack.setTransitionAnimationEnabled(false);
    auto* root = new StackPage("Root");
    ASSERT_TRUE(stack.push(root));

    auto* owned = new StackPage("Owned");
    QPointer<QWidget> ownedPointer = owned;
    ASSERT_TRUE(stack.push(owned));
    ASSERT_TRUE(stack.pop());
    processDeferredDeletes();
    EXPECT_TRUE(ownedPointer.isNull());
    EXPECT_EQ(stack.depth(), 1);

    auto* external = new StackPage("External");
    QPointer<QWidget> externalPointer = external;
    ASSERT_TRUE(stack.push(external, StackView::StackViewItemOwnership::DoesNotOwnItem));
    ASSERT_TRUE(stack.pop());
    processDeferredDeletes();
    ASSERT_FALSE(externalPointer.isNull());
    EXPECT_EQ(external->parent(), nullptr);
    delete external;

    auto* victim = new StackPage("Victim");
    ASSERT_TRUE(stack.push(victim, StackView::StackViewItemOwnership::DoesNotOwnItem));
    delete victim;
    QApplication::processEvents();
    EXPECT_EQ(stack.depth(), 1);
    EXPECT_EQ(stack.currentItem(), root);
}

TEST_F(StackViewTest, ExternalRemoveWidgetPrunesStackEntries)
{
    StackView stack;
    stack.setTransitionAnimationEnabled(false);
    auto* root = new StackPage("Root");
    auto* middle = new StackPage("Middle");
    auto* top = new StackPage("Top");
    ASSERT_TRUE(stack.push(root));
    ASSERT_TRUE(stack.push(middle, StackView::StackViewItemOwnership::DoesNotOwnItem));
    ASSERT_TRUE(stack.push(top, StackView::StackViewItemOwnership::DoesNotOwnItem));

    stack.removeWidget(middle);
    QApplication::processEvents();
    EXPECT_EQ(stack.depth(), 2);
    EXPECT_FALSE(stack.contains(middle));
    EXPECT_EQ(stack.currentItem(), top);
    middle->setParent(nullptr);
    delete middle;

    stack.removeWidget(top);
    QApplication::processEvents();
    EXPECT_EQ(stack.depth(), 1);
    EXPECT_FALSE(stack.contains(top));
    EXPECT_EQ(stack.currentItem(), root);
    top->setParent(nullptr);
    delete top;
}

TEST_F(StackViewTest, DirectQStackedWidgetCurrentIndexCanAdoptWidgets)
{
    StackView stack;
    stack.setTransitionAnimationEnabled(false);
    auto* first = new StackPage("First");
    auto* second = new StackPage("Second");

    stack.addWidget(first);
    EXPECT_EQ(stack.depth(), 1);
    EXPECT_EQ(stack.currentItem(), first);
    stack.addWidget(second);
    stack.setCurrentIndex(stack.QStackedWidget::indexOf(second));
    EXPECT_EQ(stack.depth(), 2);
    EXPECT_EQ(stack.currentItem(), second);
    EXPECT_EQ(stack.itemAt(1), second);
}

TEST_F(StackViewTest, KeyboardBackAndLayoutThemeBehavior)
{
    StackView stack;
    stack.resize(320, 220);
    stack.setTransitionAnimationEnabled(false);
    showAndProcess(stack);
    auto* first = new StackPage("First");
    auto* second = new StackPage("Second");
    ASSERT_TRUE(stack.push(first));
    ASSERT_TRUE(stack.push(second));

    stack.setFocus();
    sendKey(&stack, Qt::Key_Backspace);
    EXPECT_EQ(stack.currentItem(), first);
    EXPECT_EQ(stack.depth(), 1);

    ASSERT_TRUE(stack.push(new StackPage("Vertical")));
    stack.setOrientation(Qt::Vertical);
    EXPECT_EQ(stack.orientation(), Qt::Vertical);
    stack.resize(480, 300);
    QApplication::processEvents();
    EXPECT_EQ(stack.currentItem()->geometry(), stack.rect());

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    QApplication::processEvents();
    EXPECT_EQ(stack.depth(), 2);
    EXPECT_EQ(stack.currentItem()->geometry(), stack.rect());
}

TEST_F(StackViewTest, AnimatedTransitionFinishesWithCorrectVisibility)
{
    StackView stack;
    stack.resize(340, 220);
    stack.setTransitionDuration(80);
    showAndProcess(stack);

    auto* first = new StackPage("First");
    auto* second = new StackPage("Second");
    ASSERT_TRUE(stack.push(first));
    ASSERT_TRUE(stack.push(second));
    EXPECT_TRUE(stack.busy());
    stack.resize(420, 260);

    QTest::qWait(stack.transitionDuration() + 80);
    QApplication::processEvents();
    EXPECT_FALSE(stack.busy());
    EXPECT_EQ(stack.currentItem(), second);
    EXPECT_EQ(second->geometry(), stack.rect());
    EXPECT_TRUE(second->isVisible());
    EXPECT_FALSE(first->isVisible());
}

TEST_F(StackViewTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        GTEST_SKIP() << "Skipping visual test in offscreen mode";
    }

    auto* window = new StackViewTestWindow();
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setWindowTitle(QStringLiteral("StackView VisualCheck"));
    window->resize(820, 560);
    window->onThemeUpdated();

    auto* rootLayout = new fluent::AnchorLayout();
    window->setLayout(rootLayout);

    auto* title = new QLabel(QStringLiteral("Fluent StackView"), window);
    QFont titleFont = title->font();
    titleFont.setPixelSize(24);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto* orientationButton = new QPushButton(QStringLiteral("Orientation"), window);
    orientationButton->setFixedSize(112, 32);
    auto* themeButton = new QPushButton(QStringLiteral("Theme"), window);
    themeButton->setFixedSize(84, 32);

    auto* stack = new StackView(window);
    stack->setMinimumSize(640, 360);

    auto* pager = new PipsPager(window);
    pager->setMaxVisiblePips(7);
    
    auto* controls = new QWidget(window);
    controls->setFixedHeight(36);
    auto* controlsLayout = new fluent::AnchorLayout();
    controls->setLayout(controlsLayout);

    auto* pushButton = new QPushButton(QStringLiteral("Push"), window);
    pushButton->setFixedSize(76, 32);
    auto* popButton = new QPushButton(QStringLiteral("Pop"), window);
    popButton->setFixedSize(76, 32);
    auto* replaceButton = new QPushButton(QStringLiteral("Replace"), window);
    replaceButton->setFixedSize(96, 32);
    auto* statusLabel = new QLabel(window);
    statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    statusLabel->setMinimumWidth(220);

    fluent::AnchorLayout::Anchors titleAnchors;
    titleAnchors.left = {window, Edge::Left, 28};
    titleAnchors.top = {window, Edge::Top, 24};
    titleAnchors.right = {orientationButton, Edge::Left, -18};
    rootLayout->addAnchoredWidget(title, titleAnchors);

    fluent::AnchorLayout::Anchors themeAnchors;
    themeAnchors.top = {window, Edge::Top, 24};
    themeAnchors.right = {window, Edge::Right, -28};
    rootLayout->addAnchoredWidget(themeButton, themeAnchors);

    fluent::AnchorLayout::Anchors orientationAnchors;
    orientationAnchors.top = {window, Edge::Top, 24};
    orientationAnchors.right = {themeButton, Edge::Left, -10};
    rootLayout->addAnchoredWidget(orientationButton, orientationAnchors);

    fluent::AnchorLayout::Anchors controlsAnchors;
    controlsAnchors.left = {window, Edge::Left, 28};
    controlsAnchors.right = {window, Edge::Right, -28};
    controlsAnchors.bottom = {window, Edge::Bottom, -28};
    rootLayout->addAnchoredWidget(controls, controlsAnchors);

    fluent::AnchorLayout::Anchors pagerAnchors;
    pagerAnchors.horizontalCenter = {window, Edge::HCenter, 0};
    pagerAnchors.bottom = {controls, Edge::Top, -16};
    rootLayout->addAnchoredWidget(pager, pagerAnchors);

    fluent::AnchorLayout::Anchors stackAnchors;
    stackAnchors.left = {window, Edge::Left, 28};
    stackAnchors.right = {window, Edge::Right, -28};
    stackAnchors.top = {title, Edge::Bottom, 18};
    stackAnchors.bottom = {pager, Edge::Top, -16};
    rootLayout->addAnchoredWidget(stack, stackAnchors);

    fluent::AnchorLayout::Anchors pushAnchors;
    pushAnchors.left = {controls, Edge::Left, 0};
    pushAnchors.verticalCenter = {controls, Edge::VCenter, 0};
    controlsLayout->addAnchoredWidget(pushButton, pushAnchors);

    fluent::AnchorLayout::Anchors popAnchors;
    popAnchors.left = {pushButton, Edge::Right, 8};
    popAnchors.verticalCenter = {controls, Edge::VCenter, 0};
    controlsLayout->addAnchoredWidget(popButton, popAnchors);

    fluent::AnchorLayout::Anchors replaceAnchors;
    replaceAnchors.left = {popButton, Edge::Right, 8};
    replaceAnchors.verticalCenter = {controls, Edge::VCenter, 0};
    controlsLayout->addAnchoredWidget(replaceButton, replaceAnchors);

    fluent::AnchorLayout::Anchors statusAnchors;
    statusAnchors.right = {controls, Edge::Right, 0};
    statusAnchors.verticalCenter = {controls, Edge::VCenter, 0};
    controlsLayout->addAnchoredWidget(statusLabel, statusAnchors);

    const QList<QColor> pageColors = {
        QColor("#D7E8F7"),
        QColor("#F7D7DB"),
        QColor("#DDF0D1"),
        QColor("#F3E5B5"),
        QColor("#E4D7F7"),
        QColor("#D8F2EF")
    };
    int nextPageNumber = 1;
    auto createPage = [&pageColors, &nextPageNumber](const QString& prefix) {
        const int pageNumber = nextPageNumber++;
        return new StackPage(QStringLiteral("%1 %2").arg(prefix).arg(pageNumber),
                             pageColors.at(pageNumber % pageColors.size()));
    };

    stack->push(new StackPage(QStringLiteral("Home"), pageColors.first()));

    bool syncingPager = false;
    auto syncNavigation = [stack, pager, pushButton, popButton, replaceButton, statusLabel, window, &syncingPager]() {
        syncingPager = true;
        const int depth = stack->depth();
        const int pageCount = stack->count();
        const int currentIndex = pageCount > 0
            ? qBound(0, stack->currentIndex(), pageCount - 1)
            : 0;
        pager->setNumberOfPages(pageCount);
        pager->setSelectedPageIndex(currentIndex);
        pager->setEnabled(depth > 0 && !stack->busy());
        pushButton->setEnabled(!stack->busy());
        popButton->setEnabled(!stack->busy() && stack->canPop());
        replaceButton->setEnabled(!stack->busy() && depth > 0);

        const QString currentName = stack->currentItem()
            ? stack->currentItem()->objectName()
            : QStringLiteral("None");
        statusLabel->setText(QStringLiteral("%1 · index %2 · depth %3%4")
                                 .arg(currentName)
                                 .arg(currentIndex)
                                 .arg(depth)
                                 .arg(stack->busy() ? QStringLiteral(" · busy") : QString()));
        if (window->layout()) {
            window->layout()->invalidate();
            window->layout()->activate();
        }
        syncingPager = false;
    };
    QObject::connect(stack, &StackView::depthChanged, statusLabel, syncNavigation);
    QObject::connect(stack, &StackView::currentItemChanged, statusLabel, syncNavigation);
    QObject::connect(stack, &StackView::busyChanged, statusLabel, syncNavigation);
    syncNavigation();

    QObject::connect(pager, &PipsPager::selectedPageIndexChanged, stack, [stack, &syncingPager](int index) {
        if (syncingPager || stack->busy())
            return;
        if (index >= 0 && index < stack->count() && index != stack->currentIndex())
            stack->setCurrentIndex(index);
    });
    QObject::connect(pushButton, &QPushButton::clicked, stack, [stack, createPage]() mutable {
        stack->push(createPage(QStringLiteral("Page")));
    });
    QObject::connect(popButton, &QPushButton::clicked, stack, &StackView::goBack);
    QObject::connect(replaceButton, &QPushButton::clicked, stack, [stack, createPage]() mutable {
        stack->replace(createPage(QStringLiteral("Replacement")));
    });
    QObject::connect(orientationButton, &QPushButton::clicked, stack, [stack]() {
        stack->setOrientation(stack->orientation() == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal);
    });
    QObject::connect(themeButton, &QPushButton::clicked, window, [window]() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                                    ? fluent::FluentElement::Dark
                                    : fluent::FluentElement::Light);
        window->onThemeUpdated();
    });

    window->show();
    qApp->exec();
}
