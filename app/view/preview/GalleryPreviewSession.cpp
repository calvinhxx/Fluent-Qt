#include "GalleryPreviewSession.h"
#include "GalleryPreviewActions.h"

#include <FluentQt/Diagnostics.h>
#include <QAccessible>
#include <QApplication>
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLineEdit>
#include <QMetaEnum>
#include <QMetaProperty>
#include <QSaveFile>
#include <QScopedValueRollback>
#include <QUuid>
#include <cmath>

namespace fluent::gallery {
namespace {

QJsonObject failure(const QString& message)
{
    return {{QStringLiteral("error"), message}};
}

bool writeJson(const QString& path, const QJsonObject& object)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return file.write(bytes) == bytes.size() && file.commit();
}

QStringList dataProperties()
{
    return {QStringLiteral("text"),
            QStringLiteral("plainText"),
            QStringLiteral("placeholderText"),
            QStringLiteral("checked"),
            QStringLiteral("checkState"),
            QStringLiteral("isOn"),
            QStringLiteral("value"),
            QStringLiteral("currentIndex"),
            QStringLiteral("selectedIndex"),
            QStringLiteral("selectedPageIndex"),
            QStringLiteral("enabled"),
            QStringLiteral("visible"),
            QStringLiteral("readOnly"),
            QStringLiteral("indeterminate"),
            QStringLiteral("isIndeterminate")};
}

bool passwordField(QWidget* widget)
{
    const auto* editor = qobject_cast<QLineEdit*>(widget);
    return editor && editor->echoMode() != QLineEdit::Normal;
}

QString stateKey(QWidget* widget)
{
    return QString::fromLatin1(widget->metaObject()->className()) + QLatin1Char(':') +
           widget->objectName();
}

QJsonObject rect(const QRect& value)
{
    return {{QStringLiteral("x"), value.x()},
            {QStringLiteral("y"), value.y()},
            {QStringLiteral("width"), value.width()},
            {QStringLiteral("height"), value.height()}};
}

} // namespace

GalleryPreviewSession::GalleryPreviewSession(QWidget* root, Capture capture)
    : QObject(), m_root(root), m_capture(std::move(capture)),
      m_sessionId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    m_timer.setInterval(80);
    connect(&m_timer, &QTimer::timeout, this, [this] { poll(); });
}

GalleryPreviewSession::~GalleryPreviewSession()
{
    m_timer.stop();
    if (!m_directory.isEmpty()) {
        writeJson(m_directory + QStringLiteral("/session.json"),
                  {{QStringLiteral("session_id"), m_sessionId},
                   {QStringLiteral("status"), QStringLiteral("closed")}});
    }
}

bool GalleryPreviewSession::start(const QString& directory, QString& error)
{
    if (!m_root) {
        error = QStringLiteral("The preview root is missing.");
        return false;
    }
    if (m_lock) {
        error = QStringLiteral("Session has already started.");
        return false;
    }
    QDir dir(directory);
    if (!dir.mkpath(QStringLiteral("requests")) || !dir.mkpath(QStringLiteral("responses"))) {
        error = QStringLiteral("Could not create the local session directory.");
        return false;
    }
    auto lock = std::make_unique<QLockFile>(dir.filePath(QStringLiteral("host.lock")));
    if (!lock->tryLock(0)) {
        error = QStringLiteral("Another preview owns this session directory.");
        return false;
    }
    m_directory = dir.absolutePath();
    m_lock = std::move(lock);
    refreshObjects();
    for (auto it = m_widgets.cbegin(); it != m_widgets.cend(); ++it) {
        if (it.value())
            m_defaults.insert(it.key(), data(it.value()));
    }
    if (!writeJson(
            dir.filePath(QStringLiteral("session.json")),
            {{QStringLiteral("schema_version"), 1},
             {QStringLiteral("session_id"), m_sessionId},
             {QStringLiteral("status"), QStringLiteral("ready")},
             {QStringLiteral("pid"), static_cast<double>(QCoreApplication::applicationPid())},
             {QStringLiteral("runtime"), QStringLiteral("cpp")}})) {
        error = QStringLiteral("Could not write session metadata.");
        m_lock.reset();
        m_directory.clear();
        return false;
    }
    m_timer.start();
    return true;
}

void GalleryPreviewSession::refreshObjects()
{
    if (!m_root)
        return;
    QList<QWidget*> widgets{m_root.data()};
    widgets.append(m_root->findChildren<QWidget*>());
    for (QWidget* widget : widgets) {
        if (m_ids.contains(widget))
            continue;
        const QString id = m_sessionId + QLatin1Char(':') + QString::number(++m_nextId);
        m_ids.insert(widget, id);
        m_widgets.insert(id, widget);
        connect(widget, &QObject::destroyed, this, [this, widget, id] {
            m_ids.remove(widget);
            m_widgets.remove(id);
            m_defaults.remove(id);
            m_originals.remove(id);
        });
    }
}

