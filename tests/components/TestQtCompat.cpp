#include <gtest/gtest.h>
#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QListView>
#include <QPointF>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QStringList>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <QScopeGuard>
#include <QtGlobal>
#include <QWidget>
#include <functional>
#include <type_traits>

#ifndef interface
#define FLUENT_QT_TEST_DEFINED_INTERFACE_MACRO
#define interface struct
#endif
#include "compatibility/QtCompat.h"
#include "compatibility/TextPaintCompat.h"
#include "compatibility/private/AccessibleInterfaces_p.h"
#include "compatibility/private/VariantCompat_p.h"
#include "compatibility/private/PointerEventCompat_p.h"
#ifdef FLUENT_QT_TEST_DEFINED_INTERFACE_MACRO
#undef interface
#undef FLUENT_QT_TEST_DEFINED_INTERFACE_MACRO
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEnterEvent>
#endif

namespace {

QString repositoryRootPath()
{
    QDir dir(QFileInfo(QString::fromLocal8Bit(__FILE__)).absoluteDir());
    dir.cdUp();
    dir.cdUp();
    return dir.absolutePath();
}

QString relativeToRepository(const QString& absolutePath)
{
    return QDir(repositoryRootPath()).relativeFilePath(absolutePath);
}

QString sourceContents(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ADD_FAILURE() << "Could not read " << path.toStdString();
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

template <typename Sender, typename Signal, typename Context, typename Functor>
QMetaObject::Connection
singleShotForTest(bool fallback, Sender* sender, Signal signal, Context* context, Functor&& functor)
{
    if (fallback)
        return compatibility::detail::connectSingleShotFallback(sender, signal, context,
            std::forward<Functor>(functor));
    return fluentConnectSingleShot(sender, signal, context, std::forward<Functor>(functor));
}

class FixedHeightDelegate : public QStyledItemDelegate {
public:
    explicit FixedHeightDelegate(int height) : m_height(height) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(80, m_height);
    }

private:
    int m_height;
};

class PointerEventFilter final : public QObject {
public:
    explicit PointerEventFilter(std::function<void(QEvent*)> observe) : m_observe(std::move(observe)) {}

protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() != QEvent::Enter && event->type() != QEvent::MouseMove)
            return false;
        m_observe(event);
        return true;
    }

private:
    std::function<void(QEvent*)> m_observe;
};

} // namespace

TEST(QtCompat, FluentEnterEventDerivesFromQEvent)
{
    static_assert(std::is_base_of<QEvent, FluentEnterEvent>::value, "FluentEnterEvent must derive from QEvent");
    SUCCEED();
}

TEST(QtCompat, FluentEnterEventMatchesExpectedType)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    static_assert(std::is_same<FluentEnterEvent, QEnterEvent>::value,
        "On Qt6, FluentEnterEvent must alias QEnterEvent");
#else
    static_assert(std::is_same<FluentEnterEvent, QEvent>::value, "On Qt5, FluentEnterEvent must alias QEvent");
#endif
    SUCCEED();
}

TEST(QtCompat, QtVersionMacrosDefined)
{
    EXPECT_GT(QT_VERSION, 0);
    EXPECT_GE(QT_VERSION_MAJOR, 5);
}

TEST(QtCompat, EnterConstructionPreservesCoordinateSpaces)
{
    const QPoint local(2, 3);
    const QPoint scene(12, 23);
    const QPoint global(102, 203);
    FLUENT_MAKE_ENTER_EVENT_AT(event, local, scene, global);
    EXPECT_EQ(event.type(), QEvent::Enter);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    EXPECT_EQ(event.position(), QPointF(local));
    EXPECT_EQ(event.scenePosition(), QPointF(scene));
    EXPECT_EQ(event.globalPosition(), QPointF(global));
#else
    Q_UNUSED(local);
    Q_UNUSED(scene);
    Q_UNUSED(global);
#endif
}

