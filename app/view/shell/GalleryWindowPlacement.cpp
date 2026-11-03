#include "GalleryWindowPlacement.h"

#include <algorithm>

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QWindow>
#include <QWindowStateChangeEvent>

#include "compatibility/QtCompat.h"
#include "platform/GalleryPlatform.h"
#include "support/logging/Log.h"
#include "view/shell/GalleryWindow.h"
#include "view/shell/GalleryWindowMetrics.h"
#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {
namespace {

using AppWindowMetrics = metrics::AppWindow;

constexpr int kPlacementSaveDelayMs = 250;

int proportionalExtent(int available, int percentage, int preferredMinimum, int preferredMaximum)
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
    const int x = available.x() + leftReserve + (horizontalSlack - leftReserve) / 2;
    const int y = available.y() + verticalSlack / 2;
    return QRect(QPoint(x, y), size);
}

// Geometry uses the applied scale; a pending restart must not change it.
// zh_CN: 几何换算使用已生效倍率，等待重启的设置不得改变几何。
int placementZoomPercent(const GallerySettings* settings)
{
    Q_UNUSED(settings);
    return qRound(platform::appliedInterfaceZoom() * 100);
}

bool isNormalWindowState(Qt::WindowStates state)
{
    return !(state & (Qt::WindowMaximized | Qt::WindowMinimized | Qt::WindowFullScreen));
}

} // namespace

