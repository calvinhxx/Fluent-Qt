#ifndef VISUALGEOMETRYTESTUTILS_H
#define VISUALGEOMETRYTESTUTILS_H

#include <gtest/gtest.h>

#include <QWidget>
#include <QString>
#include <QStringList>

class QSize;
class QRect;

namespace fluent::testutils::visual_geometry {

bool dumpEnabled();
QString widgetSummary(const QWidget* widget);

template <typename T>
T* findRequiredChild(QWidget* root, const QString& objectName)
{
    if (!root) {
        ADD_FAILURE() << "Cannot find child '" << objectName.toStdString()
                      << "' because root widget is null";
        return nullptr;
    }

    T* child = root->findChild<T*>(objectName);
    if (!child) {
        ADD_FAILURE() << "Missing widget '" << objectName.toStdString()
                      << "' under " << widgetSummary(root).toStdString();
    }
    return child;
}

::testing::AssertionResult centerYWithin(const QWidget* widget,
                                         const QWidget* reference,
                                         int tolerance = 1);
::testing::AssertionResult centerYIs(const QWidget* widget,
                                     int expectedCenterY,
                                     int tolerance = 1);
::testing::AssertionResult sizeIs(const QWidget* widget,
                                  const QSize& expected);
::testing::AssertionResult spacingXIs(const QWidget* left,
                                      const QWidget* right,
                                      int expected,
                                      int tolerance = 0);
::testing::AssertionResult rectIsWithin(const QWidget* widget,
                                        const QRect& expected,
                                        int tolerance = 0);
::testing::AssertionResult containedIn(const QWidget* widget,
                                       const QWidget* parent,
                                       int tolerance = 0);

void dumpNamedWidgets(const QWidget* root, const QStringList& objectNames);
void dumpWidgetSubtree(const QWidget* root, bool includeUnnamed = false);
void maybeDumpNamedWidgets(const QWidget* root, const QStringList& objectNames);
void maybeDumpWidgetSubtree(const QWidget* root, bool includeUnnamed = false);

} // namespace fluent::testutils::visual_geometry

#endif // VISUALGEOMETRYTESTUTILS_H
