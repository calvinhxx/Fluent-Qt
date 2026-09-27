#include <gtest/gtest.h>

#include "components/foundation/overlay/OverlayPresentation_p.h"
#include "components/foundation/overlay/OverlayPresentation.h"
#include "components/basicinput/ComboBox.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/dialogs_flyouts/TeachingTip.h"
#include "components/dialogs_flyouts/ContentDialog.h"
#include "components/collections/DrawerView.h"
#include "components/status_info/ToolTip.h"
#include <QApplication>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHelpEvent>
#include <QTest>
#include <limits>
#include "design/Elevation.h"

namespace {

TEST(FoundationContractsTest, Contract_PublicPresentationDefaultsAndInvalidTransforms)
{
    namespace presentation = fluent::overlay::presentation;
    QWidget window;
    QWidget root(&window);
    root.move(31, 47);
    QWidget child(&root);
    child.move(11, 13);
    const QPoint point(17, 19);
    const QPoint native = child.mapToGlobal(point);
    EXPECT_FALSE(presentation::hasTransform(&child));
    EXPECT_EQ(presentation::mapToGlobal(&child, point), native);
    presentation::setTransform(&root, QTransform::fromTranslate(23, 29));
    EXPECT_TRUE(presentation::hasTransform(&child));
    EXPECT_EQ(presentation::mapToGlobal(&child, point), native + QPoint(23, 29));
    QWidget nativeChild(&child, Qt::Tool);
    EXPECT_FALSE(presentation::hasTransform(&nativeChild));
    presentation::clearTransform(&root);
    EXPECT_EQ(presentation::mapToGlobal(&child, point), native);
    for (const QTransform& invalid :
         {QTransform::fromScale(0, 1),
          QTransform(1, 0, 0, 0, 1, 0, std::numeric_limits<qreal>::infinity(), 0, 1),
          QTransform(1, 0, 0, 0, 1, 0, std::numeric_limits<qreal>::quiet_NaN(), 0, 1)}) {
        presentation::setTransform(&root, QTransform::fromTranslate(23, 29));
        presentation::setTransform(&root, invalid);
        EXPECT_FALSE(presentation::hasTransform(&child));
        EXPECT_EQ(presentation::mapToGlobal(&child, point), native);
    }
    presentation::setTransform(nullptr, QTransform());
    presentation::clearTransform(nullptr);
    EXPECT_FALSE(presentation::hasTransform(nullptr));
    EXPECT_EQ(presentation::mapToGlobal(nullptr, point), QPoint());
}

TEST(FoundationContractsTest, Contract_PublicMenuAnchorTracksLifetime)
{
    namespace presentation = fluent::overlay::presentation;
    QWidget window;
    window.resize(600, 420);
    QWidget root(&window);
    root.resize(window.size());
    auto* source = new QWidget(&root);
    source->setGeometry(40, 60, 100, 30);
    const QPoint point(7, 35);
    QMenu menu(&window);
    menu.addAction("Item");
    presentation::MenuAnchor seen;
    bool destroySource = false;
    QObject::connect(&menu, &QMenu::aboutToShow, &menu, [&] {
        seen = presentation::menuAnchor(&menu);
        if (destroySource)
            delete source;
    });
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    presentation::popupMenu(&menu, source, point);
    EXPECT_TRUE(seen.source.isNull());
    EXPECT_EQ(menu.pos(), source->mapToGlobal(point));
    menu.hide();
    presentation::setTransform(&root, QTransform::fromTranslate(23, 29));
    presentation::popupMenu(&menu, source, point);
    EXPECT_EQ(seen.source, source);
    EXPECT_EQ(seen.point, point);
    EXPECT_TRUE(presentation::menuAnchor(&menu).source.isNull());
    menu.hide();
    destroySource = true;
    presentation::popupMenu(&menu, source, point);
    EXPECT_TRUE(seen.source.isNull());
    EXPECT_TRUE(presentation::menuAnchor(&menu).source.isNull());
    EXPECT_TRUE(presentation::menuAnchor(nullptr).source.isNull());
    menu.hide();
}

TEST(FoundationContractsTest, Contract_OverlayPresentationPreserves2DAndNativeWindowBoundaries)
{
    using namespace fluent::overlay;
    QWidget window;
    QWidget root(&window);
    root.move(35, 48);
    QWidget child(&root);
    child.setGeometry(12, 21, 107, 39);
    EXPECT_EQ(presentedPointInTopLevel(&child, QPoint(17, 8)), child.mapTo(&window, QPoint(17, 8)));
    EXPECT_EQ(presentedRectInTopLevel(&child), QRect(child.mapTo(&window, QPoint()), child.size()));
    EXPECT_EQ(localPointFromPresentedGlobal(&child, child.mapToGlobal(QPoint(17, 8))),
              QPoint(17, 8));

    QTransform transform;
    transform.translate(41, 19);
    transform.scale(.9, 1.1);
    root.setProperty(presentationTransformPropertyName(), transform);
    const QPoint expected =
        transform.map(QPointF(child.pos() + QPoint(17, 8))).toPoint() + root.pos();
    EXPECT_EQ(presentedPointInTopLevel(&child, QPoint(17, 8)), expected);
    EXPECT_EQ(presentedRectInTopLevel(&child),
              transform.mapRect(QRectF(child.geometry())).translated(root.pos()).toAlignedRect());
    EXPECT_LE((localPointFromPresentedGlobal(&child, window.mapToGlobal(expected)) - QPoint(17, 8))
                  .manhattanLength(),
              1);

    QWidget native(&child, Qt::Tool);
    QWidget nativeChild(&native);
    nativeChild.setGeometry(9, 12, 50, 20);
    EXPECT_EQ(presentationRoot(&nativeChild), nullptr);
    EXPECT_EQ(presentedRectInTopLevel(&nativeChild), nativeChild.geometry());
    QWidget otherWindow;
    EXPECT_EQ(presentationRoot(&otherWindow), nullptr);
    child.setProperty(presentationTransformPropertyName(), QTransform::fromTranslate(4, 5));
    EXPECT_EQ(presentedPointInTopLevel(&child, QPoint(17, 8)),
              child.mapTo(&window, QPoint(21, 13)));
    child.setProperty(presentationTransformPropertyName(), QVariant());
    root.setProperty(presentationTransformPropertyName(), QVariant());
    EXPECT_EQ(presentedRectInTopLevel(&child), QRect(child.mapTo(&window, QPoint()), child.size()));
}

TEST(FoundationContractsTest, Contract_OnlyPresentationPropertyInvalidatesOverlayAnchor)
{
    using namespace fluent::overlay;
    QWidget window;
    QWidget root(&window);
    QWidget child(&root);
    QWidget unrelated(&window);
    QDynamicPropertyChangeEvent transformChanged(presentationTransformPropertyName());
    QDynamicPropertyChangeEvent unrelatedChanged("unrelatedProperty");
    EXPECT_TRUE(anchorGeometryMayChange(&root, &transformChanged, &child));
    EXPECT_FALSE(anchorGeometryMayChange(&root, &unrelatedChanged, &child));
    EXPECT_FALSE(anchorGeometryMayChange(&unrelated, &transformChanged, &child));
}

TEST(FoundationContractsTest, Contract_EmbeddedAnchorsComposeSceneAndHostPresentation)
{
    using namespace fluent::overlay;
    QWidget window;
    window.resize(900, 650);
    QWidget outer(&window);
    outer.setGeometry(20, 30, 840, 580);
    QGraphicsScene scene;
    QGraphicsView view(&scene, &outer);
    view.setGeometry(30, 40, 740, 460);
    view.setSceneRect(-300, -180, 600, 360);
    auto* card = new QWidget;
    card->resize(220, 140);
    QWidget child(card);
    child.setGeometry(15, 22, 130, 32);
    auto* proxy = scene.addWidget(card);
    proxy->setPos(-110, -70);
    proxy->setTransform(QTransform().rotate(8).scale(.85, .9));
    window.show();
    QApplication::processEvents();
    const QTransform hostTransform = QTransform().translate(21, 29).scale(.94, .97);
    presentation::setTransform(&outer, hostTransform);
    const QPoint point(33, 11);
    const QPointF inViewport =
        proxy->deviceTransform(view.viewportTransform()).map(QPointF(child.pos() + point));
    const QPoint expected =
        (hostTransform.map(inViewport + view.viewport()->mapTo(&outer, QPoint())) + outer.pos())
            .toPoint();
    EXPECT_EQ(presentedTopLevel(&child), &window);
    EXPECT_TRUE(presentation::hasTransform(&child));
    EXPECT_EQ(presentedPointInTopLevel(&child, point), expected);
    EXPECT_LE((localPointFromPresentedGlobal(&child, window.mapToGlobal(expected)) - point)
                  .manhattanLength(),
              2);
    EXPECT_TRUE(isAnchorVisibleInTopLevel(&child));
    QEvent moved(QEvent::Move);
    QDynamicPropertyChangeEvent transformed(presentationTransformPropertyName());
    EXPECT_TRUE(anchorGeometryMayChange(&outer, &transformed, &child));
    EXPECT_TRUE(anchorGeometryMayChange(&view, &moved, &child));
    outer.hide();
    EXPECT_FALSE(isAnchorVisibleInTopLevel(&child));
    outer.show();
    proxy->hide();
    EXPECT_FALSE(isAnchorVisibleInTopLevel(&child));
    // The scene owns the heap card, but the stack child must be detached first.
    child.setParent(nullptr);
}

TEST(FoundationContractsTest, Contract_EmbeddedPopupsUseNativeContainmentAndOwnership)
{
    using namespace fluent;
    QWidget window;
    window.resize(900, 650);
    QGraphicsScene scene;
    QGraphicsView view(&scene, &window);
    view.setGeometry(20, 30, 840, 560);
    view.setSceneRect(-300, -200, 600, 400);
    auto* card = new QWidget;
    card->resize(220, 90);
    auto* anchor = new basicinput::ComboBox(card);
    anchor->setGeometry(20, 35, 175, 32);
    for (int row = 0; row < 10; ++row)
        anchor->addItem(QString::number(row));
    auto* proxy = scene.addWidget(card);
    proxy->setPos(-110, -45);
    proxy->setTransform(QTransform().rotate(5));
    window.show();
    QApplication::processEvents();
    anchor->showPopup();
    auto* popup = window.findChild<QWidget*>("ComboBoxPopup");
    ASSERT_NE(popup, nullptr);
    EXPECT_EQ(popup->parentWidget(), &window);
    EXPECT_GT(popup->height(), card->height());
    EXPECT_TRUE(window.rect().contains(overlay::visibleCardGeometry(popup->geometry())));
    anchor->hidePopup();

    dialogs_flyouts::TeachingTip tip(anchor);
    tip.setAnimationEnabled(false);
    tip.setCardSize(QSize(200, 140));
    tip.showAt(anchor);
    EXPECT_EQ(tip.parentWidget(), &window);
    EXPECT_TRUE(window.rect().contains(tip.geometry()));
    tip.close();

    dialogs_flyouts::ContentDialog dialog(anchor);
    dialog.setAnimationEnabled(false);
    dialog.open();
    EXPECT_EQ(dialog.parentWidget(), &window);
    EXPECT_EQ(dialog.pos(), QPoint((window.width() - dialog.width()) / 2,
                                   (window.height() - dialog.height()) / 2));
    dialog.close();

    auto* tooltip =
        status_info::ToolTip::attach(anchor, "Embedded target", status_info::ToolTip::Below);
    tooltip->setAnimationEnabled(false);
    QHelpEvent help(QEvent::ToolTip, anchor->rect().center(),
                    overlay::presentedPointToGlobal(anchor, anchor->rect().center()));
    QApplication::sendEvent(anchor, &help);
    EXPECT_TRUE(tooltip->isVisible());
    EXPECT_EQ(tooltip->graphicsProxyWidget(), nullptr);
    const QRect target =
        overlay::presentedRectInTopLevel(anchor).translated(window.mapToGlobal(QPoint()));
    EXPECT_EQ(overlay::visibleCardGeometry(tooltip->geometry(), tooltip->shadowMargin()).top(),
              target.bottom() + 5);
    tooltip->hide();

    QMenu menu(anchor);
    menu.addAction("Native menu");
    const QPoint point(0, anchor->height());
    overlay::presentation::popupMenu(&menu, anchor, point);
    EXPECT_TRUE(menu.isVisible());
    EXPECT_EQ(menu.graphicsProxyWidget(), nullptr);
    EXPECT_EQ(menu.pos(), overlay::presentedPointToGlobal(anchor, point));
    menu.hide();
}

TEST(FoundationContractsTest, Contract_ProjectedAnchorsChoosePlacementBeforeContainment)
{
    using namespace fluent;
    QWidget window;
    window.resize(800, 600);
    QWidget root(&window);
    root.resize(window.size());
    basicinput::ComboBox anchor(&root);
    anchor.setGeometry(220, 140, 160, 32);
    for (int row = 0; row < 10; ++row)
        anchor.addItem(QString::number(row));
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    root.setProperty(overlay::presentationTransformPropertyName(),
                     QTransform::fromTranslate(25, 340));
    const QRect projected = overlay::presentedRectInTopLevel(&anchor);
    dialogs_flyouts::Flyout flyout(&anchor);
    flyout.setAnimationEnabled(false);
    flyout.setPlacement(dialogs_flyouts::Flyout::Auto);
    flyout.resize(232, 180);
    flyout.showAt(&anchor);
    EXPECT_EQ(overlay::visibleCardGeometry(flyout.geometry()).bottom(),
              projected.top() - flyout.anchorOffset() - 1);
    flyout.close();

    dialogs_flyouts::TeachingTip tip(&anchor);
    tip.setAnimationEnabled(false);
    tip.setTailVisible(false);
    tip.setCardSize(QSize(200, 148));
    tip.showAt(&anchor);
    EXPECT_EQ(overlay::visibleCardGeometry(tip.geometry()).bottom(),
              projected.top() - tip.placementMargin() - 1);
    tip.close();

    anchor.showPopup();
    auto* comboPopup = window.findChild<QWidget*>("ComboBoxPopup");
    ASSERT_NE(comboPopup, nullptr);
    EXPECT_EQ(overlay::visibleCardGeometry(comboPopup->geometry()).bottom(),
              projected.top() - anchor.popupOffset() - 1);
    anchor.hidePopup();

    root.setProperty(overlay::presentationTransformPropertyName(), QVariant());
    flyout.showAt(&anchor);
    const QRect logical(anchor.mapTo(&window, QPoint()), anchor.size());
    EXPECT_EQ(overlay::visibleCardGeometry(flyout.geometry()).top(),
              logical.bottom() + flyout.anchorOffset());
    root.setProperty(overlay::presentationTransformPropertyName(),
                     QTransform::fromTranslate(25, 340));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            return overlay::visibleCardGeometry(flyout.geometry()).bottom() ==
                   projected.top() - flyout.anchorOffset() - 1;
        },
        1000));
    flyout.close();
}

