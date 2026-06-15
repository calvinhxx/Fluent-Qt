#include <gtest/gtest.h>

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPaintEvent>
#include <QSignalSpy>
#include <QTest>
#include <QWidget>

#include "components/basicinput/Button.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/status_info/Shimmer.h"
#include "components/textfields/Label.h"
#include "design/CornerRadius.h"

using namespace fluent::status_info;

namespace {

QImage renderWidget(QWidget& widget, const QSize& size)
{
    widget.resize(size);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    widget.render(&image);
    return image;
}

bool hasVisiblePixel(const QImage& image)
{
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 0)
                return true;
        }
    }
    return false;
}

bool imagesDiffer(const QImage& left, const QImage& right)
{
    if (left.size() != right.size())
        return true;
    for (int y = 0; y < left.height(); ++y) {
        for (int x = 0; x < left.width(); ++x) {
            if (left.pixel(x, y) != right.pixel(x, y))
                return true;
        }
    }
    return false;
}

QImage renderPainterPhase(qreal progress)
{
    QImage image(QSize(180, 88), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);

    ShimmerPainter::Palette palette;
    palette.baseColor = QColor(0, 0, 0, 24);
    palette.highlightColor = QColor(255, 255, 255, 180);
    palette.borderColor = QColor(0, 0, 0, 20);
    ShimmerPainter::paint(&painter,
                          ShimmerPainter::imageCardElements(QRectF(10, 10, 160, 68), 6.0),
                          palette,
                          progress,
                          true);
    return image;
}

class ShimmerTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override
    {
        QPalette p = palette();
        p.setColor(QPalette::Window, themeColors().bgCanvas);
        setPalette(p);
        setAutoFillBackground(true);
    }
};

class SampleCard : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
public:
    explicit SampleCard(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_StyledBackground, false);
        setAutoFillBackground(false);
    }

    void onThemeUpdated() override { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const auto colors = themeColors();
        const QRectF surface = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath path;
        path.addRoundedRect(surface, CornerRadius::Overlay, CornerRadius::Overlay);

        painter.setPen(QPen(colors.strokeCard, 1.0));
        painter.setBrush(colors.bgLayer);
        painter.drawPath(path);
    }
};

fluent::textfields::Label* makeLabel(const QString& text,
                                     QWidget* parent,
                                     const QString& typography)
{
    auto* label = new fluent::textfields::Label(text, parent);
    label->setFluentTypography(typography);
    label->setTextElideMode(Qt::ElideRight);
    return label;
}

SampleCard* makeCard(QWidget* parent,
                     fluent::AnchorLayout* rootLayout,
                     const QString& titleText,
                     const QString& captionText)
{
    auto* card = new SampleCard(parent);
    auto* cardLayout = new fluent::AnchorLayout(card);
    card->setLayout(cardLayout);

    using Edge = fluent::AnchorLayout::Edge;
    auto* title = makeLabel(titleText, card, QStringLiteral("BodyStrong"));
    title->anchors()->top = {card, Edge::Top, 18};
    title->anchors()->left = {card, Edge::Left, 18};
    title->anchors()->right = {card, Edge::Right, -18};
    cardLayout->addWidget(title);

    auto* caption = makeLabel(captionText, card, QStringLiteral("Caption"));
    caption->anchors()->top = {title, Edge::Bottom, 6};
    caption->anchors()->left = {card, Edge::Left, 18};
    caption->anchors()->right = {card, Edge::Right, -18};
    cardLayout->addWidget(caption);

    rootLayout->addWidget(card);
    return card;
}

Shimmer* makeCustomTileSkeleton(QWidget* parent)
{
    auto* shimmer = new Shimmer(parent);
    shimmer->setFixedSize(330, 190);
    using Element = ShimmerPainter::Element;
    using Shape = ShimmerPainter::Shape;
    shimmer->setElements({
        Element(Shape::RoundedRect, QRectF(0, 0, 330, 94), 8.0),
        Element(Shape::Circle, QRectF(0, 116, 38, 38)),
        Element(Shape::Line, QRectF(52, 119, 190, 12)),
        Element(Shape::Line, QRectF(52, 141, 132, 12)),
        Element(Shape::RoundedRect, QRectF(255, 118, 72, 30), 6.0),
        Element(Shape::Line, QRectF(0, 174, 300, 10)),
    });
    return shimmer;
}

