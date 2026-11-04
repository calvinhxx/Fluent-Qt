#include <gtest/gtest.h>
#include "components/foundation/overlay/OverlayScrim.h"
#include <QApplication>
#include <QBackingStore>
#include <QElapsedTimer>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPaintEvent>
#include <QPainter>
#include <QScreen>
#include <QTest>
#include <QWidget>
#include <QWindow>
#ifdef FLUENT_QT_ZOOM_TEST_HAS_OPENGL_WIDGET
#include "OpenGLTestEnvironment.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#endif
#ifdef FLUENT_QT_HAS_INTERFACE_ZOOM
#include <qpa/qplatformbackingstore.h>
#endif
#include <limits>
#include <memory>
#include <thread>

#include "support/interfacezoom/InterfaceZoom.h"
#include "src/compatibility/private/InterfaceZoomRepaintCompat_p.h"
#include "src/compatibility/private/WidgetSnapshotCompat_p.h"
#include "compatibility/PaintCompat.h"
#ifdef FLUENT_QT_INTERFACE_ZOOM_HAS_REPAINT_QUEUES
#include "src/compatibility/private/InterfaceZoomLowDprCompat_p.h"
#endif

namespace zoom = fluent::support::interfacezoom;

TEST(InterfaceZoomRepaintTest, ExplicitPaintDevicesKeepHalfDprGeometry)
{
    for (qreal dpr : {0.5, 0.67, 1.0, 1.25}) {
        SCOPED_TRACE(dpr);
        QImage image(QSizeF(QSize(80, 60) * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        fluentSetRenderTargetDevicePixelRatio(image, dpr);
        {
            QPainter painter(&image);
            fluentApplyRenderTargetScale(painter, dpr);
            painter.fillRect(QRect(20, 10, 20, 20), Qt::red);
        }
        EXPECT_EQ(image.pixelColor(QPointF(QPointF(30, 20) * dpr).toPoint()), QColor(Qt::red));
        EXPECT_EQ(image.pixelColor(QPointF(QPointF(50, 20) * dpr).toPoint()), QColor(Qt::white));
        EXPECT_EQ(image.pixelColor(QPointF(QPointF(10, 20) * dpr).toPoint()), QColor(Qt::white));
        QImage cache(image.size(), QImage::Format_ARGB32_Premultiplied);
        cache.setDevicePixelRatio(dpr);
        cache.fill(Qt::white);
        {
            FluentRasterCachePainter painter(&cache);
            painter.fillRect(QRect(20, 10, 20, 20), Qt::red);
        }
        EXPECT_EQ(cache.pixelColor(QPointF(QPointF(30, 20) * dpr).toPoint()), QColor(Qt::red));
        EXPECT_EQ(cache.pixelColor(QPointF(QPointF(50, 20) * dpr).toPoint()), QColor(Qt::white));
        EXPECT_EQ(cache.pixelColor(QPointF(QPointF(10, 20) * dpr).toPoint()), QColor(Qt::white));
        EXPECT_DOUBLE_EQ(cache.devicePixelRatio(), dpr);
    }
}

TEST(InterfaceZoomRepaintTest, BitmapCachesCoverTheirLogicalExtent)
{
    for (qreal dpr : {0.5, 0.75, 1.0, 2.0}) {
        SCOPED_TRACE(dpr);
        QImage cache(QSize(80, 60) * dpr, QImage::Format_ARGB32_Premultiplied);
        cache.fill(Qt::blue);
        cache.setDevicePixelRatio(dpr);
        for (bool pixmap : {false, true}) {
            SCOPED_TRACE(pixmap);
            QImage result(90, 70, QImage::Format_ARGB32_Premultiplied);
            result.fill(Qt::white);
            {
                QPainter painter(&result);
                if (pixmap)
                    fluentDrawPixmapAt(painter, QPointF(3, 5), QPixmap::fromImage(cache));
                else
                    fluentDrawImageAt(painter, QPointF(3, 5), cache);
            }
            EXPECT_EQ(result.pixelColor(4, 6), QColor(Qt::blue));
            EXPECT_EQ(result.pixelColor(81, 63), QColor(Qt::blue));
            EXPECT_EQ(result.pixelColor(84, 66), QColor(Qt::white));
        }
    }
}

TEST(InterfaceZoomRepaintTest, HalfDprCoverCropUsesTheWholeSource)
{
    QImage source(40, 30, QImage::Format_ARGB32_Premultiplied);
    source.fill(Qt::red);
    {
        QPainter painter(&source);
        painter.fillRect(QRect(20, 0, 20, 30), Qt::blue);
    }
    source.setDevicePixelRatio(0.5);
    QImage result(30, 30, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::white);
    {
        QPainter painter(&result);
        fluentDrawCoverPixmapInLogicalRect(painter, QRectF(result.rect()), QPixmap::fromImage(source));
    }
    EXPECT_EQ(result.pixelColor(2, 15), QColor(Qt::red));
    EXPECT_EQ(result.pixelColor(27, 15), QColor(Qt::blue));
}

TEST(InterfaceZoomRepaintTest, DamageAlignmentCoversFractionalEdgesWithoutExpandingToTheWholeWindow)
{
    using namespace compatibility::detail;
    const QRect bounds(0, 0, 1200, 800);
    const QRect damage(317, 213, 201, 153);
    for (qreal factor : {0.5, 0.67, 0.75, 0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0}) {
        SCOPED_TRACE(factor);
        const int grid = interfaceZoomDamageGrid(factor);
        ASSERT_GT(grid, 0);
        const QRect aligned = alignedInterfaceZoomDamage(damage, grid, bounds);
        EXPECT_TRUE(aligned.contains(damage));
        EXPECT_LT(aligned.width() * aligned.height(), bounds.width() * bounds.height() / 4);
        for (int edge : {aligned.x(), aligned.y(), aligned.x() + aligned.width(), aligned.y() + aligned.height()})
            EXPECT_NEAR(edge * factor, std::round(edge * factor), 1e-7);
        EXPECT_EQ(alignedInterfaceZoomDamage(QRect(-10, -10, 90, 90), grid, bounds).topLeft(), QPoint());
        EXPECT_TRUE(alignedInterfaceZoomDamage(QRect(1300, 900, 10, 10), grid, bounds).isEmpty());
    }
    EXPECT_EQ(interfaceZoomDamageGrid(1.0), 1);
    EXPECT_EQ(interfaceZoomDamageGrid(0.67), 100);
    EXPECT_EQ(interfaceZoomDamageGrid(0.75), 4);
    EXPECT_EQ(interfaceZoomDamageGrid(std::numeric_limits<qreal>::quiet_NaN()), 0);
    EXPECT_EQ(alignedInterfaceZoomDamage(damage, 0, bounds), bounds);
}

TEST(InterfaceZoomRepaintTest, SparseDamageDoesNotAllocateAWholeWindowTile)
{
    using namespace compatibility::detail;
    const QRect first(10, 10, 20, 20);
    const QRect second(1800, 1000, 20, 20);
    const QRegion damage = QRegion(first) + second;
    const auto tiles = interfaceZoomRasterTiles(damage);
    ASSERT_EQ(tiles.size(), 2);
    QRegion covered;
    qint64 area = 0;
    for (const auto& tile : tiles) {
        covered |= tile;
        area += qint64(tile.width()) * tile.height();
    }
    EXPECT_EQ(covered, damage);
    EXPECT_EQ(area, 800);
    EXPECT_EQ(interfaceZoomRasterTiles(QRegion()).size(), 0);
    EXPECT_EQ(interfaceZoomRasterTiles(QRegion(QRect(0, 0, 640, 480))).size(), 1);
}

namespace {
class PartialPaintProbe final : public QWidget {
public:
    explicit PartialPaintProbe(QWidget* parent = nullptr) : QWidget(parent) { setAttribute(Qt::WA_OpaquePaintEvent); }
    QColor background = QColor(238, 238, 238);
    QRect marker;
    qint64 lastDamageArea = 0;
    int paints = 0;

    void advance(int x)
    {
        const QRect old = marker;
        marker = QRect(x, 73, 131, 169);
        update(old.united(marker).adjusted(-1, -1, 1, 1));
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        lastDamageArea = 0;
        for (const QRect& rect : event->region())
            lastDamageArea += qint64(rect.width()) * rect.height();
        ++paints;
        QPainter painter(this);
        painter.fillRect(rect(), background);
        painter.fillRect(marker, QColor(102, 102, 102));
    }
};
#ifdef FLUENT_QT_ZOOM_TEST_HAS_OPENGL_WIDGET
class TexturePaintProbe final : public QOpenGLWidget {
public:
    using QOpenGLWidget::QOpenGLWidget;
    int paints = 0;
    QWidget* captureSource = nullptr;
    QColor capturedColor;

protected:
    void paintGL() override
    {
        auto* functions = context()->functions();
        functions->glClearColor(0.0f, float(++paints % 20) / 20.0f, 0.0f, 1.0f);
        functions->glClear(GL_COLOR_BUFFER_BIT);
        if (captureSource) {
            QImage capture(captureSource->size(), QImage::Format_ARGB32_Premultiplied);
            capture.fill(Qt::transparent);
            captureSource->render(&capture);
            capturedColor = capture.pixelColor(capture.rect().center());
        }
    }
};
#endif
} // namespace

#ifdef FLUENT_QT_INTERFACE_ZOOM_HAS_REPAINT_QUEUES
TEST(InterfaceZoomRepaintTest, AlignedDamageIncludesOpaqueChildrenAndKeepsUnrelatedPixelsClean)
{
    QWidget window;
    window.resize(360, 140);
    PartialPaintProbe first(&window), second(&window), unrelated(&window);
    first.setGeometry(10, 20, 70, 80);
    second.setGeometry(100, 20, 70, 80);
    unrelated.setGeometry(260, 20, 70, 80);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(QTest::qWaitFor([&] { return first.paints && second.paints && unrelated.paints; }));
    // Native exposure/activation can queue another complete paint after the
    // first one. Start partial-damage assertions only when those paints settle.
    // zh_CN: 原生显示和激活可在首帧后追加整窗绘制；待其稳定后再断言局部重绘。
    int previousPaints = first.paints + second.paints + unrelated.paints;
    QElapsedTimer quiet;
    quiet.start();
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            const int paints = first.paints + second.paints + unrelated.paints;
            if (paints != previousPaints) {
                previousPaints = paints;
                quiet.restart();
            }
            return quiet.elapsed() >= 100;
        },
        2000));
    const int firstBefore = first.paints;
    const int secondBefore = second.paints;
    const int unrelatedBefore = unrelated.paints;
    first.update(QRect(5, 5, 6, 6));
    ASSERT_TRUE(QTest::qWaitFor([&] { return first.paints > firstBefore; }));
    ASSERT_EQ(second.paints, secondBefore);
    ASSERT_EQ(unrelated.paints, unrelatedBefore);

    class AlignPendingDamage final : public QObject {
    public:
        bool applied = false;

    protected:
        bool eventFilter(QObject* object, QEvent* event) override
        {
            if (!applied && event->type() == QEvent::UpdateRequest) {
                applied = true;
                compatibility::detail::invalidateInterfaceZoomRasterDamage(static_cast<QWidget*>(object),
                    QRegion(QRect(0, 0, 190, 120)));
            }
            return false;
        }
    } align;
    window.installEventFilter(&align);
    first.update(QRect(5, 5, 6, 6));
    ASSERT_TRUE(QTest::qWaitFor([&] { return align.applied && second.paints > secondBefore; }));
    EXPECT_EQ(second.lastDamageArea, qint64(second.width()) * second.height());
    EXPECT_EQ(unrelated.paints, unrelatedBefore);
}
#endif

