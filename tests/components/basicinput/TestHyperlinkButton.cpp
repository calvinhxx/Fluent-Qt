#include <gtest/gtest.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QLabel>
#include "components/basicinput/HyperlinkButton.h"
#include "components/foundation/FluentElement.h"

using namespace fluent;
using namespace fluent::basicinput;

class HyperlinkButtonTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class HyperlinkButtonTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new HyperlinkButtonTestWindow();
        window->setFixedSize(600, 800);
        window->setWindowTitle("Fluent HyperlinkButton Visual Test");

        auto* layout = new QVBoxLayout(window);
        layout->setContentsMargins(40, 40, 40, 40);
        layout->setSpacing(20);

        // 1. Basic HyperlinkButton
        layout->addWidget(new QLabel("1. Basic HyperlinkButton:", window));
        auto* btn1 = new HyperlinkButton("Microsoft home page", window);
        btn1->setUrl(QUrl("https://www.microsoft.com"));
        layout->addWidget(btn1);

        // 2. HyperlinkButton with different sizes
        layout->addWidget(new QLabel("2. Different Sizes:", window));
        auto* h1 = new QHBoxLayout();
        auto* small = new HyperlinkButton("Small", window);
        small->setFluentSize(Button::Small);
        auto* normal = new HyperlinkButton("Standard", window);
        normal->setFluentSize(Button::StandardSize);
        auto* large = new HyperlinkButton("Large", window);
        large->setFluentSize(Button::Large);
        h1->addWidget(small);
        h1->addWidget(normal);
        h1->addWidget(large);
        h1->addStretch();
        layout->addLayout(h1);

        // 3. HyperlinkButton with click handler
        layout->addWidget(new QLabel("3. Click to navigate (Check Console):", window));
        auto* navBtn = new HyperlinkButton("Go to GitHub", QUrl("https://github.com"), window);
        layout->addWidget(navBtn);

        // 4. Disabled state
        layout->addWidget(new QLabel("4. Disabled state (No underline by default):", window));
        auto* disabledBtn = new HyperlinkButton("Disabled link", window);
        disabledBtn->setEnabled(false);
        layout->addWidget(disabledBtn);

        // 5. With underline (Explicitly enabled)
        layout->addWidget(new QLabel("5. With underline on hover (Explicitly enabled):", window));
        auto* withUnderline = new HyperlinkButton("Underline on hover", window);
        withUnderline->setShowUnderline(true); 
        layout->addWidget(withUnderline);

        // 6. Default behavior (No underline)
        layout->addWidget(new QLabel("6. Default behavior (No underline on hover):", window));
        auto* defaultBtn = new HyperlinkButton("Default no underline", window);
        layout->addWidget(defaultBtn);

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

    HyperlinkButtonTestWindow* window = nullptr;
};

TEST_F(HyperlinkButtonTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->show();
    qApp->exec();
}
