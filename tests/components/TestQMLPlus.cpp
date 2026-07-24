#include <gtest/gtest.h>
#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include "components/foundation/QMLPlus.h"
#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"

using namespace fluent;
using namespace fluent::basicinput;
using namespace fluent::textfields;

// =============================================================================
// 1. Mock ViewModel (用于测试 Property Binding)
// =============================================================================
class QMLPlusViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString statusText READ statusText WRITE setStatusText NOTIFY statusTextChanged)
    Q_PROPERTY(bool isOnline READ isOnline WRITE setIsOnline NOTIFY isOnlineChanged)
public:
    QString statusText() const { return m_statusText; }
    void setStatusText(const QString& t) { if(m_statusText != t) { m_statusText = t; emit statusTextChanged(); } }

    bool isOnline() const { return m_isOnline; }
    void setIsOnline(bool o) { if(m_isOnline != o) { m_isOnline = o; emit isOnlineChanged(); } }

signals:
    void statusTextChanged();
    void isOnlineChanged();

private:
    QString m_statusText = "System Ready";
    bool m_isOnline = true;
};

// =============================================================================
// 2. Mock Component (用于测试 Mixin 能力)
// =============================================================================
class QMLPlusBox : public QWidget, public QMLPlus {
    Q_OBJECT
public:
    explicit QMLPlusBox(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_StyledBackground);
        setFixedSize(100, 100);
        updateStyle();
    }

    void updateStyle() {
        QString color = state() == "highlight" ? "#e74c3c" : "#3498db";
        setStyleSheet(QString("background-color: %1; border-radius: 8px;").arg(color));
    }

    // 重写 setState 增加自定义逻辑
    void setState(const QString& name) {
        QMLPlus::setState(name);
        updateStyle();
    }
};

// =============================================================================
// 3. Test Fixture
// =============================================================================
class QMLPlusTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new QWidget();
        window->setFixedSize(600, 500);
        window->setWindowTitle("QMLPlus Core Capabilities Test");
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        vm = new QMLPlusViewModel();
    }

    void TearDown() override {
        delete window;
        delete vm;
    }

    QWidget* window;
    AnchorLayout* layout;
    QMLPlusViewModel* vm;
};

TEST_F(QMLPlusTest, Contract_PropertyBindingSynchronizesOrdinaryQtProperty) {
    QMLPlusBox box;

    box.bind("windowTitle", vm, "statusText");
    EXPECT_EQ(box.windowTitle(), QStringLiteral("System Ready"));

    vm->setStatusText(QStringLiteral("Connected"));
    EXPECT_EQ(box.windowTitle(), QStringLiteral("Connected"));
}

TEST_F(QMLPlusTest, Contract_InvalidStateDoesNotBecomeCurrentState) {
    QMLPlusBox box;

    box.setState(QStringLiteral("missing"));

    EXPECT_TRUE(box.state().isEmpty());
}

TEST_F(QMLPlusTest, Contract_StateTransitionRestoresPropertiesAbsentFromNextState) {
    QMLPlusBox box;
    box.setMinimumWidth(10);
    box.setMaximumWidth(1000);

    QMLState expanded;
    expanded.name = QStringLiteral("expanded");
    expanded.changes = {
        {&box, QByteArrayLiteral("minimumWidth"), 80}
    };
    box.addState(expanded);

    QMLState constrained;
    constrained.name = QStringLiteral("constrained");
    constrained.changes = {
        {&box, QByteArrayLiteral("maximumWidth"), 240}
    };
    box.addState(constrained);

    box.setState(QStringLiteral("expanded"));
    ASSERT_EQ(box.minimumWidth(), 80);

    box.setState(QStringLiteral("constrained"));
    EXPECT_EQ(box.minimumWidth(), 10);
    EXPECT_EQ(box.maximumWidth(), 240);
}

