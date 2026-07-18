#include <gtest/gtest.h>
#include <QApplication>
#include <QWidget>
#include <QListView>
#include <QAbstractListModel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QImage>
#include <QRawFont>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTabWidget>
#include <QScrollArea>
#include <QLabel>
#include <QFrame>
#include <algorithm>
#include <functional>
#include "components/foundation/FluentElement.h"
#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "QtTestEnvironment.h"

using namespace fluent;
using namespace fluent::basicinput;
using namespace fluent::textfields;

// =============================================================================
// 1. IconFont 测试部分（保留原有功能）
// =============================================================================

struct IconData {
    QString glyph;
    QString name;
};

class IconModel : public QAbstractListModel {
public:
    explicit IconModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    void setIcons(const QVector<IconData>& icons) {
        beginResetModel();
        m_allIcons = icons;
        m_displayIcons = icons;
        endResetModel();
    }

    void filter(const QString& text) {
        beginResetModel();
        if (text.isEmpty()) {
            m_displayIcons = m_allIcons;
        } else {
            m_displayIcons.clear();
            for (const auto& icon : m_allIcons) {
                if (icon.name.contains(text, Qt::CaseInsensitive)) {
                    m_displayIcons.append(icon);
                }
            }
        }
        endResetModel();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return m_displayIcons.size();
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= m_displayIcons.size()) return {};
        
        const auto& icon = m_displayIcons[index.row()];
        if (role == Qt::DisplayRole) return icon.name;
        if (role == Qt::UserRole) return icon.glyph;
        return {};
    }

private:
    QVector<IconData> m_allIcons;
    QVector<IconData> m_displayIcons;
};

class IconDelegate : public QStyledItemDelegate {
public:
    IconDelegate(const QString& iconFamily, QObject* parent = nullptr) 
        : QStyledItemDelegate(parent), m_iconFamily(iconFamily) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);

        bool isHovered = option.state & QStyle::State_MouseOver;
        bool isSelected = option.state & QStyle::State_Selected;

        // 绘制背景 (Hover/Selected 效果)
        if (isSelected || isHovered) {
            QColor bgColor = isSelected ? QColor(0, 120, 212, 40) : QColor(128, 128, 128, 20);
            painter->setBrush(bgColor);
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(option.rect.adjusted(2, 2, -2, -2), 4, 4);
        }

        QString glyph = index.data(Qt::UserRole).toString();
        QString name = index.data(Qt::DisplayRole).toString();

        // 绘制图标
        QFont iconFont(m_iconFamily);
        iconFont.setPixelSize(28);
        painter->setFont(iconFont);
        
        QColor iconColor = isHovered ? QColor(0, 120, 212) : painter->pen().color();
        painter->setPen(iconColor);
        
        QRect iconRect = option.rect.adjusted(0, 10, 0, -30);
        painter->drawText(iconRect, Qt::AlignCenter, glyph);

        // 绘制文字
        QFont textFont = option.font;
        textFont.setPixelSize(11);
        painter->setFont(textFont);
        painter->setPen(QColor(128, 128, 128));
        
        QRect textRect = option.rect.adjusted(5, 50, -5, -5);
        painter->drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, name);

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        return QSize(120, 90);
    }

private:
    QString m_iconFamily;
};

// =============================================================================
// 2. 普通文本字体测试部分（新增）
// =============================================================================

