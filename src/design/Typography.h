#ifndef TYPOGRAPHY_H
#define TYPOGRAPHY_H

#include <QFont>
#include <QString>

/**
 * @brief Defines Fluent typography roles, font metrics, and icon glyph tokens.
 * zh_CN: 定义 Fluent 排版角色、字体度量和图标字符 token。
 *
 * Font sizes and line heights are absolute pixel values measured from the
 * Windows UI Kit typography styles.
 * zh_CN: 字号和行高为从 Windows UI Kit 排版样式测得的绝对像素值。
 *
 * Segoe UI Variable optical sizes:
 * - Small: Caption and small text.
 * - Text: Body and Body Large roles.
 * - Display: Subtitle and larger roles.
 * zh_CN: Segoe UI Variable 光学尺寸中，Small 用于 Caption，Text 用于 Body 系列，
 * zh_CN: Display 用于 Subtitle 及以上角色。
 */
namespace Typography {

    // Font family tokens.
    // zh_CN: 字体家族 token。
    namespace FontFamily {
        const QString SegoeUI          = "Segoe UI Variable";
        const QString SegoeUIVariable  = "Segoe UI Variable";
        const QString SegoeUISmall     = "Segoe UI Variable Small";    // Optical size: Small. zh_CN: 光学尺寸 Small。
        const QString SegoeUIText      = "Segoe UI Variable Text";     // Optical size: Text. zh_CN: 光学尺寸 Text。
        const QString SegoeUIDisplay   = "Segoe UI Variable Display";  // Optical size: Display. zh_CN: 光学尺寸 Display。
        const QString SegoeFluentIcons = "Segoe Fluent Icons";
    }

    // Font sizes in pixels, measured from Figma.
    // zh_CN: 从 Figma 实测的字体大小，单位为像素。
    namespace FontSize {
        const int Caption         = 12;  // Small Regular    12/16
        const int Body            = 14;  // Text Regular     14/20
        const int BodyStrong      = 14;  // Text Semibold    14/20
        const int BodyLarge       = 18;  // Text Regular     18/24
        const int BodyLargeStrong = 18;  // Text Semibold    18/24
        const int Subtitle        = 20;  // Display Semibold 20/28
        const int Title           = 28;  // Display Semibold 28/36
        const int TitleLarge      = 40;  // Display Semibold 40/52
        const int Display         = 68;  // Display Semibold 68/92
    }

    // Line heights in absolute pixels, not multipliers.
    // zh_CN: 行高为绝对像素值，不是倍率。
    namespace LineHeight {
        const int Caption         = 16;
        const int Body            = 20;
        const int BodyStrong      = 20;
        const int BodyLarge       = 24;
        const int BodyLargeStrong = 24;
        const int Subtitle        = 28;
        const int Title           = 36;
        const int TitleLarge      = 52;
        const int Display         = 92;
    }

    // Font weights measured from Figma.
    // zh_CN: 从 Figma 实测的字体粗细。
    // Display and TitleLarge use SemiBold (600), not Bold (700).
    // zh_CN: Display 和 TitleLarge 使用 SemiBold(600)，不是 Bold(700)。
    namespace FontWeight {
        const int Regular  = QFont::Normal;   // 400
        const int Medium   = QFont::Medium;   // 500; fallback not defined by Figma. zh_CN: 备用档，Figma 未定义。
        const int SemiBold = QFont::DemiBold; // 600; used by heading roles. zh_CN: 标题类主要使用。
        const int Bold     = QFont::Bold;     // 700; fallback not defined by current Figma styles. zh_CN: 备用档，当前 Figma 未定义。
    }

