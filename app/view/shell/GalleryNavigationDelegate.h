#ifndef GALLERYNAVIGATIONDELEGATE_H
#define GALLERYNAVIGATIONDELEGATE_H

class QAbstractItemDelegate;
class QObject;

namespace fluent {
class FluentElement;
}

namespace fluent::gallery {

/**
 * @brief Creates the navigation pane row delegate bound to a theme host.
 * zh_CN: 创建绑定到主题宿主的导航面板行委托。
 *
 * The concrete delegate type stays private to the implementation; callers only
 * need the resulting delegate to assign to a TreeView.
 * zh_CN: 具体委托类型对实现私有；调用方只需拿到委托并赋给 TreeView。
 */
QAbstractItemDelegate* makeGalleryNavigationDelegate(fluent::FluentElement* themeHost, QObject* parent);

} // namespace fluent::gallery

#endif // GALLERYNAVIGATIONDELEGATE_H
