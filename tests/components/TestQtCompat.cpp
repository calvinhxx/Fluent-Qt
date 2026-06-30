#include <gtest/gtest.h>
#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QListView>
#include <QPointF>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
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
    // src/compatibility is the project boundary for Qt-version branches;
    // TestQtCompat intentionally asserts that boundary.
    if (relativePath.startsWith(QStringLiteral("src/compatibility/")))
        return true;

    static const QStringList allowed = {
        QStringLiteral("tests/components/TestQtCompat.cpp")
    };
    return allowed.contains(relativePath);
}

class FixedHeightDelegate : public QStyledItemDelegate {
public:
    explicit FixedHeightDelegate(int height) : m_height(height) {}

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(80, m_height);
    }

private:
    int m_height;
};

} // namespace

TEST(QtCompat, FluentEnterEventDerivesFromQEvent) {
    static_assert(std::is_base_of<QEvent, FluentEnterEvent>::value,
                  "FluentEnterEvent must derive from QEvent");
    SUCCEED();
}

TEST(QtCompat, FluentEnterEventMatchesExpectedType) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    static_assert(std::is_same<FluentEnterEvent, QEnterEvent>::value,
                  "On Qt6, FluentEnterEvent must alias QEnterEvent");
#else
    static_assert(std::is_same<FluentEnterEvent, QEvent>::value,
                  "On Qt5, FluentEnterEvent must alias QEvent");
#endif
    SUCCEED();
}

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

TEST(QtCompat, WheelHelpersClassifyAndNormalizeNoPhasePixelInput) {
    FLUENT_MAKE_WHEEL_EVENT_WITH_PHASE(event, QPoint(8, 9), QPoint(8, 9),
                                       QPoint(0, -60), QPoint(),
                                       Qt::NoButton, Qt::NoModifier,
                                       Qt::NoScrollPhase, false);

    EXPECT_EQ(fluentWheelInputKind(&event), FluentWheelInputKind::NoPhasePixel);
    EXPECT_EQ(fluentWheelDeltaY(&event), -60.0);
}

TEST(QtCompat, WheelHelpersClassifyAndNormalizeNoPhaseDiscreteInput) {
    FLUENT_MAKE_WHEEL_EVENT(event, 8, 9, -120, Qt::NoModifier);

    EXPECT_EQ(fluentWheelInputKind(&event), FluentWheelInputKind::NoPhaseDiscrete);
    EXPECT_EQ(fluentWheelDeltaY(&event), -120.0);
}

TEST(QtCompat, WheelHelpersClassifyPhaseBasedInput) {
    FLUENT_MAKE_WHEEL_EVENT_WITH_PHASE(event, QPoint(8, 9), QPoint(8, 9),
                                       QPoint(0, -30), QPoint(),
                                       Qt::NoButton, Qt::NoModifier,
                                       Qt::ScrollUpdate, false);

    EXPECT_EQ(fluentWheelInputKind(&event), FluentWheelInputKind::PhaseBased);
    EXPECT_EQ(fluentWheelDeltaY(&event), -30.0);
}

TEST(QtCompat, ItemViewRowHeightPrefersVisualRectHeight) {
    QListView view;
    QStandardItemModel model;
    model.appendRow(new QStandardItem(QStringLiteral("row")));
    view.setModel(&model);

    const QModelIndex index = model.index(0, 0);
    EXPECT_EQ(fluentItemViewRowHeight(&view, index, QRect(0, 0, 120, 24)), 24);
}

TEST(QtCompat, ItemViewRowHeightFallsBackToDelegateSizeHint) {
    QListView view;
    QStandardItemModel model;
    model.appendRow(new QStandardItem(QStringLiteral("row")));
    FixedHeightDelegate delegate(44);
    view.setModel(&model);
    view.setItemDelegate(&delegate);

    const QModelIndex index = model.index(0, 0);
    EXPECT_EQ(fluentItemViewRowHeight(&view, index, QRect(0, 0, 120, 0)), 44);
}

TEST(QtCompat, ProjectSourcesDoNotContainScatteredQtVersionGuards) {
    const QString root = repositoryRootPath();
    const QStringList scanRoots = {
        root + QStringLiteral("/app"),
        root + QStringLiteral("/src"),
        root + QStringLiteral("/tests/components"),
        root + QStringLiteral("/tests/gallery")
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
