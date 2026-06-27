#include <gtest/gtest.h>

#include <algorithm>
#include <QApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QScrollBar>
#include <QKeyEvent>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "compatibility/QtCompat.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/basicinput/Button.h"
#include "components/basicinput/Slider.h"
#include "components/scrolling/AnnotatedScrollBar.h"
#include "components/scrolling/ScrollView.h"
#include "components/textfields/Label.h"

using fluent::AnchorLayout;
using fluent::basicinput::Button;
using fluent::basicinput::Slider;
using fluent::scrolling::AnnotatedScrollBar;
using fluent::scrolling::AnnotatedScrollBarLabel;
using fluent::scrolling::ScrollView;
using fluent::textfields::Label;

namespace {

void showAndProcess(QWidget& widget)
{
    widget.show();
    QApplication::processEvents();
}

void sendWheel(QWidget* target, int x, int y, int angleDeltaY)
{
    FLUENT_MAKE_WHEEL_EVENT(event, x, y, angleDeltaY, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
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

QWidget* createContent(const QSize& size)
{
    auto* content = new QWidget();
    content->setFixedSize(size);
    QPalette palette = content->palette();
    palette.setColor(QPalette::Window, QColor(238, 241, 245));
    content->setPalette(palette);
    content->setAutoFillBackground(true);
    return content;
}

class ColorSectionsContent : public QWidget, public fluent::FluentElement {
public:
    explicit ColorSectionsContent(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(600, 1200);
    }

    void onThemeUpdated() override
    {
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), themeColors().bgLayer);

        const QList<QColor> colors = {
            QColor("#0078D4"),
            QColor("#C239B3"),
            QColor("#00B7C3"),
            QColor("#FFB900"),
            QColor("#107C10")
        };
        for (int section = 0; section < colors.size(); ++section) {
            QColor fill = colors.at(section);
            fill.setAlphaF(currentTheme() == fluent::FluentElement::Dark ? 0.68 : 0.84);
            painter.setBrush(fill);
            painter.setPen(Qt::NoPen);
            const int y = 24 + section * 220;
            for (int index = 0; index < 12; ++index) {
                const int col = index % 4;
                const int row = index / 4;
                painter.drawRoundedRect(QRect(32 + col * 140, y + row * 58, 104, 42), 6, 6);
            }
        }
    }
};

class AnnotatedScrollBarVisualWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override
    {
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, themeColors().bgCanvas);
        setPalette(palette);
        setAutoFillBackground(true);
    }
};

class VisualPanel : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override
    {
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const auto colors = themeColors();
        const QRectF surface = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(colors.strokeCard, 1.0));
        painter.setBrush(colors.bgLayer);
        painter.drawRoundedRect(surface, themeRadius().overlay, themeRadius().overlay);
    }
};

QVector<AnnotatedScrollBarLabel> yearLabels()
{
    QVector<AnnotatedScrollBarLabel> labels;
    for (int year = 2023; year >= 2015; --year) {
        const int offset = (2023 - year) * 120;
        labels.append(AnnotatedScrollBarLabel(QString::number(year), offset,
                                              QStringLiteral("October %1").arg(year)));
    }
    return labels;
}

} // namespace

