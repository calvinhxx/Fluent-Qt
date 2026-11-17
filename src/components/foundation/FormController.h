#ifndef FLUENTQT_FORMCONTROLLER_H
#define FLUENTQT_FORMCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVector>

namespace fluent::layout {
class Field;
}

namespace fluent {

/**
 * @brief Coordinates dirty state, validation, and submission across a group of Field controls on the GUI thread.
 * Respond to validationRequested() with completeValidation(), and call markFieldChanged() on every value update.
 * zh_CN: 协调一组 Field 控件的修改状态、异步校验与提交流程（须在 GUI 线程使用）。收到 validationRequested() 后需调用 completeValidation() 回传结果，值发生变更时需调用 markFieldChanged()。
 */
class FormController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool dirty READ isDirty NOTIFY stateChanged)
    Q_PROPERTY(bool touched READ isTouched NOTIFY stateChanged)
    Q_PROPERTY(bool validating READ isValidating NOTIFY stateChanged)
    Q_PROPERTY(bool submitting READ isSubmitting NOTIFY stateChanged)
    Q_PROPERTY(bool valid READ isValid NOTIFY stateChanged)
    Q_PROPERTY(QStringList errorMessages READ errorMessages NOTIFY stateChanged)
    Q_PROPERTY(QString submissionError READ submissionError NOTIFY stateChanged)

public:
    explicit FormController(QObject* parent = nullptr);
    ~FormController() override;

    /**
     * @brief Registers a Field under a unique key (field remains owned by the application).
     * zh_CN: 按唯一键名注册 Field（仍由应用管理，不会随控制器销毁）。
     */
    bool addField(const QString& key, layout::Field* field);
    bool removeField(const QString& key);
    QStringList fieldKeys() const;
    layout::Field* field(const QString& key) const;
    bool isFieldDirty(const QString& key) const;
    bool isFieldTouched(const QString& key) const;
    bool isDirty() const;
    bool isTouched() const;
    bool isValidating() const;
    bool isSubmitting() const { return m_submitting; }
    bool isValid() const;
    QStringList errorMessages() const;
    QStringList invalidFieldKeys() const;
    QString submissionError() const { return m_submissionError; }

    /**
     * @brief Marks a field as modified and invalidates pending validation or submit tokens.
     * zh_CN: 标记字段已修改，并使进行中的校验和提交请求失效。
     */
    void markFieldChanged(const QString& key);
    void setFieldTouched(const QString& key, bool touched = true);
    /** @brief Marks all registered fields clean without changing editor values. zh_CN: 将当前字段标记为未修改，不改变编辑器内容。 */
    void markClean();
    /** @brief Resets controller tracking state without changing editor values. zh_CN: 重置表单状态，不改变编辑器内容。 */
    void reset();

    /**
     * @brief Requests validation for @p key and returns a token for completeValidation().
     * zh_CN: 发起字段校验请求，并返回供 completeValidation() 使用的请求 token。
     */
    quint64 validateField(const QString& key);
    void validate();
    /**
     * @brief Finishes a pending validation matching @p token; an empty @p message on success clears error feedback.
     * zh_CN: 完成与 @p token 匹配的挂起校验；校验通过且 @p message 为空时清除错误提示。
     */
    bool completeValidation(const QString& key, quint64 token, bool valid, const QString& message = QString());
    /**
     * @brief Validates all fields and emits submitRequested() once if all pass; ignored while already submitting.
     * zh_CN: 校验全部字段，全部通过后发出一次 submitRequested()；提交进行中忽略重复调用。
     */
    bool submit();
    /**
     * @brief Finishes the active submission; marks fields clean on success and keeps edits on failure.
     * zh_CN: 完成当前提交流程；成功时清除修改标记，失败时保留编辑状态。
     */
    bool completeSubmit(quint64 token, bool success, const QString& message = QString());
    /** @brief Focuses the first invalid field editor in registration order. zh_CN: 将焦点移至按注册顺序首个校验失败的编辑器。 */
    bool focusFirstError();

signals:
    void stateChanged();
    void validationRequested(const QString& key, quint64 token);
    void submitRequested(quint64 token);
    void submissionFinished(bool success);

private:
    enum class Status { Unknown, Pending, Valid, Invalid };
    struct Entry {
        QString key;
        QPointer<layout::Field> field;
        QMetaObject::Connection destroyedConnection;
        QMetaObject::Connection editorConnection;
        QMetaObject::Connection labelConnection;
        quint64 token = 0;
        Status status = Status::Unknown;
        QString message;
        bool dirty = false;
        bool touched = false;
    };
    int indexOf(const QString& key) const;
    void cancelPending();
    void present(const QString& key);
    bool refreshPresentation();
    void requestValidation(const QStringList& keys);
    void settleSubmission();
    QVector<Entry> m_entries;
    quint64 m_serial = 0;
    quint64 m_submissionToken = 0;
    bool m_submitting = false;
    bool m_awaitingValidation = false;
    int m_dispatchDepth = 0;
    QString m_submissionError;
};

} // namespace fluent

#endif // FLUENTQT_FORMCONTROLLER_H
