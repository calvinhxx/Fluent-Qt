#ifndef FLUENTQT_DIAGNOSTIC_SESSION_H
#define FLUENTQT_DIAGNOSTIC_SESSION_H

#include <QObject>
#include <QByteArray>
#include <QVariantMap>
#include <functional>
#include <memory>

class QWidget;

namespace fluent::diagnostics {

/**
 * @brief Collects a bounded, opt-in diagnostic session without file or network I/O.
 * zh_CN: 按需收集有容量上限的诊断会话，不执行文件或网络操作。
 *
 * UI methods require the GUI thread. The root is borrowed. Text, object names,
 * model values, paths and environment variables are not included in snapshots. Log bodies
 * are excluded unless explicitly requested; included bodies are NOT redacted.
 * zh_CN: 界面方法须在 GUI 线程调用，根控件为借用。快照不会包含文本、对象名称、模型值、
 * 路径或环境变量。日志正文默认排除；显式包含的正文不会自动脱敏。
 */
class DiagnosticSession final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool recording READ isRecording NOTIFY recordingChanged)

public:
    using MessageSink = std::function<void(QtMsgType, const QString&, const QString&)>;

    explicit DiagnosticSession(QObject* parent = nullptr);
    ~DiagnosticSession() override;

    /** @brief Starts a fresh session for a borrowed root; repeated starts are no-ops.
     * zh_CN: 为借用的根控件开始新会话；录制期间重复调用无效。
     * Zero duration keeps a bounded history until stopped; positive values auto-stop in milliseconds.
     * zh_CN: 时长为零时持续保留有上限的近期记录，正值为自动停止的毫秒数。 */
    bool start(QWidget* root, bool includeLogMessages = false, int maximumDurationMs = 300000);
    /** @brief Stops recording and captures the final bounded widget snapshot.
     * zh_CN: 停止录制并捕获有数量上限的最终控件快照。 */
    void stop();
    /** @brief Stops and discards all captured data.
     * zh_CN: 停止并丢弃全部已收集的数据。 */
    void clear();
    bool isRecording() const;
    /** @brief Returns a versioned report; the host owns review, saving and sharing.
     * zh_CN: 返回带版本的报告；预览、保存与分享由宿主负责。 */
    QVariantMap report() const;
    /** @brief Captures current widget state without interrupting recording; GUI thread only.
     * zh_CN: 在不中断记录的情况下捕获当前控件状态，仅允许 GUI 线程调用。 */
    QVariantMap captureReport();
    /** @brief Creates a ZIP in memory with report.json and optional explicitly supplied files, capped at 20 MiB.
     * zh_CN: 在内存中生成包含 report.json 和显式提供附件的 ZIP，总大小上限为 20 MiB。
     * Returns empty on invalid names or excessive size. Performs no file access or upload.
     * zh_CN: 名称无效或超限时返回空值，不读取文件或上传。 */
    static QByteArray createBundle(const QVariantMap& report, const QVariantMap& attachments = {});
    /** @brief Returns allowlisted runtime information without host names or paths.
     * zh_CN: 返回白名单内的运行环境信息，不含主机名或路径。 */
    static QVariantMap environment();
    /** @brief Records a host-forwarded message; safe from worker threads while alive.
     * zh_CN: 记录宿主转发的消息；对象存活期间允许工作线程调用。 */
    void recordMessage(QtMsgType type, const QString& category, const QString& message);
    /** @brief Returns a thread-safe sink that becomes inert after session destruction.
     * zh_CN: 返回线程安全的接收函数，会话销毁后自动失效。 */
    MessageSink messageSink() const;

signals:
    void recordingChanged(bool recording);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct State;
    struct UiState;
    std::shared_ptr<State> m_state;
    std::unique_ptr<UiState> m_ui;
    QVariantMap snapshot();
};

} // namespace fluent::diagnostics
#endif
