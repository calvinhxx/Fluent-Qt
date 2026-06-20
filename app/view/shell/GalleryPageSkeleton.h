#ifndef GALLERYPAGESKELETON_H
#define GALLERYPAGESKELETON_H

#include <QWidget>

namespace fluent::gallery {

/**
 * @brief Lightweight shimmer skeleton shown while a content page is being built.
 * zh_CN: 内容页构建期间显示的轻量 shimmer 骨架屏。
 *
 * Reused as a single resident stack page: the presenter switches to it the instant a
 * cold route is requested (0 ms feedback), then swaps in the real page once built. Its
 * blocks roughly echo a GalleryContentPage — a title bar, a subtitle, then a few cards —
 * so handing over to real content reads as "loading", not a flash. Built from Shimmer
 * (ImageCard template) blocks, which re-fit their bounds on resize, so it stays responsive.
 * zh_CN: 作为单个常驻栈页复用：请求冷路由时 presenter 立刻切到它（0ms 反馈），真页建好后换入。
 * 其骨架块大致呼应 GalleryContentPage——标题栏、副标题、几张卡片——使切到真内容读起来像「加载中」
 * 而非闪烁。由 Shimmer（ImageCard 模板）块构成，块会在 resize 时重适配 bounds，故随宽度自适应。
 */
class GalleryPageSkeleton : public QWidget {
    Q_OBJECT
public:
    explicit GalleryPageSkeleton(QWidget* parent = nullptr);
};

} // namespace fluent::gallery

#endif // GALLERYPAGESKELETON_H
