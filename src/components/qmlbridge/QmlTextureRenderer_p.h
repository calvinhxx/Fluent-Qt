#ifndef FLUENTQT_QMLBRIDGE_QMLTEXTURERENDERER_P_H
#define FLUENTQT_QMLBRIDGE_QMLTEXTURERENDERER_P_H

#include <QObject>
#include <QSize>
#include <QString>
#include <QVariantMap>
#include <memory>

class QOpenGLContext;
class QQuickWidget;
class QQuickWindow;

namespace fluent::qmlbridge {
class QmlView;

// Renders a QmlView's root QQuickItem offscreen in an internal QQuickWindow while QQuickWidget keeps engine ownership.
class QmlTextureRenderer final : public QObject {
public:
    explicit QmlTextureRenderer(QmlView* view);
    ~QmlTextureRenderer() override;
    bool attach(QQuickWidget* quick);
    void detach();
    bool render(QOpenGLContext* context, const QSize& logicalSize, qreal dpr, quint64 budget);
    void release();
    void markDirty();
    QQuickWindow* window() const;
    unsigned int textureId() const;
    QSize textureSize() const;
    quint64 frameCount() const;
    QString lastError() const;
    QVariantMap statistics() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
    void createWindow();
    void releaseNow(bool restore);
};
} // namespace fluent::qmlbridge
#endif