TEST(InterfaceZoomRepaintTest, NativePartialRepaintsMatchACompleteFrame)
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")
        || QGuiApplication::platformName() == QStringLiteral("minimal"))
        GTEST_SKIP() << "Requires a native window/backing-store capture; QWidget::grab alone hides this regression.";
    ASSERT_TRUE(zoom::isSupported());
#ifdef FLUENT_QT_HAS_INTERFACE_ZOOM
    ASSERT_TRUE(zoom::apply(1.0));
    struct Restore {
        ~Restore() { zoom::apply(1.0); }
    } restore;
    const auto verifySurface = [&](bool gpuBacked, bool alphaSurface) {
        SCOPED_TRACE(::testing::Message() << "gpuBacked=" << gpuBacked << " alpha=" << alphaSurface);
        PartialPaintProbe window;
        window.setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        window.setAttribute(Qt::WA_ShowWithoutActivating);
        window.setAttribute(Qt::WA_TranslucentBackground, alphaSurface);
        window.resize(640, 480);
        PartialPaintProbe capturedChild(&window);
        capturedChild.background = QColor(20, 80, 200);
        capturedChild.setGeometry(530, 5, 18, 18);
#ifdef FLUENT_QT_ZOOM_TEST_HAS_OPENGL_WIDGET
        std::unique_ptr<TexturePaintProbe> gpu;
        if (gpuBacked) {
            gpu = std::make_unique<TexturePaintProbe>(&window);
            gpu->setGeometry(20, 60, 80, 80);
            gpu->captureSource = &capturedChild;
        }
#endif
        // Like Gallery's startup cover, this opaque sibling covers the GPU content.
        // zh_CN: 与 Gallery 启动遮罩一样，让不透明同级控件覆盖 GPU 内容。
        PartialPaintProbe child(&window);
        child.setGeometry(7, 41, window.width() - 15, window.height() - 50);
        QWidget disabledChild(&window);
        disabledChild.setGeometry(5, 5, 10, 10);
        disabledChild.setUpdatesEnabled(false);
        window.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
#ifdef FLUENT_QT_ZOOM_TEST_HAS_OPENGL_WIDGET
        if (gpu) {
            ASSERT_TRUE(QTest::qWaitFor([&] { return gpu->isValid(); }));
            ASSERT_NE(window.windowHandle()->surfaceType(), QSurface::RasterSurface);
        }
#endif
        SCOPED_TRACE(::testing::Message() << "surfaceType=" << int(window.windowHandle()->surfaceType()));
        for (qreal factor : {0.5, 0.67, 0.75, 0.9, 1.1, 1.25, 1.5, 1.0}) {
            SCOPED_TRACE(::testing::Message() << "zoom=" << factor << " alpha=" << alphaSurface);
            ASSERT_TRUE(zoom::apply(factor));
            child.setGeometry(7, 41, window.width() - 15, window.height() - 50);
            window.background = child.background = alphaSurface ? QColor(36, 36, 36) : QColor(238, 238, 238);
            window.update();
            child.update();
            QTest::qWait(50);
            for (int frame = 0; frame < 20; ++frame) {
                child.advance(113 + frame * 3);
#ifdef FLUENT_QT_ZOOM_TEST_HAS_OPENGL_WIDGET
                if (gpu)
                    gpu->update();
#endif
                QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
                QTest::qWait(20);
            }
            ASSERT_TRUE(window.windowHandle()->isExposed())
                << "Native capture requires an exposed window; run native GUI tests serially.";
#ifdef FLUENT_QT_INTERFACE_ZOOM_HAS_REPAINT_QUEUES
            EXPECT_LT(child.lastDamageArea, child.width() * child.height() / 2)
                << "Mixed GPU/raster frames must retain partial repaints.";
#endif
            const QRect geometry = window.geometry();
            ASSERT_NE(window.backingStore(), nullptr);
            ASSERT_NE(window.backingStore()->handle(), nullptr);
            // Read the native buffer without forcing a repaint or capturing other apps.
            // zh_CN: 读取实际原生缓冲区，不强制重绘，也不截图遮住测试窗口的其他应用。
            const QImage actual = window.backingStore()->handle()->toImage().convertToFormat(QImage::Format_ARGB32);
            if (actual.isNull())
                GTEST_SKIP() << "This native platform cannot capture its window backing store.";
            QImage expected;
            if (window.devicePixelRatioF() < 1.0) {
                // Old Qt's grab() has the same low-DPR defect as its original
                // native pass. Compare partial updates with a full native repaint.
                // zh_CN: 旧 Qt 的 grab() 同样受低 DPR 缺陷影响；以完整原生重绘对照局部刷新。
                window.update();
                QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
                QTest::qWait(50);
                expected = window.backingStore()->handle()->toImage().convertToFormat(QImage::Format_ARGB32);
                const QPoint marker = child.mapTo(&window, child.marker.center());
                const QPoint pixel = QPointF(QPointF(marker) * window.devicePixelRatioF()).toPoint();
                EXPECT_EQ(actual.pixelColor(pixel), QColor(102, 102, 102));
            } else {
                expected = window.grab().toImage().convertToFormat(QImage::Format_ARGB32);
            }
            // The native frame and QWidget round the outer fractional pixel separately.
            // Compare every pixel in their common canvas; do not rescale either image.
            // zh_CN: 原生外框与 QWidget 会分别取整最外侧分数像素，逐像素比较共同画布，不缩放截图。
            ASSERT_LE(qAbs(actual.width() - expected.width()), 1);
            ASSERT_LE(qAbs(actual.height() - expected.height()), 1);
            const QSize common(qMin(actual.width(), expected.width()), qMin(actual.height(), expected.height()));
            int mismatches = 0;
            for (int y = 0; y < common.height(); ++y) {
                const auto* a = reinterpret_cast<const QRgb*>(actual.constScanLine(y));
                const auto* b = reinterpret_cast<const QRgb*>(expected.constScanLine(y));
                for (int x = 0; x < common.width(); ++x)
                    if (qAbs(qRed(a[x]) - qRed(b[x])) > 2 || qAbs(qGreen(a[x]) - qGreen(b[x])) > 2
                        || qAbs(qBlue(a[x]) - qBlue(b[x])) > 2 || qAbs(qAlpha(a[x]) - qAlpha(b[x])) > 2)
                        ++mismatches;
            }
            EXPECT_EQ(mismatches, 0) << "Incremental native rendering must match a fresh frame.";
            EXPECT_EQ(window.geometry(), geometry);
            EXPECT_FALSE(disabledChild.updatesEnabled());
            const int paints = child.paints;
            QTest::qWait(80);
            EXPECT_EQ(child.paints, paints) << "The repaint guard must not schedule a continuous repaint loop.";
#if defined(FLUENT_QT_ZOOM_TEST_HAS_OPENGL_WIDGET) && defined(FLUENT_QT_INTERFACE_ZOOM_HAS_REPAINT_QUEUES)
            if (gpu) {
                const int texturePaints = gpu->paints;
                const int windowPaints = window.paints;
                for (int frame = 0; frame < 5; ++frame) {
                    gpu->update();
                    QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
                    QTest::qWait(20);
                }
                EXPECT_GT(gpu->paints, texturePaints);
                EXPECT_EQ(gpu->capturedColor, capturedChild.background)
                    << "GPU captures must keep receiving widget paint events.";
                EXPECT_EQ(child.paints, paints) << "Pure GPU frames must not invalidate raster children.";
                EXPECT_EQ(window.paints, windowPaints) << "Pure GPU frames must not invalidate the window background.";
            }
#endif
        }
    };