TEST_F(QMLPlusTest, Contract_DestroyedStateTargetsAreRemovedFromDefaultStorage) {
    QMLPlusBox box;
    auto* target = new QObject();
    target->setProperty("phase", QStringLiteral("idle"));

    QMLState active;
    active.name = QStringLiteral("active");
    active.changes = {
        {target, QByteArrayLiteral("phase"), QStringLiteral("running")}
    };
    box.addState(active);
    box.setState(QStringLiteral("active"));
    ASSERT_EQ(target->property("phase").toString(), QStringLiteral("running"));

    delete target;
    box.setState(QString());

    EXPECT_TRUE(box.state().isEmpty());
}

// =============================================================================
// 4. Test Case
// =============================================================================
TEST_F(QMLPlusTest, CoreCapabilitiesVisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;

    // --- A. 测试 Anchors (锚点布局) ---
    QMLPlusBox* box1 = new QMLPlusBox(window);
    box1->anchors()->horizontalCenter = {window, Edge::HCenter, 0};
    box1->anchors()->top = {window, Edge::Top, 50};
    layout->addWidget(box1);

    Label* labelAnchors = new Label("Capability 1: Anchors (Centered)", window);
    labelAnchors->anchors()->top = {box1, Edge::Bottom, 10};
    labelAnchors->anchors()->horizontalCenter = {box1, Edge::HCenter, 0};
    layout->addWidget(labelAnchors);

    // --- B. 测试 States (状态机) ---
    Button* btnToggleState = new Button("Toggle Box State", window);
    btnToggleState->setFixedSize(150, 32);
    btnToggleState->anchors()->top = {labelAnchors, Edge::Bottom, 40};
    btnToggleState->anchors()->horizontalCenter = {window, Edge::HCenter, 0};
    layout->addWidget(btnToggleState);

    // 定义状态：highlight 状态下 box 变大
    QMLState highlightState;
    highlightState.name = "highlight";
    highlightState.changes = {
        { box1, "minimumSize", QSize(150, 150) }
    };
    box1->addState(highlightState);

    QObject::connect(btnToggleState, &Button::clicked, [box1]() {
        box1->setState(box1->state() == "" ? "highlight" : "");
    });

    Label* labelStates = new Label("Capability 2: States (Size & Color Change)", window);
    labelStates->anchors()->top = {btnToggleState, Edge::Bottom, 10};
    labelStates->anchors()->horizontalCenter = {btnToggleState, Edge::HCenter, 0};
    layout->addWidget(labelStates);

    // --- C. 测试 Property Binding (属性绑定) ---
    Label* bindLabel = new Label("Waiting for Binding...", window);
    bindLabel->setStyleSheet("font-weight: bold; color: #2ecc71;");
    bindLabel->anchors()->bottom = {window, Edge::Bottom, -100};
    bindLabel->anchors()->horizontalCenter = {window, Edge::HCenter, 0};
    layout->addWidget(bindLabel);

    // 绑定 Label 的 text 属性到 ViewModel 的 statusText
    bindLabel->bind("text", vm, "statusText");

    Button* btnUpdateVM = new Button("Update ViewModel", window);
    btnUpdateVM->setFixedSize(150, 32);
    btnUpdateVM->anchors()->top = {bindLabel, Edge::Bottom, 10};
    btnUpdateVM->anchors()->horizontalCenter = {window, Edge::HCenter, 0};
    layout->addWidget(btnUpdateVM);

    QObject::connect(btnUpdateVM, &Button::clicked, [this]() {
        static int count = 0;
        vm->setStatusText(QString("Update Count: %1").arg(++count));
    });

    Label* labelBinding = new Label("Capability 3: Property Binding (One-Way)", window);
    labelBinding->anchors()->top = {btnUpdateVM, Edge::Bottom, 10};
    labelBinding->anchors()->horizontalCenter = {btnUpdateVM, Edge::HCenter, 0};
    layout->addWidget(labelBinding);

    window->show();
    
    // 验证初始绑定
    EXPECT_EQ(bindLabel->text(), "System Ready");

    qApp->exec();
}

#include "TestQMLPlus.moc"
