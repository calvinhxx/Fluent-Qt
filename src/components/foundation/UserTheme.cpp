#include "components/foundation/UserTheme.h"

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
#include "components/foundation/private/ThemeSpec_p.h"
#include "utils/private/FluentQtLogging_p.h"

namespace fluent {
namespace {

using fluent::FluentElement;
using fluent::ThemeRegistry;

const QString kThemeKey = QStringLiteral("fluent");

QString themesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/themes");
}

constexpr int kThemeSchemaVersion = 1;
constexpr qint64 kMaxThemeFileBytes = 256 * 1024;

void logThemeInfo(const QString& message)
{
    qCInfo(fluent::logging::themeCategory, "%s", qUtf8Printable(message));
}

void logThemeWarning(const QString& message)
{
    qCWarning(fluent::logging::themeCategory, "%s", qUtf8Printable(message));
}

// Parse "#RRGGBB", "#RRGGBBAA", or Qt color names; returns invalid QColor so callers keep the existing token.
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

QString colorToHex(const QColor& c)
{
    if (c.alpha() == 255)
        return QString::asprintf("#%02X%02X%02X", c.red(), c.green(), c.blue());
    return QString::asprintf("#%02X%02X%02X%02X", c.red(), c.green(), c.blue(), c.alpha());
}

// Single source of truth so JSON parsing (applyColorSpec) and template export (colorsToJson) stay in sync.
template <typename Fn> void forEachColorField(FluentElement::Colors& c, Fn&& fn)
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
    fn("strokeStrong", c.strokeStrong);
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

    fn("bgLayerOverlay", c.bgLayerOverlay);
}

// Emit every overridable key so exported templates round-trip losslessly.
QJsonObject colorsToJson(const FluentElement::Colors& colors)
{
    FluentElement::Colors copy = colors;
    QJsonObject obj;
    forEachColorField(copy, [&](const char* key, QColor& field) { obj.insert(QLatin1String(key), colorToHex(field)); });
    return obj;
}

// Derive missing accent variants from accentDefault so sparse user files only need one key.
void applyColorSpec(FluentElement::Colors& c, const QJsonObject& obj, bool keepAccentVariantsOpaque = false)
{
    forEachColorField(c, [&](const char* key, QColor& field) {
        const QJsonValue v = obj.value(QLatin1String(key));
        if (v.isString()) {
            const QColor parsed = parseColor(v.toString());
            if (parsed.isValid())
                field = parsed;
        }
    });

    if (obj.contains(QLatin1String("accentDefault"))) {
        if (!obj.contains(QLatin1String("accentSecondary"))) {
            c.accentSecondary = keepAccentVariantsOpaque ? c.accentDefault : withAlpha(c.accentDefault, 230);
        }
        if (!obj.contains(QLatin1String("accentTertiary"))) {
            c.accentTertiary = keepAccentVariantsOpaque ? c.accentDefault : withAlpha(c.accentDefault, 204);
        }
        if (!obj.contains(QLatin1String("textAccentPrimary")))
            c.textAccentPrimary = c.accentDefault;
        if (!obj.contains(QLatin1String("textOnAccent"))) {
            c.textOnAccent = c.accentDefault.lightnessF() > 0.6 ? QColor(Qt::black) : QColor(Qt::white);
        }
    }
}

void applySpec(ThemeRegistry::ExtendedSnapshot& snapshot, const QJsonObject& spec)
{
    if (spec.contains(QLatin1String("radius"))) {
        const QJsonObject r = spec.value(QLatin1String("radius")).toObject();
        const FluentElement::Radius base = snapshot.base.radius;
        // Clamp radii on load so malformed JSON cannot push extreme values into widget painting.
        constexpr int kMaxRadius = 64;
        snapshot.base.radius = {qBound(0, r.value(QLatin1String("none")).toInt(base.none), kMaxRadius),
            qBound(0, r.value(QLatin1String("control")).toInt(base.control), kMaxRadius),
            qBound(0, r.value(QLatin1String("overlay")).toInt(base.overlay), kMaxRadius)};
    }

    if (spec.contains(QLatin1String("font"))) {
        const QJsonObject f = spec.value(QLatin1String("font")).toObject();
        if (f.contains(QLatin1String("family")))
            snapshot.base.fontFamilyOverride = f.value(QLatin1String("family")).toString();
        if (f.contains(QLatin1String("scale"))) {
            // Clamp font scale to prevent extreme QFont sizes from malformed JSON.
            snapshot.base.fontScale = qBound(0.5, f.value(QLatin1String("scale")).toDouble(1.0), 4.0);
        }
    }

    if (spec.contains(QLatin1String("light"))) {
        applyColorSpec(snapshot.base.lightColors, spec.value(QLatin1String("light")).toObject());
    }
    if (spec.contains(QLatin1String("dark"))) {
        applyColorSpec(snapshot.base.darkColors, spec.value(QLatin1String("dark")).toObject());
    }
    if (spec.contains(QLatin1String("contrast"))) {
        applyColorSpec(snapshot.contrastColors, spec.value(QLatin1String("contrast")).toObject(), true);
    }
}

