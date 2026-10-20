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
 * @brief Coordinates borrowed Fields without owning values, validators, or requests.
 * zh_CN: 协调借用的 Field，不接管字段值、校验器或请求。
 *
 * Connect validationRequested to application validation and return its token to
 * completeValidation. Editing, reset, removal and replacement invalidate old
 * tokens. Call markFieldChanged for every value change, including programmatic
 * changes. All methods and completions run on this object's GUI thread.
 * zh_CN: 应用响应 validationRequested，并携带 token 完成校验。编辑、重置、移除和
 * 替换会使旧 token 作废；包括程序修改在内的每次值变化都应调用 markFieldChanged。
 * 所有方法及完成回调均在此对象所属的 GUI 线程执行。
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
     * @brief Registers a unique, borrowed Field; neither it nor its editor is reparented.
     * zh_CN: 注册具有唯一标识的借用 Field，不改变它及其编辑器的父对象。
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
     * zh_CN: 标记一次编辑，并使进行中的校验和提交 token 作废。
     */
    void markFieldChanged(const QString& key);
    void setFieldTouched(const QString& key, bool touched = true);
    /** @brief Marks current values clean without writing editors. zh_CN: 将当前值标记为未修改，不写入编辑器。 */
    void markClean();
    /** @brief Clears coordination state, not editor values. zh_CN: 清除协调状态，不修改编辑器值。 */
    void reset();

    /**
     * @brief Requests validation; no connected validator leaves the request pending.
     * zh_CN: 请求校验；未连接校验器时请求保持待完成状态。
     */
    quint64 validateField(const QString& key);
    void validate();
    /**
     * @brief Accepts only the current token; an empty success message restores helper text.
     * zh_CN: 仅接受当前 token；成功文案为空时恢复帮助文字。
     */
    bool completeValidation(const QString& key, quint64 token, bool valid, const QString& message = QString());
    /**
     * @brief Validates all fields, then emits submitRequested once; duplicate submits are rejected.
     * zh_CN: 校验所有字段，通过后仅发送一次 submitRequested；拒绝重复提交。
     */
    bool submit();
    /**
     * @brief Completes the current submission; success marks values clean, failure keeps edits.
     * zh_CN: 完成当前提交；成功标记为未修改，失败保留编辑状态。
     */
    bool completeSubmit(quint64 token, bool success, const QString& message = QString());
    /** @brief Focuses the first registered invalid editor. zh_CN: 聚焦首个注册顺序中的无效编辑器。 */
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
