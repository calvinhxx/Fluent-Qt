#include "VisualGeometryTestUtils.h"

#include <QDebug>
#include <QRect>
#include <QSize>
#include <QWidget>

#include <cstdlib>
#include <iostream>

namespace fluent::testutils::visual_geometry {
namespace {

QString boolText(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString pointText(const QPoint& point)
{
    return QStringLiteral("(%1,%2)").arg(point.x()).arg(point.y());
}

QString sizeText(const QSize& size)
{
    return QStringLiteral("%1x%2").arg(size.width()).arg(size.height());
}

QString rectText(const QRect& rect)
{
    return QStringLiteral("(%1,%2 %3x%4)")
        .arg(rect.x())
        .arg(rect.y())
        .arg(rect.width())
        .arg(rect.height());
}

QString widgetName(const QWidget* widget)
{
    if (!widget)
        return QStringLiteral("<null>");
    return widget->objectName().isEmpty()
        ? QStringLiteral("<unnamed>")
        : widget->objectName();
}

bool within(int actual, int expected, int tolerance)
{
    return qAbs(actual - expected) <= qMax(0, tolerance);
}

bool rectWithin(const QRect& actual, const QRect& expected, int tolerance)
{
    return within(actual.x(), expected.x(), tolerance)
        && within(actual.y(), expected.y(), tolerance)
        && within(actual.width(), expected.width(), tolerance)
        && within(actual.height(), expected.height(), tolerance);
}

void dumpWidgetLine(const QWidget* widget, int depth)
{
    if (!widget)
        return;

    const QString indent(depth * 2, QLatin1Char(' '));
    const QRect local = widget->geometry();
    const QPoint globalTopLeft = widget->mapToGlobal(QPoint(0, 0));
    const QRect global(globalTopLeft, widget->size());

    std::cout << indent.toStdString()
              << widgetName(widget).toStdString()
              << " class=" << widget->metaObject()->className()
              << " visible=" << boolText(widget->isVisible()).toStdString()
              << " enabled=" << boolText(widget->isEnabled()).toStdString()
              << " local=" << rectText(local).toStdString()
              << " global=" << rectText(global).toStdString()
              << " center=" << pointText(local.center()).toStdString()
              << " sizeHint=" << sizeText(widget->sizeHint()).toStdString()
              << " dpr=" << widget->devicePixelRatioF()
              << '\n';
}

void dumpSubtree(const QWidget* widget, int depth, bool includeUnnamed)
{
    if (!widget)
        return;

    if (includeUnnamed || !widget->objectName().isEmpty())
        dumpWidgetLine(widget, depth);

    const auto children = widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (const QWidget* child : children)
        dumpSubtree(child, depth + 1, includeUnnamed);
}

} // namespace

bool dumpEnabled()
{
    return qEnvironmentVariableIsSet("FLUENT_QT_GEOMETRY_DUMP")
        || qEnvironmentVariableIsSet("VISUAL_GEOMETRY_DUMP");
}

QString widgetSummary(const QWidget* widget)
{
    if (!widget)
        return QStringLiteral("<null widget>");

    return QStringLiteral("%1 class=%2 local=%3 global=%4 center=%5 sizeHint=%6 visible=%7 enabled=%8 dpr=%9")
        .arg(widgetName(widget))
        .arg(QString::fromLatin1(widget->metaObject()->className()))
        .arg(rectText(widget->geometry()))
        .arg(rectText(QRect(widget->mapToGlobal(QPoint(0, 0)), widget->size())))
        .arg(pointText(widget->geometry().center()))
        .arg(sizeText(widget->sizeHint()))
        .arg(boolText(widget->isVisible()))
        .arg(boolText(widget->isEnabled()))
        .arg(widget->devicePixelRatioF());
}

::testing::AssertionResult centerYWithin(const QWidget* widget,
                                         const QWidget* reference,
                                         int tolerance)
{
    if (!widget || !reference) {
        return ::testing::AssertionFailure()
            << "Cannot compare centerY with null widget. widget="
            << widgetSummary(widget).toStdString()
            << " reference=" << widgetSummary(reference).toStdString();
    }

    const int actual = widget->geometry().center().y();
    const int expected = reference->rect().center().y();
    if (within(actual, expected, tolerance))
        return ::testing::AssertionSuccess();

    return ::testing::AssertionFailure()
        << "Expected centerY within " << tolerance
        << " px. actual=" << actual
        << " expected=" << expected
        << "\nwidget: " << widgetSummary(widget).toStdString()
        << "\nreference: " << widgetSummary(reference).toStdString();
}

::testing::AssertionResult centerYIs(const QWidget* widget,
                                     int expectedCenterY,
                                     int tolerance)
{
    if (!widget) {
        return ::testing::AssertionFailure()
            << "Cannot compare centerY with null widget";
    }

    const int actual = widget->geometry().center().y();
    if (within(actual, expectedCenterY, tolerance))
        return ::testing::AssertionSuccess();

    return ::testing::AssertionFailure()
        << "Expected centerY within " << tolerance
        << " px. actual=" << actual
        << " expected=" << expectedCenterY
        << "\nwidget: " << widgetSummary(widget).toStdString();
}

::testing::AssertionResult sizeIs(const QWidget* widget, const QSize& expected)
{
    if (!widget) {
        return ::testing::AssertionFailure()
            << "Cannot compare size with null widget";
    }

    if (widget->size() == expected)
        return ::testing::AssertionSuccess();

    return ::testing::AssertionFailure()
        << "Expected size " << sizeText(expected).toStdString()
        << " but got " << sizeText(widget->size()).toStdString()
        << "\nwidget: " << widgetSummary(widget).toStdString();
}

::testing::AssertionResult spacingXIs(const QWidget* left,
                                      const QWidget* right,
                                      int expected,
                                      int tolerance)
{
    if (!left || !right) {
        return ::testing::AssertionFailure()
            << "Cannot compare spacing with null widget. left="
            << widgetSummary(left).toStdString()
            << " right=" << widgetSummary(right).toStdString();
    }

    const int actual = right->geometry().left() - left->geometry().right();
    if (within(actual, expected, tolerance))
        return ::testing::AssertionSuccess();

    return ::testing::AssertionFailure()
        << "Expected horizontal spacing within " << tolerance
        << " px. actual=" << actual
        << " expected=" << expected
        << "\nleft: " << widgetSummary(left).toStdString()
        << "\nright: " << widgetSummary(right).toStdString();
}

::testing::AssertionResult rectIsWithin(const QWidget* widget,
                                        const QRect& expected,
                                        int tolerance)
{
    if (!widget) {
        return ::testing::AssertionFailure()
            << "Cannot compare rect with null widget";
    }

    const QRect actual = widget->geometry();
    if (rectWithin(actual, expected, tolerance))
        return ::testing::AssertionSuccess();

    return ::testing::AssertionFailure()
        << "Expected rect within " << tolerance
        << " px. actual=" << rectText(actual).toStdString()
        << " expected=" << rectText(expected).toStdString()
        << "\nwidget: " << widgetSummary(widget).toStdString();
}

::testing::AssertionResult containedIn(const QWidget* widget,
                                       const QWidget* parent,
                                       int tolerance)
{
    if (!widget || !parent) {
        return ::testing::AssertionFailure()
            << "Cannot compare containment with null widget. widget="
            << widgetSummary(widget).toStdString()
            << " parent=" << widgetSummary(parent).toStdString();
    }

    const QRect parentRect = parent->rect().adjusted(-tolerance, -tolerance,
                                                     tolerance, tolerance);
    if (parentRect.contains(widget->geometry()))
        return ::testing::AssertionSuccess();

    return ::testing::AssertionFailure()
        << "Expected widget to be contained in parent with tolerance "
        << tolerance
        << "\nwidget: " << widgetSummary(widget).toStdString()
        << "\nparent: " << widgetSummary(parent).toStdString();
}

void dumpNamedWidgets(const QWidget* root, const QStringList& objectNames)
{
    if (!root) {
        std::cout << "<null root>\n";
        return;
    }

    std::cout << "[visual-geometry] named widgets under "
              << widgetName(root).toStdString() << '\n';
    for (const QString& objectName : objectNames) {
        const QWidget* widget = root->findChild<QWidget*>(objectName);
        if (!widget) {
            std::cout << "  missing " << objectName.toStdString() << '\n';
            continue;
        }
        dumpWidgetLine(widget, 1);
    }
}

void dumpWidgetSubtree(const QWidget* root, bool includeUnnamed)
{
    std::cout << "[visual-geometry] subtree\n";
    dumpSubtree(root, 0, includeUnnamed);
}

void maybeDumpNamedWidgets(const QWidget* root, const QStringList& objectNames)
{
    if (dumpEnabled())
        dumpNamedWidgets(root, objectNames);
}

void maybeDumpWidgetSubtree(const QWidget* root, bool includeUnnamed)
{
    if (dumpEnabled())
        dumpWidgetSubtree(root, includeUnnamed);
}

} // namespace fluent::testutils::visual_geometry
