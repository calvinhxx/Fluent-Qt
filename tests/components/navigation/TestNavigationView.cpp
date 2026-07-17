#include <gtest/gtest.h>

#include <QApplication>
#include <QBoxLayout>
#include <QEvent>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QSignalSpy>
#include <QTest>
#include <functional>

#include "design/Typography.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/basicinput/Button.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/StackContentHost.h"
#include "components/textfields/Label.h"
#include "components/windowing/WindowBackdrop.h"

using fluent::AnchorLayout;
using fluent::basicinput::Button;
using fluent::navigation::NavigationView;
using fluent::navigation::StackContentHost;
using fluent::textfields::Label;

namespace {

void publishCompositedBackdrop(QWidget* window)
{
    fluent::windowing::BackdropState state;
    state.requestedEffect = fluent::windowing::BackdropEffect::Mica;
    state.effectiveEffect = fluent::windowing::BackdropEffect::Mica;
    state.backend = fluent::windowing::BackdropBackend::DwmSystemBackdrop;
    state.fidelity = fluent::windowing::BackdropFidelity::Native;
    state.surfaceMode = fluent::windowing::BackdropSurfaceMode::CompositedTransparent;
    state.platformApplied = true;
    fluent::windowing::publishWindowBackdropState(window, state);
}

class NavigationViewTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, themeColors().bgCanvas);
        setPalette(pal);
        setAutoFillBackground(true);
    }
};

class NavigationItemRow : public QWidget, public fluent::FluentElement {
public:
    NavigationItemRow(const QString& iconGlyph, const QString& text, bool selected, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_iconGlyph(iconGlyph)
        , m_text(text)
        , m_selected(selected)
    {
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(40);
    }

    QSize sizeHint() const override
    {
        if (m_compact)
            return QSize(48, 40);
        const QFontMetrics metrics(themeFont(QStringLiteral("Body")).toQFont());
        return QSize(qMax(84, metrics.horizontalAdvance(m_text) + 68), 40);
    }

    void setSelected(bool selected)
    {
        if (m_selected == selected)
            return;
        m_selected = selected;
        update();
    }

    void setCompact(bool compact)
    {
        if (m_compact == compact)
            return;
        m_compact = compact;
        updateGeometry();
        update();
    }

    bool selectedIndicatorVisible() const
    {
        return m_selected && !m_compact;
    }

    std::function<void()> onActivated;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const auto& colors = themeColors();
        const auto& radius = themeRadius();
        const QRectF itemRect = rect().adjusted(4, 2, -4, -2);

        QColor fill = Qt::transparent;
        if (m_pressed)
            fill = colors.subtleTertiary;
        else if (m_selected || m_hovered)
            fill = colors.subtleSecondary;

        if (fill.alpha() > 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawRoundedRect(itemRect, radius.control, radius.control);
        }

        if (selectedIndicatorVisible()) {
            const QRectF indicator(itemRect.left() + 4,
                                   itemRect.center().y() - 8,
                                   3,
                                   16);
            painter.setBrush(colors.accentDefault);
            painter.drawRoundedRect(indicator, 1.5, 1.5);
        }

        const int iconX = m_compact ? (width() - 20) / 2 : 22;
        const QRect iconRect(iconX, 0, 20, height());
        QFont iconFont(Typography::FontFamily::FluentIcons);
        iconFont.setPixelSize(16);
        painter.setFont(iconFont);
        painter.setPen(m_selected ? colors.textPrimary : colors.textSecondary);
        painter.drawText(iconRect, Qt::AlignCenter, m_iconGlyph);

        if (!m_compact) {
            QFont textFont = themeFont(m_selected ? QStringLiteral("BodyStrong") : QStringLiteral("Body")).toQFont();
            painter.setFont(textFont);
            painter.setPen(colors.textPrimary);
            painter.drawText(QRect(52, 0, qMax(0, width() - 64), height()),
                             Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                             m_text);
        }
    }

    void enterEvent(FluentEnterEvent* event) override
    {
        QWidget::enterEvent(event);
        m_hovered = true;
        update();
    }