TEST(QtCompat, EnterCoordinatesComeFromEventAndRejectCoordinateLessEvents)
{
    const QPointF global(102.25, 203.75);
    QEnterEvent enter(QPointF(2.25, 3.75), QPointF(12.25, 23.75), global);
    const auto position = compatibility::detail::enterEventGlobalPosition(&enter);
    ASSERT_TRUE(position.has_value());
    EXPECT_EQ(*position, global);
    EXPECT_EQ(fluentEnterPos(&enter), QPoint(2, 4));

    QEvent plainEnter(QEvent::Enter);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    EXPECT_EQ(fluentEnterPos(&plainEnter), QPoint());
#endif
    EXPECT_FALSE(compatibility::detail::enterEventGlobalPosition(&plainEnter).has_value());
    EXPECT_FALSE(compatibility::detail::enterEventGlobalPosition(nullptr).has_value());
    QWidget target;
    int received = 0;
    PointerEventFilter observer([&](QEvent*) { ++received; });
    target.installEventFilter(&observer);
    EXPECT_FALSE(compatibility::detail::sendEnterEvent(&target, &plainEnter));
    EXPECT_FALSE(compatibility::detail::sendEnterEvent(nullptr, &enter));
    EXPECT_EQ(received, 0);
}

TEST(QtCompat, RedirectedPointerEventsPreserveCoordinatesAndInputMetadata)
{
    QWidget host;
    host.move(40, 60);
    QWidget target(&host);
    target.move(12, 18);
    const QPointF global = target.mapToGlobal(QPoint(7, 9)) + QPointF(0.25, 0.75);
    const QPointF local(7.25, 9.75);
    const QPointF window(19.25, 27.75);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QPointingDevice device(QStringLiteral("Compatibility test mouse"), 123, QInputDevice::DeviceType::Mouse,
        QPointingDevice::PointerType::Generic, QInputDevice::Capability::Position, 1, 3);
    QEnterEvent enter(QPointF(-1, -2), QPointF(-3, -4), global, &device);
    enter.setTimestamp(1234);
    const auto makeMouse = [&] {
        return QMouseEvent(QEvent::MouseMove, QPointF(-1, -2), QPointF(-3, -4), global, Qt::NoButton, Qt::LeftButton,
            Qt::ControlModifier, Qt::MouseEventSynthesizedByApplication, &device);
    };
#else
    QEnterEvent enter(QPointF(-1, -2), QPointF(-3, -4), global);
    const auto makeMouse = [&] {
        return QMouseEvent(QEvent::MouseMove, QPointF(-1, -2), QPointF(-3, -4), global, Qt::NoButton, Qt::LeftButton,
            Qt::ControlModifier, Qt::MouseEventSynthesizedByApplication);
    };
#endif
    QEvent::Type expectedType = QEvent::Enter;
    QPointF expectedGlobal = global;
    bool observeNativeMouse = false;
    int received = 0;
    PointerEventFilter observer([&](QEvent* event) {
        if (observeNativeMouse) {
            // Qt may round globals during widget notification. Compare with
            // direct delivery on this runtime rather than guessing by version.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            expectedGlobal = static_cast<const QMouseEvent*>(event)->globalPosition();
#else
            expectedGlobal = static_cast<const QMouseEvent*>(event)->screenPos();
#endif
            return;
        }
        ++received;
        EXPECT_EQ(event->type(), expectedType);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const auto* point = dynamic_cast<const QSinglePointEvent*>(event);
        ASSERT_NE(point, nullptr);
        EXPECT_EQ(point->position(), local);
        EXPECT_EQ(point->scenePosition(), window);
        EXPECT_EQ(point->globalPosition(), expectedGlobal);
        EXPECT_EQ(point->pointingDevice(), &device);
        EXPECT_EQ(point->timestamp(), 1234u);
#else
        if (event->type() == QEvent::Enter) {
            const auto* point = dynamic_cast<const QEnterEvent*>(event);
            ASSERT_NE(point, nullptr);
            EXPECT_EQ(point->localPos(), local);
            EXPECT_EQ(point->windowPos(), window);
            EXPECT_EQ(point->screenPos(), expectedGlobal);
        } else {
            const auto* point = static_cast<const QMouseEvent*>(event);
            EXPECT_EQ(point->localPos(), local);
            EXPECT_EQ(point->windowPos(), window);
            EXPECT_EQ(point->screenPos(), expectedGlobal);
        }
#endif
        if (event->type() == QEvent::MouseMove) {
            const auto* moved = static_cast<const QMouseEvent*>(event);
            EXPECT_EQ(moved->button(), Qt::NoButton);
            EXPECT_EQ(moved->buttons(), Qt::LeftButton);
            EXPECT_EQ(moved->modifiers(), Qt::ControlModifier);
            EXPECT_EQ(moved->source(), Qt::MouseEventSynthesizedByApplication);
            EXPECT_EQ(moved->timestamp(), 1234u);
        }
    });
    target.installEventFilter(&observer);
    EXPECT_TRUE(compatibility::detail::sendEnterEvent(&target, &enter));
    EXPECT_EQ(received, 1);
    observeNativeMouse = true;
    auto nativeMouse = makeMouse();
    QCoreApplication::sendEvent(&target, &nativeMouse);
    observeNativeMouse = false;
    expectedType = QEvent::MouseMove;
    auto mouse = makeMouse();
    mouse.setTimestamp(1234);
    const auto translated = compatibility::detail::mouseEventForWidget(&mouse, &target);
    EXPECT_EQ(compatibility::detail::mouseLocalPosition(&translated), local);
    EXPECT_EQ(compatibility::detail::mouseGlobalPosition(&translated), global);
    EXPECT_EQ(translated.source(), mouse.source());
    EXPECT_EQ(translated.timestamp(), mouse.timestamp());
    EXPECT_EQ(translated.buttons(), mouse.buttons());
    EXPECT_EQ(translated.modifiers(), mouse.modifiers());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    EXPECT_EQ(translated.scenePosition(), window);
    EXPECT_EQ(translated.pointingDevice(), &device);
#else
    EXPECT_EQ(translated.windowPos(), window);
#endif
    EXPECT_TRUE(compatibility::detail::sendMouseEvent(&target, &mouse));
    EXPECT_EQ(received, 2);
}

