#include "GallerySampleCatalog.h"

#include "samples/BasicInputSamples.h"
#include "samples/CollectionsSamples.h"
#include "samples/DateTimeSamples.h"
#include "samples/DialogsFlyoutsSamples.h"
#include "samples/MenusToolbarsSamples.h"
#include "samples/NavigationSamples.h"
#include "samples/ScrollingSamples.h"
#include "samples/StatusInfoSamples.h"
#include "samples/TextFieldsSamples.h"
#include "samples/WindowingSamples.h"

namespace fluent::gallery {

QVector<GallerySample> gallerySamplesForRoute(const QString& routeId)
{
    // Each category resolver answers only its own routes, so probing in
    // sequence keeps the dispatcher free of a duplicate route-to-category map.
    // zh_CN: 各分类解析器只响应自己的路由，顺序探测即可，无需重复维护路由→分类映射。
    using SampleResolver = QVector<GallerySample> (*)(const QString&);
    static constexpr SampleResolver resolvers[] = {
        &basicInputSamples,
        &collectionsSamples,
        &dateTimeSamples,
        &dialogsFlyoutsSamples,
        &menusToolbarsSamples,
        &navigationSamples,
        &scrollingSamples,
        &statusInfoSamples,
        &textFieldsSamples,
        &windowingSamples
    };

    for (SampleResolver resolver : resolvers) {
        QVector<GallerySample> samples = resolver(routeId);
        if (!samples.isEmpty())
            return samples;
    }
    return {};
}

} // namespace fluent::gallery
