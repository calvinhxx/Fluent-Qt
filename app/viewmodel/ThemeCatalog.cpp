#include "viewmodel/ThemeCatalog.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths>
#include <QtGlobal>

#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "utils/Log.h"

namespace fluent::gallery {
namespace {

using fluent::FluentElement;
using fluent::ThemeRegistry;

QString themeKey(GallerySettings::StyleTheme theme)
{
    switch (theme) {
    case GallerySettings::StyleTheme::Material:
        return QStringLiteral("material");
    case GallerySettings::StyleTheme::MacOS:
        return QStringLiteral("macos");
    case GallerySettings::StyleTheme::Fluent:
        break;
    }
    return QStringLiteral("fluent");
}

QString themesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/themes");
}

// Parse "#RRGGBB", "#RRGGBBAA", or any name QColor understands. Returns an invalid color on failure
// so the caller can keep the existing token. zh_CN: 解析 "#RRGGBB"/"#RRGGBBAA" 或 QColor 可识别的名称;
// 失败返回无效色,调用方据此保留原 token。
QColor parseColor(const QString& text)
{
    const QString s = text.trimmed();
    if (s.length() == 9 && s.startsWith(QLatin1Char('#'))) {
        bool okR = false, okG = false, okB = false, okA = false;
        const int r = s.mid(1, 2).toInt(&okR, 16);
        const int g = s.mid(3, 2).toInt(&okG, 16);
        const int b = s.mid(5, 2).toInt(&okB, 16);
        const int a = s.mid(7, 2).toInt(&okA, 16);
        if (okR && okG && okB && okA)
            return QColor(r, g, b, a);
    }
    QColor color(s);
    return color;
}

QColor withAlpha(QColor color, int alpha)
{
    color.setAlpha(alpha);
    return color;
}

// Serialize back to the same "#RRGGBB"/"#RRGGBBAA" form parseColor() understands. zh_CN: 序列化回
// parseColor() 可识别的 "#RRGGBB"/"#RRGGBBAA" 形式。
QString colorToHex(const QColor& c)
{
    if (c.alpha() == 255)
        return QString::asprintf("#%02X%02X%02X", c.red(), c.green(), c.blue());
    return QString::asprintf("#%02X%02X%02X%02X", c.red(), c.green(), c.blue(), c.alpha());
}

// Single source of truth for the JSON-overridable color tokens: the exact set applyColorSpec READS
// is also what colorsToJson WRITES, so the parser and the exported template can never drift apart.
// zh_CN: JSON 可覆盖颜色 token 的唯一真源:applyColorSpec 读取的字段集即 colorsToJson 写出的字段集,
// 解析与导出模板不会漂移。
template <typename Fn>
void forEachColorField(FluentElement::Colors& c, Fn&& fn)
{
    fn("accentDefault", c.accentDefault);
    fn("accentSecondary", c.accentSecondary);
    fn("accentTertiary", c.accentTertiary);
    fn("accentDisabled", c.accentDisabled);
    fn("textOnAccent", c.textOnAccent);
    fn("textAccentPrimary", c.textAccentPrimary);

    fn("bgCanvas", c.bgCanvas);
    fn("bgLayer", c.bgLayer);
    fn("bgLayerAlt", c.bgLayerAlt);
    fn("bgSolid", c.bgSolid);

    fn("textPrimary", c.textPrimary);
    fn("textSecondary", c.textSecondary);
    fn("textTertiary", c.textTertiary);
    fn("textDisabled", c.textDisabled);

    fn("controlDefault", c.controlDefault);
    fn("controlSecondary", c.controlSecondary);
    fn("controlTertiary", c.controlTertiary);
    fn("controlDisabled", c.controlDisabled);
    fn("controlAltSecondary", c.controlAltSecondary);
    fn("controlAltTertiary", c.controlAltTertiary);
    fn("subtleSecondary", c.subtleSecondary);
    fn("subtleTertiary", c.subtleTertiary);

    fn("strokeDefault", c.strokeDefault);
    fn("strokeSecondary", c.strokeSecondary);
    fn("strokeStrong", c.strokeStrong);   // M3 outline + macOS hairline rely on this. zh_CN: M3 描边 + macOS 细线依赖它。
    fn("strokeCard", c.strokeCard);
    fn("strokeDivider", c.strokeDivider);
    fn("strokeSurface", c.strokeSurface);
    fn("strokeFocusOuter", c.strokeFocusOuter);
    fn("strokeFocusInner", c.strokeFocusInner);

    fn("systemCritical", c.systemCritical);
    fn("systemCriticalBg", c.systemCriticalBg);
    fn("systemCaution", c.systemCaution);
    fn("systemCautionBg", c.systemCautionBg);
    fn("systemInfo", c.systemInfo);
    fn("systemInfoBg", c.systemInfoBg);
    fn("systemSuccess", c.systemSuccess);
    fn("systemSuccessBg", c.systemSuccessBg);
}

// Serialize one mode's overridable tokens to a spec object (round-trips with applyColorSpec — every
// key is present, so reading it back triggers no accent derivation). zh_CN: 把某模式的可覆盖 token
// 序列化为 spec 对象(与 applyColorSpec 往返:所有 key 都在,读回时不触发强调色派生)。
QJsonObject colorsToJson(const FluentElement::Colors& colors)
{
    FluentElement::Colors copy = colors;
    QJsonObject obj;
    forEachColorField(copy, [&](const char* key, QColor& field) {
        obj.insert(QLatin1String(key), colorToHex(field));
    });
    return obj;
}

// Apply a spec object's color overrides onto one mode's Colors. Unspecified accent variants are
// derived from accentDefault so a user only has to set the primary accent. zh_CN: 把 spec 的颜色覆盖
// 应用到某模式的 Colors;未指定的强调色变体由 accentDefault 派生,用户只需设置主强调色。
void applyColorSpec(FluentElement::Colors& c, const QJsonObject& obj)
{
    forEachColorField(c, [&](const char* key, QColor& field) {
        const QJsonValue v = obj.value(QLatin1String(key));
        if (v.isString()) {
            const QColor parsed = parseColor(v.toString());
            if (parsed.isValid())
                field = parsed;
        }
    });

    // Derive accent variants the spec did not pin, so a brand/user only needs accentDefault.
    // zh_CN: 派生 spec 未指定的强调色变体,使品牌/用户只需提供 accentDefault。
    if (obj.contains(QLatin1String("accentDefault"))) {
        if (!obj.contains(QLatin1String("accentSecondary")))
            c.accentSecondary = withAlpha(c.accentDefault, 230);
        if (!obj.contains(QLatin1String("accentTertiary")))
            c.accentTertiary = withAlpha(c.accentDefault, 204);
        if (!obj.contains(QLatin1String("textAccentPrimary")))
            c.textAccentPrimary = c.accentDefault;
        if (!obj.contains(QLatin1String("textOnAccent"))) {
            c.textOnAccent = c.accentDefault.lightnessF() > 0.6 ? QColor(Qt::black)
                                                                : QColor(Qt::white);
        }
    }
}

// Install a full spec ({ radius?, font?, light?, dark? }) into the registry on top of whatever is
// currently seeded. zh_CN: 把完整 spec 安装进注册表,叠加在当前已播种值之上。
void applySpec(const QJsonObject& spec)
{
    ThemeRegistry& reg = ThemeRegistry::instance();

    if (spec.contains(QLatin1String("radius"))) {
        const QJsonObject r = spec.value(QLatin1String("radius")).toObject();
        const FluentElement::Radius base = reg.radius();
        // Clamp user-file radii to a sane range: a malformed/hostile themes/*.json could otherwise set a
        // negative or absurd corner radius that flows into every control's drawRoundedRect. Qt would clamp
        // it at paint time, but bounding it here keeps the stored token sane. zh_CN: 把用户文件圆角夹到合理范围:
        // 否则畸形/恶意 themes/*.json 可设负值或离谱圆角并流入所有控件的 drawRoundedRect;在此设界使存储 token 保持合理。
        constexpr int kMaxRadius = 64;
        reg.setRadius(qBound(0, r.value(QLatin1String("none")).toInt(base.none), kMaxRadius),
                      qBound(0, r.value(QLatin1String("control")).toInt(base.control), kMaxRadius),
                      qBound(0, r.value(QLatin1String("overlay")).toInt(base.overlay), kMaxRadius));
    }

    if (spec.contains(QLatin1String("font"))) {
        const QJsonObject f = spec.value(QLatin1String("font")).toObject();
        if (f.contains(QLatin1String("family")))
            reg.setFontFamilyOverride(f.value(QLatin1String("family")).toString());
        if (f.contains(QLatin1String("scale"))) {
            // Bound the font scale so a stray "scale": 100000 can't blow up every QFont pixel size and
            // text-layout allocation. zh_CN: 给字号缩放设界,避免误填 "scale": 100000 撑爆所有 QFont 像素尺寸与文本布局分配。
            reg.setFontScale(qBound(0.5, f.value(QLatin1String("scale")).toDouble(1.0), 4.0));
        }
    }

    if (spec.contains(QLatin1String("light"))) {
        FluentElement::Colors c = reg.colors(false);
        applyColorSpec(c, spec.value(QLatin1String("light")).toObject());
        reg.setColors(false, c);
    }
    if (spec.contains(QLatin1String("dark"))) {
        FluentElement::Colors c = reg.colors(true);
        applyColorSpec(c, spec.value(QLatin1String("dark")).toObject());
        reg.setColors(true, c);
    }
}

QJsonObject colorObj(std::initializer_list<std::pair<const char*, const char*>> entries)
{
    QJsonObject obj;
    for (const auto& [key, value] : entries)
        obj.insert(QLatin1String(key), QLatin1String(value));
    return obj;
}

// Built-in brand spec. Colors are recognizable Material 3 / macOS values; corner radius matches each
// system's shape language. Only accent + surfaces + text + semantic colors are overridden — control
// fills and strokes keep the (alpha-based, theme-correct) Fluent defaults. zh_CN: 内置品牌 spec。
// 颜色为可辨识的 Material 3 / macOS 值,圆角对齐各系统形态语言。仅覆盖强调色+表面+文字+语义色;控件填充
// 与描边保留(基于 alpha、随主题正确的)Fluent 默认。
QJsonObject builtinSpec(GallerySettings::StyleTheme theme)
{
    QJsonObject spec;
    if (theme == GallerySettings::StyleTheme::Material) {
        QJsonObject radius;
        radius.insert(QStringLiteral("control"), 8);
        radius.insert(QStringLiteral("overlay"), 12);
        spec.insert(QStringLiteral("radius"), radius);
        spec.insert(QStringLiteral("light"), colorObj({
            {"accentDefault", "#6750A4"}, {"textOnAccent", "#FFFFFF"},
            {"bgCanvas", "#FEF7FF"}, {"bgLayer", "#FFFFFF"}, {"bgLayerAlt", "#F7F2FA"}, {"bgSolid", "#F3EDF7"},
            {"textPrimary", "#1D1B20"}, {"textSecondary", "#49454F"}, {"textTertiary", "#79747E"},
            {"systemCritical", "#B3261E"}, {"systemCriticalBg", "#F9DEDC"},
            {"systemCaution", "#7A5900"}, {"systemCautionBg", "#FCEFC7"},
            {"systemInfo", "#6750A4"}, {"systemInfoBg", "#EADDFF"},
            {"systemSuccess", "#146C2E"}, {"systemSuccessBg", "#C4EED0"},
        }));
        spec.insert(QStringLiteral("dark"), colorObj({
            {"accentDefault", "#D0BCFF"}, {"textOnAccent", "#381E72"},
            {"bgCanvas", "#141218"}, {"bgLayer", "#1D1B20"}, {"bgLayerAlt", "#211F26"}, {"bgSolid", "#0F0D13"},
            {"textPrimary", "#E6E0E9"}, {"textSecondary", "#CAC4D0"}, {"textTertiary", "#938F99"},
            {"systemCritical", "#F2B8B5"}, {"systemCriticalBg", "#8C1D18"},
            {"systemCaution", "#F5C518"}, {"systemCautionBg", "#4A3C00"},
            {"systemInfo", "#D0BCFF"}, {"systemInfoBg", "#4F378B"},
            {"systemSuccess", "#6DD58C"}, {"systemSuccessBg", "#0A3818"},
        }));
    } else if (theme == GallerySettings::StyleTheme::MacOS) {
        QJsonObject radius;
        radius.insert(QStringLiteral("control"), 6);
        radius.insert(QStringLiteral("overlay"), 10);
        spec.insert(QStringLiteral("radius"), radius);
        spec.insert(QStringLiteral("light"), colorObj({
            {"accentDefault", "#007AFF"}, {"textOnAccent", "#FFFFFF"},
            {"bgCanvas", "#ECECEC"}, {"bgLayer", "#FFFFFF"}, {"bgLayerAlt", "#F5F5F7"}, {"bgSolid", "#E3E3E3"},
            {"textPrimary", "#000000D9"}, {"textSecondary", "#0000007F"}, {"textTertiary", "#00000042"},
            {"systemCritical", "#FF3B30"}, {"systemCriticalBg", "#FFE5E3"},
            {"systemCaution", "#FF9500"}, {"systemCautionBg", "#FFF0DB"},
            {"systemInfo", "#007AFF"}, {"systemInfoBg", "#E3F0FF"},
            {"systemSuccess", "#34C759"}, {"systemSuccessBg", "#E3F9E5"},
        }));
        spec.insert(QStringLiteral("dark"), colorObj({
            {"accentDefault", "#0A84FF"}, {"textOnAccent", "#FFFFFF"},
            {"bgCanvas", "#1E1E1E"}, {"bgLayer", "#2C2C2E"}, {"bgLayerAlt", "#3A3A3C"}, {"bgSolid", "#161618"},
            {"textPrimary", "#FFFFFF"}, {"textSecondary", "#FFFFFF8C"}, {"textTertiary", "#FFFFFF40"},
            {"systemCritical", "#FF453A"}, {"systemCriticalBg", "#3A1F1E"},
            {"systemCaution", "#FF9F0A"}, {"systemCautionBg", "#3A2C12"},
            {"systemInfo", "#0A84FF"}, {"systemInfoBg", "#16263A"},
            {"systemSuccess", "#30D158"}, {"systemSuccessBg", "#15321E"},
        }));
    }
    return spec;
}

// Export a full, editable JSON template for a style theme the first time it is applied — snapshotting
// the RESOLVED registry palette (built-in preset already layered, user overrides NOT yet). Done for
// EVERY brand including Fluent, so the themes folder is symmetric (fluent.json / material.json /
// macos.json) and each file is a complete starting point listing every overridable token + radius.
// zh_CN: 某样式主题首次应用时导出完整可编辑 JSON 模板——快照「已解析的注册表调色板」(已叠加内置预设、
// 尚未叠加用户覆盖)。对每套品牌(含 Fluent)都执行,使主题文件夹对称(fluent/material/macos.json),
// 每个文件都是列出全部可覆盖 token + 圆角的完整起点。
void exportTemplateIfAbsent(GallerySettings::StyleTheme theme)
{
    const QString path = themesDir() + QStringLiteral("/") + themeKey(theme) + QStringLiteral(".json");
    if (QFile::exists(path))
        return;
    if (!QDir().mkpath(themesDir()))
        return;

    const ThemeRegistry& reg = ThemeRegistry::instance();
    const FluentElement::Radius r = reg.radius();
    QJsonObject radius;
    radius.insert(QStringLiteral("none"), r.none);
    radius.insert(QStringLiteral("control"), r.control);
    radius.insert(QStringLiteral("overlay"), r.overlay);

    QJsonObject spec;
    spec.insert(QStringLiteral("radius"), radius);
    spec.insert(QStringLiteral("light"), colorsToJson(reg.colors(false)));
    spec.insert(QStringLiteral("dark"), colorsToJson(reg.colors(true)));

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    file.write(QJsonDocument(spec).toJson(QJsonDocument::Indented));
    LOG_INFO(QStringLiteral("ThemeCatalog exported editable template %1").arg(path));
}

QJsonObject readUserSpec(GallerySettings::StyleTheme theme)
{
    const QString path = themesDir() + QStringLiteral("/") + themeKey(theme) + QStringLiteral(".json");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_WARN(QStringLiteral("ThemeCatalog ignoring malformed theme file %1: %2")
                     .arg(path, error.errorString()));
        return {};
    }
    return doc.object();
}

