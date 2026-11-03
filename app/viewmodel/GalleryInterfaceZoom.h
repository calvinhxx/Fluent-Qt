#ifndef GALLERYINTERFACEZOOM_H
#define GALLERYINTERFACEZOOM_H

#include <array>
#include <cstddef>

namespace fluent::gallery::interfacezoom {

/**
 * @brief Browser-style whole-interface zoom levels, in percent of the default size.
 * zh_CN: 浏览器式整体界面缩放档位，以默认尺寸的百分比表示。
 */
constexpr int DefaultPercent = 100;
constexpr std::array<int, 11> Levels{50, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200};

/**
 * @brief Returns the supported level closest to @p percent; ties keep the smaller level.
 * zh_CN: 返回最接近 @p percent 的档位；距离相同时取较小档位。
 */
constexpr int nearestLevel(int percent)
{
    int nearest = Levels.front();
    for (const int level : Levels) {
        const int distance = level > percent ? level - percent : percent - level;
        const int nearestDistance = nearest > percent ? nearest - percent : percent - nearest;
        if (distance < nearestDistance)
            nearest = level;
    }
    return nearest;
}

/**
 * @brief Returns the next larger level, staying at the largest level.
 * zh_CN: 返回下一个更大的档位，已是最大档位时保持不变。
 */
constexpr int zoomedIn(int percent)
{
    const int current = nearestLevel(percent);
    for (const int level : Levels) {
        if (level > current)
            return level;
    }
    return current;
}

/**
 * @brief Returns the next smaller level, staying at the smallest level.
 * zh_CN: 返回下一个更小的档位，已是最小档位时保持不变。
 */
constexpr int zoomedOut(int percent)
{
    const int current = nearestLevel(percent);
    for (std::size_t index = Levels.size(); index > 0; --index) {
        if (Levels[index - 1] < current)
            return Levels[index - 1];
    }
    return current;
}

} // namespace fluent::gallery::interfacezoom

#endif // GALLERYINTERFACEZOOM_H