    // Icon glyphs from Segoe Fluent Icons / MDL2 Assets.
    // zh_CN: 来自 Segoe Fluent Icons / MDL2 Assets 的图标字符。
    // Reference: https://learn.microsoft.com/en-us/windows/apps/design/style/segoe-fluent-icons-font
    namespace Icons {
        // Navigation & Window
        const QString GlobalNav       = QString::fromUtf16(u"\uE700");
        const QString ChevronDown     = QString::fromUtf16(u"\uE70D");
        const QString ChevronUp       = QString::fromUtf16(u"\uE70E");
        const QString ChevronLeft     = QString::fromUtf16(u"\uE76B");
        const QString ChevronRight    = QString::fromUtf16(u"\uE76C");
        const QString ChevronDownMed  = QString::fromUtf16(u"\uE972");
        const QString ChevronUpMed    = QString::fromUtf16(u"\uE971");
        const QString ChevronLeftMed  = QString::fromUtf16(u"\uE973");
        const QString ChevronRightMed = QString::fromUtf16(u"\uE974");
        // FlipView-specific arrows, smaller and lighter than common chevrons.
        // zh_CN: FlipView 专用箭头，比常规 Chevron 更小更细。
        const QString FlipViewPrevH   = QString::fromUtf16(u"\uEDD9");
        const QString FlipViewNextH   = QString::fromUtf16(u"\uEDDA");
        const QString FlipViewPrevV   = QString::fromUtf16(u"\uEDDB");
        const QString FlipViewNextV   = QString::fromUtf16(u"\uEDDC");
        const QString Back            = QString::fromUtf16(u"\uE72B");
        const QString Forward         = QString::fromUtf16(u"\uE72A");
        const QString Home            = QString::fromUtf16(u"\uE80F");
        const QString Menu            = QString::fromUtf16(u"\uE700");
        const QString Up              = QString::fromUtf16(u"\uE74A");
        const QString Down            = QString::fromUtf16(u"\uE74B");
        const QString FullScreen      = QString::fromUtf16(u"\uE740");
        const QString BackToWindow    = QString::fromUtf16(u"\uE73F");
        const QString More            = QString::fromUtf16(u"\uE712");
        const QString AllApps         = QString::fromUtf16(u"\uE71D");
        const QString TitleBarBack    = QString::fromUtf16(u"\uE830");
        const QString ChromeMinimize  = QString::fromUtf16(u"\uE921");
        const QString ChromeMaximize  = QString::fromUtf16(u"\uE922");
        const QString ChromeRestore   = QString::fromUtf16(u"\uE923");
        const QString ChromeClose     = QString::fromUtf16(u"\uE8BB");

        // Common Actions & Editing
        const QString Add             = QString::fromUtf16(u"\uE710");
        const QString Cancel          = QString::fromUtf16(u"\uE711");
        const QString Delete          = QString::fromUtf16(u"\uE74D");
        const QString Save            = QString::fromUtf16(u"\uE74E");
        const QString SaveAs          = QString::fromUtf16(u"\uE792");
        const QString Search          = QString::fromUtf16(u"\uE721");
        const QString View            = QString::fromUtf16(u"\uE890");
        const QString Hide            = QString::fromUtf16(u"\uED1A");
        const QString Settings        = QString::fromUtf16(u"\uE713");
        const QString Edit            = QString::fromUtf16(u"\uE70F");
        const QString Refresh         = QString::fromUtf16(u"\uE72C");
        const QString Share           = QString::fromUtf16(u"\uE72D");
        const QString Copy            = QString::fromUtf16(u"\uE8C8");
        const QString Cut             = QString::fromUtf16(u"\uE8C6");
        const QString Paste           = QString::fromUtf16(u"\uE8C7");
        const QString Filter          = QString::fromUtf16(u"\uE71C");
        const QString Link            = QString::fromUtf16(u"\uE71B");
        const QString FavoriteStar    = QString::fromUtf16(u"\uE734");
        const QString FavoriteStarFill= QString::fromUtf16(u"\uE735");
        const QString Pin             = QString::fromUtf16(u"\uE718");
        const QString PinFill         = QString::fromUtf16(u"\uE746");
        const QString Unpin           = QString::fromUtf16(u"\uE77A");
        const QString Flag            = QString::fromUtf16(u"\uE7C1");
        const QString Block           = QString::fromUtf16(u"\uE733");
        const QString Zoom            = QString::fromUtf16(u"\uE71E");
        const QString ZoomIn          = QString::fromUtf16(u"\uE8A3");
        const QString ZoomOut         = QString::fromUtf16(u"\uE71F");
        const QString Undo            = QString::fromUtf16(u"\uE7A7");
        const QString Redo            = QString::fromUtf16(u"\uE7A6");
        const QString SelectAll       = QString::fromUtf16(u"\uE8B3");

