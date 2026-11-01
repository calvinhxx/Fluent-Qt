#ifndef GALLERYSEARCHRANKING_H
#define GALLERYSEARCHRANKING_H

#include <algorithm>
#include <utility>

#include <QMetaType>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fluent::gallery::search {

/**
 * @brief WinUI-Gallery parity search matching/ranking, shared by the title-bar suggestion box
 * and the shell's Enter-to-navigate resolution so both agree on what a query matches.
 * zh_CN: 仿 WinUI-Gallery 的搜索匹配/排序，标题栏建议框与外壳的回车导航共用，确保对「匹配什么」判断一致。
 */

/// One searchable destination: a route, or an example shown on that route's page.
/// zh_CN: 一个可搜索的目标：路由，或该路由页面中展示的示例。
struct Entry {
    QString routeId;
    QString title;
    /// Owning page title for an example; empty for a route. zh_CN: 示例所属页面标题；路由为空。
    QString context;

    bool isExample() const { return !context.isEmpty(); }
    /// Row text in the suggestion list. zh_CN: 建议列表中的行文本。
    QString text() const { return isExample() ? title + QStringLiteral(" · ") + context : title; }
};

/// Splits a query on whitespace into AND-matched tokens, so "split button" reaches SplitButton.
/// zh_CN: 按空白把查询拆成 AND 词元，使「split button」能命中 SplitButton。
inline QStringList splitTokens(const QString& query)
{
    return query.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

/// True when the text contains every token (case-insensitive). zh_CN: 文本包含每个词元（忽略大小写）时为真。
inline bool containsTokens(const QString& text, const QStringList& tokens)
{
    for (const QString& token : tokens) {
        if (!text.contains(token, Qt::CaseInsensitive))
            return false;
    }
    return true;
}

/// Filters on every token and ranks like WinUI-Gallery. Routes precede examples, so controls lead the
/// list; each group orders exact title, title prefix, title match, then examples that need their page
/// title to match, and finally text alphabetically.
/// zh_CN: 按全部词元过滤并仿 WinUI-Gallery 排序。路由先于示例，使控件排在列表前部；各组内依次为精确标题、标题前缀、
/// 标题匹配、需借助所属页面标题才匹配的示例，最后按文本字母序。
inline QVector<Entry> rankedEntries(const QVector<Entry>& entries, const QString& query)
{
    const QString needle = query.trimmed();
    const QStringList tokens = splitTokens(needle);
    if (tokens.isEmpty())
        return {};

    struct Ranked {
        int tier = 0;
        const Entry* entry = nullptr;
    };
    QVector<Ranked> ranked;
    for (const Entry& entry : entries) {
        int tier = 0;
        if (entry.title.compare(needle, Qt::CaseInsensitive) == 0)
            tier = 0;
        else if (entry.title.startsWith(needle, Qt::CaseInsensitive))
            tier = 1;
        else if (containsTokens(entry.title, tokens))
            tier = 2;
        else if (entry.isExample() && containsTokens(entry.title + QLatin1Char(' ') + entry.context, tokens))
            tier = 3;
        else
            continue;
        ranked.append({tier, &entry});
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const Ranked& a, const Ranked& b) {
        if (a.entry->isExample() != b.entry->isExample())
            return !a.entry->isExample();
        if (a.tier != b.tier)
            return a.tier < b.tier;
        return a.entry->text().compare(b.entry->text(), Qt::CaseInsensitive) < 0;
    });

    QVector<Entry> result;
    result.reserve(ranked.size());
    for (const Ranked& item : std::as_const(ranked))
        result.append(*item.entry);
    return result;
}

} // namespace fluent::gallery::search

Q_DECLARE_METATYPE(fluent::gallery::search::Entry)

#endif // GALLERYSEARCHRANKING_H