// Write a user override spec back to themes/<key>.json (pretty-printed). zh_CN: 把用户覆盖 spec 写回
// themes/<key>.json(美化输出)。
bool writeUserSpec(GallerySettings::StyleTheme theme, const QJsonObject& spec)
{
    if (!QDir().mkpath(themesDir()))
        return false;
    const QString path = themesDir() + QStringLiteral("/") + themeKey(theme) + QStringLiteral(".json");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_WARN(QStringLiteral("ThemeCatalog could not write theme file %1").arg(path));
        return false;
    }
    file.write(QJsonDocument(spec).toJson(QJsonDocument::Indented));
    return true;
}

// Set obj[mode][key] = value, creating the nested mode object if needed. zh_CN: 设置 obj[mode][key],必要时
//创建嵌套的 mode 对象。
void setNestedColor(QJsonObject& obj, const char* mode, const char* key, const QString& value)
{
    QJsonObject m = obj.value(QLatin1String(mode)).toObject();
    m.insert(QLatin1String(key), value);
    obj.insert(QLatin1String(mode), m);
}

// Remove obj[mode][key]; drop the mode object entirely if it becomes empty. zh_CN: 移除 obj[mode][key];
// 若该 mode 对象变空则一并删除。
void removeNestedColor(QJsonObject& obj, const char* mode, const char* key)
{
    if (!obj.contains(QLatin1String(mode)))
        return;
    QJsonObject m = obj.value(QLatin1String(mode)).toObject();
    m.remove(QLatin1String(key));
    if (m.isEmpty())
        obj.remove(QLatin1String(mode));
    else
        obj.insert(QLatin1String(mode), m);
}

} // namespace

