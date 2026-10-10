#ifndef FLUENTQT_QMLBRIDGEEXAMPLE_MODEL_H
#define FLUENTQT_QMLBRIDGEEXAMPLE_MODEL_H

#include <QObject>
#include <QString>

namespace fluentqt::qml_bridge {

// The application owns this model. Widgets and QML observe the same properties.
// zh_CN: 模型由应用持有；Widgets 与 QML 观察同一组属性。
class PreviewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(int progress READ progress WRITE setProgress NOTIFY progressChanged)
public:
    explicit PreviewModel(QObject* parent = nullptr) : QObject(parent) {}
    QString title() const { return m_title; }
    int progress() const { return m_progress; }

public slots:
    void setTitle(const QString& title)
    {
        if (m_title == title)
            return;
        m_title = title;
        emit titleChanged();
    }
    void setProgress(int progress)
    {
        progress = qBound(0, progress, 100);
        if (m_progress == progress)
            return;
        m_progress = progress;
        emit progressChanged();
    }

signals:
    void titleChanged();
    void progressChanged();

private:
    QString m_title = QStringLiteral("A shared canvas");
    int m_progress = 40;
};

} // namespace fluentqt::qml_bridge
#endif