QWidget* GalleryPreviewSession::resolve(const QString& selector) const
{
    if (!m_root)
        return nullptr;
    if (selector == QStringLiteral("@root"))
        return m_root;
    if (m_widgets.contains(selector)) {
        QWidget* widget = m_widgets.value(selector);
        return widget && (widget == m_root || m_root->isAncestorOf(widget)) ? widget : nullptr;
    }
    if (selector.isEmpty())
        return nullptr;
    auto matches = m_root->findChildren<QWidget*>(selector);
    if (m_root->objectName() == selector)
        matches.prepend(m_root);
    return matches.size() == 1 ? matches.first() : nullptr;
}

QJsonObject GalleryPreviewSession::data(QWidget* widget) const
{
    QJsonObject fields;
    if (!widget)
        return fields;
    for (const QString& name : dataProperties()) {
        if (passwordField(widget) && name == QStringLiteral("text"))
            continue;
        const QByteArray key = name.toUtf8();
        const int index = widget->metaObject()->indexOfProperty(key.constData());
        if (index < 0)
            continue;
        const auto property = widget->metaObject()->property(index);
        if (!property.isReadable() || !property.isWritable())
            continue;
        const QJsonValue value = QJsonValue::fromVariant(property.read(widget));
        if (!value.isBool() && !value.isDouble() && !value.isString())
            continue;
        QJsonObject field{{QStringLiteral("value"), value},
                          {QStringLiteral("type"), QString::fromLatin1(property.typeName())}};
        if (property.isEnumType()) {
            QJsonObject choices;
            const auto enumeration = property.enumerator();
            for (int i = 0; i < enumeration.keyCount(); ++i)
                choices.insert(QString::fromLatin1(enumeration.key(i)), enumeration.value(i));
            field.insert(QStringLiteral("choices"), choices);
        }
        fields.insert(name, field);
    }
    return fields;
}

QJsonObject GalleryPreviewSession::inspect()
{
    refreshObjects();
    QJsonArray nodes;
    QList<QWidget*> widgets{m_root.data()};
    widgets.append(m_root->findChildren<QWidget*>());
    const int count = qMin(2000, static_cast<int>(widgets.size()));
    for (int i = 0; i < count; ++i) {
        QWidget* widget = widgets.at(i);
        auto* accessible = QAccessible::queryAccessibleInterface(widget);
        nodes.append(QJsonObject{
            {QStringLiteral("id"), m_ids.value(widget)},
            {QStringLiteral("parent_id"), m_ids.value(widget->parentWidget())},
            {QStringLiteral("object_name"), widget->objectName()},
            {QStringLiteral("class"), QString::fromLatin1(widget->metaObject()->className())},
            {QStringLiteral("accessible_name"),
             accessible ? accessible->text(QAccessible::Name) : widget->accessibleName()},
            {QStringLiteral("accessible_role"), accessible ? int(accessible->role()) : 0},
            {QStringLiteral("rect"), rect(QRect(widget->mapTo(m_root, QPoint()), widget->size()))},
            {QStringLiteral("visible"), widget->isVisible()},
            {QStringLiteral("enabled"), widget->isEnabled()},
            {QStringLiteral("data"), data(widget)}});
    }
    return {{QStringLiteral("session_id"), m_sessionId},
            {QStringLiteral("nodes"), nodes},
            {QStringLiteral("truncated"), widgets.size() > count},
            {QStringLiteral("inspector"), fluent::diagnostics::Inspector::report(m_root)}};
}

QJsonObject GalleryPreviewSession::setData(QWidget* widget, const QString& name,
                                           const QJsonValue& value, bool rememberOriginal)
{
    const auto fields = data(widget);
    if (!fields.contains(name))
        return failure(QStringLiteral("Property is not exposed as preview data."));
    const auto oldValue = fields.value(name).toObject().value(QStringLiteral("value"));
    const QByteArray key = name.toUtf8();
    const auto property =
        widget->metaObject()->property(widget->metaObject()->indexOfProperty(key.constData()));
    if ((oldValue.isBool() && !value.isBool()) ||
        (oldValue.isString() && (!value.isString() || value.toString().size() > 32768)) ||
        (oldValue.isDouble() && (!value.isDouble() || !std::isfinite(value.toDouble()))))
        return failure(QStringLiteral("Value does not match the property's data type or size."));
    if (property.isEnumType() && !property.enumerator().valueToKey(value.toInt(-99999)))
        return failure(QStringLiteral("Value is not a declared enumeration member."));
    const int type = property.userType();
    if (oldValue.isDouble() && type != QMetaType::Double && type != QMetaType::Float &&
        (value.toDouble() != std::floor(value.toDouble()) || value.toDouble() < -2147483648.0 ||
         value.toDouble() > 2147483647.0))
        return failure(QStringLiteral("Use an integer in the supported range."));
    const QString id = m_ids.value(widget);
    QPointer<QWidget> guard(widget);
    if (rememberOriginal && !m_originals.value(id).contains(name))
        m_originals[id].insert(name, oldValue);
    if (!property.write(widget, value.toVariant()))
        return failure(QStringLiteral("Qt rejected the data value."));
    return {{QStringLiteral("applied"), true},
            {QStringLiteral("data"), guard ? data(guard) : QJsonObject{}},
            {QStringLiteral("target_removed"), !guard}};
}