TEST(QtCompat, OpenGLWidgetWindowReplacementMatchesQtRuntimeLine)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    EXPECT_EQ(FLUENT_OPENGL_WIDGET_CAN_REPLACE_NATIVE_WINDOW, 1);
#else
    EXPECT_EQ(FLUENT_OPENGL_WIDGET_CAN_REPLACE_NATIVE_WINDOW, 0);
#endif
}

TEST(QtCompat, AccessibleInterfaceCapabilitiesMatchQtVersion)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    EXPECT_EQ(FLUENT_HAS_ACCESSIBLE_HYPERLINK_INTERFACE, 1);
#else
    EXPECT_EQ(FLUENT_HAS_ACCESSIBLE_HYPERLINK_INTERFACE, 0);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    EXPECT_EQ(FLUENT_HAS_ACCESSIBLE_SELECTION_INTERFACE, 1);
#else
    EXPECT_EQ(FLUENT_HAS_ACCESSIBLE_SELECTION_INTERFACE, 0);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    EXPECT_EQ(FLUENT_HAS_ACCESSIBLE_DESCRIPTION_RELATION, 1);
    EXPECT_EQ(FLUENT_HAS_UNCONDITIONAL_ACCESSIBLE_UPDATE_HANDLER, 1);
#else
    EXPECT_EQ(FLUENT_HAS_ACCESSIBLE_DESCRIPTION_RELATION, 0);
    EXPECT_EQ(FLUENT_HAS_UNCONDITIONAL_ACCESSIBLE_UPDATE_HANDLER, 0);
#endif
}

