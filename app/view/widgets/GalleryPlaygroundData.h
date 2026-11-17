#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <functional>

class QObject;

namespace fluent::gallery::playground_data {
using Quote = std::function<QString(const QString&, bool)>;
bool handles(const QJsonObject& field);
QJsonObject resolve(QJsonObject field, QObject* target);
QJsonValue read(const QJsonObject& field, QObject* target);
bool validate(const QJsonObject& field, const QJsonValue& value, QObject* target, QString* error);
void write(const QJsonObject& field, const QJsonValue& value, QObject* target);
QString code(const QJsonObject& field, const QJsonValue& value, QObject* target, bool python, const Quote& quote);
} // namespace fluent::gallery::playground_data
