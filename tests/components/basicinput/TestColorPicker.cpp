#include <gtest/gtest.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QLabel>
#include "components/basicinput/ColorPicker.h"
#include "components/basicinput/Button.h"
#include "components/foundation/FluentElement.h"

using namespace fluent;
using namespace fluent::basicinput;

class ColorPickerTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class ColorPickerTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new ColorPickerTestWindow();
        window->setFixedSize(520, 620);
        window->setWindowTitle("Fluent ColorPicker Visual Test");

        auto* root = new QVBoxLayout(window);
        root->setContentsMargins(24, 24, 24, 24);
        root->setSpacing(16);

        auto* title = new QLabel("ColorPicker", window);
        title->setFont(window->themeFont("Title").toQFont());
        root->addWidget(title);

        auto* picker = new ColorPicker(window);
        picker->setAlphaEnabled(true);  // UT 默认开启 Alpha 通道
        root->addWidget(picker, 1);

        auto* status = new QLabel("Color: #FFFFFFFF", window);
        status->setFont(window->themeFont("Body").toQFont());
        root->addWidget(status);

        QObject::connect(picker, &ColorPicker::colorChanged, [status](const QColor& c) {
            status->setText(QString("Color: #%1%2%3%4")
                                .arg(c.red(), 2, 16, QLatin1Char('0'))
                                .arg(c.green(), 2, 16, QLatin1Char('0'))
                                .arg(c.blue(), 2, 16, QLatin1Char('0'))
                                .arg(c.alpha(), 2, 16, QLatin1Char('0')).toUpper());
        });

        auto* themeBtn = new Button("Switch Theme", window);
        themeBtn->setFixedSize(120, 32);
        root->addWidget(themeBtn, 0, Qt::AlignLeft);
        QObject::connect(themeBtn, &Button::clicked, []() {
            fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                                        ? fluent::FluentElement::Dark
                                        : fluent::FluentElement::Light);
        });

        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    ColorPickerTestWindow* window = nullptr;
};

TEST_F(ColorPickerTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->show();
    qApp->exec();
}