TEST(QtCompat, AccessibleRelationsPreserveCombinedFlagsWithoutDuplicates)
{
#if QT_CONFIG(accessibility)
    QWidget target;
    QWidget other;
    auto* targetInterface = QAccessible::queryAccessibleInterface(&target);
    auto* otherInterface = QAccessible::queryAccessibleInterface(&other);
    ASSERT_NE(targetInterface, nullptr);
    ASSERT_NE(otherInterface, nullptr);

    QList<QAccessible::Relation> relations = {QAccessible::Label, QAccessible::Controlled};
    const auto description = compatibility::detail::accessibleDescriptionRelation();
    if (description)
        relations.append(description);
    for (const auto relation : relations) {
        SCOPED_TRACE(static_cast<int>(relation));
        const auto combined = relation | QAccessible::Controller;
        const FluentAccessibleRelationList inherited = {{otherInterface, relation}, {targetInterface, combined}};
        EXPECT_EQ(
            compatibility::detail::appendAccessibleRelation(inherited, &target, relation, QAccessible::AllRelations),
            inherited);
        EXPECT_EQ(
            compatibility::detail::appendAccessibleRelation(inherited, &target, relation, QAccessible::Controller),
            inherited);
        EXPECT_EQ(
            compatibility::detail::appendAccessibleRelation(inherited, nullptr, relation, QAccessible::AllRelations),
            inherited);

        const FluentAccessibleRelationList unrelated = {{otherInterface, combined},
            {targetInterface, QAccessible::Controller}};
        const auto appended = compatibility::detail::appendAccessibleRelation(unrelated, &target, relation,
            QAccessible::AllRelations);
        ASSERT_EQ(appended.size(), 3);
        EXPECT_EQ(appended.at(0), unrelated.at(0));
        EXPECT_EQ(appended.at(1), unrelated.at(1));
        EXPECT_EQ(appended.last().first, targetInterface);
        EXPECT_EQ(appended.last().second, relation);
    }
    EXPECT_TRUE(compatibility::detail::appendAccessibleRelation({}, &target, {}, QAccessible::AllRelations).isEmpty());
#else
    GTEST_SKIP() << "Qt accessibility is disabled.";
#endif
}

TEST(QtCompat, CheckBoxStateConnectionNormalizesState)
{
    QCheckBox checkBox;
    checkBox.setTristate(true);
    QVector<Qt::CheckState> observed;

    const auto connection = fluentConnectCheckStateChanged(&checkBox, &checkBox, [&observed](Qt::CheckState state) {
        observed.push_back(state);
    });

    EXPECT_TRUE(connection);
    checkBox.setCheckState(Qt::PartiallyChecked);
    ASSERT_EQ(observed.size(), 1);
    EXPECT_EQ(observed.constFirst(), Qt::PartiallyChecked);
}

TEST(QtCompat, WheelPositionHelperReturnsLocalPosition)
{
    FLUENT_MAKE_WHEEL_EVENT(event, 12, 18, 120, Qt::ControlModifier);

    EXPECT_EQ(fluentWheelPosition(&event), QPointF(12, 18));
}

TEST(QtCompat, NativeGestureConstructorCapabilityIsCentralized)
{
    EXPECT_EQ(fluentCanConstructNativeGestureEvent(), FLUENT_HAS_NATIVE_GESTURE_EVENT_CONSTRUCTOR != 0);
    EXPECT_NE(QString::fromLatin1(fluentNativeGestureEventSkipReason()), QString());
}

TEST(QtCompat, NativeGesturePositionHelperReturnsLocalPositionWhenConstructible)
{
#if FLUENT_HAS_NATIVE_GESTURE_EVENT_CONSTRUCTOR
    ASSERT_NE(qApp, nullptr);
    QWidget target;
    target.resize(120, 90);

    FLUENT_MAKE_NATIVE_GESTURE_EVENT(event, &target, Qt::ZoomNativeGesture, 13, 21, 0.25);

    EXPECT_EQ(fluentNativeGesturePosition(&event), QPointF(13, 21));
#else
    GTEST_SKIP() << fluentNativeGestureEventSkipReason();
#endif
}

