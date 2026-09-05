#include "compatibility/WindowBackdropTypes.h"
#include "compatibility/private/WindowBackdropEvents_p.h"

#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QEvent>
#include <QMetaEnum>
#include <QVariant>
#include <QWidget>

namespace {

class BackdropEventReceiver : public QWidget {
public:
    int requests = 0;

protected:
    bool event(QEvent* event) override
    {
        if (compatibility::detail::isWindowBackdropReevaluationEvent(event)) {
            ++requests;
            return true;
        }
        return QWidget::event(event);
    }
};

} // namespace

TEST(WindowBackdropCompatibility, PreservesEnumNamesAndStateMetatype)
{
    using namespace fluent::windowing;
    EXPECT_STREQ(QMetaEnum::fromType<BackdropEffect>().valueToKey(1), "Mica");
    EXPECT_STREQ(QMetaEnum::fromType<BackdropBackend>().valueToKey(3), "MacVibrancy");
    EXPECT_STREQ(QMetaEnum::fromType<BackdropFidelity>().valueToKey(3), "Native");
    EXPECT_STREQ(QMetaEnum::fromType<BackdropSurfaceMode>().valueToKey(2), "CompositedTransparent");
    EXPECT_STREQ(QMetaEnum::fromType<BackdropEffect>().scope(), "fluent::windowing");

    BackdropState state;
    state.requestedEffect = BackdropEffect::Acrylic;
    state.reason = QStringLiteral("platform protocol round trip");
    const QVariant value = QVariant::fromValue(state);
    ASSERT_TRUE(value.canConvert<BackdropState>());
    EXPECT_EQ(value.value<BackdropState>(), state);
}

TEST(WindowBackdropCompatibility, CompositorBlurRequiresAlphaAndDoesNotRepresentMica)
{
    using namespace fluent::windowing;
    BackdropCapabilities capabilities;
    capabilities.compositorBlur = true;
    EXPECT_TRUE(capabilities.supportsCompositor(BackdropEffect::Acrylic));
    EXPECT_FALSE(capabilities.supportsTransparentMaterial(BackdropEffect::Acrylic));
    capabilities.alphaSurfaceSupported = true;
    EXPECT_TRUE(capabilities.supportsTransparentMaterial(BackdropEffect::Acrylic));
    EXPECT_FALSE(capabilities.supportsTransparentMaterial(BackdropEffect::Mica));
    EXPECT_FALSE(capabilities.supportsTransparentMaterial(BackdropEffect::Solid));
}

TEST(WindowBackdropCompatibility, PostsRequestsToTopLevelWithoutSynchronousReentry)
{
    BackdropEventReceiver window;
    BackdropEventReceiver child;
    child.setParent(&window);
    compatibility::detail::requestWindowBackdropReevaluation(nullptr);
    compatibility::detail::requestWindowBackdropReevaluation(&child);
    EXPECT_EQ(window.requests, 0);
    EXPECT_EQ(child.requests, 0);
    QCoreApplication::sendPostedEvents(&window);
    EXPECT_EQ(window.requests, 1);
    EXPECT_EQ(child.requests, 0);

    QEvent unrelated(QEvent::User);
    EXPECT_FALSE(compatibility::detail::isWindowBackdropReevaluationEvent(&unrelated));
    EXPECT_FALSE(compatibility::detail::isWindowBackdropReevaluationEvent(nullptr));
}

TEST(WindowBackdropCompatibility, DestroyedWindowCancelsPendingRequests)
{
    auto* window = new BackdropEventReceiver;
    compatibility::detail::requestWindowBackdropReevaluation(window);
    delete window;
    QCoreApplication::sendPostedEvents();
}
