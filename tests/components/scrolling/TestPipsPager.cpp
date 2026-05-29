#include <gtest/gtest.h>

#include <QApplication>
#include <QFrame>
#include <QFont>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPalette>
#include <QPointingDevice>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>

#include "components/foundation/FluentElement.h"
#include "components/collections/FlipView.h"
#include "components/scrolling/PipsPager.h"

using fluent::collections::FlipView;
using fluent::scrolling::PipsPager;

namespace {

class PipsPagerTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override {
        const auto& colors = themeColors();
        QPalette pal = palette();
        pal.setColor(QPalette::Window, colors.bgCanvas);
        setPalette(pal);
        setAutoFillBackground(true);
    }
};

class PipsPagerFlipPage : public QWidget {
public:
    PipsPagerFlipPage(const QColor& accent, const QColor& fill, const QString& title, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_accent(accent)
        , m_fill(fill)
        , m_title(title)
    {
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QLinearGradient gradient(rect().topLeft(), rect().bottomRight());
        gradient.setColorAt(0.0, m_fill.lighter(118));
        gradient.setColorAt(1.0, m_accent.darker(118));
        painter.fillRect(rect(), gradient);

        QRectF band(0, height() - 44, width(), 44);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 112));
        painter.drawRect(band);

        painter.setPen(Qt::white);
        QFont labelFont(QStringLiteral("Segoe UI Variable"));
        labelFont.setPixelSize(15);
        labelFont.setWeight(QFont::DemiBold);
        painter.setFont(labelFont);
        painter.drawText(band.adjusted(16, 0, -16, 0), Qt::AlignVCenter | Qt::AlignLeft, m_title);
    }

private:
    QColor m_accent;
    QColor m_fill;
    QString m_title;
};

void showAndProcess(QWidget& widget)
{
    widget.show();
    QApplication::processEvents();
}

// In-process input simulation — avoids CGEventPost on macOS and the
// resulting Accessibility permission dialog that QTest::mouse*/keyClick cause.
void simulateMouseMove(QWidget* w, QPoint pt)
{
    QMouseEvent ev(QEvent::MouseMove, QPointF(pt), QPointF(w->mapToGlobal(pt)),
                   Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(w, &ev);
}

void simulateMousePress(QWidget* w, Qt::MouseButton btn, Qt::KeyboardModifiers mod, QPoint pt)
{
    QMouseEvent ev(QEvent::MouseButtonPress, QPointF(pt), QPointF(w->mapToGlobal(pt)),
                   btn, btn, mod);
    QApplication::sendEvent(w, &ev);
}

void simulateMouseRelease(QWidget* w, Qt::MouseButton btn, Qt::KeyboardModifiers mod, QPoint pt)
{
    QMouseEvent ev(QEvent::MouseButtonRelease, QPointF(pt), QPointF(w->mapToGlobal(pt)),
                   btn, Qt::NoButton, mod);
    QApplication::sendEvent(w, &ev);
}

void simulateMouseClick(QWidget* w, Qt::MouseButton btn, Qt::KeyboardModifiers mod, QPoint pt)
{
    simulateMousePress(w, btn, mod, pt);
    simulateMouseRelease(w, btn, mod, pt);
}

void simulateKeyClick(QWidget* w, Qt::Key key, Qt::KeyboardModifiers mod = Qt::NoModifier)
{
    QKeyEvent press(QEvent::KeyPress, key, mod);
    QApplication::sendEvent(w, &press);
    QKeyEvent release(QEvent::KeyRelease, key, mod);
    QApplication::sendEvent(w, &release);
}

QImage renderPager(PipsPager& pager)
{
    if (pager.size().isEmpty()) {
        pager.resize(pager.sizeHint());
    }

    QImage image(pager.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    pager.render(&image);
    return image;
}

int paintedPixelCount(const QImage& image)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 0) {
                ++count;
            }
        }
    }
    return count;
}