#ifdef FLUENT_QT_ZOOM_TEST_HAS_OPENGL_WIDGET
    const bool canPaintTextures = tests::support::nativeOpenGLContextUnavailableReason().isEmpty();
#endif
    for (bool alphaSurface : {false, true}) {
        verifySurface(false, alphaSurface);
#ifdef FLUENT_QT_ZOOM_TEST_HAS_OPENGL_WIDGET
        if (canPaintTextures)
            verifySurface(true, alphaSurface);
#endif
    }
#endif
}

TEST(InterfaceZoomRepaintTest, NativeHalfDprSparsePatchesMatchACompleteFrame)
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")
        || QGuiApplication::platformName() == QStringLiteral("minimal"))
        GTEST_SKIP() << "Requires a native backing-store capture";
    ASSERT_TRUE(zoom::isSupported());
#ifdef FLUENT_QT_INTERFACE_ZOOM_HAS_REPAINT_QUEUES
    ASSERT_TRUE(zoom::apply(1.0));
    struct Restore {
        ~Restore() { zoom::apply(1.0); }
    } restore;
    PartialPaintProbe window;
    window.setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    window.resize(640, 480);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(zoom::apply(0.5 / window.devicePixelRatioF()));
    ASSERT_NEAR(window.devicePixelRatioF(), 0.5, 1e-6);
    PartialPaintProbe first(&window), second(&window);
    first.setGeometry(20, 20, 24, 24);
    second.setGeometry(window.width() - 44, window.height() - 44, 24, 24);
    first.show();
    second.show();
    QTest::qWait(80);
    first.background = QColor(Qt::red);
    second.background = QColor(Qt::blue);
    first.update();
    second.update();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
    QTest::qWait(80);
    const auto actual = window.backingStore()->handle()->toImage().copy();
    ASSERT_FALSE(actual.isNull());
    EXPECT_EQ(actual.pixelColor(QPointF(first.geometry().center() * 0.5).toPoint()), first.background);
    EXPECT_EQ(actual.pixelColor(QPointF(second.geometry().center() * 0.5).toPoint()), second.background);
    window.update();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
    QTest::qWait(80);
    EXPECT_EQ(actual, window.backingStore()->handle()->toImage());
