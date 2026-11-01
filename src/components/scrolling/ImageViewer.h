#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <memory>

#include <QElapsedTimer>
#include <QPixmap>
#include <QPointF>
#include <QSizeF>
#include <QWidget>

#include "compatibility/InputEventCompat.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QImage;
class QVariantAnimation;

namespace fluent::scrolling {

class ImageViewerZoomBar;
class ImageViewerZoomIndicator;

namespace detail {
class ImageViewerPixmapCache;
}

/**
 * @brief Pan-and-zoom viewer for a single image.
 * zh_CN: 单张图片的平移缩放查看器。
 *
 * ImageViewer paints one caller-provided pixmap inside its viewport. Users zoom
 * around the pointer with the mouse wheel, Ctrl/Command + wheel or a trackpad
 * pinch, pan by dragging, and use +, -, 0, 1 and the arrow keys. An axis without
 * a pannable range ignores drag, scroll and arrow input so a parent such as
 * FlipView can handle it.
 * zh_CN: ImageViewer 在视口内绘制调用方提供的单张 pixmap。用户可用鼠标滚轮、
 * Ctrl/Command + 滚轮或触控板捏合围绕指针缩放，拖动平移，并使用 +、-、0、1
 * 和方向键。某一轴没有可平移范围时，拖动、滚动和方向键会交给父控件（例如 FlipView）。
 *
 * A zoom factor of 1.0 shows the pixmap at its device-independent size
 * (pixel size divided by devicePixelRatio).
 * zh_CN: 缩放因子 1.0 按 pixmap 的设备无关尺寸（像素尺寸除以 devicePixelRatio）显示。
 */
class ImageViewer : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT

    /**
     * @brief Image shown by the viewer; setting a new one resets the view.
     * zh_CN: 查看器显示的图片；设置新图片会复位视图。
     */
    Q_PROPERTY(QPixmap pixmap READ pixmap WRITE setPixmap NOTIFY pixmapChanged)
    /**
     * @brief Current zoom factor; 1.0 is the image's device-independent size.
     * zh_CN: 当前缩放因子；1.0 为图片的设备无关尺寸。
     */
    Q_PROPERTY(qreal zoomFactor READ zoomFactor WRITE setZoomFactor NOTIFY zoomFactorChanged)
    /**
     * @brief Smallest zoom factor; the fit factor stays reachable below it.
     * zh_CN: 最小缩放因子；低于它的适应比例仍然可以到达。
     */
    Q_PROPERTY(qreal minZoomFactor READ minZoomFactor WRITE setMinZoomFactor NOTIFY minZoomFactorChanged)
    /**
     * @brief Largest zoom factor; the fit factor stays reachable above it.
     * zh_CN: 最大缩放因子；高于它的适应比例仍然可以到达。
     */
    Q_PROPERTY(qreal maxZoomFactor READ maxZoomFactor WRITE setMaxZoomFactor NOTIFY maxZoomFactorChanged)
    /**
     * @brief Image point, in device-independent image pixels, shown at the viewport center.
     * zh_CN: 显示在视口中心的图片坐标点，单位为设备无关的图片像素。
     */
    Q_PROPERTY(QPointF viewCenter READ viewCenter WRITE setViewCenter NOTIFY viewCenterChanged)
    /**
     * @brief Whether the view follows a fit rule when the viewport resizes.
     * zh_CN: 视口尺寸变化时视图是否跟随适应规则。
     */
    Q_PROPERTY(FitMode fitMode READ fitMode WRITE setFitMode NOTIFY fitModeChanged)
    /**
     * @brief What an unmodified mouse wheel does.
     * zh_CN: 不按修饰键的鼠标滚轮执行的操作。
     */
    Q_PROPERTY(WheelAction wheelAction READ wheelAction WRITE setWheelAction NOTIFY wheelActionChanged)
    /**
     * @brief Visibility policy of the built-in zoom controls.
     * zh_CN: 内置缩放工具条的可见性策略。
     */
    Q_PROPERTY(ControlsVisibility controlsVisibility
        READ controlsVisibility
        WRITE setControlsVisibility
        NOTIFY controlsVisibilityChanged)
    /**
     * @brief Whether direct zoom input briefly shows the zoom percentage when the controls are hidden.
     * zh_CN: 工具条隐藏时，直接缩放输入是否短暂显示缩放比例。
     */
    Q_PROPERTY(bool zoomIndicatorEnabled
        READ isZoomIndicatorEnabled
        WRITE setZoomIndicatorEnabled
        NOTIFY zoomIndicatorEnabledChanged)
    /**
     * @brief Whether the viewer paints its rounded background and border.
     * zh_CN: 查看器是否绘制圆角背景和边框。
     */
    Q_PROPERTY(bool frameVisible READ isFrameVisible WRITE setFrameVisible NOTIFY frameVisibleChanged)
    /**
     * @brief Sampling used to scale the image; Fast keeps individual pixels sharp.
     * zh_CN: 缩放图片时的采样方式；Fast 可保持单个像素清晰。
     */
    Q_PROPERTY(Qt::TransformationMode transformationMode
        READ transformationMode
        WRITE setTransformationMode
        NOTIFY transformationModeChanged)

public:
    /**
     * @brief Fit rule followed by the view.
     * zh_CN: 视图遵循的适应规则。
     */
    enum class FitMode {
        Manual,   ///< Keeps the zoom factor and view center. zh_CN: 保持缩放因子和视图中心。
        Contain,  ///< Fits the whole image, enlarging small images. zh_CN: 完整适应视口，小图也会放大。
        ScaleDown ///< Fits large images and keeps small ones at 100%. zh_CN: 大图缩小适应，小图保持 100%。
    };
    Q_ENUM(FitMode)