TEST(QtCompat, WheelHelpersClassifyAndNormalizeNoPhasePixelInput)
{
    FLUENT_MAKE_WHEEL_EVENT_WITH_PHASE(event, QPoint(8, 9), QPoint(8, 9), QPoint(0, -60), QPoint(), Qt::NoButton,
        Qt::NoModifier, Qt::NoScrollPhase, false);

    EXPECT_EQ(fluentWheelInputKind(&event), FluentWheelInputKind::NoPhasePixel);
    EXPECT_EQ(fluentWheelDeltaY(&event), -60.0);
}

TEST(QtCompat, WheelHelpersClassifyAndNormalizeNoPhaseDiscreteInput)
{
    FLUENT_MAKE_WHEEL_EVENT(event, 8, 9, -120, Qt::NoModifier);

    EXPECT_EQ(fluentWheelInputKind(&event), FluentWheelInputKind::NoPhaseDiscrete);
    EXPECT_EQ(fluentWheelDeltaY(&event), -120.0);
}

TEST(QtCompat, WheelHelpersClassifyPhaseBasedInput)
{
    ASSERT_TRUE(fluentWheelEventSupportsPhase());

    FLUENT_MAKE_WHEEL_EVENT_WITH_PHASE(event, QPoint(8, 9), QPoint(8, 9), QPoint(0, -30), QPoint(), Qt::NoButton,
        Qt::NoModifier, Qt::ScrollUpdate, false);

    EXPECT_EQ(fluentWheelInputKind(&event), FluentWheelInputKind::PhaseBased);
    EXPECT_EQ(fluentWheelDeltaY(&event), -30.0);
}

TEST(QtCompat, SingleShotConnectionsKeepArgumentsReentrancyAndContextLifetime)
{
    for (bool fallback : {false, true}) {
        SCOPED_TRACE(fallback);
        QObject sender;
        QObject receiver;
        QStringList observed;
        singleShotForTest(fallback, &sender, &QObject::objectNameChanged, &receiver, [&](const QString& name) {
            observed.append(name);
            sender.setObjectName(QStringLiteral("recursive"));
        });
        sender.setObjectName(QStringLiteral("first"));
        sender.setObjectName(QStringLiteral("later"));
        EXPECT_EQ(observed, QStringList{QStringLiteral("first")});

        int ignoredArgumentCalls = 0;
        singleShotForTest(fallback, &sender, &QObject::objectNameChanged, &receiver, [&] { ++ignoredArgumentCalls; });
        sender.setObjectName(QStringLiteral("ignored"));
        sender.setObjectName(QStringLiteral("ignored again"));
        EXPECT_EQ(ignoredArgumentCalls, 1);

        singleShotForTest(fallback, &sender, &QObject::objectNameChanged, &receiver,
            static_cast<void (QObject::*)(const QString&)>(&QObject::setObjectName));
        sender.setObjectName(QStringLiteral("member slot"));
        sender.setObjectName(QStringLiteral("member slot again"));
        EXPECT_EQ(receiver.objectName(), QStringLiteral("member slot"));

        auto context = std::make_unique<QObject>();
        auto capture = std::make_shared<int>(42);
        std::weak_ptr<int> lifetime = capture;
        int cancelledCalls = 0;
        singleShotForTest(fallback, &sender, &QObject::objectNameChanged, context.get(),
            [capture, &cancelledCalls](const QString&) { ++cancelledCalls; });
        capture.reset();
        EXPECT_FALSE(lifetime.expired());
        context.reset();
        EXPECT_TRUE(lifetime.expired());
        sender.setObjectName(QStringLiteral("after context destruction"));
        EXPECT_EQ(cancelledCalls, 0);
    }
}

