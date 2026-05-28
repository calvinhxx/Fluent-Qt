#include <gtest/gtest.h>

#include <QApplication>
#include <QFrame>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPalette>
#include <QPointF>
#include <QRect>
#include <QSignalSpy>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <cmath>

#include "design/Typography.h"
#include "view/foundation/FluentElement.h"
#include "view/basicinput/Button.h"
#include "view/status_info/InfoBadge.h"

using namespace view::status_info;
using view::basicinput::Button;

class InfoBadgeTestWindow : public QWidget, public FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        QPalette pal = palette();
        pal.setColor(QPalette::Window, c.bgCanvas);
        setPalette(pal);
        setAutoFillBackground(true);
    }
};

class InfoBadgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        FluentElement::setTheme(FluentElement::Light);
        window = new InfoBadgeTestWindow();
        window->setWindowTitle("InfoBadge Visual Test");
        window->resize(900, 720);
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
        window = nullptr;
        FluentElement::setTheme(FluentElement::Light);
    }

    QImage renderBadge(InfoBadge& badge) {
        badge.resize(badge.sizeHint());
        QImage image(badge.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        badge.render(&image);
        return image;
    }

    bool hasVisiblePixel(const QImage& image) const {
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(image.pixel(x, y)) > 0) return true;
            }
        }
        return false;
    }

    QRect visibleAlphaBounds(const QImage& image) const {
        int left = image.width();
        int top = image.height();
        int right = -1;
        int bottom = -1;

        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(image.pixel(x, y)) == 0) continue;
                left = qMin(left, x);
                top = qMin(top, y);
                right = qMax(right, x);
                bottom = qMax(bottom, y);
            }
        }

        if (right < left || bottom < top) return QRect();
        return QRect(QPoint(left, top), QPoint(right, bottom));
    }

    InfoBadgeTestWindow* window = nullptr;
};

TEST_F(InfoBadgeTest, DefaultPropertyValues) {
    InfoBadge badge;

    EXPECT_EQ(badge.value(), -1);
    EXPECT_TRUE(badge.iconGlyph().isEmpty());
    EXPECT_EQ(badge.displayMode(), InfoBadge::InfoBadgeDisplayMode::Auto);
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Dot);
    EXPECT_EQ(badge.status(), InfoBadge::InfoBadgeStatus::Attention);
    EXPECT_DOUBLE_EQ(badge.badgeOpacity(), 1.0);
    EXPECT_FALSE(badge.customBackgroundColor().isValid());
    EXPECT_FALSE(badge.customTextColor().isValid());
    EXPECT_EQ(badge.valueFontRole(), Typography::FontRole::Caption);
    EXPECT_EQ(badge.beaconDiameter(), 4);
    EXPECT_EQ(badge.badgeHeight(), 16);
    EXPECT_EQ(badge.valueHorizontalPadding(), 8);
    EXPECT_EQ(badge.iconGlyphSize(), 10);
    EXPECT_EQ(badge.iconFontFamily(), Typography::FontFamily::SegoeFluentIcons);
    EXPECT_EQ(badge.badgeBackgroundInset(), 0);
    EXPECT_EQ(badge.contentOffset(), QPoint(0, 0));
    EXPECT_EQ(badge.sizeHint(), QSize(4, 4));
}

