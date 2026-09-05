#include "viewmodel/UpdateChecker.h"

#include <QApplication>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QPointer>
#include <QSignalSpy>
#include <gtest/gtest.h>

using fluent::gallery::UpdateChecker;

TEST(UpdateCheckerTest, DestructionCancelsRequestWithoutDeliveringResult)
{
    auto* checker = new UpdateChecker;
    auto* network = checker->findChild<QNetworkAccessManager*>();
    ASSERT_NE(network, nullptr);
    network->setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, QStringLiteral("127.0.0.1"), 9));
    QSignalSpy started(checker, &UpdateChecker::checkStarted);
    QSignalSpy finished(checker, &UpdateChecker::checkFinished);
    checker->checkForUpdates();
    ASSERT_TRUE(checker->isChecking());
    ASSERT_EQ(started.count(), 1);
    QPointer<QNetworkReply> reply = network->findChild<QNetworkReply*>();
    ASSERT_FALSE(reply.isNull());

    delete checker;
    EXPECT_TRUE(reply.isNull());
    QApplication::processEvents();
    EXPECT_EQ(finished.count(), 0);
}

TEST(UpdateCheckerTest, CheckStartedCallbackMayDestroyChecker)
{
    QPointer<UpdateChecker> checker = new UpdateChecker;
    auto* network = checker->findChild<QNetworkAccessManager*>();
    ASSERT_NE(network, nullptr);
    network->setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, QStringLiteral("127.0.0.1"), 9));
    QObject::connect(checker, &UpdateChecker::checkStarted, qApp,
                     [checker]() { delete checker.data(); });
    checker->checkForUpdates();
    EXPECT_TRUE(checker.isNull());
}
