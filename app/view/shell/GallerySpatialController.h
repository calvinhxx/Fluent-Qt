#ifndef GALLERYSPATIALCONTROLLER_H
#define GALLERYSPATIALCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QPoint>
#include <QVariantMap>
#include <memory>

class QWidget;
namespace fluent::navigation {
class NavigationView;
}
namespace fluent::gallery {

/** @brief Drives Gallery 3D perspective depth and transitions based on Settings.
 * zh_CN: 按 Settings 控制 Gallery 的 3D 景深外观和转场效果。 */
class GallerySpatialController final : public QObject {
    Q_OBJECT
public:
    // Activate a short Spatial session to install native painting before building pages.
    // Hardware probing and GPU creation remain deferred to the controller's active session.
    static void prepareApplicationStyle();
    GallerySpatialController(QWidget* window, fluent::navigation::NavigationView* navigation);
    ~GallerySpatialController() override;
    bool transitionRunning() const;
    void cancelTransition();
    /** @brief Maps a widget's local coordinates to its projected position in the window.
     * zh_CN: 将控件局部坐标映射到窗口中实际投影显示的位置。 */
    QPoint projectedPosition(const QWidget* widget, const QPoint& position) const;
    /** @brief Returns cache size and opt-in frame timing diagnostics.
     * zh_CN: 返回缓存尺寸；帧计时统计仅在显式启用性能测量时收集。 */
    QVariantMap renderingStatistics() const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void disableSpatial(const QString& reason);
    void releaseOversizedPresentation();

private:
    void ensureRenderer();
    void checkRenderer();
    void startPresentation();
    void applyMode(bool enabled);
    struct Private;
    std::unique_ptr<Private> d;
};
} // namespace fluent::gallery
#endif