        // Media & Sound
        const QString Play            = QString::fromUtf16(u"\uE768");
        const QString Pause           = QString::fromUtf16(u"\uE769");
        const QString Stop            = QString::fromUtf16(u"\uE71A");
        const QString Volume          = QString::fromUtf16(u"\uE767");
        const QString Mute            = QString::fromUtf16(u"\uE74F");
        const QString Microphone      = QString::fromUtf16(u"\uE720");
        const QString Video           = QString::fromUtf16(u"\uE714");
        const QString Camera          = QString::fromUtf16(u"\uE722");
        const QString Music           = QString::fromUtf16(u"\uE8D6");
        const QString Movie           = QString::fromUtf16(u"\uE8B2");
        const QString Headphones      = QString::fromUtf16(u"\uE7F6");
        const QString Speaker         = QString::fromUtf16(u"\uE7F5");
        const QString SkipBack        = QString::fromUtf16(u"\uE892");
        const QString SkipForward     = QString::fromUtf16(u"\uE893");

        // Communication & User
        const QString Mail            = QString::fromUtf16(u"\uE715");
        const QString People          = QString::fromUtf16(u"\uE716");
        const QString Phone           = QString::fromUtf16(u"\uE717");
        const QString Message         = QString::fromUtf16(u"\uE891");
        const QString Send            = QString::fromUtf16(u"\uE724");
        const QString Contact         = QString::fromUtf16(u"\uE77B");
        const QString Group           = QString::fromUtf16(u"\uE902");
        const QString Emoji           = QString::fromUtf16(u"\uE899");
        const QString World           = QString::fromUtf16(u"\uE909");
        const QString ContactInfo     = QString::fromUtf16(u"\uE779");
        const QString Accounts        = QString::fromUtf16(u"\uE910");

        // Files & Folders
        const QString Folder          = QString::fromUtf16(u"\uE838");
        const QString File            = QString::fromUtf16(u"\uE8A5");
        const QString Document        = QString::fromUtf16(u"\uE8A5");
        const QString Cloud           = QString::fromUtf16(u"\uE753");
        const QString Download        = QString::fromUtf16(u"\uE896");
        const QString Upload          = QString::fromUtf16(u"\uE898");
        const QString Sync            = QString::fromUtf16(u"\uE895");
        const QString Storage         = QString::fromUtf16(u"\uE8B7");
        const QString Calculator      = QString::fromUtf16(u"\uE8EF");
        const QString Calendar        = QString::fromUtf16(u"\uE787");
        const QString Clock           = QString::fromUtf16(u"\uE917");
        const QString History         = QString::fromUtf16(u"\uE81C");

        // System & Hardware
        const QString Wifi            = QString::fromUtf16(u"\uE701");
        const QString Bluetooth       = QString::fromUtf16(u"\uE702");
        const QString Battery         = QString::fromUtf16(u"\uE83F");
        const QString Print           = QString::fromUtf16(u"\uE749");
        const QString Laptop          = QString::fromUtf16(u"\uE7F8");
        const QString Mobile          = QString::fromUtf16(u"\uE8EA");
        const QString Desktop         = QString::fromUtf16(u"\uE9EE");
        const QString AppIconDefault  = QString::fromUtf16(u"\uEA3A");
        const QString Mouse           = QString::fromUtf16(u"\uE962");
        const QString Keyboard        = QString::fromUtf16(u"\uE92E");
        const QString Controller      = QString::fromUtf16(u"\uE7FC");
        const QString Power           = QString::fromUtf16(u"\uE7E8");
        const QString Brightness      = QString::fromUtf16(u"\uE706");
        const QString Airplane        = QString::fromUtf16(u"\uE709");