TEST_F(InfoBadgeTest, PropertySignalsAndSameValueNoSignal) {
    InfoBadge badge;

    QSignalSpy valueSpy(&badge, &InfoBadge::valueChanged);
    badge.setValue(5);
    EXPECT_EQ(valueSpy.count(), 1);
    badge.setValue(5);
    EXPECT_EQ(valueSpy.count(), 1);

    QSignalSpy iconSpy(&badge, &InfoBadge::iconGlyphChanged);
    badge.setIconGlyph(Typography::Icons::Mail);
    EXPECT_EQ(iconSpy.count(), 1);
    badge.setIconGlyph(Typography::Icons::Mail);
    EXPECT_EQ(iconSpy.count(), 1);

    QSignalSpy modeSpy(&badge, &InfoBadge::displayModeChanged);
    badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Icon);
    EXPECT_EQ(modeSpy.count(), 1);
    badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Icon);
    EXPECT_EQ(modeSpy.count(), 1);

    QSignalSpy statusSpy(&badge, &InfoBadge::statusChanged);
    badge.setStatus(InfoBadge::InfoBadgeStatus::Critical);
    EXPECT_EQ(statusSpy.count(), 1);
    badge.setStatus(InfoBadge::InfoBadgeStatus::Critical);
    EXPECT_EQ(statusSpy.count(), 1);

    QSignalSpy opacitySpy(&badge, &InfoBadge::badgeOpacityChanged);
    badge.setBadgeOpacity(0.5);
    EXPECT_EQ(opacitySpy.count(), 1);
    badge.setBadgeOpacity(0.5);
    EXPECT_EQ(opacitySpy.count(), 1);

    QSignalSpy backgroundSpy(&badge, &InfoBadge::customBackgroundColorChanged);
    badge.setCustomBackgroundColor(QColor("#C42B1C"));
    EXPECT_EQ(backgroundSpy.count(), 1);
    badge.setCustomBackgroundColor(QColor("#C42B1C"));
    EXPECT_EQ(backgroundSpy.count(), 1);

    QSignalSpy textSpy(&badge, &InfoBadge::customTextColorChanged);
    badge.setCustomTextColor(Qt::white);
    EXPECT_EQ(textSpy.count(), 1);
    badge.setCustomTextColor(Qt::white);
    EXPECT_EQ(textSpy.count(), 1);

    QSignalSpy glyphSizeSpy(&badge, &InfoBadge::iconGlyphSizeChanged);
    badge.setIconGlyphSize(12);
    EXPECT_EQ(glyphSizeSpy.count(), 1);
    badge.setIconGlyphSize(12);
    EXPECT_EQ(glyphSizeSpy.count(), 1);
}

TEST_F(InfoBadgeTest, ConfigurableMetricsAffectGeometryAndRendering) {
    InfoBadge badge;

    QSignalSpy beaconSpy(&badge, &InfoBadge::beaconDiameterChanged);
    badge.setBeaconDiameter(6);
    EXPECT_EQ(beaconSpy.count(), 1);
    EXPECT_EQ(badge.sizeHint(), QSize(6, 6));
    badge.setBeaconDiameter(0);
    EXPECT_EQ(badge.beaconDiameter(), 6);
    EXPECT_EQ(beaconSpy.count(), 1);

    badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Icon);
    badge.setIconGlyph(Typography::Icons::Message);
    QSignalSpy heightSpy(&badge, &InfoBadge::badgeHeightChanged);
    badge.setBadgeHeight(18);
    EXPECT_EQ(heightSpy.count(), 1);
    EXPECT_EQ(badge.sizeHint(), QSize(18, 18));

    QSignalSpy fontFamilySpy(&badge, &InfoBadge::iconFontFamilyChanged);
    badge.setIconFontFamily(Typography::FontFamily::SegoeUI);
    EXPECT_EQ(fontFamilySpy.count(), 1);
    badge.setIconFontFamily(Typography::FontFamily::SegoeUI);
    EXPECT_EQ(fontFamilySpy.count(), 1);

    QSignalSpy insetSpy(&badge, &InfoBadge::badgeBackgroundInsetChanged);
    badge.setBadgeBackgroundInset(1);
    EXPECT_EQ(insetSpy.count(), 1);
    badge.setBadgeBackgroundInset(-2);
    EXPECT_EQ(badge.badgeBackgroundInset(), 0);
    EXPECT_EQ(insetSpy.count(), 2);

    QSignalSpy offsetSpy(&badge, &InfoBadge::contentOffsetChanged);
    badge.setContentOffset(QPoint(1, -1));
    EXPECT_EQ(offsetSpy.count(), 1);
    EXPECT_EQ(badge.contentOffset(), QPoint(1, -1));

    badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Value);
    badge.setValue(100);
    badge.setBadgeHeight(16);
    const int compactWidth = badge.sizeHint().width();
    QSignalSpy paddingSpy(&badge, &InfoBadge::valueHorizontalPaddingChanged);
    badge.setValueHorizontalPadding(18);
    EXPECT_EQ(paddingSpy.count(), 1);
    EXPECT_GT(badge.sizeHint().width(), compactWidth);
    badge.setValueHorizontalPadding(18);
    EXPECT_EQ(paddingSpy.count(), 1);
}