    void leaveEvent(QEvent* event) override
    {
        QWidget::leaveEvent(event);
        m_hovered = false;
        m_pressed = false;
        update();
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressed = true;
            update();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        const bool activate = m_pressed && rect().contains(event->pos());
        m_pressed = false;
        update();
        if (activate && onActivated) {
            onActivated();
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

private:
    QString m_iconGlyph;
    QString m_text;
    bool m_selected = false;
    bool m_compact = false;
    bool m_hovered = false;
    bool m_pressed = false;
};

class NavigationPaneSection : public QWidget, public fluent::FluentElement {
public:
    struct Entry {
        QString icon;
        QString text;
        bool selected = false;
    };

    NavigationPaneSection(Qt::Orientation orientation, const QVector<Entry>& entries, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_layout(new QBoxLayout(QBoxLayout::TopToBottom, this))
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_layout->setContentsMargins(8, 8, 8, 8);
        m_layout->setSpacing(4);
        for (const Entry& entry : entries) {
            const int rowIndex = m_rows.size();
            auto* row = new NavigationItemRow(entry.icon, entry.text, entry.selected, this);
            row->onActivated = [this, rowIndex]() {
                setSelectedIndex(rowIndex);
                if (onActivated)
                    onActivated(rowIndex);
            };
            m_rows.append(row);
            m_layout->addWidget(row);
        }
        m_layout->addStretch();
        setLayout(m_layout);
        setOrientation(orientation);
    }

    QSize sizeHint() const override
    {
        if (m_orientation == Qt::Horizontal) {
            int widthValue = 16;
            for (NavigationItemRow* row : m_rows)
                widthValue += row->sizeHint().width() + 4;
            return QSize(widthValue, 48);
        }
        return QSize(280, m_preferredVerticalHeight > 0 ? m_preferredVerticalHeight : 16 + m_rows.size() * 44);
    }

    QSize minimumSizeHint() const override
    {
        if (m_orientation == Qt::Horizontal)
            return QSize(0, 44);
        return QSize(48, 16 + m_rows.size() * 40);
    }

    void setOrientation(Qt::Orientation orientation)
    {
        if (m_orientation == orientation) {
            updateGeometry();
            return;
        }
        m_orientation = orientation;
        m_layout->setDirection(orientation == Qt::Vertical
                                   ? QBoxLayout::TopToBottom
                                   : QBoxLayout::LeftToRight);
        m_layout->setContentsMargins(8, orientation == Qt::Vertical ? 8 : 4, 8, orientation == Qt::Vertical ? 8 : 4);
        for (NavigationItemRow* row : m_rows) {
            row->setCompact(m_compact);
            row->setFixedHeight(orientation == Qt::Vertical ? 40 : 36);
        }
        m_layout->invalidate();
        m_layout->activate();
        updateGeometry();
    }

    void setPreferredVerticalHeight(int height)
    {
        if (m_preferredVerticalHeight == height)
            return;
        m_preferredVerticalHeight = height;
        updateGeometry();
    }

    void setCompact(bool compact)
    {
        m_compact = compact;
        for (NavigationItemRow* row : m_rows)
            row->setCompact(compact);
        updateGeometry();
    }

    void setSelectedIndex(int index)
    {
        for (int i = 0; i < m_rows.size(); ++i)
            m_rows.at(i)->setSelected(i == index);
    }

    void clearSelection()
    {
        setSelectedIndex(-1);
    }

    void onThemeUpdated() override
    {
        for (NavigationItemRow* row : m_rows)
            row->update();
        update();
    }

    std::function<void(int)> onActivated;

private:
    Qt::Orientation m_orientation = Qt::Vertical;
    int m_preferredVerticalHeight = 0;
    bool m_compact = false;
    QBoxLayout* m_layout = nullptr;
    QVector<NavigationItemRow*> m_rows;
};

class DashboardPage : public QWidget, public fluent::FluentElement {
public:
    explicit DashboardPage(QWidget* parent = nullptr)
        : DashboardPage(QStringLiteral("Home"),
                        QStringLiteral("Shell content supplied by the application layer"),
                        {
                            QStringLiteral("Navigation shell switched to lightweight composition"),
                            QStringLiteral("Documents page opened from the side pane"),
                            QStringLiteral("Settings footer item is available as app chrome")
                        },
                        parent)
    {
    }

    DashboardPage(const QString& title, const QString& subtitle, const QStringList& activityRows, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_title(title)
        , m_subtitle(subtitle)
        , m_activityRows(activityRows)
    {
        setAutoFillBackground(false);
    }

    void onThemeUpdated() override
    {
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const auto& colors = themeColors();
        const int pad = 32;
        painter.fillRect(rect(), colors.bgLayer);

        painter.setPen(colors.textPrimary);
        painter.setFont(themeFont(QStringLiteral("Title")).toQFont());
        painter.drawText(QRect(pad, 28, width() - pad * 2, 42),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         m_title);

        painter.setPen(colors.textSecondary);
        painter.setFont(themeFont(QStringLiteral("Body")).toQFont());
        painter.drawText(QRect(pad, 68, width() - pad * 2, 26),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         m_subtitle);

        const int gap = 16;
        const int cardTop = 116;
        const int cardHeight = 112;
        const int cardWidth = qMax(160, (width() - pad * 2 - gap * 2) / 3);
        drawCard(painter, QRect(pad, cardTop, cardWidth, cardHeight),
                 QStringLiteral("Active projects"), QStringLiteral("24"), colors.systemInfo);
        drawCard(painter, QRect(pad + cardWidth + gap, cardTop, cardWidth, cardHeight),
                 QStringLiteral("Pinned files"), QStringLiteral("8"), colors.systemSuccess);
        drawCard(painter, QRect(pad + (cardWidth + gap) * 2, cardTop, cardWidth, cardHeight),
                 QStringLiteral("Sync status"), QStringLiteral("Ready"), colors.accentDefault);

        const QRect panel(pad, cardTop + cardHeight + 24, width() - pad * 2, qMax(180, height() - cardTop - cardHeight - 64));
        painter.setPen(colors.strokeCard);
        painter.setBrush(colors.bgCanvas);
        painter.drawRoundedRect(panel, themeRadius().overlay, themeRadius().overlay);

        painter.setPen(colors.textPrimary);
        painter.setFont(themeFont(QStringLiteral("BodyStrong")).toQFont());
        painter.drawText(panel.adjusted(20, 16, -20, -panel.height() + 48),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("Recent activity"));

        painter.setFont(themeFont(QStringLiteral("Body")).toQFont());
        int y = panel.top() + 64;
        for (const QString& row : m_activityRows) {
            const QRect rowRect(panel.left() + 16, y, panel.width() - 32, 40);
            painter.setPen(colors.strokeDivider);
            painter.drawLine(rowRect.topLeft(), rowRect.topRight());
            painter.setPen(colors.textPrimary);
            painter.drawText(rowRect.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, row);
            y += 44;
        }
    }

private:
    QString m_title;
    QString m_subtitle;
    QStringList m_activityRows;

    void drawCard(QPainter& painter, const QRect& rect, const QString& title, const QString& value, const QColor& accent)
    {
        const auto& colors = themeColors();
        painter.setPen(colors.strokeCard);
        painter.setBrush(colors.bgCanvas);
        painter.drawRoundedRect(rect, themeRadius().overlay, themeRadius().overlay);

        QRect indicator(rect.left() + 16, rect.top() + 18, 4, 36);
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawRoundedRect(indicator, 2, 2);

        painter.setPen(colors.textSecondary);
        painter.setFont(themeFont(QStringLiteral("Caption")).toQFont());
        painter.drawText(rect.adjusted(32, 18, -16, -rect.height() + 44), Qt::AlignLeft | Qt::AlignVCenter, title);

        painter.setPen(colors.textPrimary);
        painter.setFont(themeFont(QStringLiteral("Subtitle")).toQFont());
        painter.drawText(rect.adjusted(32, 46, -16, -16), Qt::AlignLeft | Qt::AlignVCenter, value);
    }
};

class PaintTrackingStackContentHost final : public StackContentHost {
public:
    int paintCount() const { return m_paintCount; }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        ++m_paintCount;
        StackContentHost::paintEvent(event);
    }

private:
    int m_paintCount = 0;
};

void showAndProcess(QWidget& widget)
{
    widget.show();
    QApplication::processEvents();
}

void addAnchored(AnchorLayout* layout, QWidget* widget)
{
    layout->addWidget(widget);
}

QWidget* fixedWidget(const QSize& size)
{
    auto* widget = new QWidget();
    widget->setMinimumSize(size);
    widget->resize(size);
    return widget;
}

} // namespace

class NavigationViewTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<fluent::navigation::NavigationView::DisplayMode>(
            "fluent::navigation::NavigationView::DisplayMode");
        qRegisterMetaType<fluent::navigation::StackContentHost::TransitionEffect>(
            "fluent::navigation::StackContentHost::TransitionEffect");
    }

    void SetUp() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
        window = new NavigationViewTestWindow();
        window->resize(920, 620);
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated();
    }

    void TearDown() override
    {
        delete window;
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    NavigationViewTestWindow* window = nullptr;
    AnchorLayout* layout = nullptr;
};

