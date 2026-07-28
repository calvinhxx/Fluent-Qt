#include "GalleryToast.h"

#include <QMargins>
#include <QWidget>

#include "components/status_info/Toast.h"
#include "support/logging/Log.h"

namespace fluent::gallery {
namespace {

constexpr int kGalleryTitleBarHeight = 36;
constexpr int kGalleryToastTopGap = 14;
constexpr int kGalleryToastVisibleMs = 1700;

} // namespace

void showGalleryToast(QWidget* anchor, const QString& message)
{
    // Apply Gallery title-bar clearance before present so the first layout and
    // any stack eviction use the final inset. zh_CN: 在 present 前写入 Gallery
    // 标题栏留白，保证首次布局与堆叠淘汰都使用最终边距。
    auto* toast = status_info::Toast::showToast(
        anchor,
        message,
        status_info::Toast::Success,
        kGalleryToastVisibleMs,
        status_info::Toast::Top,
        QMargins(
            16,
            kGalleryTitleBarHeight + kGalleryToastTopGap,
            16,
            16));
    if (!toast) {
        LOG_WARN(
            QStringLiteral(
                "GalleryToast show rejected reason=missing-host message=%1")
                .arg(message));
        return;
    }

    toast->setObjectName(QStringLiteral("galleryToast"));

    if (auto* card = toast->findChild<QWidget*>(
            QStringLiteral("fluentToastCard"))) {
        card->setObjectName(QStringLiteral("galleryToastCard"));
    }
    if (auto* icon = toast->findChild<QWidget*>(
            QStringLiteral("fluentToastIcon"))) {
        icon->setObjectName(QStringLiteral("galleryToastIcon"));
    }

    LOG_DEBUG(
        QStringLiteral("GalleryToast show message=%1").arg(message));
}

} // namespace fluent::gallery
