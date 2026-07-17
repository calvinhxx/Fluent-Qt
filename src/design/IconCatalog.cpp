#include "IconCatalog.h"

#include <algorithm>
#include <utility>

#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include "compatibility/FontCompat.h"
#include "utils/private/FluentQtLogging_p.h"

namespace Typography::Icons {
namespace {

QString codepointGlyph(quint32 codepoint)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const char32_t value = static_cast<char32_t>(codepoint);
#else
    const uint value = codepoint;
#endif
    return QString::fromUcs4(&value, 1);
}

QString humanizeName(QString value)
{
    value.replace(QLatin1Char('_'), QLatin1Char(' '));
    bool capitalize = true;
    for (int index = 0; index < value.size(); ++index) {
        if (value.at(index).isSpace()) {
            capitalize = true;
        } else if (capitalize) {
            value[index] = value.at(index).toUpper();
            capitalize = false;
        }
    }
    return value;
}

QVector<IconInfo> loadCatalog()
{
    QFile file(QStringLiteral(":/res/icons/FluentQtIcons.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(fluent::logging::typographyCategory)
            << "Could not open the bundled Fluent icon catalog";
        return {};
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qCWarning(fluent::logging::typographyCategory)
            << "Could not parse the bundled Fluent icon catalog" << error.errorString();
        return {};
    }

    static const QRegularExpression namePattern(
        QStringLiteral("^ic_fluent_(.+)_([0-9]+)_regular$"));
    QVector<IconInfo> icons;
    const QJsonObject object = document.object();
    icons.reserve(object.size());
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        IconInfo info;
        info.name = it.key();
        info.codepoint = static_cast<quint32>(it.value().toDouble());

        const QRegularExpressionMatch match = namePattern.match(info.name);
        if (match.hasMatch()) {
            info.displayName = humanizeName(match.captured(1));
            info.designSize = match.captured(2).toInt();
        } else {
            info.displayName = humanizeName(info.name);
        }
        icons.append(std::move(info));
    }

    std::sort(icons.begin(), icons.end(), [](const IconInfo& left, const IconInfo& right) {
        const int semanticOrder = QString::compare(
            left.displayName, right.displayName, Qt::CaseInsensitive);
        return semanticOrder == 0 ? left.designSize < right.designSize
                                  : semanticOrder < 0;
    });
    return icons;
}

const QHash<QString, int>& catalogIndex()
{
    static const QHash<QString, int> index = [] {
        QHash<QString, int> result;
        const auto& icons = catalog();
        result.reserve(icons.size());
        for (int row = 0; row < icons.size(); ++row)
            result.insert(icons.at(row).name, row);
        return result;
    }();
    return index;
}

} // namespace

QString IconInfo::glyph() const
{
    return codepointGlyph(codepoint);
}

const QVector<IconInfo>& catalog()
{
    static const QVector<IconInfo> icons = loadCatalog();
    return icons;
}

QString glyph(const QString& name)
{
    const auto it = catalogIndex().constFind(name);
    return it == catalogIndex().constEnd() ? QString() : catalog().at(it.value()).glyph();
}

QFont font(int pixelSize)
{
    QFont result(fluent::fontcompat::IconFamily);
    result.setPixelSize(pixelSize);
    result.setHintingPreference(QFont::PreferNoHinting);
    result.setStyleStrategy(QFont::StyleStrategy(
        QFont::PreferAntialias | QFont::NoSubpixelAntialias));
    return result;
}

} // namespace Typography::Icons
