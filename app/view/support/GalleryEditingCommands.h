#ifndef GALLERYEDITINGCOMMANDS_H
#define GALLERYEDITINGCOMMANDS_H

class QWidget;

namespace fluent::textfields {
class EditingCommandRouter;
}

namespace fluent::gallery {

/**
 * @brief Returns the Gallery window router, with a local fallback for isolated previews.
 * zh_CN: 返回 Gallery 窗口路由器；独立预览中则创建局部回退实例。
 */
fluent::textfields::EditingCommandRouter*
galleryWindowEditingCommandRouter(QWidget* fallbackContext);

} // namespace fluent::gallery

#endif // GALLERYEDITINGCOMMANDS_H
