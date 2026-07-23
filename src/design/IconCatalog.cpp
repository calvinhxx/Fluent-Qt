#include "IconCatalog.h"

#include <algorithm>
#include <limits>
#include <utility>

#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <QPainter>
#include <QPainterPath>
#include <QPaintDevice>

#include "compatibility/FontCompat.h"
#include "utils/private/FluentQtLogging_p.h"

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
    static const QRegularExpression pattern(
        QStringLiteral("^(ic_fluent_.+)_([0-9]+)_regular$"));
    return pattern;
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

const QHash<quint32, int>& catalogCodepointIndex()
{
    static const QHash<quint32, int> index = [] {
        QHash<quint32, int> result;
        const auto& icons = catalog();
        result.reserve(icons.size());
        for (int row = 0; row < icons.size(); ++row)
            result.insert(icons.at(row).codepoint, row);
        return result;
    }();
    return index;
}

const QHash<QString, QVector<int>>& catalogVariants()
{
    static const QHash<QString, QVector<int>> variants = [] {
        QHash<QString, QVector<int>> result;
        const auto& icons = catalog();
        for (int row = 0; row < icons.size(); ++row) {
            const QRegularExpressionMatch match = iconNamePattern().match(icons.at(row).name);
            if (match.hasMatch())
                result[match.captured(1)].append(row);
        }
        return result;
    }();
    return variants;
}

QHash<quint32, QString> loadAliasTargets()
{
    QFile file(QStringLiteral(":/res/icons/FluentQtIconAliases.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(fluent::logging::typographyCategory)
            << "Could not open the bundled Fluent semantic icon aliases";
        return {};
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qCWarning(fluent::logging::typographyCategory)
            << "Could not parse the bundled Fluent semantic icon aliases"
            << error.errorString();
        return {};
    }

    QHash<quint32, QString> result;
    const QJsonObject object = document.object();
    result.reserve(object.size());
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        bool ok = false;
        const quint32 codepoint = it.key().toUInt(&ok, 16);
        if (ok && it.value().isString())
            result.insert(codepoint, it.value().toString());
    }
    return result;
}

const QHash<quint32, QString>& aliasTargets()
{
    static const QHash<quint32, QString> targets = loadAliasTargets();
    return targets;
}

QString sourceNameFor(const QString& glyphOrName)
{
    if (catalogIndex().contains(glyphOrName))
        return glyphOrName;

    const auto codepoints = glyphOrName.toUcs4();
    if (codepoints.size() != 1)
        return {};

    const quint32 codepoint = static_cast<quint32>(codepoints.constFirst());
    const auto alias = aliasTargets().constFind(codepoint);
    if (alias != aliasTargets().constEnd())
        return alias.value();

    const auto catalogEntry = catalogCodepointIndex().constFind(codepoint);
    return catalogEntry == catalogCodepointIndex().constEnd()
        ? QString()
        : catalog().at(catalogEntry.value()).name;
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
    const auto it = catalogIndex().constFind(name);
    return it == catalogIndex().constEnd() ? QString() : catalog().at(it.value()).glyph();
}

int snapIconPixelSize(int requestedPixelSize)
{
    // WinUI FontIcon crisp sizes + Fluent System Icons compact/optical set.
    // zh_CN: WinUI FontIcon 清晰字号 + Fluent System Icons 紧凑/光学档。
    static constexpr int kOpticalSizes[] = {
        12, 16, 20, 24, 28, 32, 40, 48, 64
    };
    if (requestedPixelSize <= 0)
        return 16;
    // Preserve intentional micro slots (8–11) used by FlipView / PipsPager / spin.
    // zh_CN: 保留 FlipView / PipsPager / 微调钮等有意使用的 8–11 微槽。
    if (requestedPixelSize < 12)
        return requestedPixelSize;

    int best = kOpticalSizes[0];
    int bestDelta = qAbs(requestedPixelSize - best);
    for (int size : kOpticalSizes) {
        const int delta = qAbs(requestedPixelSize - size);
        // Prefer larger on ties so mid sizes like 18/21 do not look undersized.
        // zh_CN: 等距时取更大档，避免 18/21 等中间值视觉偏小。
        if (delta < bestDelta || (delta == bestDelta && size > best)) {
            best = size;
            bestDelta = delta;
        }
    }
    return best;
}

QString glyphForSize(const QString& glyphOrName, int designSize)
{
    QString fallback = glyphOrName;
    const auto namedEntry = catalogIndex().constFind(glyphOrName);
    if (namedEntry != catalogIndex().constEnd())
        fallback = catalog().at(namedEntry.value()).glyph();
    if (designSize <= 0)
        return fallback;

    const int snappedSize = snapIconPixelSize(designSize);

    const QString sourceName = sourceNameFor(glyphOrName);
    const QRegularExpressionMatch sourceMatch = iconNamePattern().match(sourceName);
    if (!sourceMatch.hasMatch())
        return fallback;

    const QString family = sourceMatch.captured(1);
    const QString exactName = family + QLatin1Char('_') + QString::number(snappedSize)
        + QStringLiteral("_regular");
    const auto exact = catalogIndex().constFind(exactName);
    if (exact != catalogIndex().constEnd())
        return catalog().at(exact.value()).glyph();

    const auto variants = catalogVariants().constFind(family);
    if (variants == catalogVariants().constEnd() || variants.value().isEmpty())
        return fallback;

    int bestRow = -1;
    int bestDelta = std::numeric_limits<int>::max();
    int bestSize = 0;
    for (int row : variants.value()) {
        const int candidateSize = catalog().at(row).designSize;
        const int delta = qAbs(candidateSize - snappedSize);
        const bool candidateAtOrAbove = candidateSize >= snappedSize;
        const bool bestAtOrAbove = bestSize >= snappedSize;
        if (delta < bestDelta
            || (delta == bestDelta && candidateAtOrAbove && !bestAtOrAbove)
            || (delta == bestDelta && candidateAtOrAbove == bestAtOrAbove
                && candidateSize < bestSize)) {
            bestRow = row;
            bestDelta = delta;
            bestSize = candidateSize;
        }
    }
    return bestRow >= 0 ? catalog().at(bestRow).glyph() : fallback;
}

QFont font(int pixelSize)
{
    QFont result(fluent::fontcompat::IconFamily);
    result.setPixelSize(snapIconPixelSize(pixelSize));
    // Match UI text: DirectWrite vertical grid fitting fattens compact Fluent
    // System Icon strokes (12–16 px caption / chevron / badge slots). Keep
    // vertical hinting on FreeType/CoreText where it sharpens without bloat.
    // zh_CN: 与 UI 正文一致：DirectWrite 纵向网格拟合会把紧凑 Fluent System Icon
    // 描边（12–16 px 标题栏/箭头/徽标槽）压粗；在 FreeType/CoreText 上保留纵向
    // hinting，清晰且不明显发胖。
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
    painter.setFont(font(pixelSize));

    // Draw into a device-pixel-aligned slot with Qt's text alignment. Measuring
    // ink via QPainterPath and placing a raw baseline often mis-centers compact
    // chevrons (ComboBox AlignRight on a full-width rect) and softens edges on
    // DirectWrite compared to drawText(rect, AlignCenter) used by SplitButton.
    // zh_CN: 在设备像素对齐的槽里用 Qt 文本对齐绘制。用 QPainterPath 量墨再摆基线
    // 容易让紧凑箭头偏心（ComboBox 整行 AlignRight），且在 DirectWrite 上比
    // SplitButton 的 drawText(rect, AlignCenter) 更易发虚。
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
}

} // namespace Typography::Icons