QRect paintedBounds(const QImage& image, QRect clip)
{
    clip = clip.intersected(image.rect());
    QRect bounds;
    bool found = false;

    for (int y = clip.top(); y <= clip.bottom(); ++y) {
        for (int x = clip.left(); x <= clip.right(); ++x) {
            if (qAlpha(image.pixel(x, y)) <= 0) continue;
            if (!found) {
                bounds = QRect(QPoint(x, y), QSize(1, 1));
                found = true;
            } else {
                bounds = bounds.united(QRect(QPoint(x, y), QSize(1, 1)));
            }
        }
    }

    return bounds;
}

} // namespace

class PipsPagerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<fluent::scrolling::PipsPager::PipsPagerButtonVisibility>(
            "fluent::scrolling::PipsPager::PipsPagerButtonVisibility");
        qRegisterMetaType<Qt::Orientation>("Qt::Orientation");
    }

    void SetUp() override {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    void TearDown() override {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }
};

TEST_F(PipsPagerTest, DefaultPropertyValues) {
    PipsPager pager;

    EXPECT_EQ(pager.numberOfPages(), 5);
    EXPECT_EQ(pager.selectedPageIndex(), 0);
    EXPECT_EQ(pager.maxVisiblePips(), 5);
    EXPECT_EQ(pager.orientation(), Qt::Horizontal);
    EXPECT_EQ(pager.previousButtonVisibility(), PipsPager::PipsPagerButtonVisibility::Collapsed);
    EXPECT_EQ(pager.nextButtonVisibility(), PipsPager::PipsPagerButtonVisibility::Collapsed);
    EXPECT_EQ(pager.pipCellSize(), 12);
    EXPECT_EQ(pager.inactivePipDiameter(), 4);
    EXPECT_EQ(pager.selectedPipDiameter(), 6);
    EXPECT_EQ(pager.navigationButtonSize(), 24);
    EXPECT_EQ(pager.navigationIconSize(), 8);
    EXPECT_TRUE(pager.selectionAnimationEnabled());
    EXPECT_EQ(pager.selectionAnimationDuration(), 250);
    EXPECT_EQ(pager.visiblePipCount(), 5);
    EXPECT_EQ(pager.firstVisiblePage(), 0);
    EXPECT_FALSE(pager.hasPreviousPage());
    EXPECT_TRUE(pager.hasNextPage());
    EXPECT_EQ(pager.sizeHint(), QSize(60, 12));
    EXPECT_EQ(pager.minimumSizeHint(), QSize(60, 12));
    EXPECT_EQ(pager.accessibleName(), QStringLiteral("PipsPager"));
    EXPECT_EQ(pager.accessibleDescription(), QStringLiteral("Page 1 of 5 selected"));
}