TEST_F(NavigationViewTest, DefaultsAndPropertiesMatchShellContainer)
{
    NavigationView nav;

    EXPECT_EQ(nav.displayMode(), NavigationView::DisplayMode::Auto);
    EXPECT_TRUE(nav.isPaneOpen());
    EXPECT_EQ(nav.expandedPaneWidth(), 320);
    EXPECT_EQ(nav.compactPaneWidth(), 48);
    EXPECT_EQ(nav.topBarHeight(), 48);
    EXPECT_TRUE(nav.isAnimationEnabled());
    EXPECT_NE(nav.contentHost(), nullptr);
    EXPECT_EQ(nav.accessibleName(), QStringLiteral("NavigationView"));
    EXPECT_FALSE(nav.sizeHint().isEmpty());
    EXPECT_FALSE(nav.minimumSizeHint().isEmpty());
    EXPECT_NE(dynamic_cast<QWidget*>(&nav), nullptr);
    EXPECT_NE(dynamic_cast<fluent::FluentElement*>(&nav), nullptr);
    EXPECT_NE(dynamic_cast<fluent::QMLPlus*>(&nav), nullptr);

    QSignalSpy displaySpy(&nav, &NavigationView::displayModeChanged);
    QSignalSpy openSpy(&nav, &NavigationView::paneOpenChanged);
    QSignalSpy compactSpy(&nav, &NavigationView::compactModeThresholdWidthChanged);
    QSignalSpy expandedSpy(&nav, &NavigationView::expandedModeThresholdWidthChanged);
    QSignalSpy expandedPaneSpy(&nav, &NavigationView::expandedPaneWidthChanged);
    QSignalSpy compactPaneSpy(&nav, &NavigationView::compactPaneWidthChanged);
    QSignalSpy topBarSpy(&nav, &NavigationView::topBarHeightChanged);
    QSignalSpy animationSpy(&nav, &NavigationView::animationEnabledChanged);

    nav.setDisplayMode(NavigationView::DisplayMode::Top);
    nav.setDisplayMode(NavigationView::DisplayMode::Top);
    nav.setPaneOpen(false);
    nav.setPaneOpen(false);
    nav.setCompactModeThresholdWidth(500);
    nav.setCompactModeThresholdWidth(500);
    nav.setExpandedModeThresholdWidth(900);
    nav.setExpandedModeThresholdWidth(900);
    nav.setExpandedPaneWidth(280);
    nav.setExpandedPaneWidth(280);
    nav.setCompactPaneWidth(56);
    nav.setCompactPaneWidth(56);
    nav.setTopBarHeight(56);
    nav.setTopBarHeight(56);
    nav.setAnimationEnabled(false);
    nav.setAnimationEnabled(false);

    EXPECT_EQ(displaySpy.count(), 1);
    EXPECT_EQ(openSpy.count(), 1);
    EXPECT_EQ(compactSpy.count(), 1);
    EXPECT_EQ(expandedSpy.count(), 1);
    EXPECT_EQ(expandedPaneSpy.count(), 1);
    EXPECT_EQ(compactPaneSpy.count(), 1);
    EXPECT_EQ(topBarSpy.count(), 1);
    EXPECT_EQ(animationSpy.count(), 1);
}

TEST_F(NavigationViewTest, SideChromeSlotsUseHeaderMainFooterVerticalStructure)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* header = fixedWidget(QSize(180, 72));
    auto* main = fixedWidget(QSize(180, 240));
    auto* footer = fixedWidget(QSize(180, 96));
    nav.setHeaderChromeWidget(header);
    nav.setMainChromeWidget(main);
    nav.setFooterChromeWidget(footer);

    auto* firstPage = new QWidget();
    ASSERT_TRUE(nav.contentHost()->insertPage(0, firstPage));
    nav.contentHost()->setCurrentIndex(0, 0, false);

    nav.setDisplayMode(NavigationView::DisplayMode::Left);
    nav.resize(900, 600);
    showAndProcess(nav);

    EXPECT_EQ(nav.effectiveDisplayMode(), NavigationView::DisplayMode::Left);
    EXPECT_EQ(nav.chromeGeometry(), QRect(0, 0, 320, 600));
    EXPECT_EQ(nav.paneGeometry(), nav.chromeGeometry());
    EXPECT_EQ(nav.contentGeometry(), QRect(320, 0, 580, 600));
    EXPECT_EQ(nav.headerChromeGeometry(), QRect(0, 0, 320, 72));
    EXPECT_EQ(nav.mainChromeGeometry(), QRect(0, 72, 320, 432));
    EXPECT_EQ(nav.footerChromeGeometry(), QRect(0, 504, 320, 96));
    EXPECT_EQ(header->geometry(), nav.headerChromeGeometry());
    EXPECT_EQ(main->geometry(), nav.mainChromeGeometry());
    EXPECT_EQ(footer->geometry(), nav.footerChromeGeometry());
    EXPECT_EQ(nav.contentHost()->geometry(), nav.contentGeometry());
    EXPECT_EQ(firstPage->parentWidget(), nav.contentHost());
}

