#ifndef TYPOGRAPHY_H
#define TYPOGRAPHY_H

#include <QFont>
#include <QObject>
#include <QString>

#include "compatibility/FontCompat.h"
#include "design/IconCatalog.h"

/**
 * @brief Defines Fluent typography roles, font metrics, and icon glyph tokens.
 * zh_CN: 定义 Fluent 排版角色、字体度量和图标字符 token。
 *
 * Font sizes and line heights are absolute pixel values measured from the
 * Windows UI Kit typography styles.
 * zh_CN: 字号和行高为从 Windows UI Kit 排版样式测得的绝对像素值。
 *
 * FluentQt ships renamed, hinted static faces from the open-source Inter font
 * so every supported Qt/platform combination resolves the same face:
 * - Text: Caption and body roles.
 * - Heading: Subtitle and Title roles.
 * - Display: Title Large and Display roles.
 * zh_CN: FluentQt 内置经过重命名的开源 Inter 静态实例：Text 用于说明与正文，
 * zh_CN: Heading 用于副标题和标题，Display 用于大标题和展示文本。
 */
namespace Typography {
    Q_NAMESPACE

    // Font family tokens.
    // zh_CN: 字体家族 token。
    namespace FontFamily {
        extern const QString UI;
        extern const QString UIText;
        extern const QString UIHeading;
        extern const QString UIDisplay;
        extern const QString FluentIcons;
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

    // Optical design sizes used by the bundled Fluent UI System Icons face.
    // WinUI's normal 32 px controls use a 16 px icon slot; compact indicators
    // such as chevrons and checkmarks use the native 12 px drawing.
    // zh_CN: 内置 Fluent UI System Icons 字形的光学设计尺寸。WinUI 的常规 32 px
    // 控件使用 16 px 图标槽，chevron/checkmark 等紧凑指示符使用原生 12 px 字形。
    namespace IconSize {
        const int Compact  = 12;
        const int Standard = 16;
        const int Large    = 20;
        const int XLarge   = 24;
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

    // Semantic shortcuts rendered by the complete bundled FluentQt Icons
    // face. Use Icons::catalog()/glyph() for the complete upstream collection.
    // Glyph provenance is documented in THIRD_PARTY_NOTICES.md.
    // zh_CN: 完整内置 FluentQt Icons 字体中的常用语义快捷项；完整上游集合请使用
    // Icons::catalog()/glyph()。字形来源见 THIRD_PARTY_NOTICES.md。
    namespace Icons {
        // Navigation & Window
        extern const QString GlobalNav;
        extern const QString ChevronDown;
        extern const QString ChevronUp;
        extern const QString ChevronLeft;
        extern const QString ChevronRight;
        extern const QString ChevronDownMed;
        extern const QString ChevronUpMed;
        extern const QString ChevronLeftMed;
        extern const QString ChevronRightMed;
        // FlipView-specific arrows, smaller and lighter than common chevrons.
        // zh_CN: FlipView 专用箭头，比常规 Chevron 更小更细。
        extern const QString FlipViewPrevH;
        extern const QString FlipViewNextH;
        extern const QString FlipViewPrevV;
        extern const QString FlipViewNextV;
        extern const QString Back;
        extern const QString Forward;
        extern const QString Home;
        extern const QString Menu;
        extern const QString Up;
        extern const QString Down;
        extern const QString FullScreen;
        extern const QString BackToWindow;
        extern const QString More;
        extern const QString AllApps;
        extern const QString TitleBarBack;
        extern const QString ChromeMinimize;
        extern const QString ChromeMaximize;
        extern const QString ChromeRestore;
        extern const QString ChromeClose;

        // Common Actions & Editing
        extern const QString Add;
        extern const QString Cancel;
        extern const QString Delete;
        extern const QString Save;
        extern const QString SaveAs;
        extern const QString Search;
        extern const QString View;
        extern const QString Hide;
        extern const QString Settings;
        extern const QString Edit;
        extern const QString Refresh;
        extern const QString Share;
        extern const QString Copy;
        extern const QString Cut;
        extern const QString Paste;
        extern const QString Filter;
        extern const QString Link;
        extern const QString FavoriteStar;
        extern const QString FavoriteStarFill;
        extern const QString Pin;
        extern const QString PinFill;
        extern const QString Unpin;
        extern const QString Flag;
        extern const QString Block;
        extern const QString Zoom;
        extern const QString ZoomIn;
        extern const QString ZoomOut;
        extern const QString Undo;
        extern const QString Redo;
        extern const QString SelectAll;

        // Media & Sound
        extern const QString Play;
        extern const QString Pause;
        extern const QString Stop;
        extern const QString Volume;
        extern const QString Mute;
        extern const QString Microphone;
        extern const QString Video;
        extern const QString Camera;
        extern const QString Music;
        extern const QString Movie;
        extern const QString Headphones;
        extern const QString Speaker;
        extern const QString SkipBack;
        extern const QString SkipForward;

        // Communication & User
        extern const QString Mail;
        extern const QString People;
        extern const QString Phone;
        extern const QString Message;
        extern const QString Send;
        extern const QString Contact;
        extern const QString Group;
        extern const QString Emoji;
        extern const QString World;
        extern const QString ContactInfo;
        extern const QString Accounts;