TEST(FoundationContractsTest, Contract_ProjectionAnchorsTooltipsButDoesNotMoveOwnerSurfaces)
{
    using namespace fluent;
    QWidget window;
    window.resize(800, 600);
    QWidget root(&window);
    root.resize(window.size());
    QWidget anchor(&root);
    anchor.setGeometry(220, 140, 160, 32);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    root.setProperty(overlay::presentationTransformPropertyName(),
                     QTransform::fromTranslate(25, 140));
    auto* tooltip =
        status_info::ToolTip::attach(&anchor, "Projected target", status_info::ToolTip::Below);
    tooltip->setAnimationEnabled(false);
    QHelpEvent help(QEvent::ToolTip, anchor.rect().center(),
                    anchor.mapToGlobal(anchor.rect().center()));
    QApplication::sendEvent(&anchor, &help);
    ASSERT_TRUE(tooltip->isVisible());
    const auto aligned = [&] {
        const QRect target =
            overlay::presentedRectInTopLevel(&anchor).translated(window.mapToGlobal(QPoint()));
        return overlay::visibleCardGeometry(tooltip->geometry(), tooltip->shadowMargin()).top() ==
               target.bottom() + 5;
    };
    EXPECT_TRUE(aligned());
    root.setProperty(overlay::presentationTransformPropertyName(),
                     QTransform::fromTranslate(35, 180));
    ASSERT_TRUE(QTest::qWaitFor(aligned, 1000));
    tooltip->hide();

    dialogs_flyouts::Dialog dialog(&anchor);
    dialog.setAnimationEnabled(false);
    dialog.resize(250, 180);
    dialog.open();
    EXPECT_EQ(dialog.parentWidget(), &window);
    EXPECT_FALSE(dialog.isWindow());
    EXPECT_EQ(dialog.pos(), QPoint((window.width() - dialog.width()) / 2,
                                   (window.height() - dialog.height()) / 2));
    dialog.close();
    dialogs_flyouts::ContentDialog contentDialog(&anchor);
    contentDialog.setAnimationEnabled(false);
    contentDialog.open();
    EXPECT_EQ(contentDialog.parentWidget(), &window);
    EXPECT_FALSE(contentDialog.isWindow());
    EXPECT_EQ(contentDialog.pos(), QPoint((window.width() - contentDialog.width()) / 2,
                                          (window.height() - contentDialog.height()) / 2));
    contentDialog.close();
    collections::DrawerView drawer(&anchor);
    drawer.setAnimationEnabled(false);
    drawer.open();
    EXPECT_EQ(drawer.parentWidget(), &window);
    EXPECT_FALSE(drawer.isWindow());
    const QRect projectedGeometry = drawer.geometry();
    root.setProperty(overlay::presentationTransformPropertyName(), QVariant());
    QApplication::processEvents();
    EXPECT_EQ(drawer.geometry(), projectedGeometry);
    drawer.close();
}

