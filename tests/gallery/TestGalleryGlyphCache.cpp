#include <gtest/gtest.h>
#include <FluentQt/FluentQt.h>
#include <QOpenGLWidget>
#include <QPainter>
#include <QScopeGuard>
#include <QTest>
#include "compatibility/TextPaintCompat.h"
#include "QtTestEnvironment.h"
#include "SpatialTestEnvironment.h"
#include "support/spatial/SpatialTextureHost.h"
#include "view/shell/GalleryGlyphPaintDevice.h"
#include "view/shell/GallerySpatialController.h"
#include "viewmodel/GallerySettings.h"

using namespace fluent::gallery::spatial_render;

namespace {
QImage tile(const QColor& color, QSize size = QSize(8, 8), qreal dpr = 1)
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(color);
    image.setDevicePixelRatio(dpr);
    return image;
}

class ImageSubmissionRecorder final : public QPaintDevice {
public:
    explicit ImageSubmissionRecorder(const QImage& target) : m_target(target), m_engine(m_keys) {}
    QPaintEngine* paintEngine() const override { return &m_engine; }
    const QVector<qint64>& imageKeys() const { return m_keys; }

protected:
    int metric(PaintDeviceMetric metric) const override
    {
        switch (metric) {
        case PdmWidth:
            return m_target.width();
        case PdmHeight:
            return m_target.height();
        case PdmWidthMM:
            return m_target.widthMM();
        case PdmHeightMM:
            return m_target.heightMM();
        case PdmNumColors:
            return m_target.colorCount();
        case PdmDepth:
            return m_target.depth();
        case PdmDpiX:
            return m_target.logicalDpiX();
        case PdmDpiY:
            return m_target.logicalDpiY();
        case PdmPhysicalDpiX:
            return m_target.physicalDpiX();
        case PdmPhysicalDpiY:
            return m_target.physicalDpiY();
        default:
            return fluent::painting::devicePixelRatioMetric(metric, m_target.devicePixelRatioF());
        }
    }

private:
    class Engine final : public QPaintEngine {
    public:
        explicit Engine(QVector<qint64>& keys)
            : QPaintEngine(AlphaBlend | PorterDuff | PrimitiveTransform | PixmapTransform | ConstantOpacity),
              m_keys(keys)
        {}
        Type type() const override { return User; }
        bool begin(QPaintDevice*) override
        {
            setActive(true);
            return true;
        }
        bool end() override
        {
            setActive(false);
            return true;
        }
        void updateState(const QPaintEngineState&) override {}
        void drawPixmap(const QRectF&, const QPixmap&, const QRectF&) override
        {
            ADD_FAILURE() << "Glyph tiles must be submitted as images";
        }
        void drawImage(const QRectF&, const QImage& image, const QRectF&, Qt::ImageConversionFlags) override
        {
            m_keys.append(image.cacheKey());
        }

    private:
        QVector<qint64>& m_keys;
    };

    QImage m_target;
    QVector<qint64> m_keys;
    mutable Engine m_engine;
};
} // namespace

TEST(GalleryGlyphCacheTest, ExactPixelsReuseKeysWithoutChangingColorGeometryOrDensity)
{
    GlyphImageCache cache;
    const auto red = cache.retain(tile(Qt::red));
    EXPECT_EQ(cache.retain(tile(Qt::red)).cacheKey(), red.cacheKey());
    const auto blue = cache.retain(tile(Qt::blue));
    EXPECT_NE(blue.cacheKey(), red.cacheKey());
    EXPECT_EQ(blue.pixelColor(0, 0), QColor(Qt::blue));
    const auto dense = cache.retain(tile(Qt::red, QSize(8, 8), 2));
    EXPECT_EQ(dense.devicePixelRatioF(), 2);
    EXPECT_NE(dense.cacheKey(), red.cacheKey());
    const auto wide = cache.retain(tile(Qt::red, QSize(16, 4)));
    EXPECT_EQ(wide.size(), QSize(16, 4));
    EXPECT_NE(wide.cacheKey(), red.cacheKey());
    QImage detached = red;
    detached.fill(Qt::green);
    EXPECT_EQ(cache.retain(tile(Qt::red)).pixelColor(0, 0), QColor(Qt::red));
    EXPECT_EQ(cache.retain(tile(Qt::red)).cacheKey(), red.cacheKey());
}

TEST(GalleryGlyphCacheTest, LeastRecentlyUsedEntriesStayWithinTheBudgetAndClear)
{
    GlyphImageCache cache(2800);
    const auto red = cache.retain(tile(Qt::red));
    const auto blue = cache.retain(tile(Qt::blue));
    EXPECT_EQ(cache.retain(tile(Qt::red)).cacheKey(), red.cacheKey());
    cache.retain(tile(Qt::green));
    EXPECT_LE(cache.cost(), 2800);
    EXPECT_EQ(cache.entryCount(), 2);
    EXPECT_EQ(cache.retain(tile(Qt::red)).cacheKey(), red.cacheKey());
    EXPECT_NE(cache.retain(tile(Qt::blue)).cacheKey(), blue.cacheKey());
    const int before = cache.cost();
    const QImage oversized = tile(Qt::yellow, QSize(64, 64));
    EXPECT_EQ(cache.retain(oversized).cacheKey(), oversized.cacheKey());
    EXPECT_EQ(cache.cost(), before);
    cache.clear();
    EXPECT_EQ(cache.cost(), 0);
    EXPECT_EQ(cache.entryCount(), 0);
}