#endif
}

TEST(InterfaceZoomRepaintTest, HalfDprWaitsForTheNativeBackingStore)
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")
        || QGuiApplication::platformName() == QStringLiteral("minimal"))
        GTEST_SKIP() << "Requires native window initialization.";
    ASSERT_TRUE(zoom::isSupported());
#ifdef FLUENT_QT_INTERFACE_ZOOM_HAS_REPAINT_QUEUES
    ASSERT_TRUE(zoom::apply(1.0));
    struct Restore {
        ~Restore() { zoom::apply(1.0); }
    } restore;
    PartialPaintProbe window;
    window.resize(640, 480);
    ASSERT_NE(window.winId(), 0u);
    ASSERT_TRUE(zoom::apply(0.5 / window.devicePixelRatioF()));
    ASSERT_NE(window.backingStore(), nullptr);
    ASSERT_NE(QWidgetPrivate::get(&window)->maybeRepaintManager(), nullptr);
    ASSERT_TRUE(window.backingStore()->size().isEmpty());
    EXPECT_FALSE(compatibility::detail::repaintLowDprRegion(&window, window.rect()));
    EXPECT_EQ(window.paints, 0);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(QTest::qWaitFor([&] { return window.paints > 0; }));
    EXPECT_EQ(window.backingStore()->size(), window.size());
    window.hide();
    const int paints = window.paints;
    EXPECT_FALSE(compatibility::detail::repaintLowDprRegion(&window, window.rect()));
    EXPECT_EQ(window.paints, paints);