        // Feedback & Status
        const QString Warning         = QString::fromUtf16(u"\uE7BA");
        const QString ErrorIcon       = QString::fromUtf16(u"\uE783");
        const QString Info            = QString::fromUtf16(u"\uE946");
        const QString CheckMark       = QString::fromUtf16(u"\uE73E");
        const QString Success         = QString::fromUtf16(u"\uE73E");
        const QString AsteriskBadge12 = QString::fromUtf16(u"\uE625");
        const QString CheckmarkBadge12= QString::fromUtf16(u"\uE65F");
        const QString ErrorBadge12    = QString::fromUtf16(u"\uEDAE");
        const QString ImportantBadge12= QString::fromUtf16(u"\uEDB1");
        const QString Hyphen          = QString::fromUtf16(u"\uE738");
        const QString Shield          = QString::fromUtf16(u"\uEA18");
        const QString Lock            = QString::fromUtf16(u"\uE72E");
        const QString Unlock          = QString::fromUtf16(u"\uE785");
        const QString PasswordKeyShow = QString::fromUtf16(u"\uE9A8");
        const QString PasswordKeyHide = QString::fromUtf16(u"\uE9A9");
        const QString RevealPasswordMedium = QString::fromUtf16(u"\uF78D");
        const QString Heart           = QString::fromUtf16(u"\uEB51");
        const QString HeartFill       = QString::fromUtf16(u"\uEB52");
        const QString Star            = QString::fromUtf16(u"\uE74C");
        const QString Dismiss         = QString::fromUtf16(u"\uE894");
        const QString Clear           = QString::fromUtf16(u"\uE894");

        // Design & Layout
        const QString Brush           = QString::fromUtf16(u"\uE790");
        const QString Color           = QString::fromUtf16(u"\uE790");
        const QString Font            = QString::fromUtf16(u"\uE8D2");
        const QString Grid            = QString::fromUtf16(u"\uE80A");
        const QString List            = QString::fromUtf16(u"\uEA37");
        const QString AlignLeft       = QString::fromUtf16(u"\uE8E4");
        const QString AlignCenter     = QString::fromUtf16(u"\uE8E3");
        const QString AlignRight      = QString::fromUtf16(u"\uE8E2");
        const QString MapPin          = QString::fromUtf16(u"\uE707");

        // Weather
        const QString Sunny           = QString::fromUtf16(u"\uE706");
        const QString CloudWeather    = QString::fromUtf16(u"\uE753");
        const QString Rain            = QString::fromUtf16(u"\uE9D5");
        const QString Snow            = QString::fromUtf16(u"\uE944");
    }

    // FontRole string constants accepted by themeFont(role).
    // zh_CN: themeFont(role) 接受的 FontRole 字符串常量。
    namespace FontRole {
        const QString Caption         = QStringLiteral("Caption");
        const QString Body            = QStringLiteral("Body");
        const QString BodyStrong      = QStringLiteral("BodyStrong");
        const QString BodyLarge       = QStringLiteral("BodyLarge");
        const QString BodyLargeStrong = QStringLiteral("BodyLargeStrong");
        const QString Subtitle        = QStringLiteral("Subtitle");
        const QString Title           = QStringLiteral("Title");
        const QString TitleLarge      = QStringLiteral("TitleLarge");
        const QString Display         = QStringLiteral("Display");
    }

    /**
     * @brief Complete font metrics for one Fluent typography role.
     * zh_CN: 一个 Fluent 排版角色的完整字体度量。
     *
     * styleName maps to a Segoe UI Variable optical style and is applied through
     * QFont::setStyleName when available.
     * zh_CN: styleName 对应 Segoe UI Variable 光学尺寸变体，可用时通过 QFont::setStyleName 设置。
     */
    struct FontStyle {
        QString family;
        QString styleName;   // Figma style name, e.g. "Small Regular". zh_CN: Figma 字体样式名。
        int     size;        // px
        int     weight;      // QFont::Weight
        int     lineHeight;  // Absolute pixels measured from Figma. zh_CN: Figma 实测绝对像素值。

