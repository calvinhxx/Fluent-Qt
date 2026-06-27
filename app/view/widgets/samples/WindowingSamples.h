#ifndef GALLERYWINDOWINGSAMPLES_H
#define GALLERYWINDOWINGSAMPLES_H

#include <QString>
#include <QVector>

#include "components/foundation/FluentElement.h"
#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

/**
 * @brief Which platform caption controls the TitleBar sample renders.
 * zh_CN: TitleBar 示例渲染哪种平台标题栏控件。
 *
 * WindowsCaptionButtons is the trailing minimize/maximize/close glyph row (Fluent + Material);
 * MacTrafficLights is the leading red/yellow/green dot cluster (Cupertino). The choice is driven
 * by the active DESIGN LANGUAGE, not the host OS, so the sample showcases the real platform
 * difference regardless of where the gallery runs.
 * zh_CN: WindowsCaptionButtons 为尾部 最小化/最大化/关闭 字形按钮行(Fluent + Material);
 * MacTrafficLights 为前导 红/黄/绿 圆点簇(Cupertino)。选择依据是当前**设计语言**而非宿主系统,
 * 故无论 gallery 运行在何处,示例都能展示真实的平台差异。
 */
enum class TitleBarCaptionStyle { WindowsCaptionButtons, MacTrafficLights };

/**
 * @brief Maps a design language to its characteristic title-bar caption style.
 * zh_CN: 将设计语言映射到其标志性的标题栏 caption 样式。
 */
TitleBarCaptionStyle captionStyleForDesignLanguage(fluent::FluentElement::DesignLanguage language);

/**
 * @brief Live samples for the Windowing category routes; empty when uncovered.
 * zh_CN: Windowing 分类路由的实样；未覆盖时返回空。
 */
QVector<GallerySample> windowingSamples(const QString& routeId);

} // namespace fluent::gallery

#endif // GALLERYWINDOWINGSAMPLES_H