class TypographyTestWidget : public QWidget, public fluent::FluentElement {
public:
    TypographyTestWidget(const QString& uiFamily, QWidget* parent = nullptr) 
        : QWidget(parent), m_uiFamily(uiFamily) {
        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(40, 30, 40, 30);
        mainLayout->setSpacing(30);

        // 标题
        auto* titleLabel = new QLabel("FluentQt Typography Showcase", this);
        QFont titleFont(m_uiFamily);
        titleFont.setPixelSize(24);
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        mainLayout->addWidget(titleLabel);

        // 滚动区域
        auto* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setStyleSheet("background: transparent; border: none;");
        
        auto* contentWidget = new QWidget();
        auto* contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(20, 20, 20, 20);
        contentLayout->setSpacing(40);

        // 1. Fluent Typography 样式展示
        addSection(contentLayout, "Fluent Typography Styles", [this]() {
            return createTypographyStylesSection();
        });

        // 2. 字体粗细对比
        addSection(contentLayout, "Font Weight Comparison", [this]() {
            return createFontWeightSection();
        });

        // 3. 字体大小对比
        addSection(contentLayout, "Font Size Comparison", [this]() {
            return createFontSizeSection();
        });

        // 4. 原生字体 vs FluentQt 内置字体
        addSection(contentLayout, "Native Font vs FluentQt UI", [this]() {
            return createFontComparisonSection();
        });

        contentLayout->addStretch();
        scrollArea->setWidget(contentWidget);
        mainLayout->addWidget(scrollArea);

        onThemeUpdated();
    }

    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1; color: %2;")
            .arg(c.bgCanvas.name()).arg(c.textPrimary.name()));
    }

private:
    QString m_uiFamily;

    void addSection(QVBoxLayout* layout, const QString& title, std::function<QWidget*()> factory) {
        // 分组标题
        auto* sectionTitle = new QLabel(title, this);
        QFont sectionFont(m_uiFamily);
        sectionFont.setPixelSize(18);
        sectionFont.setBold(true);
        sectionTitle->setFont(sectionFont);
        layout->addWidget(sectionTitle);

        // 分隔线
        auto* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        line->setFixedHeight(1);
        layout->addWidget(line);

        // 内容
        QWidget* content = factory();
        layout->addWidget(content);

        // 间距
        layout->addSpacing(20);
    }

    QWidget* createTypographyStylesSection() {
        auto* widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);
        layout->setSpacing(20);

        struct StyleInfo {
            QString name;
            QString styleName;
            QString sampleText;
        };

        QVector<StyleInfo> styles = {
            {"Display", "Display", "The quick brown fox jumps over the lazy dog"},
            {"Title Large", "TitleLarge", "The quick brown fox jumps over the lazy dog"},
            {"Title", "Title", "The quick brown fox jumps over the lazy dog"},
            {"Subtitle", "Subtitle", "The quick brown fox jumps over the lazy dog"},
            {"Body Strong", "BodyStrong", "The quick brown fox jumps over the lazy dog"},
            {"Body", "Body", "The quick brown fox jumps over the lazy dog. This is standard body text used for most content."},
            {"Caption", "Caption", "The quick brown fox jumps over the lazy dog"}
        };

        for (const auto& style : styles) {
            auto* label = new Label(style.sampleText, widget);
            label->setFluentTypography(style.styleName);
            label->setWordWrap(true);
            layout->addWidget(label);

            // 显示样式信息
            auto* infoLabel = new QLabel(QString("  (%1 - %2px, %3)")
                .arg(style.name)
                .arg(label->font().pixelSize())
                .arg(label->font().weight() == QFont::Bold ? "Bold" :
                     label->font().weight() == QFont::DemiBold ? "SemiBold" :
                     label->font().weight() == QFont::Medium ? "Medium" : "Regular"), widget);
            QFont infoFont(m_uiFamily);
            infoFont.setPixelSize(11);
            infoLabel->setFont(infoFont);
            layout->addWidget(infoLabel);
            layout->addSpacing(10);
        }

        return widget;
    }

    QWidget* createFontWeightSection() {
        auto* widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);
        layout->setSpacing(15);

        QString sampleText = "The quick brown fox jumps over the lazy dog";
        int baseSize = 16;

        struct WeightInfo {
            QString name;
            QFont::Weight weight;
        };

        QVector<WeightInfo> weights = {
            {"Regular (400)", QFont::Normal},
            {"Medium (500)", QFont::Medium},
            {"SemiBold (600)", QFont::DemiBold},
            {"Bold (700)", QFont::Bold}
        };

        for (const auto& w : weights) {
            auto* label = new QLabel(sampleText, widget);
            QFont font(m_uiFamily);
            font.setPixelSize(baseSize);
            font.setWeight(w.weight);
            label->setFont(font);
            layout->addWidget(label);

            auto* infoLabel = new QLabel(QString("  %1").arg(w.name), widget);
            QFont infoFont(m_uiFamily);
            infoFont.setPixelSize(11);
            infoLabel->setFont(infoFont);
            layout->addWidget(infoLabel);
        }

        return widget;
    }

    QWidget* createFontSizeSection() {
        auto* widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);
        layout->setSpacing(15);

        QString sampleText = "Aa";

        QVector<int> sizes = {12, 14, 16, 18, 20, 24, 28, 32, 36, 40, 48};

        for (int size : sizes) {
            auto* label = new QLabel(sampleText, widget);
            QFont font(m_uiFamily);
            font.setPixelSize(size);
            label->setFont(font);
            layout->addWidget(label);

            auto* infoLabel = new QLabel(QString("  %1px").arg(size), widget);
            QFont infoFont(m_uiFamily);
            infoFont.setPixelSize(11);
            infoLabel->setFont(infoFont);
            layout->addWidget(infoLabel);
        }

        return widget;
    }

    QWidget* createFontComparisonSection() {
        auto* widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);
        layout->setSpacing(20);

        QString sampleText = "The quick brown fox jumps over the lazy dog. 0123456789";

        // 当前平台的原生 UI 字体。
        auto* nativeLabel = new QLabel("Native System Font:", widget);
        QFont nativeTitleFont(m_uiFamily);
        nativeTitleFont.setPixelSize(14);
        nativeTitleFont.setBold(true);
        nativeLabel->setFont(nativeTitleFont);
        layout->addWidget(nativeLabel);

        QFont nativeFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        nativeFont.setPixelSize(16);
        auto* nativeText = new QLabel(sampleText, widget);
        nativeText->setFont(nativeFont);
        layout->addWidget(nativeText);
        layout->addSpacing(10);

        // FluentQt 打包并注册的跨平台字体。
        auto* bundledLabel = new QLabel("FluentQt UI (Bundled):", widget);
        QFont bundledTitleFont(m_uiFamily);
        bundledTitleFont.setPixelSize(14);
        bundledTitleFont.setBold(true);
        bundledLabel->setFont(bundledTitleFont);
        layout->addWidget(bundledLabel);

        QFont bundledFont(m_uiFamily);
        bundledFont.setPixelSize(16);
        auto* bundledText = new QLabel(sampleText, widget);
        bundledText->setFont(bundledFont);
        layout->addWidget(bundledText);

        return widget;
    }
};

