#include <gtest/gtest.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include "view/basicinput/CheckBox.h"
#include "view/basicinput/Button.h"
#include "view/foundation/FluentElement.h"
#include "design/Typography.h"

using namespace view;
using namespace view::basicinput;

class CheckBoxTestWindow : public QWidget, public FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class CheckBoxTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new CheckBoxTestWindow();
        window->setFixedSize(600, 750);
        window->setWindowTitle("Fluent CheckBox Visual Test");

        auto* layout = new QVBoxLayout(window);
        layout->setContentsMargins(40, 40, 40, 40);
        layout->setSpacing(10);

        // 1. Basic 2-state CheckBox
        layout->addWidget(new QLabel("1. A 2-state CheckBox:", window));
        auto* cb1 = new CheckBox("Two-state CheckBox", window);
        auto* label1 = new QLabel("Output: Unchecked", window);
        QObject::connect(cb1, &CheckBox::stateChanged, [label1](int state) {
            label1->setText(QString("Output: %1").arg(state == Qt::Checked ? "Checked" : "Unchecked"));
        });
        layout->addWidget(cb1);
        layout->addWidget(label1);

        // 2. 3-state CheckBox
        layout->addWidget(new QLabel("2. A 3-state CheckBox:", window));
        auto* cb2 = new CheckBox("Three-state CheckBox", window);
        cb2->setTristate(true);
        auto* label2 = new QLabel("Output: Unchecked", window);
        QObject::connect(cb2, &CheckBox::stateChanged, [label2](int state) {
            QString s = "Unchecked";
            if (state == Qt::Checked) s = "Checked";
            else if (state == Qt::PartiallyChecked) s = "Indeterminate";
            label2->setText(QString("Output: %1").arg(s));
        });
        layout->addWidget(cb2);
        layout->addWidget(label2);

        // 3. Select All scenario
        layout->addWidget(new QLabel("3. Using a 3-state CheckBox (Select All):", window));
        auto* selectAll = new CheckBox("Select all", window);
        selectAll->setTristate(true);
        layout->addWidget(selectAll); // 关键修复：添加到布局中
        
        auto* subLayout = new QVBoxLayout();
        subLayout->setContentsMargins(40, 0, 0, 0); // 增加缩进，使其看起来是子项
        auto* opt1 = new CheckBox("Option 1", window);
        auto* opt2 = new CheckBox("Option 2", window);
        auto* opt3 = new CheckBox("Option 3", window);
        subLayout->addWidget(opt1);
        subLayout->addWidget(opt2);
        subLayout->addWidget(opt3);
        layout->addLayout(subLayout);

        auto updateSelectAll = [selectAll, opt1, opt2, opt3]() {
            int checkedCount = 0;
            if (opt1->isChecked()) checkedCount++;
            if (opt2->isChecked()) checkedCount++;
            if (opt3->isChecked()) checkedCount++;

            if (checkedCount == 0) selectAll->setCheckState(Qt::Unchecked);
            else if (checkedCount == 3) selectAll->setCheckState(Qt::Checked);
            else selectAll->setCheckState(Qt::PartiallyChecked);
        };

        QObject::connect(selectAll, &CheckBox::clicked, [selectAll, opt1, opt2, opt3]() {
            Qt::CheckState target = (selectAll->checkState() == Qt::Checked) ? Qt::Checked : Qt::Unchecked;
            opt1->setChecked(target == Qt::Checked);
            opt2->setChecked(target == Qt::Checked);
            opt3->setChecked(target == Qt::Checked);
        });

        QObject::connect(opt1, &CheckBox::clicked, updateSelectAll);
        QObject::connect(opt2, &CheckBox::clicked, updateSelectAll);
        QObject::connect(opt3, &CheckBox::clicked, updateSelectAll);

        // 4. Disabled states
        layout->addWidget(new QLabel("4. Disabled states:", window));
        auto* hLayout = new QHBoxLayout();
        auto* d1 = new CheckBox("Disabled Off", window);
        d1->setEnabled(false);
        auto* d2 = new CheckBox("Disabled On", window);
        d2->setChecked(true);
        d2->setEnabled(false);
        hLayout->addWidget(d1);
        hLayout->addWidget(d2);
        hLayout->addStretch();
        layout->addLayout(hLayout);

        // 5. Customizable properties
        layout->addWidget(new QLabel("5. Customizable properties:", window));
        
        // 5.1 Larger box size
        auto* largeBox = new CheckBox("Larger box (24px)", window);
        largeBox->setBoxSize(24);
        largeBox->setFixedSize(200, 32);
        layout->addWidget(largeBox);
        
        // 5.2 Custom margins and gaps
        auto* customSpacing = new CheckBox("Custom spacing (margin: 12px, gap: 10px)", window);
        customSpacing->setBoxMargin(12);
        customSpacing->setTextGap(10);
        layout->addWidget(customSpacing);
        
        // 5.3 Compact style (smaller box, tighter spacing)
        auto* compact = new CheckBox("Compact style (box: 16px, margin: 4px, gap: 6px)", window);
        compact->setBoxSize(16);
        compact->setBoxMargin(4);
        compact->setTextGap(6);
        layout->addWidget(compact);

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

    CheckBoxTestWindow* window = nullptr;
};

TEST_F(CheckBoxTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->show();
    qApp->exec();
}
