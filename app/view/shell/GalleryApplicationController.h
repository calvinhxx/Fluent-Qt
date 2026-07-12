#ifndef GALLERYAPPLICATIONCONTROLLER_H
#define GALLERYAPPLICATIONCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <Qt>

#include <cstdint>

class QEvent;
class QMenu;
class QSystemTrayIcon;

namespace fluent::gallery {

class GalleryWindow;

/**
 * @brief Owns application close behavior and the platform status-area icon.
 * zh_CN: 统一管理应用关闭行为及平台状态区图标。
 */
class GalleryApplicationController final : public QObject {
public:
    explicit GalleryApplicationController(GalleryWindow* window,
                                          QObject* parent = nullptr);
    ~GalleryApplicationController() override;

    void restoreWindow();
    void openSettings();
    void requestQuit();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupStatusItem();
    void handleCloseRequested();
    void showCloseBehaviorDialog();
    void applyConfiguredCloseBehavior();
    void captureRestoreState();
    void completeWindowRestore(std::uint64_t generation, bool refreshNativeFrame);

    QPointer<GalleryWindow> m_window;
    QSystemTrayIcon* m_statusIcon = nullptr;
    QMenu* m_statusMenu = nullptr;
    Qt::WindowStates m_restoreState = Qt::WindowNoState;
    bool m_statusAreaAvailable = false;
    bool m_exitRequested = false;
    bool m_closeRequestScheduled = false;
    bool m_closePromptOpen = false;
    std::uint64_t m_restoreGeneration = 0;
};

} // namespace fluent::gallery

#endif // GALLERYAPPLICATIONCONTROLLER_H
