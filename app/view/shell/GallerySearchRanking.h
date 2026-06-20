#ifndef GALLERYSEARCHRANKING_H
#define GALLERYSEARCHRANKING_H

#include <algorithm>

#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace fluent::gallery::search {

/**
 * @brief WinUI-Gallery parity search matching/ranking, shared by the title-bar suggestion box
 * and the shell's Enter-to-navigate resolution so both agree on what a query matches.
 * zh_CN: 仿 WinUI-Gallery 的搜索匹配/排序，标题栏建议框与外壳的回车导航共用，确保对「匹配什么」判断一致。
 */

/// Splits a query on whitespace into AND-matched tokens, so "split button" reaches SplitButton.
/// zh_CN: 按空白把查询拆成 AND 词元，使「split button」能命中 SplitButton。
inline QStringList splitTokens(const QString& query)
{
    return query.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

/// True when the title contains every token (case-insensitive). zh_CN: 标题包含每个词元（忽略大小写）时为真。
inline bool titleMatchesTokens(const QString& title, const QStringList& tokens)
{
    for (const QString& token : tokens) {
        if (!title.contains(token, Qt::CaseInsensitive))
            return false;
    }
    return true;
}

/// Filters then ranks like WinUI-Gallery: titles starting with the raw query come first, the rest
/// follow alphabetically (case-insensitive). zh_CN: 仿 WinUI-Gallery 过滤后排序：以原始查询为前缀的标题优先，其余按字母序。
inline QStringList rankedTitles(const QStringList& titles, const QString& query)
{
    const QString needle = query.trimmed();
    const QStringList tokens = splitTokens(needle);
    if (tokens.isEmpty())
        return titles;

    QStringList matched;
    matched.reserve(titles.size());
    for (const QString& title : titles) {
        if (titleMatchesTokens(title, tokens))
            matched.append(title);
    }

    std::stable_sort(matched.begin(), matched.end(),
                     [&needle](const QString& a, const QString& b) {
                         const bool aPrefix = a.startsWith(needle, Qt::CaseInsensitive);
                         const bool bPrefix = b.startsWith(needle, Qt::CaseInsensitive);
                         if (aPrefix != bPrefix)
                             return aPrefix;  // prefix matches rank ahead
                         return a.compare(b, Qt::CaseInsensitive) < 0;
                     });
    return matched;
}

} // namespace fluent::gallery::search

#endif // GALLERYSEARCHRANKING_H