// =============================================================================
// 3. 主测试窗口（Tab 组织）
// =============================================================================

class TypographyVisualCheckWindow : public QWidget, public fluent::FluentElement {
public:
    TypographyVisualCheckWindow(const QString& iconFamily, const QString& uiFamily) {
        setFixedSize(1200, 800);
        setWindowTitle("FluentQt Typography & Icon Test Suite");

        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // Tab 控件
        m_tabs = new QTabWidget(this);
        m_tabs->setStyleSheet(
            "QTabWidget::pane { border: none; background: transparent; }"
            "QTabBar::tab { padding: 8px 20px; }"
        );

        // Tab 1: IconFont GridView
        m_iconWidget = createIconFontTab(iconFamily);
        m_tabs->addTab(m_iconWidget, "Icon Fonts");

        // Tab 2: Typography
        m_typographyWidget = new TypographyTestWidget(uiFamily, this);
        m_tabs->addTab(m_typographyWidget, "Typography");

        mainLayout->addWidget(m_tabs);

        // 主题切换按钮（固定在右上角）
        m_themeBtn = new Button("Switch Theme", this);
        m_themeBtn->setFixedSize(140, 36);
        m_themeBtn->setFluentStyle(Button::Accent);
        m_themeBtn->move(width() - 160, 10);

        connect(m_themeBtn, &Button::clicked, []() {
            fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == Light ? Dark : Light);
        });

