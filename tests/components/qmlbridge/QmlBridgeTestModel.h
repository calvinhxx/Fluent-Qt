#ifndef FLUENTQT_TESTS_QMLBRIDGETESTMODEL_H
#define FLUENTQT_TESTS_QMLBRIDGETESTMODEL_H
#include <QObject>

class QmlBridgeTestModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(int progress READ progress WRITE setProgress NOTIFY progressChanged)
public:
    using QObject::QObject;
    QString title() const { return m_title; }
    int progress() const { return m_progress; }
    void setTitle(const QString& title)
    {
        if (m_title == title)
            return;
        m_title = title;
        emit titleChanged();
    }
    void setProgress(int progress)
    {
        if (m_progress == progress)
            return;
        m_progress = progress;
        emit progressChanged();
    }
    Q_INVOKABLE void stopBridge() { emit stopRequested(); }
signals:
    void titleChanged();
    void progressChanged();
    void stopRequested();

private:
    QString m_title = QStringLiteral("Shared model");
    int m_progress = 25;
};
#endif
