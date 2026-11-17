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
 * @brief Records a bounded in-memory diagnostic session for a widget tree and optional log messages.
 * UI calls must run on the GUI thread; snapshots exclude widget text and paths, and log messages are only recorded when explicitly enabled.
 * zh_CN: 在内存中采集有限容量的界面状态与可选日志诊断会话。界面相关方法须在 GUI 线程调用；快照会自动过滤控件文本与路径，日志正文仅在显式开启时记录（不会自动脱敏）。
 */
class DiagnosticSession final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool recording READ isRecording NOTIFY recordingChanged)

public:
    using MessageSink = std::function<void(QtMsgType, const QString&, const QString&)>;

    explicit DiagnosticSession(QObject* parent = nullptr);
    ~DiagnosticSession() override;

    /**
     * @brief Starts recording for @p root (caller retains ownership); returns false if already recording.
     * A zero @p maximumDurationMs records until stop() is called; positive values stop automatically after that many milliseconds.
     * zh_CN: 为 @p root 启动诊断录制（控件仍由应用管理，不会随会话销毁）；若已在录制中则返回 false。@p maximumDurationMs 为 0 时持续记录直到手动停止，正值则在指定毫秒后自动停止。
     */
    bool start(QWidget* root, bool includeLogMessages = false, int maximumDurationMs = 300000);
    /**
     * @brief Stops recording and captures a final widget snapshot.
     * zh_CN: 停止录制并采集最终界面快照。
     */
    void stop();
    /**
     * @brief Stops recording and discards all collected data.
     * zh_CN: 停止录制并清空所有已采集的数据。
     */
    void clear();
    bool isRecording() const;
    /**
     * @brief Returns the collected JSON inspection report as a QVariantMap.
     * zh_CN: 返回已采集的 JSON 检查报告（QVariantMap 格式）。
     */
    QVariantMap report() const;
    /**
     * @brief Captures a JSON inspection report of the current widget state without stopping the session (GUI thread only).
     * zh_CN: 在不中断录制的情况下采集当前界面的 JSON 检查报告（仅限 GUI 线程调用）。
     */
    QVariantMap captureReport();
    /**
     * @brief Packages report.json and optional attachments into an in-memory ZIP archive (up to 20 MiB).
     * Returns an empty byte array if any attachment name is invalid or the size limit is exceeded.
     * zh_CN: 在内存中将 report.json 与可选附件打包为 ZIP（上限 20 MiB）；若附件名无效或超出大小上限则返回空数组。
     */
    static QByteArray createBundle(const QVariantMap& report, const QVariantMap& attachments = {});
    /**
     * @brief Returns safe runtime environment metadata without host names or file paths.
     * zh_CN: 返回运行环境基本信息（不含主机名和本地路径）。
     */
    static QVariantMap environment();
    /**
     * @brief Records a log message; safe to call from worker threads while this session is alive.
     * zh_CN: 记录一条日志消息；在会话存活期间可从工作线程安全调用。
     */
    void recordMessage(QtMsgType type, const QString& category, const QString& message);
    /**
     * @brief Returns a thread-safe log callback that becomes a no-op after the session is destroyed.
     * zh_CN: 返回线程安全的日志接收回调；会话销毁后自动失效。
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
