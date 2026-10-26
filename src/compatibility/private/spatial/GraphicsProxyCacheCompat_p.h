#pragma once

#include <QEvent>
#include <QGraphicsProxyWidget>
#include <QPointer>
#include <QWidget>

namespace compatibility::detail {

// QWidget repaint requests do not always invalidate the proxy's item pixmap.
// Spatial proxies use the default logical cache size. Retire that Qt cache
// while preserving the selected caching policy.
// zh_CN: QWidget 重绘请求未必使代理图元缓存失效；清理 Qt 缓存并保留所选缓存策略。
class GraphicsProxyCacheCompat : public QGraphicsProxyWidget {
public:
    using QGraphicsProxyWidget::QGraphicsProxyWidget;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (!m_resettingCache && event->type() == QEvent::UpdateRequest && watched == widget()
            && cacheMode() != QGraphicsItem::NoCache) {
            const QPointer<GraphicsProxyCacheCompat> guard(this);
            const QPointer<QObject> source(watched);
            const auto mode = cacheMode();
            m_resettingCache = true;
            setCacheMode(QGraphicsItem::NoCache);
            if (!guard)
                return source.isNull();
            setCacheMode(mode);
            if (!guard)
                return source.isNull();
            m_resettingCache = false;
            if (!source)
                return true;
        }
        return QGraphicsProxyWidget::eventFilter(watched, event);
    }

private:
    bool m_resettingCache = false;
};

} // namespace compatibility::detail
