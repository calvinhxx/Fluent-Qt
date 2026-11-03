#include "InterfaceZoomRepaintCompat_p.h"

#include <QApplication>
#include <QCoreApplication>
#include <QScopedValueRollback>
#include <QEvent>
#include <QGuiApplication>
#include <QHash>
#include <QPaintEvent>
#include <QSet>
#include <QTimer>
#include <QPointer>
#include <QThread>
#include <QWidget>
#include <QWindow>
#include <numeric>

#ifdef Q_OS_MACOS
#include <objc/message.h>
#include <objc/runtime.h>
#endif

#ifdef FLUENT_QT_INTERFACE_ZOOM_HAS_REPAINT_QUEUES
#include "InterfaceZoomLowDprCompat_p.h"
#include "WidgetSnapshotCompat_p.h"
#endif

namespace compatibility::detail {
namespace {
#ifdef FLUENT_QT_INTERFACE_ZOOM_HAS_REPAINT_QUEUES

qreal interfaceZoomNativePointFactor(QWindow* window)
{
#ifdef Q_OS_MACOS
    if (!window || !window->handle() || QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return 1.0;
    const auto sendObject = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend);
    const auto sendScale = reinterpret_cast<double (*)(id, SEL)>(objc_msgSend);
    const id view = reinterpret_cast<id>(window->winId());
    const id nativeWindow = sendObject(view, sel_registerName("window"));
    const double backingScale = nativeWindow ? sendScale(nativeWindow, sel_registerName("backingScaleFactor")) : 0;
    return backingScale > 0.0 ? window->devicePixelRatio() / backingScale : 1.0;
#else
    Q_UNUSED(window);
    return 1.0;
#endif
}

class InterfaceZoomRepaintGuard final : public QObject {
public:
    explicit InterfaceZoomRepaintGuard(QObject* parent) : QObject(parent)
    {
        setLowDprSnapshotRenderer(lowDprWidgetSnapshot);
    }