#endif
}

TEST(InterfaceZoomRepaintTest, HalfDprTranslucentWindowNeverFlushesAClearedFrame)
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")
        || QGuiApplication::platformName() == QStringLiteral("minimal"))
        GTEST_SKIP() << "Requires native window initialization.";
    ASSERT_TRUE(zoom::isSupported());
#ifdef FLUENT_QT_INTERFACE_ZOOM_HAS_REPAINT_QUEUES
    ASSERT_TRUE(zoom::apply(1.0));
    struct Restore {
        ~Restore() { zoom::apply(1.0); }
    } restore;
    // Mica windows have an alpha backing store, which Qt clears before painting.
    // zh_CN: Mica 窗口使用带 alpha 的后备缓冲，Qt 绘制前会将其清为透明。
    QWidget window;
    window.setAttribute(Qt::WA_TranslucentBackground);
    window.resize(360, 240);
    auto* content = new PartialPaintProbe(&window);
    ASSERT_NE(window.winId(), 0u);
    ASSERT_TRUE(zoom::apply(0.5 / window.devicePixelRatioF()));
    // Zoom keeps the native size, so the logical window grew; cover all of it.
    // zh_CN: 缩放保持原生尺寸，逻辑窗口随之变大；内容需覆盖整个窗口。
    content->setGeometry(window.rect());
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(QTest::qWaitFor([&] { return content->paints > 0; }));
    QTest::qWait(100);

    // The adapter eats the outer request and re-sends it for Qt's own sync, which
    // flushes before the unit-density tile is drawn. Inspect the platform buffer
    // at that flush: it must hold the presented pixels, not a transparent hole.
    // zh_CN: 适配器拦截外层请求后重新发送给 Qt 同步，Qt 在单位密度图块绘制前提交。
    // 在该次提交时检查平台缓冲区：应保留已显示的像素，而不是透明空洞。
    class FlushProbe final : public QObject {
    public:
        explicit FlushProbe(PartialPaintProbe* content) : m_content(content) {}
        bool inspected = false;
        int paintsDuringSync = -1;
        int transparentBefore = -1;
        int transparentPixels = -1;

    protected:
        bool eventFilter(QObject* object, QEvent* event) override
        {
            if (inspected || event->type() != QEvent::UpdateRequest)
                return false;
            inspected = true;
            auto* window = static_cast<QWidget*>(object);
            const int paints = m_content->paints;
            transparentBefore = transparentPixelCount(window);
            static_cast<QObject*>(object)->event(event);
            paintsDuringSync = m_content->paints - paints;
            transparentPixels = transparentPixelCount(window);
            return true;
        }

    private:
        static int transparentPixelCount(QWidget* window)
        {
            const QImage image = window->backingStore()->handle()->toImage();
            int count = 0;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x)
                    count += qAlpha(image.pixel(x, y)) == 0;
            }
            return count;
        }

        PartialPaintProbe* m_content;
    } probe(content);
    window.installEventFilter(&probe);
    content->update();
    ASSERT_TRUE(QTest::qWaitFor([&] { return probe.inspected; }));
    window.removeEventFilter(&probe);
    EXPECT_EQ(probe.paintsDuringSync, 0) << "Expected the adapter's native pass, which skips raster painting";
    ASSERT_EQ(probe.transparentBefore, 0) << "The presented frame must be opaque before the sync";
    EXPECT_EQ(probe.transparentPixels, 0);
    ASSERT_TRUE(QTest::qWaitFor([&] { return content->paints > 1; }));