TEST_F(NavigationViewTest, TopChromeSlotsUseHorizontalStructureWithFooterAlignedRight)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* header = fixedWidget(QSize(140, 40));
    auto* main = fixedWidget(QSize(260, 40));
    auto* footer = fixedWidget(QSize(120, 40));
    nav.setHeaderChromeWidget(header);
    nav.setMainChromeWidget(main);
    nav.setFooterChromeWidget(footer);
    nav.setDisplayMode(NavigationView::DisplayMode::Top);
    nav.resize(900, 500);
    showAndProcess(nav);

    EXPECT_EQ(nav.effectiveDisplayMode(), NavigationView::DisplayMode::Top);
    EXPECT_EQ(nav.topBarGeometry(), QRect(0, 0, 900, 48));
    EXPECT_EQ(nav.contentGeometry(), QRect(0, 48, 900, 452));
    EXPECT_EQ(nav.headerChromeGeometry(), QRect(0, 0, 140, 48));
    EXPECT_EQ(nav.mainChromeGeometry(), QRect(140, 0, 260, 48));
    EXPECT_EQ(nav.footerChromeGeometry(), QRect(780, 0, 120, 48));
    EXPECT_EQ(nav.contentHost()->geometry(), nav.contentGeometry());
}

TEST_F(NavigationViewTest, AutoCompactAndMinimalModesOnlyAffectShellGeometry)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* main = fixedWidget(QSize(80, 200));
    nav.setMainChromeWidget(main);
    nav.resize(1100, 500);
    showAndProcess(nav);

    EXPECT_EQ(nav.effectiveDisplayMode(), NavigationView::DisplayMode::Left);
    EXPECT_EQ(nav.chromeGeometry().width(), 320);
    EXPECT_EQ(nav.contentGeometry().left(), 320);

    nav.resize(700, 500);
    QApplication::processEvents();
    EXPECT_EQ(nav.effectiveDisplayMode(), NavigationView::DisplayMode::LeftCompact);
    nav.setPaneOpen(false);
    QApplication::processEvents();
    EXPECT_EQ(nav.chromeGeometry().width(), 48);
    EXPECT_EQ(nav.contentGeometry().left(), 48);

    nav.resize(420, 500);
    QApplication::processEvents();
    EXPECT_EQ(nav.effectiveDisplayMode(), NavigationView::DisplayMode::LeftMinimal);
    nav.setPaneOpen(false);
    QApplication::processEvents();
    EXPECT_TRUE(nav.chromeGeometry().isEmpty());
    EXPECT_EQ(nav.contentGeometry().left(), 0);
    nav.setPaneOpen(true);
    QApplication::processEvents();
    EXPECT_EQ(nav.chromeGeometry().width(), 320);
    EXPECT_EQ(nav.contentGeometry().left(), 0);
}

TEST_F(NavigationViewTest, SideModeTransitionsOnlyAnimateHorizontalGeometry)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* header = fixedWidget(QSize(1, 72));
    auto* main = fixedWidget(QSize(1, 240));
    auto* footer = fixedWidget(QSize(1, 96));
    nav.setHeaderChromeWidget(header);
    nav.setMainChromeWidget(main);
    nav.setFooterChromeWidget(footer);
    auto* page = new QWidget();
    ASSERT_TRUE(nav.contentHost()->insertPage(0, page));
    nav.contentHost()->setCurrentIndex(0, 0, false);

    nav.setDisplayMode(NavigationView::DisplayMode::LeftCompact);
    nav.setPaneOpen(false);
    nav.resize(900, 600);
    window->show();
    showAndProcess(nav);
    nav.setAnimationEnabled(true);

    const QRect compactHeader = header->geometry();
    const QRect compactMain = main->geometry();
    const QRect compactFooter = footer->geometry();
    const QRect compactContent = nav.contentHost()->geometry();

    nav.setDisplayMode(NavigationView::DisplayMode::LeftMinimal);
    QApplication::processEvents();
    ASSERT_TRUE(nav.setProperty("layoutTransitionProgress", 0.5));
    QApplication::processEvents();

    const auto expectVerticalStable = [](const QRect& actual, const QRect& baseline) {
        EXPECT_EQ(actual.y(), baseline.y());
        EXPECT_EQ(actual.height(), baseline.height());
    };
    expectVerticalStable(header->geometry(), compactHeader);
    expectVerticalStable(main->geometry(), compactMain);
    expectVerticalStable(footer->geometry(), compactFooter);
    expectVerticalStable(nav.contentHost()->geometry(), compactContent);

    EXPECT_GT(header->geometry().width(), 0);
    EXPECT_LT(header->geometry().width(), compactHeader.width());
    EXPECT_LT(nav.contentHost()->geometry().left(), compactContent.left());
    EXPECT_EQ(nav.contentHost()->geometry().right(), compactContent.right());
}

