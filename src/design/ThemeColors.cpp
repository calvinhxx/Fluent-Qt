#include "design/ThemeColors.h"

namespace ThemeColors::Light::Fill {

const QColor AccentDefault("#005FB8");
const QColor AccentSecondary(0, 95, 184, 230);
const QColor AccentTertiary(0, 95, 184, 204);
const QColor AccentDisabled(0, 0, 0, 37);
const QColor ControlDefault("#FFFFFF");
const QColor ControlSecondary(249, 249, 249, 242);
const QColor ControlTertiary(249, 249, 249, 204);
const QColor ControlDisabled(249, 249, 249, 77);
const QColor ControlAltTransparent(255, 255, 255, 0);
const QColor ControlAltSecondary(0, 0, 0, 5);
const QColor ControlAltTertiary(0, 0, 0, 15);
const QColor ControlAltQuarternary(0, 0, 0, 31);
const QColor SubtleTransparent(0, 0, 0, 0);
const QColor SubtleSecondary(0, 0, 0, 9);
const QColor SubtleTertiary(0, 0, 0, 6);
const QColor SubtleDisabled(0, 0, 0, 0);

} // namespace ThemeColors::Light::Fill

namespace ThemeColors::Light::Stroke {

const QColor ControlDefault(0, 0, 0, 12);
const QColor ControlSecondary(0, 0, 0, 41);
const QColor ControlStrong(0, 0, 0, 112);
const QColor ControlOnImage(28, 28, 28, 196);
const QColor CardDefault(0, 0, 0, 13);
const QColor DividerDefault(0, 0, 0, 20);
const QColor SurfaceDefault("#757575");
const QColor FocusOuter(0, 0, 0, 230);
const QColor FocusInner("#FFFFFF");

} // namespace ThemeColors::Light::Stroke

namespace ThemeColors::Light::Text {

const QColor Primary(0, 0, 0, 230);
const QColor Secondary(0, 0, 0, 154);
const QColor Tertiary(0, 0, 0, 112);
const QColor Disabled(0, 0, 0, 92);
const QColor OnAccentPrimary("#FFFFFF");
const QColor OnAccentSecondary(255, 255, 255, 204);
const QColor OnAccentTertiary(255, 255, 255, 179);
const QColor OnAccentDisabled(255, 255, 255, 128);
const QColor AccentPrimary("#003E92");

} // namespace ThemeColors::Light::Text

namespace ThemeColors::Light::System {

const QColor Critical("#C42B1C");
const QColor CriticalBackground("#FDE7E9");
const QColor Caution("#9D5D00");
const QColor CautionBackground("#FFF4CE");
const QColor Informational("#015CDA");
const QColor InfoBackground("#F6F6F6");
const QColor Success("#0F7B0F");
const QColor SuccessBackground("#DFF6DD");

} // namespace ThemeColors::Light::System

namespace ThemeColors::Light {

const QColor BackgroundCanvas("#F3F3F3");
const QColor BackgroundLayer("#FFFFFF");
const QColor BackgroundLayerAlt("#F9F9F9");
const QColor BackgroundSolid("#EEEEEE");
const QColor Grey10("#FAF9F8");
const QColor Grey20("#F3F2F1");
const QColor Grey30("#EDEBE9");
const QColor Grey40("#E1DFDD");
const QColor Grey50("#D2D0CE");
const QColor Grey60("#C8C6C4");
const QColor Grey80("#B3B0AD");
const QColor Grey90("#A19F9D");
const QColor Grey100("#979593");
const QColor Grey110("#8A8886");
const QColor Grey120("#797775");
const QColor Grey130("#605E5C");
const QColor Grey140("#484644");
const QColor Grey150("#3B3A39");
const QColor Grey160("#323130");
const QColor Grey170("#292827");
const QColor Grey180("#252423");
const QColor Grey190("#201F1E");
const QColor Grey200("#11100F");
const std::vector<QColor> Charts = {
    QColor("#005FB8"), QColor("#00BCF2"), QColor("#2B88D8"),
    QColor("#004B50"), QColor("#00AD56"), QColor("#007833"),
    QColor("#881798"), QColor("#B4009E"), QColor("#E3008C"),
    QColor("#D83B01"), QColor("#EA4300"), QColor("#FF8C00")
};

} // namespace ThemeColors::Light

