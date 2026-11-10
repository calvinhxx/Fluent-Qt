#include "GalleryFeedbackButton.h"
#include <QJsonObject>
#include "components/status_info/ToolTip.h"
#include "viewmodel/GalleryDiagnostics.h"
#include "platform/GalleryPlatform.h"
namespace fluent::gallery {
GalleryFeedbackButton::GalleryFeedbackButton(QWidget* parent) : Button(parent)
{
    setObjectName(QStringLiteral("galleryFeedbackButton"));
    setAccessibleName(tr("Report a problem"));
    setAccessibleDescription(platform::diagnosticStorageDirectory().isEmpty()
            ? tr("Download a diagnostic ZIP and open a GitHub issue.")
            : tr("Copy a diagnostic ZIP and open a GitHub issue."));
    status_info::ToolTip::attach(this, tr("Report a problem"), status_info::ToolTip::Above);
    setFluentLayout(IconOnly);
    setFluentSize(Small);
    setFluentStyle(Subtle);
    setIconGlyph(QStringLiteral("ic_fluent_bug_16_regular"), Typography::IconSize::Compact);
    setFixedSize(24, 24);
    setThemeOverrides({{"radius", QJsonObject{{"control", 4}}}});
    connect(this, &Button::clicked, this, [this] { GalleryDiagnostics::ensure(this)->reportProblem(this); });
}
} // namespace fluent::gallery
