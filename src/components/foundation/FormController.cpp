#include "components/foundation/FormController.h"

#include "components/layout/Field.h"

#include <algorithm>

namespace fluent {

FormController::FormController(QObject* parent) : QObject(parent) {}

FormController::~FormController()
{
    for (const auto& entry : m_entries) {
        disconnect(entry.destroyedConnection);
        disconnect(entry.editorConnection);
        disconnect(entry.labelConnection);
    }
}

int FormController::indexOf(const QString& key) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).key == key)
            return i;
    }
    return -1;
}

bool FormController::addField(const QString& key, layout::Field* field)
{
    if (key.trimmed().isEmpty() || !field || indexOf(key) >= 0)
        return false;
    for (const auto& entry : m_entries) {
        if (entry.field == field)
            return false;
    }
    cancelPending();
    Entry entry;
    entry.key = key;
    entry.field = field;
    entry.destroyedConnection = connect(field, &QObject::destroyed, this, [this, key]() { removeField(key); });
    entry.editorConnection = connect(field, &layout::Field::editorChanged, this, [this, key]() {
        markFieldChanged(key);
    });
    entry.labelConnection = connect(field, &layout::Field::labelTextChanged, this, &FormController::stateChanged);
    m_entries.append(entry);
    if (refreshPresentation())
        emit stateChanged();
    return true;
}

bool FormController::removeField(const QString& key)
{
    const int index = indexOf(key);
    if (index < 0)
        return false;
    disconnect(m_entries.at(index).destroyedConnection);
    disconnect(m_entries.at(index).editorConnection);
    disconnect(m_entries.at(index).labelConnection);
    m_entries.removeAt(index);
    cancelPending();
    if (refreshPresentation())
        emit stateChanged();
    return true;
}

QStringList FormController::fieldKeys() const
{
    QStringList result;
    for (const auto& entry : m_entries)
        result.append(entry.key);
    return result;
}

layout::Field* FormController::field(const QString& key) const
{
    const int index = indexOf(key);
    return index < 0 ? nullptr : m_entries.at(index).field.data();
}

bool FormController::isFieldDirty(const QString& key) const
{
    const int index = indexOf(key);
    return index >= 0 && m_entries.at(index).dirty;
}

bool FormController::isFieldTouched(const QString& key) const
{
    const int index = indexOf(key);
    return index >= 0 && m_entries.at(index).touched;
}

bool FormController::isDirty() const
{
    return std::any_of(m_entries.cbegin(), m_entries.cend(), [](const Entry& e) { return e.dirty; });
}

bool FormController::isTouched() const
{
    return std::any_of(m_entries.cbegin(), m_entries.cend(), [](const Entry& e) { return e.touched; });
}

bool FormController::isValidating() const
{
    return std::any_of(m_entries.cbegin(), m_entries.cend(), [](const Entry& e) {
        return e.status == Status::Pending;
    });
}

bool FormController::isValid() const
{
    return !m_entries.isEmpty() && std::all_of(m_entries.cbegin(), m_entries.cend(), [](const Entry& e) {
        return e.status == Status::Valid;
    });
}

QStringList FormController::invalidFieldKeys() const
{
    QStringList result;
    for (const auto& entry : m_entries) {
        if (entry.status == Status::Invalid)
            result.append(entry.key);
    }
    return result;
}

QStringList FormController::errorMessages() const
{
    QStringList result;
    for (const auto& entry : m_entries) {
        if (entry.status == Status::Invalid) {
            const QString label =
                entry.field && !entry.field->labelText().isEmpty() ? entry.field->labelText() : entry.key;
            result.append(label + QStringLiteral(": ") + entry.message);
        }
    }
    return result;
}

void FormController::cancelPending()
{
    m_submitting = false;
    m_awaitingValidation = false;
    m_submissionToken = 0;
    m_submissionError.clear();
    for (auto& entry : m_entries) {
        entry.token = 0;
        if (entry.status == Status::Pending) {
            entry.status = Status::Unknown;
            entry.message.clear();
        }
    }
}

void FormController::present(const QString& key)
{
    const int index = indexOf(key);
    if (index < 0)
        return;
    const Entry entry = m_entries.at(index);
    QPointer<FormController> guard(this);
    QPointer<layout::Field> field = entry.field;
    if (!field)
        return;
    using State = layout::Field::ValidationState;
    const auto state = entry.status == Status::Pending ? State::Validating
        : entry.status == Status::Invalid              ? State::Error
        : entry.status == Status::Valid && !entry.message.isEmpty()
        ? State::Success
        : State::None;
    field->setValidationState(state);
    if (!guard)
        return;
    const int current = indexOf(key);
    if (field && current >= 0 && m_entries.at(current).token == entry.token
        && m_entries.at(current).status == entry.status && m_entries.at(current).message == entry.message)
        field->setValidationMessage(entry.status == Status::Pending ? tr("Checking…") : entry.message);
}

void FormController::markFieldChanged(const QString& key)
{
    const int index = indexOf(key);
    if (index < 0)
        return;
    cancelPending();
    auto& entry = m_entries[index];
    entry.dirty = true;
    entry.status = Status::Unknown;
    entry.message.clear();
    const auto keys = fieldKeys();
    QPointer<FormController> guard(this);
    for (const auto& id : keys) {
        present(id);
        if (!guard)
            return;
    }
    emit stateChanged();
}