TEST_F(InfoBadgeTest, DisplayModeSelection) {
    InfoBadge badge;
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Dot);

    badge.setIconGlyph(Typography::Icons::Message);
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Icon);
    EXPECT_EQ(badge.sizeHint(), QSize(16, 16));

    badge.setValue(5);
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Value);
    EXPECT_EQ(badge.sizeHint(), QSize(16, 16));

    badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Dot);
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Dot);
    EXPECT_EQ(badge.sizeHint(), QSize(4, 4));

    badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Icon);
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Icon);
    EXPECT_EQ(badge.sizeHint(), QSize(16, 16));

    badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Value);
    badge.setValue(-1);
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Value);
    EXPECT_EQ(badge.sizeHint(), QSize(16, 16));
}

TEST_F(InfoBadgeTest, ValueClampAndSentinelFallback) {
    InfoBadge badge;
    badge.setIconGlyph(Typography::Icons::Message);
    badge.setValue(10);
    EXPECT_EQ(badge.value(), 10);
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Value);
    EXPECT_GT(badge.sizeHint().width(), 16);

    badge.setValue(-1);
    EXPECT_EQ(badge.value(), -1);
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Icon);

    badge.setIconGlyph(QString());
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Dot);

    badge.setValue(6);
    QSignalSpy valueSpy(&badge, &InfoBadge::valueChanged);
    badge.setValue(-9);
    EXPECT_EQ(badge.value(), -1);
    EXPECT_EQ(valueSpy.count(), 1);
}

TEST_F(InfoBadgeTest, SizeHintsReflectVariants) {
    InfoBadge badge;
    EXPECT_EQ(badge.sizeHint(), QSize(4, 4));
    EXPECT_EQ(badge.minimumSizeHint(), QSize(4, 4));

    badge.setIconGlyph(Typography::Icons::Mail);
    EXPECT_EQ(badge.sizeHint(), QSize(16, 16));

    badge.setValue(7);
    EXPECT_EQ(badge.sizeHint(), QSize(16, 16));

    badge.setValue(100);
    EXPECT_EQ(badge.sizeHint().height(), 16);
    EXPECT_GT(badge.sizeHint().width(), 16);
}

TEST_F(InfoBadgeTest, OpacityClampAndRendering) {
    InfoBadge badge;
    badge.setValue(100);
    const QSize wideSize = badge.sizeHint();

    QSignalSpy opacitySpy(&badge, &InfoBadge::badgeOpacityChanged);
    badge.setBadgeOpacity(2.0);
    EXPECT_DOUBLE_EQ(badge.badgeOpacity(), 1.0);
    EXPECT_EQ(opacitySpy.count(), 0);

    badge.setBadgeOpacity(-0.5);
    EXPECT_DOUBLE_EQ(badge.badgeOpacity(), 0.0);
    EXPECT_EQ(opacitySpy.count(), 1);
    EXPECT_EQ(badge.sizeHint(), wideSize);
    EXPECT_FALSE(hasVisiblePixel(renderBadge(badge)));

    badge.setBadgeOpacity(1.0);
    EXPECT_TRUE(hasVisiblePixel(renderBadge(badge)));
}

TEST_F(InfoBadgeTest, StatusCustomColorsAndThemeRendering) {
    InfoBadge badge;
    badge.setValue(5);

    badge.setStatus(InfoBadge::InfoBadgeStatus::Informational);
    EXPECT_EQ(badge.effectiveBackgroundColor(), badge.themeColors().systemInfo);
    badge.setStatus(InfoBadge::InfoBadgeStatus::Caution);
    EXPECT_EQ(badge.effectiveBackgroundColor(), badge.themeColors().systemCaution);
    badge.setStatus(InfoBadge::InfoBadgeStatus::Success);
    EXPECT_EQ(badge.effectiveBackgroundColor(), badge.themeColors().systemSuccess);
    badge.setStatus(InfoBadge::InfoBadgeStatus::Critical);
    EXPECT_EQ(badge.effectiveBackgroundColor(), badge.themeColors().systemCritical);

    badge.setCustomBackgroundColor(QColor("#C42B1C"));
    badge.setCustomTextColor(QColor("#FFFFFF"));
    EXPECT_EQ(badge.effectiveBackgroundColor(), QColor("#C42B1C"));
    EXPECT_EQ(badge.effectiveForegroundColor(), QColor("#FFFFFF"));

    const QImage light = renderBadge(badge);
    FluentElement::setTheme(FluentElement::Dark);
    badge.setCustomBackgroundColor(QColor());
    badge.setCustomTextColor(QColor());
    badge.onThemeUpdated();
    badge.setStatus(InfoBadge::InfoBadgeStatus::Attention);
    EXPECT_EQ(badge.effectiveBackgroundColor(), badge.themeColors().accentDefault);
    const QImage dark = renderBadge(badge);
    EXPECT_NE(light, dark);

    badge.setEnabled(false);
    QApplication::processEvents();
    EXPECT_EQ(badge.effectiveBackgroundColor(), badge.themeColors().accentDisabled);
    EXPECT_TRUE(hasVisiblePixel(renderBadge(badge)));
}