TEST_F(PipsPagerTest, PropertySignalsSkipDuplicateValues) {
    PipsPager pager;

    QSignalSpy selectedSpy(&pager, &PipsPager::selectedPageIndexChanged);
    QSignalSpy selectedIndexSpy(&pager, &PipsPager::selectedIndexChanged);
    pager.setSelectedPageIndex(2);
    EXPECT_EQ(selectedSpy.count(), 1);
    EXPECT_EQ(selectedIndexSpy.count(), 1);
    EXPECT_EQ(selectedIndexSpy.at(0).at(0).toInt(), 0);
    EXPECT_EQ(selectedIndexSpy.at(0).at(1).toInt(), 2);

    pager.setSelectedPageIndex(2);
    EXPECT_EQ(selectedSpy.count(), 1);
    EXPECT_EQ(selectedIndexSpy.count(), 1);

    QSignalSpy pageCountSpy(&pager, &PipsPager::numberOfPagesChanged);
    pager.setNumberOfPages(2);
    EXPECT_EQ(pageCountSpy.count(), 1);
    EXPECT_EQ(selectedSpy.count(), 2);
    EXPECT_EQ(selectedIndexSpy.count(), 2);
    EXPECT_EQ(selectedIndexSpy.at(1).at(0).toInt(), 2);
    EXPECT_EQ(selectedIndexSpy.at(1).at(1).toInt(), 1);

    pager.setNumberOfPages(2);
    EXPECT_EQ(pageCountSpy.count(), 1);

    QSignalSpy maxVisibleSpy(&pager, &PipsPager::maxVisiblePipsChanged);
    pager.setMaxVisiblePips(4);
    pager.setMaxVisiblePips(4);
    EXPECT_EQ(maxVisibleSpy.count(), 1);

    QSignalSpy orientationSpy(&pager, &PipsPager::orientationChanged);
    pager.setOrientation(Qt::Vertical);
    pager.setOrientation(Qt::Vertical);
    EXPECT_EQ(orientationSpy.count(), 1);

    QSignalSpy previousSpy(&pager, &PipsPager::previousButtonVisibilityChanged);
    QSignalSpy nextSpy(&pager, &PipsPager::nextButtonVisibilityChanged);
    pager.setPreviousButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
    pager.setPreviousButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
    pager.setNextButtonVisibility(PipsPager::PipsPagerButtonVisibility::VisibleOnPointerOver);
    pager.setNextButtonVisibility(PipsPager::PipsPagerButtonVisibility::VisibleOnPointerOver);
    EXPECT_EQ(previousSpy.count(), 1);
    EXPECT_EQ(nextSpy.count(), 1);

    QSignalSpy cellSizeSpy(&pager, &PipsPager::pipCellSizeChanged);
    QSignalSpy inactiveDiameterSpy(&pager, &PipsPager::inactivePipDiameterChanged);
    QSignalSpy selectedDiameterSpy(&pager, &PipsPager::selectedPipDiameterChanged);
    QSignalSpy buttonSizeSpy(&pager, &PipsPager::navigationButtonSizeChanged);
    QSignalSpy iconSizeSpy(&pager, &PipsPager::navigationIconSizeChanged);
    QSignalSpy animationEnabledSpy(&pager, &PipsPager::selectionAnimationEnabledChanged);
    QSignalSpy animationDurationSpy(&pager, &PipsPager::selectionAnimationDurationChanged);
    pager.setPipCellSize(16);
    pager.setPipCellSize(16);
    pager.setInactivePipDiameter(5);
    pager.setInactivePipDiameter(5);
    pager.setSelectedPipDiameter(8);
    pager.setSelectedPipDiameter(8);
    pager.setNavigationButtonSize(28);
    pager.setNavigationButtonSize(28);
    pager.setNavigationIconSize(10);
    pager.setNavigationIconSize(10);
    pager.setSelectionAnimationEnabled(false);
    pager.setSelectionAnimationEnabled(false);
    pager.setSelectionAnimationDuration(90);
    pager.setSelectionAnimationDuration(90);
    EXPECT_EQ(cellSizeSpy.count(), 1);
    EXPECT_EQ(inactiveDiameterSpy.count(), 1);
    EXPECT_EQ(selectedDiameterSpy.count(), 1);
    EXPECT_EQ(buttonSizeSpy.count(), 1);
    EXPECT_EQ(iconSizeSpy.count(), 1);
    EXPECT_EQ(animationEnabledSpy.count(), 1);
    EXPECT_EQ(animationDurationSpy.count(), 1);
}

TEST_F(PipsPagerTest, PageCountClampsSelectionAndZeroPagesDisableNavigation) {
    PipsPager pager;
    pager.setSelectedPageIndex(4);
    EXPECT_EQ(pager.selectedPageIndex(), 4);

    pager.setNumberOfPages(3);
    EXPECT_EQ(pager.numberOfPages(), 3);
    EXPECT_EQ(pager.selectedPageIndex(), 2);
    EXPECT_EQ(pager.accessibleDescription(), QStringLiteral("Page 3 of 3 selected"));

    pager.setNumberOfPages(0);
    EXPECT_EQ(pager.numberOfPages(), 0);
    EXPECT_EQ(pager.selectedPageIndex(), 0);
    EXPECT_EQ(pager.visiblePipCount(), 0);
    EXPECT_FALSE(pager.hasPreviousPage());
    EXPECT_FALSE(pager.hasNextPage());
    EXPECT_TRUE(pager.pipHitRect(0).isNull());
    EXPECT_EQ(pager.accessibleDescription(), QStringLiteral("No pages selected"));

    pager.setNumberOfPages(-12);
    EXPECT_EQ(pager.numberOfPages(), 0);
}

