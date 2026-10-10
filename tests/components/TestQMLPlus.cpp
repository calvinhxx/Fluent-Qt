#include <gtest/gtest.h>
#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <QVariantAnimation>
#include "components/foundation/MotionPolicy.h"
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
    void setStatusText(const QString& t)
    {
        if (m_statusText != t) {
            m_statusText = t;
            emit statusTextChanged();
        }
    }

    bool isOnline() const { return m_isOnline; }
    void setIsOnline(bool o)
    {
        if (m_isOnline != o) {
            m_isOnline = o;
            emit isOnlineChanged();
        }
    }

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
class QMLPlusBox : public QWidget, public fluent::QMLPlus {
    Q_OBJECT
public:
    explicit QMLPlusBox(QWidget* parent = nullptr) : QWidget(parent)
    {
        setAttribute(Qt::WA_StyledBackground);
        setFixedSize(100, 100);
        updateStyle();
    }

    void updateStyle()
    {
        QString color = state() == "highlight" ? "#e74c3c" : "#3498db";
        setStyleSheet(QString("background-color: %1; border-radius: 8px;").arg(color));
    }

    // 重写 setState 增加自定义逻辑
    void setState(const QString& name)
    {
        QMLPlus::setState(name);
        updateStyle();
    }
};

class QMLPlusStateTarget : public QObject {
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
public:
    int value() const { return m_value; }
    void setValue(int value)
    {
        if (m_value == value)
            return;
        m_value = value;
        emit valueChanged(value);
    }

signals:
    void valueChanged(int value);

private:
    int m_value = 0;
};

class QMLPlusValueWidget : public QWidget, public fluent::QMLPlus {
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
public:
    explicit QMLPlusValueWidget(QWidget* parent = nullptr) : QWidget(parent) {}

    int value() const { return m_value; }
    void setValue(int value)
    {
        if (m_value == value)
            return;
        m_value = value;
        emit valueChanged(value);
    }

signals:
    void valueChanged(int value);

private:
    int m_value = 0;
};

class QMLPlusObjectController : public QObject, public fluent::QMLPlus {};

// =============================================================================
// 3. Test Fixture
// =============================================================================
class QMLPlusTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        previousMotionMode = MotionPolicy::instance().mode();
        MotionPolicy::instance().setMode(MotionPolicy::Mode::Full);
        window = new QWidget();
        window->setFixedSize(600, 500);
        window->setWindowTitle("QMLPlus Core Capabilities Test");
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        vm = new QMLPlusViewModel();
    }

    void TearDown() override
    {
        delete window;
        delete vm;
        MotionPolicy::instance().setMode(previousMotionMode);
    }

    QWidget* window;
    AnchorLayout* layout;
    QMLPlusViewModel* vm;
    MotionPolicy::Mode previousMotionMode = MotionPolicy::Mode::Full;
};

TEST_F(QMLPlusTest, Contract_PropertyBindingSynchronizesOrdinaryQtProperty)
{
    QMLPlusBox box;

    box.bind("windowTitle", vm, "statusText");
    EXPECT_EQ(box.windowTitle(), QStringLiteral("System Ready"));

    vm->setStatusText(QStringLiteral("Connected"));
    EXPECT_EQ(box.windowTitle(), QStringLiteral("Connected"));
}

TEST_F(QMLPlusTest, Contract_LegacyBindingSynchronizesWithoutSourceNotify)
{
    QWidget source;
    QMLPlusValueWidget target;
    source.setMinimumWidth(24);
    target.bind("value", &source, "minimumWidth");
    EXPECT_EQ(target.value(), 24);

    source.setMinimumWidth(48);
    EXPECT_EQ(target.value(), 24);
    EXPECT_FALSE(target.bindManaged("value", &source, "minimumWidth").isValid());
    EXPECT_EQ(target.value(), 24);
}

TEST_F(QMLPlusTest, Contract_LegacyTwoWayBindingObservesOnlyAvailableNotifyDirections)
{
    QWidget unobserved;
    QMLPlusValueWidget observed;
    unobserved.setMinimumWidth(12);
    PropertyBinder::bind(&unobserved, "minimumWidth", &observed, "value", PropertyBinder::TwoWay);
    EXPECT_EQ(observed.value(), 12);
    observed.setValue(36);
    EXPECT_EQ(unobserved.minimumWidth(), 36);
    unobserved.setMinimumWidth(48);
    EXPECT_EQ(observed.value(), 36);

    QMLPlusStateTarget source;
    QWidget target;
    source.setValue(10);
    PropertyBinder::bind(&source, "value", &target, "minimumWidth", PropertyBinder::TwoWay);
    EXPECT_EQ(target.minimumWidth(), 10);
    source.setValue(20);
    EXPECT_EQ(target.minimumWidth(), 20);
    target.setMinimumWidth(30);
    EXPECT_EQ(source.value(), 20);
    EXPECT_FALSE(PropertyBinder::bindManaged(&source, "value", &target, "minimumWidth",
                                             PropertyBinder::TwoWay)
                     .isValid());
    source.setValue(40);
    EXPECT_EQ(target.minimumWidth(), 40);
}

