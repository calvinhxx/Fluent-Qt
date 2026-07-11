#include <gtest/gtest.h>

#include <QElapsedTimer>
#include <QProcess>
#include <QString>
#include <QTest>
#include <QUuid>

#include "view/shell/GallerySingleInstance.h"

using fluent::gallery::GallerySingleInstance;

namespace {

class ProcessGuard {
public:
    ~ProcessGuard()
    {
        if (process.state() != QProcess::NotRunning) {
            process.kill();
            process.waitForFinished(1000);
        }
    }

    QProcess process;
};

QString uniqueInstanceKey()
{
    return QStringLiteral("com.fluentqt.gallery.test.%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QByteArray waitForOutput(QProcess& process, const QByteArray& marker, int timeoutMs)
{
    QByteArray output;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        output += process.readAllStandardOutput();
        if (output.contains(marker))
            break;
        if (process.state() == QProcess::NotRunning)
            break;
        process.waitForReadyRead(qMin(50, timeoutMs - static_cast<int>(timer.elapsed())));
    }
    output += process.readAllStandardOutput();
    return output;
}

} // namespace

TEST(GallerySingleInstanceTest, FirstOwnerBecomesPrimary)
{
    GallerySingleInstance instance(uniqueInstanceKey());

    EXPECT_EQ(instance.start(), GallerySingleInstance::StartResult::Primary);
    EXPECT_TRUE(instance.isPrimary());
    EXPECT_FALSE(instance.serverName().isEmpty());
    EXPECT_TRUE(instance.errorString().isEmpty());
    EXPECT_EQ(instance.start(), GallerySingleInstance::StartResult::Primary);
}

TEST(GallerySingleInstanceTest, OwnershipCanBeReacquiredAfterPrimaryExits)
{
    const QString key = uniqueInstanceKey();
    {
        GallerySingleInstance primary(key);
        ASSERT_EQ(primary.start(), GallerySingleInstance::StartResult::Primary);
    }

    GallerySingleInstance successor(key);
    EXPECT_EQ(successor.start(), GallerySingleInstance::StartResult::Primary);
    EXPECT_TRUE(successor.isPrimary());
}

TEST(GallerySingleInstanceTest, EmptyApplicationIdFailsClosed)
{
    GallerySingleInstance instance{QString()};

    EXPECT_EQ(instance.start(), GallerySingleInstance::StartResult::Error);
    EXPECT_FALSE(instance.isPrimary());
    EXPECT_FALSE(instance.errorString().isEmpty());
}

TEST(GallerySingleInstanceTest, SeparateProcessNotifiesPrimaryAndExits)
{
    const QString key = uniqueInstanceKey();
    const QString probe = QString::fromLocal8Bit(
        FLUENT_QT_GALLERY_SINGLE_INSTANCE_PROBE_PATH);

    ProcessGuard primary;
    primary.process.setProgram(probe);
    primary.process.setArguments({key});
    primary.process.start();
    EXPECT_TRUE(primary.process.waitForStarted(2000))
        << primary.process.errorString().toStdString();
    if (primary.process.state() == QProcess::NotRunning)
        return;
    const QByteArray primaryStartup = waitForOutput(primary.process, "PRIMARY", 2000);
    EXPECT_TRUE(primaryStartup.contains("PRIMARY")) << primaryStartup.constData();
    if (!primaryStartup.contains("PRIMARY"))
        return;

    ProcessGuard secondary;
    secondary.process.setProgram(probe);
    secondary.process.setArguments({key});
    secondary.process.start();
    EXPECT_TRUE(secondary.process.waitForFinished(3000))
        << secondary.process.errorString().toStdString();
    const QByteArray secondaryOutput = secondary.process.readAllStandardOutput();
    EXPECT_EQ(secondary.process.exitStatus(), QProcess::NormalExit);
    EXPECT_EQ(secondary.process.exitCode(), 0) << secondaryOutput.constData();
    EXPECT_TRUE(secondaryOutput.contains("SECONDARY")) << secondaryOutput.constData();

    EXPECT_TRUE(primary.process.waitForFinished(3000));
    const QByteArray primaryActivation = primary.process.readAllStandardOutput();
    EXPECT_EQ(primary.process.exitStatus(), QProcess::NormalExit);
    EXPECT_EQ(primary.process.exitCode(), 0) << primaryActivation.constData();
    EXPECT_TRUE(primaryActivation.contains("ACTIVATED")) << primaryActivation.constData();
}