TEST_F(PipsPagerTest, VisiblePipWindowCentersSelectedPageWhenPossible) {
    PipsPager pager;
    pager.setNumberOfPages(10);
    pager.setMaxVisiblePips(5);

    pager.setSelectedPageIndex(0);
    EXPECT_EQ(pager.visiblePipCount(), 5);
    EXPECT_EQ(pager.firstVisiblePage(), 0);

    pager.setSelectedPageIndex(4);
    EXPECT_EQ(pager.firstVisiblePage(), 2);

    pager.setSelectedPageIndex(9);
    EXPECT_EQ(pager.firstVisiblePage(), 5);

    pager.setMaxVisiblePips(0);
    EXPECT_EQ(pager.visiblePipCount(), 0);
    EXPECT_EQ(pager.firstVisiblePage(), 0);
    EXPECT_TRUE(pager.pipHitRect(9).isNull());
}

TEST_F(PipsPagerTest, SizeHintAndHitRectsFollowOrientationAndButtonSlots) {
    PipsPager pager;
    pager.setNumberOfPages(5);
    pager.setSelectedPageIndex(2);
    pager.setPreviousButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
    pager.setNextButtonVisibility(PipsPager::PipsPagerButtonVisibility::VisibleOnPointerOver);
    pager.setFixedSize(pager.sizeHint());

    EXPECT_EQ(pager.sizeHint(), QSize(108, 24));
    EXPECT_EQ(pager.previousButtonRect(), QRect(0, 0, 24, 24));
    EXPECT_EQ(pager.nextButtonRect(), QRect(84, 0, 24, 24));
    EXPECT_EQ(pager.pipHitRect(0), QRect(24, 6, 12, 12));
    EXPECT_EQ(pager.pipHitRect(4), QRect(72, 6, 12, 12));

    pager.setOrientation(Qt::Vertical);
    pager.setFixedSize(pager.sizeHint());
    EXPECT_EQ(pager.sizeHint(), QSize(24, 108));
    EXPECT_EQ(pager.previousButtonRect(), QRect(0, 0, 24, 24));
    EXPECT_EQ(pager.nextButtonRect(), QRect(0, 84, 24, 24));
    EXPECT_EQ(pager.pipHitRect(0), QRect(6, 24, 12, 12));
    EXPECT_EQ(pager.pipHitRect(4), QRect(6, 72, 12, 12));
}

TEST_F(PipsPagerTest, ConfigurableMetricsUpdateLayoutAndRendering) {
    PipsPager pager;
    pager.setNumberOfPages(5);
    pager.setPipCellSize(16);
    pager.setInactivePipDiameter(6);
    pager.setSelectedPipDiameter(10);
    pager.setNavigationButtonSize(28);
    pager.setNavigationIconSize(12);
    pager.setPreviousButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
    pager.setNextButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
    pager.setFixedSize(pager.sizeHint());
    showAndProcess(pager);

    EXPECT_EQ(pager.sizeHint(), QSize(136, 28));
    EXPECT_EQ(pager.previousButtonRect(), QRect(0, 0, 28, 28));
    EXPECT_EQ(pager.nextButtonRect(), QRect(108, 0, 28, 28));
    EXPECT_EQ(pager.pipHitRect(0), QRect(28, 6, 16, 16));
    EXPECT_EQ(pager.pipHitRect(4), QRect(92, 6, 16, 16));

    const QImage image = renderPager(pager);
    const QRect selectedBounds = paintedBounds(image, pager.pipHitRect(0));
    const QRect inactiveBounds = paintedBounds(image, pager.pipHitRect(1));
    EXPECT_GE(selectedBounds.width(), 9);
    EXPECT_GE(inactiveBounds.width(), 5);
}

