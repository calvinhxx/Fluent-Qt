#include <gtest/gtest.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPalette>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>
#include <QVector>
#include <QVBoxLayout>

#include "view/foundation/FluentElement.h"
#include "view/basicinput/Button.h"
#include "view/basicinput/HyperlinkButton.h"
#include "view/status_info/InfoBar.h"
#include "view/textfields/Label.h"

using namespace view::status_info;
using view::basicinput::Button;
using view::basicinput::HyperlinkButton;
using view::textfields::Label;

namespace {
constexpr int kExpectedCloseContentGap = 12;

QString renderedLabelText(const Label* label) {
    return static_cast<const QLabel*>(label)->text();
}
}

class InfoBarTestWindow : public QWidget, public FluentElement {
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

class InfoBarTest : public ::testing::Test {
protected:
    void SetUp() override {
        FluentElement::setTheme(FluentElement::Light);
        window = new InfoBarTestWindow();
        window->setWindowTitle("InfoBar Visual Test");
        window->resize(760, 720);
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
        window = nullptr;
        FluentElement::setTheme(FluentElement::Light);
    }

    QImage renderBar(InfoBar& bar) {
        bar.resize(bar.sizeHint());
        bar.show();
        QApplication::processEvents();

        QImage image(bar.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        bar.render(&image);
        return image;
    }

    QColor badgeCenterColor(const QImage& image) const {
        return QColor::fromRgba(image.pixel(23, 24));
    }

    InfoBarTestWindow* window = nullptr;
};

TEST_F(InfoBarTest, DefaultPropertyValues) {
    InfoBar bar;

    EXPECT_TRUE(bar.isOpen());
    EXPECT_EQ(bar.title(), "Title");
    EXPECT_TRUE(bar.message().isEmpty());
    EXPECT_EQ(bar.severity(), InfoBar::Informational);
    EXPECT_TRUE(bar.isClosable());
    EXPECT_TRUE(bar.isIconVisible());
    EXPECT_TRUE(bar.singleLine());
    EXPECT_EQ(bar.preferredWidth(), 600);
    EXPECT_EQ(bar.actionWidget(), nullptr);
    EXPECT_EQ(bar.severityIconGlyphSize(), 10);
    EXPECT_EQ(bar.severityIconBackgroundInset(), 1);
    EXPECT_EQ(bar.informationalIconGlyph(), Typography::Icons::AsteriskBadge12);
    EXPECT_EQ(bar.successIconGlyph(), Typography::Icons::CheckmarkBadge12);
    EXPECT_EQ(bar.warningIconGlyph(), Typography::Icons::ImportantBadge12);
    EXPECT_EQ(bar.errorIconGlyph(), Typography::Icons::ErrorBadge12);
    EXPECT_EQ(bar.sizeHint(), QSize(600, 50));
    EXPECT_GE(bar.minimumSizeHint().height(), 50);

    auto* closeButton = bar.findChild<Button*>("InfoBarCloseButton");
    ASSERT_NE(closeButton, nullptr);
    EXPECT_FALSE(closeButton->isHidden());
}

TEST_F(InfoBarTest, PropertySignalsAndSameValueNoSignal) {
    InfoBar bar;

    QSignalSpy titleSpy(&bar, &InfoBar::titleChanged);
    bar.setTitle("Updated");
    bar.setTitle("Updated");
    EXPECT_EQ(titleSpy.count(), 1);

    QSignalSpy messageSpy(&bar, &InfoBar::messageChanged);
    bar.setMessage("Message");
    bar.setMessage("Message");
    EXPECT_EQ(messageSpy.count(), 1);

    QSignalSpy severitySpy(&bar, &InfoBar::severityChanged);
    bar.setSeverity(InfoBar::Warning);
    bar.setSeverity(InfoBar::Warning);
    EXPECT_EQ(severitySpy.count(), 1);

    QSignalSpy openSpy(&bar, &InfoBar::isOpenChanged);
    bar.setIsOpen(false);
    bar.setIsOpen(false);
    EXPECT_EQ(openSpy.count(), 1);
    bar.setIsOpen(true);
    EXPECT_EQ(openSpy.count(), 2);

    QSignalSpy closableSpy(&bar, &InfoBar::isClosableChanged);
    bar.setIsClosable(false);
    bar.setIsClosable(false);
    EXPECT_EQ(closableSpy.count(), 1);

    QSignalSpy iconSpy(&bar, &InfoBar::isIconVisibleChanged);
    bar.setIsIconVisible(false);
    bar.setIsIconVisible(false);
    EXPECT_EQ(iconSpy.count(), 1);

    QSignalSpy singleLineSpy(&bar, &InfoBar::singleLineChanged);
    bar.setSingleLine(false);
    bar.setSingleLine(false);
    EXPECT_EQ(singleLineSpy.count(), 1);

    QSignalSpy widthSpy(&bar, &InfoBar::preferredWidthChanged);
    bar.setPreferredWidth(420);
    bar.setPreferredWidth(420);
    bar.setPreferredWidth(0);
    bar.setPreferredWidth(-1);
    EXPECT_EQ(widthSpy.count(), 1);
    EXPECT_EQ(bar.preferredWidth(), 420);

    QSignalSpy actionSpy(&bar, &InfoBar::actionWidgetChanged);
    auto* action = new Button("Action");
    bar.setActionWidget(action);
    bar.setActionWidget(action);
    EXPECT_EQ(actionSpy.count(), 1);
}

TEST_F(InfoBarTest, ConfigurableLayoutAndIconProperties) {
    InfoBar bar;
    bar.resize(bar.sizeHint());
    bar.show();
    QApplication::processEvents();

    auto* closeButton = bar.findChild<Button*>("InfoBarCloseButton");
    ASSERT_NE(closeButton, nullptr);
    EXPECT_GT(bar.width() - closeButton->geometry().right(), bar.contentMargins().right());

    QSignalSpy marginsSpy(&bar, &InfoBar::contentMarginsChanged);
    bar.setContentMargins(QMargins(20, 16, 20, 16));
    EXPECT_EQ(marginsSpy.count(), 1);
    EXPECT_EQ(bar.contentMargins(), QMargins(20, 16, 20, 16));

    bar.resize(bar.sizeHint());
    QApplication::processEvents();
    EXPECT_EQ(bar.width() - closeButton->geometry().right(), bar.contentMargins().right() + 1);

    QSignalSpy iconGlyphSpy(&bar, &InfoBar::errorIconGlyphChanged);
    bar.setErrorIconGlyph("E");
    EXPECT_EQ(iconGlyphSpy.count(), 1);
    bar.setErrorIconGlyph("E");
    EXPECT_EQ(iconGlyphSpy.count(), 1);
    EXPECT_EQ(bar.errorIconGlyph(), "E");

    QSignalSpy titleFontSpy(&bar, &InfoBar::titleFontRoleChanged);
    bar.setTitleFontRole(Typography::FontRole::Subtitle);
    EXPECT_EQ(titleFontSpy.count(), 1);
    EXPECT_EQ(bar.titleFontRole(), Typography::FontRole::Subtitle);

    QSignalSpy radiusSpy(&bar, &InfoBar::cornerRadiusChanged);
    bar.setCornerRadius(-4);
    EXPECT_EQ(radiusSpy.count(), 1);
    EXPECT_EQ(bar.cornerRadius(), 0);

    QSignalSpy insetSpy(&bar, &InfoBar::severityIconBackgroundInsetChanged);
    bar.setSeverityIconBackgroundInset(2);
    EXPECT_EQ(insetSpy.count(), 1);
    EXPECT_EQ(bar.severityIconBackgroundInset(), 2);
}

TEST_F(InfoBarTest, OpenCloseAndCloseButtonBehavior) {
    InfoBar bar;
    bar.resize(bar.sizeHint());
    bar.show();
    QApplication::processEvents();

    auto* closeButton = bar.findChild<Button*>("InfoBarCloseButton");
    ASSERT_NE(closeButton, nullptr);

    QSignalSpy openSpy(&bar, &InfoBar::isOpenChanged);
    QSignalSpy closedSpy(&bar, &InfoBar::closed);
    QTest::mouseClick(closeButton, Qt::LeftButton);

    EXPECT_FALSE(bar.isOpen());
    EXPECT_TRUE(bar.isHidden());
    EXPECT_EQ(bar.sizeHint(), QSize(0, 0));
    EXPECT_EQ(openSpy.count(), 1);
    EXPECT_EQ(closedSpy.count(), 1);

    bar.setIsOpen(true);
    bar.setIsClosable(false);
    closeButton->click();
    EXPECT_TRUE(bar.isOpen());
    EXPECT_EQ(closedSpy.count(), 1);

    bar.setIsClosable(true);
    bar.setEnabled(false);
    closeButton->click();
    EXPECT_TRUE(bar.isOpen());
    EXPECT_EQ(closedSpy.count(), 1);
}

TEST_F(InfoBarTest, TriggerButtonOpensClosedInfoBar) {
    InfoBar bar;
    bar.setIsOpen(false);

    Button trigger("Show InfoBar");
    QObject::connect(&trigger, &Button::clicked, [&bar]() {
        bar.setIsOpen(true);
    });

    QSignalSpy openSpy(&bar, &InfoBar::isOpenChanged);
    trigger.click();

    EXPECT_TRUE(bar.isOpen());
    EXPECT_EQ(bar.sizeHint(), QSize(600, 50));
    EXPECT_EQ(openSpy.count(), 1);
}

TEST_F(InfoBarTest, SeverityAndIconVisibilityAffectRendering) {
    InfoBar bar;
    bar.setMessage("Essential app message for your users.");

    const QImage infoImage = renderBar(bar);
    const QColor infoBadge = badgeCenterColor(infoImage);
    const QColor infoBackground = QColor::fromRgba(infoImage.pixel(5, 5));
    EXPECT_GT(infoBadge.alpha(), 200);

    bar.setSeverity(InfoBar::Warning);
    const QImage warningImage = renderBar(bar);
    const QColor warningBadge = badgeCenterColor(warningImage);
    const QColor warningBackground = QColor::fromRgba(warningImage.pixel(5, 5));
    EXPECT_NE(infoBadge.rgb(), warningBadge.rgb());
    EXPECT_NE(infoBackground.rgb(), warningBackground.rgb());

    bar.resize(bar.sizeHint());
    auto* titleLabel = bar.findChild<Label*>("InfoBarTitleLabel");
    ASSERT_NE(titleLabel, nullptr);
    const int withIconLeft = titleLabel->geometry().left();

    bar.setIsIconVisible(false);
    bar.resize(bar.sizeHint());
    QApplication::processEvents();
    EXPECT_LT(titleLabel->geometry().left(), withIconLeft);
}

TEST_F(InfoBarTest, SingleLineAndMultiLineLayout) {
    InfoBar bar;
    bar.setTitle("Title");
    bar.setMessage("A short essential app message.");
    bar.resize(bar.sizeHint());
    QApplication::processEvents();

    auto* titleLabel = bar.findChild<Label*>("InfoBarTitleLabel");
    auto* messageLabel = bar.findChild<Label*>("InfoBarMessageLabel");
    ASSERT_NE(titleLabel, nullptr);
    ASSERT_NE(messageLabel, nullptr);

    EXPECT_EQ(bar.sizeHint(), QSize(600, 50));
    EXPECT_EQ(titleLabel->geometry().top(), messageLabel->geometry().top());
    EXPECT_LE(titleLabel->geometry().right(), messageLabel->geometry().left());

    const QString longSingleLineMessage = "A long essential app message for your users to be informed of, acknowledge, or take action on before closing the notification.";
    bar.setMessage(longSingleLineMessage);
    bar.resize(bar.sizeHint());
    QApplication::processEvents();
    EXPECT_EQ(messageLabel->text(), longSingleLineMessage);
    EXPECT_TRUE(messageLabel->isTextElided());
    EXPECT_NE(renderedLabelText(messageLabel), longSingleLineMessage);

    auto* closeButton = bar.findChild<Button*>("InfoBarCloseButton");
    ASSERT_NE(closeButton, nullptr);
    EXPECT_GE(closeButton->geometry().left() - messageLabel->geometry().right(), kExpectedCloseContentGap);

    bar.setSingleLine(false);
    bar.setMessage("A long essential app message for your users to be informed of, acknowledge, or take action on. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin dapibus dolor vitae justo rutrum, ut lobortis nibh mattis.");
    EXPECT_GE(bar.sizeHint().height(), 110);
    bar.resize(bar.sizeHint());
    QApplication::processEvents();

    EXPECT_LT(titleLabel->geometry().top(), messageLabel->geometry().top());
    EXPECT_TRUE(messageLabel->wordWrap());
    EXPECT_FALSE(messageLabel->isTextElided());

    auto* action = new Button("Action");
    bar.setActionWidget(action);
    EXPECT_GE(bar.sizeHint().height(), 158);
    bar.resize(bar.sizeHint());
    QApplication::processEvents();
    EXPECT_GT(action->geometry().top(), messageLabel->geometry().bottom());
}

TEST_F(InfoBarTest, ActionWidgetManagement) {
    InfoBar bar;
    QSignalSpy actionSpy(&bar, &InfoBar::actionWidgetChanged);

    auto* button = new Button("Action");
    bar.setActionWidget(button);
    EXPECT_EQ(bar.actionWidget(), button);
    EXPECT_EQ(button->parentWidget(), &bar);
    EXPECT_FALSE(button->isHidden());
    EXPECT_EQ(actionSpy.count(), 1);

    auto* link = new HyperlinkButton("Informational link", QUrl("https://www.example.com"));
    bar.setActionWidget(link);
    EXPECT_EQ(bar.actionWidget(), link);
    EXPECT_EQ(link->parentWidget(), &bar);
    EXPECT_EQ(button->parentWidget(), nullptr);
    EXPECT_EQ(actionSpy.count(), 2);

    bar.setActionWidget(nullptr);
    EXPECT_EQ(bar.actionWidget(), nullptr);
    EXPECT_EQ(link->parentWidget(), nullptr);
    EXPECT_EQ(actionSpy.count(), 3);

    delete button;
    delete link;
}

TEST_F(InfoBarTest, ThemeAndDisabledState) {
    InfoBar bar;
    bar.setMessage("Theme sensitive message.");

    const QImage lightImage = renderBar(bar);
    const QColor lightBackground = QColor::fromRgba(lightImage.pixel(5, 5));

    FluentElement::setTheme(FluentElement::Dark);
    const QImage darkImage = renderBar(bar);
    const QColor darkBackground = QColor::fromRgba(darkImage.pixel(5, 5));
    EXPECT_NE(lightBackground.rgb(), darkBackground.rgb());

    auto* closeButton = bar.findChild<Button*>("InfoBarCloseButton");
    ASSERT_NE(closeButton, nullptr);
    bar.setEnabled(false);
    EXPECT_FALSE(closeButton->isEnabled());
}

TEST_F(InfoBarTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        GTEST_SKIP() << "Skipping visual test in offscreen mode";
    }