TEST(GalleryGlyphCacheTest, RetainedRasterTilesPreserveShapingAndFractionalCoverage)
{
    GlyphImageCache cache;
    for (qreal dpr : {1., 1.5, 2.}) {
        for (auto hint : {QFont::PreferFullHinting, QFont::PreferNoHinting}) {
            SCOPED_TRACE(dpr);
            SCOPED_TRACE(int(hint));
            const auto draw = [&](QPaintDevice& target, GlyphImageCache* images) {
                GalleryGlyphPaintDevice device(target, 1, {}, images);
                QPainter painter(&device);
                QFont font = painter.font();
                font.setPixelSize(18);
                font.setHintingPreference(hint);
                painter.setFont(font);
                painter.setPen(QColor(20, 90, 160, 210));
                painter.setOpacity(.65);
                painter.translate(.35, .65);
                painter.drawText(QPointF(6, 26), QString::fromUtf8("office مرحبًا é 演示"));
                painter.end();
            };
            const auto render = [&](GlyphImageCache* images) {
                QImage output(QSize(640, 180), QImage::Format_ARGB32_Premultiplied);
                output.setDevicePixelRatio(dpr);
                output.fill(Qt::transparent);
                draw(output, images);
                return output;
            };
            const QImage reference = render(nullptr);
            EXPECT_EQ(render(&cache), reference);
            EXPECT_EQ(render(&cache), reference);
            EXPECT_GT(cache.entryCount(), 0);
            EXPECT_LE(cache.cost(), kGlyphCacheBytes);

            // Observe the images submitted by real glyph drawing, not only retain().
            // zh_CN: 核验真实字形绘制提交的图像，不能只验证 retain() 自身的缓存行为。
            GlyphImageCache submittedImages;
            ImageSubmissionRecorder cold(reference), warm(reference);
            draw(cold, &submittedImages);
            const int coldEntries = submittedImages.entryCount();
            ASSERT_GT(coldEntries, 0);
            ASSERT_FALSE(cold.imageKeys().isEmpty());
            draw(warm, &submittedImages);
            EXPECT_EQ(warm.imageKeys(), cold.imageKeys());
            EXPECT_EQ(submittedImages.entryCount(), coldEntries);
            EXPECT_LE(submittedImages.cost(), kGlyphCacheBytes);
        }
    }
}

TEST(GalleryGlyphCacheTest, CachePlanReservesImagesAndTexturesWithoutReducingNormalDensity)
{
    const std::array<QSizeF, 2> panels = {QSizeF(272, 776), QSizeF(936, 776)};
    const auto plan = planCaches(panels, 1, 16384, 2, kCacheBudgetBytes, 2, 1208LL * 776 * 4, kGlyphWorkingSetBytes, 0,
        8);
    ASSERT_TRUE(plan.valid());
    EXPECT_EQ(plan.dpr, 2);
    EXPECT_EQ(plan.glyphScratchBytes, kGlyphScratchBytes + 2LL * kGlyphCacheBytes);
    EXPECT_LE(plan.estimatedBytes, kCacheBudgetBytes);
}

TEST(GalleryGlyphCacheTest, NativeModeChangesReleaseRetainedImages)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires a native GPU surface";
    const QString unavailable = tests::support::nativeOpenGLUnavailableReason();
    if (!unavailable.isEmpty())
        GTEST_SKIP() << unavailable.toStdString();
    QWidget window;
    if (!needsNativeGlyphCoverage(window.devicePixelRatioF()))
        GTEST_SKIP() << "This display uses Qt's direct glyph path";
    auto& settings = fluent::gallery::GallerySettings::instance();
    const bool previous = settings.spatialModeEnabled();
    const auto restore = qScopeGuard([&] { settings.setSpatialModeEnabled(previous); });
    settings.setSpatialModeEnabled(false);
    window.resize(800, 600);
    auto* navigation = new fluent::navigation::NavigationView(&window);
    navigation->resize(window.size());
    auto* label = new fluent::basicinput::Button(QStringLiteral("Retained glyph pixels"));
    navigation->contentHost()->insertPage(0, label);
    navigation->contentHost()->setCurrentIndex(0, 0, false);
    fluent::gallery::GallerySpatialController controller(&window, navigation);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    for (int cycle = 0; cycle < 2; ++cycle) {
        settings.setSpatialModeEnabled(true);
        auto* surface = window.findChild<QWidget*>("gallerySpatialSurface");
        if (qobject_cast<fluent::spatial::SpatialTextureHost*>(surface)) {
            RecordProperty("backend", "RHI");
            GTEST_SKIP() << "The active RHI backend does not use the OpenGL glyph image cache";
        }
        ASSERT_NE(qobject_cast<QOpenGLWidget*>(surface), nullptr);
        RecordProperty("backend", "OpenGL");
        ASSERT_TRUE(QTest::qWaitFor(
            [&] { return controller.renderingStatistics().value("glyphCacheBytes").toInt() > 0; }, 5000));
        controller.cancelTransition();
        const auto stats = controller.renderingStatistics();
        EXPECT_LE(stats.value("glyphCacheBytes").toInt(), kGlyphCacheBytes);
        EXPECT_LE(stats.value("cacheEstimatedBytes").toLongLong(), kCacheBudgetBytes);
        settings.setSpatialModeEnabled(false);
        controller.cancelTransition();
        EXPECT_EQ(controller.renderingStatistics().value("glyphCacheBytes").toInt(), 0);
    }
}