TEST_F(PipsPagerTest, PointerCanSelectPipsAndNavigateButtonsWithoutWrapping) {
    PipsPager pager;
    pager.setNumberOfPages(5);
    pager.setFixedSize(pager.sizeHint());
    showAndProcess(pager);

    QSignalSpy selectedSpy(&pager, &PipsPager::selectedPageIndexChanged);
    simulateMouseClick(&pager, Qt::LeftButton, Qt::NoModifier, pager.pipHitRect(3).center());
    EXPECT_EQ(pager.selectedPageIndex(), 3);
    EXPECT_EQ(selectedSpy.count(), 1);

    pager.setPreviousButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
    pager.setNextButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
    pager.setFixedSize(pager.sizeHint());
    QApplication::processEvents();

    simulateMouseClick(&pager, Qt::LeftButton, Qt::NoModifier, pager.previousButtonRect().center());
    EXPECT_EQ(pager.selectedPageIndex(), 2);

    simulateMouseClick(&pager, Qt::LeftButton, Qt::NoModifier, pager.nextButtonRect().center());
    EXPECT_EQ(pager.selectedPageIndex(), 3);

    pager.setSelectedPageIndex(0);
    simulateMouseClick(&pager, Qt::LeftButton, Qt::NoModifier, pager.previousButtonRect().center());
    EXPECT_EQ(pager.selectedPageIndex(), 0);

    pager.setSelectedPageIndex(4);
    simulateMouseClick(&pager, Qt::LeftButton, Qt::NoModifier, pager.nextButtonRect().center());
    EXPECT_EQ(pager.selectedPageIndex(), 4);
}

TEST_F(PipsPagerTest, KeyboardNavigationMatchesOrientationAndDoesNotWrap) {
    PipsPager pager;
    pager.setNumberOfPages(5);
    pager.setSelectedPageIndex(2);
    pager.setFixedSize(pager.sizeHint());
    showAndProcess(pager);
    pager.setFocus();

    simulateKeyClick(&pager, Qt::Key_Left);
    EXPECT_EQ(pager.selectedPageIndex(), 1);
    simulateKeyClick(&pager, Qt::Key_Right);
    EXPECT_EQ(pager.selectedPageIndex(), 2);
    simulateKeyClick(&pager, Qt::Key_Home);
    EXPECT_EQ(pager.selectedPageIndex(), 0);
    simulateKeyClick(&pager, Qt::Key_Left);
    EXPECT_EQ(pager.selectedPageIndex(), 0);
    simulateKeyClick(&pager, Qt::Key_End);
    EXPECT_EQ(pager.selectedPageIndex(), 4);
    simulateKeyClick(&pager, Qt::Key_Right);
    EXPECT_EQ(pager.selectedPageIndex(), 4);

    pager.setOrientation(Qt::Vertical);
    pager.setFixedSize(pager.sizeHint());
    simulateKeyClick(&pager, Qt::Key_Up);
    EXPECT_EQ(pager.selectedPageIndex(), 3);
    simulateKeyClick(&pager, Qt::Key_Down);
    EXPECT_EQ(pager.selectedPageIndex(), 4);
}

TEST_F(PipsPagerTest, DisabledStateBlocksPointerAndKeyboardInput) {
    PipsPager pager;
    pager.setNumberOfPages(5);
    pager.setFixedSize(pager.sizeHint());
    showAndProcess(pager);
    pager.setFocus();
    pager.setEnabled(false);

    simulateMouseClick(&pager, Qt::LeftButton, Qt::NoModifier, pager.pipHitRect(3).center());
    simulateKeyClick(&pager, Qt::Key_End);
    EXPECT_EQ(pager.selectedPageIndex(), 0);

    const QImage image = renderPager(pager);
    EXPECT_GT(paintedPixelCount(image), 0);
}

