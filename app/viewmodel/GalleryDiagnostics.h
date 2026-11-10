#ifndef FLUENTQT_GALLERY_DIAGNOSTICS_H
#define FLUENTQT_GALLERY_DIAGNOSTICS_H
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QUrl>
#include <QVariantMap>

class QWidget;
namespace fluent::diagnostics {
class DiagnosticSession;
}
namespace fluent::gallery {
class GalleryFeedbackTip;
// Application policy: the reusable collector itself never touches disk or GitHub.
// zh_CN: 文件保存和 GitHub 属于应用策略，公共采集器不执行这些操作。
class GalleryDiagnostics final : public QObject {
public:
    explicit GalleryDiagnostics(QWidget* root, const QString& storageDirectory);
    ~GalleryDiagnostics() override;
    static GalleryDiagnostics* ensure(QWidget* root);
    QVariantMap captureReport();
    QUrl issueUrl(const QVariantMap& report) const;
    QVariantMap savedAttachments() const;
    void persist();
    void reportProblem(QWidget* source = nullptr);
    QString sessionId() const { return m_sessionId; }
    diagnostics::DiagnosticSession* session() const { return m_session; }

private:
    GalleryFeedbackTip* showFeedback(const QString& title, const QString& message, bool error = false);
    void showPreparedReport(const QUrl& url, bool downloaded, bool openFailed = false);
    QVariantMap storedReport() const;
    QVariantMap m_identity;
    QVariantMap m_previousIdentity;
    QString m_directory;
    bool m_preparing = false;
    bool m_feedbackVisible = false;
    QPointer<QWidget> m_feedbackAnchor;
    QPointer<GalleryFeedbackTip> m_feedbackTip;
    QPointer<QWidget> m_root;
    diagnostics::DiagnosticSession* m_session = nullptr;
    QString m_sessionId;
    QString m_path;
    QByteArray m_previous;
    qint64 m_lastReportBytes = 0;
    QThread m_writerThread;
    QObject* m_writer = nullptr;
};
} // namespace fluent::gallery
#endif