        onThemeUpdated();
    }

    void setCurrentTab(int index) {
        m_tabs->setCurrentIndex(index);
    }

    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        if (m_themeBtn) {
            m_themeBtn->move(width() - 160, 10);
        }
    }

    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1; color: %2;")
            .arg(c.bgCanvas.name()).arg(c.textPrimary.name()));
    }

private:
    QTabWidget* m_tabs;
    QWidget* m_iconWidget;
    TypographyTestWidget* m_typographyWidget;
    Button* m_themeBtn;

    QWidget* createIconFontTab(const QString& iconFamily) {
        auto* widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(30, 30, 30, 30);
        layout->setSpacing(20);

        // 搜索框
        auto* searchEdit = new QLineEdit(widget);
        searchEdit->setPlaceholderText("Search icons...");
        searchEdit->setFixedHeight(35);
        searchEdit->setStyleSheet("padding: 0 10px; border-radius: 4px; border: 1px solid rgba(128,128,128,0.2);");
        layout->addWidget(searchEdit);

        // GridView
        m_iconView = new QListView(widget);
        m_iconView->setViewMode(QListView::IconMode);
        m_iconView->setResizeMode(QListView::Adjust);
        m_iconView->setSpacing(10);
        m_iconView->setFrameShape(QFrame::NoFrame);
        m_iconView->setStyleSheet("background: transparent;");
        
        m_iconModel = new IconModel(widget);
        m_iconView->setModel(m_iconModel);
        
        m_iconDelegate = new IconDelegate(iconFamily, widget);
        m_iconView->setItemDelegate(m_iconDelegate);
        
        layout->addWidget(m_iconView);

        // Fill the same complete catalog used by the Gallery browser.
        // zh_CN: 填充与 Gallery 浏览器相同的完整图标目录。
        QVector<IconData> icons;
        const auto& catalog = Typography::Icons::catalog();
        icons.reserve(catalog.size());
        for (const auto& icon : catalog)
            icons.append({icon.glyph(), icon.name});

        m_iconModel->setIcons(icons);
        connect(searchEdit, &QLineEdit::textChanged, m_iconModel, &IconModel::filter);

        return widget;
    }

    QListView* m_iconView;
    IconModel* m_iconModel;
    IconDelegate* m_iconDelegate;
};

// =============================================================================
// 4. Test Fixture
// =============================================================================

class TypographyTest : public ::testing::Test {};

TEST_F(TypographyTest, BundledTypographyRolesResolveExactStaticFaces) {
    struct ExpectedRole {
        Typography::FontStyle style;
        QString family;
        QString styleName;
        int weight;
    };
    const QList<ExpectedRole> roles = {
        {Typography::Styles::Caption,
         fluent::fontcompat::UITextFamily,
         QStringLiteral("Regular"),
         QFont::Normal},
        {Typography::Styles::Body,
         fluent::fontcompat::UITextFamily,
         QStringLiteral("Regular"),
         QFont::Normal},
        {Typography::Styles::BodyStrong,
         fluent::fontcompat::UITextFamily,
         QStringLiteral("Semibold"),
         QFont::DemiBold},
        {Typography::Styles::Title,
         fluent::fontcompat::UIHeadingFamily,
         QStringLiteral("Semibold"),
         QFont::DemiBold},
        {Typography::Styles::Display,
         fluent::fontcompat::UIDisplayFamily,
         QStringLiteral("Semibold"),
         QFont::DemiBold},
    };

    for (const ExpectedRole& role : roles) {
        const QFont requested = role.style.toQFont();
        const QFontInfo resolved(requested);
        SCOPED_TRACE(role.family.toStdString());
        EXPECT_TRUE(resolved.exactMatch());
        EXPECT_EQ(resolved.family(), role.family);
        EXPECT_EQ(resolved.styleName(), role.styleName);
        EXPECT_EQ(resolved.weight(), role.weight);
        EXPECT_EQ(resolved.pixelSize(), role.style.size);
    }
}

