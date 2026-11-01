#ifndef GALLERYSEARCHSERVICE_H
#define GALLERYSEARCHSERVICE_H

#include <functional>

#include <QObject>
#include <QVector>

#include "GallerySearchRanking.h"

namespace fluent::gallery {

/**
 * @brief Application-side search backend for the title-bar AutoSuggestBox in asynchronous mode.
 * zh_CN: 标题栏 AutoSuggestBox 异步模式的应用侧搜索后端。
 *
 * Synchronous lookup uses routes until the example index is complete. Suggestion requests build
 * the index in GUI-thread slices bounded by route count and a soft time budget, then return a
 * complete ranked page on a later turn. A single loader call cannot be preempted.
 * zh_CN: 同步查询在示例索引完成前仅使用路由；建议请求按路由数量与软时间预算在 GUI 线程分片建索引，
 * 完成后在后续事件循环返回完整排序页。单次加载器调用无法被抢占。
 */
class GallerySearchService : public QObject {
    Q_OBJECT
public:
    using ExampleLoader = std::function<QVector<search::Entry>(const search::Entry& route)>;

    GallerySearchService(QVector<search::Entry> routes, ExampleLoader loadExamples, QObject* parent = nullptr);

    /// Queues a ranked page; resultsReady() answers once the example index is complete.
    /// zh_CN: 排队请求一页排序结果；示例索引完成后由 resultsReady() 回复。
    void requestResults(quint64 token, const QString& query, int offset, int count);
    /// Drops a queued request whose token the box invalidated. zh_CN: 丢弃输入框已作废 token 的排队请求。
    void cancelRequest(quint64 token);
    /// Ranks known routes, or the complete index, without loading any examples.
    /// zh_CN: 仅排序已知路由或完整索引，不加载示例；冷启动的示例查询需使用 requestResults()。
    QVector<search::Entry> rankedResults(const QString& query) const;
    bool isIndexComplete() const { return m_nextRoute >= m_routes.size(); }

signals:
    void resultsReady(quint64 token, int offset, const QVector<fluent::gallery::search::Entry>& page, bool hasMore);

private:
    struct Request {
        quint64 token = 0;
        QString query;
        int offset = 0;
        int count = 0;
        quint64 sequence = 0;
    };

    void schedule(int delayMs = 0);
    bool indexBatch();
    void answerPending();

    QVector<search::Entry> m_routes;
    QVector<search::Entry> m_entries;
    ExampleLoader m_loadExamples;
    QVector<Request> m_pending;
    int m_nextRoute = 0;
    quint64 m_requestSequence = 0;
    bool m_scheduled = false;
};

} // namespace fluent::gallery

#endif // GALLERYSEARCHSERVICE_H
