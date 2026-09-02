#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include <QString>
#include <QWidget>

#include "components/basicinput/ToggleSwitch.h"
#include "view/widgets/GallerySampleCatalog.h"

namespace {

TEST(GallerySampleAccessibilityTest, ToggleSwitchStatePreviewAndSnippetExposeTheSameName)
{
    const QVector<fluent::gallery::GallerySample> samples =
        fluent::gallery::gallerySamplesForRoute(QStringLiteral("toggle-switch"));
    const auto sample = std::find_if(
        samples.cbegin(), samples.cend(), [](const fluent::gallery::GallerySample& candidate) {
            return candidate.id == QStringLiteral("toggle-switch-state");
        });
    ASSERT_NE(sample, samples.cend());
    EXPECT_TRUE(
        sample->codeSnippet.contains(QStringLiteral("setAccessibleName(\"Feature toggle\")")));

    std::unique_ptr<QWidget> preview(sample->createPreview(nullptr));
    ASSERT_NE(preview, nullptr);
    auto* toggle = preview->findChild<fluent::basicinput::ToggleSwitch*>();
    ASSERT_NE(toggle, nullptr);
    EXPECT_EQ(toggle->accessibleName(), QStringLiteral("Feature toggle"));
}

} // namespace