TEST_F(QMLPlusTest, Contract_LegacyBindingReplacementPreservesStateOverrides)
{
    QMLPlusStateTarget originalSource;
    QMLPlusValueWidget target;
    originalSource.setValue(12);
    auto original = target.bindManaged("value", &originalSource, "value");
    ASSERT_TRUE(original.isValid());

    QWidget replacementSource;
    replacementSource.setMinimumWidth(24);
    target.bind("value", &replacementSource, "minimumWidth");
    EXPECT_FALSE(original.isValid());
    EXPECT_EQ(target.value(), 24);
    originalSource.setValue(36);
    EXPECT_EQ(target.value(), 24);

    QMLState active;
    active.name = QStringLiteral("active");
    active.changes = {{&target, "value", 100}};
    target.addState(active);
    target.setState(active.name);
    replacementSource.setMinimumWidth(48);
    EXPECT_EQ(target.value(), 100);
    target.setState(QString());
    EXPECT_EQ(target.value(), 48);

    target.bind("value", &originalSource, "value");
    EXPECT_EQ(target.value(), 36);
    originalSource.setValue(60);
    EXPECT_EQ(target.value(), 60);
}

TEST_F(QMLPlusTest, Contract_CrossThreadBindingRejectionPreservesExistingBinding)
{
    QMLPlusStateTarget originalSource;
    QMLPlusValueWidget target;
    originalSource.setValue(12);
    auto original = target.bindManaged("value", &originalSource, "value");
    ASSERT_TRUE(original.isValid());

    QThread thread;
    auto* source = new QMLPlusStateTarget;
    source->setValue(24);
    source->moveToThread(&thread);
    QObject::connect(&thread, &QThread::finished, source, &QObject::deleteLater);
    thread.start();

    target.bind("value", source, "value");
    EXPECT_EQ(target.value(), 12);
    EXPECT_TRUE(original.isValid());
    EXPECT_FALSE(target.bindManaged("value", source, "value").isValid());
    int evaluations = 0;
    auto computed = target.bindComputed("value", {{source, "value"}}, [&] {
        ++evaluations;
        return QVariant(99);
    });
    EXPECT_FALSE(computed.isValid());
    EXPECT_EQ(evaluations, 0);
    originalSource.setValue(36);
    EXPECT_EQ(target.value(), 36);

    thread.quit();
    thread.wait();
}

TEST_F(QMLPlusTest, Contract_ManagedBindingPausesResumesAndUnbinds)
{
    QMLPlusStateTarget source;
    QMLPlusValueWidget target;
    source.setValue(12);
    auto binding = target.bindManaged("value", &source, "value");
    ASSERT_TRUE(binding.isValid());
    EXPECT_EQ(target.value(), 12);

    binding.pause();
    EXPECT_TRUE(binding.isEnabled());
    EXPECT_TRUE(binding.isSuspended());
    source.setValue(37);
    EXPECT_EQ(target.value(), 12);
    binding.resume();
    EXPECT_TRUE(binding.isEnabled());
    EXPECT_FALSE(binding.isSuspended());
    EXPECT_EQ(target.value(), 37);

    binding.unbind();
    EXPECT_FALSE(binding.isValid());
    source.setValue(61);
    EXPECT_EQ(target.value(), 37);
}

TEST_F(QMLPlusTest, Contract_ReplacementInvalidatesPreviousBindingAndRejectsInvalidReplacement)
{
    QMLPlusStateTarget first;
    QMLPlusStateTarget second;
    QMLPlusValueWidget target;
    first.setValue(10);
    second.setValue(20);
    auto original = target.bindManaged("value", &first, "value");
    ASSERT_TRUE(original.isValid());

    auto invalid = target.bindManaged("value", &second, "missing");
    EXPECT_FALSE(invalid.isValid());
    first.setValue(11);
    EXPECT_EQ(target.value(), 11);
    EXPECT_TRUE(original.isValid());

    auto replacement = target.bindManaged("value", &second, "value");
    ASSERT_TRUE(replacement.isValid());
    EXPECT_FALSE(original.isValid());
    EXPECT_EQ(target.value(), 20);
    first.setValue(99);
    EXPECT_EQ(target.value(), 20);
    second.setValue(21);
    EXPECT_EQ(target.value(), 21);
}

