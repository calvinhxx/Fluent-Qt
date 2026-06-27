#include <gtest/gtest.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include "components/basicinput/CheckBox.h"
#include "components/basicinput/Button.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "design/Typography.h"

using namespace fluent;
using namespace fluent::basicinput;

class CheckBoxTestWindow : public QWidget, public fluent::FluentElement {
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
            fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light 
                                    ? fluent::FluentElement::Dark 
                                    : fluent::FluentElement::Light);
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

// ── Design-language compatibility ─────────────────────────────────────────────
// Renders a CheckBox under every brand design language × theme × check state and
// asserts it paints without crashing and visibly differs from the empty state.
// zh_CN: 在每种品牌设计语言 × 主题 × 选中态下渲染 CheckBox，断言其能正常绘制且与空态有可见差异。

class CheckBoxDesignLanguageTest : public ::testing::Test {
protected:
    // Design language + theme are GLOBAL singletons; restore the defaults after every case so other
    // suites in this executable are unaffected. zh_CN: 设计语言与主题是全局单例;每个用例后恢复默认值,
    // 避免影响本可执行文件中的其它测试套件。
    void TearDown() override {
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Grab a freshly-painted snapshot of a CheckBox in the given language/theme/state.
    // zh_CN: 在给定语言/主题/状态下抓取 CheckBox 的即时渲染快照。
    static QImage grabCheckBox(FluentElement::DesignLanguage lang,
                               FluentElement::Theme theme,
                               Qt::CheckState state) {
        fluent::ThemeRegistry::instance().setDesignLanguage(lang);
        fluent::FluentElement::setTheme(theme);

        CheckBox cb("Sample");
        cb.setTristate(true);
        cb.setCheckState(state);
        cb.resize(cb.sizeHint().expandedTo(QSize(200, 32)));
        return cb.grab().toImage();
    }
};

TEST_F(CheckBoxDesignLanguageTest, RendersAcrossLanguagesThemesAndStates) {
    const FluentElement::DesignLanguage langs[] = {
        FluentElement::DesignFluent,
        FluentElement::DesignMaterial,
        FluentElement::DesignCupertino,
    };
    const FluentElement::Theme themes[] = {
        FluentElement::Light,
        FluentElement::Dark,
    };
    const Qt::CheckState states[] = {
        Qt::Unchecked,
        Qt::Checked,
        Qt::PartiallyChecked,
    };

    for (auto lang : langs) {
        for (auto theme : themes) {
            // Baseline unchecked snapshot for this lang/theme (regression-lock reference).
            // zh_CN: 该语言/主题下的未选中基线快照(回归对照参照)。
            const QImage unchecked = grabCheckBox(lang, theme, Qt::Unchecked);
            ASSERT_FALSE(unchecked.isNull())
                << "lang=" << lang << " theme=" << theme << " unchecked grab was null";

            for (auto state : states) {
                const QImage img = grabCheckBox(lang, theme, state);

                // Renders without crashing, produces a valid image of the expected size.
                // zh_CN: 正常绘制、产出有效图像且尺寸符合预期。
                ASSERT_FALSE(img.isNull())
                    << "lang=" << lang << " theme=" << theme << " state=" << state;
                EXPECT_EQ(img.size(), unchecked.size())
                    << "lang=" << lang << " theme=" << theme << " state=" << state;

                if (state != Qt::Unchecked) {
                    // Content was painted: at least one pixel differs from the top-left background.
                    // zh_CN: 已绘制内容:至少有一个像素不同于左上角背景。
                    const QRgb bg = img.pixel(0, 0);
                    bool paintedSomething = false;
                    for (int y = 0; y < img.height() && !paintedSomething; ++y) {
                        for (int x = 0; x < img.width(); ++x) {
                            if (img.pixel(x, y) != bg) { paintedSomething = true; break; }
                        }
                    }
                    EXPECT_TRUE(paintedSomething)
                        << "no content painted for lang=" << lang
                        << " theme=" << theme << " state=" << state;

                    // Regression lock: a checked/indeterminate box differs from the unchecked one.
                    // zh_CN: 回归锁:选中/半选方框与未选中方框存在差异。
                    EXPECT_NE(img, unchecked)
                        << "state=" << state << " image matched unchecked for lang="
                        << lang << " theme=" << theme;
                }
            }
        }
    }
}