TEST_F(PipsPagerTest, RenderingUsesExpectedPipStateSizesAndThemeColors) {
    PipsPager pager;
    pager.setNumberOfPages(5);
    pager.setFixedSize(pager.sizeHint());
    showAndProcess(pager);

    const QRect selectedCell = pager.pipHitRect(0);
    const QImage restImage = renderPager(pager);
    const QRect restBounds = paintedBounds(restImage, selectedCell);
    EXPECT_GE(restBounds.width(), 5);
    EXPECT_LE(restBounds.width(), 8);

    simulateMouseMove(&pager, selectedCell.center());
    QApplication::processEvents();
    const QImage hoverImage = renderPager(pager);
    const QRect hoverBounds = paintedBounds(hoverImage, selectedCell);
    EXPECT_GE(hoverBounds.width(), restBounds.width());

    simulateMousePress(&pager, Qt::LeftButton, Qt::NoModifier, selectedCell.center());
    QApplication::processEvents();
    const QImage pressedImage = renderPager(pager);
    const QRect pressedBounds = paintedBounds(pressedImage, selectedCell);
    EXPECT_LE(pressedBounds.width(), hoverBounds.width());
    simulateMouseRelease(&pager, Qt::LeftButton, Qt::NoModifier, selectedCell.center());

    const QColor lightCenter = QColor(restImage.pixelColor(selectedCell.center()));
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    QApplication::processEvents();
    const QImage darkImage = renderPager(pager);
    const QColor darkCenter = QColor(darkImage.pixelColor(selectedCell.center()));
    EXPECT_NE(lightCenter.rgb(), darkCenter.rgb());
}

TEST_F(PipsPagerTest, SelectionAnimationMovesActivePipBetweenVisibleCells) {
    PipsPager pager;
    pager.setNumberOfPages(5);
    pager.setSelectionAnimationDuration(80);
    pager.setFixedSize(pager.sizeHint());
    showAndProcess(pager);

    const QRect firstCell = pager.pipHitRect(0);
    const QRect secondCell = pager.pipHitRect(1);
    pager.setSelectedPageIndex(1);

    const QImage startImage = renderPager(pager);
    const QRect firstStartBounds = paintedBounds(startImage, firstCell);
    const QRect secondStartBounds = paintedBounds(startImage, secondCell);
    EXPECT_GE(firstStartBounds.width(), 5);
    EXPECT_LE(secondStartBounds.width(), 5);

    QTest::qWait(pager.selectionAnimationDuration() + 50);
    QApplication::processEvents();
    const QImage endImage = renderPager(pager);
    const QRect firstEndBounds = paintedBounds(endImage, firstCell);
    const QRect secondEndBounds = paintedBounds(endImage, secondCell);
    EXPECT_LE(firstEndBounds.width(), 5);
    EXPECT_GE(secondEndBounds.width(), 5);
}

TEST_F(PipsPagerTest, SelectionAnimationShowsMotionWhenVisibleWindowRecenters) {
    PipsPager pager;
    pager.setNumberOfPages(7);
    pager.setMaxVisiblePips(5);
    pager.setSelectionAnimationEnabled(false);
    pager.setSelectedPageIndex(2);
    pager.setSelectionAnimationEnabled(true);
    pager.setSelectionAnimationDuration(400);
    pager.setFixedSize(pager.sizeHint());
    showAndProcess(pager);

    pager.setSelectedPageIndex(3);
    EXPECT_EQ(pager.firstVisiblePage(), 1);

    QTest::qWait(pager.selectionAnimationDuration() / 5);
    QApplication::processEvents();
    const QImage midImage = renderPager(pager);
    const QRect centeredCell = pager.pipHitRect(3);
    EXPECT_GE(paintedBounds(midImage, centeredCell).width(), 5);

    QTest::qWait(pager.selectionAnimationDuration());
    QApplication::processEvents();
    const QImage endImage = renderPager(pager);
    EXPECT_GE(paintedBounds(endImage, centeredCell).width(), 5);
    EXPECT_GT(paintedPixelCount(midImage), paintedPixelCount(endImage));
}