    window->setWindowTitle("InfoBar VisualCheck - click buttons to open bars");
    window->resize(780, 760);

    QVector<InfoBar*> bars;

    auto* layout = new QVBoxLayout(window);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(14);

    auto* header = new QHBoxLayout();
    auto* title = new Label("InfoBar", window);
    title->setFluentTypography(Typography::FontRole::Title);

    auto* themeButton = new Button("Toggle Theme", window);
    themeButton->setFixedSize(160, 32);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(themeButton);
    layout->addLayout(header);

    QObject::connect(themeButton, &Button::clicked, [this, &bars]() {
        FluentElement::setTheme(FluentElement::currentTheme() == FluentElement::Light
            ? FluentElement::Dark
            : FluentElement::Light);
        window->onThemeUpdated();
        for (auto* bar : bars) {
            bar->onThemeUpdated();
        }
    });

    auto addDemo = [&](const QString& buttonText, InfoBar* bar) {
        auto* row = new QWidget(window);
        auto* rowLayout = new QVBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        auto* openButton = new Button(buttonText, row);
        openButton->setFixedSize(220, 32);
        rowLayout->addWidget(openButton, 0, Qt::AlignLeft);

        bar->setParent(row);
        bar->setIsOpen(false);
        rowLayout->addWidget(bar);
        layout->addWidget(row);
        bars.push_back(bar);

        QObject::connect(openButton, &Button::clicked, [bar, rowLayout]() {
            bar->setIsOpen(true);
            rowLayout->activate();
        });

        return bar;
    };