TEST_F(InfoBadgeTest, IconGlyphIsVisuallyCentered) {
    InfoBadge badge;
    badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Icon);
    badge.setIconGlyph(Typography::Icons::Message);
    badge.setCustomBackgroundColor(QColor(0, 0, 0, 0));
    badge.setCustomTextColor(Qt::black);

    const QImage image = renderBadge(badge);
    const QRect bounds = visibleAlphaBounds(image);
    ASSERT_FALSE(bounds.isNull());

    const QPointF imageCenter((image.width() - 1) / 2.0, (image.height() - 1) / 2.0);
    const QPointF glyphCenter = QRectF(bounds).center();
    EXPECT_LE(std::abs(glyphCenter.x() - imageCenter.x()), 1.0);
    EXPECT_LE(std::abs(glyphCenter.y() - imageCenter.y()), 1.0);
}

TEST_F(InfoBadgeTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        GTEST_SKIP() << "Skipping visual test in offscreen mode";
    }

    window->resize(940, 760);

    auto* root = new QVBoxLayout(window);
    root->setContentsMargins(24, 22, 24, 24);
    root->setSpacing(14);

    auto* header = new QHBoxLayout();
    auto* title = new QLabel("InfoBadge", window);
    QFont titleFont = title->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    title->setFont(titleFont);
    auto* themeButton = new Button("Toggle Light/Dark", window);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(themeButton);
    root->addLayout(header);

    QObject::connect(themeButton, &Button::clicked, [this]() {
        FluentElement::setTheme(FluentElement::currentTheme() == FluentElement::Light
            ? FluentElement::Dark
            : FluentElement::Light);
        window->onThemeUpdated();
    });

    auto makeSectionLabel = [this](const QString& text) {
        auto* label = new QLabel(text, window);
        QFont font = label->font();
        font.setBold(true);
        label->setFont(font);
        return label;
    };

    auto makePanel = [this](int minimumHeight) {
        auto* panel = new QFrame(window);
        panel->setFrameShape(QFrame::StyledPanel);
        panel->setMinimumHeight(minimumHeight);
        panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        return panel;
    };

    root->addWidget(makeSectionLabel("Variants"));
    auto* variantsPanel = makePanel(320);
    auto* grid = new QGridLayout(variantsPanel);
    grid->setContentsMargins(28, 22, 28, 22);
    grid->setHorizontalSpacing(30);
    grid->setVerticalSpacing(18);
    root->addWidget(variantsPanel);

    const QVector<InfoBadge::InfoBadgeStatus> statuses = {
        InfoBadge::InfoBadgeStatus::Informational,
        InfoBadge::InfoBadgeStatus::Attention,
        InfoBadge::InfoBadgeStatus::Caution,
        InfoBadge::InfoBadgeStatus::Success,
        InfoBadge::InfoBadgeStatus::Critical,
    };
    const QStringList statusNames = {"Info", "Attention", "Caution", "Success", "Critical"};

    auto addBadgeExample = [grid, variantsPanel](int row, int column, const QString& labelText,
                                                 InfoBadge::InfoBadgeStatus status,
                                                 InfoBadge::InfoBadgeDisplayMode mode,
                                                 int value,
                                                 const QString& glyph) {
        auto* cell = new QWidget(variantsPanel);
        auto* layout = new QVBoxLayout(cell);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        layout->setAlignment(Qt::AlignHCenter);

        auto* badge = new InfoBadge(cell);
        badge->setStatus(status);
        badge->setDisplayMode(mode);
        badge->setIconGlyph(glyph);
        badge->setValue(value);

        auto* label = new QLabel(labelText, cell);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(badge, 0, Qt::AlignHCenter);
        layout->addWidget(label, 0, Qt::AlignHCenter);
        grid->addWidget(cell, row, column);
    };

    for (int i = 0; i < statuses.size(); ++i) {
        addBadgeExample(0, i, statusNames.at(i) + " dot", statuses.at(i), InfoBadge::InfoBadgeDisplayMode::Dot, -1, QString());
        addBadgeExample(1, i, statusNames.at(i) + " icon", statuses.at(i), InfoBadge::InfoBadgeDisplayMode::Icon, -1, Typography::Icons::ImportantBadge12);
        addBadgeExample(2, i, statusNames.at(i) + " 5", statuses.at(i), InfoBadge::InfoBadgeDisplayMode::Value, 5, QString());
        addBadgeExample(3, i, statusNames.at(i) + " 100", statuses.at(i), InfoBadge::InfoBadgeDisplayMode::Value, 100, QString());
    }

    root->addWidget(makeSectionLabel("Embedded"));
    auto* embeddedPanel = makePanel(190);
    auto* embeddedLayout = new QHBoxLayout(embeddedPanel);
    embeddedLayout->setContentsMargins(28, 24, 28, 24);
    embeddedLayout->setSpacing(42);
    root->addWidget(embeddedPanel);

    auto* buttonHost = new QWidget(embeddedPanel);
    buttonHost->setFixedSize(170, 64);
    auto* inboxButton = new Button("Inbox", buttonHost);
    inboxButton->setGeometry(0, 16, 150, 32);
    auto* cornerBadge = new InfoBadge(buttonHost);
    cornerBadge->setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Icon);
    cornerBadge->setIconGlyph(Typography::Icons::Mail);
    cornerBadge->setCustomBackgroundColor(QColor("#C42B1C"));
    cornerBadge->setCustomTextColor(Qt::white);
    cornerBadge->resize(cornerBadge->sizeHint());
    cornerBadge->move(138, 8);
    embeddedLayout->addWidget(buttonHost, 0, Qt::AlignTop);

    auto makeNavRow = [embeddedPanel](const QString& text, qreal opacity) {
        auto* row = new QFrame(embeddedPanel);
        row->setFrameShape(QFrame::StyledPanel);
        row->setFixedSize(230, 44);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(14, 6, 14, 6);
        layout->setSpacing(12);
        auto* label = new QLabel(text, row);
        auto* badge = new InfoBadge(row);
        badge->setValue(5);
        badge->setBadgeOpacity(opacity);
        layout->addWidget(label);
        layout->addStretch();
        layout->addWidget(badge, 0, Qt::AlignVCenter);
        return row;
    };
    embeddedLayout->addWidget(makeNavRow("Navigation item", 1.0), 0, Qt::AlignTop);
    embeddedLayout->addWidget(makeNavRow("Opacity reserved", 0.0), 0, Qt::AlignTop);
    embeddedLayout->addStretch();

    root->addWidget(makeSectionLabel("Dynamic values"));
    auto* dynamicPanel = makePanel(110);
    auto* dynamicLayout = new QHBoxLayout(dynamicPanel);
    dynamicLayout->setContentsMargins(28, 18, 28, 18);
    dynamicLayout->setSpacing(28);
    root->addWidget(dynamicPanel);

    for (int value : {-1, 0, 5, 10, 100}) {
        auto* cell = new QWidget(dynamicPanel);
        auto* layout = new QVBoxLayout(cell);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        auto* badge = new InfoBadge(cell);
        badge->setIconGlyph(Typography::Icons::Message);
        badge->setValue(value);
        auto* label = new QLabel(QString::number(value), cell);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(badge, 0, Qt::AlignHCenter);
        layout->addWidget(label, 0, Qt::AlignHCenter);
        dynamicLayout->addWidget(cell, 0, Qt::AlignTop);
    }
    dynamicLayout->addStretch();

    window->show();
    qApp->exec();
}