TEST(QtCompat, SingleShotQueuedDeliveriesInvokeOnlyTheFirstSignal)
{
    for (bool fallback : {false, true}) {
        SCOPED_TRACE(fallback);
        QThread worker;
        auto* sender = new QObject;
        sender->moveToThread(&worker);
        QObject::connect(&worker, &QThread::finished, sender, &QObject::deleteLater);
        const auto cleanup = qScopeGuard([&] {
            worker.quit();
            worker.wait();
        });
        QObject receiver;
        QStringList observed;
        singleShotForTest(fallback, sender, &QObject::objectNameChanged, &receiver, [&](const QString& name) {
            observed.append(name);
        });
        worker.start();
        ASSERT_TRUE(QMetaObject::invokeMethod(
            sender,
            [sender] {
                sender->setObjectName(QStringLiteral("first"));
                sender->setObjectName(QStringLiteral("second"));
            },
            Qt::BlockingQueuedConnection));
        EXPECT_TRUE(observed.isEmpty());
        QCoreApplication::sendPostedEvents(&receiver, QEvent::MetaCall);
        EXPECT_EQ(observed, QStringList{QStringLiteral("first")});
    }
}

TEST(QtCompat, ItemViewRowHeightPrefersVisualRectHeight)
{
    QListView view;
    QStandardItemModel model;
    model.appendRow(new QStandardItem(QStringLiteral("row")));
    view.setModel(&model);

    const QModelIndex index = model.index(0, 0);
    EXPECT_EQ(fluentItemViewRowHeight(&view, index, QRect(0, 0, 120, 24)), 24);
}

TEST(QtCompat, ItemViewRowHeightFallsBackToDelegateSizeHint)
{
    QListView view;
    QStandardItemModel model;
    model.appendRow(new QStandardItem(QStringLiteral("row")));
    FixedHeightDelegate delegate(44);
    view.setModel(&model);
    view.setItemDelegate(&delegate);

    const QModelIndex index = model.index(0, 0);
    EXPECT_EQ(fluentItemViewRowHeight(&view, index, QRect(0, 0, 120, 0)), 44);
}

TEST(QtCompat, AdjacentButtonRowSpacingDoesNotShrinkRequestedSpacing)
{
    EXPECT_GE(fluentAdjacentButtonRowSpacing(8), 8);
}

TEST(QtCompat, DisplayScaleChangeHelperCoversSupportedScreenEvents)
{
    EXPECT_FALSE(fluentIsDisplayScaleChangeEvent(nullptr));

    QEvent resizeEvent(QEvent::Resize);
    EXPECT_FALSE(fluentIsDisplayScaleChangeEvent(&resizeEvent));

    QEvent screenChangeEvent(QEvent::ScreenChangeInternal);
    EXPECT_TRUE(fluentIsDisplayScaleChangeEvent(&screenChangeEvent));
}

TEST(QtCompat, VariantConversionUsesTheRequestedPropertyType)
{
    QVariant value(QStringLiteral("2.5"));
    EXPECT_TRUE(::compatibility::detail::convertVariantToType(value, QMetaType::Double));
    EXPECT_EQ(value.userType(), QMetaType::Double);
    EXPECT_DOUBLE_EQ(value.toDouble(), 2.5);
    QVariant invalid(QStringLiteral("not a date"));
    EXPECT_FALSE(::compatibility::detail::convertVariantToType(invalid, QMetaType::QDate));
}

TEST(QtCompat, LocalSceneMouseMappingPreservesInputIdentity)
{
    QMouseEvent original(QEvent::MouseButtonPress, QPointF(12, 18), QPointF(40, 60), QPointF(120, 180), Qt::LeftButton,
        Qt::LeftButton, Qt::ShiftModifier, Qt::MouseEventSynthesizedByApplication);
    original.setTimestamp(5678);
    const auto mapped = ::compatibility::detail::mouseEventForLocalScene(&original);
    EXPECT_EQ(fluentMousePos(&mapped), QPoint(12, 18));
    EXPECT_EQ(fluentMouseGlobalPos(&mapped), QPoint(120, 180));
    EXPECT_EQ(mapped.type(), original.type());
    EXPECT_EQ(mapped.button(), original.button());
    EXPECT_EQ(mapped.buttons(), original.buttons());
    EXPECT_EQ(mapped.modifiers(), original.modifiers());
    EXPECT_EQ(mapped.source(), original.source());
    EXPECT_EQ(mapped.timestamp(), original.timestamp());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    EXPECT_EQ(mapped.scenePosition(), original.position());
    EXPECT_EQ(mapped.pointingDevice(), original.pointingDevice());
#else
    EXPECT_EQ(mapped.windowPos(), original.localPos());
#endif
}

