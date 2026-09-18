#ifndef GALLERYSPLASHSCREEN_H
#define GALLERYSPLASHSCREEN_H

#include <FluentQt/StatusInfo.h>
#include <QVector>

namespace fluent::layout {
class ParticleBackdrop;
}

namespace fluent::gallery {

/** @brief Supplies Gallery branding and one-shot ownership to the shared startup surface.
 * zh_CN: 为公共启动遮罩提供 Gallery 图标和一次性生命周期。
 */
class GallerySplashScreen : public fluent::status_info::SplashScreen {
    Q_OBJECT
public:
    explicit GallerySplashScreen(QWidget* parent = nullptr);
    ~GallerySplashScreen() override;

    // Cache the covered Gallery content only for this dismissal. Never replaces another effect.
    // zh_CN: 仅在本次退场期间缓存被遮盖的 Gallery 内容，不替换已有特效。
    void cacheDismissalContent(QWidget* content);
    void clearDismissalContent();

protected:
    void hideEvent(QHideEvent* event) override;

private:
    QPointer<QWidget> m_cachedContent;
    QPointer<QGraphicsEffect> m_contentEffect;
    QVector<QPointer<fluent::layout::ParticleBackdrop>> m_pausedBackdrops;
};

} // namespace fluent::gallery
#endif // GALLERYSPLASHSCREEN_H
