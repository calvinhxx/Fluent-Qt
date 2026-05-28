#include <gtest/gtest.h>
#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QPointF>
#include <QStringList>
#include <QtGlobal>
#include <QWidget>
#include <type_traits>

#include "compatibility/QtCompat.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEnterEvent>
#endif

namespace {

QString repositoryRootPath() {
    QDir dir(QFileInfo(QString::fromLocal8Bit(__FILE__)).absoluteDir());
    dir.cdUp();
    dir.cdUp();
    return dir.absolutePath();
}

QString relativeToRepository(const QString& absolutePath) {
    return QDir(repositoryRootPath()).relativeFilePath(absolutePath);
}

bool fileContainsQtVersionGuard(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ADD_FAILURE() << "Could not read " << path.toStdString();
        return false;
    }

    const QString content = QString::fromUtf8(file.readAll());
    return content.contains(QStringLiteral("QT_VERSION_CHECK")) ||
           content.contains(QStringLiteral("QT_VERSION >=")) ||
           content.contains(QStringLiteral("QT_VERSION <"));
}

bool isAllowedQtVersionGuardFile(const QString& relativePath) {
    // WindowChromeCompat is a platform compatibility boundary. TestWindow keeps
    // one assertion for the macOS Qt 6.9 expanded-client-area contract.
    static const QStringList allowed = {
        QStringLiteral("tests/views/TestQtCompat.cpp"),
        QStringLiteral("tests/views/windowing/TestWindow.cpp")
    };
    return allowed.contains(relativePath);
}

} // namespace

TEST(QtCompat, FluentEnterEventDerivesFromQEvent) {
    static_assert(std::is_base_of<QEvent, FluentEnterEvent>::value,
                  "FluentEnterEvent must derive from QEvent");
    SUCCEED();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
TEST(QtCompat, FluentEnterEventMatchesQt6Type) {
    static_assert(std::is_same<FluentEnterEvent, QEnterEvent>::value,
                  "On Qt6, FluentEnterEvent must alias QEnterEvent");
    SUCCEED();
}
#else
TEST(QtCompat, FluentEnterEventMatchesQt5Type) {
    static_assert(std::is_same<FluentEnterEvent, QEvent>::value,
                  "On Qt5, FluentEnterEvent must alias QEvent");
    SUCCEED();
}
#endif

TEST(QtCompat, QtVersionMacrosDefined) {
    EXPECT_GT(QT_VERSION, 0);
    EXPECT_GE(QT_VERSION_MAJOR, 5);
}

TEST(QtCompat, WheelPositionHelperReturnsLocalPosition) {
    FLUENT_MAKE_WHEEL_EVENT(event, 12, 18, 120, Qt::ControlModifier);

    EXPECT_EQ(fluentWheelPosition(&event), QPointF(12, 18));
}

TEST(QtCompat, NativeGestureConstructorCapabilityIsCentralized) {
    EXPECT_EQ(fluentCanConstructNativeGestureEvent(),
              FLUENT_HAS_NATIVE_GESTURE_EVENT_CONSTRUCTOR != 0);
    EXPECT_NE(QString::fromLatin1(fluentNativeGestureEventSkipReason()), QString());
}

TEST(QtCompat, NativeGesturePositionHelperReturnsLocalPositionWhenConstructible) {
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

TEST(QtCompat, ComponentSourcesDoNotContainScatteredQtVersionGuards) {
    const QString root = repositoryRootPath();
    const QStringList scanRoots = {
        root + QStringLiteral("/src/view"),
        root + QStringLiteral("/tests/views")
    };
    const QStringList nameFilters = {
        QStringLiteral("*.h"),
        QStringLiteral("*.cpp")
    };

    QStringList offenders;
    for (const QString& scanRoot : scanRoots) {
        QDirIterator it(scanRoot, nameFilters, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString relativePath = relativeToRepository(path);
            if (isAllowedQtVersionGuardFile(relativePath))
                continue;
            if (fileContainsQtVersionGuard(path))
                offenders.push_back(relativePath);
        }
    }

    EXPECT_TRUE(offenders.isEmpty()) << "Scattered Qt version guard in: "
                                     << offenders.join(QStringLiteral(", ")).toStdString();
}