TEST_F(PipsPagerTest, SelectionAnimationCanBeDisabled) {
    PipsPager pager;
    pager.setNumberOfPages(5);
    pager.setSelectionAnimationEnabled(false);
    pager.setFixedSize(pager.sizeHint());
    showAndProcess(pager);

    const QRect firstCell = pager.pipHitRect(0);
    const QRect secondCell = pager.pipHitRect(1);
    pager.setSelectedPageIndex(1);

    const QImage image = renderPager(pager);
    EXPECT_LE(paintedBounds(image, firstCell).width(), 5);
    EXPECT_GE(paintedBounds(image, secondCell).width(), 5);
}

TEST_F(PipsPagerTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        GTEST_SKIP() << "Skipping visual test in offscreen mode";
    }

    PipsPagerTestWindow window;
    window.setWindowTitle("PipsPager VisualCheck");
    window.resize(760, 760);
    window.onThemeUpdated();

    auto* root = new QVBoxLayout(&window);
    root->setContentsMargins(28, 24, 28, 28);
    root->setSpacing(18);

    auto* header = new QHBoxLayout();
    auto* title = new QLabel("PipsPager", &window);
    QFont titleFont = title->font();
    titleFont.setPointSize(22);
    titleFont.setBold(true);
    title->setFont(titleFont);
    auto* themeButton = new QPushButton("Toggle Light/Dark", &window);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(themeButton);
    root->addLayout(header);

    QObject::connect(themeButton, &QPushButton::clicked, [&window]() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
            ? fluent::FluentElement::Dark
            : fluent::FluentElement::Light);
        window.onThemeUpdated();
    });

    auto* flipFrame = new QFrame(&window);
    flipFrame->setFrameShape(QFrame::StyledPanel);
    auto* flipLayout = new QVBoxLayout(flipFrame);
    flipLayout->setContentsMargins(20, 16, 20, 18);
    flipLayout->setSpacing(10);

    auto* flipTitle = new QLabel("PipsPager integrated with a FlipView", flipFrame);
    QFont flipTitleFont = flipTitle->font();
    flipTitleFont.setPointSize(14);
    flipTitleFont.setBold(true);
    flipTitle->setFont(flipTitleFont);
    flipLayout->addWidget(flipTitle);

    auto* flipView = new FlipView(flipFrame);
    flipView->setFixedSize(520, 292);
    flipView->setShowPageIndicator(false);
    flipView->addPage(new PipsPagerFlipPage(QColor("#375f90"), QColor("#b9d7ea"), "Coastal route", flipView));
    flipView->addPage(new PipsPagerFlipPage(QColor("#3f7b52"), QColor("#c8e3b4"), "Forest trail", flipView));
    flipView->addPage(new PipsPagerFlipPage(QColor("#9a6339"), QColor("#f0cf9a"), "Desert light", flipView));
    flipView->addPage(new PipsPagerFlipPage(QColor("#7b4f91"), QColor("#d9c2ea"), "Evening ridge", flipView));
    flipView->addPage(new PipsPagerFlipPage(QColor("#4b6d73"), QColor("#c7e6e4"), "Harbor morning", flipView));
    flipView->addPage(new PipsPagerFlipPage(QColor("#8c4f4f"), QColor("#e9c7c7"), "Mountain dusk", flipView)); 
    flipView->addPage(new PipsPagerFlipPage(QColor("#5a5a5a"), QColor("#dcdcdc"), "City skyline", flipView));   
    flipLayout->addWidget(flipView, 0, Qt::AlignHCenter);

    auto* flipPager = new PipsPager(flipFrame);
    flipPager->setNumberOfPages(flipView->pageCount());
    flipPager->setMaxVisiblePips(5);
    flipPager->setSelectionAnimationDuration(220);
    flipLayout->addWidget(flipPager, 0, Qt::AlignHCenter);

    QObject::connect(flipPager, &PipsPager::selectedPageIndexChanged,
                     flipView, &FlipView::setCurrentIndex);
    QObject::connect(flipView, &FlipView::currentIndexChanged,
                     flipPager, &PipsPager::setSelectedPageIndex);

    root->addWidget(flipFrame);

    auto addRow = [&window, root](const QString& labelText, PipsPager* pager) {
        auto* frame = new QFrame(&window);
        frame->setFrameShape(QFrame::StyledPanel);
        auto* row = new QHBoxLayout(frame);
        row->setContentsMargins(20, 14, 20, 14);
        row->setSpacing(24);
        auto* label = new QLabel(labelText, frame);
        label->setMinimumWidth(180);
        row->addWidget(label);
        row->addWidget(pager, 0, Qt::AlignCenter);
        row->addStretch();
        root->addWidget(frame);
    };

    auto* defaultPager = new PipsPager(&window);
    defaultPager->setNumberOfPages(7);
    defaultPager->setSelectedPageIndex(2);
    addRow("Horizontal", defaultPager);

    auto* buttonPager = new PipsPager(&window);
    buttonPager->setNumberOfPages(10);
    buttonPager->setSelectedPageIndex(5);
    buttonPager->setPreviousButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
    buttonPager->setNextButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
    addRow("With carets", buttonPager);

    auto* hoverPager = new PipsPager(&window);
    hoverPager->setNumberOfPages(8);
    hoverPager->setSelectedPageIndex(4);
    hoverPager->setPreviousButtonVisibility(PipsPager::PipsPagerButtonVisibility::VisibleOnPointerOver);
    hoverPager->setNextButtonVisibility(PipsPager::PipsPagerButtonVisibility::VisibleOnPointerOver);
    addRow("Carets on pointer", hoverPager);

    auto* verticalPager = new PipsPager(&window);
    verticalPager->setNumberOfPages(7);
    verticalPager->setSelectedPageIndex(3);
    verticalPager->setOrientation(Qt::Vertical);
    verticalPager->setPreviousButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
    verticalPager->setNextButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
    addRow("Vertical", verticalPager);

    auto* disabledPager = new PipsPager(&window);
    disabledPager->setNumberOfPages(6);
    disabledPager->setSelectedPageIndex(2);
    disabledPager->setPreviousButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
    disabledPager->setNextButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
    disabledPager->setEnabled(false);
    addRow("Disabled", disabledPager);

    auto* bindingFrame = new QFrame(&window);
    bindingFrame->setFrameShape(QFrame::StyledPanel);
    auto* bindingRow = new QHBoxLayout(bindingFrame);
    bindingRow->setContentsMargins(20, 14, 20, 14);
    bindingRow->setSpacing(24);
    auto* bindingLabel = new QLabel("Bound content", bindingFrame);
    bindingLabel->setMinimumWidth(180);
    auto* bindingPager = new PipsPager(bindingFrame);
    bindingPager->setNumberOfPages(9);
    bindingPager->setSelectedPageIndex(4);
    bindingPager->setPreviousButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
    bindingPager->setNextButtonVisibility(PipsPager::PipsPagerButtonVisibility::Visible);
    auto* contentLabel = new QLabel(bindingFrame);
    auto updateContent = [bindingPager, contentLabel]() {
        contentLabel->setText(QStringLiteral("Page %1 content")
            .arg(bindingPager->selectedPageIndex() + 1));
    };
    QObject::connect(bindingPager, &PipsPager::selectedPageIndexChanged, contentLabel, updateContent);
    updateContent();
    bindingRow->addWidget(bindingLabel);
    bindingRow->addWidget(bindingPager, 0, Qt::AlignCenter);
    bindingRow->addWidget(contentLabel);
    bindingRow->addStretch();
    root->addWidget(bindingFrame);

    window.show();
    qApp->exec();
}