#endif
}

TEST(InterfaceZoomRepaintTest, HalfDprKeepsDamageRaisedDuringEffectPainting)
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")
        || QGuiApplication::platformName() == QStringLiteral("minimal"))
        GTEST_SKIP() << "Requires a native backing store.";
    ASSERT_TRUE(zoom::isSupported());
#ifdef FLUENT_QT_INTERFACE_ZOOM_HAS_REPAINT_QUEUES
    ASSERT_TRUE(zoom::apply(1.0));
    struct Restore {
        ~Restore() { zoom::apply(1.0); }
    } restore;
    class InvalidatingEffect final : public QGraphicsEffect {
    public:
        PartialPaintProbe* other = nullptr;
        bool pending = false;

    protected:
        void draw(QPainter* painter) override
        {
            drawSource(painter);
            if (pending && compatibility::detail::nativeRasterDevice == painter->device()) {
                pending = false;
                other->background = Qt::red;
                other->update();
            }
        }
    };
    PartialPaintProbe window;
    window.resize(900, 600);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(zoom::apply(0.5 / window.devicePixelRatioF()));
    window.resize(900, 600);
    PartialPaintProbe changed(&window);
    changed.background = Qt::blue;
    changed.setGeometry(400, 200, 80, 60);
    changed.show();
    PartialPaintProbe trigger(&window);
    trigger.setGeometry(100, 100, 80, 60);
    auto* effect = new InvalidatingEffect;
    effect->other = &changed;
    trigger.setGraphicsEffect(effect);
    trigger.show();
    QTest::qWait(80);
    effect->pending = true;
    window.update();
    QTest::qWait(80);
    EXPECT_FALSE(effect->pending);
    const QImage actual = window.backingStore()->handle()->toImage();
    ASSERT_FALSE(actual.isNull());
    EXPECT_EQ(actual.pixelColor(220, 115), QColor(Qt::red));
    const int paints = changed.paints;
    QTest::qWait(80);
    EXPECT_EQ(changed.paints, paints);
#endif
}

TEST(InterfaceZoomRepaintTest, HalfDprGpuFramesAvoidReadbackAndRecoverAfterCompositorUnlock)
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")
        || QGuiApplication::platformName() == QStringLiteral("minimal"))
        GTEST_SKIP() << "Requires the native GPU compositor.";
    ASSERT_TRUE(zoom::isSupported());