TEST_F(QMLPlusTest, Contract_ComputedBindingUsesAllDependenciesAndCoalescesDeferredNotifications)
{
    QMLPlusStateTarget first;
    QMLPlusStateTarget second;
    QMLPlusValueWidget target;
    first.setValue(20);
    second.setValue(40);
    int evaluations = 0;
    BindingOptions options;
    options.deferred = true;
    auto binding = target.bindComputed(
        "value", {{&first, "value"}, {&second, "value"}},
        [&] {
            ++evaluations;
            return QVariant((first.value() + second.value()) / 2);
        },
        options);
    ASSERT_TRUE(binding.isValid());
    EXPECT_EQ(target.value(), 30);
    const int initialEvaluations = evaluations;

    first.setValue(60);
    second.setValue(80);
    first.setValue(100);
    EXPECT_EQ(target.value(), 30);
    ASSERT_TRUE(QTest::qWaitFor([&] { return target.value() == 90; }, 1000));
    EXPECT_EQ(evaluations, initialEvaluations + 1);
}

TEST_F(QMLPlusTest, Contract_ComputedBindingRejectsUnobservableDependenciesBeforeEvaluation)
{
    QMLPlusValueWidget target;
    QWidget dependency;
    int evaluations = 0;
    auto binding = target.bindComputed("value", {{&dependency, "minimumWidth"}}, [&] {
        ++evaluations;
        return QVariant(10);
    });

    EXPECT_FALSE(binding.isValid());
    EXPECT_EQ(evaluations, 0);
    EXPECT_EQ(target.value(), 0);
}

TEST_F(QMLPlusTest, Contract_DisabledComputedBindingEnablesAndRefreshesConstantExpression)
{
    QMLPlusValueWidget target;
    int input = 10;
    int evaluations = 0;
    BindingOptions options;
    options.enabled = false;
    auto binding = target.bindComputed(
        "value", {},
        [&] {
            ++evaluations;
            return QVariant(input);
        },
        options);
    ASSERT_TRUE(binding.isValid());
    EXPECT_FALSE(binding.isEnabled());
    EXPECT_EQ(evaluations, 0);
    binding.setEnabled(true);
    EXPECT_EQ(target.value(), 10);
    input = 20;
    binding.refresh();
    EXPECT_EQ(target.value(), 20);
    binding.pause();
    binding.setEnabled(false);
    input = 30;
    binding.resume();
    binding.refresh();
    EXPECT_EQ(target.value(), 20);
    binding.setEnabled(true);
    EXPECT_EQ(target.value(), 30);
}

TEST_F(QMLPlusTest, Contract_DestroyedBindingEndpointsInvalidateHandlesAndCancelQueuedEvaluation)
{
    auto* source = new QMLPlusStateTarget;
    QMLPlusValueWidget target;
    int evaluations = 0;
    BindingOptions options;
    options.deferred = true;
    auto binding = target.bindComputed(
        "value", {{source, "value"}},
        [&] {
            ++evaluations;
            return QVariant(7);
        },
        options);
    ASSERT_TRUE(binding.isValid());
    source->setValue(1);
    delete source;
    QApplication::processEvents();
    EXPECT_FALSE(binding.isValid());
    EXPECT_EQ(evaluations, 1);

    QMLPlusStateTarget survivingSource;
    auto* temporaryTarget = new QMLPlusValueWidget;
    auto temporaryBinding = temporaryTarget->bindManaged("value", &survivingSource, "value");
    ASSERT_TRUE(temporaryBinding.isValid());
    delete temporaryTarget;
    survivingSource.setValue(9);
    EXPECT_FALSE(temporaryBinding.isValid());
}

TEST_F(QMLPlusTest, Contract_ComputedEvaluatorMayDestroyItsTarget)
{
    QMLPlusStateTarget source;
    auto* target = new QMLPlusValueWidget;
    QPointer<QMLPlusValueWidget> targetGuard(target);
    auto binding = target->bindComputed("value", {{&source, "value"}}, [&] {
        if (source.value() == 1)
            delete target;
        return QVariant(5);
    });
    ASSERT_TRUE(binding.isValid());
    source.setValue(1);
    EXPECT_TRUE(targetGuard.isNull());
    EXPECT_FALSE(binding.isValid());
}

TEST_F(QMLPlusTest, Contract_ReentrantComputedBindingConvergesToLatestSourceValue)
{
    QMLPlusStateTarget source;
    QMLPlusValueWidget target;
    int notifications = 0;
    QObject::connect(&target, &QMLPlusValueWidget::valueChanged, &target, [&](int value) {
        ++notifications;
        if (value == 1)
            source.setValue(2);
    });
    auto binding = target.bindComputed("value", {{&source, "value"}},
                                       [&] { return QVariant(source.value()); });
    ASSERT_TRUE(binding.isValid());

    source.setValue(1);

    EXPECT_EQ(target.value(), 2);
    EXPECT_EQ(source.value(), 2);
    EXPECT_LE(notifications, 3);
}

