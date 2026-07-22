#include "GalleryWindowPlacement.h"

#include <algorithm>

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QWindow>

#include "compatibility/QtCompat.h"
#include "support/logging/Log.h"
#include "view/shell/GalleryWindow.h"
#include "view/shell/GalleryWindowMetrics.h"
#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {
namespace {

using AppWindowMetrics = metrics::AppWindow;

constexpr int kPlacementSaveDelayMs = 250;

int proportionalExtent(int available,
                       int percentage,
                       int preferredMinimum,
                       int preferredMaximum)
{
    if (available <= 0)
        return 0;
    const int lower = qMin(available, preferredMinimum);
    const int upper = qMin(available, preferredMaximum);
    return qBound(lower, qRound(available * percentage / 100.0), upper);
}

QRect centeredInitialGeometry(const QRect& available, const QSize& size)
{
    const int horizontalSlack = qMax(0, available.width() - size.width());
    const int verticalSlack = qMax(0, available.height() - size.height());
    const int leftReserve = qMin(AppWindowMetrics::LeftPanelReserve, horizontalSlack);
    const int x = available.x() + leftReserve
        + (horizontalSlack - leftReserve) / 2;
    const int y = available.y() + verticalSlack / 2;
    return QRect(QPoint(x, y), size);
}

} // namespace

namespace windowplacement {

QSize effectiveMinimumSize(const QSize& availableSize)
{
    if (!availableSize.isValid() || availableSize.isEmpty())
        return QSize(AppWindowMetrics::MinWidth, AppWindowMetrics::MinHeight);
    return QSize(qMin(AppWindowMetrics::MinWidth, availableSize.width()),
                 qMin(AppWindowMetrics::MinHeight, availableSize.height()));
}

QSize recommendedInitialSize(const QSize& availableSize)
{
    if (!availableSize.isValid() || availableSize.isEmpty()) {
        return QSize(AppWindowMetrics::InitialWidth,
                     AppWindowMetrics::InitialHeight);
    }

    return QSize(
        proportionalExtent(availableSize.width(),
                           AppWindowMetrics::InitialWidthPercent,
                           AppWindowMetrics::InitialMinWidth,
                           AppWindowMetrics::InitialMaxWidth),
        proportionalExtent(availableSize.height(),
                           AppWindowMetrics::InitialHeightPercent,
                           AppWindowMetrics::InitialMinHeight,
                           AppWindowMetrics::InitialMaxHeight));
}

QRect constrainGeometry(const QRect& requested,
                        const QRect& available,
                        const QSize& minimumSize)
{
    if (!available.isValid() || available.isEmpty())
        return requested;

    const QSize effectiveMinimum(
        qMin(qMax(1, minimumSize.width()), available.width()),
        qMin(qMax(1, minimumSize.height()), available.height()));
    QSize size = requested.size();
    if (!size.isValid() || size.isEmpty())
        size = effectiveMinimum;
    size.setWidth(qBound(effectiveMinimum.width(), size.width(), available.width()));
    size.setHeight(qBound(effectiveMinimum.height(), size.height(), available.height()));

    const int maximumX = available.right() - size.width() + 1;
    const int maximumY = available.bottom() - size.height() + 1;
    const int x = qBound(available.left(), requested.x(), maximumX);
    const int y = qBound(available.top(), requested.y(), maximumY);
    return QRect(QPoint(x, y), size);
}

QRect restoredGeometry(const QRect& savedGeometry,
                       const QRect& available,
                       const QSize& minimumSize)
{
    const bool savedSizeIsUsable = savedGeometry.width() >= minimumSize.width()
        && savedGeometry.height() >= minimumSize.height();
    if (savedGeometry.isValid() && !savedGeometry.isEmpty()
        && savedSizeIsUsable) {
        return constrainGeometry(savedGeometry, available, minimumSize);
    }

    return centeredInitialGeometry(available,
        recommendedInitialSize(available.size()));
}

} // namespace windowplacement

GalleryWindowPlacement::GalleryWindowPlacement(GalleryWindow* window,
                                               GallerySettings* settings,
                                               QObject* parent)
    : QObject(parent)
    , m_window(window)
    , m_settings(settings)
    , m_saveTimer(new QTimer(this))
{
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(kPlacementSaveDelayMs);
    connect(m_saveTimer, &QTimer::timeout, this, &GalleryWindowPlacement::saveNow);
    if (m_window)
        m_window->installEventFilter(this);
    if (qApp)
        connect(qApp, &QCoreApplication::aboutToQuit, this, &GalleryWindowPlacement::saveNow);
}