TEST_F(NavigationViewTest, TopSideTransitionRevealsChromeWithoutMorphingIt)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* header = fixedWidget(QSize(180, 40));
    auto* main = fixedWidget(QSize(240, 40));
    auto* footer = fixedWidget(QSize(96, 40));
    nav.setHeaderChromeWidget(header);
    nav.setMainChromeWidget(main);
    nav.setFooterChromeWidget(footer);
    auto* page = new QWidget();
    ASSERT_TRUE(nav.contentHost()->insertPage(0, page));
    nav.contentHost()->setCurrentIndex(0, 0, false);

    nav.setDisplayMode(NavigationView::DisplayMode::Top);
    nav.resize(900, 600);
    window->show();
    showAndProcess(nav);
    nav.setAnimationEnabled(true);

    nav.setDisplayMode(NavigationView::DisplayMode::Left);
    QApplication::processEvents();
    ASSERT_TRUE(nav.setProperty("layoutTransitionProgress", 0.5));
    QApplication::processEvents();

    EXPECT_EQ(header->geometry(), QRect(-160, 0, 320, 40));
    EXPECT_EQ(main->geometry(), QRect(-160, 40, 320, 520));
    EXPECT_EQ(footer->geometry(), QRect(-160, 560, 320, 40));
    EXPECT_EQ(nav.contentHost()->geometry(), QRect(160, 24, 740, 576));

    ASSERT_TRUE(nav.setProperty("layoutTransitionProgress", 1.0));
    QApplication::processEvents();
    nav.setAnimationEnabled(false);
    nav.setAnimationEnabled(true);

    nav.setDisplayMode(NavigationView::DisplayMode::Top);
    QApplication::processEvents();
    ASSERT_TRUE(nav.setProperty("layoutTransitionProgress", 0.5));
    QApplication::processEvents();

    EXPECT_EQ(header->geometry(), QRect(0, -24, 180, 48));
    EXPECT_EQ(main->geometry(), QRect(180, -24, 240, 48));
    EXPECT_EQ(footer->geometry(), QRect(804, -24, 96, 48));
    EXPECT_EQ(nav.contentHost()->geometry(), QRect(160, 24, 740, 576));
}

TEST_F(NavigationViewTest, ConfigurableMetricsAndSlotReplacementUpdateGeometry)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* header = fixedWidget(QSize(100, 40));
    auto* replacement = fixedWidget(QSize(100, 64));
    nav.setHeaderChromeWidget(header);
    nav.setDisplayMode(NavigationView::DisplayMode::Left);
    nav.setExpandedPaneWidth(280);
    nav.setCompactPaneWidth(64);
    nav.resize(900, 500);
    showAndProcess(nav);

    EXPECT_EQ(nav.chromeGeometry().width(), 280);
    EXPECT_EQ(nav.contentGeometry().left(), 280);
    EXPECT_EQ(nav.headerChromeGeometry().height(), 40);

    nav.setHeaderChromeWidget(replacement);
    QApplication::processEvents();
    EXPECT_EQ(nav.headerChromeWidget(), replacement);
    EXPECT_EQ(replacement->parentWidget(), &nav);
    EXPECT_EQ(header->parentWidget(), nullptr);
    EXPECT_EQ(nav.headerChromeGeometry().height(), 64);

    nav.setDisplayMode(NavigationView::DisplayMode::LeftCompact);
    nav.setPaneOpen(false);
    QApplication::processEvents();
    EXPECT_EQ(nav.chromeGeometry().width(), 64);
    EXPECT_EQ(nav.contentGeometry().left(), 64);

    nav.setDisplayMode(NavigationView::DisplayMode::Top);
    nav.setTopBarHeight(56);
    QApplication::processEvents();
    EXPECT_EQ(nav.topBarGeometry().height(), 56);
    EXPECT_EQ(nav.contentGeometry().top(), 56);
}

TEST_F(NavigationViewTest, StackContentHostOwnsContentPagesAtApplicationLayer)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* first = new Label(QStringLiteral("First"));
    auto* second = new Label(QStringLiteral("Second"));
    ASSERT_TRUE(nav.contentHost()->insertPage(0, first));
    ASSERT_TRUE(nav.contentHost()->insertPage(1, second));
    nav.resize(760, 420);
    showAndProcess(nav);

    QSignalSpy currentSpy(nav.contentHost(), &StackContentHost::currentIndexChanged);
    nav.contentHost()->setCurrentIndex(0, 0, false);
    EXPECT_EQ(nav.contentHost()->currentIndex(), 0);
    EXPECT_EQ(first->parentWidget(), nav.contentHost());
    EXPECT_EQ(first->geometry(), QRect(QPoint(0, 0), nav.contentGeometry().size()));

    nav.contentHost()->setCurrentIndex(1, 1, false);
    EXPECT_EQ(nav.contentHost()->currentIndex(), 1);
    EXPECT_EQ(second->geometry(), QRect(QPoint(0, 0), nav.contentGeometry().size()));
    EXPECT_EQ(currentSpy.count(), 2);
}

TEST_F(NavigationViewTest, StackContentHostClearsTranslucentBackdropPixels)
{
    StackContentHost host;
    host.setAttribute(Qt::WA_TranslucentBackground, true);
    publishCompositedBackdrop(&host);
    host.resize(240, 160);

    QImage image(host.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 255, 255));

    QPainter painter(&image);
    host.render(&painter, QPoint(), QRegion(), QWidget::DrawWindowBackground);
    painter.end();

    EXPECT_EQ(image.pixelColor(host.rect().center()).alpha(), 0)
        << "A translucent content host must replace pixels left by the outgoing page";
}

