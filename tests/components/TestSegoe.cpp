#include <gtest/gtest.h>
#include <QApplication>
#include <QDebug>
#include <QWidget>
#include <QListView>
#include <QAbstractListModel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QFontDatabase>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTabWidget>
#include <QScrollArea>
#include <QLabel>
#include <QFrame>
#include <functional>
#include "components/foundation/FluentElement.h"
#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"

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
        auto* titleLabel = new QLabel("Segoe UI Variable Typography Showcase", this);
        QFont titleFont(m_uiFamily);
        titleFont.setPixelSize(24);
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        mainLayout->addWidget(titleLabel);

        // 滚动区域
        auto* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
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

        // 4. 原生字体 vs Segoe UI Variable
        addSection(contentLayout, "Native Font vs Segoe UI Variable", [this]() {
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

        // 原生字体（macOS: .AppleSystemUIFont, Windows: Segoe UI）
        auto* nativeLabel = new QLabel("Native System Font:", widget);
        QFont nativeTitleFont(m_uiFamily);
        nativeTitleFont.setPixelSize(14);
        nativeTitleFont.setBold(true);
        nativeLabel->setFont(nativeTitleFont);
        layout->addWidget(nativeLabel);

#ifdef Q_OS_MAC
        QFont nativeFont(".AppleSystemUIFont");
#else
        QFont nativeFont("Segoe UI");
#endif
        nativeFont.setPixelSize(16);
        auto* nativeText = new QLabel(sampleText, widget);
        nativeText->setFont(nativeFont);
        layout->addWidget(nativeText);
        layout->addSpacing(10);

        // Segoe UI Variable（加载的字体）
        auto* segoeLabel = new QLabel("Segoe UI Variable (Loaded):", widget);
        QFont segoeTitleFont(m_uiFamily);
        segoeTitleFont.setPixelSize(14);
        segoeTitleFont.setBold(true);
        segoeLabel->setFont(segoeTitleFont);
        layout->addWidget(segoeLabel);

        QFont segoeFont(m_uiFamily);
        segoeFont.setPixelSize(16);
        auto* segoeText = new QLabel(sampleText, widget);
        segoeText->setFont(segoeFont);
        layout->addWidget(segoeText);

        return widget;
    }
};

// =============================================================================
// 3. 主测试窗口（Tab 组织）
// =============================================================================

class SegoeTestWindow : public QWidget, public fluent::FluentElement {
public:
    SegoeTestWindow(const QString& iconFamily, const QString& uiFamily) {
        setFixedSize(1200, 800);
        setWindowTitle("Segoe Font & Icon Test Suite");

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

        // 填充图标数据
        QVector<IconData> icons;
        auto add = [&](const QString& g, const QString& n) { icons.append({g, n}); };
        
        add(Typography::Icons::GlobalNav, "GlobalNav");
        add(Typography::Icons::ChevronDown, "ChevronDown");
        add(Typography::Icons::ChevronUp, "ChevronUp");
        add(Typography::Icons::ChevronLeft, "ChevronLeft");
        add(Typography::Icons::ChevronRight, "ChevronRight");
        add(Typography::Icons::Back, "Back");
        add(Typography::Icons::Forward, "Forward");
        add(Typography::Icons::Home, "Home");
        add(Typography::Icons::FullScreen, "FullScreen");
        add(Typography::Icons::Add, "Add");
        add(Typography::Icons::Cancel, "Cancel");
        add(Typography::Icons::Delete, "Delete");
        add(Typography::Icons::Save, "Save");
        add(Typography::Icons::Search, "Search");
        add(Typography::Icons::Settings, "Settings");
        add(Typography::Icons::Edit, "Edit");
        add(Typography::Icons::Undo, "Undo");
        add(Typography::Icons::Redo, "Redo");
        add(Typography::Icons::Copy, "Copy");
        add(Typography::Icons::Paste, "Paste");
        add(Typography::Icons::FavoriteStar, "FavoriteStar");
        add(Typography::Icons::PinFill, "PinFill");
        add(Typography::Icons::Play, "Play");
        add(Typography::Icons::Pause, "Pause");
        add(Typography::Icons::Volume, "Volume");
        add(Typography::Icons::Music, "Music");
        add(Typography::Icons::Headphones, "Headphones");
        add(Typography::Icons::SkipBack, "SkipBack");
        add(Typography::Icons::SkipForward, "SkipForward");
        add(Typography::Icons::Accounts, "Accounts");
        add(Typography::Icons::ContactInfo, "ContactInfo");
        add(Typography::Icons::Wifi, "Wifi");
        add(Typography::Icons::Battery, "Battery");
        add(Typography::Icons::Power, "Power");
        add(Typography::Icons::Brightness, "Brightness");
        add(Typography::Icons::History, "History");
        add(Typography::Icons::Warning, "Warning");
        add(Typography::Icons::ErrorIcon, "ErrorIcon");
        add(Typography::Icons::Success, "Success");
        add(Typography::Icons::HeartFill, "HeartFill");
        add(Typography::Icons::Lock, "Lock");
        add(Typography::Icons::Unlock, "Unlock");
        add(Typography::Icons::Sunny, "Sunny");
        add(Typography::Icons::Rain, "Rain");
        add(Typography::Icons::Snow, "Snow");

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

class SegoeTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        Q_INIT_RESOURCE(resources);

        auto loadAndRegister = [](const QString& path, const QString& fallback) -> QString {
            int id = QFontDatabase::addApplicationFont(path);
            if (id != -1) {
                QStringList families = QFontDatabase::applicationFontFamilies(id);
                if (!families.isEmpty()) {
                    qDebug() << "Loaded Font:" << path << "Families:" << families;
                    return families.first();
                }
            }
            return fallback;
        };

        m_iconFamily = loadAndRegister(":/res/Segoe Fluent Icons.ttf", "Segoe Fluent Icons");
        m_uiFamily = loadAndRegister(":/res/SegoeUI-VF.ttf", "Segoe UI");

        if (!m_uiFamily.isEmpty() && m_uiFamily != "Segoe UI") {
            QFont::insertSubstitution("Segoe UI", m_uiFamily);
            QFont::insertSubstitution("Segoe UI Semibold", m_uiFamily);
            QFont::insertSubstitution("Segoe UI Bold", m_uiFamily);
            qDebug() << "Font mapping: 'Segoe UI' ->" << m_uiFamily;
        }
    }
    static QString m_iconFamily;
    static QString m_uiFamily;
};

QString SegoeTest::m_iconFamily = "";
QString SegoeTest::m_uiFamily = "";

TEST_F(SegoeTest, ComprehensiveCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        GTEST_SKIP();
    }
    SegoeTestWindow window(m_iconFamily, m_uiFamily);
    window.show();
    qApp->exec();
}
