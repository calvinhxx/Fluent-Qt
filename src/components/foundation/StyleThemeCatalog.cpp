#include "components/foundation/StyleThemeCatalog.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
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

constexpr int kThemeSchemaVersion = 1;
constexpr qint64 kMaxThemeFileBytes = 256 * 1024;

// Parse "#RRGGBB", "#RRGGBBAA", or any color name understood by QColor.
// Failure returns an invalid color so the caller can preserve the current token.
// zh_CN: 解析十六进制或 QColor 可识别的颜色名；失败时返回无效颜色，
// 由调用方保留当前 token。
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

// Serialize to the "#RRGGBB"/"#RRGGBBAA" form understood by parseColor().
// zh_CN: 序列化为 parseColor() 可识别的十六进制形式。
QString colorToHex(const QColor& c)
{
    if (c.alpha() == 255)
        return QString::asprintf("#%02X%02X%02X", c.red(), c.green(), c.blue());
    return QString::asprintf("#%02X%02X%02X%02X", c.red(), c.green(), c.blue(), c.alpha());
}

// Single source of truth for the JSON-overridable color tokens: the exact set applyColorSpec READS
// is also what colorsToJson WRITES, so the parser and the exported template can never drift apart.
// zh_CN: JSON 可覆盖颜色 token 的唯一字段表；读取与导出共享它，避免漂移。
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
    fn("strokeStrong", c.strokeStrong);   // Used by M3 outlines and macOS hairlines. zh_CN: 用于 M3 描边和 macOS 细线。
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

// Serialize one mode's overridable tokens to a complete spec object. Because
// every key is present, importing the exported template is lossless.
// zh_CN: 将单个明暗模式的可覆盖 token 序列化为完整 spec，重新导入时无损。
QJsonObject colorsToJson(const FluentElement::Colors& colors)
{
    FluentElement::Colors copy = colors;
    QJsonObject obj;
    forEachColorField(copy, [&](const char* key, QColor& field) {
        obj.insert(QLatin1String(key), colorToHex(field));
    });
    return obj;
}

// Apply color overrides to one mode. Missing accent variants are derived from
// accentDefault so a sparse user file only needs to specify the primary accent.
// zh_CN: 将颜色覆盖应用到单个模式；未指定的强调色变体由 accentDefault 派生。
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

    // Derive variants not pinned by the spec.
    // zh_CN: 对 spec 未固定的强调色变体执行派生。
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

// Layer a spec ({ radius?, font?, light?, dark? }) onto a candidate snapshot.
// zh_CN: 将 spec 分层应用到候选主题快照。
void applySpec(ThemeRegistry::Snapshot& snapshot, const QJsonObject& spec)
{
    if (spec.contains(QLatin1String("radius"))) {
        const QJsonObject r = spec.value(QLatin1String("radius")).toObject();
        const FluentElement::Radius base = snapshot.radius;
        // Clamp user-file radii to a sane range: a malformed/hostile themes/*.json could otherwise set a
        // negative or absurd corner radius that flows into every control's drawRoundedRect. Qt would clamp
        // it at paint time, but bounding it here keeps the stored token sane.
        // zh_CN: 在入口约束用户圆角，避免异常值流入所有控件的绘制路径。
        constexpr int kMaxRadius = 64;
        snapshot.radius = {
            qBound(0, r.value(QLatin1String("none")).toInt(base.none), kMaxRadius),
            qBound(0, r.value(QLatin1String("control")).toInt(base.control), kMaxRadius),
            qBound(0, r.value(QLatin1String("overlay")).toInt(base.overlay), kMaxRadius)
        };
    }

    if (spec.contains(QLatin1String("font"))) {
        const QJsonObject f = spec.value(QLatin1String("font")).toObject();
        if (f.contains(QLatin1String("family")))
            snapshot.fontFamilyOverride = f.value(QLatin1String("family")).toString();
        if (f.contains(QLatin1String("scale"))) {
            // Bound the font scale so a stray "scale": 100000 can't blow up every QFont pixel size and
            // text-layout allocation. zh_CN: 限制字号缩放，避免异常值放大字体与文本布局分配。
            snapshot.fontScale =
                qBound(0.5, f.value(QLatin1String("scale")).toDouble(1.0), 4.0);
        }
    }

    if (spec.contains(QLatin1String("light"))) {
        applyColorSpec(snapshot.lightColors,
                       spec.value(QLatin1String("light")).toObject());
    }
    if (spec.contains(QLatin1String("dark"))) {
        applyColorSpec(snapshot.darkColors,
                       spec.value(QLatin1String("dark")).toObject());
    }
}

QJsonObject colorObj(std::initializer_list<std::pair<const char*, const char*>> entries)
{
    QJsonObject obj;
    for (const auto& [key, value] : entries)
        obj.insert(QLatin1String(key), QLatin1String(value));
    return obj;
}