    ~InterfaceZoomRepaintGuard() override { setLowDprSnapshotRenderer(nullptr); }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        const auto type = event->type();
        if (type != QEvent::UpdateRequest && type != QEvent::Expose
            && !(type == QEvent::Paint && (m_nativePass || !m_lowDprWindows.isEmpty())))
            return false;
        auto* widget = qobject_cast<QWidget*>(watched);
        if (type == QEvent::Paint) {
            if (!widget || lowDprSnapshotDepth != 0)
                return false;
            QWidget* root = widget->window();
            if (!m_lowDprWindows.contains(root) || !isNativeRasterPaint(widget))
                return false;
            // Preserve captures performed by GPU renderers during the same sync.
            // zh_CN: 保留 GPU 渲染器在同次同步中的离屏抓图。
            if (root == m_nativePass) {
                if (QWidgetPrivate::get(widget)->renderToTexture)
                    return false;
                restorePresentedPixels(widget, root, static_cast<QPaintEvent*>(event)->region());
                return true;
            }
            // Qt's compositor unlock callback can sync without UpdateRequest.
            // Repair that native damage once; texture-only frames have no raster device.
            // zh_CN: Qt 合成器解锁可绕过 UpdateRequest 直接同步；单次修复该原生区域，纯纹理帧没有位图目标。
            queueNativeRepair(root,
                static_cast<QPaintEvent*>(event)->region().translated(widget->mapTo(root, QPoint())));
            return false;
        }
        if (m_handling)
            return false;
        if (event->type() == QEvent::Expose) {
            auto* handle = qobject_cast<QWindow*>(watched);
            if (!handle || !handle->isExposed() || handle->devicePixelRatio() >= 1.0)
                return false;
            for (QWidget* topLevel : QApplication::topLevelWidgets()) {
                if (topLevel->windowHandle() == handle) {
                    widget = topLevel;
                    break;
                }
            }
        } else if (event->type() != QEvent::UpdateRequest)
            return false;
        if (!widget || !widget->isWindow() || !widget->isVisible() || !widget->updatesEnabled())
            return false;
        QWindow* window = widget->windowHandle();
        // A visible but occluded window can still retain or paint damage. Align
        // it now, so exposing the window later cannot present stale edge pixels.
        // zh_CN: 可见但被遮挡的窗口仍可能保留或绘制脏区域，提前对齐，避免再显示时残留边缘像素。
        if (!window || !widget->backingStore() || widget->testAttribute(Qt::WA_PaintOnScreen))
            return false;
        const int pointGrid = interfaceZoomDamageGrid(interfaceZoomNativePointFactor(window));
        const bool lowDpr = window->devicePixelRatio() < 1.0;
        trackWindow(widget, lowDpr);
        if (pointGrid == 1 && !lowDpr)
            return false;
        if (lowDpr && (!widget->testAttribute(Qt::WA_Mapped) || interfaceZoomTexturesLocked(widget)))
            return false;
        if (auto* manager = QWidgetPrivate::get(widget)->maybeRepaintManager()) {
            QRegion damage = pendingInterfaceZoomRasterDamage(manager, widget);
            if (widget->backingStore()->size() != widget->size())
                damage = widget->rect();
            const int pixelGrid = lowDpr ? interfaceZoomDamageGrid(window->devicePixelRatio()) : 1;
            const int grid = pointGrid > 0 && pixelGrid > 0 ? std::lcm(pointGrid, pixelGrid) : 0;
            QRegion aligned;
            for (const QRect& rect : damage)
                aligned += alignedInterfaceZoomDamage(rect, grid, widget->rect());
            // BufferInvalid includes opaque descendants. A parent-only update
            // would leave their dirty edges unaligned after Qt's occlusion pass.
            // zh_CN: 使区域内的不透明子控件一并失效；仅更新父控件无法对齐它们的脏边缘。
            if (lowDpr && !aligned.isEmpty()) {
                // Let Qt maintain its dirty queues and GPU textures. Replace
                // native raster painting with the bounded unit-density tile.
                // zh_CN: Qt 继续管理脏队列与 GPU 纹理；原生位图改在有限大小的单位密度图块中绘制。
                QScopedValueRollback<bool> handling(m_handling, true);
                QPointer<QWidget> alive(widget);
                {
                    // Qt clears an alpha backing store before painting and flushes
                    // after it. This pass skips raster painting, so its flush would
                    // present a transparent hole until the tile below; on Windows the
                    // whole window can vanish for a frame. Keep the presented pixels.
                    // zh_CN: Qt 会先把带 alpha 的后备缓冲清为透明、绘制后再提交。此轮跳过位图绘制，
                    // 提交的透明空洞会一直显示到下面的图块完成，Windows 上整窗可能闪失一帧；保留已显示的像素。
                    QScopedValueRollback<QImage> presented(m_presented, presentedPixels(widget, aligned));
                    QScopedValueRollback<QPointer<QWidget>> nativePass(m_nativePass, widget);
                    QCoreApplication::sendEvent(watched, event);
                }
                if (alive && alive->isVisible() && alive->updatesEnabled())
                    repaintLowDprRegion(alive, aligned);
                return true;
            }
            invalidateInterfaceZoomRasterDamage(widget, aligned);
            return false;
        }
        return false;
    }

private:
    static QRect pixelBounds(const QRect& logical, qreal dpr)
    {
        return QRectF(QPointF(logical.topLeft()) * dpr, QSizeF(logical.size()) * dpr).toAlignedRect();
    }

    static QImage presentedPixels(QWidget* window, const QRegion& region)
    {
        // A deep copy only: Qt paints into the platform image after this returns.
        // zh_CN: 仅做深拷贝；返回后 Qt 会继续绘制平台图像。
        auto* platform = window->backingStore()->handle();
        const QImage image = platform ? platform->toImage() : QImage();
        if (image.isNull())
            return {};
        const QRect pixels = pixelBounds(region.boundingRect(), window->devicePixelRatioF()).adjusted(-1, -1, 1, 1)
            & image.rect();
        QImage copy = image.copy(pixels);
        // Platform captures can retain the native screen DPR instead of Gallery's
        // effective DPR. Restoration below addresses physical pixels directly.
        // zh_CN: 平台抓图可能保留屏幕原生 DPR，而非 Gallery 的有效 DPR；下方恢复直接使用物理像素。
        copy.setDevicePixelRatio(1.0);
        copy.setOffset(pixels.topLeft());
        return copy;
    }