namespace ThemeColors::Dark::Fill {

const QColor AccentDefault("#60CDFF");
const QColor AccentSecondary(96, 205, 255, 230);
const QColor AccentTertiary(96, 205, 255, 204);
const QColor AccentDisabled(255, 255, 255, 37);
const QColor ControlDefault(255, 255, 255, 15);
const QColor ControlSecondary(255, 255, 255, 23);
const QColor ControlTertiary(255, 255, 255, 10);
const QColor ControlDisabled(255, 255, 255, 6);
const QColor ControlAltTransparent(255, 255, 255, 0);
const QColor ControlAltSecondary(255, 255, 255, 15);
const QColor ControlAltTertiary(255, 255, 255, 20);
const QColor ControlAltQuarternary(255, 255, 255, 30);
const QColor SubtleTransparent(255, 255, 255, 0);
const QColor SubtleSecondary(255, 255, 255, 15);
const QColor SubtleTertiary(255, 255, 255, 10);
const QColor SubtleDisabled(255, 255, 255, 0);

} // namespace ThemeColors::Dark::Fill

namespace ThemeColors::Dark::Stroke {

const QColor ControlDefault(255, 255, 255, 17);
const QColor ControlSecondary(255, 255, 255, 23);
const QColor ControlStrong(255, 255, 255, 138);
const QColor ControlOnImage(255, 255, 255, 26);
const QColor CardDefault(255, 255, 255, 10);
const QColor DividerDefault(255, 255, 255, 20);
const QColor SurfaceDefault(255, 255, 255, 30);
const QColor FocusOuter(255, 255, 255, 230);
const QColor FocusInner(0, 0, 0, 230);

} // namespace ThemeColors::Dark::Stroke

namespace ThemeColors::Dark::Text {

const QColor Primary("#FFFFFF");
const QColor Secondary(255, 255, 255, 199);
const QColor Tertiary(255, 255, 255, 138);
const QColor Disabled(255, 255, 255, 92);
const QColor OnAccentPrimary("#000000");
const QColor OnAccentSecondary(0, 0, 0, 128);
const QColor OnAccentTertiary(0, 0, 0, 77);
const QColor OnAccentDisabled(0, 0, 0, 128);
const QColor AccentPrimary("#99EBFF");

} // namespace ThemeColors::Dark::Text

namespace ThemeColors::Dark::System {

const QColor Critical("#FF99A4");
const QColor CriticalBackground("#442726");
const QColor Caution("#FCE100");
const QColor CautionBackground("#433519");
const QColor Informational("#60CDFF");
const QColor InfoBackground("#1F3150");
const QColor Success("#6CCB5F");
const QColor SuccessBackground("#1E3C1F");

} // namespace ThemeColors::Dark::System

namespace ThemeColors::Dark {

const QColor BackgroundCanvas("#202020");
const QColor BackgroundLayer("#2C2C2C");
const QColor BackgroundLayerAlt("#3D3D3D");
const QColor BackgroundSolid("#1C1C1C");
const QColor Grey10("#FAF9F8");
const QColor Grey20("#F3F2F1");
const QColor Grey30("#EDEBE9");
const QColor Grey40("#E1DFDD");
const QColor Grey50("#D2D0CE");
const QColor Grey60("#C8C6C4");
const QColor Grey90("#A19F9D");
const QColor Grey130("#605E5C");
const QColor Grey160("#323130");
const QColor Grey190("#201F1E");
const std::vector<QColor> Charts = {
    QColor("#60CDFF"), QColor("#00BCF2"), QColor("#2B88D8"),
    QColor("#00AD56"), QColor("#107C10"), QColor("#004B50"),
    QColor("#FF8C00"), QColor("#F7630C"), QColor("#EA4300"),
    QColor("#E3008C"), QColor("#BF0077"), QColor("#C239B3")
};

} // namespace ThemeColors::Dark

namespace ThemeColors::Contrast::Fill {

const QColor AccentDefault("#1AEBFF");
const QColor AccentSelected("#000000");
const QColor ControlDefault("#000000");
const QColor ControlFocus("#000000");
const QColor ButtonText("#FFFFFF");

} // namespace ThemeColors::Contrast::Fill

namespace ThemeColors::Contrast::Stroke {

const QColor ControlDefault("#FFFFFF");
const QColor ControlFocused("#1AEBFF");
const QColor ButtonBorder("#FFFFFF");

} // namespace ThemeColors::Contrast::Stroke

namespace ThemeColors::Contrast::Text {

const QColor Primary("#FFFFFF");
const QColor Secondary("#FFFFFF");
const QColor Disabled("#3FF23F");
const QColor OnAccent("#000000");
const QColor Hyperlink("#FFFF00");

} // namespace ThemeColors::Contrast::Text

namespace ThemeColors::Contrast {

const QColor BackgroundCanvas("#000000");
const QColor BackgroundLayer("#000000");

} // namespace ThemeColors::Contrast