// Built-in brand spec. Material 3/macOS colors and radii establish each
// design language while shared control fills and strokes retain safe defaults.
// zh_CN: 内置品牌 spec 使用各设计语言的代表性色彩与圆角，并复用安全的控件基础 token。
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

// Build the full editable template exported only through the explicit API. It
// snapshots the resolved built-in preset before user overrides are layered.
// zh_CN: 构建仅由显式 API 导出的完整模板；内容是叠加用户覆盖之前的内置预设快照。
QJsonObject resolvedPresetSpec(StyleTheme theme)
{
    ThemeRegistry::Snapshot snapshot = ThemeRegistry::defaultSnapshot();
    snapshot.designLanguage =
        theme == StyleTheme::Material ? FluentElement::DesignMaterial
        : theme == StyleTheme::MacOS ? FluentElement::DesignCupertino
                                     : FluentElement::DesignFluent;
    const QJsonObject builtin = builtinSpec(theme);
    if (!builtin.isEmpty())
        applySpec(snapshot, builtin);

    QJsonObject radius;
    radius.insert(QStringLiteral("none"), snapshot.radius.none);
    radius.insert(QStringLiteral("control"), snapshot.radius.control);
    radius.insert(QStringLiteral("overlay"), snapshot.radius.overlay);

    QJsonObject spec;
    spec.insert(QStringLiteral("radius"), radius);
    spec.insert(QStringLiteral("light"), colorsToJson(snapshot.lightColors));
    spec.insert(QStringLiteral("dark"), colorsToJson(snapshot.darkColors));
    return spec;
}

enum class UserSpecState {
    Missing,
    Current,
    Legacy,
    Rejected
};

struct UserSpecResult {
    UserSpecState state = UserSpecState::Missing;
    QJsonObject spec;

    bool isUsable() const
    {
        return state == UserSpecState::Current
            || state == UserSpecState::Legacy;
    }

    bool canModify() const
    {
        return state != UserSpecState::Rejected;
    }
};

bool isLegacyFlatSpec(const QJsonObject& root)
{
    if (root.contains(QLatin1String("schemaVersion"))
        || root.contains(QLatin1String("theme"))
        || root.contains(QLatin1String("overrides"))) {
        return false;
    }

    bool containsSpecSection = false;
    for (const char* key : {"radius", "font", "light", "dark"}) {
        const QLatin1String section(key);
        if (!root.contains(section))
            continue;
        if (!root.value(section).isObject())
            return false;
        containsSpecSection = true;
    }
    return containsSpecSection;
}

UserSpecResult readUserSpec(StyleTheme theme)
{
    const QString path = themesDir() + QStringLiteral("/") + themeKeyFor(theme) + QStringLiteral(".json");
    const bool exists = QFile::exists(path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (!exists)
            return {};
        qCWarning(fluent::logging::themeCategory).noquote()
            << QStringLiteral("StyleThemeCatalog preserving unreadable theme file %1").arg(path);
        return {UserSpecState::Rejected, {}};
    }
    if (file.size() > kMaxThemeFileBytes) {
        qCWarning(fluent::logging::themeCategory).noquote()
            << QStringLiteral("StyleThemeCatalog ignoring oversized theme file %1").arg(path);
        return {UserSpecState::Rejected, {}};
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(fluent::logging::themeCategory).noquote()
            << QStringLiteral("StyleThemeCatalog ignoring malformed theme file %1: %2")
                   .arg(path, error.errorString());
        return {UserSpecState::Rejected, {}};
    }

    const QJsonObject root = doc.object();
    if (isLegacyFlatSpec(root)) {
        qCInfo(fluent::logging::themeCategory).noquote()
            << QStringLiteral("StyleThemeCatalog loaded legacy flat theme file %1; "
                              "the next explicit edit will migrate it")
                   .arg(path);
        return {UserSpecState::Legacy, root};
    }

    if (root.value(QLatin1String("schemaVersion")).toInt(-1) != kThemeSchemaVersion) {
        qCWarning(fluent::logging::themeCategory).noquote()
            << QStringLiteral("StyleThemeCatalog ignoring unsupported schema in %1").arg(path);
        return {UserSpecState::Rejected, {}};
    }
    if (root.value(QLatin1String("theme")).toString() != themeKeyFor(theme)
        || !root.value(QLatin1String("overrides")).isObject()) {
        qCWarning(fluent::logging::themeCategory).noquote()
            << QStringLiteral("StyleThemeCatalog ignoring invalid theme envelope %1").arg(path);
        return {UserSpecState::Rejected, {}};
    }
    return {UserSpecState::Current,
            root.value(QLatin1String("overrides")).toObject()};
}

