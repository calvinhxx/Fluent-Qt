#include <gtest/gtest.h>

#include <FluentQt/FluentQt.h>

#include "design/IconCatalog.h"

namespace {

// These probes intentionally run before the shared test main constructs
// QApplication. They exercise the static-library startup order that an
// application can hit from its own global objects.
// zh_CN: 这些探针刻意在共享测试 main 创建 QApplication 前运行，用于覆盖
// 应用全局对象可能触发的静态库启动顺序。
const int kPreApplicationCatalogSize =
    Typography::Icons::catalog().size();
const bool kPreApplicationInitializationResult =
    fluent::initializeResources();

TEST(ResourceInitializationTest,
     Contract_PreApplicationAccessDoesNotPoisonResourceInitialization)
{
    EXPECT_FALSE(kPreApplicationInitializationResult);
    EXPECT_GT(kPreApplicationCatalogSize, 0);
    EXPECT_TRUE(fluent::initializeResources());
    EXPECT_EQ(Typography::Icons::catalog().size(),
              kPreApplicationCatalogSize);
}

} // namespace