// Snapshot default tokens before user overrides so exportTemplate() writes a clean baseline.
QJsonObject resolvedPresetSpec()
{
    ThemeRegistry::ExtendedSnapshot snapshot = ThemeRegistry::defaultExtendedSnapshot();

    QJsonObject radius;
    radius.insert(QStringLiteral("none"), snapshot.base.radius.none);
    radius.insert(QStringLiteral("control"), snapshot.base.radius.control);
    radius.insert(QStringLiteral("overlay"), snapshot.base.radius.overlay);

    QJsonObject spec;
    spec.insert(QStringLiteral("radius"), radius);
    spec.insert(QStringLiteral("font"),
        QJsonObject{{QStringLiteral("family"), snapshot.base.fontFamilyOverride},
            {QStringLiteral("scale"), snapshot.base.fontScale}});
    spec.insert(QStringLiteral("light"), colorsToJson(snapshot.base.lightColors));
    spec.insert(QStringLiteral("dark"), colorsToJson(snapshot.base.darkColors));
    spec.insert(QStringLiteral("contrast"), colorsToJson(snapshot.contrastColors));
    return spec;
}

enum class UserSpecState { Missing, Current, Legacy, Rejected };

struct UserSpecResult {
    UserSpecState state = UserSpecState::Missing;
    QJsonObject spec;

    bool isUsable() const { return state == UserSpecState::Current || state == UserSpecState::Legacy; }

    bool canModify() const { return state != UserSpecState::Rejected; }
};

bool isLegacyFlatSpec(const QJsonObject& root)
{
    if (root.contains(QLatin1String("schemaVersion")) || root.contains(QLatin1String("theme"))
        || root.contains(QLatin1String("overrides"))) {
        return false;
    }

    bool containsSpecSection = false;
    for (const char* key : {"radius", "font", "light", "dark", "contrast"}) {
        const QLatin1String section(key);
        if (!root.contains(section))
            continue;
        if (!root.value(section).isObject())
            return false;
        containsSpecSection = true;
    }
    return containsSpecSection;
}

UserSpecResult readUserSpec()
{
    const QString path = themesDir() + QStringLiteral("/") + kThemeKey + QStringLiteral(".json");
    const bool exists = QFile::exists(path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (!exists)
            return {};
        logThemeWarning(QStringLiteral("UserTheme preserving unreadable theme file %1").arg(path));
        return {UserSpecState::Rejected, {}};
    }
    if (file.size() > kMaxThemeFileBytes) {
        logThemeWarning(QStringLiteral("UserTheme ignoring oversized theme file %1").arg(path));
        return {UserSpecState::Rejected, {}};
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        logThemeWarning(
            QStringLiteral("UserTheme ignoring malformed theme file %1: %2").arg(path, error.errorString()));
        return {UserSpecState::Rejected, {}};
    }

    const QJsonObject root = doc.object();
    if (isLegacyFlatSpec(root)) {
        logThemeInfo(QStringLiteral(
            "UserTheme loaded legacy flat theme file %1; "
            "the next explicit edit will migrate it")
                .arg(path));
        return {UserSpecState::Legacy, root};
    }

    if (root.value(QLatin1String("schemaVersion")).toInt(-1) != kThemeSchemaVersion) {
        logThemeWarning(QStringLiteral("UserTheme ignoring unsupported schema in %1").arg(path));
        return {UserSpecState::Rejected, {}};
    }
    if (root.value(QLatin1String("theme")).toString() != kThemeKey
        || !root.value(QLatin1String("overrides")).isObject()) {
        logThemeWarning(QStringLiteral("UserTheme ignoring invalid theme envelope %1").arg(path));
        return {UserSpecState::Rejected, {}};
    }
    return {UserSpecState::Current, root.value(QLatin1String("overrides")).toObject()};
}

bool writeUserSpec(const QJsonObject& spec)
{
    if (!QDir().mkpath(themesDir()))
        return false;
    const QString path = themesDir() + QStringLiteral("/") + kThemeKey + QStringLiteral(".json");

    QJsonObject root;
    root.insert(QLatin1String("schemaVersion"), kThemeSchemaVersion);
    root.insert(QLatin1String("theme"), kThemeKey);
    root.insert(QLatin1String("overrides"), spec);
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (payload.size() > kMaxThemeFileBytes)
        return false;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logThemeWarning(QStringLiteral("UserTheme could not write theme file %1").arg(path));
        return false;
    }
    if (file.write(payload) != payload.size() || !file.commit()) {
        logThemeWarning(QStringLiteral("UserTheme could not atomically commit theme file %1").arg(path));
        return false;
    }
    return true;
}

void setNestedColor(QJsonObject& obj, const char* mode, const char* key, const QString& value)
{
    QJsonObject m = obj.value(QLatin1String(mode)).toObject();
    m.insert(QLatin1String(key), value);
    obj.insert(QLatin1String(mode), m);
}

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

