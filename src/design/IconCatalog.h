#ifndef FLUENTICONCATALOG_H
#define FLUENTICONCATALOG_H

#include <QFont>
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
 * @brief Creates a platform-consistent font for painting an icon glyph.
 * zh_CN: 创建用于绘制图标字形、跨平台一致的字体。
 */
QFont font(int pixelSize);

} // namespace Typography::Icons

#endif // FLUENTICONCATALOG_H
