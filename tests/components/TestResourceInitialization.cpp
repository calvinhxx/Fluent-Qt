#include <gtest/gtest.h>

#include <FluentQt/FluentQt.h>

#include "design/IconCatalog.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

// These probes intentionally run before the shared test main constructs
// QApplication. They exercise the static-library startup order that an
// application can hit from its own global objects.
// zh_CN: 这些探针刻意在共享测试 main 创建 QApplication 前运行，用于覆盖
// 应用全局对象可能触发的静态库启动顺序。
const int kPreApplicationCatalogSize = Typography::Icons::catalog().size();
const bool kPreApplicationInitializationResult = fluent::initializeResources();

TEST(ResourceInitializationTest, Contract_FastLookupMatchesEveryCatalogNameAndSemanticAlias)
{
    using namespace Typography::Icons;
    for (const auto& info : catalog()) {
        ASSERT_EQ(glyph(info.name), info.glyph()) << info.name.toStdString();
        ASSERT_EQ(glyphForSize(info.name, info.designSize), info.glyph()) << info.name.toStdString();
    }
    QFile file(QStringLiteral(":/res/icons/FluentQtIconAliases.json"));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const auto aliases = QJsonDocument::fromJson(file.readAll()).object();
    ASSERT_FALSE(aliases.isEmpty());
    for (auto it = aliases.begin(); it != aliases.end(); ++it) {
        const QString alias(QChar(it.key().toUShort(nullptr, 16)));
        for (int size : {8, 12, 18, 24, 32, 64})
            ASSERT_EQ(glyphForSize(alias, size), glyphForSize(it.value().toString(), size)) << it.key().toStdString();
    }
    EXPECT_TRUE(glyph(QStringLiteral("ic_fluent_add_020_regular")).isEmpty());
    EXPECT_TRUE(glyph(QStringLiteral("ic_fluent_missing_20_regular")).isEmpty());
}

TEST(ResourceInitializationTest, Contract_PreApplicationAccessDoesNotPoisonResourceInitialization)
{
    EXPECT_FALSE(kPreApplicationInitializationResult);
    EXPECT_GT(kPreApplicationCatalogSize, 0);
    EXPECT_TRUE(fluent::initializeResources());
    EXPECT_EQ(Typography::Icons::catalog().size(), kPreApplicationCatalogSize);
}

} // namespace
