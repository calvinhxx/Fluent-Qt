#include "components/foundation/overlay/OverlayCoordinator.h"

#include <QEvent>

#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayWindow.h"

namespace fluent::overlay {

OverlayCoordinator::OverlayCoordinator(QWidget* overlay, QObject* parent)
    : QObject(parent),
      m_overlay(overlay)
{
}

OverlayCoordinator::~OverlayCoordinator()
{
    if (m_topLevel)
        m_topLevel->removeEventFilter(this);
    releaseScrim(ScrimDeletion::Immediate);
}

void OverlayCoordinator::attachTo(QWidget* topLevel)
{
    if (!topLevel)
        return;

    if (m_topLevel != topLevel) {
        if (m_topLevel)
            m_topLevel->removeEventFilter(this);
        m_topLevel = topLevel;
        m_topLevel->installEventFilter(this);
    }

    attachToTopLevel(m_overlay.data(), topLevel);
    if (m_scrim)
        attachToTopLevel(m_scrim.data(), topLevel);
    syncScrimGeometry();
}

void OverlayCoordinator::detach()
{
    if (m_topLevel)
        m_topLevel->removeEventFilter(this);
    m_topLevel = nullptr;
}

OverlayScrim* OverlayCoordinator::ensureScrim(const QString& objectName)
{
    if (!m_topLevel)
        return nullptr;

    if (!m_scrim) {
        m_scrim = new OverlayScrim(m_topLevel.data(), objectName);
        connect(m_scrim, &OverlayScrim::pressed,
                this, &OverlayCoordinator::scrimPressed);
    } else {
        attachToTopLevel(m_scrim.data(), m_topLevel.data());
    }

    syncScrimGeometry();
    return m_scrim.data();
}

void OverlayCoordinator::releaseScrim(ScrimDeletion deletion)
{
    if (!m_scrim)
        return;

    OverlayScrim* scrimWidget = m_scrim.data();
    m_scrim = nullptr;
    scrimWidget->hide();
    if (deletion == ScrimDeletion::Immediate)
        delete scrimWidget;
    else
        scrimWidget->deleteLater();
}

void OverlayCoordinator::syncScrimGeometry()
{
    if (m_scrim && m_topLevel)
        m_scrim->setGeometry(overlaySurfaceRect(m_topLevel.data()));
}

void OverlayCoordinator::raiseStack()
{
    raiseOverlayStack(m_scrim.data(), m_overlay.data());
}

bool OverlayCoordinator::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_topLevel || !event)
        return QObject::eventFilter(watched, event);

    if (event->type() == QEvent::Destroy) {
        m_scrim = nullptr;
        m_topLevel = nullptr;
        emit hostDestroyed();
        return false;
    }

    if (event->type() == QEvent::Resize) {
        syncScrimGeometry();
        raiseStack();
        emit hostGeometryChanged();
    }
    return false;
}

} // namespace fluent::overlay