        // Files & Folders
        extern const QString Folder;
        extern const QString File;
        extern const QString Document;
        extern const QString Cloud;
        extern const QString Download;
        extern const QString Upload;
        extern const QString Sync;
        extern const QString Storage;
        extern const QString Calculator;
        extern const QString Calendar;
        extern const QString Clock;
        extern const QString History;

        // System & Hardware
        extern const QString Wifi;
        extern const QString Bluetooth;
        extern const QString Battery;
        extern const QString Print;
        extern const QString Laptop;
        extern const QString Mobile;
        extern const QString Desktop;
        extern const QString AppIconDefault;
        extern const QString Mouse;
        extern const QString Keyboard;
        extern const QString Controller;
        extern const QString Power;
        extern const QString Brightness;
        extern const QString Airplane;

        // Feedback & Status
        extern const QString Warning;
        extern const QString ErrorIcon;
        extern const QString Info;
        extern const QString CheckMark;
        extern const QString Success;
        extern const QString AsteriskBadge12;
        extern const QString CheckmarkBadge12;
        extern const QString ErrorBadge12;
        extern const QString ImportantBadge12;
        extern const QString Hyphen;
        extern const QString Shield;
        extern const QString Lock;
        extern const QString Unlock;
        extern const QString PasswordKeyShow;
        extern const QString PasswordKeyHide;
        extern const QString RevealPasswordMedium;
        extern const QString Heart;
        extern const QString HeartFill;
        extern const QString Star;
        extern const QString Dismiss;
        extern const QString Clear;

        // Design & Layout
        extern const QString Brush;
        extern const QString Color;
        extern const QString Font;
        extern const QString Grid;
        extern const QString List;
        extern const QString AlignLeft;
        extern const QString AlignCenter;
        extern const QString AlignRight;
        extern const QString MapPin;

        // Weather
        extern const QString Sunny;
        extern const QString CloudWeather;
        extern const QString Rain;
        extern const QString Snow;
    }

    /**
     * @brief Strongly typed Fluent typography role.
     * zh_CN: 强类型 Fluent 排版角色。
     */
    enum class FontRole {
        Caption,
        Body,
        BodyStrong,
        BodyLarge,
        BodyLargeStrong,
        Subtitle,
        Title,
        TitleLarge,
        Display
    };
    Q_ENUM_NS(FontRole)

    /**
     * @brief Complete font metrics for one Fluent typography role.
     * zh_CN: 一个 Fluent 排版角色的完整字体度量。
     *
     * styleName selects an exact static Regular/Semibold face. Optical sizing
     * is encoded in the FluentQt-specific family itself.
     * zh_CN: styleName 选择精确的 Regular/Semibold 静态字体；光学尺寸已编码在
     * FluentQt 专用字族中。
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
            fluentApplyFontStyleName(font, styleName);
            fluentConfigureTextRendering(font);
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
    // family selects the matching static optical-size family; styleName selects
    // the exact face within that family.
    // zh_CN: family 选择对应的静态光学尺寸字族，styleName 选择其中的精确字体。
    namespace Styles {
        extern const FontStyle Caption;
        extern const FontStyle Body;
        extern const FontStyle BodyStrong;
        extern const FontStyle BodyLarge;
        extern const FontStyle BodyLargeStrong;
        extern const FontStyle Subtitle;
        extern const FontStyle Title;
        extern const FontStyle TitleLarge;
        extern const FontStyle Display;
    }

    /**
     * @brief Returns the stable serialization key for a typography role.
     * zh_CN: 返回排版角色用于序列化的稳定 key。
     */
    inline QString fontRoleKey(FontRole role) {
        switch (role) {
        case FontRole::Caption:         return QStringLiteral("Caption");
        case FontRole::Body:            return QStringLiteral("Body");
        case FontRole::BodyStrong:      return QStringLiteral("BodyStrong");
        case FontRole::BodyLarge:       return QStringLiteral("BodyLarge");
        case FontRole::BodyLargeStrong: return QStringLiteral("BodyLargeStrong");
        case FontRole::Subtitle:        return QStringLiteral("Subtitle");
        case FontRole::Title:           return QStringLiteral("Title");
        case FontRole::TitleLarge:      return QStringLiteral("TitleLarge");
        case FontRole::Display:         return QStringLiteral("Display");
        }
        return QStringLiteral("Body");
    }

    /**
     * @brief Resolves the immutable metrics for a typography role.
     * zh_CN: 解析排版角色对应的不可变字体度量。
     */
    inline const FontStyle& fontStyle(FontRole role) {
        switch (role) {
        case FontRole::Caption:         return Styles::Caption;
        case FontRole::BodyStrong:      return Styles::BodyStrong;
        case FontRole::BodyLarge:       return Styles::BodyLarge;
        case FontRole::BodyLargeStrong: return Styles::BodyLargeStrong;
        case FontRole::Subtitle:        return Styles::Subtitle;
        case FontRole::Title:           return Styles::Title;
        case FontRole::TitleLarge:      return Styles::TitleLarge;
        case FontRole::Display:         return Styles::Display;
        case FontRole::Body:            return Styles::Body;
        }
        return Styles::Body;
    }
}

Q_DECLARE_METATYPE(Typography::FontRole)

#endif // TYPOGRAPHY_H