#if defined(FLUENT_QT_INTERFACE_ZOOM_HAS_REPAINT_QUEUES) && defined(FLUENT_QT_ZOOM_TEST_HAS_OPENGL_WIDGET)
    const QString unavailable = tests::support::nativeOpenGLContextUnavailableReason();
    if (!unavailable.isEmpty())
        GTEST_SKIP() << unavailable.toStdString();
    ASSERT_TRUE(zoom::apply(1.0));
    struct Restore {
        ~Restore() { zoom::apply(1.0); }
    } restore;
    PartialPaintProbe window;
    window.resize(640, 400);
    TexturePaintProbe gpu(&window);
    gpu.setGeometry(20, 20, 80, 80);
    PartialPaintProbe body(&window);
    body.setGeometry(180, 160, 80, 60);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(QTest::qWaitFor([&] { return gpu.isValid(); }));
    ASSERT_TRUE(zoom::apply(0.5 / window.devicePixelRatioF()));
    window.resize(640, 400);
    QTest::qWait(100);
    for (int frame = 0; frame < 5; ++frame) {
        const int paints = gpu.paints;
        window.update(QRect(0, 0, 140, 140));
        gpu.update();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
        QTest::qWait(20);
        EXPECT_EQ(gpu.paints, paints + 1) << "Raster repair must not call grabFramebuffer or repaint the GPU again";
    }
    body.background = Qt::red;
    body.update();
    QWidgetPrivate::get(&window)->syncBackingStore();
    QTest::qWait(80);
    EXPECT_EQ(window.backingStore()->handle()->toImage().pixelColor(110, 95), QColor(Qt::red));

    const auto& textures = QWidgetPrivate::get(&window)->topData()->widgetTextures;
    ASSERT_FALSE(textures.empty());
    QPointer<QPlatformTextureList> held = &*textures.front();
    held->lock(true);
    body.background = Qt::blue;
    body.update();
    const int paints = body.paints;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
    QTest::qWait(30);
    EXPECT_EQ(body.paints, paints) << "A locked compositor must not cause a retry loop";
    ASSERT_TRUE(held);
    held->lock(false);
    QTest::qWait(80);
    EXPECT_EQ(window.backingStore()->handle()->toImage().pixelColor(110, 95), QColor(Qt::blue));
    const int settled = body.paints;
    QTest::qWait(80);
    EXPECT_EQ(body.paints, settled);
#endif
}

TEST(InterfaceZoomRepaintTest, HalfDprPreservesNativeEffectsAndOverlaySnapshots)
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")
        || QGuiApplication::platformName() == QStringLiteral("minimal"))
        GTEST_SKIP() << "Requires a native backing store.";
    ASSERT_TRUE(zoom::isSupported());
#ifdef FLUENT_QT_HAS_INTERFACE_ZOOM
    ASSERT_TRUE(zoom::apply(1.0));
    struct Restore {
        ~Restore() { zoom::apply(1.0); }
    } restore;
    PartialPaintProbe window;
    window.resize(900, 600);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(zoom::apply(0.5 / window.devicePixelRatioF()));
    ASSERT_NEAR(window.devicePixelRatioF(), 0.5, 0.001);
    window.resize(900, 600);
    PartialPaintProbe body(&window);
    body.setGeometry(200, 100, 80, 60);
    body.background = QColor(200, 50, 40);
    auto* shadow = new QGraphicsDropShadowEffect(&body);
    shadow->setOffset(100, 0);
    shadow->setBlurRadius(4);
    shadow->setColor(QColor(20, 80, 200));
    body.setGraphicsEffect(shadow);
    body.show();
    PartialPaintProbe translucent(&window);
    translucent.setGeometry(200, 240, 100, 80);
    translucent.background = QColor(200, 60, 40);
    auto* opacity = new QGraphicsOpacityEffect(&translucent);
    opacity->setOpacity(0.5);
    translucent.setGraphicsEffect(opacity);
    translucent.show();
    for (QColor background : {QColor(240, 240, 240), QColor(30, 40, 50)}) {
        window.background = background;
        window.update();
        QTest::qWait(80);
        const QRect geometry = window.geometry();
        const QImage native = window.backingStore()->handle()->toImage().convertToFormat(QImage::Format_ARGB32);
        ASSERT_FALSE(native.isNull());
        const auto checkColor = [](const QImage& image, QPoint point, QColor expected) {
            const QColor actual = image.pixelColor(point);
            EXPECT_NEAR(actual.red(), expected.red(), 3);
            EXPECT_NEAR(actual.green(), expected.green(), 3);
            EXPECT_NEAR(actual.blue(), expected.blue(), 3);
        };
        checkColor(native, QPoint(120, 65), body.background);
        checkColor(native, QPoint(170, 65), shadow->color());
        const QColor blended((background.red() + 200) / 2, (background.green() + 60) / 2, (background.blue() + 40) / 2);
        checkColor(native, QPoint(125, 140), blended);
        checkColor(native, QPoint(300, 200), background);

        QWidget upper(&window);
        upper.setGeometry(700, 400, 100, 80);
        fluent::overlay::OverlayScrim scrim(&window);
        scrim.setGeometry(20, 20, 840, 540);
        scrim.setModalAndDim(false, false);
        scrim.setBackdrop(&upper, 1);
        QImage snapshot(window.size(), QImage::Format_ARGB32_Premultiplied);
        snapshot.fill(background);
        {
            QPainter painter(&snapshot);
            compatibility::detail::renderWidgetSnapshot(painter, &scrim, scrim.pos(), scrim.rect());
        }
        checkColor(snapshot, QPoint(240, 130), body.background);
        checkColor(snapshot, QPoint(340, 130), shadow->color());
        checkColor(snapshot, QPoint(250, 280), blended);
        checkColor(snapshot, QPoint(600, 400), background);
        EXPECT_EQ(window.geometry(), geometry);
        EXPECT_EQ(body.graphicsEffect(), shadow);
        EXPECT_EQ(shadow->offset(), QPointF(100, 0));
        EXPECT_EQ(translucent.graphicsEffect(), opacity);
        EXPECT_DOUBLE_EQ(opacity->opacity(), 0.5);
        EXPECT_TRUE(body.isVisible());
        EXPECT_TRUE(translucent.isVisible());
    }
