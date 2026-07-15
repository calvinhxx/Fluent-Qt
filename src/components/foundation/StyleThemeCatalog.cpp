#include "components/foundation/StyleThemeCatalog.h"

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
#include "utils/private/FluentQtLogging_p.h"

namespace fluent {
namespace {

using fluent::FluentElement;
using fluent::ThemeRegistry;

QString themeKeyFor(StyleTheme theme)
{
    switch (theme) {
    case StyleTheme::Material:
        return QStringLiteral("material");
    case StyleTheme::MacOS:
        return QStringLiteral("macos");
    case StyleTheme::Fluent:
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
// so the caller can keep the existing token. zh_CN: 瑙ｆ瀽 "#RRGGBB"/"#RRGGBBAA" 鎴?QColor 鍙瘑鍒殑鍚嶇О;
// 澶辫触杩斿洖鏃犳晥鑹?璋冪敤鏂规嵁姝や繚鐣欏師 token銆?
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

// Serialize back to the same "#RRGGBB"/"#RRGGBBAA" form parseColor() understands. zh_CN: 搴忓垪鍖栧洖
// parseColor() 鍙瘑鍒殑 "#RRGGBB"/"#RRGGBBAA" 褰㈠紡銆?
QString colorToHex(const QColor& c)
{
    if (c.alpha() == 255)
        return QString::asprintf("#%02X%02X%02X", c.red(), c.green(), c.blue());
    return QString::asprintf("#%02X%02X%02X%02X", c.red(), c.green(), c.blue(), c.alpha());
}

// Single source of truth for the JSON-overridable color tokens: the exact set applyColorSpec READS
// is also what colorsToJson WRITES, so the parser and the exported template can never drift apart.
// zh_CN: JSON 鍙鐩栭鑹?token 鐨勫敮涓€鐪熸簮:applyColorSpec 璇诲彇鐨勫瓧娈甸泦鍗?colorsToJson 鍐欏嚭鐨勫瓧娈甸泦,
// 瑙ｆ瀽涓庡鍑烘ā鏉夸笉浼氭紓绉汇€?
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
    fn("strokeStrong", c.strokeStrong);   // M3 outline + macOS hairline rely on this. zh_CN: M3 鎻忚竟 + macOS 缁嗙嚎渚濊禆瀹冦€?
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

// Serialize one mode's overridable tokens to a spec object (round-trips with applyColorSpec 鈥?every
// key is present, so reading it back triggers no accent derivation). zh_CN: 鎶婃煇妯″紡鐨勫彲瑕嗙洊 token
// 搴忓垪鍖栦负 spec 瀵硅薄(涓?applyColorSpec 寰€杩?鎵€鏈?key 閮藉湪,璇诲洖鏃朵笉瑙﹀彂寮鸿皟鑹叉淳鐢?銆?
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
// derived from accentDefault so a user only has to set the primary accent. zh_CN: 鎶?spec 鐨勯鑹茶鐩?
// 搴旂敤鍒版煇妯″紡鐨?Colors;鏈寚瀹氱殑寮鸿皟鑹插彉浣撶敱 accentDefault 娲剧敓,鐢ㄦ埛鍙渶璁剧疆涓诲己璋冭壊銆?
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
    // zh_CN: 娲剧敓 spec 鏈寚瀹氱殑寮鸿皟鑹插彉浣?浣垮搧鐗?鐢ㄦ埛鍙渶鎻愪緵 accentDefault銆?
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
// currently seeded. zh_CN: 鎶婂畬鏁?spec 瀹夎杩涙敞鍐岃〃,鍙犲姞鍦ㄥ綋鍓嶅凡鎾鍊间箣涓娿€?
void applySpec(const QJsonObject& spec)
{
    ThemeRegistry& reg = ThemeRegistry::instance();

    if (spec.contains(QLatin1String("radius"))) {
        const QJsonObject r = spec.value(QLatin1String("radius")).toObject();
        const FluentElement::Radius base = reg.radius();
        // Clamp user-file radii to a sane range: a malformed/hostile themes/*.json could otherwise set a
        // negative or absurd corner radius that flows into every control's drawRoundedRect. Qt would clamp
        // it at paint time, but bounding it here keeps the stored token sane. zh_CN: 鎶婄敤鎴锋枃浠跺渾瑙掑す鍒板悎鐞嗚寖鍥?
        // 鍚﹀垯鐣稿舰/鎭舵剰 themes/*.json 鍙璐熷€兼垨绂昏氨鍦嗚骞舵祦鍏ユ墍鏈夋帶浠剁殑 drawRoundedRect;鍦ㄦ璁剧晫浣垮瓨鍌?token 淇濇寔鍚堢悊銆?
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
            // text-layout allocation. zh_CN: 缁欏瓧鍙风缉鏀捐鐣?閬垮厤璇～ "scale": 100000 鎾戠垎鎵€鏈?QFont 鍍忕礌灏哄涓庢枃鏈竷灞€鍒嗛厤銆?
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
// system's shape language. Only accent + surfaces + text + semantic colors are overridden 鈥?control
// fills and strokes keep the (alpha-based, theme-correct) Fluent defaults. zh_CN: 鍐呯疆鍝佺墝 spec銆?
// 棰滆壊涓哄彲杈ㄨ瘑鐨?Material 3 / macOS 鍊?鍦嗚瀵归綈鍚勭郴缁熷舰鎬佽瑷€銆備粎瑕嗙洊寮鸿皟鑹?琛ㄩ潰+鏂囧瓧+璇箟鑹?鎺т欢濉厖
// 涓庢弿杈逛繚鐣?鍩轰簬 alpha銆侀殢涓婚姝ｇ‘鐨?Fluent 榛樿銆?
QJsonObject builtinSpec(StyleTheme theme)
{
    QJsonObject spec;
    if (theme == StyleTheme::Material) {
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
    } else if (theme == StyleTheme::MacOS) {
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

// Export a full, editable JSON template for a style theme the first time it is applied 鈥?snapshotting
// the RESOLVED registry palette (built-in preset already layered, user overrides NOT yet). Done for
// EVERY brand including Fluent, so the themes folder is symmetric (fluent.json / material.json /
// macos.json) and each file is a complete starting point listing every overridable token + radius.
// zh_CN: 鏌愭牱寮忎富棰橀娆″簲鐢ㄦ椂瀵煎嚭瀹屾暣鍙紪杈?JSON 妯℃澘鈥斺€斿揩鐓с€屽凡瑙ｆ瀽鐨勬敞鍐岃〃璋冭壊鏉裤€?宸插彔鍔犲唴缃璁俱€?
// 灏氭湭鍙犲姞鐢ㄦ埛瑕嗙洊)銆傚姣忓鍝佺墝(鍚?Fluent)閮芥墽琛?浣夸富棰樻枃浠跺す瀵圭О(fluent/material/macos.json),
// 姣忎釜鏂囦欢閮芥槸鍒楀嚭鍏ㄩ儴鍙鐩?token + 鍦嗚鐨勫畬鏁磋捣鐐广€?
void exportTemplateIfAbsent(StyleTheme theme)
{
    const QString path = themesDir() + QStringLiteral("/") + themeKeyFor(theme) + QStringLiteral(".json");
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
    qCInfo(fluent::logging::themeCategory).noquote()
        << QStringLiteral("StyleThemeCatalog exported editable template %1").arg(path);
}

QJsonObject readUserSpec(StyleTheme theme)
{
    const QString path = themesDir() + QStringLiteral("/") + themeKeyFor(theme) + QStringLiteral(".json");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(fluent::logging::themeCategory).noquote()
            << QStringLiteral("StyleThemeCatalog ignoring malformed theme file %1: %2")
                   .arg(path, error.errorString());
        return {};
    }
    return doc.object();
}

// Write a user override spec back to themes/<key>.json (pretty-printed). zh_CN: 鎶婄敤鎴疯鐩?spec 鍐欏洖
// themes/<key>.json(缇庡寲杈撳嚭)銆?
bool writeUserSpec(StyleTheme theme, const QJsonObject& spec)
{
    if (!QDir().mkpath(themesDir()))
        return false;
    const QString path = themesDir() + QStringLiteral("/") + themeKeyFor(theme) + QStringLiteral(".json");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(fluent::logging::themeCategory).noquote()
            << QStringLiteral("StyleThemeCatalog could not write theme file %1").arg(path);
        return false;
    }
    file.write(QJsonDocument(spec).toJson(QJsonDocument::Indented));
    return true;
}

// Set obj[mode][key] = value, creating the nested mode object if needed. zh_CN: 璁剧疆 obj[mode][key],蹇呰鏃?
//鍒涘缓宓屽鐨?mode 瀵硅薄銆?
void setNestedColor(QJsonObject& obj, const char* mode, const char* key, const QString& value)
{
    QJsonObject m = obj.value(QLatin1String(mode)).toObject();
    m.insert(QLatin1String(key), value);
    obj.insert(QLatin1String(mode), m);
}

// Remove obj[mode][key]; drop the mode object entirely if it becomes empty. zh_CN: 绉婚櫎 obj[mode][key];
// 鑻ヨ mode 瀵硅薄鍙樼┖鍒欎竴骞跺垹闄ゃ€?
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

namespace StyleThemeCatalog {

QString themeKey(StyleTheme theme)
{
    return themeKeyFor(theme);
}

FluentElement::DesignLanguage designLanguageFor(StyleTheme theme)
{
    switch (theme) {
    case StyleTheme::Material:
        return FluentElement::DesignMaterial;
    case StyleTheme::MacOS:
        return FluentElement::DesignCupertino;
    case StyleTheme::Fluent:
        break;
    }
    return FluentElement::DesignFluent;
}

void apply(StyleTheme theme)
{
    ThemeRegistry& reg = ThemeRegistry::instance();
    reg.resetToDefaults();
    reg.setDesignLanguage(designLanguageFor(theme));

    const QJsonObject builtin = builtinSpec(theme);
    if (!builtin.isEmpty())
        applySpec(builtin);

    // Snapshot the resolved preset (Fluent seed, or seed + brand spec) as an editable template before
    // layering user overrides 鈥?so the exported file reflects the brand, not the user's tweaks.
    // zh_CN: 鍦ㄥ彔鍔犵敤鎴疯鐩栧墠,鎶婂凡瑙ｆ瀽鐨勯璁?Fluent 绉嶅瓙,鎴?绉嶅瓙+鍝佺墝 spec)蹇収涓哄彲缂栬緫妯℃澘銆?
    exportTemplateIfAbsent(theme);

    // User overrides win over the built-in preset, so anyone can tweak a brand (or even Fluent) by
    // editing themes/<key>.json without recompiling. zh_CN: 鐢ㄦ埛瑕嗙洊浼樺厛浜庡唴缃璁?浠讳綍浜洪兘鑳介€氳繃缂栬緫
    // themes/<key>.json 鍦ㄤ笉閲嶇紪鐨勬儏鍐典笅寰皟鏌愬搧鐗?鐢氳嚦 Fluent)銆?
    const QJsonObject userSpec = readUserSpec(theme);
    if (!userSpec.isEmpty())
        applySpec(userSpec);

    qCInfo(fluent::logging::themeCategory).noquote()
        << QStringLiteral("StyleThemeCatalog applied style theme key=%1 revision=%2")
               .arg(themeKeyFor(theme))
               .arg(reg.revision());
}

const char* const kDerivedAccentKeys[] = {
    "accentSecondary", "accentTertiary", "textAccentPrimary", "textOnAccent"
};

void applyAccentOverride(const QColor& accent)
{
    if (!accent.isValid())
        return;

    QJsonObject spec;
    const QString hex = colorToHex(accent);
    for (const char* mode : {"light", "dark"}) {
        setNestedColor(spec, mode, "accentDefault", hex);
        for (const char* key : kDerivedAccentKeys)
            removeNestedColor(spec, mode, key);
    }

    applySpec(spec);
}

QString userThemeFilePath(StyleTheme theme)
{
    return themesDir() + QStringLiteral("/") + themeKeyFor(theme) + QStringLiteral(".json");
}

QString themesDirectory()
{
    QDir().mkpath(themesDir());
    return themesDir();
}

void setUserAccent(StyleTheme theme, const QColor& accent)
{
    if (!accent.isValid())
        return;
    // One accent drives both modes; the per-mode variants (secondary/tertiary/textOnAccent/...) are
    // re-derived from accentDefault at apply() time, so we pin accentDefault and DROP any stale
    // variants to keep the palette internally consistent. zh_CN: 鍗曚竴寮鸿皟鑹查┍鍔ㄦ槑鏆椾袱鎬?鍥哄畾
    // accentDefault 骞舵竻闄ゆ畫鐣欏彉浣?浣?apply() 鏃剁粺涓€閲嶇畻,淇濊瘉璋冭壊鏉胯嚜娲姐€?
    const QString hex = colorToHex(accent);
    QJsonObject spec = readUserSpec(theme);
    for (const char* mode : {"light", "dark"}) {
        setNestedColor(spec, mode, "accentDefault", hex);
        for (const char* key : kDerivedAccentKeys)
            removeNestedColor(spec, mode, key);
    }
    writeUserSpec(theme, spec);
}

void clearUserAccent(StyleTheme theme)
{
    QJsonObject spec = readUserSpec(theme);
    if (spec.isEmpty())
        return;
    // Drop accentDefault AND the derived variants so the theme reverts cleanly to the preset accent
    // (no half-overridden palette left behind). zh_CN: 鍚屾椂娓呴櫎 accentDefault 涓庢淳鐢熷彉浣?骞插噣鍥為€€鍒伴璁惧己璋冭壊銆?
    for (const char* mode : {"light", "dark"}) {
        removeNestedColor(spec, mode, "accentDefault");
        for (const char* key : kDerivedAccentKeys)
            removeNestedColor(spec, mode, key);
    }
    writeUserSpec(theme, spec);
}

QColor presetAccent(StyleTheme theme, bool dark)
{
    const QJsonObject spec = builtinSpec(theme);
    if (!spec.isEmpty()) {
        const QJsonObject mode = spec.value(dark ? QLatin1String("dark") : QLatin1String("light")).toObject();
        const QColor parsed = parseColor(mode.value(QLatin1String("accentDefault")).toString());
        if (parsed.isValid())
            return parsed;
    }
    // Fluent has no override spec; fall back to its measured-from-Figma seed accent. zh_CN: Fluent 鏃犺鐩?
    // spec,鍥為€€鍒板叾 Figma 瀹炴祴鐨勭瀛愬己璋冭壊銆?
    return dark ? QColor(QStringLiteral("#60CDFF")) : QColor(QStringLiteral("#005FB8"));
}

} // namespace StyleThemeCatalog

} // namespace fluent




