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
 * zh_CN: 按需采集有容量上限的诊断会话，不执行文件或网络操作。
 *
 * UI methods require the GUI thread, and the session does not take ownership of
 * the root widget. Text, object names, model values, paths, and environment
 * variables are excluded from snapshots. Log bodies are omitted unless
 * explicitly requested; included bodies are not redacted.
 * zh_CN: 界面方法须在 GUI 线程调用，且会话不接管根控件的所有权。快照不包含文本、
 * 对象名、模型值、路径或环境变量。日志正文默认排除；显式包含的正文不会自动脱敏。
 */
class DiagnosticSession final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool recording READ isRecording NOTIFY recordingChanged)

public:
    using MessageSink = std::function<void(QtMsgType, const QString&, const QString&)>;

    explicit DiagnosticSession(QObject* parent = nullptr);
    ~DiagnosticSession() override;

    /**
     * @brief Starts a new session for @p root without taking ownership; returns false if already recording.
     * zh_CN: 为 @p root 启动新的诊断会话（不接管所有权）；若正在录制则返回 false。
     *
     * A zero @p maximumDurationMs keeps a bounded history until stopped; positive
     * values auto-stop after that interval in milliseconds.
     * zh_CN: @p maximumDurationMs 为 0 时持续保留有上限的近期记录直到停止；正值则在对应毫秒后自动停止。
     */
    bool start(QWidget* root, bool includeLogMessages = false, int maximumDurationMs = 300000);
    /**
     * @brief Stops recording and captures the final bounded widget snapshot.
     * zh_CN: 停止录制并捕获有数量上限的最终控件快照。
     */
    void stop();
    /**
     * @brief Stops and discards all captured data.
     * zh_CN: 停止并丢弃全部已收集的数据。
     */
    void clear();
    bool isRecording() const;
    /**
     * @brief Returns a versioned report; the caller owns review, saving, and sharing.
     * zh_CN: 返回带版本号的报告；预览、保存与分享由调用方负责。
     */
    QVariantMap report() const;
    /**
     * @brief Captures current widget state without interrupting recording; GUI thread only.
     * zh_CN: 在不中断录制的情况下捕获当前控件状态，仅允许 GUI 线程调用。
     */
    QVariantMap captureReport();
    /**
     * @brief Creates an in-memory ZIP containing report.json and optional attachments, capped at 20 MiB.
     * zh_CN: 在内存中生成包含 report.json 和可选附件的 ZIP，总大小上限为 20 MiB。
     *
     * Returns empty on invalid attachment names or excessive size without disk or network access.
     * zh_CN: 附件名称无效或超出上限时返回空值，不访问磁盘或网络。
     */
    static QByteArray createBundle(const QVariantMap& report, const QVariantMap& attachments = {});
    /**
     * @brief Returns allowlisted runtime information without host names or paths.
     * zh_CN: 返回白名单内的运行环境信息，不含主机名或路径。
     */
    static QVariantMap environment();
    /**
     * @brief Records a forwarded message; safe from worker threads while this session is alive.
     * zh_CN: 记录外部转发的消息；会话存活期间允许工作线程调用。
     */
    void recordMessage(QtMsgType type, const QString& category, const QString& message);
    /**
     * @brief Returns a thread-safe sink that becomes inert after session destruction.
     * zh_CN: 返回线程安全的接收回调，会话销毁后自动失效。
     */
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
