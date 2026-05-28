#include <gtest/gtest.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include "view/menus_toolbars/Menu.h"
#include "view/basicinput/SplitButton.h"
#include "view/foundation/FluentElement.h"
#include "design/Typography.h"

using namespace view;
using namespace view::basicinput;
using namespace view::menus_toolbars;

class SplitButtonTestWindow : public QWidget, public FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class SplitButtonTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new SplitButtonTestWindow();
        window->setFixedSize(600, 500);
        window->setWindowTitle("Fluent SplitButton Visual Test");

        auto* layout = new QVBoxLayout(window);
        layout->setContentsMargins(40, 40, 40, 40);
        layout->setSpacing(20);

        // 1. Basic SplitButton (Standard)
        layout->addWidget(new QLabel("1. Basic SplitButton (Standard):", window));
        auto* split1 = new SplitButton("Choose Color", window);
        
        // 使用自定义 FluentMenu
        FluentMenu* menu1 = new FluentMenu("Colors", split1);
        menu1->addAction(new FluentMenuItem("Red", menu1));
        menu1->addAction(new FluentMenuItem("Green", menu1));
        menu1->addAction(new FluentMenuItem("Blue", menu1));
        split1->setMenu(menu1);
        
        QLabel* status1 = new QLabel("Status: Ready", window);
        QObject::connect(split1, &SplitButton::clicked, [status1]() {
            status1->setText("Status: Primary Clicked!");
        });
        
        layout->addWidget(split1);
        layout->addWidget(status1);

        // 2. Accent SplitButton
        layout->addWidget(new QLabel("2. Accent SplitButton:", window));
        auto* split2 = new SplitButton("Submit", window);
        split2->setFluentStyle(Button::Accent);
        
        FluentMenu* menu2 = new FluentMenu("Actions", split2);
        menu2->addAction(new FluentMenuItem("Submit and close", menu2));
        menu2->addAction(new FluentMenuItem("Submit and notify", menu2));
        split2->setMenu(menu2);
        layout->addWidget(split2);

        // 3. Different Sizes
        layout->addWidget(new QLabel("3. Different Sizes:", window));
        auto* h1 = new QHBoxLayout();
        auto* sSmall = new SplitButton("Small", window);
        sSmall->setFluentSize(Button::Small);
        auto* sNormal = new SplitButton("Standard", window);
        sNormal->setFluentSize(Button::StandardSize);
        auto* sLarge = new SplitButton("Large", window);
        sLarge->setFluentSize(Button::Large);
        h1->addWidget(sSmall);
        h1->addWidget(sNormal);
        h1->addWidget(sLarge);
        h1->addStretch();
        layout->addLayout(h1);

        layout->addStretch();

        // Theme switch button
        auto* themeBtn = new Button("Switch Theme", window);
        themeBtn->setFixedSize(120, 32);
        layout->addWidget(themeBtn);
        QObject::connect(themeBtn, &Button::clicked, []() {
            FluentElement::setTheme(FluentElement::currentTheme() == FluentElement::Light 
                                    ? FluentElement::Dark 
                                    : FluentElement::Light);
        });

        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    SplitButtonTestWindow* window = nullptr;
};

TEST_F(SplitButtonTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->show();
    qApp->exec();
}
