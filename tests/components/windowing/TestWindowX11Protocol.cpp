#include <gtest/gtest.h>

#include "compatibility/private/WindowBackdropXcb_p.h"

using compatibility::detail::X11BackdropBackgroundPreparation;
using compatibility::detail::x11ClientRoundTripCompleted;

namespace {

namespace xcb = compatibility::detail::xcb;

bool protocolError = false;
bool missingReply = false;
bool connectionError = false;

int connectionStatus(xcb::Connection*)
{
    return connectionError ? 1 : 0;
}

xcb::Error* checkedError(xcb::Connection*, xcb::Cookie)
{
    return protocolError ? static_cast<xcb::Error*>(std::calloc(1, 36)) : nullptr;
}

xcb::Cookie sendRoundTrip(xcb::Connection*)
{
    return {1};
}

xcb::IdReply* readRoundTrip(xcb::Connection*, xcb::Cookie, xcb::Error** error)
{
    *error = checkedError(nullptr, {});
    if (protocolError || missingReply)
        return nullptr;
    return static_cast<xcb::IdReply*>(std::calloc(1, sizeof(xcb::IdReply)));
}

class X11CheckedRequestTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        protocolError = false;
        missingReply = false;
        connectionError = false;
        api.connectionHasError = connectionStatus;
        api.requestCheck = checkedError;
        api.getInputFocus = sendRoundTrip;
        api.getInputFocusReply = readRoundTrip;
    }

    xcb::Api api;
    xcb::Connection* connection = reinterpret_cast<xcb::Connection*>(quintptr{1});
};

} // namespace

TEST_F(X11CheckedRequestTest, ProtocolErrorRejectsWriteWithoutEndingConnection)
{
    protocolError = true;
    EXPECT_FALSE(api.check(connection, {1}));
    protocolError = false;
    EXPECT_TRUE(api.check(connection, {2}));
    connectionError = true;
    EXPECT_FALSE(api.check(connection, {3}));
}

TEST_F(X11CheckedRequestTest, ReplyAndErrorCompleteFenceButMissingReplyOrBrokenConnectionDoNot)
{
    EXPECT_TRUE(api.roundTrip(connection));
    protocolError = true;
    EXPECT_TRUE(api.roundTrip(connection));
    EXPECT_FALSE(api.reply(connection, {1}, api.getInputFocusReply));
    protocolError = false;
    missingReply = true;
    EXPECT_FALSE(api.roundTrip(connection));
    missingReply = false;
    connectionError = true;
    EXPECT_FALSE(api.roundTrip(connection));
}

TEST(X11WindowBackdropProtocolTest, ServerErrorCompletesAnOrderedRoundTrip)
{
    EXPECT_TRUE(x11ClientRoundTripCompleted(true, true, false, false));
    EXPECT_TRUE(x11ClientRoundTripCompleted(true, false, true, false));
    EXPECT_FALSE(x11ClientRoundTripCompleted(false, false, true, false));
    EXPECT_FALSE(x11ClientRoundTripCompleted(true, false, false, false));
    EXPECT_FALSE(x11ClientRoundTripCompleted(true, true, false, true));
    EXPECT_FALSE(x11ClientRoundTripCompleted(true, false, true, true));
}

TEST(X11WindowBackdropProtocolTest, RepeatedPreparationAvoidsNativeWorkAndOrdersEachSurfaceOnce)
{
    X11BackdropBackgroundPreparation state;
    int orders = 0;
    int writes = 0;
    const auto order = [&] {
        ++orders;
        return true;
    };
    const auto apply = [&](quint32, bool) {
        ++writes;
        return true;
    };
    ASSERT_TRUE(state.prepare(10, 0xff202020, false, order, apply));
    for (int i = 0; i < 20; ++i)
        ASSERT_TRUE(state.prepare(10, 0xff202020, false, order, apply));
    EXPECT_EQ(orders, 1);
    EXPECT_EQ(writes, 1);

    ASSERT_TRUE(state.prepare(10, 0xffeeeeee, false, order, apply));
    EXPECT_EQ(orders, 1);
    EXPECT_EQ(writes, 2);
    ASSERT_TRUE(state.prepare(10, 0xffeeeeee, true, order, apply));
    ASSERT_TRUE(state.prepare(10, 0xff202020, true, order, apply));
    EXPECT_EQ(orders, 1);
    EXPECT_EQ(writes, 3);

    ASSERT_TRUE(state.prepare(11, 0xff202020, false, order, apply));
    EXPECT_EQ(orders, 2);
    EXPECT_EQ(writes, 4);
    state.invalidate();
    ASSERT_TRUE(state.prepare(11, 0xff202020, false, order, apply));
    EXPECT_EQ(orders, 3) << "A recreated surface may reuse the same native ID";
    EXPECT_EQ(writes, 5);
}

TEST(X11WindowBackdropProtocolTest, FailedOrderAndCheckedWriteRemainRetryable)
{
    X11BackdropBackgroundPreparation state;
    int orders = 0;
    int writes = 0;
    bool connectionReady = false;
    bool windowExists = false;
    const auto order = [&] {
        ++orders;
        return connectionReady;
    };
    const auto apply = [&](quint32, bool) {
        ++writes;
        return windowExists;
    };
    EXPECT_FALSE(state.prepare(10, 0xff202020, false, order, apply));
    EXPECT_EQ(orders, 1);
    EXPECT_EQ(writes, 0);
    connectionReady = true;
    EXPECT_FALSE(state.prepare(10, 0xff202020, false, order, apply));
    EXPECT_EQ(orders, 2);
    EXPECT_EQ(writes, 1);
    windowExists = true;
    EXPECT_TRUE(state.prepare(10, 0xff202020, false, order, apply));
    EXPECT_EQ(orders, 2);
    EXPECT_EQ(writes, 2);
    EXPECT_TRUE(state.prepare(10, 0xff202020, false, order, apply));
    EXPECT_EQ(writes, 2);
}