namespace windowplacement {

QSize effectiveMinimumSize(const QSize& availableSize, int zoomPercent)
{
    return AppWindowMetrics::minimumSize(availableSize, zoomPercent);
}

QSize recommendedInitialSize(const QSize& availableSize)
{
    if (!availableSize.isValid() || availableSize.isEmpty()) {
        return QSize(AppWindowMetrics::InitialWidth, AppWindowMetrics::InitialHeight);
    }

    return QSize(proportionalExtent(availableSize.width(), AppWindowMetrics::InitialWidthPercent,
                     AppWindowMetrics::InitialMinWidth, AppWindowMetrics::InitialMaxWidth),
        proportionalExtent(availableSize.height(), AppWindowMetrics::InitialHeightPercent,
            AppWindowMetrics::InitialMinHeight, AppWindowMetrics::InitialMaxHeight));
}

QRect constrainGeometry(const QRect& requested, const QRect& available, const QSize& minimumSize)
{
    if (!available.isValid() || available.isEmpty())
        return requested;

    const QSize effectiveMinimum(qMin(qMax(1, minimumSize.width()), available.width()),
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

QRect restoredGeometry(const QRect& savedGeometry, const QRect& available, const QSize& minimumSize)
{
    // Scale conversion rounds geometry, while minimum bounds round upwards.
    // A one-pixel difference must clamp the saved bounds, not discard placement.
    // zh_CN: 几何换算四舍五入，而最小尺寸向上取整；一像素误差应钳制，不应丢弃位置。
    const bool savedSizeIsUsable = savedGeometry.width() >= minimumSize.width() - 1
        && savedGeometry.height() >= minimumSize.height() - 1;
    if (savedGeometry.isValid() && !savedGeometry.isEmpty() && savedSizeIsUsable) {
        return constrainGeometry(savedGeometry, available, minimumSize);
    }

    return centeredInitialGeometry(available, recommendedInitialSize(available.size()));
}

} // namespace windowplacement

GalleryWindowPlacement::GalleryWindowPlacement(GalleryWindow* window, GallerySettings* settings, QObject* parent)
    : QObject(parent), m_window(window), m_settings(settings), m_saveTimer(new QTimer(this))
{
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(kPlacementSaveDelayMs);
    connect(m_saveTimer, &QTimer::timeout, this, &GalleryWindowPlacement::saveNow);
    if (m_window) {
        m_window->installEventFilter(this);
        rememberNormalGeometry(m_window->normalGeometry());
    }
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
    const QSize minimum = windowplacement::effectiveMinimumSize(available.size(), placementZoomPercent(m_settings));
    m_window->setMinimumSize(minimum);

    const QRect saved = m_settings->windowNormalGeometry();
    const qreal ratio = qreal(m_settings->windowGeometryZoomPercent()) / placementZoomPercent(m_settings);
    const QPoint origin = screen->geometry().topLeft();
    const QPoint offset = saved.topLeft() - origin;
    const QRect scaled(origin + QPoint(qRound(offset.x() * ratio), qRound(offset.y() * ratio)),
        QSize(qRound(saved.width() * ratio), qRound(saved.height() * ratio)));
    const QRect target = windowplacement::restoredGeometry(scaled, available, minimum);

    m_window->setGeometry(target);
    rememberNormalGeometry(target);
    m_restoring = false;
    LOG_INFO(QStringLiteral("GalleryWindowPlacement restored geometry=%1,%2 %3x%4 screen=%5 maximized=%6")
            .arg(target.x())
            .arg(target.y())
            .arg(target.width())
            .arg(target.height())
            .arg(screen->name())
            .arg(m_settings->windowMaximized()));
    return m_settings->windowMaximized();
}

void GalleryWindowPlacement::saveNow()
{
    if (!m_window || !m_settings || m_restoring)
        return;

    const Qt::WindowStates state = m_window->windowState();
    // Qt keeps normalGeometry() at its old scale while maximized. Track normal
    // placement ourselves and convert only when saving, avoiding cumulative rounding.
    // zh_CN: 最大化时 Qt 的 normalGeometry() 仍使用旧倍率。独立记录普通窗口位置，
    // 仅在保存时换算，避免连续缩放累积舍入误差。
    if (isNormalWindowState(state))
        rememberNormalGeometry(m_window->geometry());
    if (!m_lastNormalGeometry.isValid() || m_lastNormalGeometry.isEmpty())
        rememberNormalGeometry(m_window->geometry());

    const QRect geometry = normalGeometryAtCurrentZoom();

    QScreen* screen = m_window->screen();
    if (!screen)
        screen = QGuiApplication::screenAt(geometry.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    m_settings->setWindowPlacement(geometry, screen ? screen->name() : QString(), state.testFlag(Qt::WindowMaximized));
}

bool GalleryWindowPlacement::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_window || !event)
        return QObject::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::Show:
        connectScreenTracking();
        if (isNormalWindowState(m_window->windowState()))
            rememberNormalGeometry(m_window->geometry());
        scheduleSave();
        break;
    case QEvent::Move:
    case QEvent::Resize:
        if (isNormalWindowState(m_window->windowState()))
            rememberNormalGeometry(m_window->geometry());
        scheduleSave();
        break;
    case QEvent::WindowStateChange:
        // Show/move/resize already recorded a visible window's client bounds.
        // Keep that snapshot: Qt's state transition can replace normalGeometry()
        // with frame-adjusted bounds. Hidden windows may still have pending
        // geometry events, so capture their normal target here.
        // zh_CN: 显示/移动/缩放已记录可见窗口的客户区，保留该快照，避免状态切换时 Qt 的窗框换算改写它；
        // 隐藏窗口可能仍有待投递的几何事件，在此捕获其普通窗口目标。
        if (isNormalWindowState(static_cast<QWindowStateChangeEvent*>(event)->oldState())) {
            if (!m_window->isVisible() || !m_lastNormalGeometry.isValid())
                rememberNormalGeometry(m_window->normalGeometry());
        } else if (isNormalWindowState(m_window->windowState()))
            rememberNormalGeometry(m_window->geometry());
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

void GalleryWindowPlacement::rememberNormalGeometry(const QRect& geometry)
{
    if (!geometry.isValid() || geometry.isEmpty())
        return;
    m_lastNormalGeometry = geometry;
    m_normalGeometryZoomPercent = placementZoomPercent(m_settings);
    const QScreen* screen = m_window ? m_window->screen() : nullptr;
    m_normalGeometryScreenOrigin = screen ? screen->geometry().topLeft() : QPoint();
}

QRect GalleryWindowPlacement::normalGeometryAtCurrentZoom() const
{
    const qreal ratio = qreal(m_normalGeometryZoomPercent) / placementZoomPercent(m_settings);
    const QPoint offset = m_lastNormalGeometry.topLeft() - m_normalGeometryScreenOrigin;
    // Global Qt scaling preserves each screen's origin; scale coordinates relative to it.
    // zh_CN: Qt 全局缩放保留各屏幕原点，位置需相对该原点换算。
    return QRect(m_normalGeometryScreenOrigin + QPoint(qRound(offset.x() * ratio), qRound(offset.y() * ratio)),
        QSize(qRound(m_lastNormalGeometry.width() * ratio), qRound(m_lastNormalGeometry.height() * ratio)));
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
    connect(m_window->windowHandle(), &QWindow::screenChanged, this, [this](QScreen* screen) {
        applyScreenConstraints(screen, true);
    });
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
    const QSize minimum = windowplacement::effectiveMinimumSize(available.size(), placementZoomPercent(m_settings));
    m_window->setMinimumSize(minimum);
    const Qt::WindowStates state = m_window->windowState();
    if (!constrainWindow || state.testFlag(Qt::WindowMaximized) || state.testFlag(Qt::WindowMinimized)
        || state.testFlag(Qt::WindowFullScreen)) {
        scheduleSave();
        return;
    }

    const QRect constrained = windowplacement::constrainGeometry(m_window->geometry(), available, minimum);
    if (constrained != m_window->geometry()) {
        m_restoring = true;
        m_window->setGeometry(constrained);
        rememberNormalGeometry(constrained);
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
