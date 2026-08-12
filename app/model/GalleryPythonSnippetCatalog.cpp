#include "GalleryPythonSnippetCatalog.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "support/logging/Log.h"

namespace fluent::gallery {
namespace {

constexpr int kExpectedSchemaVersion = 1;

QString snippetKey(const QString& routeId, const QString& sampleId)
{
    return routeId + QChar(0x001f) + sampleId;
}

GalleryPythonSnippetCatalog loadCatalog()
{
    QFile file(QStringLiteral(":/app/data/gallery_python_snippets.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_WARN(QStringLiteral(
            "Gallery Python snippet resource could not be opened"));
        return {};
    }
    return GalleryPythonSnippetCatalog::fromJson(file.readAll());
}

const GalleryPythonSnippetCatalog& catalogData()
{
    // The generated payload is loaded only when a component page first asks
    // for Python source. No Python runtime or preview widgets are retained.
    static const GalleryPythonSnippetCatalog data = loadCatalog();
    return data;
}

} // namespace

GalleryPythonSnippetCatalog GalleryPythonSnippetCatalog::fromJson(
    const QByteArray& payload)
{
    GalleryPythonSnippetCatalog data;

    QJsonParseError error;
    const QJsonDocument document =
        QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError
        || !document.isObject()) {
        LOG_WARN(QStringLiteral(
                     "Gallery Python snippet resource is invalid: %1")
                     .arg(error.errorString()));
        return data;
    }

    const QJsonObject root = document.object();
    const QJsonValue schemaVersion =
        root.value(QStringLiteral("schema_version"));
    const QJsonValue samplesValue = root.value(QStringLiteral("samples"));
    if (!schemaVersion.isDouble()
        || schemaVersion.toInt() != kExpectedSchemaVersion
        || !samplesValue.isArray()) {
        LOG_WARN(QStringLiteral(
            "Gallery Python snippet resource has an unsupported schema"));
        return data;
    }

    data.m_loaded = true;
    const QJsonArray samples = samplesValue.toArray();
    data.m_sources.reserve(samples.size());
    QSet<QString> seenKeys;
    QSet<QString> parsedRouteIds;
    for (const QJsonValue& value : samples) {
        if (!value.isObject()) {
            LOG_WARN(QStringLiteral(
                "Gallery Python snippet resource contains a non-object entry"));
            continue;
        }
        const QJsonObject sample = value.toObject();
        const QString routeId =
            sample.value(QStringLiteral("route_id")).toString();
        const QString sampleId =
            sample.value(QStringLiteral("sample_id")).toString();
        const QString source =
            sample.value(QStringLiteral("source")).toString();
        if (routeId.isEmpty() || sampleId.isEmpty()
            || source.trimmed().isEmpty()) {
            LOG_WARN(QStringLiteral(
                "Gallery Python snippet resource contains an incomplete entry"));
            if (!routeId.isEmpty())
                data.m_invalidRouteIds.insert(routeId);
            continue;
        }

        const QString key = snippetKey(routeId, sampleId);
        if (seenKeys.contains(key)) {
            LOG_WARN(QStringLiteral(
                         "Gallery Python snippet resource contains a duplicate entry: %1/%2")
                         .arg(routeId, sampleId));
            data.m_invalidRouteIds.insert(routeId);
            continue;
        }
        seenKeys.insert(key);
        parsedRouteIds.insert(routeId);
        data.m_sources.insert(key, source);
    }

    for (const QString& invalidRouteId : data.m_invalidRouteIds) {
        const QString prefix = invalidRouteId + QChar(0x001f);
        for (auto it = data.m_sources.begin(); it != data.m_sources.end();) {
            if (it.key().startsWith(prefix))
                it = data.m_sources.erase(it);
            else
                ++it;
        }
        parsedRouteIds.remove(invalidRouteId);
    }

    const QJsonObject summary =
        root.value(QStringLiteral("summary")).toObject();
    const int declaredSampleCount =
        summary.value(QStringLiteral("sample_count")).toInt(-1);
    const int declaredComponentCount =
        summary.value(QStringLiteral("component_count")).toInt(-1);
    if (declaredSampleCount != samples.size()
        || declaredComponentCount != parsedRouteIds.size()) {
        // Summary values are generated diagnostics, not a runtime allowlist.
        // Catalog growth must not disable otherwise complete routes.
        LOG_WARN(QStringLiteral(
            "Gallery Python snippet resource summary is stale; using parsed entries"));
    }
    return data;
}

QString GalleryPythonSnippetCatalog::snippet(const QString& routeId,
                                             const QString& sampleId) const
{
    if (!m_loaded || m_invalidRouteIds.contains(routeId))
        return {};
    return m_sources.value(snippetKey(routeId, sampleId));
}

bool GalleryPythonSnippetCatalog::hasCompleteRoute(
    const QString& routeId,
    const QStringList& sampleIds) const
{
    if (!m_loaded || routeId.isEmpty() || sampleIds.isEmpty()
        || m_invalidRouteIds.contains(routeId)) {
        return false;
    }
    for (const QString& sampleId : sampleIds) {
        if (sampleId.isEmpty()
            || !m_sources.contains(snippetKey(routeId, sampleId))) {
            return false;
        }
    }
    return true;
}

QString galleryPythonSnippet(const QString& routeId,
                             const QString& sampleId)
{
    return catalogData().snippet(routeId, sampleId);
}

bool galleryPythonSnippetsAvailable(const QString& routeId,
                                    const QStringList& sampleIds)
{
    return catalogData().hasCompleteRoute(routeId, sampleIds);
}

int galleryPythonSnippetCount()
{
    return catalogData().snippetCount();
}

} // namespace fluent::gallery
