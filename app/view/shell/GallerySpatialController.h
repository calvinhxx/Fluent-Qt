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

/** @brief Presents Gallery depth and transitions using the shared Settings mode.
 * zh_CN: 按 Settings 统一模式呈现 Gallery 的立体外观和转场。 */
class GallerySpatialController final : public QObject {
    Q_OBJECT
public:
    GallerySpatialController(QWidget* window, fluent::navigation::NavigationView* navigation);
    ~GallerySpatialController() override;
    bool transitionRunning() const;
    void cancelTransition();
    /** @brief Maps native control coordinates to their presented position in the window.
     * zh_CN: 将原生控件坐标映射到窗口中实际显示的位置。 */
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