TEST_F(NavigationViewTest, StackContentHostCoalescesBackdropClearAcrossRapidSwitches)
{
    PaintTrackingStackContentHost host;
    host.setAttribute(Qt::WA_TranslucentBackground, true);
    publishCompositedBackdrop(&host);
    host.resize(240, 160);

    auto* first = new QWidget;
    auto* second = new QWidget;
    ASSERT_TRUE(host.insertPage(0, first));
    ASSERT_TRUE(host.insertPage(1, second));
    host.setCurrentIndex(0, 0, false);
    showAndProcess(host);

    const int paintsBeforeSwitch = host.paintCount();
    for (int i = 0; i < 100; ++i)
        host.setCurrentIndex(i % 2, 0, false);

    EXPECT_EQ(host.paintCount(), paintsBeforeSwitch)
        << "Rapid navigation must not synchronously re-enter backing-store painting";
    EXPECT_FALSE(host.busy());
    QApplication::processEvents();
    EXPECT_GT(host.paintCount(), paintsBeforeSwitch)
        << "The coalesced frame must still replace the composited backdrop";
    EXPECT_EQ(host.currentIndex(), 1);
    EXPECT_TRUE(second->isVisible());
    EXPECT_FALSE(first->isVisible());
}

TEST_F(NavigationViewTest, StackContentHostTransitionEffectControlsIncomingOffset)
{
    StackContentHost host;
    host.resize(420, 320);

    auto* first = new Label(QStringLiteral("First"));
    auto* second = new Label(QStringLiteral("Second"));
    ASSERT_TRUE(host.insertPage(0, first));
    ASSERT_TRUE(host.insertPage(1, second));
    host.setCurrentIndex(0, 0, false);
    showAndProcess(host);

    EXPECT_EQ(host.transitionEffect(), StackContentHost::TransitionEffect::SlideFromLeft);

    QSignalSpy effectSpy(&host, &StackContentHost::transitionEffectChanged);
    host.setTransitionEffect(StackContentHost::TransitionEffect::SlideFromBottom);
    host.setTransitionEffect(StackContentHost::TransitionEffect::SlideFromBottom);
    EXPECT_EQ(effectSpy.count(), 1);

    host.setCurrentIndex(1, 0, true);
    EXPECT_EQ(host.currentIndex(), 1);
    EXPECT_TRUE(host.busy());
    EXPECT_TRUE(second->isVisible());
    EXPECT_EQ(second->geometry().size(), host.rect().size());
    EXPECT_EQ(second->geometry().left(), 0);
    EXPECT_GT(second->geometry().top(), 0);
    EXPECT_LT(second->geometry().top(), host.height());

    QTRY_VERIFY_WITH_TIMEOUT(!host.busy(), 1000);
    EXPECT_EQ(second->geometry(), host.rect());
}

TEST_F(NavigationViewTest, ThemeRefreshKeepsShellAndSlotsStable)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* header = fixedWidget(QSize(120, 48));
    auto* main = fixedWidget(QSize(120, 160));
    nav.setHeaderChromeWidget(header);
    nav.setMainChromeWidget(main);
    nav.resize(900, 500);
    showAndProcess(nav);

    const QRect chromeRect = nav.chromeGeometry();
    const QRect contentRect = nav.contentGeometry();

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    nav.onThemeUpdated();
    QApplication::processEvents();

    EXPECT_EQ(nav.chromeGeometry(), chromeRect);
    EXPECT_EQ(nav.contentGeometry(), contentRect);
    EXPECT_EQ(header->geometry(), nav.headerChromeGeometry());
    EXPECT_EQ(main->geometry(), nav.mainChromeGeometry());
}

TEST_F(NavigationViewTest, ChromeSizeHintChangesRelayoutAfterLeftToTopSwitch)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* header = new NavigationPaneSection(Qt::Vertical, {
        {Typography::Icons::Back, QStringLiteral("Back"), false},
        {Typography::Icons::Search, QStringLiteral("Search"), false},
    });
    auto* main = new NavigationPaneSection(Qt::Vertical, {
        {Typography::Icons::Home, QStringLiteral("Home"), true},
        {Typography::Icons::Contact, QStringLiteral("Account"), false},
    });
    auto* footer = new NavigationPaneSection(Qt::Vertical, {
        {Typography::Icons::Info, QStringLiteral("Help"), false},
        {Typography::Icons::Settings, QStringLiteral("Settings"), false},
    });
    header->setPreferredVerticalHeight(88);
    footer->setPreferredVerticalHeight(96);
    nav.setHeaderChromeWidget(header);
    nav.setMainChromeWidget(main);
    nav.setFooterChromeWidget(footer);
    nav.setDisplayMode(NavigationView::DisplayMode::Left);
    nav.resize(900, 520);
    showAndProcess(nav);

    EXPECT_EQ(nav.effectiveDisplayMode(), NavigationView::DisplayMode::Left);
    EXPECT_EQ(nav.headerChromeGeometry().width(), nav.expandedPaneWidth());
    EXPECT_EQ(nav.headerChromeGeometry().height(), 88);

    nav.setDisplayMode(NavigationView::DisplayMode::Top);
    QApplication::processEvents();
    EXPECT_EQ(nav.effectiveDisplayMode(), NavigationView::DisplayMode::Top);

    header->setOrientation(Qt::Horizontal);
    main->setOrientation(Qt::Horizontal);
    footer->setOrientation(Qt::Horizontal);
    QApplication::sendPostedEvents(&nav, QEvent::LayoutRequest);
    QApplication::processEvents();

    EXPECT_EQ(nav.contentGeometry().left(), 0);
    EXPECT_EQ(nav.contentGeometry().top(), nav.topBarHeight());
    EXPECT_EQ(nav.headerChromeGeometry().height(), nav.topBarHeight());
    EXPECT_LT(nav.headerChromeGeometry().width(), nav.expandedPaneWidth());
    EXPECT_EQ(header->geometry(), nav.headerChromeGeometry());
}

TEST_F(NavigationViewTest, CompactNavigationRowsDoNotPaintSelectedIndicator)
{
    NavigationItemRow row(Typography::Icons::Home, QStringLiteral("Home"), true);

    EXPECT_TRUE(row.selectedIndicatorVisible());

    row.setCompact(true);
    EXPECT_FALSE(row.selectedIndicatorVisible());

    row.setCompact(false);
    EXPECT_TRUE(row.selectedIndicatorVisible());
}