TEST_F(QMLPlusTest, Contract_StateSuspendsComputedBindingAndRestoresLatestValueRepeatedly)
{
    QMLPlusStateTarget first;
    QMLPlusStateTarget second;
    QMLPlusValueWidget target;
    first.setValue(20);
    second.setValue(40);
    auto binding = target.bindComputed("value", {{&first, "value"}, {&second, "value"}}, [&] {
        return QVariant((first.value() + second.value()) / 2);
    });
    ASSERT_TRUE(binding.isValid());
    QMLState saving;
    saving.name = QStringLiteral("saving");
    saving.changes = {{&target, "value", 100}};
    target.addState(saving);

    for (int i = 0; i < 3; ++i) {
        SCOPED_TRACE(i);
        target.setState(QStringLiteral("saving"));
        EXPECT_TRUE(binding.isSuspended());
        first.setValue(40 + i * 10);
        EXPECT_EQ(target.value(), 100);
        target.setState(QString());
        EXPECT_FALSE(binding.isSuspended());
        EXPECT_EQ(target.value(), (first.value() + second.value()) / 2);
    }
}

TEST_F(QMLPlusTest, Contract_StateOverridesNeverWriteBackThroughTwoWayBinding)
{
    QMLPlusStateTarget source;
    QMLPlusValueWidget target;
    source.setValue(20);
    auto binding = target.bindManaged("value", &source, "value", PropertyBinder::TwoWay);
    ASSERT_TRUE(binding.isValid());
    QSignalSpy sourceSpy(&source, &QMLPlusStateTarget::valueChanged);
    QMLState active;
    active.name = QStringLiteral("active");
    active.changes = {{&target, "value", 100}};
    target.addState(active);

    target.setState(QStringLiteral("active"));
    EXPECT_EQ(target.value(), 100);
    EXPECT_EQ(source.value(), 20);
    EXPECT_EQ(sourceSpy.count(), 0);
    source.setValue(35);
    EXPECT_EQ(target.value(), 100);
    target.setState(QString());
    EXPECT_EQ(target.value(), 35);
    EXPECT_EQ(source.value(), 35);
    EXPECT_EQ(sourceSpy.count(), 1);

    target.setValue(45);
    EXPECT_EQ(source.value(), 45);
}

TEST_F(QMLPlusTest, Contract_DestroyingStateControllerResumesExternalTargetBinding)
{
    QMLPlusStateTarget source;
    QMLPlusStateTarget target;
    source.setValue(20);
    auto binding = PropertyBinder::bindManaged(&source, "value", &target, "value");
    auto* controller = new QMLPlusValueWidget;
    QMLState active;
    active.name = QStringLiteral("active");
    active.changes = {{&target, "value", 100}};
    controller->addState(active);
    controller->setState(QStringLiteral("active"));
    ASSERT_TRUE(binding.isSuspended());
    source.setValue(35);

    delete controller;

    EXPECT_TRUE(binding.isValid());
    EXPECT_FALSE(binding.isSuspended());
    EXPECT_EQ(target.value(), 35);
    source.setValue(45);
    EXPECT_EQ(target.value(), 45);
}

TEST_F(QMLPlusTest, Contract_QObjectStateControllerCoordinatesLegacyBindingAndDestruction)
{
    QMLPlusStateTarget source;
    QMLPlusValueWidget target;
    source.setValue(12);
    PropertyBinder::bind(&source, "value", &target, "value");
    ASSERT_EQ(target.value(), 12);
    auto* controller = new QMLPlusObjectController;
    QMLState active;
    active.name = QStringLiteral("active");
    active.changes = {{&target, "value", 100}};
    controller->addState(active);
    controller->setState(QStringLiteral("active"));
    source.setValue(24);
    EXPECT_EQ(target.value(), 100);
    controller->setState(QString());
    EXPECT_EQ(target.value(), 24);

    controller->setState(QStringLiteral("active"));
    source.setValue(36);
    EXPECT_EQ(target.value(), 100);
    delete controller;
    EXPECT_EQ(target.value(), 36);
    source.setValue(42);
    EXPECT_EQ(target.value(), 42);
}