        QFont toQFont() const {
            QFont font(family, -1, weight);
            font.setPixelSize(size);
            if (!styleName.isEmpty())
                font.setStyleName(styleName);
            return font;
        }

        QString toStyleSheet() const {
            QString w;
            if      (weight == QFont::Bold)     w = "700";
            else if (weight == QFont::DemiBold) w = "600";
            else if (weight == QFont::Medium)   w = "500";
            else                                w = "400";
            return QString("font-family: \"%1\"; font-size: %2px; font-weight: %3; line-height: %4px;")
                .arg(family).arg(size).arg(w).arg(lineHeight);
        }
    };

    // Predefined style instances mapped to Figma typography variables.
    // zh_CN: 与 Figma 排版变量对应的预定义样式实例。
    // family uses the matching optical-size family; styleName remains as fallback metadata.
    // zh_CN: family 使用对应光学尺寸子族名，styleName 保留为回退元数据。
    namespace Styles {
        // Caption → "Segoe UI Variable Small" / Small Regular / 12px / 400 / 16px
        const FontStyle Caption = {
            FontFamily::SegoeUISmall, "Small Regular",
            FontSize::Caption, FontWeight::Regular, LineHeight::Caption
        };
        // Body → "Segoe UI Variable Text" / Text Regular / 14px / 400 / 20px
        const FontStyle Body = {
            FontFamily::SegoeUIText, "Text Regular",
            FontSize::Body, FontWeight::Regular, LineHeight::Body
        };
        // Body Strong → "Segoe UI Variable Text" / Text Semibold / 14px / 600 / 20px
        const FontStyle BodyStrong = {
            FontFamily::SegoeUIText, "Text Semibold",
            FontSize::BodyStrong, FontWeight::SemiBold, LineHeight::BodyStrong
        };
        // Body Large → "Segoe UI Variable Text" / Text Regular / 18px / 400 / 24px
        const FontStyle BodyLarge = {
            FontFamily::SegoeUIText, "Text Regular",
            FontSize::BodyLarge, FontWeight::Regular, LineHeight::BodyLarge
        };
        // Body Large Strong → "Segoe UI Variable Text" / Text Semibold / 18px / 600 / 24px
        const FontStyle BodyLargeStrong = {
            FontFamily::SegoeUIText, "Text Semibold",
            FontSize::BodyLargeStrong, FontWeight::SemiBold, LineHeight::BodyLargeStrong
        };
        // Subtitle → "Segoe UI Variable Display" / Display Semibold / 20px / 600 / 28px
        const FontStyle Subtitle = {
            FontFamily::SegoeUIDisplay, "Display Semibold",
            FontSize::Subtitle, FontWeight::SemiBold, LineHeight::Subtitle
        };
        // Title → "Segoe UI Variable Display" / Display Semibold / 28px / 600 / 36px
        const FontStyle Title = {
            FontFamily::SegoeUIDisplay, "Display Semibold",
            FontSize::Title, FontWeight::SemiBold, LineHeight::Title
        };
        // Title Large → "Segoe UI Variable Display" / Display Semibold / 40px / 600 / 52px
        const FontStyle TitleLarge = {
            FontFamily::SegoeUIDisplay, "Display Semibold",
            FontSize::TitleLarge, FontWeight::SemiBold, LineHeight::TitleLarge
        };
        // Display → "Segoe UI Variable Display" / Display Semibold / 68px / 600 / 92px
        const FontStyle Display = {
            FontFamily::SegoeUIDisplay, "Display Semibold",
            FontSize::Display, FontWeight::SemiBold, LineHeight::Display
        };
    }
}

#endif // TYPOGRAPHY_H