TEST_F(NavigationViewTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;

    auto* visual = new NavigationViewTestWindow();
    visual->setAttribute(Qt::WA_DeleteOnClose);
    visual->resize(1120, 760);
    visual->setWindowTitle(QStringLiteral("NavigationView VisualCheck"));
    visual->onThemeUpdated();

    auto* visualLayout = new AnchorLayout(visual);
    visual->setLayout(visualLayout);

    auto* nav = new NavigationView(visual);
    nav->setDisplayMode(NavigationView::DisplayMode::Left);
    nav->setExpandedPaneWidth(280);
    nav->anchors()->top = {visual, Edge::Top, 64};
    nav->anchors()->left = {visual, Edge::Left, 32};
    nav->anchors()->right = {visual, Edge::Right, -32};
    nav->anchors()->bottom = {visual, Edge::Bottom, -32};
    addAnchored(visualLayout, nav);

    auto* title = new Label(QStringLiteral("NavigationView"), visual);
    title->setFluentTypography(QStringLiteral("Subtitle"));
    title->anchors()->top = {visual, Edge::Top, 24};
    title->anchors()->left = {visual, Edge::Left, 32};
    addAnchored(visualLayout, title);

    auto* topButton = new Button(QStringLiteral("Top"), visual);
    topButton->setFluentLayout(Button::IconBefore);
    topButton->setIconGlyph(Typography::Icons::GlobalNav, 16);
    topButton->setFixedSize(96, 32);
    topButton->anchors()->top = {visual, Edge::Top, 24};
    topButton->anchors()->right = {visual, Edge::Right, -528};
    addAnchored(visualLayout, topButton);

    auto* leftButton = new Button(QStringLiteral("Left"), visual);
    leftButton->setFluentLayout(Button::IconBefore);
    leftButton->setIconGlyph(Typography::Icons::List, 16);
    leftButton->setFixedSize(96, 32);
    leftButton->anchors()->top = {visual, Edge::Top, 24};
    leftButton->anchors()->right = {visual, Edge::Right, -424};
    addAnchored(visualLayout, leftButton);

    auto* leftCompactButton = new Button(QStringLiteral("LeftCompact"), visual);
    leftCompactButton->setFluentLayout(Button::IconBefore);
    leftCompactButton->setIconGlyph(Typography::Icons::List, 16);
    leftCompactButton->setFixedSize(136, 32);
    leftCompactButton->anchors()->top = {visual, Edge::Top, 24};
    leftCompactButton->anchors()->right = {visual, Edge::Right, -280};
    addAnchored(visualLayout, leftCompactButton);

    auto* leftMinimalButton = new Button(QStringLiteral("LeftMinimal"), visual);
    leftMinimalButton->setFluentLayout(Button::IconBefore);
    leftMinimalButton->setIconGlyph(Typography::Icons::GlobalNav, 16);
    leftMinimalButton->setFixedSize(136, 32);
    leftMinimalButton->anchors()->top = {visual, Edge::Top, 24};
    leftMinimalButton->anchors()->right = {visual, Edge::Right, -136};
    addAnchored(visualLayout, leftMinimalButton);

    auto* themeButton = new Button(QStringLiteral("Dark"), visual);
    themeButton->setFluentLayout(Button::IconBefore);
    themeButton->setIconGlyph(Typography::Icons::Color, 16);
    themeButton->setFixedSize(96, 32);
    themeButton->anchors()->top = {visual, Edge::Top, 24};
    themeButton->anchors()->right = {visual, Edge::Right, -32};
    addAnchored(visualLayout, themeButton);

    auto* sideHeader = new NavigationPaneSection(Qt::Vertical, {
        {Typography::Icons::Back, QStringLiteral("Back"), false},
        {Typography::Icons::Search, QStringLiteral("Search"), false},
    }, nav);
    sideHeader->setPreferredVerticalHeight(88);
    auto* sideMain = new NavigationPaneSection(Qt::Vertical, {
        {Typography::Icons::Home, QStringLiteral("Home"), true},
        {Typography::Icons::Contact, QStringLiteral("Account"), false},
        {Typography::Icons::Document, QStringLiteral("Documents"), false},
        {Typography::Icons::Download, QStringLiteral("Downloads"), false},
    }, nav);
    auto* sideFooter = new NavigationPaneSection(Qt::Vertical, {
        {Typography::Icons::Info, QStringLiteral("Help"), false},
        {Typography::Icons::Settings, QStringLiteral("Settings"), false},
    }, nav);
    sideFooter->setPreferredVerticalHeight(96);
    nav->setHeaderChromeWidget(sideHeader);
    nav->setMainChromeWidget(sideMain);
    nav->setFooterChromeWidget(sideFooter);

    auto addPage = [nav](const QString& title, const QString& subtitle, const QStringList& activityRows) {
        auto* page = new DashboardPage(title, subtitle, activityRows);
        const int index = nav->contentHost()->count();
        EXPECT_TRUE(nav->contentHost()->insertPage(index, page));
        return page;
    };

    QVector<DashboardPage*> pages;
    pages.append(addPage(QStringLiteral("Home"),
                         QStringLiteral("Shell content supplied by the application layer"),
                         {
                             QStringLiteral("Navigation shell switched to lightweight composition"),
                             QStringLiteral("Documents page opened from the side pane"),
                             QStringLiteral("Settings footer item is available as app chrome")
                         }));
    pages.append(addPage(QStringLiteral("Search"),
                         QStringLiteral("Find project files and recent navigation targets"),
                         {
                             QStringLiteral("Search opened from the header chrome"),
                             QStringLiteral("Recent documents are ready for lookup"),
                             QStringLiteral("Query results stay inside the content host")
                         }));
    pages.append(addPage(QStringLiteral("Account"),
                         QStringLiteral("Profile and session state owned by the application layer"),
                         {
                             QStringLiteral("Account settings loaded from a content page"),
                             QStringLiteral("NavigationView only assigns shell geometry"),
                             QStringLiteral("Chrome selection remains decoupled from page widgets")
                         }));
    pages.append(addPage(QStringLiteral("Documents"),
                         QStringLiteral("Document workspace rendered inside StackContentHost"),
                         {
                             QStringLiteral("Documents page opened from the side pane"),
                             QStringLiteral("Pinned files are represented by page content"),
                             QStringLiteral("Page transition effect follows the display mode")
                         }));
    pages.append(addPage(QStringLiteral("Downloads"),
                         QStringLiteral("Download queue and transfer status"),
                         {
                             QStringLiteral("Download history switched through NavigationPaneSection"),
                             QStringLiteral("Side modes slide content from the left"),
                             QStringLiteral("Top mode slides content from the bottom")
                         }));
    pages.append(addPage(QStringLiteral("Help"),
                         QStringLiteral("Support resources surfaced from footer chrome"),
                         {
                             QStringLiteral("Help footer item switches the content host"),
                             QStringLiteral("Keyboard and visual tests share the same shell"),
                             QStringLiteral("Animation remains configured outside NavigationView")
                         }));
    pages.append(addPage(QStringLiteral("Settings"),
                         QStringLiteral("Application settings page supplied by the demo"),
                         {
                             QStringLiteral("Settings footer item is available as app chrome"),
                             QStringLiteral("Content pages stay owned by StackContentHost"),
                             QStringLiteral("Display mode buttons only change shell layout")
                         }));
    nav->contentHost()->setCurrentIndex(0, 0, false);

    auto applyContentTransitionEffect = [nav](NavigationView::DisplayMode mode) {
        nav->contentHost()->setTransitionEffect(mode == NavigationView::DisplayMode::Top
                                                    ? StackContentHost::TransitionEffect::SlideFromBottom
                                                    : StackContentHost::TransitionEffect::SlideFromLeft);
    };
    applyContentTransitionEffect(nav->displayMode());
    QObject::connect(nav, &NavigationView::effectiveDisplayModeChanged, [applyContentTransitionEffect](NavigationView::DisplayMode mode) {
        applyContentTransitionEffect(mode);
    });

    auto applyChromePresentation = [nav, sideHeader, sideMain, sideFooter](NavigationView::DisplayMode mode) {
        const bool topMode = mode == NavigationView::DisplayMode::Top;
        const bool compactChrome = !topMode && mode != NavigationView::DisplayMode::Left;
        const Qt::Orientation orientation = topMode ? Qt::Horizontal : Qt::Vertical;
        sideHeader->setOrientation(orientation);
        sideMain->setOrientation(orientation);
        sideFooter->setOrientation(orientation);
        sideHeader->setCompact(compactChrome);
        sideMain->setCompact(compactChrome);
        sideFooter->setCompact(compactChrome);
        nav->onThemeUpdated();
    };
    applyChromePresentation(nav->displayMode());

    auto selectPage = [nav, sideHeader, sideMain, sideFooter](NavigationPaneSection* section, int rowIndex, int pageIndex) {
        sideHeader->clearSelection();
        sideMain->clearSelection();
        sideFooter->clearSelection();
        section->setSelectedIndex(rowIndex);
        const int direction = pageIndex >= nav->contentHost()->currentIndex() ? 1 : -1;
        nav->contentHost()->setCurrentIndex(pageIndex, direction, true);
    };

    sideHeader->onActivated = [selectPage, sideHeader](int index) {
        selectPage(sideHeader, index, index == 0 ? 0 : 1);
    };
    sideMain->onActivated = [selectPage, sideMain](int index) {
        selectPage(sideMain, index, index == 0 ? 0 : index + 1);
    };
    sideFooter->onActivated = [selectPage, sideFooter](int index) {
        selectPage(sideFooter, index, index + 5);
    };

    auto setDisplayMode = [nav, applyChromePresentation, applyContentTransitionEffect](NavigationView::DisplayMode nextMode) {
        applyContentTransitionEffect(nextMode);
        applyChromePresentation(nextMode);
        nav->setPaneOpen(nextMode == NavigationView::DisplayMode::Left || nextMode == NavigationView::DisplayMode::Top);
        nav->setDisplayMode(nextMode);
    };

    QObject::connect(topButton, &Button::clicked, [setDisplayMode]() {
        setDisplayMode(NavigationView::DisplayMode::Top);
    });
    QObject::connect(leftButton, &Button::clicked, [setDisplayMode]() {
        setDisplayMode(NavigationView::DisplayMode::Left);
    });
    QObject::connect(leftCompactButton, &Button::clicked, [setDisplayMode]() {
        setDisplayMode(NavigationView::DisplayMode::LeftCompact);
    });
    QObject::connect(leftMinimalButton, &Button::clicked, [setDisplayMode]() {
        setDisplayMode(NavigationView::DisplayMode::LeftMinimal);
    });
    QObject::connect(themeButton, &Button::clicked, [visual, nav, themeButton, sideHeader, sideMain, sideFooter, pages]() {
        const bool useDark = fluent::FluentElement::currentTheme() == fluent::FluentElement::Light;
        fluent::FluentElement::setTheme(useDark ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
        visual->onThemeUpdated();
        nav->onThemeUpdated();
        sideHeader->onThemeUpdated();
        sideMain->onThemeUpdated();
        sideFooter->onThemeUpdated();
        for (DashboardPage* page : pages)
            page->onThemeUpdated();
        themeButton->setText(useDark ? QStringLiteral("Light") : QStringLiteral("Dark"));
    });

    visual->show();
    qApp->exec();
}