TEST_F(QMLPlusTest, Contract_ConditionalStatesUseRegistrationPriorityAndInheritance)
{
    QMLPlusValueWidget target;
    vm->setIsOnline(false);
    QMLState base;
    base.name = QStringLiteral("base");
    base.changes = {{&target, "windowTitle", QStringLiteral("Working")}, {&target, "value", 10}};
    target.addState(base);

    QMLState first;
    first.name = QStringLiteral("first");
    first.extend = QStringLiteral("base");
    first.changes = {{&target, "value", 20}};
    first.dependencies = {{vm, "isOnline"}};
    first.when = [this] { return vm->isOnline(); };
    target.addState(first);

    QMLState second;
    second.name = QStringLiteral("second");
    second.changes = {{&target, "value", 30}};
    second.dependencies = {{vm, "isOnline"}};
    second.when = [this] { return vm->isOnline(); };
    target.addState(second);
    vm->setIsOnline(true);
    EXPECT_EQ(target.state(), QStringLiteral("first"));
    EXPECT_EQ(target.value(), 20);
    EXPECT_EQ(target.windowTitle(), QStringLiteral("Working"));

    QSignalSpy replacementStateSpy(target.runtime(), &QMLPlusRuntime::stateChanged);
    first.changes = {{&target, "value", 25}};
    target.addState(first);
    EXPECT_EQ(target.value(), 25);
    EXPECT_EQ(replacementStateSpy.count(), 0);
    target.evaluateStates();
    EXPECT_EQ(target.state(), QStringLiteral("first"));
    EXPECT_EQ(target.value(), 25);
    EXPECT_EQ(replacementStateSpy.count(), 0);
    vm->setIsOnline(false);
    EXPECT_TRUE(target.state().isEmpty());
    EXPECT_EQ(target.value(), 0);
    EXPECT_TRUE(target.windowTitle().isEmpty());
}

TEST_F(QMLPlusTest, Contract_InvalidActiveInheritanceReplacementPreservesStateAndRestoration)
{
    QMLPlusStateTarget source;
    QMLPlusValueWidget target;
    source.setValue(20);
    auto binding = target.bindManaged("value", &source, "value");
    QMLState active;
    active.name = QStringLiteral("active");
    active.changes = {{&target, "value", 100}};
    target.addState(active);
    const QStringList invalidAncestors{QStringLiteral("missing"), QStringLiteral("active")};
    for (const QString& ancestor : invalidAncestors) {
        SCOPED_TRACE(ancestor.toStdString());
        target.setState(QStringLiteral("active"));
        QSignalSpy stateSpy(target.runtime(), &QMLPlusRuntime::stateChanged);
        QMLState invalid = active;
        invalid.extend = ancestor;
        invalid.changes = {{&target, "value", 200}};

        target.addState(invalid);

        EXPECT_EQ(target.state(), QStringLiteral("active"));
        EXPECT_EQ(target.value(), 100);
        EXPECT_TRUE(binding.isSuspended());
        EXPECT_EQ(stateSpy.count(), 0);
        source.setValue(source.value() + 10);
        target.setState(QString());
        EXPECT_TRUE(target.state().isEmpty());
        EXPECT_FALSE(binding.isSuspended());
        EXPECT_EQ(target.value(), source.value());
        target.setState(QStringLiteral("active"));
        EXPECT_EQ(target.value(), 100);
        target.setState(QString());
    }
}

TEST_F(QMLPlusTest, Contract_DestroyedConditionalDependencyIsNotEvaluated)
{
    QMLPlusValueWidget target;
    auto* source = new QMLPlusStateTarget;
    int evaluations = 0;
    QMLState active;
    active.name = QStringLiteral("active");
    active.changes = {{&target, "value", 100}};
    active.dependencies = {{source, "value"}};
    active.when = [&] {
        ++evaluations;
        return source->value() > 0;
    };
    target.addState(active);
    source->setValue(1);
    ASSERT_EQ(target.state(), QStringLiteral("active"));
    delete source;
    const int evaluationsBeforeRefresh = evaluations;

    target.evaluateStates();

    EXPECT_EQ(evaluations, evaluationsBeforeRefresh);
    EXPECT_TRUE(target.state().isEmpty());
    EXPECT_EQ(target.value(), 0);
}

TEST_F(QMLPlusTest, Contract_ParentTeardownDoesNotReenterStateCallbacksAfterSiblingDestruction)
{
    auto* parent = new QWidget;
    auto* earlierSibling = new QLabel(QStringLiteral("Status"), parent);
    QPointer<QLabel> siblingGuard(earlierSibling);
    auto* source = new QMLPlusStateTarget;
    source->setParent(parent);
    source->setValue(1);
    auto* controller = new QMLPlusValueWidget(parent);
    QPointer<QMLPlusValueWidget> controllerGuard(controller);
    QMLState active;
    active.name = QStringLiteral("active");
    active.dependencies = {{source, "value"}};
    active.when = [source] { return source->value() > 0; };
    active.changes = {{controller, "value", 100}};
    controller->addState(active);
    ASSERT_EQ(controller->state(), QStringLiteral("active"));
    int callbacksAfterSiblingDestruction = 0;
    QObject::connect(controller->runtime(), &QMLPlusRuntime::stateChanged, parent,
                     [&](const QString&) {
                         if (siblingGuard.isNull())
                             ++callbacksAfterSiblingDestruction;
                     });

    delete parent;
    QApplication::processEvents();

    EXPECT_TRUE(siblingGuard.isNull());
    EXPECT_TRUE(controllerGuard.isNull());
    EXPECT_EQ(callbacksAfterSiblingDestruction, 0);
}

