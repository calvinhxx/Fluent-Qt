#ifndef FLUENTICONCATALOG_H
#define FLUENTICONCATALOG_H

#include <QFont>
#include <QPainter>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace Typography::Icons {

/**
 * @brief Metadata for one glyph in the bundled Fluent UI System Icons face.
 * zh_CN: 内置 Fluent UI System Icons 字体中单个字形的元数据。
 */
struct IconInfo {
    QString
        name; // Upstream catalog key, e.g. ic_fluent_add_20_regular. zh_CN: 上游目录键名，例如 ic_fluent_add_20_regular。
    QString displayName; // Human-readable icon name. zh_CN: 易读的图标名称。
    quint32 codepoint = 0;
    int designSize = 0; // Upstream design size in pixels. zh_CN: 图标原始设计尺寸（像素）。

    /**
     * @brief Returns the UTF-16 glyph string for this codepoint.
     * zh_CN: 返回该码位对应的 UTF-16 字形字符串。
     */
    QString glyph() const;
};

/**
 * @brief Returns the complete bundled Regular icon catalog.
 * zh_CN: 返回内置 Regular 图标的完整目录。
 */
const QVector<IconInfo>& catalog();

/**
 * @brief Resolves an upstream icon name to a glyph string.
 * zh_CN: 将上游图标名称解析为字形字符串。
 */
QString glyph(const QString& name);

/**
 * @brief Snaps an icon pixel size to the nearest crisp design size (12, 16, 20, 24, 32, ...).
 * Requests below 12 px stay unchanged for small chevrons, and ties prefer the larger size.
 * zh_CN: 将请求的图标像素尺寸对齐到最接近的清晰设计尺寸（12、16、20、24、32 等）；小于 12 px 时保持原值，等距时取更大一档。
 */
int snapIconPixelSize(int requestedPixelSize);

/**
 * @brief Resolves an icon glyph or catalog name to the variant matching @p designSize.
 * Accepts a Typography::Icons constant, a glyph string, or an upstream catalog name, falling back to the nearest available size.
 * zh_CN: 将图标字形或目录名称解析为最匹配 @p designSize 的尺寸变体。支持 Typography::Icons 常量、字形字符串或上游目录名；没有对应尺寸时自动改用最接近的可用尺寸。
 */
QString glyphForSize(const QString& glyphOrName, int designSize);

/**
 * @brief Creates an icon font at a snapped pixel size with platform-consistent hinting.
 * zh_CN: 使用 snapIconPixelSize() 对齐字号并创建跨平台渲染一致的图标字体。
 */
QFont font(int pixelSize);

/**
 * @brief Paints an icon glyph or catalog name into @p targetRect at @p pixelSize, restoring the painter font afterward.
 * zh_CN: 在 @p targetRect 中以 @p pixelSize 绘制图标字形或目录名称，并在返回前恢复 painter 原字体。
 */
void paintGlyph(QPainter& painter,
    const QRectF& targetRect,
    const QString& glyphOrName,
    int pixelSize,
    Qt::Alignment alignment = Qt::AlignCenter);

} // namespace Typography::Icons

#endif // FLUENTICONCATALOG_H
