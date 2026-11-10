#include "DiagnosticSession.h"

#include <QJsonDocument>
#include <QRegularExpression>

namespace fluent::diagnostics {
namespace {
// Stored ZIP entries keep export dependency-free and readable by ordinary archive tools.
// zh_CN: 使用无压缩 ZIP 条目，不引入额外依赖，可直接由常见解压工具读取。
void u16(QByteArray& bytes, quint16 value)
{
    bytes.append(char(value & 255));
    bytes.append(char(value >> 8));
}
void u32(QByteArray& bytes, quint32 value)
{
    u16(bytes, quint16(value));
    u16(bytes, quint16(value >> 16));
}
quint32 crc32(const QByteArray& bytes)
{
    quint32 crc = 0xffffffffu;
    for (unsigned char byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
} // namespace

QByteArray DiagnosticSession::createBundle(const QVariantMap& report, const QVariantMap& attachments)
{
    constexpr qint64 maximumBytes = 20 * 1024 * 1024;
    if (report.isEmpty() || attachments.size() > 8 || attachments.contains(QStringLiteral("report.json")))
        return {};
    QVariantMap files = attachments;
    files.insert(QStringLiteral("report.json"), QJsonDocument::fromVariant(report).toJson());
    qint64 total = 22;
    const QRegularExpression safeName(QStringLiteral("\\A[A-Za-z0-9][A-Za-z0-9_.-]{0,79}\\z"));
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        if (!safeName.match(it.key()).hasMatch() || it.key().contains(QStringLiteral(".."))
            || it.value().userType() != QMetaType::QByteArray)
            return {};
        total += it.value().toByteArray().size() + 76 + 2 * it.key().toUtf8().size();
        if (total > maximumBytes)
            return {};
    }
    QByteArray result, central;
    result.reserve(int(total));
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        const auto name = it.key().toUtf8();
        const auto data = it.value().toByteArray();
        const auto crc = crc32(data);
        const auto offset = quint32(result.size());
        u32(result, 0x04034b50);
        u16(result, 20);
        u16(result, 0);
        u16(result, 0);
        u16(result, 0);
        u16(result, 33);
        u32(result, crc);
        u32(result, quint32(data.size()));
        u32(result, quint32(data.size()));
        u16(result, quint16(name.size()));
        u16(result, 0);
        result.append(name);
        result.append(data);
        u32(central, 0x02014b50);
        u16(central, 20);
        u16(central, 20);
        u16(central, 0);
        u16(central, 0);
        u16(central, 0);
        u16(central, 33);
        u32(central, crc);
        u32(central, quint32(data.size()));
        u32(central, quint32(data.size()));
        u16(central, quint16(name.size()));
        u16(central, 0);
        u16(central, 0);
        u16(central, 0);
        u16(central, 0);
        u32(central, 0);
        u32(central, offset);
        central.append(name);
    }
    const auto centralOffset = quint32(result.size());
    result.append(central);
    u32(result, 0x06054b50);
    u16(result, 0);
    u16(result, 0);
    u16(result, quint16(files.size()));
    u16(result, quint16(files.size()));
    u32(result, quint32(central.size()));
    u32(result, centralOffset);
    u16(result, 0);
    return result;
}
} // namespace fluent::diagnostics