TEST_F(QMLPlusTest, Contract_DestroyedConditionalDependencyAutomaticallyRestoresSurvivingController)
{
    QMLPlusValueWidget controller;
    auto* source = new QMLPlusStateTarget;
    source->setValue(1);
    int evaluations = 0;
    QMLState active;
    active.name = QStringLiteral("active");
    active.dependencies = {{source, "value"}};
    active.when = [&] {
        ++evaluations;
        return source->value() > 0;
    };
    active.changes = {{&controller, "value", 100}};
    controller.addState(active);
    ASSERT_EQ(controller.state(), QStringLiteral("active"));
    const int evaluationsBeforeDestruction = evaluations;

    delete source;

    EXPECT_EQ(controller.state(), QStringLiteral("active"));
    ASSERT_TRUE(QTest::qWaitFor([&] { return controller.state().isEmpty(); }, 1000));
    EXPECT_EQ(controller.value(), 0);
    EXPECT_EQ(evaluations, evaluationsBeforeDestruction);
}

TEST_F(QMLPlusTest, Contract_ConditionMayDestroyItsHost)
{
    QMLPlusStateTarget source;
    auto* target = new QMLPlusValueWidget;
    QPointer<QMLPlusValueWidget> targetGuard(target);
    QMLState active;
    active.name = QStringLiteral("active");
    active.dependencies = {{&source, "value"}};
    active.when = [&] {
        if (source.value() == 1)
            delete target;
        return true;
    };
    target->addState(active);

    source.setValue(1);

    EXPECT_TRUE(targetGuard.isNull());
}

TEST_F(QMLPlusTest, Contract_ManagedBindingInitialValueIsImmediateWithBehavior)
{
    QMLPlusStateTarget source;
    QMLPlusValueWidget target;
    source.setValue(30);
    PropertyAnimation animation;
    animation.duration = 1000;
    target.setBehavior("value", animation);

    auto binding = target.bindManaged("value", &source, "value");

    ASSERT_TRUE(binding.isValid());
    EXPECT_EQ(target.value(), 30);
    EXPECT_FALSE(target.runtime()->isTransitionRunning());
}

TEST_F(QMLPlusTest, Contract_BehaviorRetargetsFromCurrentValueAndRespectsMotionPolicy)
{
    QMLPlusValueWidget target;
    PropertyAnimation animation;
    animation.duration = 1000;
    animation.easing = QEasingCurve::Linear;
    target.setBehavior("value", animation);
    target.writeProperty("value", 100);
    auto animations = target.findChildren<QVariantAnimation*>();
    ASSERT_FALSE(animations.isEmpty());
    ASSERT_EQ(animations.last()->state(), QAbstractAnimation::Running);
    animations.last()->setCurrentTime(400);
    ASSERT_GT(target.value(), 0);
    ASSERT_LT(target.value(), 100);
    const int intermediate = target.value();

    target.writeProperty("value", 200);
    EXPECT_EQ(target.value(), intermediate);
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Disabled);
    EXPECT_EQ(target.value(), 200);
    EXPECT_FALSE(target.runtime()->isTransitionRunning());
    target.writeProperty("value", 250);
    EXPECT_EQ(target.value(), 250);
    EXPECT_FALSE(target.runtime()->isTransitionRunning());

    MotionPolicy::instance().setMode(MotionPolicy::Mode::Reduced);
    target.writeProperty("value", 300);
    animations = target.findChildren<QVariantAnimation*>();
    ASSERT_FALSE(animations.isEmpty());
    EXPECT_LE(animations.last()->duration(), 50);
    ASSERT_TRUE(QTest::qWaitFor([&] { return target.value() == 300; }, 1000));
    EXPECT_FALSE(target.runtime()->isTransitionRunning());
}

