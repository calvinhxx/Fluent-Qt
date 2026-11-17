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
 * @brief Coordinates Field controls without owning their values, validators, or requests.
 * zh_CN: 协调一组 Field 控件的状态与校验流程，不接管字段值、校验器或提交请求的所有权。
 *
 * Connect validationRequested to application validation and pass its token back to
 * completeValidation(). Editing, reset, removal, and replacement invalidate older
 * tokens. Call markFieldChanged() on every value change, including programmatic
 * updates. All methods and callbacks run on this object's GUI thread.
 * zh_CN: 应用响应 validationRequested 后携带对应 token 完成校验。编辑、重置、移除和
 * 替换字段都会使旧 token 作废；任何值变更（包括代码修改）都应调用 markFieldChanged()。
 * 所有方法及回调均须在此对象所属的 GUI 线程执行。
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
     * @brief Registers a Field under a unique key without taking ownership or reparenting it or its editor.
     * zh_CN: 以唯一键名注册 Field，不接管所有权，也不改变它或编辑器的父对象。
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
     * @brief Marks an edit and invalidates in-flight validation and submission tokens.
     * zh_CN: 标记字段已编辑，并使进行中的校验和提交 token 作废。
     */
    void markFieldChanged(const QString& key);
    void setFieldTouched(const QString& key, bool touched = true);
    /** @brief Marks current fields clean without modifying editors. zh_CN: 将当前字段标记为未修改，不改动编辑器内容。 */
    void markClean();
    /** @brief Clears coordination state without modifying editor values. zh_CN: 重置协调状态，不改动编辑器内容。 */
    void reset();

    /**
     * @brief Requests validation; leaves the request pending until completeValidation() is called.
     * zh_CN: 请求校验；未连接校验器时请求保持待完成状态。
     */
    quint64 validateField(const QString& key);
    void validate();
    /**
     * @brief Completes the matching pending validation; an empty success message clears validation feedback.
     * zh_CN: 仅当 token 匹配当前挂起请求时生效；校验通过且 message 为空时恢复提示文案。
     */
    bool completeValidation(const QString& key, quint64 token, bool valid, const QString& message = QString());
    /**
     * @brief Validates all fields, then emits submitRequested once; rejects duplicate calls while submitting.
     * zh_CN: 校验全部字段，全部通过后仅发出一次 submitRequested；提交进行中拒绝重复调用。
     */
    bool submit();
    /**
     * @brief Completes the current submission; success marks fields clean, failure keeps edits.
     * zh_CN: 完成当前提交；成功时清除已修改标记，失败时保留编辑状态。
     */
    bool completeSubmit(quint64 token, bool success, const QString& message = QString());
    /** @brief Focuses the first invalid editor in registration order. zh_CN: 将焦点移至按注册顺序找到的第一个校验失败的编辑器。 */
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