bool GalleryWindowPlacement::restore()
{
    if (!m_window || !m_settings)
        return false;

    m_restoring = true;
    QScreen* screen = restoredScreen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen) {
        m_restoring = false;
        return m_settings->windowMaximized();
    }

    const QRect available = screen->availableGeometry();
    const QSize minimum = windowplacement::effectiveMinimumSize(available.size());
    m_window->setMinimumSize(minimum);

    const QRect target = windowplacement::restoredGeometry(
        m_settings->windowNormalGeometry(),
        available,
        minimum);

    m_window->setGeometry(target);
    m_lastNormalGeometry = target;
    m_restoring = false;
    LOG_INFO(QStringLiteral("GalleryWindowPlacement restored geometry=%1,%2 %3x%4 screen=%5 maximized=%6")
                 .arg(target.x()).arg(target.y()).arg(target.width()).arg(target.height())
                 .arg(screen->name())
                 .arg(m_settings->windowMaximized()));
    return m_settings->windowMaximized();
}

void GalleryWindowPlacement::saveNow()
{
    if (!m_window || !m_settings || m_restoring)
        return;

    const Qt::WindowStates state = m_window->windowState();
    if (!state.testFlag(Qt::WindowMinimized)
        && !state.testFlag(Qt::WindowFullScreen)) {
        // On Wayland/WSLg QWidget::normalGeometry() can remain at Qt's
        // pre-show 640x480 placeholder while a normal window is visibly much
        // larger. Persist the live geometry for a normal window and reserve
        // normalGeometry() for the maximized state where it is actually the
        // restore target.
        const QRect candidate = state.testFlag(Qt::WindowMaximized)
            ? m_window->normalGeometry()
            : m_window->geometry();
        if (candidate.isValid() && !candidate.isEmpty())
            m_lastNormalGeometry = candidate;
    }
    if (!m_lastNormalGeometry.isValid() || m_lastNormalGeometry.isEmpty())
        m_lastNormalGeometry = m_window->geometry();

    QScreen* screen = m_window->screen();
    if (!screen)
        screen = QGuiApplication::screenAt(m_lastNormalGeometry.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    m_settings->setWindowPlacement(
        m_lastNormalGeometry,
        screen ? screen->name() : QString(),
        state.testFlag(Qt::WindowMaximized));
}

bool GalleryWindowPlacement::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_window || !event)
        return QObject::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::Show:
        connectScreenTracking();
        scheduleSave();
        break;
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::WindowStateChange:
        scheduleSave();
        break;
    default:
        if (fluentIsDisplayScaleChangeEvent(event)) {
            QTimer::singleShot(0, this, [this]() {
                if (m_window)
                    applyScreenConstraints(m_window->screen(), true);
            });
        }
        break;
    }
    return QObject::eventFilter(watched, event);
}

QScreen* GalleryWindowPlacement::restoredScreen() const
{
    if (!m_settings)
        return QGuiApplication::primaryScreen();

    const QString preferredName = m_settings->windowScreenName();
    for (QScreen* screen : QGuiApplication::screens()) {
        if (screen && !preferredName.isEmpty() && screen->name() == preferredName)
            return screen;
    }

    const QRect saved = m_settings->windowNormalGeometry();
    if (saved.isValid()) {
        if (QScreen* screen = QGuiApplication::screenAt(saved.center()))
            return screen;
    }
    return QGuiApplication::primaryScreen();
}

void GalleryWindowPlacement::connectScreenTracking()
{
    if (m_screenTrackingConnected || !m_window || !m_window->windowHandle())
        return;
    m_screenTrackingConnected = true;
    connect(m_window->windowHandle(), &QWindow::screenChanged, this,
            [this](QScreen* screen) { applyScreenConstraints(screen, true); });
}

void GalleryWindowPlacement::applyScreenConstraints(QScreen* screen, bool constrainWindow)
{
    if (!m_window)
        return;
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect available = screen->availableGeometry();
    const QSize minimum = windowplacement::effectiveMinimumSize(available.size());
    m_window->setMinimumSize(minimum);
    const Qt::WindowStates state = m_window->windowState();
    if (!constrainWindow || state.testFlag(Qt::WindowMaximized)
        || state.testFlag(Qt::WindowMinimized)
        || state.testFlag(Qt::WindowFullScreen)) {
        scheduleSave();
        return;
    }

    const QRect constrained = windowplacement::constrainGeometry(
        m_window->geometry(), available, minimum);
    if (constrained != m_window->geometry()) {
        m_restoring = true;
        m_window->setGeometry(constrained);
        m_lastNormalGeometry = constrained;
        m_restoring = false;
    }
    scheduleSave();
}

void GalleryWindowPlacement::scheduleSave()
{
    if (m_saveTimer && !m_restoring)
        m_saveTimer->start();
}

} // namespace fluent::gallery
