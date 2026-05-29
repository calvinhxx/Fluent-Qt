#include <gtest/gtest.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include "components/basicinput/RepeatButton.h"
#include "components/foundation/FluentElement.h"

using namespace fluent;
using namespace fluent::basicinput;

class RepeatButtonTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class RepeatButtonTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new RepeatButtonTestWindow();
        window->setFixedSize(600, 400);
        window->setWindowTitle("Fluent RepeatButton Visual Test");

        auto* layout = new QVBoxLayout(window);
        layout->setContentsMargins(40, 40, 40, 40);
        layout->setSpacing(20);

        // 1. Basic RepeatButton with counter
        layout->addWidget(new QLabel("1. Click and hold to increment counter:", window));
        
        auto* hLayout = new QHBoxLayout();
        auto* repeatBtn = new RepeatButton("Click and hold", window);
        auto* countLabel = new QLabel("Number of clicks: 0", window);
        
        static int count = 0;
        QObject::connect(repeatBtn, &RepeatButton::clicked, [countLabel]() {
            count++;
            countLabel->setText(QString("Number of clicks: %1").arg(count));
        });

        hLayout->addWidget(repeatBtn);
        hLayout->addWidget(countLabel);
        hLayout->addStretch();
        layout->addLayout(hLayout);

        // 2. Different Intervals
        layout->addWidget(new QLabel("2. Fast repeat (Interval: 20ms):", window));
        auto* fastBtn = new RepeatButton("Fast Repeat", window);
        fastBtn->setInterval(20);
        layout->addWidget(fastBtn);

        // 3. Different Styles (inherits from Button)
        layout->addWidget(new QLabel("3. Accent Style RepeatButton:", window));
        auto* accentBtn = new RepeatButton("Accent Repeat", window);
        accentBtn->setFluentStyle(Button::Accent);
        layout->addWidget(accentBtn);

        layout->addStretch();

        // Theme switch button
        auto* themeBtn = new QPushButton("Switch Theme", window);
        layout->addWidget(themeBtn);
        QObject::connect(themeBtn, &QPushButton::clicked, []() {
            fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light 
                                    ? fluent::FluentElement::Dark 
                                    : fluent::FluentElement::Light);
        });

        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    RepeatButtonTestWindow* window = nullptr;
};

TEST_F(RepeatButtonTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->show();
    qApp->exec();
}