    auto* info = addDemo("Show informational", new InfoBar());
    info->setMessage("Essential app message for your users to be informed of, acknowledge, or take action on.");

    auto* success = addDemo("Show success", new InfoBar());
    success->setSeverity(InfoBar::Success);
    success->setMessage("Success message with a short single-line layout.");

    auto* warning = addDemo("Show warning", new InfoBar());
    warning->setSeverity(InfoBar::Warning);
    warning->setIsClosable(false);
    warning->setMessage("Warning message without a close button.");

    auto* error = addDemo("Show error without icon", new InfoBar());
    error->setSeverity(InfoBar::Error);
    error->setIsIconVisible(true);
    error->setMessage("Error message with the severity icon hidden.");

    auto* longBar = addDemo("Show long message", new InfoBar());
    longBar->setSingleLine(false);
    longBar->setMessage("A long essential app message for your users to be informed of, acknowledge, or take action on. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin dapibus dolor vitae justo rutrum, ut lobortis nibh mattis. Aenean id elit commodo, semper felis nec.");

    auto* buttonBar = addDemo("Show button action", new InfoBar());
    buttonBar->setSingleLine(false);
    buttonBar->setMessage("A long essential app message with an action button.");
    buttonBar->setActionWidget(new Button("Action"));

    auto* linkBar = addDemo("Show hyperlink action", new InfoBar());
    linkBar->setSingleLine(false);
    linkBar->setMessage("A long essential app message with a hyperlink action.");
    linkBar->setActionWidget(new HyperlinkButton("Informational link", QUrl("https://www.example.com")));

    window->show();
    qApp->exec();
}