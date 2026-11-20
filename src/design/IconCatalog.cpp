#include "IconCatalog.h"

#include <algorithm>
#include <limits>
#include <utility>

#include <QFile>
#include <QStringView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <QPainter>
#include <QPainterPath>
#include <QPaintDevice>

#include "compatibility/FontCompat.h"
#include "utils/private/FluentQtLogging_p.h"
#include "utils/private/FluentQtResources_p.h"

namespace Typography::Icons {
namespace {

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

const QRegularExpression& iconNamePattern()
{
    static const QRegularExpression pattern(QStringLiteral("^(ic_fluent_.+)_([0-9]+)_regular$"));
    return pattern;
}

QVector<IconInfo> loadCatalog()
{
    fluent::resources::ensureRegistered();
    QFile file(QStringLiteral(":/res/icons/FluentQtIcons.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(fluent::logging::typographyCategory) << "Could not open the bundled Fluent icon catalog";
        return {};
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qCWarning(fluent::logging::typographyCategory)
            << "Could not parse the bundled Fluent icon catalog" << error.errorString();
        return {};
    }

    QVector<IconInfo> icons;
    const QJsonObject object = document.object();
    icons.reserve(object.size());
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        IconInfo info;
        info.name = it.key();
        info.codepoint = static_cast<quint32>(it.value().toDouble());

        const QRegularExpressionMatch match = iconNamePattern().match(info.name);
        if (match.hasMatch()) {
            QString familyName = match.captured(1);
            familyName.remove(QStringLiteral("ic_fluent_"));
            info.displayName = humanizeName(familyName);
            info.designSize = match.captured(2).toInt();
        } else {
            info.displayName = humanizeName(info.name);
        }
        icons.append(std::move(info));
    }

    std::sort(icons.begin(), icons.end(), [](const IconInfo& left, const IconInfo& right) {
        const int semanticOrder = QString::compare(left.displayName, right.displayName, Qt::CaseInsensitive);
        return semanticOrder == 0 ? left.designSize < right.designSize : semanticOrder < 0;
    });
    return icons;
}

struct Family {
    const char* name;
    int first;
    int count;
};
struct Variant {
    quint32 code;
    int size;
};
struct CodeFamily {
    quint32 code;
    int family;
};
#include "FluentQtIconIndex.inc"

struct NamedMatch {
    const Family* family = nullptr;
    quint32 code = 0;
};

NamedMatch findName(QStringView name)
{
    const QLatin1String prefix("ic_fluent_"), suffix("_regular");
    if (!name.startsWith(prefix) || !name.endsWith(suffix))
        return {};
    const auto stem = name.mid(prefix.size(), name.size() - prefix.size() - suffix.size());
    const auto split = stem.lastIndexOf(QLatin1Char('_'));
    if (split <= 0)
        return {};
    const auto number = stem.mid(split + 1);
    if (number.isEmpty() || number.size() > 3 || number.front() == QLatin1Char('0'))
        return {};
    int size = 0;
    for (QChar ch : number) {
        if (ch < QLatin1Char('0') || ch > QLatin1Char('9'))
            return {};
        size = size * 10 + ch.unicode() - '0';
    }
    const auto key = stem.left(split);
    const auto* family = std::lower_bound(std::begin(families), std::end(families), key,
        [](const Family& f, QStringView k) { return k.compare(QLatin1String(f.name)) > 0; });
    if (family == std::end(families) || key.compare(QLatin1String(family->name)) != 0)
        return {};
    for (int i = family->first; i < family->first + family->count; ++i)
        if (variants[i].size == size)
            return {family, variants[i].code};
    return {};
}

const Family* familyForGlyph(const QString& glyph)
{
    quint32 code = 0;
    if (glyph.size() == 1)
        code = glyph.front().unicode();
    else if (glyph.size() == 2 && glyph.front().isHighSurrogate() && glyph.back().isLowSurrogate())
        code = QChar::surrogateToUcs4(glyph.front(), glyph.back());
    else
        return nullptr;
    const auto* match = std::lower_bound(std::begin(codeFamilies), std::end(codeFamilies), code,
        [](const CodeFamily& entry, quint32 value) { return entry.code < value; });
    return match != std::end(codeFamilies) && match->code == code ? &families[match->family] : nullptr;
}

} // namespace

QString IconInfo::glyph() const
{
    return fluentCodepointGlyph(codepoint);
}

const QVector<IconInfo>& catalog()
{
    static const QVector<IconInfo> icons = loadCatalog();
    return icons;
}

QString glyph(const QString& name)
{
    const auto match = findName(name);
    return match.family ? fluentCodepointGlyph(match.code) : QString();
}

int snapIconPixelSize(int requestedPixelSize)
{
    // WinUI FontIcon crisp sizes plus the 12 px compact Fluent System Icons slot.
    static constexpr int kOpticalSizes[] = {12, 16, 20, 24, 28, 32, 40, 48, 64};
    if (requestedPixelSize <= 0)
        return 16;
    // Preserve 8-11 px micro slots used by FlipView, PipsPager, and spin buttons.
    if (requestedPixelSize < 12)
        return requestedPixelSize;

    int best = kOpticalSizes[0];
    int bestDelta = qAbs(requestedPixelSize - best);
    for (int size : kOpticalSizes) {
        const int delta = qAbs(requestedPixelSize - size);
        // Prefer larger on ties so intermediate sizes (e.g. 18/21) do not read undersized.
        if (delta < bestDelta || (delta == bestDelta && size > best)) {
            best = size;
            bestDelta = delta;
        }
    }
    return best;
}

QString glyphForSize(const QString& glyphOrName, int designSize)
{
    const auto named = findName(glyphOrName);
    const Family* family = named.family ? named.family : familyForGlyph(glyphOrName);
    if (!family || designSize <= 0)
        return named.family ? fluentCodepointGlyph(named.code) : glyphOrName;
    const int size = snapIconPixelSize(designSize);
    const Variant* best = &variants[family->first];
    for (int i = family->first + 1; i < family->first + family->count; ++i) {
        const auto& candidate = variants[i];
        const int delta = qAbs(candidate.size - size);
        const int bestDelta = qAbs(best->size - size);
        if (delta < bestDelta || (delta == bestDelta && candidate.size > best->size))
            best = &candidate;
    }
    return fluentCodepointGlyph(best->code);
}

QFont font(int pixelSize)
{
    QFont result(fluent::fontcompat::IconFamily);
    result.setPixelSize(snapIconPixelSize(pixelSize));
    // DirectWrite vertical grid fitting fattens 12-16 px icon strokes; fluentConfigureTextRendering
    // disables hinting there while keeping vertical hinting on FreeType/CoreText.
    fluentConfigureTextRendering(result);
    return result;
}

void paintGlyph(QPainter& painter,
    const QRectF& targetRect,
    const QString& glyphOrName,
    int pixelSize,
    Qt::Alignment alignment)
{
    if (glyphOrName.isEmpty() || targetRect.isEmpty() || pixelSize <= 0)
        return;

    const QString paintedGlyph = glyphForSize(glyphOrName, pixelSize);
    const QFont previousFont = painter.font();
    painter.setFont(font(pixelSize));

    // Draw into a device-pixel-aligned rect via QPainter::drawText rather than measuring ink
    // bounds with QPainterPath, keeping compact chevrons centered and sharp on DirectWrite.
    QRectF slot = targetRect;
    const qreal dpr = painter.device() ? painter.device()->devicePixelRatioF() : 1.0;
    if (dpr > 0.0) {
        const qreal left = qRound(slot.left() * dpr) / dpr;
        const qreal top = qRound(slot.top() * dpr) / dpr;
        const qreal right = qRound(slot.right() * dpr) / dpr;
        const qreal bottom = qRound(slot.bottom() * dpr) / dpr;
        slot = QRectF(QPointF(left, top), QPointF(right, bottom));
    }

    Qt::Alignment flags = alignment;
    if (!(flags & Qt::AlignHorizontal_Mask))
        flags |= Qt::AlignHCenter;
    if (!(flags & Qt::AlignVertical_Mask))
        flags |= Qt::AlignVCenter;

    painter.drawText(slot, flags, paintedGlyph);
    painter.setFont(previousFont);
}

} // namespace Typography::Icons
