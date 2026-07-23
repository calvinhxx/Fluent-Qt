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
    QString name;         // Stable upstream key, e.g. ic_fluent_add_20_regular.
    QString displayName;  // Human-readable semantic name. zh_CN: 易读语义名称。
    quint32 codepoint = 0;
    int designSize = 0;   // Upstream optical design size in pixels. zh_CN: 上游光学设计尺寸。

    /** @brief Returns the UTF-16 glyph string for this codepoint. */
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
 * @brief Snaps a requested paint size onto Fluent/WinUI crisp optical slots.
 * zh_CN: 将请求绘制字号对齐到 Fluent/WinUI 清晰光学档位。
 *
 * Discrete sizes match WinUI guidance (16/20/24/32/…) plus the Fluent System
 * Icons 12 px compact slot. Requests below 12 stay exact for micro chevrons
 * (FlipView, PipsPager, NumberBox). Ties prefer the larger slot so icons do
 * not read undersized after face substitution.
 * zh_CN: 离散档对齐 WinUI 建议（16/20/24/32/…）并保留 Fluent System Icons 的
 * 12 px 紧凑槽；小于 12 的请求保持原值给微箭头用。等距时取更大档，避免换字后
 * 图标视觉偏小。
 */
int snapIconPixelSize(int requestedPixelSize);

/**
 * @brief Resolves a semantic or catalog glyph to its native optical-size variant.
 * zh_CN: 将语义字形或目录字形解析为对应原生光学尺寸的变体。
 *
 * The input may be a Typography::Icons semantic glyph, a glyph returned by
 * glyph(), or an upstream catalog name. If that icon family does not publish
 * the requested size, the nearest available design is used.
 * zh_CN: 输入可以是 Typography::Icons 语义字形、glyph() 返回的字形或上游目录名；
 * 若该图标族没有目标尺寸，则选择最接近的原生设计。
 */
QString glyphForSize(const QString& glyphOrName, int designSize);

/**
 * @brief Creates a platform-consistent font for painting an icon glyph.
 * zh_CN: 创建用于绘制图标字形、跨平台一致的字体。
 *
 * Pixel size is passed through snapIconPixelSize() before QFont construction.
 * Hinting matches Fluent text policy: PreferNoHinting on Windows DirectWrite
 * (vertical grid fitting fattens 12–16 px strokes), PreferVerticalHinting
 * elsewhere.
 * zh_CN: 构造 QFont 前会经 snapIconPixelSize() 对齐字号。Hinting 与正文策略一致：
 * Windows DirectWrite 用 PreferNoHinting（纵向网格拟合会把 12–16 px 描边压粗），
 * 其它平台保留 PreferVerticalHinting。
 */
QFont font(int pixelSize);

/**
 * @brief Paints a Fluent icon glyph crisply into @p targetRect.
 * zh_CN: 将 Fluent 图标字形清晰绘制到 @p targetRect。
 *
 * Resolves the optical variant for @p pixelSize, snaps @p targetRect to the
 * device pixel grid, then draws with QPainter::drawText(rect, alignment) so
 * compact chevrons stay centered and sharp (same pattern as SplitButton).
 * zh_CN: 按 @p pixelSize 解析光学变体，将 @p targetRect 对齐到设备像素网格，再用
 * QPainter::drawText(rect, alignment) 绘制，使紧凑箭头居中且清晰（与 SplitButton 同模式）。
 */
void paintGlyph(QPainter& painter,
                const QRectF& targetRect,
                const QString& glyphOrName,
                int pixelSize,
                Qt::Alignment alignment = Qt::AlignCenter);

} // namespace Typography::Icons

#endif // FLUENTICONCATALOG_H