TEST_F(QMLPlusTest, Contract_AnimatedTwoWayBindingDoesNotWriteIntermediateValuesBack)
{
    QMLPlusStateTarget source;
    QMLPlusValueWidget target;
    PropertyAnimation animation;
    animation.duration = 1000;
    animation.easing = QEasingCurve::Linear;
    target.setBehavior("value", animation);
    auto binding = target.bindManaged("value", &source, "value", PropertyBinder::TwoWay);
    ASSERT_TRUE(binding.isValid());
    QSignalSpy sourceSpy(&source, &QMLPlusStateTarget::valueChanged);
    source.setValue(100);
    auto animations = target.findChildren<QVariantAnimation*>();
    ASSERT_FALSE(animations.isEmpty());
    ASSERT_EQ(animations.last()->state(), QAbstractAnimation::Running);
    animations.last()->setCurrentTime(400);

    EXPECT_GT(target.value(), 0);
    EXPECT_LT(target.value(), 100);
    EXPECT_EQ(source.value(), 100);
    EXPECT_EQ(sourceSpy.count(), 1);
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Disabled);
    EXPECT_EQ(target.value(), 100);
    EXPECT_EQ(sourceSpy.count(), 1);
}

TEST_F(QMLPlusTest, Contract_StateTransitionEmitsFinishedAndRestoresAfterInterruption)
{
    QMLPlusValueWidget target;
    QMLState active;
    active.name = QStringLiteral("active");
    active.changes = {{&target, "value", 100}};
    target.addState(active);
    StateTransition transition;
    transition.properties = {QByteArrayLiteral("value")};
    transition.animation.duration = 1000;
    transition.animation.easing = QEasingCurve::Linear;
    target.addTransition(transition);
    QSignalSpy finishedSpy(target.runtime(), &QMLPlusRuntime::transitionFinished);

    target.setState(QStringLiteral("active"));
    ASSERT_TRUE(target.runtime()->isTransitionRunning());
    auto animations = target.findChildren<QVariantAnimation*>();
    ASSERT_FALSE(animations.isEmpty());
    animations.last()->setCurrentTime(400);
    EXPECT_GT(target.value(), 0);
    EXPECT_LT(target.value(), 100);
    target.setState(QString());
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Disabled);

    EXPECT_EQ(target.value(), 0);
    EXPECT_TRUE(target.state().isEmpty());
    EXPECT_FALSE(target.runtime()->isTransitionRunning());
    ASSERT_GE(finishedSpy.count(), 1);
    EXPECT_TRUE(finishedSpy.last().at(0).toString().isEmpty());
}

TEST_F(QMLPlusTest, Contract_ReplacementCancellationListenerMayDestroyBindingTarget)
{
    QMLPlusStateTarget source;
    QMLPlusStateTarget replacementSource;
    source.setValue(20);
    replacementSource.setValue(40);
    auto* target = new QMLPlusValueWidget;
    QPointer<QMLPlusValueWidget> targetGuard(target);
    auto original = target->bindManaged("value", &source, "value");
    QMLState active;
    active.name = QStringLiteral("active");
    active.changes = {{target, "value", 100}};
    target->addState(active);
    StateTransition transition;
    transition.properties = {QByteArrayLiteral("value")};
    transition.animation.duration = 1000;
    target->addTransition(transition);
    target->setState(QStringLiteral("active"));
    auto animations = target->findChildren<QVariantAnimation*>();
    ASSERT_FALSE(animations.isEmpty());
    animations.last()->setCurrentTime(1000);
    ASSERT_EQ(target->value(), 100);
    target->setState(QString());
    ASSERT_TRUE(target->runtime()->isTransitionRunning());
    int cancellationNotifications = 0;
    QObject::connect(target->runtime(), &QMLPlusRuntime::transitionRunningChanged, &source,
                     [&](bool running) {
                         if (!running) {
                             ++cancellationNotifications;
                             delete target;
                         }
                     });

    auto replacement = target->bindManaged("value", &replacementSource, "value");

    EXPECT_TRUE(targetGuard.isNull());
    EXPECT_FALSE(original.isValid());
    EXPECT_FALSE(replacement.isValid());
    EXPECT_EQ(cancellationNotifications, 1);
}

TEST_F(QMLPlusTest, Contract_ReentrantReplacementKeepsOnlyLatestBindingActive)
{
    QMLPlusStateTarget source;
    QMLPlusStateTarget outerSource;
    QMLPlusStateTarget nestedSource;
    QMLPlusValueWidget target;
    source.setValue(20);
    outerSource.setValue(40);
    nestedSource.setValue(60);
    auto original = target.bindManaged("value", &source, "value");
    QMLState active;
    active.name = QStringLiteral("active");
    active.changes = {{&target, "value", 100}};
    target.addState(active);
    StateTransition transition;
    transition.properties = {QByteArrayLiteral("value")};
    transition.animation.duration = 1000;
    target.addTransition(transition);
    target.setState(QStringLiteral("active"));
    auto animations = target.findChildren<QVariantAnimation*>();
    ASSERT_FALSE(animations.isEmpty());
    animations.last()->setCurrentTime(1000);
    target.setState(QString());
    ASSERT_TRUE(target.runtime()->isTransitionRunning());
    BindingHandle nested;
    bool registeredNested = false;
    QObject::connect(target.runtime(), &QMLPlusRuntime::transitionRunningChanged, &target,
                     [&](bool running) {
                         if (!running && !registeredNested) {
                             registeredNested = true;
                             nested = target.bindManaged("value", &nestedSource, "value");
                         }
                     });

    auto outer = target.bindManaged("value", &outerSource, "value");

    ASSERT_TRUE(registeredNested);
    EXPECT_FALSE(original.isValid());
    EXPECT_FALSE(outer.isValid());
    EXPECT_TRUE(nested.isValid());
    EXPECT_EQ(target.value(), 60);
    outerSource.setValue(80);
    source.setValue(90);
    EXPECT_EQ(target.value(), 60);
    nestedSource.setValue(70);
    EXPECT_EQ(target.value(), 70);
}