namespace detail {

bool validateThemeSpec(const QJsonObject& spec)
{
    for (auto it = spec.begin(); it != spec.end(); ++it) {
        if (!it.value().isObject())
            return false;
        const QJsonObject section = it.value().toObject();
        if (it.key() == QLatin1String("light") || it.key() == QLatin1String("dark")
            || it.key() == QLatin1String("contrast")) {
            FluentElement::Colors colors{};
            for (auto field = section.begin(); field != section.end(); ++field) {
                bool known = false;
                forEachColorField(colors, [&](const char* name, QColor&) {
                    known |= field.key() == QLatin1String(name);
                });
                if (!known || !field.value().isString() || !parseColor(field.value().toString()).isValid())
                    return false;
            }
        } else if (it.key() == QLatin1String("radius")) {
            for (auto field = section.begin(); field != section.end(); ++field) {
                if (field.key() != QLatin1String("none") && field.key() != QLatin1String("control")
                    && field.key() != QLatin1String("overlay"))
                    return false;
                const double value = field.value().toDouble(-1);
                if (!field.value().isDouble() || value < 0 || value > 64 || value != static_cast<int>(value))
                    return false;
            }
        } else if (it.key() == QLatin1String("font")) {
            for (auto field = section.begin(); field != section.end(); ++field) {
                if (field.key() == QLatin1String("family")) {
                    if (!field.value().isString())
                        return false;
                } else if (field.key() == QLatin1String("scale")) {
                    const double value = field.value().toDouble(-1);
                    if (!field.value().isDouble() || value < 0.5 || value > 4.0)
                        return false;
                } else {
                    return false;
                }
            }
        } else {
            return false;
        }
    }
    return true;
}

void applyThemeSpec(ThemeRegistry::ExtendedSnapshot& snapshot, const QJsonObject& spec)
{
    applySpec(snapshot, spec);
}

} // namespace detail

namespace UserTheme {

bool applyOverrides(const QJsonObject& overrides)
{
    if (!detail::validateThemeSpec(overrides))
        return false;
    auto& registry = ThemeRegistry::instance();
    auto next = registry.extendedSnapshot();
    applySpec(next, overrides);
    return registry.applyExtendedSnapshot(next);
}

void apply()
{
    ThemeRegistry& reg = ThemeRegistry::instance();
    ThemeRegistry::ExtendedSnapshot next = ThemeRegistry::defaultExtendedSnapshot();

    const UserSpecResult userSpec = readUserSpec();
    if (userSpec.isUsable() && !userSpec.spec.isEmpty())
        applySpec(next, userSpec.spec);

    reg.applyExtendedSnapshot(next);

    logThemeInfo(
        QStringLiteral("UserTheme applied Fluent overrides key=%1 revision=%2").arg(kThemeKey).arg(reg.revision()));
}

const char* const kDerivedAccentKeys[] = {"accentSecondary", "accentTertiary", "textAccentPrimary", "textOnAccent"};

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
    ThemeRegistry::ExtendedSnapshot next = registry.extendedSnapshot();
    applySpec(next, spec);
    registry.applyExtendedSnapshot(next);
}

QString filePath()
{
    return themesDir() + QStringLiteral("/") + kThemeKey + QStringLiteral(".json");
}

QString directory()
{
    return themesDir();
}

bool exportTemplate(bool overwrite)
{
    const QString path = filePath();
    if (!overwrite && QFile::exists(path))
        return false;

    const bool written = writeUserSpec(resolvedPresetSpec());
    if (written) {
        logThemeInfo(QStringLiteral("UserTheme exported editable template %1").arg(path));
    }
    return written;
}

void setAccent(const QColor& accent)
{
    if (!accent.isValid())
        return;
    // Drop derived accent keys so apply() re-derives a consistent palette from accentDefault.
    const QString hex = colorToHex(accent);
    const UserSpecResult userSpec = readUserSpec();
    if (!userSpec.canModify()) {
        logThemeWarning(QStringLiteral("UserTheme refused to overwrite existing theme file %1").arg(filePath()));
        return;
    }

    QJsonObject spec = userSpec.spec;
    for (const char* mode : {"light", "dark"}) {
        setNestedColor(spec, mode, "accentDefault", hex);
        for (const char* key : kDerivedAccentKeys)
            removeNestedColor(spec, mode, key);
    }
    writeUserSpec(spec);
}

void clearAccent()
{
    const UserSpecResult userSpec = readUserSpec();
    if (!userSpec.canModify() || userSpec.spec.isEmpty())
        return;
    QJsonObject spec = userSpec.spec;
    // Drop derived accent keys alongside accentDefault so the theme reverts cleanly to defaults.
    for (const char* mode : {"light", "dark"}) {
        removeNestedColor(spec, mode, "accentDefault");
        for (const char* key : kDerivedAccentKeys)
            removeNestedColor(spec, mode, key);
    }
    writeUserSpec(spec);
}

QColor defaultAccent(bool dark)
{
    // Built-in seed accent, unaffected by user overrides.
    return dark ? QColor(QStringLiteral("#60CDFF")) : QColor(QStringLiteral("#005FB8"));
}

} // namespace UserTheme

} // namespace fluent