TEST_F(TypographyTest, ApplicationDefaultUsesBundledTextRegular) {
    const QFontInfo resolved(qApp->font());
    EXPECT_EQ(resolved.family(), fluent::fontcompat::UITextFamily);
    EXPECT_EQ(resolved.styleName(), QStringLiteral("Regular"));
    EXPECT_EQ(resolved.weight(), static_cast<int>(QFont::Normal));
}

TEST_F(TypographyTest, BundledIconFaceContainsCompleteCatalogAndSemanticAliases) {
    const auto& catalog = Typography::Icons::catalog();
    ASSERT_EQ(catalog.size(), 9558);
    EXPECT_FALSE(Typography::Icons::glyph(
        QStringLiteral("ic_fluent_add_20_regular")).isEmpty());

    const QFont font = Typography::Icons::font(24);
    const QFontInfo resolved(font);
    EXPECT_TRUE(resolved.exactMatch());
    EXPECT_EQ(resolved.family(), fluent::fontcompat::IconFamily);
    EXPECT_EQ(font.pixelSize(), Typography::IconSize::XLarge);
    EXPECT_EQ(font.hintingPreference(), QFont::PreferVerticalHinting);
    EXPECT_NE(font.styleStrategy() & QFont::PreferQuality, 0);
    EXPECT_NE(font.styleStrategy() & QFont::NoSubpixelAntialias, 0);

    const QFontMetrics metrics(font);
    const QStringList aliasedGlyphs = {
        Typography::Icons::Paste,
        Typography::Icons::PinFill,
        Typography::Icons::Block,
        Typography::Icons::Speaker,
        Typography::Icons::Clock,
        Typography::Icons::Desktop,
        Typography::Icons::AsteriskBadge12,
        Typography::Icons::CheckmarkBadge12,
        Typography::Icons::ErrorBadge12,
        Typography::Icons::ImportantBadge12,
        Typography::Icons::PasswordKeyShow,
        Typography::Icons::PasswordKeyHide,
        Typography::Icons::Snow,
    };
    for (const QString& glyph : aliasedGlyphs) {
        ASSERT_EQ(glyph.size(), 1);
        EXPECT_TRUE(metrics.inFont(glyph.front()));
    }

    const auto supplementary = std::find_if(
        catalog.cbegin(), catalog.cend(),
        [](const Typography::Icons::IconInfo& icon) { return icon.codepoint > 0xFFFF; });
    ASSERT_NE(supplementary, catalog.cend());
    EXPECT_TRUE(metrics.inFontUcs4(supplementary->codepoint));
}

TEST_F(TypographyTest, SemanticIconsResolveToNativeOpticalVariants) {
    const QString add12 = Typography::Icons::glyph(
        QStringLiteral("ic_fluent_add_12_regular"));
    const QString add16 = Typography::Icons::glyph(
        QStringLiteral("ic_fluent_add_16_regular"));
    const QString add20 = Typography::Icons::glyph(
        QStringLiteral("ic_fluent_add_20_regular"));
    ASSERT_FALSE(add12.isEmpty());
    ASSERT_FALSE(add16.isEmpty());
    ASSERT_FALSE(add20.isEmpty());

    EXPECT_EQ(Typography::Icons::glyphForSize(
                  Typography::Icons::Add, Typography::IconSize::Compact),
              add12);
    EXPECT_EQ(Typography::Icons::glyphForSize(
                  Typography::Icons::Add, Typography::IconSize::Standard),
              add16);
    EXPECT_EQ(Typography::Icons::glyphForSize(
                  Typography::Icons::Add, Typography::IconSize::Large),
              add20);

    // Catalog names and catalog glyph values follow the same variant path as
    // the stable semantic aliases.
    EXPECT_EQ(Typography::Icons::glyphForSize(
                  QStringLiteral("ic_fluent_add_20_regular"),
                  Typography::IconSize::Standard),
              add16);
    EXPECT_EQ(Typography::Icons::glyphForSize(add20, Typography::IconSize::Standard),
              add16);
}