// Atomically write a pretty-printed user override.
// zh_CN: 以原子方式写入格式化的用户覆盖文件。
bool writeUserSpec(StyleTheme theme, const QJsonObject& spec)
{
    if (!QDir().mkpath(themesDir()))
        return false;
    const QString path = themesDir() + QStringLiteral("/") + themeKeyFor(theme) + QStringLiteral(".json");

    QJsonObject root;
    root.insert(QLatin1String("schemaVersion"), kThemeSchemaVersion);
    root.insert(QLatin1String("theme"), themeKeyFor(theme));
    root.insert(QLatin1String("overrides"), spec);
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (payload.size() > kMaxThemeFileBytes)
        return false;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(fluent::logging::themeCategory).noquote()
            << QStringLiteral("StyleThemeCatalog could not write theme file %1").arg(path);
        return false;
    }
    if (file.write(payload) != payload.size() || !file.commit()) {
        qCWarning(fluent::logging::themeCategory).noquote()
            << QStringLiteral("StyleThemeCatalog could not atomically commit theme file %1").arg(path);
        return false;
    }
    return true;
}

// Set obj[mode][key], creating the nested mode object when needed.
// zh_CN: 设置 obj[mode][key]，必要时创建嵌套模式对象。
void setNestedColor(QJsonObject& obj, const char* mode, const char* key, const QString& value)
{
    QJsonObject m = obj.value(QLatin1String(mode)).toObject();
    m.insert(QLatin1String(key), value);
    obj.insert(QLatin1String(mode), m);
}

// Remove obj[mode][key] and drop an empty mode object.
// zh_CN: 移除 obj[mode][key]，模式对象为空时一并删除。
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
    ThemeRegistry::Snapshot next = ThemeRegistry::defaultSnapshot();
    next.designLanguage = designLanguageFor(theme);

    const QJsonObject builtin = builtinSpec(theme);
    if (!builtin.isEmpty())
        applySpec(next, builtin);

    // User overrides win over the built-in preset.
    // zh_CN: 用户覆盖优先于内置预设。
    const UserSpecResult userSpec = readUserSpec(theme);
    if (userSpec.isUsable() && !userSpec.spec.isEmpty())
        applySpec(next, userSpec.spec);

    reg.applySnapshot(next);

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

    ThemeRegistry& registry = ThemeRegistry::instance();
    ThemeRegistry::Snapshot next = registry.snapshot();
    applySpec(next, spec);
    registry.applySnapshot(next);
}

QString userThemeFilePath(StyleTheme theme)
{
    return themesDir() + QStringLiteral("/") + themeKeyFor(theme) + QStringLiteral(".json");
}

QString themesDirectory()
{
    return themesDir();
}

bool exportUserThemeTemplate(StyleTheme theme, bool overwrite)
{
    const QString path = userThemeFilePath(theme);
    if (!overwrite && QFile::exists(path))
        return false;

    const bool written = writeUserSpec(theme, resolvedPresetSpec(theme));
    if (written) {
        qCInfo(fluent::logging::themeCategory).noquote()
            << QStringLiteral("StyleThemeCatalog exported editable template %1").arg(path);
    }
    return written;
}

void setUserAccent(StyleTheme theme, const QColor& accent)
{
    if (!accent.isValid())
        return;
    // One accent drives both modes; the per-mode variants (secondary/tertiary/textOnAccent/...) are
    // re-derived from accentDefault at apply() time, so we pin accentDefault and DROP any stale
    // variants to keep the palette internally consistent.
    // zh_CN: 单个强调色驱动明暗模式，并清除旧派生值以保持调色板一致。
    const QString hex = colorToHex(accent);
    const UserSpecResult userSpec = readUserSpec(theme);
    if (!userSpec.canModify()) {
        qCWarning(fluent::logging::themeCategory).noquote()
            << QStringLiteral("StyleThemeCatalog refused to overwrite existing theme file %1")
                   .arg(userThemeFilePath(theme));
        return;
    }

    QJsonObject spec = userSpec.spec;
    for (const char* mode : {"light", "dark"}) {
        setNestedColor(spec, mode, "accentDefault", hex);
        for (const char* key : kDerivedAccentKeys)
            removeNestedColor(spec, mode, key);
    }
    writeUserSpec(theme, spec);
}

void clearUserAccent(StyleTheme theme)
{
    const UserSpecResult userSpec = readUserSpec(theme);
    if (!userSpec.canModify() || userSpec.spec.isEmpty())
        return;
    QJsonObject spec = userSpec.spec;
    // Drop accentDefault AND the derived variants so the theme reverts cleanly to the preset accent
    // (no half-overridden palette left behind).
    // zh_CN: 同时移除主强调色与派生值，完整回退到预设。
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
    // Fluent has no built-in override spec; use its seed accent.
    // zh_CN: Fluent 没有内置覆盖 spec，回退到种子强调色。
    return dark ? QColor(QStringLiteral("#60CDFF")) : QColor(QStringLiteral("#005FB8"));
}

} // namespace StyleThemeCatalog

} // namespace fluent