TEST(FoundationContractsTest, Contract_NormalOverlayCardClampsInsideAvailableBounds)
{
    const QRect bounds(10, 20, 100, 80);
    const QSize cardSize(30, 20);

    const QPoint clamped = fluent::overlay::clampCardTopLeft(QPoint(-20, 500), cardSize, bounds, 8);

    EXPECT_EQ(clamped, QPoint(18, 72));
    EXPECT_GE(clamped.x(), bounds.left() + 8);
    EXPECT_GE(clamped.y(), bounds.top() + 8);
    EXPECT_LE(clamped.x() + cardSize.width() - 1, bounds.right() - 8);
    EXPECT_LE(clamped.y() + cardSize.height() - 1, bounds.bottom() - 8);
}

TEST(FoundationContractsTest, Contract_OversizedOverlayCardUsesStableAvailableOrigin)
{
    const QRect bounds(10, 20, 100, 80);
    const QSize oversizedCard(300, 200);

    const QPoint clamped =
        fluent::overlay::clampCardTopLeft(QPoint(70, 60), oversizedCard, bounds, 8);

    EXPECT_EQ(clamped, QPoint(18, 28));
}

TEST(FoundationContractsTest, Contract_ElevationNoneHasNoVisibleShadow)
{
    for (bool dark : {false, true}) {
        const Elevation::ShadowParams& shadow = Elevation::getShadow(Elevation::None, dark);

        EXPECT_EQ(shadow.offsetX, 0);
        EXPECT_EQ(shadow.offsetY, 0);
        EXPECT_EQ(shadow.blurRadius, 0);
        EXPECT_EQ(shadow.spreadRadius, 0);
        EXPECT_DOUBLE_EQ(shadow.opacity, 0.0);
    }
}

} // namespace