TEST_F(TypographyTest, MissingOpticalSizeUsesNearestNativeVariant) {
    const QString more16 = Typography::Icons::glyph(
        QStringLiteral("ic_fluent_more_horizontal_16_regular"));
    ASSERT_FALSE(more16.isEmpty());
    EXPECT_EQ(Typography::Icons::glyphForSize(
                  Typography::Icons::More, Typography::IconSize::Compact),
              more16);
}

TEST_F(TypographyTest, BundledTextFacesRetainHintingTablesAndRenderingPolicy) {
    const QList<Typography::FontStyle> roles = {
        Typography::Styles::Caption,
        Typography::Styles::BodyStrong,
        Typography::Styles::Title,
        Typography::Styles::Display,
    };
    const QList<QByteArray> hintTables = {
        QByteArrayLiteral("cvt "),
        QByteArrayLiteral("fpgm"),
        QByteArrayLiteral("prep"),
        QByteArrayLiteral("gasp"),
    };

    for (const Typography::FontStyle& role : roles) {
        const QFont font = role.toQFont();
#ifdef Q_OS_WIN
        EXPECT_EQ(font.hintingPreference(), QFont::PreferNoHinting);
#else
        EXPECT_EQ(font.hintingPreference(), QFont::PreferVerticalHinting);
#endif
        EXPECT_NE(font.styleStrategy() & QFont::PreferQuality, 0);
        EXPECT_NE(font.styleStrategy() & QFont::NoSubpixelAntialias, 0);

        const QRawFont rawFont = QRawFont::fromFont(font);
        ASSERT_TRUE(rawFont.isValid()) << role.family.toStdString();
        for (const QByteArray& table : hintTables) {
            EXPECT_FALSE(rawFont.fontTable(table.constData()).isEmpty())
                << role.family.toStdString() << " " << table.constData();
        }
    }
}

TEST_F(TypographyTest, RegularAndSemiboldRenderDifferentRealFaces) {
    const auto render = [](QFont font) {
        font.setPixelSize(40);
        QImage image(QSize(420, 80), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setPen(Qt::black);
        painter.setFont(font);
        painter.drawText(QPoint(8, 56), QStringLiteral("Fluent Typography 600"));
        return image;
    };
    const QImage regular = render(Typography::Styles::Body.toQFont());
    const QImage semibold = render(Typography::Styles::BodyStrong.toQFont());
    EXPECT_FALSE(regular == semibold);

    const auto inkCount = [](const QImage& image) {
        int count = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (image.pixelColor(x, y).alpha() > 0)
                    ++count;
            }
        }
        return count;
    };
    EXPECT_GT(inkCount(semibold), inkCount(regular));
}

TEST_F(TypographyTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        GTEST_SKIP();
    }
    TypographyVisualCheckWindow window(Typography::FontFamily::FluentIcons,
                                       Typography::FontFamily::UIText);
    window.show();

    if (tests::support::shouldCaptureVisualSnapshot()) {
        struct SnapshotCase {
            int tabIndex;
            QString variant;
            tests::support::VisualSnapshotTheme theme;
        };
        const QList<SnapshotCase> snapshots = {
            {0, QStringLiteral("icons-light"), tests::support::VisualSnapshotTheme::Light},
            {0, QStringLiteral("icons-dark"), tests::support::VisualSnapshotTheme::Dark},
            {1, QStringLiteral("typography-light"), tests::support::VisualSnapshotTheme::Light},
            {1, QStringLiteral("typography-dark"), tests::support::VisualSnapshotTheme::Dark},
        };

        for (const SnapshotCase& snapshot : snapshots) {
            window.setCurrentTab(snapshot.tabIndex);
            tests::support::VisualSnapshotOptions options;
            options.variant = snapshot.variant;
            options.theme = snapshot.theme;
            ASSERT_TRUE(tests::support::captureVisualSnapshot(&window, options));
        }
        return;
    }

    qApp->exec();
}