class AnnotatedScrollBarTest : public ::testing::Test {
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

TEST_F(AnnotatedScrollBarTest, DefaultsExposeExpectedApi)
{
    AnnotatedScrollBar bar;

    EXPECT_EQ(bar.minimum(), 0);
    EXPECT_EQ(bar.maximum(), 0);
    EXPECT_EQ(bar.value(), 0);
    EXPECT_EQ(bar.pageStep(), 0);
    EXPECT_TRUE(bar.labels().isEmpty());
    EXPECT_FALSE(bar.hasDetailLabelProvider());
    EXPECT_FALSE(bar.sizeHint().isEmpty());
    EXPECT_GE(bar.sizeHint().width(), 90);
    EXPECT_GE(bar.sizeHint().height(), 500);
    EXPECT_NE(dynamic_cast<fluent::FluentElement*>(&bar), nullptr);
    EXPECT_NE(dynamic_cast<fluent::QMLPlus*>(&bar), nullptr);
}

TEST_F(AnnotatedScrollBarTest, ConfigurableLayoutMetricsAffectHintsAndLabelDensity)
{
    AnnotatedScrollBar bar;
    QSignalSpy metricsSpy(&bar, &AnnotatedScrollBar::layoutMetricsChanged);

    bar.setPreferredSize(QSize(144, 420));
    bar.setMinimumBarSize(QSize(72, 160));
    bar.setVerticalPadding(24);
    bar.setCaretSize(QSize(18, 20));
    bar.setLabelColumnWidth(64);
    bar.setLabelLineHeight(24);
    bar.setMinimumLabelSpacing(72);
    bar.setIndicatorWidth(36);
    bar.setIndicatorThickness(5);
    bar.setLineStepFallback(7);

    EXPECT_EQ(bar.sizeHint(), QSize(144, 420));
    EXPECT_EQ(bar.minimumSizeHint(), QSize(72, 160));
    EXPECT_EQ(bar.verticalPadding(), 24);
    EXPECT_EQ(bar.caretSize(), QSize(18, 20));
    EXPECT_EQ(bar.labelColumnWidth(), 64);
    EXPECT_EQ(bar.labelLineHeight(), 24);
    EXPECT_EQ(bar.minimumLabelSpacing(), 72);
    EXPECT_EQ(bar.indicatorWidth(), 36);
    EXPECT_EQ(bar.indicatorThickness(), 5);
    EXPECT_EQ(bar.lineStepFallback(), 7);
    EXPECT_EQ(metricsSpy.count(), 10);

    bar.resize(144, 180);
    bar.setRange(0, 400);
    bar.setLabels({AnnotatedScrollBarLabel(QStringLiteral("A"), 0),
                   AnnotatedScrollBarLabel(QStringLiteral("B"), 100),
                   AnnotatedScrollBarLabel(QStringLiteral("C"), 200),
                   AnnotatedScrollBarLabel(QStringLiteral("D"), 300),
                   AnnotatedScrollBarLabel(QStringLiteral("E"), 400)});

    const int sparseCount = bar.visibleLabelCount();
    bar.setMinimumLabelSpacing(10);
    EXPECT_GT(bar.visibleLabelCount(), sparseCount);
}

TEST_F(AnnotatedScrollBarTest, RangeValueAndPageStepUseSetterGuards)
{
    AnnotatedScrollBar bar;
    QSignalSpy rangeSpy(&bar, &AnnotatedScrollBar::rangeChanged);
    QSignalSpy valueSpy(&bar, &AnnotatedScrollBar::valueChanged);
    QSignalSpy pageStepSpy(&bar, &AnnotatedScrollBar::pageStepChanged);

    bar.setRange(0, 100);
    EXPECT_EQ(rangeSpy.count(), 1);
    bar.setRange(0, 100);
    EXPECT_EQ(rangeSpy.count(), 1);

    bar.setValue(120);
    EXPECT_EQ(bar.value(), 100);
    EXPECT_EQ(valueSpy.count(), 1);
    bar.setValue(100);
    EXPECT_EQ(valueSpy.count(), 1);

    bar.setPageStep(40);
    EXPECT_EQ(bar.pageStep(), 40);
    EXPECT_EQ(pageStepSpy.count(), 1);
    bar.setPageStep(40);
    EXPECT_EQ(pageStepSpy.count(), 1);
}

TEST_F(AnnotatedScrollBarTest, LabelsSortAndCollapseWhenHeightIsConstrained)
{
    AnnotatedScrollBar bar;
    bar.resize(97, 535);
    bar.setRange(0, 1000);
    QSignalSpy labelsSpy(&bar, &AnnotatedScrollBar::labelsChanged);

    bar.setLabels({
        AnnotatedScrollBarLabel(QStringLiteral("C"), 500),
        AnnotatedScrollBarLabel(QStringLiteral("A"), 0),
        AnnotatedScrollBarLabel(QStringLiteral("B"), 250),
        AnnotatedScrollBarLabel(QStringLiteral("D"), 1000)
    });

    ASSERT_EQ(labelsSpy.count(), 1);
    const QVector<AnnotatedScrollBarLabel> visible = bar.visibleLabels();
    ASSERT_EQ(visible.size(), 4);
    EXPECT_EQ(visible.at(0).text, QStringLiteral("A"));
    EXPECT_EQ(visible.at(1).text, QStringLiteral("B"));
    EXPECT_EQ(visible.at(2).text, QStringLiteral("C"));
    EXPECT_EQ(visible.at(3).text, QStringLiteral("D"));

    QVector<AnnotatedScrollBarLabel> denseLabels;
    for (int index = 0; index < 10; ++index)
        denseLabels.append(AnnotatedScrollBarLabel(QString::number(index), index * 100));
    bar.setLabels(denseLabels);
    bar.resize(97, 96);
    EXPECT_LT(bar.visibleLabelCount(), bar.labels().size());
}

TEST_F(AnnotatedScrollBarTest, DetailProviderAndFallbackDrivePopupState)
{
    AnnotatedScrollBar bar;
    bar.resize(120, 240);
    bar.setRange(0, 1000);
    showAndProcess(bar);

    QSignalSpy requestedSpy(&bar, &AnnotatedScrollBar::detailLabelRequested);
    bar.setDetailLabelProvider([](int offset) {
        return QStringLiteral("Offset %1").arg(offset);
    });
    simulateMouseMove(&bar, QPoint(110, 120));
    QApplication::processEvents();

    EXPECT_TRUE(bar.isDetailLabelVisible());
    EXPECT_TRUE(bar.detailLabelText().startsWith(QStringLiteral("Offset ")));
    EXPECT_GT(requestedSpy.count(), 0);

    QEvent leaveEvent(QEvent::Leave);
    QApplication::sendEvent(&bar, &leaveEvent);
    EXPECT_FALSE(bar.isDetailLabelVisible());

    bar.clearDetailLabelProvider();
    bar.setLabels({AnnotatedScrollBarLabel(QStringLiteral("Start"), 0, QStringLiteral("Start detail"))});
    simulateMouseMove(&bar, QPoint(12, 19));
    QApplication::processEvents();

    EXPECT_TRUE(bar.isDetailLabelVisible());
    EXPECT_EQ(bar.detailLabelText(), QStringLiteral("Start detail"));
}

TEST_F(AnnotatedScrollBarTest, MouseInputRequestsScrollingAndActivatesLabels)
{
    AnnotatedScrollBar bar;
    bar.resize(120, 300);
    bar.setRange(0, 1000);
    bar.setLabels({AnnotatedScrollBarLabel(QStringLiteral("Middle"), 500)});
    showAndProcess(bar);

    QSignalSpy scrollSpy(&bar, &AnnotatedScrollBar::scrollRequested);
    QSignalSpy labelSpy(&bar, &AnnotatedScrollBar::labelActivated);

    simulateMouseClick(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(110, 260));
    EXPECT_GT(bar.value(), 700);
    EXPECT_EQ(scrollSpy.count(), 1);

    simulateMouseClick(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(12, 150));
    EXPECT_EQ(labelSpy.count(), 1);
    EXPECT_EQ(bar.value(), 500);
}

TEST_F(AnnotatedScrollBarTest, DragWheelAndKeyboardUpdateValue)
{
    AnnotatedScrollBar bar;
    bar.resize(120, 300);
    bar.setRange(0, 1000);
    bar.setPageStep(100);
    showAndProcess(bar);

    QSignalSpy scrollSpy(&bar, &AnnotatedScrollBar::scrollRequested);
    simulateMousePress(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(110, 40));
    simulateMouseMove(&bar, QPoint(110, 260));
    simulateMouseRelease(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(110, 260));
    EXPECT_GT(bar.value(), 700);
    EXPECT_GT(scrollSpy.count(), 0);

    bar.setValue(500);
    sendWheel(&bar, 110, 150, 120);
    EXPECT_LT(bar.value(), 500);

    bar.setFocus();
    simulateKeyClick(&bar, Qt::Key_End);
    EXPECT_EQ(bar.value(), 1000);
    simulateKeyClick(&bar, Qt::Key_PageUp);
    EXPECT_LT(bar.value(), 1000);
    simulateKeyClick(&bar, Qt::Key_Home);
    EXPECT_EQ(bar.value(), 0);
}

TEST_F(AnnotatedScrollBarTest, DisabledStateIgnoresInputAndHidesDetail)
{
    AnnotatedScrollBar bar;
    bar.resize(120, 240);
    bar.setRange(0, 1000);
    bar.setValue(400);
    bar.setDetailLabelProvider([](int) { return QStringLiteral("Detail"); });
    showAndProcess(bar);

    simulateMouseMove(&bar, QPoint(110, 120));
    EXPECT_TRUE(bar.isDetailLabelVisible());

    bar.setEnabled(false);
    EXPECT_FALSE(bar.isDetailLabelVisible());
    sendWheel(&bar, 110, 120, -120);
    EXPECT_EQ(bar.value(), 400);
}

TEST_F(AnnotatedScrollBarTest, ConnectToScrollViewSynchronizesBothDirections)
{
    ScrollView view;
    view.resize(180, 120);
    view.setWidget(createContent(QSize(420, 360)));
    showAndProcess(view);

    AnnotatedScrollBar bar;
    bar.resize(120, 300);
    bar.connectToScrollView(&view);
    showAndProcess(bar);

    EXPECT_EQ(bar.minimum(), view.verticalScrollBar()->minimum());
    EXPECT_EQ(bar.maximum(), view.verticalScrollBar()->maximum());
    EXPECT_EQ(bar.pageStep(), view.verticalScrollBar()->pageStep());

    view.scrollTo(40, 80, false);
    QApplication::processEvents();
    EXPECT_EQ(bar.value(), view.verticalOffset());

    simulateMouseClick(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(110, 260));
    QApplication::processEvents();
    EXPECT_EQ(view.horizontalOffset(), 40);
    EXPECT_EQ(view.verticalOffset(), bar.value());
    EXPECT_GT(view.verticalOffset(), 80);

    const int previous = bar.value();
    bar.disconnectScrollView();
    view.scrollTo(40, 0, false);
    QApplication::processEvents();
    EXPECT_EQ(bar.value(), previous);
}

TEST_F(AnnotatedScrollBarTest, RenderReflectsThemeAndDisabledState)
{
    AnnotatedScrollBar bar;
    bar.resize(120, 300);
    bar.setRange(0, 1000);
    bar.setValue(500);
    bar.setLabels({AnnotatedScrollBarLabel(QStringLiteral("A"), 0),
                   AnnotatedScrollBarLabel(QStringLiteral("B"), 1000)});
    showAndProcess(bar);

    auto renderIndicator = [&bar]() {
        QImage image(bar.size(), QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        bar.render(&image);
        const QPoint center = bar.indicatorCenter().toPoint();
        return QColor::fromRgba(image.pixel(center));
    };

    const QColor lightColor = renderIndicator();
    EXPECT_GT(lightColor.alpha(), 0);

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    QApplication::processEvents();
    const QColor darkColor = renderIndicator();
    EXPECT_NE(lightColor.rgb(), darkColor.rgb());

    bar.setEnabled(false);
    const QColor disabledColor = renderIndicator();
    EXPECT_NE(darkColor.rgba(), disabledColor.rgba());
}

// ─── Design-language × theme sweep ──────────────────────────────────────────
//
// AnnotatedScrollBar's position indicator pill is colors.accentDefault — accent is the correct read
// under EVERY design language (Fluent / Material 3 / macOS), so it stays accent across the board; no
// neutral rail is painted, so there is nothing to give the M3/macOS neutral treatment. This sweep
// guards that the indicator still paints a visible, non-opaque-black pill in all 3 langs × 2 themes
// (no DesignLanguage branch was added to the .cpp). Design language + theme are GLOBAL singletons, so
// the fixture restores both in TearDown. zh_CN: AnnotatedScrollBar 的位置指示器胶囊为 colors.accentDefault
// ——强调色在三种设计语言(Fluent/Material 3/macOS)下都是正确读法,故跨语言保持强调色;未绘制中性轨道,
// 无需做 M3/macOS 中性处理。本扫描确保指示器在 3 语言 × 2 主题下仍绘制出可见且非不透明黑的胶囊
//(.cpp 未新增 DesignLanguage 分支)。设计语言与主题为全局单例,夹具在 TearDown 中恢复二者。
class AnnotatedScrollBarDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override
    {
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }
};

TEST_F(AnnotatedScrollBarDesignLanguageTest, IndicatorPaintsAccentPillWithoutOpaqueBlack)
{
    struct LangCase { fluent::FluentElement::DesignLanguage lang; const char* name; };
    struct ThemeCase { fluent::FluentElement::Theme theme; const char* name; };

    const LangCase langs[] = {
        { fluent::FluentElement::DesignFluent, "Fluent" },
        { fluent::FluentElement::DesignMaterial, "Material" },
        { fluent::FluentElement::DesignCupertino, "Cupertino" },
    };
    const ThemeCase themes[] = {
        { fluent::FluentElement::Light, "Light" },
        { fluent::FluentElement::Dark, "Dark" },
    };

    for (const auto& lang : langs) {
        for (const auto& th : themes) {
            fluent::ThemeRegistry::instance().setDesignLanguage(lang.lang);
            fluent::FluentElement::setTheme(th.theme);

            const std::string ctx = std::string(lang.name) + "/" + th.name;

            AnnotatedScrollBar bar;
            bar.resize(120, 300);
            bar.setRange(0, 1000);
            bar.setValue(500);
            bar.setLabels({AnnotatedScrollBarLabel(QStringLiteral("A"), 0),
                           AnnotatedScrollBarLabel(QStringLiteral("B"), 1000)});
            showAndProcess(bar);

            QImage image(bar.size(), QImage::Format_ARGB32);
            image.fill(Qt::transparent);
            bar.render(&image);

            // 1. The indicator pill paints something visible at its center. zh_CN: 指示器胶囊在中心绘制出可见内容。
            const QPoint center = bar.indicatorCenter().toPoint();
            const QColor c = QColor::fromRgba(image.pixel(center));
            EXPECT_GT(c.alpha(), 0) << "indicator painted nothing: " << ctx;

            // 2. It must NOT be an opaque near-#000 surface (invalid-QColor trap). The accent default is
            // a saturated hue under every preset, never near-black. zh_CN: 不得为不透明近黑表面(无效 QColor
            // 陷阱)。各预设的 accentDefault 都是饱和色相,绝非近黑。
            const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
            const bool opaqueBlack = c.alpha() > 200 && lum < 16;
            EXPECT_FALSE(opaqueBlack)
                << "AnnotatedScrollBar painted an opaque black indicator: " << ctx << " rgba=("
                << c.red() << "," << c.green() << "," << c.blue() << "," << c.alpha() << ")";
        }
    }
}

TEST_F(AnnotatedScrollBarTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;

    auto* window = new AnnotatedScrollBarVisualWindow();
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setWindowTitle(QStringLiteral("AnnotatedScrollBar Visual Test"));
    window->resize(1120, 760);
    window->onThemeUpdated();

    auto* root = new AnchorLayout(window);
    window->setLayout(root);

    auto* title = new Label(QStringLiteral("AnnotatedScrollBar"), window);
    title->setFluentTypography(QStringLiteral("Title"));
    title->setFixedSize(360, 40);
    AnchorLayout::Anchors titleAnchors;
    titleAnchors.left = {window, Edge::Left, 24};
    titleAnchors.top = {window, Edge::Top, 24};
    root->addAnchoredWidget(title, titleAnchors);

    auto* compactButton = new Button(QStringLiteral("Compact"), window);
    compactButton->setFixedSize(100, 32);

    auto* themeButton = new Button(QStringLiteral("Theme"), window);
    themeButton->setFixedSize(100, 32);
    AnchorLayout::Anchors themeAnchors;
    themeAnchors.right = {window, Edge::Right, -24};
    themeAnchors.top = {window, Edge::Top, 28};
    root->addAnchoredWidget(themeButton, themeAnchors);

    AnchorLayout::Anchors compactAnchors;
    compactAnchors.right = {themeButton, Edge::Left, -12};
    compactAnchors.top = {themeButton, Edge::Top, 0};
    root->addAnchoredWidget(compactButton, compactAnchors);

    auto* settingsPanel = new VisualPanel(window);
    settingsPanel->setFixedSize(400, 152);
    auto* settingsLayout = new AnchorLayout(settingsPanel);
    settingsPanel->setLayout(settingsLayout);
    AnchorLayout::Anchors settingsAnchors;
    settingsAnchors.right = {window, Edge::Right, -24};
    settingsAnchors.top = {window, Edge::Top, 96};
    root->addAnchoredWidget(settingsPanel, settingsAnchors);

    auto* standalone = new AnnotatedScrollBar(window);
    standalone->setRange(0, 960);
    standalone->setLabelColumnWidth(56);
    standalone->setIndicatorWidth(32);
    standalone->setLabels(yearLabels());
    standalone->setDetailLabelProvider([](int offset) {
        const int year = 2023 - std::clamp(offset / 120, 0, 8);
        return QStringLiteral("October %1").arg(year);
    });
    standalone->setFixedSize(148, 520);
    AnchorLayout::Anchors standaloneAnchors;
    standaloneAnchors.left = {window, Edge::Left, 36};
    standaloneAnchors.top = {window, Edge::Top, 172};
    root->addAnchoredWidget(standalone, standaloneAnchors);

    auto* scrollView = new ScrollView(window);
    scrollView->setFixedSize(600, 460);
    scrollView->setWidget(new ColorSectionsContent());
    scrollView->setVerticalScrollBarVisibility(ScrollView::ScrollBarVisibility::Hidden);
    AnchorLayout::Anchors scrollAnchors;
    scrollAnchors.left = {standalone, Edge::Right, 36};
    scrollAnchors.top = {window, Edge::Top, 268};
    root->addAnchoredWidget(scrollView, scrollAnchors);

    auto* linkedBar = new AnnotatedScrollBar(window);
    linkedBar->setLabelColumnWidth(92);
    linkedBar->setMinimumLabelSpacing(64);
    linkedBar->setIndicatorWidth(36);
    linkedBar->setCaretSize(QSize(16, 18));
    linkedBar->setPreferredSize(QSize(164, 460));
    linkedBar->setMinimumBarSize(QSize(128, 240));
    linkedBar->setFixedSize(164, 460);
    linkedBar->setLabels({
        AnnotatedScrollBarLabel(QStringLiteral("Azure"), 0),
        AnnotatedScrollBarLabel(QStringLiteral("Crimson"), 180),
        AnnotatedScrollBarLabel(QStringLiteral("Cyan"), 360),
        AnnotatedScrollBarLabel(QStringLiteral("Fuchsia"), 540),
        AnnotatedScrollBarLabel(QStringLiteral("Gold"), 700)
    });
    linkedBar->setDetailLabelProvider([linkedBar](int offset) {
        const QVector<AnnotatedScrollBarLabel> labels = linkedBar->labels();
        AnnotatedScrollBarLabel current = labels.first();
        for (const AnnotatedScrollBarLabel& label : labels) {
            if (offset >= label.offset)
                current = label;
        }
        return current.text;
    });
    linkedBar->connectToScrollView(scrollView);
    AnchorLayout::Anchors linkedAnchors;
    linkedAnchors.left = {scrollView, Edge::Right, 12};
    linkedAnchors.top = {scrollView, Edge::Top, 0};
    root->addAnchoredWidget(linkedBar, linkedAnchors);

    auto* heightDescription = new Label(QStringLiteral("Changing the AnnotatedScrollBar height refreshes its Labels layout."), settingsPanel);
    heightDescription->setFluentTypography(QStringLiteral("Body"));
    heightDescription->setWordWrap(true);
    heightDescription->setFixedSize(360, 42);
    AnchorLayout::Anchors descriptionAnchors;
    descriptionAnchors.left = {settingsPanel, Edge::Left, 20};
    descriptionAnchors.top = {settingsPanel, Edge::Top, 14};
    settingsLayout->addAnchoredWidget(heightDescription, descriptionAnchors);

    auto* heightLabel = new Label(QStringLiteral("AnnotatedScrollBar maximum height:"), settingsPanel);
    heightLabel->setFluentTypography(QStringLiteral("BodyStrong"));
    heightLabel->setFixedSize(260, 24);
    AnchorLayout::Anchors heightLabelAnchors;
    heightLabelAnchors.left = {heightDescription, Edge::Left, 0};
    heightLabelAnchors.top = {heightDescription, Edge::Bottom, 10};
    settingsLayout->addAnchoredWidget(heightLabel, heightLabelAnchors);

    auto* heightValue = new Label(QStringLiteral("460 px"), settingsPanel);
    heightValue->setFluentTypography(QStringLiteral("Caption"));
    heightValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    heightValue->setFixedSize(72, 24);
    AnchorLayout::Anchors heightValueAnchors;
    heightValueAnchors.right = {heightDescription, Edge::Right, 0};
    heightValueAnchors.top = {heightLabel, Edge::Top, 0};
    settingsLayout->addAnchoredWidget(heightValue, heightValueAnchors);

    auto* heightSlider = new Slider(Qt::Horizontal, settingsPanel);
    heightSlider->setRange(240, 460);
    heightSlider->setSingleStep(20);
    heightSlider->setPageStep(40);
    heightSlider->setValue(460);
    heightSlider->setFixedSize(360, 36);
    AnchorLayout::Anchors sliderAnchors;
    sliderAnchors.left = {heightDescription, Edge::Left, 0};
    sliderAnchors.top = {heightLabel, Edge::Bottom, 8};
    settingsLayout->addAnchoredWidget(heightSlider, sliderAnchors);

    QObject::connect(themeButton, &Button::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                                    ? fluent::FluentElement::Dark
                                    : fluent::FluentElement::Light);
    });
    QObject::connect(compactButton, &Button::clicked, heightSlider, [heightSlider]() {
        heightSlider->setValue(heightSlider->value() > 320 ? 260 : 460);
    });
    QObject::connect(heightSlider, &Slider::valueChanged, linkedBar, [linkedBar, heightValue](int value) {
        linkedBar->setFixedHeight(value);
        linkedBar->updateGeometry();
        heightValue->setText(QStringLiteral("%1 px").arg(value));
    });

    window->show();
    qApp->exec();
}