TEST_F(QMLPlusTest, Contract_InvalidStateDoesNotBecomeCurrentState)
{
    QMLPlusBox box;

    box.setState(QStringLiteral("missing"));

    EXPECT_TRUE(box.state().isEmpty());
}

TEST_F(QMLPlusTest, Contract_StateTransitionRestoresPropertiesAbsentFromNextState)
{
    QMLPlusBox box;
    box.setMinimumWidth(10);
    box.setMaximumWidth(1000);

    QMLState expanded;
    expanded.name = QStringLiteral("expanded");
    expanded.changes = {{&box, QByteArrayLiteral("minimumWidth"), 80}};
    box.addState(expanded);

    QMLState constrained;
    constrained.name = QStringLiteral("constrained");
    constrained.changes = {{&box, QByteArrayLiteral("maximumWidth"), 240}};
    box.addState(constrained);

    box.setState(QStringLiteral("expanded"));
    ASSERT_EQ(box.minimumWidth(), 80);

    box.setState(QStringLiteral("constrained"));
    EXPECT_EQ(box.minimumWidth(), 10);
    EXPECT_EQ(box.maximumWidth(), 240);
}

TEST_F(QMLPlusTest, Contract_DestroyedStateTargetsAreRemovedFromDefaultStorage)
{
    QMLPlusBox box;
    auto* target = new QObject();
    target->setProperty("phase", QStringLiteral("idle"));

    QMLState active;
    active.name = QStringLiteral("active");
    active.changes = {{target, QByteArrayLiteral("phase"), QStringLiteral("running")}};
    box.addState(active);
    box.setState(QStringLiteral("active"));
    ASSERT_EQ(target->property("phase").toString(), QStringLiteral("running"));

    delete target;
    box.setState(QString());

    EXPECT_TRUE(box.state().isEmpty());
}

TEST_F(QMLPlusTest, Contract_ReentrantStateChangeUsesLatestRequestedState)
{
    QMLPlusBox box;
    QMLPlusStateTarget target;

    QMLState first;
    first.name = QStringLiteral("first");
    first.changes = {{&target, QByteArrayLiteral("value"), 1}};
    box.addState(first);

    QMLState second;
    second.name = QStringLiteral("second");
    second.changes = {{&target, QByteArrayLiteral("value"), 2}};
    box.addState(second);

    int callbackCount = 0;
    QObject::connect(&target, &QMLPlusStateTarget::valueChanged, &target, [&](int value) {
        ++callbackCount;
        if (value == 1)
            box.setState(QStringLiteral("second"));
    });

    box.setState(QStringLiteral("first"));

    EXPECT_EQ(box.state(), QStringLiteral("second"));
    EXPECT_EQ(target.value(), 2);
    EXPECT_LE(callbackCount, 3);
}

TEST_F(QMLPlusTest, Contract_StateTargetCanDeleteItselfDuringDefaultRestore)
{
    QMLPlusBox box;
    auto* target = new QMLPlusStateTarget;
    QPointer<QMLPlusStateTarget> targetGuard(target);

    QMLState active;
    active.name = QStringLiteral("active");
    active.changes = {{target, QByteArrayLiteral("value"), 1}};
    box.addState(active);
    box.setState(QStringLiteral("active"));
    ASSERT_EQ(target->value(), 1);

    QObject::connect(target, &QMLPlusStateTarget::valueChanged, target, [target](int value) {
        if (value == 0)
            delete target;
    });

    box.setState(QString());

    EXPECT_TRUE(targetGuard.isNull());
    EXPECT_TRUE(box.state().isEmpty());
}

// =============================================================================
// 4. Test Case
// =============================================================================
TEST_F(QMLPlusTest, CoreCapabilitiesVisualCheck)
{
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
    highlightState.changes = {{box1, "minimumSize", QSize(150, 150)}};
    box1->addState(highlightState);

    QObject::connect(btnToggleState, &Button::clicked,
                     [box1]() { box1->setState(box1->state() == "" ? "highlight" : ""); });

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