namespace ThemeCatalog {

FluentElement::DesignLanguage designLanguageFor(GallerySettings::StyleTheme theme)
{
    switch (theme) {
    case GallerySettings::StyleTheme::Material:
        return FluentElement::DesignMaterial;
    case GallerySettings::StyleTheme::MacOS:
        return FluentElement::DesignCupertino;
    case GallerySettings::StyleTheme::Fluent:
        break;
    }
    return FluentElement::DesignFluent;
}

void apply(GallerySettings::StyleTheme theme)
{
    ThemeRegistry& reg = ThemeRegistry::instance();
    reg.resetToDefaults();
    reg.setDesignLanguage(designLanguageFor(theme));

    const QJsonObject builtin = builtinSpec(theme);
    if (!builtin.isEmpty())
        applySpec(builtin);

    // Snapshot the resolved preset (Fluent seed, or seed + brand spec) as an editable template before
    // layering user overrides — so the exported file reflects the brand, not the user's tweaks.
    // zh_CN: 在叠加用户覆盖前,把已解析的预设(Fluent 种子,或 种子+品牌 spec)快照为可编辑模板。
    exportTemplateIfAbsent(theme);

    // User overrides win over the built-in preset, so anyone can tweak a brand (or even Fluent) by
    // editing themes/<key>.json without recompiling. zh_CN: 用户覆盖优先于内置预设,任何人都能通过编辑
    // themes/<key>.json 在不重编的情况下微调某品牌(甚至 Fluent)。
    const QJsonObject userSpec = readUserSpec(theme);
    if (!userSpec.isEmpty())
        applySpec(userSpec);

    LOG_INFO(QStringLiteral("ThemeCatalog applied style theme key=%1 revision=%2")
                 .arg(themeKey(theme))
                 .arg(reg.revision()));
}

QString userThemeFilePath(GallerySettings::StyleTheme theme)
{
    return themesDir() + QStringLiteral("/") + themeKey(theme) + QStringLiteral(".json");
}

QString themesDirectory()
{
    QDir().mkpath(themesDir());
    return themesDir();
}

// The accent variants that applyColorSpec DERIVES from accentDefault. When a user pins accentDefault
// we must drop these so the derive recomputes them from the NEW accent — otherwise, on a full-dump
// template (where they are explicit), the OLD variants linger and clash with the new accentDefault
// (e.g. a green accentDefault with stale blue secondary/textAccent — the bug this fixes).
// zh_CN: applyColorSpec 会从 accentDefault 派生的强调色变体。用户固定 accentDefault 时必须清除它们,
// 让派生从新强调色重算;否则在完整 dump 模板(变体为显式值)中,旧变体会残留并与新 accentDefault 冲突。
const char* const kDerivedAccentKeys[] = {
    "accentSecondary", "accentTertiary", "textAccentPrimary", "textOnAccent"
};

void setUserAccent(GallerySettings::StyleTheme theme, const QColor& accent)
{
    if (!accent.isValid())
        return;
    // One accent drives both modes; the per-mode variants (secondary/tertiary/textOnAccent/...) are
    // re-derived from accentDefault at apply() time, so we pin accentDefault and DROP any stale
    // variants to keep the palette internally consistent. zh_CN: 单一强调色驱动明暗两态;固定
    // accentDefault 并清除残留变体,使 apply() 时统一重算,保证调色板自洽。
    const QString hex = colorToHex(accent);
    QJsonObject spec = readUserSpec(theme);
    for (const char* mode : {"light", "dark"}) {
        setNestedColor(spec, mode, "accentDefault", hex);
        for (const char* key : kDerivedAccentKeys)
            removeNestedColor(spec, mode, key);
    }
    writeUserSpec(theme, spec);
}

void clearUserAccent(GallerySettings::StyleTheme theme)
{
    QJsonObject spec = readUserSpec(theme);
    if (spec.isEmpty())
        return;
    // Drop accentDefault AND the derived variants so the theme reverts cleanly to the preset accent
    // (no half-overridden palette left behind). zh_CN: 同时清除 accentDefault 与派生变体,干净回退到预设强调色。
    for (const char* mode : {"light", "dark"}) {
        removeNestedColor(spec, mode, "accentDefault");
        for (const char* key : kDerivedAccentKeys)
            removeNestedColor(spec, mode, key);
    }
    writeUserSpec(theme, spec);
}

QColor presetAccent(GallerySettings::StyleTheme theme, bool dark)
{
    const QJsonObject spec = builtinSpec(theme);
    if (!spec.isEmpty()) {
        const QJsonObject mode = spec.value(dark ? QLatin1String("dark") : QLatin1String("light")).toObject();
        const QColor parsed = parseColor(mode.value(QLatin1String("accentDefault")).toString());
        if (parsed.isValid())
            return parsed;
    }
    // Fluent has no override spec; fall back to its measured-from-Figma seed accent. zh_CN: Fluent 无覆盖
    // spec,回退到其 Figma 实测的种子强调色。
    return dark ? QColor(QStringLiteral("#60CDFF")) : QColor(QStringLiteral("#005FB8"));
}

} // namespace ThemeCatalog

} // namespace fluent::gallery
