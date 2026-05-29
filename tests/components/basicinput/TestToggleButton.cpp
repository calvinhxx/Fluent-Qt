#include <gtest/gtest.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include "components/basicinput/ToggleButton.h"
#include "components/foundation/FluentElement.h"

using namespace fluent;
using namespace fluent::basicinput;

class ToggleButtonTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class ToggleButtonTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new ToggleButtonTestWindow();
        window->setFixedSize(600, 600);
        window->setWindowTitle("Fluent ToggleButton Visual Test");

        auto* layout = new QVBoxLayout(window);
        layout->setContentsMargins(40, 40, 40, 40);
        layout->setSpacing(20);

        // 1. Basic ToggleButton
        layout->addWidget(new QLabel("1. Basic ToggleButton:", window));
        auto* hLayout1 = new QHBoxLayout();
        auto* toggle1 = new ToggleButton("ToggleButton", window);
        auto* label1 = new QLabel("Output: Off", window);
        
        QObject::connect(toggle1, &ToggleButton::toggled, [label1](bool checked) {
            label1->setText(QString("Output: %1").arg(checked ? "On" : "Off"));
        });

        hLayout1->addWidget(toggle1);
        hLayout1->addWidget(label1);
        hLayout1->addStretch();
        layout->addLayout(hLayout1);

        // 2. Disabled ToggleButton
        layout->addWidget(new QLabel("2. Disabled ToggleButton:", window));
        auto* hLayout2 = new QHBoxLayout();
        auto* toggle2 = new ToggleButton("Disabled Off", window);
        toggle2->setEnabled(false);
        auto* toggle3 = new ToggleButton("Disabled On", window);
        toggle3->setChecked(true);
        toggle3->setEnabled(false);
        hLayout2->addWidget(toggle2);
        hLayout2->addWidget(toggle3);
        hLayout2->addStretch();
        layout->addLayout(hLayout2);

        // 3. Different Sizes
        layout->addWidget(new QLabel("3. Different Sizes:", window));
        auto* hLayout3 = new QHBoxLayout();
        auto* small = new ToggleButton("Small", window);
        small->setFluentSize(Button::Small);
        auto* normal = new ToggleButton("Standard", window);
        normal->setFluentSize(Button::StandardSize);
        auto* large = new ToggleButton("Large", window);
        large->setFluentSize(Button::Large);
        hLayout3->addWidget(small);
        hLayout3->addWidget(normal);
        hLayout3->addWidget(large);
        hLayout3->addStretch();
        layout->addLayout(hLayout3);

        // 4. ThreeState ToggleButton
        layout->addWidget(new QLabel("4. ThreeState ToggleButton (Unchecked -> Checked -> Indeterminate):", window));
        auto* hLayout4 = new QHBoxLayout();
        auto* toggle4 = new ToggleButton("ThreeState", window);
        toggle4->setThreeState(true);
        auto* label4 = new QLabel("State: Unchecked", window);
        
        QObject::connect(toggle4, &ToggleButton::checkStateChanged, [label4](Qt::CheckState state) {
            QString stateStr = "Unchecked";
            if (state == Qt::Checked) stateStr = "Checked";
            else if (state == Qt::PartiallyChecked) stateStr = "Indeterminate";
            label4->setText(QString("State: %1").arg(stateStr));
        });

        hLayout4->addWidget(toggle4);
        hLayout4->addWidget(label4);
        hLayout4->addStretch();
        layout->addLayout(hLayout4);

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

    ToggleButtonTestWindow* window = nullptr;
};

TEST_F(ToggleButtonTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->show();
    qApp->exec();
}