    void restorePresentedPixels(QWidget* widget, QWidget* root, const QRegion& region) const
    {
        QPoint offset;
        auto* device = QWidgetPrivate::get(widget)->redirected(&offset);
        if (m_presented.isNull() || !device || device->devType() != QInternal::Image)
            return;
        auto* native = static_cast<QImage*>(device);
        const qreal dpr = root->devicePixelRatioF();
        QRegion clip;
        for (const QRect& rect : region.translated(widget->mapTo(root, QPoint())))
            clip += pixelBounds(rect, dpr);
        // Pixel coordinates over the borrowed buffer, as in repaintLowDprRegion.
        // zh_CN: 与 repaintLowDprRegion 相同，以像素坐标视图借用原生缓冲区。
        QImage pixels(native->bits(), native->width(), native->height(), native->bytesPerLine(), native->format());
        QPainter painter(&pixels);
        painter.setClipRegion(clip);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(m_presented.offset(), m_presented);
    }

    bool isNativeRasterPaint(QWidget* widget) const
    {
        QPoint offset;
        auto* data = QWidgetPrivate::get(widget);
        auto* redirected = data->redirected(&offset);
        if (!redirected || redirected->devType() != QInternal::Image || redirected->devicePixelRatioF() >= 1.0)
            return false;
        auto* store = widget->backingStore();
        if (!store || store->size().isEmpty())
            return false;
        auto* device = store->paintDevice();
        auto* shared = data->sharedPainter();
        return redirected == device && (!shared || !shared->isActive() || shared->device() == device);
    }

    void trackWindow(QWidget* widget, bool lowDpr)
    {
        if (!lowDpr) {
            m_lowDprWindows.remove(widget);
            m_repairs.remove(widget);
            return;
        }
        m_lowDprWindows.insert(widget);
        if (!m_watchedWindows.contains(widget)) {
            m_watchedWindows.insert(widget);
            connect(widget, &QObject::destroyed, this, [this, widget] {
                m_lowDprWindows.remove(widget);
                m_watchedWindows.remove(widget);
                m_repairs.remove(widget);
            });
        }
    }

    void queueNativeRepair(QWidget* widget, const QRegion& damage)
    {
        const bool queued = m_repairs.contains(widget);
        m_repairs[widget] |= damage;
        if (queued)
            return;
        const QPointer<QWidget> alive(widget);
        QTimer::singleShot(0, this, [this, alive] {
            if (!alive)
                return;
            const QRegion damage = m_repairs.take(alive);
            auto* window = alive->windowHandle();
            if (!window || !alive->isVisible() || !alive->updatesEnabled() || window->devicePixelRatio() >= 1.0)
                return;
            const int pointGrid = interfaceZoomDamageGrid(interfaceZoomNativePointFactor(window));
            const int pixelGrid = interfaceZoomDamageGrid(window->devicePixelRatio());
            const int grid = pointGrid > 0 && pixelGrid > 0 ? std::lcm(pointGrid, pixelGrid) : 0;
            QRegion aligned;
            for (const QRect& rect : damage)
                aligned += alignedInterfaceZoomDamage(rect, grid, alive->rect());
            QScopedValueRollback<bool> handling(m_handling, true);
            repaintLowDprRegion(alive, aligned);
        });
    }

    bool m_handling = false;
    QPointer<QWidget> m_nativePass;
    QImage m_presented;
    QSet<QWidget*> m_lowDprWindows;
    QSet<QWidget*> m_watchedWindows;
    QHash<QWidget*, QRegion> m_repairs;
};

#endif
} // namespace

void installInterfaceZoomRepaintGuard()
{
#ifdef FLUENT_QT_INTERFACE_ZOOM_HAS_REPAINT_QUEUES
    QObject* app = QCoreApplication::instance();
    if (!app || QThread::currentThread() != app->thread())
        return;
    static QPointer<InterfaceZoomRepaintGuard> guard;
    if (!guard) {
        guard = new InterfaceZoomRepaintGuard(app);
        app->installEventFilter(guard);
    }
#endif
}

} // namespace compatibility::detail
