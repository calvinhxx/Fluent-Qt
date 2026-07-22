#ifndef GALLERYWINDOWPLACEMENT_H
#define GALLERYWINDOWPLACEMENT_H

#include <QObject>
#include <QRect>
#include <QSize>

class QEvent;
class QScreen;
class QTimer;

namespace fluent::gallery {

class GallerySettings;
class GalleryWindow;

namespace windowplacement {

QSize effectiveMinimumSize(const QSize& availableSize);
QSize recommendedInitialSize(const QSize& availableSize);
QRect constrainGeometry(const QRect& requested,
                        const QRect& available,
                        const QSize& minimumSize);
QRect restoredGeometry(const QRect& savedGeometry,
                       const QRect& available,
                       const QSize& minimumSize);

} // namespace windowplacement

/**
 * @brief Restores, constrains, and persists the Gallery top-level window.
 * zh_CN: 恢复、约束并持久化 Gallery 顶层窗口的位置与尺寸。
 */
class GalleryWindowPlacement final : public QObject {
public:
    explicit GalleryWindowPlacement(GalleryWindow* window,
                                    GallerySettings* settings,
                                    QObject* parent = nullptr);

    bool restore();
    void saveNow();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QScreen* restoredScreen() const;
    void connectScreenTracking();
    void applyScreenConstraints(QScreen* screen, bool constrainWindow);
    void scheduleSave();

    GalleryWindow* m_window = nullptr;
    GallerySettings* m_settings = nullptr;
    QTimer* m_saveTimer = nullptr;
    QRect m_lastNormalGeometry;
    bool m_screenTrackingConnected = false;
    bool m_restoring = false;
};

} // namespace fluent::gallery

#endif // GALLERYWINDOWPLACEMENT_H