    /**
     * @brief Action of an unmodified mouse wheel.
     * zh_CN: 不按修饰键的鼠标滚轮动作。
     */
    enum class WheelAction {
        Zoom, ///< Zooms around the pointer. zh_CN: 围绕指针缩放。
        Pan   ///< Pans, or passes the wheel to the parent without range. zh_CN: 平移；没有范围时交给父控件。
    };
    Q_ENUM(WheelAction)

    /**
     * @brief Visibility policy of the zoom controls.
     * zh_CN: 缩放工具条的可见性策略。
     */
    enum class ControlsVisibility {
        Hidden, ///< Never shown. zh_CN: 始终隐藏。
        Auto,   ///< Shown while hovered, focused or dragged. zh_CN: 悬停、聚焦或拖动时显示。
        Visible ///< Always shown while an image is set. zh_CN: 设置图片后始终显示。
    };
    Q_ENUM(ControlsVisibility)

    explicit ImageViewer(QWidget* parent = nullptr);
    ~ImageViewer() override;

    QPixmap pixmap() const { return m_pixmap; }
    void setPixmap(const QPixmap& pixmap);

    /**
     * @brief Converts an image to a pixmap and shows it.
     * zh_CN: 将图片转换为 pixmap 并显示。
     */
    void setImage(const QImage& image);

    /**
     * @brief Returns the image's device-independent size, or an empty size without an image.
     * zh_CN: 返回图片的设备无关尺寸；没有图片时返回空尺寸。
     */
    QSizeF imageSize() const { return m_imageSize; }

    /**
     * @brief Returns the factor that fits the whole image into the current viewport.
     * zh_CN: 返回在当前视口中完整显示图片所需的缩放因子。
     */
    qreal fitZoomFactor() const;

    qreal zoomFactor() const { return m_zoomFactor; }
    void setZoomFactor(qreal factor);

    qreal minZoomFactor() const { return m_minZoomFactor; }
    void setMinZoomFactor(qreal factor);

    qreal maxZoomFactor() const { return m_maxZoomFactor; }
    void setMaxZoomFactor(qreal factor);

    QPointF viewCenter() const { return m_viewCenter; }
    void setViewCenter(const QPointF& center);

    FitMode fitMode() const { return m_fitMode; }
    void setFitMode(FitMode mode);

    WheelAction wheelAction() const { return m_wheelAction; }
    void setWheelAction(WheelAction action);

    ControlsVisibility controlsVisibility() const { return m_controlsVisibility; }
    void setControlsVisibility(ControlsVisibility visibility);

    bool isZoomIndicatorEnabled() const { return m_zoomIndicatorEnabled; }
    void setZoomIndicatorEnabled(bool enabled);

    bool isFrameVisible() const { return m_frameVisible; }
    void setFrameVisible(bool visible);

    Qt::TransformationMode transformationMode() const { return m_transformationMode; }
    void setTransformationMode(Qt::TransformationMode mode);

    /**
     * @brief Zooms in to the next preset step around the viewport center.
     * zh_CN: 以视口中心为锚点放大到下一个预设档位。
     */
    Q_INVOKABLE void zoomIn(bool animated = false);
    /**
     * @brief Zooms out to the previous preset step around the viewport center.
     * zh_CN: 以视口中心为锚点缩小到上一个预设档位。
     */
    Q_INVOKABLE void zoomOut(bool animated = false);
    /**
     * @brief Zooms to a factor around the viewport center.
     * zh_CN: 以视口中心为锚点缩放到指定因子。
     */
    Q_INVOKABLE void zoomTo(qreal factor, bool animated = false);
    /**
     * @brief Zooms to a factor while keeping the image point under a viewport position fixed.
     * zh_CN: 缩放到指定因子，并保持视口位置下的图片点不动。
     */
    Q_INVOKABLE void zoomTo(qreal factor, const QPointF& viewportAnchor, bool animated = false);
    /**
     * @brief Shows the image at 100% around the viewport center.
     * zh_CN: 以视口中心为锚点按 100% 显示图片。
     */
    Q_INVOKABLE void zoomToActualSize(bool animated = false);
    /**
     * @brief Fits the whole image into the viewport and keeps fitting on resize.
     * zh_CN: 让整张图片适应视口，并在尺寸变化时保持适应。
     */
    Q_INVOKABLE void fitToView(bool animated = false);
    /**
     * @brief Returns to the initial view: large images fit, small images stay at 100%.
     * zh_CN: 回到初始视图：大图缩小适应，小图保持 100%。
     */
    Q_INVOKABLE void resetView(bool animated = false);
    /**
     * @brief Moves the visible region by viewport pixels; positive values reveal the right and bottom.
     * zh_CN: 按视口像素移动可见区域；正值显示更右侧和更下方的内容。
     */
    Q_INVOKABLE void panBy(qreal dx, qreal dy);