#endif
}

TEST(InterfaceZoomBackendTest, LiveBackendPreservesGeometryAndRejectsInvalidOrNestedChanges)
{
    ASSERT_TRUE(zoom::isSupported()) << "This lane must build the live backend, including on Qt 5.15 and 6.2.";
    ASSERT_TRUE(zoom::apply(1.0));
    struct Restore {
        ~Restore() { zoom::apply(1.0); }
    } restore;
    EXPECT_FALSE(zoom::prepareStartup(1.25));
    for (qreal invalid : {0.0, -1.0, std::numeric_limits<qreal>::infinity(), std::numeric_limits<qreal>::quiet_NaN()})
        EXPECT_FALSE(zoom::apply(invalid));
    bool otherThreadResult = true;
    std::thread other([&] { otherThreadResult = zoom::apply(1.25); });
    other.join();
    EXPECT_FALSE(otherThreadResult);

    QWidget first;
    QWidget second;
    first.resize(600, 400);
    second.resize(300, 200);
    first.show();
    second.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&first));
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&second));
    const qreal originalDpr = first.windowHandle()->devicePixelRatio();
    const QSize original = first.size();
    const QSize otherSize = second.size();
    bool nestedAttempted = false;
    bool nestedResult = true;
    auto connection = QObject::connect(first.windowHandle(), &QWindow::widthChanged, &first, [&] {
        nestedAttempted = true;
        nestedResult = zoom::apply(0.75);
    });
    for (qreal factor : {0.5, 0.67, 1.25, 1.5, 2.0, 1.0}) {
        SCOPED_TRACE(factor);
        ASSERT_TRUE(zoom::apply(factor));
        EXPECT_DOUBLE_EQ(zoom::appliedZoom(), factor);
        EXPECT_NEAR(first.windowHandle()->devicePixelRatio(), originalDpr * factor, 0.001);
        EXPECT_NEAR(first.width() * factor, original.width(), factor);
        EXPECT_NEAR(first.height() * factor, original.height(), factor);
        EXPECT_NEAR(second.width() * factor, otherSize.width(), factor);
        EXPECT_NEAR(second.height() * factor, otherSize.height(), factor);
        // Rendering after the scale transition exercises backing-store DPR renewal.
        EXPECT_FALSE(first.grab().isNull());
        EXPECT_FALSE(second.grab().isNull());
    }
    QObject::disconnect(connection);
    EXPECT_TRUE(nestedAttempted);
    EXPECT_FALSE(nestedResult);
}

TEST(InterfaceZoomBackendTest, RestartBackendAppliesSavedScaleBeforeApplicationCreation)
{
    qreal defaultDpr = 0;
    for (qreal factor : {1.0, 0.5, 1.5, 2.0}) {
        SCOPED_TRACE(factor);
        QProcess child;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("QT_SCALE_FACTOR"), QStringLiteral("1.25"));
        child.setProcessEnvironment(environment);
        child.start(QString::fromUtf8(FLUENT_QT_ZOOM_STARTUP_PROBE), {QString::number(factor)});
        ASSERT_TRUE(child.waitForStarted(10000));
        ASSERT_TRUE(child.waitForFinished(15000));
        ASSERT_EQ(child.exitStatus(), QProcess::NormalExit) << child.readAllStandardError().constData();
        ASSERT_EQ(child.exitCode(), 0) << child.readAllStandardError().constData();
        const auto result = QJsonDocument::fromJson(child.readAllStandardOutput()).object();
        ASSERT_FALSE(result.isEmpty());
        EXPECT_FALSE(result.value(QStringLiteral("live")).toBool());
        EXPECT_FALSE(result.value(QStringLiteral("latePrepare")).toBool());
        EXPECT_DOUBLE_EQ(result.value(QStringLiteral("zoom")).toDouble(), factor);
        const qreal dpr = result.value(QStringLiteral("dpr")).toDouble();
        if (factor == 1.0)
            defaultDpr = dpr;
        EXPECT_NEAR(dpr, defaultDpr * factor, 0.001);
    }
}