Shimmer* makeDashboardSkeleton(QWidget* parent)
{
    auto* shimmer = new Shimmer(parent);
    shimmer->setFixedSize(330, 138);
    using Element = ShimmerPainter::Element;
    using Shape = ShimmerPainter::Shape;
    shimmer->setElements({
        Element(Shape::Line, QRectF(0, 0, 148, 14)),
        Element(Shape::Line, QRectF(0, 26, 212, 10)),
        Element(Shape::RoundedRect, QRectF(0, 52, 92, 52), 7.0),
        Element(Shape::RoundedRect, QRectF(112, 52, 92, 52), 7.0),
        Element(Shape::RoundedRect, QRectF(224, 52, 92, 52), 7.0),
        Element(Shape::Line, QRectF(0, 124, 290, 10)),
    });
    return shimmer;
}

} // namespace

class ShimmerTest : public ::testing::Test {
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

TEST_F(ShimmerTest, DefaultPropertyValues)
{
    Shimmer shimmer;
    EXPECT_TRUE(shimmer.isActive());
    EXPECT_TRUE(shimmer.isAnimationEnabled());
    EXPECT_DOUBLE_EQ(shimmer.shimmerProgress(), 0.0);
    EXPECT_EQ(shimmer.cycleDuration(), 1400);
    EXPECT_EQ(shimmer.shimmerTemplate(), Shimmer::ShimmerTemplate::TextBlock);
    EXPECT_FALSE(shimmer.isAnimationRunning());
    EXPECT_EQ(shimmer.sizeHint(), QSize(240, 72));
}

TEST_F(ShimmerTest, PropertySignalsAndValidation)
{
    Shimmer shimmer;

    QSignalSpy activeSpy(&shimmer, &Shimmer::activeChanged);
    shimmer.setActive(false);
    EXPECT_FALSE(shimmer.isActive());
    EXPECT_EQ(activeSpy.count(), 1);
    shimmer.setActive(false);
    EXPECT_EQ(activeSpy.count(), 1);

    QSignalSpy animationSpy(&shimmer, &Shimmer::animationEnabledChanged);
    shimmer.setAnimationEnabled(false);
    EXPECT_FALSE(shimmer.isAnimationEnabled());
    EXPECT_EQ(animationSpy.count(), 1);
    shimmer.setAnimationEnabled(false);
    EXPECT_EQ(animationSpy.count(), 1);

    QSignalSpy progressSpy(&shimmer, &Shimmer::shimmerProgressChanged);
    shimmer.setShimmerProgress(1.25);
    EXPECT_DOUBLE_EQ(shimmer.shimmerProgress(), 0.25);
    EXPECT_EQ(progressSpy.count(), 1);

    QSignalSpy durationSpy(&shimmer, &Shimmer::cycleDurationChanged);
    shimmer.setCycleDuration(100);
    EXPECT_EQ(shimmer.cycleDuration(), 250);
    EXPECT_EQ(durationSpy.count(), 1);

    QSignalSpy templateSpy(&shimmer, &Shimmer::shimmerTemplateChanged);
    shimmer.setShimmerTemplate(Shimmer::ShimmerTemplate::ImageCard);
    EXPECT_EQ(shimmer.shimmerTemplate(), Shimmer::ShimmerTemplate::ImageCard);
    EXPECT_EQ(templateSpy.count(), 1);
    EXPECT_EQ(shimmer.sizeHint(), QSize(240, 140));
}

TEST_F(ShimmerTest, CustomElementsSwitchTemplateAndRender)
{
    Shimmer shimmer;
    const QVector<ShimmerPainter::Element> elements{
        {ShimmerPainter::Shape::Circle, QRectF(8, 8, 28, 28)},
        {ShimmerPainter::Shape::Line, QRectF(48, 12, 96, 12)}
    };
    QSignalSpy elementsSpy(&shimmer, &Shimmer::elementsChanged);
    shimmer.setElements(elements);

    EXPECT_EQ(shimmer.shimmerTemplate(), Shimmer::ShimmerTemplate::Custom);
    EXPECT_EQ(shimmer.elements().size(), 2);
    EXPECT_EQ(elementsSpy.count(), 1);

    const QImage image = renderWidget(shimmer, QSize(180, 56));
    EXPECT_TRUE(hasVisiblePixel(image));
}

TEST_F(ShimmerTest, InactiveDoesNotRenderSkeleton)
{
    Shimmer shimmer;
    shimmer.setActive(false);

    const QImage image = renderWidget(shimmer, QSize(180, 56));
    EXPECT_FALSE(hasVisiblePixel(image));
}

TEST_F(ShimmerTest, PainterTemplatesProduceExpectedElementCounts)
{
    EXPECT_EQ(ShimmerPainter::imageCardElements(QRectF(0, 0, 160, 90)).size(), 1);
    EXPECT_EQ(ShimmerPainter::avatarTextRowElements(QRectF(0, 0, 220, 56)).size(), 3);
    EXPECT_EQ(ShimmerPainter::textBlockElements(QRectF(0, 0, 220, 80), 4).size(), 4);
    EXPECT_TRUE(ShimmerPainter::textBlockElements(QRectF(0, 0, 220, 80), 0).isEmpty());
}

TEST_F(ShimmerTest, PainterProgressMovesHighlight)
{
    const QImage first = renderPainterPhase(0.05);
    const QImage second = renderPainterPhase(0.65);

    EXPECT_TRUE(hasVisiblePixel(first));
    EXPECT_TRUE(hasVisiblePixel(second));
    EXPECT_TRUE(imagesDiffer(first, second));
}

TEST_F(ShimmerTest, AnimationLifecycleTracksVisibilityAndActiveState)
{
    Shimmer shimmer;
    shimmer.resize(240, 72);
    shimmer.show();
    QVERIFY(QTest::qWaitForWindowExposed(&shimmer));
    EXPECT_TRUE(shimmer.isAnimationRunning());

    const qreal before = shimmer.shimmerProgress();
    QTest::qWait(40);
    EXPECT_NE(shimmer.shimmerProgress(), before);

    shimmer.setActive(false);
    EXPECT_FALSE(shimmer.isAnimationRunning());

    shimmer.setActive(true);
    shimmer.setAnimationEnabled(false);
    EXPECT_FALSE(shimmer.isAnimationRunning());

    shimmer.setAnimationEnabled(true);
    EXPECT_TRUE(shimmer.isAnimationRunning());

    shimmer.hide();
    EXPECT_FALSE(shimmer.isAnimationRunning());
}

TEST_F(ShimmerTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST"))
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";

    auto* window = new ShimmerTestWindow();
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->resize(920, 640);
    window->setMinimumSize(760, 560);
    window->setWindowTitle(QStringLiteral("Shimmer VisualCheck"));
    window->onThemeUpdated();

    using Edge = fluent::AnchorLayout::Edge;
    auto* layout = new fluent::AnchorLayout(window);
    window->setLayout(layout);

    auto* title = makeLabel(QStringLiteral("Shimmer loading states"), window, QStringLiteral("Title"));
    title->setFluentTypography(QStringLiteral("Title"));
    title->anchors()->top = {window, Edge::Top, 24};
    title->anchors()->left = {window, Edge::Left, 32};
    title->anchors()->right = {window, Edge::Right, -220};
    layout->addWidget(title);

    auto* description = makeLabel(
        QStringLiteral("Use Shimmer while content is loading. The painter helper can also draw skeletons inside delegates."),
        window,
        QStringLiteral("Body"));
    description->anchors()->top = {title, Edge::Bottom, 8};
    description->anchors()->left = {window, Edge::Left, 32};
    description->anchors()->right = {window, Edge::Right, -220};
    layout->addWidget(description);

    auto* themeButton = new fluent::basicinput::Button(QStringLiteral("Switch theme"), window);
    themeButton->setFluentStyle(fluent::basicinput::Button::Accent);
    themeButton->setFixedSize(132, 34);
    themeButton->anchors()->top = {window, Edge::Top, 30};
    themeButton->anchors()->right = {window, Edge::Right, -32};
    layout->addWidget(themeButton);
    QObject::connect(themeButton, &fluent::basicinput::Button::clicked, []() {
        fluent::FluentElement::setTheme(
            fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                ? fluent::FluentElement::Dark
                : fluent::FluentElement::Light);
    });

    auto* templatesCard = makeCard(window,
                                   layout,
                                   QStringLiteral("Built-in templates"),
                                   QStringLiteral("Image card, avatar row, and text block templates."));
    templatesCard->setFixedSize(404, 250);
    templatesCard->anchors()->top = {description, Edge::Bottom, 26};
    templatesCard->anchors()->left = {window, Edge::Left, 32};

    auto* image = new Shimmer(templatesCard);
    image->setShimmerTemplate(Shimmer::ShimmerTemplate::ImageCard);
    image->setFixedSize(166, 96);
    image->anchors()->top = {templatesCard, Edge::Top, 82};
    image->anchors()->left = {templatesCard, Edge::Left, 18};
    qobject_cast<fluent::AnchorLayout*>(templatesCard->layout())->addWidget(image);

    auto* row = new Shimmer(templatesCard);
    row->setShimmerTemplate(Shimmer::ShimmerTemplate::AvatarTextRow);
    row->setFixedSize(178, 58);
    row->anchors()->top = {templatesCard, Edge::Top, 82};
    row->anchors()->left = {image, Edge::Right, 22};
    qobject_cast<fluent::AnchorLayout*>(templatesCard->layout())->addWidget(row);

    auto* text = new Shimmer(templatesCard);
    text->setShimmerTemplate(Shimmer::ShimmerTemplate::TextBlock);
    text->setFixedSize(350, 46);
    text->anchors()->top = {image, Edge::Bottom, 14};
    text->anchors()->left = {templatesCard, Edge::Left, 18};
    qobject_cast<fluent::AnchorLayout*>(templatesCard->layout())->addWidget(text);

    auto* tileCard = makeCard(window,
                              layout,
                              QStringLiteral("Gallery card loading"),
                              QStringLiteral("A custom skeleton for remote image cards."));
    tileCard->setFixedSize(404, 250);
    tileCard->anchors()->top = {templatesCard, Edge::Top, 0};
    tileCard->anchors()->left = {templatesCard, Edge::Right, 28};
    tileCard->anchors()->right = {window, Edge::Right, -32};

    auto* tile = makeCustomTileSkeleton(tileCard);
    tile->anchors()->top = {tileCard, Edge::Top, 78};
    tile->anchors()->left = {tileCard, Edge::Left, 22};
    qobject_cast<fluent::AnchorLayout*>(tileCard->layout())->addWidget(tile);

    auto* listCard = makeCard(window,
                              layout,
                              QStringLiteral("List rows"),
                              QStringLiteral("Repeated row placeholders for async feeds or search results."));
    listCard->setFixedSize(404, 246);
    listCard->anchors()->top = {templatesCard, Edge::Bottom, 24};
    listCard->anchors()->left = {window, Edge::Left, 32};

    QWidget* previousRow = nullptr;
    for (int i = 0; i < 3; ++i) {
        auto* skeleton = new Shimmer(listCard);
        skeleton->setShimmerTemplate(Shimmer::ShimmerTemplate::AvatarTextRow);
        skeleton->setFixedSize(348, 52);
        skeleton->anchors()->top = previousRow
            ? fluent::AnchorLayout::Anchor{previousRow, Edge::Bottom, 6}
            : fluent::AnchorLayout::Anchor{listCard, Edge::Top, 78};
        skeleton->anchors()->left = {listCard, Edge::Left, 22};
        qobject_cast<fluent::AnchorLayout*>(listCard->layout())->addWidget(skeleton);
        previousRow = skeleton;
    }

    auto* dashboardCard = makeCard(window,
                                   layout,
                                   QStringLiteral("Dashboard section"),
                                   QStringLiteral("Composed elements can represent metrics and charts."));
    dashboardCard->setFixedSize(404, 246);
    dashboardCard->anchors()->top = {tileCard, Edge::Bottom, 24};
    dashboardCard->anchors()->left = {tileCard, Edge::Left, 0};
    dashboardCard->anchors()->right = {window, Edge::Right, -32};

    auto* dashboard = makeDashboardSkeleton(dashboardCard);
    dashboard->anchors()->top = {dashboardCard, Edge::Top, 78};
    dashboard->anchors()->left = {dashboardCard, Edge::Left, 22};
    qobject_cast<fluent::AnchorLayout*>(dashboardCard->layout())->addWidget(dashboard);

    auto* staticHint = makeLabel(QStringLiteral("Static preview"), dashboardCard, QStringLiteral("Caption"));
    staticHint->anchors()->bottom = {dashboardCard, Edge::Bottom, -18};
    staticHint->anchors()->right = {dashboardCard, Edge::Right, -22};
    qobject_cast<fluent::AnchorLayout*>(dashboardCard->layout())->addWidget(staticHint);

    dashboard->setAnimationEnabled(false);
    dashboard->setShimmerProgress(0.42);

    window->show();
    qApp->exec();
}
