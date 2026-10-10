#ifndef GALLERYPREVIEWSESSION_H
#define GALLERYPREVIEWSESSION_H

#include <QHash>
#include <QJsonObject>
#include <QLockFile>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <functional>
#include <memory>

namespace fluent::gallery {

/**
 * @brief Local, opt-in command session for the development preview host.
 * zh_CN: 开发预览宿主显式启用的本地命令会话。
 */
class GalleryPreviewSession final : public QObject {
public:
    using Capture = std::function<QByteArray()>;

    explicit GalleryPreviewSession(QWidget* root, Capture capture = {});
    ~GalleryPreviewSession() override;
    bool start(const QString& directory, QString& error);
    QJsonObject execute(const QString& method, const QJsonObject& parameters = {});
    QString sessionId() const { return m_sessionId; }

private:
    void poll();
    void refreshObjects();
    QWidget* resolve(const QString& selector) const;
    QJsonObject inspect();
    QJsonObject data(QWidget* widget) const;
    QJsonObject setData(QWidget* widget, const QString& property, const QJsonValue& value,
                        bool rememberOriginal = true);
    QJsonObject state() const;

    QPointer<QWidget> m_root;
    Capture m_capture;
    QTimer m_timer;
    std::unique_ptr<QLockFile> m_lock;
    QString m_directory;
    QString m_sessionId;
    QHash<QString, QPointer<QWidget>> m_widgets;
    QHash<QWidget*, QString> m_ids;
    QHash<QString, QJsonObject> m_defaults;
    QHash<QString, QJsonObject> m_originals;
    quint64 m_nextId = 0;
    bool m_busy = false;
};

} // namespace fluent::gallery
#endif
