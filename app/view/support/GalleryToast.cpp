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
    auto* toast = status_info::Toast::showToast(
        anchor,
        message,
        status_info::Toast::Success,
        kGalleryToastVisibleMs);
    if (!toast) {
        LOG_WARN(
            QStringLiteral(
                "GalleryToast show rejected reason=missing-host message=%1")
                .arg(message));
        return;
    }

    // The reusable Toast knows only the host surface. Gallery owns its custom
    // title-bar spacing policy and keeps that application-specific offset here.
    toast->setObjectName(QStringLiteral("galleryToast"));
    toast->setPlacementMargins(
        QMargins(
            16,
            kGalleryTitleBarHeight + kGalleryToastTopGap,
            16,
            16));

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