QJsonObject GalleryPreviewSession::state() const
{
    QJsonArray values;
    for (auto it = m_defaults.cbegin(); it != m_defaults.cend(); ++it) {
        QWidget* widget = m_widgets.value(it.key());
        if (!widget || widget->objectName().isEmpty() || resolve(widget->objectName()) != widget)
            continue;
        const auto current = data(widget);
        QJsonObject changed;
        for (auto field = current.begin(); field != current.end(); ++field) {
            if (it.value().contains(field.key()) && it.value().value(field.key()) != field.value())
                changed.insert(field.key(),
                               field.value().toObject().value(QStringLiteral("value")));
        }
        if (!changed.isEmpty())
            values.append(QJsonObject{{QStringLiteral("object_name"), widget->objectName()},
                                      {QStringLiteral("class_key"), stateKey(widget)},
                                      {QStringLiteral("values"), changed}});
    }
    return {{QStringLiteral("widgets"), values}};
}

QJsonObject GalleryPreviewSession::execute(const QString& method, const QJsonObject& parameters)
{
    if (!m_root)
        return failure(QStringLiteral("The preview window was destroyed."));
    refreshObjects();
    if (method == QStringLiteral("inspect") || method == QStringLiteral("data"))
        return inspect();
    if (method == QStringLiteral("screenshot")) {
        QByteArray png;
        if (m_capture) {
            png = m_capture();
        } else {
            QBuffer buffer(&png);
            buffer.open(QIODevice::WriteOnly);
            m_root->grab().save(&buffer, "PNG");
        }
        if (png.isEmpty() || png.size() > 20 * 1024 * 1024)
            return failure(QStringLiteral("Snapshot is empty or exceeds 20 MiB."));
        return {{QStringLiteral("mime_type"), QStringLiteral("image/png")},
                {QStringLiteral("data"), QString::fromLatin1(png.toBase64())}};
    }
    if (method == QStringLiteral("capture_state"))
        return state();
    if (method == QStringLiteral("restore_state")) {
        int restored = 0;
        for (const auto& item : parameters.value(QStringLiteral("widgets")).toArray()) {
            const auto entry = item.toObject();
            QPointer<QWidget> widget =
                resolve(entry.value(QStringLiteral("object_name")).toString());
            if (!widget || stateKey(widget) != entry.value(QStringLiteral("class_key")).toString())
                continue;
            const auto values = entry.value(QStringLiteral("values")).toObject();
            for (auto value = values.begin(); value != values.end(); ++value) {
                if (!widget)
                    break;
                if (!setData(widget, value.key(), value.value()).contains(QStringLiteral("error")))
                    ++restored;
            }
        }
        return {{QStringLiteral("restored"), restored}};
    }
    if (method == QStringLiteral("reset_data")) {
        const auto originals = m_originals;
        QHash<QString, QJsonObject> writeErrors;
        for (auto object = originals.cbegin(); object != originals.cend(); ++object) {
            QPointer<QWidget> widget = resolve(object.key());
            for (auto property = object.value().begin(); property != object.value().end();
                 ++property) {
                if (!widget || resolve(object.key()) != widget)
                    break;
                const auto result = setData(widget, property.key(), property.value(), false);
                if (result.contains(QStringLiteral("error")))
                    writeErrors[object.key()].insert(property.key(),
                                                     result.value(QStringLiteral("error")));
            }
        }
        // Verify after all setters: one restored property may change another.
        int restored = 0;
        QJsonArray errors;
        for (auto object = originals.cbegin(); object != originals.cend(); ++object) {
            QWidget* widget = resolve(object.key());
            const auto current = widget ? data(widget) : QJsonObject{};
            for (auto property = object.value().begin(); property != object.value().end();
                 ++property) {
                if (current.contains(property.key()) &&
                    current.value(property.key()).toObject().value(QStringLiteral("value")) ==
                        property.value()) {
                    m_originals[object.key()].remove(property.key());
                    ++restored;
                } else {
                    QString message =
                        writeErrors.value(object.key()).value(property.key()).toString();
                    if (message.isEmpty())
                        message =
                            widget ? QStringLiteral("The widget did not retain the original value.")
                                   : QStringLiteral("The target was removed or left the preview.");
                    errors.append(QJsonObject{{QStringLiteral("target"), object.key()},
                                              {QStringLiteral("property"), property.key()},
                                              {QStringLiteral("message"), message}});
                }
            }
            if (m_originals.value(object.key()).isEmpty())
                m_originals.remove(object.key());
        }
        int pending = 0;
        for (const auto& values : m_originals)
            pending += values.size();
        QJsonObject result{{QStringLiteral("restored"), restored},
                           {QStringLiteral("failed"), errors.size()},
                           {QStringLiteral("pending"), pending},
                           {QStringLiteral("errors"), errors}};
        if (!errors.isEmpty())
            result.insert(QStringLiteral("error"),
                          QStringLiteral("Some values could not be restored. Retained values can "
                                         "be retried with reset_data."));
        return result;
    }
    const QString selector = parameters.value(QStringLiteral("target")).toString();
    QWidget* target = resolve(selector);
    if (!target)
        return failure(
            QStringLiteral("Unknown, stale or ambiguous target. Inspect the preview again."));
    if (method == QStringLiteral("set_data"))
        return setData(target, parameters.value(QStringLiteral("property")).toString(),
                       parameters.value(QStringLiteral("value")));
    if (method == QStringLiteral("action")) {
        if (!target->isVisible() || !target->isEnabled())
            return failure(QStringLiteral("Control must be visible and enabled."));
        const QString action = parameters.value(QStringLiteral("action")).toString();
        if (!QStringList{QStringLiteral("click"), QStringLiteral("focus"),
                         QStringLiteral("type_text"), QStringLiteral("key")}
                 .contains(action))
            return failure(QStringLiteral("Supported actions: click, focus, type_text, key."));
        if (parameters.value(QStringLiteral("text")).toString().size() > 32768)
            return failure(QStringLiteral("Input is too long."));
        QJsonObject step;
        for (const QString& key :
             {QStringLiteral("target"), QStringLiteral("action"), QStringLiteral("text"),
              QStringLiteral("key"), QStringLiteral("modifiers")}) {
            if (parameters.contains(key))
                step.insert(key, parameters.value(key));
        }
        QPointer<QWidget> guard(target);
        target->setFocus(Qt::OtherFocusReason);
        if (!guard || !m_root)
            return failure(QStringLiteral("Target was removed while focusing."));
        const auto result = executeGalleryPreviewActions(
            m_root,
            {{QStringLiteral("schema_version"), 1}, {QStringLiteral("steps"), QJsonArray{step}}},
            QString(), [this](const QString& id) { return resolve(id); });
        if (!result.passed)
            return {{QStringLiteral("error"), QStringLiteral("Interaction failed.")},
                    {QStringLiteral("interaction"), result.report}};
        return result.report;
    }
    return failure(QStringLiteral("Unknown preview command."));
}