bool FormController::refreshPresentation()
{
    const auto keys = fieldKeys();
    QPointer<FormController> guard(this);
    for (const auto& key : keys) {
        present(key);
        if (!guard)
            return false;
    }
    return true;
}

void FormController::setFieldTouched(const QString& key, bool touched)
{
    const int index = indexOf(key);
    if (index < 0 || m_entries.at(index).touched == touched)
        return;
    m_entries[index].touched = touched;
    emit stateChanged();
}

void FormController::markClean()
{
    if (!isDirty())
        return;
    for (auto& entry : m_entries)
        entry.dirty = false;
    emit stateChanged();
}

void FormController::reset()
{
    const bool changed = isDirty() || isTouched() || isValidating() || isSubmitting() || !m_submissionError.isEmpty()
        || std::any_of(m_entries.cbegin(), m_entries.cend(), [](const Entry& e) {
               return e.status != Status::Unknown;
           });
    cancelPending();
    for (auto& entry : m_entries) {
        entry.status = Status::Unknown;
        entry.message.clear();
        entry.dirty = false;
        entry.touched = false;
    }
    QPointer<FormController> guard(this);
    const auto keys = fieldKeys();
    for (const auto& key : keys) {
        present(key);
        if (!guard)
            return;
    }
    if (changed)
        emit stateChanged();
}

void FormController::requestValidation(const QStringList& keys)
{
    QVector<QPair<QString, quint64>> requests;
    for (const auto& key : keys) {
        const int index = indexOf(key);
        if (index < 0)
            continue;
        auto& entry = m_entries[index];
        entry.token = ++m_serial;
        entry.status = Status::Pending;
        entry.message.clear();
        requests.append(qMakePair(key, entry.token));
    }
    ++m_dispatchDepth;
    QPointer<FormController> guard(this);
    for (const auto& request : requests) {
        present(request.first);
        if (!guard)
            return;
    }
    emit stateChanged();
    if (!guard)
        return;
    for (const auto& request : requests) {
        const int index = indexOf(request.first);
        if (index >= 0 && m_entries.at(index).token == request.second)
            emit validationRequested(request.first, request.second);
        if (!guard)
            return;
    }
    --m_dispatchDepth;
    settleSubmission();
}

quint64 FormController::validateField(const QString& key)
{
    if (indexOf(key) < 0 || m_submitting)
        return 0;
    // The token remains useful even when a synchronous validator completes it.
    const quint64 token = m_serial + 1;
    requestValidation({key});
    return token;
}

void FormController::validate()
{
    if (!m_submitting && !m_entries.isEmpty())
        requestValidation(fieldKeys());
}

bool FormController::completeValidation(const QString& key, quint64 token, bool valid, const QString& message)
{
    const int index = indexOf(key);
    if (!token || index < 0 || m_entries.at(index).token != token || m_entries.at(index).status != Status::Pending)
        return false;
    auto& entry = m_entries[index];
    entry.token = 0;
    entry.status = valid ? Status::Valid : Status::Invalid;
    entry.message = !valid && message.isEmpty() ? tr("Check this value.") : message;
    QPointer<FormController> guard(this);
    present(key);
    if (!guard)
        return true;
    emit stateChanged();
    if (guard)
        settleSubmission();
    return true;
}

bool FormController::submit()
{
    if (m_submitting || m_entries.isEmpty())
        return false;
    m_submitting = true;
    m_awaitingValidation = true;
    m_submissionError.clear();
    m_submissionToken = ++m_serial;
    for (auto& entry : m_entries)
        entry.touched = true;
    requestValidation(fieldKeys());
    return true;
}

void FormController::settleSubmission()
{
    if (!m_submitting || !m_awaitingValidation || m_dispatchDepth || isValidating())
        return;
    m_awaitingValidation = false;
    if (isValid()) {
        emit submitRequested(m_submissionToken);
        return;
    }
    m_submitting = false;
    m_submissionToken = 0;
    QPointer<FormController> guard(this);
    focusFirstError();
    if (!guard)
        return;
    emit stateChanged();
    if (guard)
        emit submissionFinished(false);
}

bool FormController::completeSubmit(quint64 token, bool success, const QString& message)
{
    if (!token || token != m_submissionToken || !m_submitting || m_awaitingValidation)
        return false;
    m_submitting = false;
    m_submissionToken = 0;
    m_submissionError = success ? QString() : (message.isEmpty() ? tr("Unable to save. Try again.") : message);
    if (success) {
        for (auto& entry : m_entries)
            entry.dirty = false;
    }
    QPointer<FormController> guard(this);
    emit stateChanged();
    if (guard)
        emit submissionFinished(success);
    return true;
}

bool FormController::focusFirstError()
{
    for (const auto& entry : m_entries) {
        if (entry.status == Status::Invalid && entry.field && entry.field->editor()
            && entry.field->editor()->isEnabled() && entry.field->editor()->isVisible()) {
            entry.field->editor()->setFocus(Qt::OtherFocusReason);
            return true;
        }
    }
    return false;
}

} // namespace fluent