    /**
     * @brief Maps a viewport position to device-independent image coordinates.
     * zh_CN: 将视口坐标映射为设备无关的图片坐标。
     */
    QPointF mapToImage(const QPointF& viewportPoint) const;
    /**
     * @brief Maps device-independent image coordinates to a viewport position.
     * zh_CN: 将设备无关的图片坐标映射为视口坐标。
     */
    QPointF mapFromImage(const QPointF& imagePoint) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void pixmapChanged();
    void zoomFactorChanged(qreal factor);
    void minZoomFactorChanged(qreal factor);
    void maxZoomFactorChanged(qreal factor);
    void viewCenterChanged(const QPointF& center);
    void fitModeChanged(fluent::scrolling::ImageViewer::FitMode mode);
    void wheelActionChanged(fluent::scrolling::ImageViewer::WheelAction action);
    void controlsVisibilityChanged(fluent::scrolling::ImageViewer::ControlsVisibility visibility);
    void zoomIndicatorEnabledChanged(bool enabled);
    void frameVisibleChanged(bool visible);
    void transformationModeChanged(Qt::TransformationMode mode);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void onThemeUpdated() override;

private:
    enum class ChangeSource { Program, Input };

    struct ViewState {
        qreal zoom = 1.0;
        QPointF center;
    };

    bool hasImage() const;
    QPointF viewportCenter() const;
    QPointF imageCenter() const;
    qreal effectiveMinZoom() const;
    qreal effectiveMaxZoom() const;
    qreal clampedZoom(qreal factor) const;
    QPointF clampedCenter(qreal zoom, const QPointF& center) const;
    qreal fitTargetZoom(FitMode mode) const;
    bool canPanHorizontally() const;
    bool canPanVertically() const;
    qreal presetStep(bool zoomIn) const;

    void applyView(qreal zoom, const QPointF& center, FitMode mode, ChangeSource source);
    void zoomAround(qreal factor, const QPointF& viewportAnchor, bool animated, ChangeSource source);
    void fitWithMode(FitMode mode, bool animated, ChangeSource source);
    void stepZoom(bool zoomIn, bool animated, ChangeSource source);
    void toggleZoomAt(const QPointF& viewportPos);
    void panByInternal(const QPointF& delta, ChangeSource source);
    void animateTo(const ViewState& target, FitMode mode, const QPointF& anchor, bool anchored, ChangeSource source);
    void updateAnimationFrame(qreal progress);
    void stopViewAnimation();
    void finishViewAnimation();
    void reapplyConstraints();

    void updateCursorShape();
    void updateChrome();
    void layoutChrome();
    bool areControlsShown() const;
    void showZoomIndicator();
    QString percentText() const;

    bool handleNativeGesture(QEvent* event);
    bool handlePinchGesture(QEvent* event);
    bool shouldSuppressWheel() const;

    QPixmap m_pixmap;
    QSizeF m_imageSize;
    qreal m_zoomFactor = 1.0;
    qreal m_minZoomFactor = 0.1;
    qreal m_maxZoomFactor = 10.0;
    QPointF m_viewCenter;
    FitMode m_fitMode = FitMode::ScaleDown;
    WheelAction m_wheelAction = WheelAction::Zoom;
    ControlsVisibility m_controlsVisibility = ControlsVisibility::Auto;
    bool m_zoomIndicatorEnabled = true;
    bool m_frameVisible = true;
    Qt::TransformationMode m_transformationMode = Qt::SmoothTransformation;

    bool m_hovered = false;
    bool m_dragging = false;
    bool m_keyboardFocusVisible = false;
    QPointF m_lastDragPos;

    QVariantAnimation* m_viewAnimation = nullptr;
    ViewState m_animationFrom;
    ViewState m_animationTo;
    FitMode m_animationMode = FitMode::Manual;
    QPointF m_animationAnchor;
    QPointF m_animationImageAnchor;
    bool m_animationAnchored = false;
    ChangeSource m_animationSource = ChangeSource::Program;

    QElapsedTimer m_lastNativeZoomGestureTime;
    bool m_nativeZoomGestureActive = false;
    bool m_nativeZoomGestureHasZoomed = false;

    ImageViewerZoomBar* m_zoomBar = nullptr;
    ImageViewerZoomIndicator* m_zoomIndicator = nullptr;

    std::unique_ptr<detail::ImageViewerPixmapCache> m_pixmapCache;
};

} // namespace fluent::scrolling

#endif // IMAGEVIEWER_H