void GalleryPreviewSession::poll()
{
    if (m_busy || !m_root)
        return;
    QScopedValueRollback<bool> busy(m_busy, true);
    QPointer<GalleryPreviewSession> guard(this);
    QDir requests(m_directory + QStringLiteral("/requests"));
    const auto names = requests.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    if (names.isEmpty())
        return;
    const QString name = names.first();
    QFile file(requests.filePath(name));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QByteArray bytes = file.read(256 * 1024 + 1);
    file.close();
    file.remove();
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes, &parseError);
    const auto request = document.object();
    QJsonObject result;
    if (bytes.size() > 256 * 1024 || parseError.error != QJsonParseError::NoError ||
        !document.isObject())
        result = failure(QStringLiteral("Invalid or oversized request."));
    else if (request.value(QStringLiteral("session_id")).toString() != m_sessionId)
        result = failure(QStringLiteral("Session changed. Reconnect to the current preview."));
    else if (!request.value(QStringLiteral("parameters")).isObject())
        result = failure(QStringLiteral("Request parameters must be an object."));
    else
        result = execute(request.value(QStringLiteral("method")).toString(),
                         request.value(QStringLiteral("parameters")).toObject());
    if (!guard)
        return;
    writeJson(m_directory + QStringLiteral("/responses/") + name,
              {{QStringLiteral("id"), request.value(QStringLiteral("id"))},
               {QStringLiteral("session_id"), m_sessionId},
               {QStringLiteral("result"), result}});
}

} // namespace fluent::gallery
