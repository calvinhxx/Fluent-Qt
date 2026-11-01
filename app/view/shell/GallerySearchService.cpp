#include "GallerySearchService.h"

#include <algorithm>
#include <utility>

#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>

namespace fluent::gallery {

namespace {
// Routes whose sample lists are built per event-loop turn. zh_CN: 每轮事件循环构建示例列表的路由数。
constexpr int kIndexBatchSize = 4;
constexpr qint64 kIndexSliceMs = 4;
constexpr int kIndexPauseMs = 1;
} // namespace

GallerySearchService::GallerySearchService(QVector<search::Entry> routes, ExampleLoader loadExamples, QObject* parent)
    : QObject(parent), m_routes(std::move(routes)), m_entries(m_routes), m_loadExamples(std::move(loadExamples))
{
    if (!m_loadExamples)
        m_nextRoute = m_routes.size();
}

void GallerySearchService::requestResults(quint64 token, const QString& query, int offset, int count)
{
    cancelRequest(token);
    m_pending.append({token, query, qMax(0, offset), qMax(1, count), ++m_requestSequence});
    schedule();
}

void GallerySearchService::cancelRequest(quint64 token)
{
    m_pending.erase(std::remove_if(m_pending.begin(), m_pending.end(),
                        [token](const Request& request) { return request.token == token; }),
        m_pending.end());
}

QVector<search::Entry> GallerySearchService::rankedResults(const QString& query) const
{
    // Routes precede every example, so a route match is final even while the index is incomplete.
    // zh_CN: 路由排在所有示例之前，因此索引未完成时，路由匹配也已是确定的首选结果。
    return search::rankedEntries(isIndexComplete() ? m_entries : m_routes, query);
}

void GallerySearchService::schedule(int delayMs)
{
    if (m_scheduled)
        return;
    m_scheduled = true;
    QTimer::singleShot(delayMs, this, [this]() {
        m_scheduled = false;
        if (m_pending.isEmpty())
            return;
        if (!isIndexComplete()) {
            if (!indexBatch())
                return;
            if (!m_pending.isEmpty())
                schedule(kIndexPauseMs);
            return;
        }
        answerPending();
    });
}

bool GallerySearchService::indexBatch()
{
    QElapsedTimer elapsed;
    elapsed.start();
    const QPointer<GallerySearchService> guard(this);
    for (int count = 0; count < kIndexBatchSize && !isIndexComplete() && !m_pending.isEmpty(); ++count) {
        // Keep callback inputs alive even if it cancels requests or destroys its owner.
        // zh_CN: 即使回调取消请求或销毁所属对象，也保留本次调用所需的输入。
        const ExampleLoader load = m_loadExamples;
        const search::Entry route = m_routes.at(m_nextRoute);
        const QVector<search::Entry> examples = load(route);
        if (!guard)
            return false;
        m_entries += examples;
        ++m_nextRoute;
        if (elapsed.elapsed() >= kIndexSliceMs)
            break;
    }
    return true;
}

void GallerySearchService::answerPending()
{
    // Slots may cancel another pending reply, replace its token, or delete this service.
    // zh_CN: 槽函数可能取消其他待回复项、复用其 token，或销毁服务；新请求留到下一轮处理。
    const QVector<Request> requests = m_pending;
    const QPointer<GallerySearchService> guard(this);
    for (const Request& request : requests) {
        const auto pending = std::find_if(m_pending.begin(), m_pending.end(), [&request](const Request& item) {
            return item.sequence == request.sequence;
        });
        if (pending == m_pending.end())
            continue;
        m_pending.erase(pending);
        const QVector<search::Entry> ranked = search::rankedEntries(m_entries, request.query);
        const QVector<search::Entry> page = ranked.mid(request.offset, request.count);
        emit resultsReady(request.token, request.offset, page, request.offset + page.size() < ranked.size());
        if (!guard)
            return;
    }
}

} // namespace fluent::gallery