TEST(QtCompat, TouchGlobalPositionHandlesEmptyAndPopulatedEvents)
{
    QPoint position(1, 2);
    const QTouchEvent empty(QEvent::TouchBegin);
    EXPECT_FALSE(fluentFirstTouchGlobalPos(&empty, position));
    EXPECT_EQ(position, QPoint(1, 2));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QEventPoint point(0, QEventPoint::State::Pressed, QPointF(10, 20), QPointF(110, 120));
    const QTouchEvent populated(QEvent::TouchBegin, nullptr, Qt::NoModifier, {point});
#else
    QTouchEvent::TouchPoint point(0);
    point.setScreenPos(QPointF(110, 120));
    const QTouchEvent populated(QEvent::TouchBegin, nullptr, Qt::NoModifier, Qt::TouchPointPressed, {point});
#endif
    EXPECT_TRUE(fluentFirstTouchGlobalPos(&populated, position));
    EXPECT_EQ(position, QPoint(110, 120));
}

TEST(QtCompat, InstalledHeadersDoNotDependOnUninstalledImplementationHeaders)
{
    const QString root = repositoryRootPath();
    const QString allowlist = sourceContents(root + "/cmake/FluentQtInstallHeaders.cmake");
    const QRegularExpression headerPath(QStringLiteral(R"(^\s*((?:src|include)/[^\s]+\.h)\s*$)"),
        QRegularExpression::MultilineOption);
    QSet<QString> installed;
    auto paths = headerPath.globalMatch(allowlist);
    while (paths.hasNext())
        installed.insert(paths.next().captured(1));
    ASSERT_FALSE(installed.isEmpty());
    const QRegularExpression include(QStringLiteral(R"re(#\s*include\s*["<]([^">]+)[">])re"));
    for (const QString& header : installed) {
        EXPECT_FALSE(header.contains("/private/")) << header.toStdString();
        auto includes = include.globalMatch(sourceContents(root + '/' + header));
        while (includes.hasNext()) {
            const QString name = includes.next().captured(1);
            const QStringList candidates = {QFileInfo(root + '/' + header).dir().filePath(name), root + "/src/" + name,
                root + "/include/" + name};
            for (const QString& candidate : candidates) {
                if (!QFileInfo::exists(candidate))
                    continue;
                const QString dependency = relativeToRepository(QDir::cleanPath(candidate));
                EXPECT_TRUE(installed.contains(dependency))
                    << header.toStdString() << " includes uninstalled " << dependency.toStdString();
                break;
            }
        }
    }
}

TEST(QtCompat, PaintDeviceDprMetricsPreserveImagePrecision)
{
    class ForwardingDevice final : public QPaintDevice {
    public:
        explicit ForwardingDevice(qreal dpr) : m_dpr(dpr) {}
        QPaintEngine* paintEngine() const override { return nullptr; }

    protected:
        int metric(PaintDeviceMetric metric) const override
        {
            return fluent::painting::devicePixelRatioMetric(metric, m_dpr);
        }

    private:
        qreal m_dpr;
    };
    const qreal scale = QPaintDevice::devicePixelRatioFScale();
    for (qreal dpr : {1.0, 1.1, 1.25, 1.5, 2.1875, 2.625, 3.0, 1.0 - .5 / scale, 1.0 + .5 / scale, 1.0 + 1.5 / scale,
             2.0 - .5 / scale, 2.0 + .5 / scale, 2.0 + 1.5 / scale}) {
        SCOPED_TRACE(dpr);
        QImage image(1, 1, QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(dpr);
        ForwardingDevice device(image.devicePixelRatioF());
        EXPECT_EQ(device.devicePixelRatioF(), image.devicePixelRatioF());
        EXPECT_EQ(fluent::painting::imageRasterDpr(dpr), image.devicePixelRatioF());
    }
}
