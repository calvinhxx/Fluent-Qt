#ifndef GALLERYPYTHONSNIPPETCATALOG_H
#define GALLERYPYTHONSNIPPETCATALOG_H

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include "GalleryContentCatalog.h"

namespace fluent::gallery {

/**
 * @brief Parsed, route-aware Python teaching-source catalog.
 * zh_CN: 已解析、可按路由判断完整性的 Python 教学源码目录。
 */
class GalleryPythonSnippetCatalog {
public:
    static GalleryPythonSnippetCatalog fromJson(const QByteArray& payload);

    bool isLoaded() const { return m_loaded; }
    int snippetCount() const { return m_sources.size(); }
    QString snippet(const QString& routeId, const QString& sampleId) const;
    QVector<GallerySourceFile> sourceFiles(const QString& routeId, const QString& sampleId) const;
    bool hasCompleteRoute(const QString& routeId, const QStringList& sampleIds) const;

private:
    QHash<QString, QString> m_sources;
    QHash<QString, QVector<GallerySourceFile>> m_sourceFiles;
    QSet<QString> m_invalidRouteIds;
    bool m_loaded = false;
};

/**
 * @brief Returns the generated PySide6 teaching source for one native sample.
 * zh_CN: 返回某个原生示例对应的已生成 PySide6 教学源码。
 */
QString galleryPythonSnippet(const QString& routeId, const QString& sampleId);

/** @brief Returns the named Python and companion files for one sample.
 * zh_CN: 返回某个示例的命名 Python 源码与配套文件。 */
QVector<GallerySourceFile> galleryPythonSourceFiles(const QString& routeId,
                                                    const QString& sampleId);

/**
 * @brief Returns whether one route has Python source for every code sample.
 * zh_CN: 返回某路由的每个代码示例是否都有 Python 源码。
 */
bool galleryPythonSnippetsAvailable(const QString& routeId, const QStringList& sampleIds);

/** @brief Number of usable entries loaded from the generated resource. */
int galleryPythonSnippetCount();

} // namespace fluent::gallery

#endif // GALLERYPYTHONSNIPPETCATALOG_H
