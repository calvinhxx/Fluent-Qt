#include "GallerySplashScreen.h"

#include <QGraphicsEffect>
#include <QHideEvent>
#include <QPainter>

#include "components/foundation/MotionPolicy.h"
#include "components/layout/ParticleBackdrop.h"
#include "view/shell/AppIcon.h"

namespace fluent::gallery {
namespace {

// Reuse the real widgets' rendering for the short handoff. Keeping them visible preserves their
// lifecycle and native backdrop alpha; their expensive paint code need not run on every fade tick.
// zh_CN: 短暂交接期间复用真实控件的绘制结果；保持可见及原有生命周期和材质透明度，避免逐帧重绘整页。
class StartupContentEffect final : public QGraphicsEffect {
public:
    explicit StartupContentEffect(QWidget* content) : QGraphicsEffect(content)
    {
        setObjectName(QStringLiteral("galleryStartupContentEffect"));
        content->installEventFilter(this);
    }

protected:
    void prepare(qreal dpr)
    {
        const auto bounds = sourceBoundingRect(Qt::LogicalCoordinates);
        const int theme = FluentElement::themeGeneration();
        if (m_frame.isNull() || bounds != m_bounds || dpr != m_dpr || theme != m_theme) {
            m_frame = sourcePixmap(Qt::LogicalCoordinates, &m_offset, NoPad);
            m_bounds = bounds;
            m_dpr = dpr;
            m_theme = theme;
        }
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::LayoutRequest:
        case QEvent::StyleChange:
        case QEvent::PaletteChange:
        case QEvent::FontChange:
            m_frame = QPixmap();
            break;
        default:
            break;
        }
        return QGraphicsEffect::eventFilter(watched, event);
    }

    void draw(QPainter* painter) override
    {
        prepare(painter->device()->devicePixelRatioF());
        painter->drawPixmap(m_offset, m_frame);
    }

private:
    QPixmap m_frame;
    QPoint m_offset;
    QRectF m_bounds;
    qreal m_dpr = 0.0;
    int m_theme = -1;
};

} // namespace

GallerySplashScreen::GallerySplashScreen(QWidget* parent) : SplashScreen(parent)
{
    setObjectName(QStringLiteral("gallerySplashScreen"));
    setIcon(appicon::icon());
    setIconSize(QSize(112, 112));
    setTitle(QStringLiteral("FluentQt"));
    setSubtitle(QStringLiteral("Small details. Fluent experiences."));
    setText(QStringLiteral("Preparing your workspace"));
    connect(this, &SplashScreen::dismissed, this, &QObject::deleteLater);
    connect(this, &SplashScreen::replaced, this, &QObject::deleteLater);
}

GallerySplashScreen::~GallerySplashScreen()
{
    clearDismissalContent();
}

void GallerySplashScreen::cacheDismissalContent(QWidget* content)
{
    clearDismissalContent();
    if (!isVisible() || !content || !content->isVisible() || content->graphicsEffect() ||
        content->window() != window() || content == this || content->isAncestorOf(this) ||
        isAncestorOf(content) || MotionPolicy::instance().mode() != MotionPolicy::Mode::Full)
        return;
    m_cachedContent = content;
    // Freeze the decorative clock with its pixels, so resuming never jumps ahead by the
    // handoff duration. Hidden and already-disabled samples keep their own preferences.
    // zh_CN: 装饰动效的时钟随画面一起暂停，恢复时不跳过整段交接时间；隐藏及已禁用的示例保持原状。
    for (auto* backdrop : content->findChildren<layout::ParticleBackdrop*>()) {
        if (backdrop->isVisible() && backdrop->isAnimationEnabled()) {
            m_pausedBackdrops.append(backdrop);
            backdrop->setAnimationEnabled(false);
        }
    }
    auto* effect = new StartupContentEffect(content);
    m_contentEffect = effect;
    content->setGraphicsEffect(effect);
    // Prime the effect through QWidget's paint context, before starting either animation.
    // The clipped result is discarded; the effect retains the full source at native DPR.
    // zh_CN: 动效开始前通过 QWidget 绘制上下文预热特效；丢弃裁剪结果，特效保留原生 DPR 的完整源图。
    content->grab(QRect(0, 0, 1, 1));
}

void GallerySplashScreen::clearDismissalContent()
{
    if (m_cachedContent && m_contentEffect && m_cachedContent->graphicsEffect() == m_contentEffect)
        m_cachedContent->setGraphicsEffect(nullptr);
    m_cachedContent.clear();
    m_contentEffect.clear();
    const auto paused = m_pausedBackdrops;
    m_pausedBackdrops.clear();
    for (const auto& backdrop : paused) {
        if (backdrop)
            backdrop->setAnimationEnabled(true);
    }
}

void GallerySplashScreen::hideEvent(QHideEvent* event)
{
    clearDismissalContent();
    SplashScreen::hideEvent(event);
}

} // namespace fluent::gallery
