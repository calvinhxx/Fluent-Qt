#include "NavigationSamples.h"

#include <functional>

#include <QBoxLayout>
#include <QColor>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QStringList>
#include <QVariant>

#include "components/basicinput/Button.h"
#include "components/navigation/Breadcrumb.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/Pivot.h"
#include "components/navigation/SelectorBar.h"
#include "components/navigation/StackContentHost.h"
#include "components/navigation/TabView.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::navigation::Breadcrumb;
using fluent::navigation::BreadcrumbItem;
using fluent::navigation::NavigationView;
using fluent::navigation::Pivot;
using fluent::navigation::PivotItem;
using fluent::navigation::SelectorBar;
using fluent::navigation::SelectorBarItem;
using fluent::navigation::StackContentHost;
using fluent::navigation::TabView;
using fluent::navigation::TabViewItem;
using fluent::textfields::Label;
using samples::horizontalGroup;
using samples::makeSample;
using samples::verticalGroup;

Label* makeStatusLabel(QWidget* parent, const QString& text)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(Typography::FontRole::Body);
    label->setWordWrap(true);
    return label;
}

QWidget* makeHostPage(QWidget* parent, const QString& title, const QString& body)
{
    QWidget* page = verticalGroup(parent, 6);
    page->layout()->setContentsMargins(16, 14, 16, 14);

    auto* heading = new Label(title, page);
    heading->setFluentTypography(Typography::FontRole::BodyStrong);
    auto* summary = makeStatusLabel(page, body);
    summary->setFluentTypography(Typography::FontRole::Caption);

    page->layout()->addWidget(heading);
    page->layout()->addWidget(summary);
    return page;
}

Button* makeControlButton(QWidget* parent, const QString& text)
{
    auto* button = new Button(text, parent);
    button->setFluentSize(Button::Small);
    return button;
}

QString navigationDisplayModeName(NavigationView::DisplayMode mode)
{
    switch (mode) {
    case NavigationView::DisplayMode::Auto:
        return QStringLiteral("Auto");
    case NavigationView::DisplayMode::Left:
        return QStringLiteral("Left");
    case NavigationView::DisplayMode::LeftCompact:
        return QStringLiteral("LeftCompact");
    case NavigationView::DisplayMode::LeftMinimal:
        return QStringLiteral("LeftMinimal");
    case NavigationView::DisplayMode::Top:
        return QStringLiteral("Top");
    }
    return QString();
}

class NavigationDemoRow : public QWidget, public fluent::FluentElement {
public:
    NavigationDemoRow(const QString& iconGlyph, const QString& text, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_iconGlyph(iconGlyph)
        , m_text(text)
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
        return QSize(qMax(88, metrics.horizontalAdvance(m_text) + 68), 40);
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

    std::function<void()> onActivated;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const auto colors = themeColors();
        const QRectF itemRect = rect().adjusted(4, 2, -4, -2);

        QColor fill = Qt::transparent;
        if (m_pressed)
            fill = colors.subtleTertiary;
        else if (m_selected || m_hovered)
            fill = colors.subtleSecondary;
        if (fill.alpha() > 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawRoundedRect(itemRect, themeRadius().control, themeRadius().control);
        }

        if (m_selected && !m_compact) {
            const QRectF indicator(itemRect.left() + 4, itemRect.center().y() - 7, 3, 14);
            painter.setBrush(colors.accentDefault);
            painter.drawRoundedRect(indicator, 1.5, 1.5);
        }

        const int iconX = m_compact ? (width() - 20) / 2 : 22;
        QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
        iconFont.setPixelSize(16);
        painter.setFont(iconFont);
        painter.setPen(m_selected ? colors.textPrimary : colors.textSecondary);
        painter.drawText(QRect(iconX, 0, 20, height()), Qt::AlignCenter, m_iconGlyph);

        if (!m_compact) {
            painter.setFont(themeFont(m_selected ? QStringLiteral("BodyStrong") : QStringLiteral("Body")).toQFont());
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

class NavigationDemoSection : public QWidget, public fluent::FluentElement {
public:
    struct Entry {
        QString icon;
        QString text;
        int pageIndex = -1;
    };

    explicit NavigationDemoSection(const QVector<Entry>& entries, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_layout(new QBoxLayout(QBoxLayout::TopToBottom, this))
        , m_entries(entries)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_layout->setContentsMargins(8, 8, 8, 8);
        m_layout->setSpacing(4);
        for (int i = 0; i < m_entries.size(); ++i) {
            auto* row = new NavigationDemoRow(m_entries.at(i).icon, m_entries.at(i).text, this);
            row->onActivated = [this, i]() {
                setSelectedIndex(i);
                if (onActivated)
                    onActivated(m_entries.at(i).pageIndex);
            };
            m_rows.append(row);
            m_layout->addWidget(row);
        }
        m_layout->addStretch();
    }

    QSize sizeHint() const override
    {
        if (m_orientation == Qt::Horizontal) {
            int widthValue = 16;
            for (NavigationDemoRow* row : m_rows)
                widthValue += row->sizeHint().width() + m_layout->spacing();
            return QSize(widthValue, 48);
        }
        return QSize(220, m_preferredVerticalHeight > 0 ? m_preferredVerticalHeight : 16 + m_rows.size() * 44);
    }

    QSize minimumSizeHint() const override
    {
        return m_orientation == Qt::Horizontal ? QSize(0, 44) : QSize(48, 16 + m_rows.size() * 40);
    }

    void setOrientation(Qt::Orientation orientation)
    {
        m_orientation = orientation;
        m_layout->setDirection(orientation == Qt::Vertical
                                   ? QBoxLayout::TopToBottom
                                   : QBoxLayout::LeftToRight);
        m_layout->setContentsMargins(8,
                                     orientation == Qt::Vertical ? 8 : 4,
                                     8,
                                     orientation == Qt::Vertical ? 8 : 4);
        for (NavigationDemoRow* row : m_rows) {
            row->setFixedHeight(orientation == Qt::Vertical ? 40 : 36);
            row->setCompact(m_compact);
        }
        updateGeometry();
    }

    void setCompact(bool compact)
    {
        m_compact = compact;
        for (NavigationDemoRow* row : m_rows)
            row->setCompact(compact);
        updateGeometry();
    }

    void setPreferredVerticalHeight(int height)
    {
        m_preferredVerticalHeight = height;
        updateGeometry();
    }

    void clearSelection()
    {
        setSelectedIndex(-1);
    }

    void setSelectedIndex(int index)
    {
        if (m_selectedIndex == index)
            return;
        m_selectedIndex = index;
        for (int i = 0; i < m_rows.size(); ++i)
            m_rows.at(i)->setSelected(i == index);
    }

    std::function<void(int)> onActivated;

private:
    QBoxLayout* m_layout = nullptr;
    QVector<Entry> m_entries;
    QVector<NavigationDemoRow*> m_rows;
    Qt::Orientation m_orientation = Qt::Vertical;
    int m_selectedIndex = -1;
    int m_preferredVerticalHeight = 0;
    bool m_compact = false;
};

class NavigationSampleSurface : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(themeColors().strokeCard);
        painter.setBrush(themeColors().bgCanvas);
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), themeRadius().overlay, themeRadius().overlay);
    }
};

class NavigationDashboardPage : public QWidget, public fluent::FluentElement {
public:
    NavigationDashboardPage(const QString& title,
                            const QString& subtitle,
                            const QString& firstMetricTitle,
                            const QString& firstMetricValue,
                            const QString& secondMetricTitle,
                            const QString& secondMetricValue,
                            const QStringList& activityRows,
                            QWidget* parent = nullptr)
        : QWidget(parent)
        , m_title(title)
        , m_subtitle(subtitle)
        , m_firstMetricTitle(firstMetricTitle)
        , m_firstMetricValue(firstMetricValue)
        , m_secondMetricTitle(secondMetricTitle)
        , m_secondMetricValue(secondMetricValue)
        , m_activityRows(activityRows)
    {
        setAutoFillBackground(false);
    }

    void onThemeUpdated() override
    {
        update();
    }

    void setContentLeftInset(int inset)
    {
        const int normalized = qMax(0, inset);
        if (m_contentLeftInset == normalized)
            return;
        m_contentLeftInset = normalized;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const auto colors = themeColors();
        const int sidePad = 22;
        const int leftInset = qMin(m_contentLeftInset, qMax(0, width() - 220));
        const int leftPad = sidePad + leftInset;
        const int contentWidth = qMax(0, width() - leftPad - sidePad);
        painter.fillRect(rect(), colors.bgLayer);

        painter.setPen(colors.textPrimary);
        painter.setFont(themeFont(QStringLiteral("Subtitle")).toQFont());
        painter.drawText(QRect(leftPad, 12, contentWidth, 28),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         m_title);

        painter.setPen(colors.textSecondary);
        painter.setFont(themeFont(QStringLiteral("Caption")).toQFont());
        painter.drawText(QRect(leftPad, 42, contentWidth, 30),
                         Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                         m_subtitle);

        const int gap = 12;
        const int cardTop = 82;
        const int cardHeight = 56;
        const int cardWidth = qMax(96, (contentWidth - gap) / 2);
        drawCard(painter,
                 QRect(leftPad, cardTop, cardWidth, cardHeight),
                 m_firstMetricTitle,
                 m_firstMetricValue,
                 colors.accentDefault);
        drawCard(painter,
                 QRect(leftPad + cardWidth + gap, cardTop, cardWidth, cardHeight),
                 m_secondMetricTitle,
                 m_secondMetricValue,
                 colors.systemInfo);

        const int panelTop = cardTop + cardHeight + 14;
        const int panelHeight = qMax(72, height() - panelTop - 14);
        const QRect panel(leftPad, panelTop, contentWidth, panelHeight);
        painter.setPen(colors.strokeCard);
        painter.setBrush(colors.bgCanvas);
        painter.drawRoundedRect(panel, themeRadius().overlay, themeRadius().overlay);

        painter.setPen(colors.textPrimary);
        painter.setFont(themeFont(QStringLiteral("BodyStrong")).toQFont());
        painter.drawText(panel.adjusted(16, 10, -16, -panel.height() + 34),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("Recent activity"));

        painter.setFont(themeFont(QStringLiteral("Caption")).toQFont());
        int y = panel.top() + 38;
        for (const QString& row : m_activityRows) {
            const QRect rowRect(panel.left() + 14, y, panel.width() - 28, 24);
            painter.setPen(colors.strokeDivider);
            painter.drawLine(rowRect.topLeft(), rowRect.topRight());
            painter.setPen(colors.textSecondary);
            painter.drawText(rowRect.adjusted(2, 0, -2, 0),
                             Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                             row);
            y += 28;
            if (y > panel.bottom() - 16)
                break;
        }
    }

private:
    QString m_title;
    QString m_subtitle;
    QString m_firstMetricTitle;
    QString m_firstMetricValue;
    QString m_secondMetricTitle;
    QString m_secondMetricValue;
    QStringList m_activityRows;
    int m_contentLeftInset = 0;

    void drawCard(QPainter& painter,
                  const QRect& rectValue,
                  const QString& title,
                  const QString& value,
                  const QColor& accent)
    {
        const auto colors = themeColors();
        painter.setPen(colors.strokeCard);
        painter.setBrush(colors.bgCanvas);
        painter.drawRoundedRect(rectValue, themeRadius().control, themeRadius().control);

        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawRoundedRect(QRect(rectValue.left() + 12, rectValue.top() + 12, 3, 30), 1.5, 1.5);

        painter.setPen(colors.textSecondary);
        painter.setFont(themeFont(QStringLiteral("Caption")).toQFont());
        painter.drawText(rectValue.adjusted(26, 8, -12, -rectValue.height() + 30),
                         Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                         title);

        painter.setPen(colors.textPrimary);
        painter.setFont(themeFont(QStringLiteral("BodyStrong")).toQFont());
        painter.drawText(rectValue.adjusted(26, 28, -12, -8),
                         Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                         value);
    }
};

NavigationSampleSurface* makeNavigationPreviewSurface(QWidget* parent, int spacing = 8)
{
    auto* surface = new NavigationSampleSurface(parent);
    surface->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto* layout = new QBoxLayout(QBoxLayout::TopToBottom, surface);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(spacing);
    surface->setLayout(layout);
    return surface;
}

struct NavigationPageSpec {
    QString title;
    QString subtitle;
    QString firstMetricTitle;
    QString firstMetricValue;
    QString secondMetricTitle;
    QString secondMetricValue;
    QStringList activityRows;
};

QVector<NavigationPageSpec> navigationPageSpecs()
{
    return {
        {QStringLiteral("Home"),
         QStringLiteral("Navigation rows route to pages owned by StackContentHost."),
         QStringLiteral("Page"),
         QStringLiteral("0"),
         QStringLiteral("Scope"),
         QStringLiteral("Home"),
         {QStringLiteral("Home row activated page index 0"),
          QStringLiteral("NavigationView keeps shell geometry separate"),
          QStringLiteral("Selection is maintained by the app chrome")}},
        {QStringLiteral("Search"),
         QStringLiteral("Header chrome can navigate to a hosted search page."),
         QStringLiteral("Page"),
         QStringLiteral("1"),
         QStringLiteral("Results"),
         QStringLiteral("128"),
         {QStringLiteral("Search row activated page index 1"),
          QStringLiteral("Header and main rows share the same host"),
          QStringLiteral("The active page stays inside contentHost()")}},
        {QStringLiteral("Documents"),
         QStringLiteral("Document workspace content is supplied by the application layer."),
         QStringLiteral("Page"),
         QStringLiteral("2"),
         QStringLiteral("Pinned"),
         QStringLiteral("8 files"),
         {QStringLiteral("Documents row activated page index 2"),
          QStringLiteral("The content transition follows the shell mode"),
          QStringLiteral("Pane width does not change page ownership")}},
        {QStringLiteral("Downloads"),
         QStringLiteral("Downloads keeps working across expanded, compact, and minimal pane modes."),
         QStringLiteral("Page"),
         QStringLiteral("3"),
         QStringLiteral("Queue"),
         QStringLiteral("6 items"),
         {QStringLiteral("Downloads row activated page index 3"),
          QStringLiteral("Compact rows keep icons centered"),
          QStringLiteral("Minimal overlay leaves the host page readable")}},
        {QStringLiteral("Help"),
         QStringLiteral("Footer chrome participates in the same page routing as the main section."),
         QStringLiteral("Page"),
         QStringLiteral("4"),
         QStringLiteral("Articles"),
         QStringLiteral("36"),
         {QStringLiteral("Help row activated page index 4"),
          QStringLiteral("Footer chrome is laid out separately"),
          QStringLiteral("StackContentHost owns the page widget")}},
        {QStringLiteral("Settings"),
         QStringLiteral("Settings is an ordinary hosted page selected from app-owned chrome."),
         QStringLiteral("Page"),
         QStringLiteral("5"),
         QStringLiteral("Updates"),
         QStringLiteral("Current"),
         {QStringLiteral("Settings row activated page index 5"),
          QStringLiteral("Routing is shared across NavigationView samples"),
          QStringLiteral("NavigationView does not own a menu model")}}
    };
}

QString navigationPageTitle(int index)
{
    const auto specs = navigationPageSpecs();
    if (index < 0 || index >= specs.size())
        return QStringLiteral("Unknown");
    return specs.at(index).title;
}

QVector<NavigationDashboardPage*> populateNavigationPages(StackContentHost* host)
{
    QVector<NavigationDashboardPage*> pages;
    const auto specs = navigationPageSpecs();
    for (int i = 0; i < specs.size(); ++i) {
        const auto& spec = specs.at(i);
        auto* page = new NavigationDashboardPage(spec.title,
                                                 spec.subtitle,
                                                 spec.firstMetricTitle,
                                                 spec.firstMetricValue,
                                                 spec.secondMetricTitle,
                                                 spec.secondMetricValue,
                                                 spec.activityRows,
                                                 host);
        host->insertPage(i, page);
        pages.append(page);
    }
    host->setCurrentIndex(0, 0, false);
    return pages;
}

void setNavigationPagesLeftInset(const QVector<NavigationDashboardPage*>& pages, int inset)
{
    for (NavigationDashboardPage* page : pages)
        page->setContentLeftInset(inset);
}

void connectNavigationRoutes(NavigationDemoSection* headerSection,
                             NavigationDemoSection* mainSection,
                             NavigationDemoSection* footerSection,
                             StackContentHost* host,
                             Label* status)
{
    auto routeToPage = [headerSection, mainSection, footerSection, host, status](NavigationDemoSection* source, int pageIndex) {
        if (pageIndex < 0 || pageIndex >= host->count())
            return;
        if (headerSection && source != headerSection)
            headerSection->clearSelection();
        if (mainSection && source != mainSection)
            mainSection->clearSelection();
        if (footerSection && source != footerSection)
            footerSection->clearSelection();
        const int direction = pageIndex >= host->currentIndex() ? 1 : -1;
        host->setCurrentIndex(pageIndex, direction, true);
        if (status)
            status->setText(QStringLiteral("Current page: %1").arg(navigationPageTitle(pageIndex)));
    };

    if (headerSection) {
        headerSection->onActivated = [headerSection, routeToPage](int pageIndex) {
            routeToPage(headerSection, pageIndex);
        };
    }
    if (mainSection) {
        mainSection->onActivated = [mainSection, routeToPage](int pageIndex) {
            routeToPage(mainSection, pageIndex);
        };
    }
    if (footerSection) {
        footerSection->onActivated = [footerSection, routeToPage](int pageIndex) {
            routeToPage(footerSection, pageIndex);
        };
    }
}

QStringList breadcrumbDemoPath()
{
    return {
        QStringLiteral("Home"),
        QStringLiteral("Projects"),
        QStringLiteral("Fluent"),
        QStringLiteral("Controls"),
        QStringLiteral("Breadcrumb")
    };
}

QString hiddenIndexesText(const QVector<int>& indexes)
{
    if (indexes.isEmpty())
        return QStringLiteral("none");
    QStringList values;
    for (int index : indexes)
        values.append(QString::number(index));
    return values.join(QStringLiteral(", "));
}

QVector<GallerySample> breadcrumbSamples()
{
    return {
        makeSample(QStringLiteral("breadcrumb-size"),
                   QStringLiteral("BreadcrumbSize"),
                   QStringLiteral("Standard uses the compact 20 px row; Large uses the 40 px row and larger item padding."),
                   QStringLiteral("const QStringList path{\"Home\", \"Documents\", \"Images\"};\n\n"
                                  "auto* standard = new Breadcrumb(this);\n"
                                  "standard->setItems(path);\n"
                                  "standard->setBreadcrumbSize(Breadcrumb::BreadcrumbSize::Standard);\n"
                                  "standard->setFixedHeight(20);\n\n"
                                  "auto* large = new Breadcrumb(this);\n"
                                  "large->setItems(path);\n"
                                  "large->setBreadcrumbSize(Breadcrumb::BreadcrumbSize::Large);\n"
                                  "large->setFixedHeight(40);"),
                   [](QWidget* parent) {
                       auto* group = verticalGroup(parent, 10);
                       const QStringList path{
                           QStringLiteral("Home"),
                           QStringLiteral("Documents"),
                           QStringLiteral("Images")
                       };

                       auto* standardRow = horizontalGroup(group, 10);
                       auto* standardLabel = new Label(QStringLiteral("Standard"), standardRow);
                       standardLabel->setFluentTypography(Typography::FontRole::Caption);
                       standardLabel->setFixedWidth(72);
                       auto* standard = new Breadcrumb(standardRow);
                       standard->setItems(path);
                       standard->setBreadcrumbSize(Breadcrumb::BreadcrumbSize::Standard);
                       standard->setFixedSize(360, 20);
                       standardRow->layout()->addWidget(standardLabel);
                       standardRow->layout()->addWidget(standard);

                       auto* largeRow = horizontalGroup(group, 10);
                       auto* largeLabel = new Label(QStringLiteral("Large"), largeRow);
                       largeLabel->setFluentTypography(Typography::FontRole::Caption);
                       largeLabel->setFixedWidth(72);
                       auto* large = new Breadcrumb(largeRow);
                       large->setItems(path);
                       large->setBreadcrumbSize(Breadcrumb::BreadcrumbSize::Large);
                       large->setFixedSize(430, 40);
                       largeRow->layout()->addWidget(largeLabel);
                       largeRow->layout()->addWidget(large);

                       group->layout()->addWidget(standardRow);
                       group->layout()->addWidget(largeRow);
                       return group;
                   }),
        makeSample(QStringLiteral("breadcrumb-overflow-mode"),
                   QStringLiteral("OverflowMode"),
                   QStringLiteral("Beginning keeps the leaf visible; Middle keeps root and leaf visible; None never creates an overflow button."),
                   QStringLiteral("const QStringList path{\"Home\", \"Projects\", \"Fluent\", \"Controls\", \"Breadcrumb\"};\n\n"
                                  "auto* none = new Breadcrumb(this);\n"
                                  "none->setItems(path);\n"
                                  "none->setOverflowMode(Breadcrumb::OverflowMode::None);\n\n"
                                  "auto* beginning = new Breadcrumb(this);\n"
                                  "beginning->setItems(path);\n"
                                  "beginning->setOverflowMode(Breadcrumb::OverflowMode::Beginning);\n\n"
                                  "auto* middle = new Breadcrumb(this);\n"
                                  "middle->setItems(path);\n"
                                  "middle->setOverflowMode(Breadcrumb::OverflowMode::Middle);\n"
                                  "// Give Beginning and Middle less width so the overflow button appears.\n"
                                  "none->setFixedWidth(520);\n"
                                  "beginning->setFixedWidth(260);\n"
                                  "middle->setFixedWidth(260);\n\n"
                                  "connect(middle, &Breadcrumb::overflowActivated,\n"
                                  "        this, [](const QVector<int>& hiddenIndexes) {\n"
                                  "            // hiddenIndexes are the path segments behind the overflow button.\n"
                                  "        });"),
                   [](QWidget* parent) {
                       auto* group = verticalGroup(parent, 10);
                       const QStringList path = breadcrumbDemoPath();
                       auto* status = makeStatusLabel(group, QStringLiteral("Hidden indexes: none"));
                       status->setFluentTypography(Typography::FontRole::Caption);

                       auto addModeRow = [group, path, status](const QString& label,
                                                               Breadcrumb::OverflowMode mode,
                                                               int width) {
                           auto* row = horizontalGroup(group, 10);
                           auto* rowLabel = new Label(label, row);
                           rowLabel->setFluentTypography(Typography::FontRole::Caption);
                           rowLabel->setFixedWidth(72);
                           auto* breadcrumb = new Breadcrumb(row);
                           breadcrumb->setItems(path);
                           breadcrumb->setOverflowMode(mode);
                           breadcrumb->setFixedSize(width, 20);
                           QObject::connect(breadcrumb, &Breadcrumb::overflowActivated,
                                            status, [status, label](const QVector<int>& hiddenIndexes) {
                                                status->setText(QStringLiteral("%1 hidden indexes: %2")
                                                                    .arg(label, hiddenIndexesText(hiddenIndexes)));
                                            });
                           row->layout()->addWidget(rowLabel);
                           row->layout()->addWidget(breadcrumb);
                           group->layout()->addWidget(row);
                       };

                       addModeRow(QStringLiteral("None"), Breadcrumb::OverflowMode::None, 520);
                       addModeRow(QStringLiteral("Beginning"), Breadcrumb::OverflowMode::Beginning, 260);
                       addModeRow(QStringLiteral("Middle"), Breadcrumb::OverflowMode::Middle, 260);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("breadcrumb-auto-truncate"),
                   QStringLiteral("autoTruncateOnItemClick"),
                   QStringLiteral("When enabled, activating an ancestor removes all items after it."),
                   QStringLiteral("const QStringList fullPath{\"This PC\", \"Local Disk\", \"Users\", \"Public\", \"Pictures\"};\n\n"
                                  "auto* breadcrumb = new Breadcrumb(this);\n"
                                  "breadcrumb->setItems(fullPath);\n"
                                  "breadcrumb->setAutoTruncateOnItemClick(true);\n\n"
                                  "auto* resetButton = new Button(\"Reset\", this);\n"
                                  "connect(breadcrumb, &Breadcrumb::itemsChanged,\n"
                                  "        this, [breadcrumb] {\n"
                                  "            // After clicking an ancestor, itemCount() becomes index + 1.\n"
                                  "        });\n"
                                  "connect(resetButton, &Button::clicked,\n"
                                  "        breadcrumb, [breadcrumb, fullPath] { breadcrumb->setItems(fullPath); });"),
                   [](QWidget* parent) {
                       auto* group = verticalGroup(parent, 10);
                       const QStringList fullPath{
                           QStringLiteral("This PC"),
                           QStringLiteral("Local Disk"),
                           QStringLiteral("Users"),
                           QStringLiteral("Public"),
                           QStringLiteral("Pictures")
                       };

                       auto* breadcrumb = new Breadcrumb(group);
                       breadcrumb->setItems(fullPath);
                       breadcrumb->setAutoTruncateOnItemClick(true);
                       breadcrumb->setFixedSize(520, 20);

                       auto* controls = horizontalGroup(group, 8);
                       auto* resetButton = makeControlButton(controls, QStringLiteral("Reset"));
                       controls->layout()->addWidget(resetButton);
                       auto* status = makeStatusLabel(group, QStringLiteral("Items: 5"));
                       status->setFluentTypography(Typography::FontRole::Caption);

                       QObject::connect(breadcrumb, &Breadcrumb::itemActivated,
                                        status, [status](int index, const BreadcrumbItem& item) {
                                            status->setText(QStringLiteral("Activated %1 at index %2")
                                                                .arg(item.text)
                                                                .arg(index));
                                        });
                       QObject::connect(breadcrumb, &Breadcrumb::itemsChanged,
                                        status, [status, breadcrumb]() {
                                            if (breadcrumb->itemCount() == 0) {
                                                status->setText(QStringLiteral("Items: 0"));
                                                return;
                                            }
                                            status->setText(QStringLiteral("Items: %1, current: %2")
                                                                .arg(breadcrumb->itemCount())
                                                                .arg(breadcrumb->itemAt(breadcrumb->itemCount() - 1).text));
                                        });
                       QObject::connect(resetButton, &Button::clicked,
                                        breadcrumb, [breadcrumb, fullPath, status]() {
                                            breadcrumb->setItems(fullPath);
                                            status->setText(QStringLiteral("Items: 5"));
                                        });

                       group->layout()->addWidget(breadcrumb);
                       group->layout()->addWidget(controls);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("breadcrumb-activation"),
                   QStringLiteral("itemActivated with hosted pages"),
                   QStringLiteral("A full, non-overflowing breadcrumb can jump to any ancestor without changing the trail."),
                   QStringLiteral("const QStringList fullPath{\"Home\", \"Projects\", \"Fluent\", \"Controls\", \"Breadcrumb\"};\n\n"
                                  "auto* breadcrumb = new Breadcrumb(this);\n"
                                  "breadcrumb->setItems(fullPath);\n"
                                  "breadcrumb->setOverflowMode(Breadcrumb::OverflowMode::None);\n\n"
                                  "auto* host = new StackContentHost(this);\n"
                                  "for (int i = 0; i < fullPath.size(); ++i)\n"
                                  "    host->insertPage(i, createPage(fullPath.at(i)));\n\n"
                                  "connect(breadcrumb, &Breadcrumb::itemActivated,\n"
                                  "        host, [host](int index, const BreadcrumbItem&) {\n"
                                  "            host->setCurrentIndex(index, 0, true);\n"
                                  "        });"),
                   [](QWidget* parent) {
                       auto* group = verticalGroup(parent, 10);

                       auto* breadcrumb = new Breadcrumb(group);
                       const QStringList fullPath = breadcrumbDemoPath();
                       breadcrumb->setItems(fullPath);
                       breadcrumb->setOverflowMode(Breadcrumb::OverflowMode::None);
                       breadcrumb->setFixedSize(540, 20);

                       auto* host = new StackContentHost(group);
                       host->setFixedSize(540, 126);
                       host->setTransitionAnimationEnabled(true);
                       for (int i = 0; i < fullPath.size(); ++i) {
                           host->insertPage(i, makeHostPage(
                               host,
                               fullPath.at(i),
                               QStringLiteral("StackContentHost page %1 selected from a full breadcrumb trail.")
                                   .arg(i + 1)));
                       }
                       host->setCurrentIndex(fullPath.size() - 1, 0, false);

                       auto* status = makeStatusLabel(group, QStringLiteral("Current: Breadcrumb"));
                       status->setFluentTypography(Typography::FontRole::Caption);
                       auto jumpTo = [host, status, fullPath](int index) {
                           const int bounded = qBound(0, index, fullPath.size() - 1);
                           host->setCurrentIndex(bounded, bounded >= host->currentIndex() ? 1 : -1, true);
                           status->setText(QStringLiteral("Current: %1").arg(fullPath.at(bounded)));
                       };
                       QObject::connect(breadcrumb, &Breadcrumb::itemActivated,
                                        host, [jumpTo](int index, const BreadcrumbItem&) {
                                            jumpTo(index);
                                        });

                       group->layout()->addWidget(breadcrumb);
                       group->layout()->addWidget(host);
                       group->layout()->addWidget(status);
                       return group;
                   })
    };
}

QVector<GallerySample> navigationViewSamples()
{
    return {
        makeSample(QStringLiteral("navigation-view-chrome-slots"),
                   QStringLiteral("Chrome slots"),
                   QStringLiteral("Header, main, and footer chrome are caller-owned widgets; NavigationView only assigns their shell geometry."),
                   QStringLiteral("auto* navView = new NavigationView(this);\n"
                                  "navView->setDisplayMode(NavigationView::DisplayMode::Left);\n"
                                  "navView->setExpandedPaneWidth(180);\n"
                                  "navView->setHeaderChromeWidget(headerSection);\n"
                                  "navView->setMainChromeWidget(mainSection);\n"
                                  "navView->setFooterChromeWidget(footerSection);\n"
                                  "populateNavigationPages(navView->contentHost());\n"
                                  "auto routeToPage = [host = navView->contentHost()](int pageIndex) {\n"
                                  "    const int direction = pageIndex >= host->currentIndex() ? 1 : -1;\n"
                                  "    host->setCurrentIndex(pageIndex, direction, true);\n"
                                  "};\n"
                                  "headerSection->onActivated = routeToPage;\n"
                                  "mainSection->onActivated = routeToPage;\n"
                                  "footerSection->onActivated = routeToPage;"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 8);
                       auto* surface = makeNavigationPreviewSurface(group);

                       auto* navView = new NavigationView(surface);
                       navView->setFixedSize(620, 340);
                       navView->setDisplayMode(NavigationView::DisplayMode::Left);
                       navView->setExpandedPaneWidth(180);

                       auto* headerSection = new NavigationDemoSection({
                           {Typography::Icons::Back, QStringLiteral("Back"), 0},
                           {Typography::Icons::Search, QStringLiteral("Search"), 1},
                       }, navView);
                       headerSection->setPreferredVerticalHeight(88);

                       auto* mainSection = new NavigationDemoSection({
                           {Typography::Icons::Home, QStringLiteral("Home"), 0},
                           {Typography::Icons::Document, QStringLiteral("Documents"), 2},
                           {Typography::Icons::Download, QStringLiteral("Downloads"), 3},
                       }, navView);
                       mainSection->setSelectedIndex(0);

                       auto* footerSection = new NavigationDemoSection({
                           {Typography::Icons::Info, QStringLiteral("Help"), 4},
                           {Typography::Icons::Settings, QStringLiteral("Settings"), 5},
                       }, navView);
                       footerSection->setPreferredVerticalHeight(88);

                       navView->setHeaderChromeWidget(headerSection);
                       navView->setMainChromeWidget(mainSection);
                       navView->setFooterChromeWidget(footerSection);

                       populateNavigationPages(navView->contentHost());
                       auto* status = makeStatusLabel(surface, QStringLiteral("Current page: Home"));
                       status->setFluentTypography(Typography::FontRole::Caption);
                       connectNavigationRoutes(headerSection, mainSection, footerSection, navView->contentHost(), status);

                       surface->layout()->addWidget(navView);
                       surface->layout()->addWidget(status);
                       group->layout()->addWidget(surface);
                       return group;
                   }),

        makeSample(QStringLiteral("navigation-view-display-modes"),
                   QStringLiteral("DisplayMode and chrome presentation"),
                   QStringLiteral("Switching display mode changes shell geometry; the app also updates chrome orientation and content transition semantics."),
                   QStringLiteral("navView->setAnimationEnabled(true);\n"
                                  "navView->setExpandedPaneWidth(180);\n"
                                  "navView->setCompactPaneWidth(52);\n"
                                  "auto applyDisplayMode = [&](NavigationView::DisplayMode mode) {\n"
                                  "    const bool top = mode == NavigationView::DisplayMode::Top;\n"
                                  "    const bool compact = mode != NavigationView::DisplayMode::Left;\n"
                                  "    const auto orientation = top ? Qt::Horizontal : Qt::Vertical;\n"
                                  "    headerSection->setOrientation(orientation);\n"
                                  "    mainSection->setOrientation(orientation);\n"
                                  "    footerSection->setOrientation(orientation);\n"
                                  "    headerSection->setCompact(compact);\n"
                                  "    mainSection->setCompact(compact);\n"
                                  "    footerSection->setCompact(compact);\n"
                                  "    navView->contentHost()->setTransitionEffect(\n"
                                  "        top ? StackContentHost::TransitionEffect::SlideFromBottom\n"
                                  "            : StackContentHost::TransitionEffect::SlideFromLeft);\n"
                                  "    navView->setPaneOpen(mode == NavigationView::DisplayMode::Left || top);\n"
                                  "    navView->setDisplayMode(mode);\n"
                                  "};\n"
                                  "auto routeToPage = [host = navView->contentHost()](int pageIndex) {\n"
                                  "    const int direction = pageIndex >= host->currentIndex() ? 1 : -1;\n"
                                  "    host->setCurrentIndex(pageIndex, direction, true);\n"
                                  "};\n"
                                  "headerSection->onActivated = routeToPage;\n"
                                  "mainSection->onActivated = routeToPage;\n"
                                  "footerSection->onActivated = routeToPage;\n"
                                  "QObject::connect(leftButton, &Button::clicked, navView, [&] {\n"
                                  "    applyDisplayMode(NavigationView::DisplayMode::Left);\n"
                                  "});\n"
                                  "QObject::connect(compactButton, &Button::clicked, navView, [&] {\n"
                                  "    applyDisplayMode(NavigationView::DisplayMode::LeftCompact);\n"
                                  "});\n"
                                  "QObject::connect(minimalButton, &Button::clicked, navView, [&] {\n"
                                  "    applyDisplayMode(NavigationView::DisplayMode::LeftMinimal);\n"
                                  "});\n"
                                  "QObject::connect(topButton, &Button::clicked, navView, [&] {\n"
                                  "    applyDisplayMode(NavigationView::DisplayMode::Top);\n"
                                  "});"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 8);
                       auto* surface = makeNavigationPreviewSurface(group);
                       auto* controls = horizontalGroup(surface, 6);
                       auto* leftButton = makeControlButton(controls, QStringLiteral("Left"));
                       auto* compactButton = makeControlButton(controls, QStringLiteral("Compact"));
                       auto* minimalButton = makeControlButton(controls, QStringLiteral("Minimal"));
                       auto* topButton = makeControlButton(controls, QStringLiteral("Top"));
                       leftButton->setIconGlyph(Typography::Icons::List, 16);
                       compactButton->setIconGlyph(Typography::Icons::GlobalNav, 16);
                       minimalButton->setIconGlyph(Typography::Icons::GlobalNav, 16);
                       topButton->setIconGlyph(Typography::Icons::AllApps, 16);
                       for (Button* button : {leftButton, compactButton, minimalButton, topButton}) {
                           button->setFluentLayout(Button::IconBefore);
                           controls->layout()->addWidget(button);
                       }

                       auto* navView = new NavigationView(surface);
                       navView->setFixedSize(620, 340);
                       navView->setAnimationEnabled(true);
                       navView->setExpandedPaneWidth(180);
                       navView->setCompactPaneWidth(52);
                       navView->setTopBarHeight(48);

                       auto* headerSection = new NavigationDemoSection({
                           {Typography::Icons::Back, QStringLiteral("Back"), 0},
                           {Typography::Icons::Search, QStringLiteral("Search"), 1},
                       }, navView);
                       headerSection->setPreferredVerticalHeight(88);
                       auto* mainSection = new NavigationDemoSection({
                           {Typography::Icons::Home, QStringLiteral("Home"), 0},
                           {Typography::Icons::Document, QStringLiteral("Documents"), 2},
                           {Typography::Icons::Download, QStringLiteral("Downloads"), 3},
                       }, navView);
                       mainSection->setSelectedIndex(0);
                       auto* footerSection = new NavigationDemoSection({
                           {Typography::Icons::Info, QStringLiteral("Help"), 4},
                           {Typography::Icons::Settings, QStringLiteral("Settings"), 5},
                       }, navView);
                       footerSection->setPreferredVerticalHeight(88);
                       navView->setHeaderChromeWidget(headerSection);
                       navView->setMainChromeWidget(mainSection);
                       navView->setFooterChromeWidget(footerSection);

                       populateNavigationPages(navView->contentHost());

                       auto* modeStatus = makeStatusLabel(surface, QStringLiteral("Mode: Left, effective: Left, transition: SlideFromLeft"));
                       modeStatus->setFluentTypography(Typography::FontRole::Caption);
                       auto* routeStatus = makeStatusLabel(surface, QStringLiteral("Current page: Home"));
                       routeStatus->setFluentTypography(Typography::FontRole::Caption);
                       connectNavigationRoutes(headerSection, mainSection, footerSection, navView->contentHost(), routeStatus);

                       auto updateStatus = [modeStatus, navView]() {
                           const QString transition = navView->contentHost()->transitionEffect() == StackContentHost::TransitionEffect::SlideFromBottom
                                                          ? QStringLiteral("SlideFromBottom")
                                                          : QStringLiteral("SlideFromLeft");
                           modeStatus->setText(QStringLiteral("Mode: %1, effective: %2, transition: %3")
                                               .arg(navigationDisplayModeName(navView->displayMode()),
                                                    navigationDisplayModeName(navView->effectiveDisplayMode()),
                                                    transition));
                       };

                       auto setMode = [navView, headerSection, mainSection, footerSection,
                                       leftButton, compactButton, minimalButton, topButton,
                                       updateStatus](NavigationView::DisplayMode mode) {
                           const bool top = mode == NavigationView::DisplayMode::Top;
                           const bool compactChrome = mode != NavigationView::DisplayMode::Left;
                           const Qt::Orientation orientation = top ? Qt::Horizontal : Qt::Vertical;
                           headerSection->setOrientation(orientation);
                           mainSection->setOrientation(orientation);
                           footerSection->setOrientation(orientation);
                           headerSection->setCompact(compactChrome);
                           mainSection->setCompact(compactChrome);
                           footerSection->setCompact(compactChrome);
                           navView->contentHost()->setTransitionEffect(
                               top ? StackContentHost::TransitionEffect::SlideFromBottom
                                   : StackContentHost::TransitionEffect::SlideFromLeft);
                           navView->setPaneOpen(mode == NavigationView::DisplayMode::Left || top);
                           navView->setDisplayMode(mode);
                           leftButton->setFluentStyle(mode == NavigationView::DisplayMode::Left ? Button::Accent : Button::Standard);
                           compactButton->setFluentStyle(mode == NavigationView::DisplayMode::LeftCompact ? Button::Accent : Button::Standard);
                           minimalButton->setFluentStyle(mode == NavigationView::DisplayMode::LeftMinimal ? Button::Accent : Button::Standard);
                           topButton->setFluentStyle(mode == NavigationView::DisplayMode::Top ? Button::Accent : Button::Standard);
                           updateStatus();
                       };
                       setMode(NavigationView::DisplayMode::Left);
                       QObject::connect(leftButton, &Button::clicked,
                                        navView, [setMode]() { setMode(NavigationView::DisplayMode::Left); });
                       QObject::connect(compactButton, &Button::clicked,
                                        navView, [setMode]() { setMode(NavigationView::DisplayMode::LeftCompact); });
                       QObject::connect(minimalButton, &Button::clicked,
                                        navView, [setMode]() { setMode(NavigationView::DisplayMode::LeftMinimal); });
                       QObject::connect(topButton, &Button::clicked,
                                        navView, [setMode]() { setMode(NavigationView::DisplayMode::Top); });

                       surface->layout()->addWidget(controls);
                       surface->layout()->addWidget(navView);
                       surface->layout()->addWidget(modeStatus);
                       surface->layout()->addWidget(routeStatus);
                       group->layout()->addWidget(surface);
                       return group;
                   }),

        makeSample(QStringLiteral("navigation-view-pane-metrics"),
                   QStringLiteral("PaneOpen and metrics"),
                   QStringLiteral("expandedPaneWidth, compactPaneWidth, topBarHeight, and paneOpen directly change the shell rectangles."),
                   QStringLiteral("navView->setAnimationEnabled(true);\n"
                                  "navView->setExpandedPaneWidth(180);\n"
                                  "navView->setCompactPaneWidth(52);\n"
                                  "navView->setTopBarHeight(52);\n"
                                  "auto showExpanded = [&] {\n"
                                  "    mainSection->setOrientation(Qt::Vertical);\n"
                                  "    mainSection->setCompact(false);\n"
                                  "    setNavigationPagesLeftInset(pages, 0);\n"
                                  "    navView->setDisplayMode(NavigationView::DisplayMode::Left);\n"
                                  "    navView->setPaneOpen(true);\n"
                                  "};\n"
                                  "auto showCompact = [&] {\n"
                                  "    mainSection->setOrientation(Qt::Vertical);\n"
                                  "    mainSection->setCompact(true);\n"
                                  "    setNavigationPagesLeftInset(pages, 0);\n"
                                  "    navView->setDisplayMode(NavigationView::DisplayMode::LeftCompact);\n"
                                  "    navView->setPaneOpen(false);\n"
                                  "};\n"
                                  "auto showMinimalOverlay = [&] {\n"
                                  "    mainSection->setOrientation(Qt::Vertical);\n"
                                  "    mainSection->setCompact(false);\n"
                                  "    setNavigationPagesLeftInset(pages, navView->expandedPaneWidth());\n"
                                  "    navView->setDisplayMode(NavigationView::DisplayMode::LeftMinimal);\n"
                                  "    navView->setPaneOpen(true);\n"
                                  "};\n"
                                  "auto showTop = [&] {\n"
                                  "    mainSection->setOrientation(Qt::Horizontal);\n"
                                  "    mainSection->setCompact(false);\n"
                                  "    setNavigationPagesLeftInset(pages, 0);\n"
                                  "    navView->setDisplayMode(NavigationView::DisplayMode::Top);\n"
                                  "    navView->setPaneOpen(true);\n"
                                  "};"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 8);
                       auto* surface = makeNavigationPreviewSurface(group);
                       auto* controls = horizontalGroup(surface, 6);
                       auto* expandedButton = makeControlButton(controls, QStringLiteral("Left 180"));
                       auto* compactButton = makeControlButton(controls, QStringLiteral("Compact 52"));
                       auto* minimalButton = makeControlButton(controls, QStringLiteral("Minimal overlay"));
                       auto* topButton = makeControlButton(controls, QStringLiteral("Top 52"));
                       for (Button* button : {expandedButton, compactButton, minimalButton, topButton})
                           controls->layout()->addWidget(button);

                       auto* navView = new NavigationView(surface);
                       navView->setFixedSize(620, 320);
                       navView->setAnimationEnabled(true);
                       navView->setExpandedPaneWidth(180);
                       navView->setCompactPaneWidth(52);
                       navView->setTopBarHeight(52);

                       auto* mainSection = new NavigationDemoSection({
                           {Typography::Icons::Home, QStringLiteral("Home"), 0},
                           {Typography::Icons::Document, QStringLiteral("Documents"), 2},
                           {Typography::Icons::Settings, QStringLiteral("Settings"), 5},
                       }, navView);
                       mainSection->setSelectedIndex(0);
                       navView->setMainChromeWidget(mainSection);
                       const QVector<NavigationDashboardPage*> pages = populateNavigationPages(navView->contentHost());

                       auto* modeStatus = makeStatusLabel(surface, QStringLiteral("expandedPaneWidth: 180, compactPaneWidth: 52, topBarHeight: 52"));
                       modeStatus->setFluentTypography(Typography::FontRole::Caption);
                       auto* routeStatus = makeStatusLabel(surface, QStringLiteral("Current page: Home"));
                       routeStatus->setFluentTypography(Typography::FontRole::Caption);
                       connectNavigationRoutes(nullptr, mainSection, nullptr, navView->contentHost(), routeStatus);

                       auto updateButtons = [expandedButton, compactButton, minimalButton, topButton](NavigationView::DisplayMode mode) {
                           expandedButton->setFluentStyle(mode == NavigationView::DisplayMode::Left ? Button::Accent : Button::Standard);
                           compactButton->setFluentStyle(mode == NavigationView::DisplayMode::LeftCompact ? Button::Accent : Button::Standard);
                           minimalButton->setFluentStyle(mode == NavigationView::DisplayMode::LeftMinimal ? Button::Accent : Button::Standard);
                           topButton->setFluentStyle(mode == NavigationView::DisplayMode::Top ? Button::Accent : Button::Standard);
                       };
                       auto applyMode = [navView, mainSection, pages, modeStatus, updateButtons](NavigationView::DisplayMode mode, bool paneOpen) {
                           const bool top = mode == NavigationView::DisplayMode::Top;
                           mainSection->setOrientation(top ? Qt::Horizontal : Qt::Vertical);
                           mainSection->setCompact(mode == NavigationView::DisplayMode::LeftCompact && !paneOpen);
                           setNavigationPagesLeftInset(pages,
                                                        mode == NavigationView::DisplayMode::LeftMinimal && paneOpen
                                                            ? navView->expandedPaneWidth()
                                                            : 0);
                           navView->setDisplayMode(mode);
                           navView->setPaneOpen(paneOpen);
                           updateButtons(mode);
                           modeStatus->setText(QStringLiteral("Mode: %1, paneOpen: %2, expanded: 180, compact: 52, topBar: 52")
                                               .arg(navigationDisplayModeName(navView->displayMode()),
                                                    navView->isPaneOpen() ? QStringLiteral("true") : QStringLiteral("false")));
                       };

                       applyMode(NavigationView::DisplayMode::Left, true);
                       QObject::connect(expandedButton, &Button::clicked,
                                        navView, [applyMode]() { applyMode(NavigationView::DisplayMode::Left, true); });
                       QObject::connect(compactButton, &Button::clicked,
                                        navView, [applyMode]() { applyMode(NavigationView::DisplayMode::LeftCompact, false); });
                       QObject::connect(minimalButton, &Button::clicked,
                                        navView, [applyMode]() { applyMode(NavigationView::DisplayMode::LeftMinimal, true); });
                       QObject::connect(topButton, &Button::clicked,
                                        navView, [applyMode]() { applyMode(NavigationView::DisplayMode::Top, true); });

                       surface->layout()->addWidget(controls);
                       surface->layout()->addWidget(navView);
                       surface->layout()->addWidget(modeStatus);
                       surface->layout()->addWidget(routeStatus);
                       group->layout()->addWidget(surface);
                       return group;
                   }),

        makeSample(QStringLiteral("navigation-view-auto-responsive"),
                   QStringLiteral("Auto responsive thresholds"),
                   QStringLiteral("DisplayMode::Auto resolves to Left, LeftCompact, or LeftMinimal from the widget width and configured thresholds."),
                   QStringLiteral("navView->setAnimationEnabled(true);\n"
                                  "navView->setDisplayMode(NavigationView::DisplayMode::Auto);\n"
                                  "navView->setExpandedPaneWidth(180);\n"
                                  "navView->setCompactPaneWidth(52);\n"
                                  "navView->setFixedSize(520, 300);\n"
                                  "auto resolveAutoAsLeft = [&] {\n"
                                  "    navView->setCompactModeThresholdWidth(260);\n"
                                  "    navView->setExpandedModeThresholdWidth(420);\n"
                                  "    navView->setPaneOpen(true);\n"
                                  "};\n"
                                  "auto resolveAutoAsCompact = [&] {\n"
                                  "    navView->setCompactModeThresholdWidth(340);\n"
                                  "    navView->setExpandedModeThresholdWidth(620);\n"
                                  "    navView->setPaneOpen(false);\n"
                                  "};\n"
                                  "auto resolveAutoAsMinimal = [&] {\n"
                                  "    navView->setCompactModeThresholdWidth(540);\n"
                                  "    navView->setExpandedModeThresholdWidth(680);\n"
                                  "    navView->setPaneOpen(false);\n"
                                  "};\n"
                                  "mainSection->onActivated = [host = navView->contentHost()](int pageIndex) {\n"
                                  "    const int direction = pageIndex >= host->currentIndex() ? 1 : -1;\n"
                                  "    host->setCurrentIndex(pageIndex, direction, true);\n"
                                  "};"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 8);
                       auto* surface = makeNavigationPreviewSurface(group);
                       auto* controls = horizontalGroup(surface, 6);
                       auto* leftAutoButton = makeControlButton(controls, QStringLiteral("Auto Left"));
                       auto* compactAutoButton = makeControlButton(controls, QStringLiteral("Auto Compact"));
                       auto* minimalAutoButton = makeControlButton(controls, QStringLiteral("Auto Minimal"));
                       for (Button* button : {leftAutoButton, compactAutoButton, minimalAutoButton})
                           controls->layout()->addWidget(button);

                       auto* navView = new NavigationView(surface);
                       navView->setAnimationEnabled(true);
                       navView->setDisplayMode(NavigationView::DisplayMode::Auto);
                       navView->setFixedSize(520, 300);
                       navView->setExpandedPaneWidth(180);
                       navView->setCompactPaneWidth(52);

                       auto* mainSection = new NavigationDemoSection({
                           {Typography::Icons::Home, QStringLiteral("Home"), 0},
                           {Typography::Icons::Document, QStringLiteral("Documents"), 2},
                           {Typography::Icons::Info, QStringLiteral("Help"), 4},
                       }, navView);
                       mainSection->setSelectedIndex(0);
                       navView->setMainChromeWidget(mainSection);
                       populateNavigationPages(navView->contentHost());

                       auto* modeStatus = makeStatusLabel(surface, QStringLiteral("Width: 520 px, effective: Left, thresholds: 260 / 420"));
                       modeStatus->setFluentTypography(Typography::FontRole::Caption);
                       auto* routeStatus = makeStatusLabel(surface, QStringLiteral("Current page: Home"));
                       routeStatus->setFluentTypography(Typography::FontRole::Caption);
                       connectNavigationRoutes(nullptr, mainSection, nullptr, navView->contentHost(), routeStatus);

                       auto updateButtons = [leftAutoButton, compactAutoButton, minimalAutoButton](NavigationView::DisplayMode mode) {
                           leftAutoButton->setFluentStyle(mode == NavigationView::DisplayMode::Left ? Button::Accent : Button::Standard);
                           compactAutoButton->setFluentStyle(mode == NavigationView::DisplayMode::LeftCompact ? Button::Accent : Button::Standard);
                           minimalAutoButton->setFluentStyle(mode == NavigationView::DisplayMode::LeftMinimal ? Button::Accent : Button::Standard);
                       };
                       auto applyAutoThresholds = [navView, mainSection, modeStatus, updateButtons](int compactThreshold,
                                                                                                    int expandedThreshold,
                                                                                                    bool paneOpen) {
                           if (expandedThreshold > navView->expandedModeThresholdWidth()) {
                               navView->setExpandedModeThresholdWidth(expandedThreshold);
                               navView->setCompactModeThresholdWidth(compactThreshold);
                           } else {
                               navView->setCompactModeThresholdWidth(compactThreshold);
                               navView->setExpandedModeThresholdWidth(expandedThreshold);
                           }
                           navView->setPaneOpen(paneOpen);
                           const auto effectiveMode = navView->effectiveDisplayMode();
                           mainSection->setCompact(effectiveMode != NavigationView::DisplayMode::Left);
                           modeStatus->setText(QStringLiteral("Width: 520 px, effective: %1, thresholds: %2 / %3")
                                                   .arg(navigationDisplayModeName(effectiveMode))
                                                   .arg(compactThreshold)
                                                   .arg(expandedThreshold));
                           updateButtons(effectiveMode);
                       };

                       applyAutoThresholds(260, 420, true);
                       QObject::connect(leftAutoButton, &Button::clicked,
                                        navView, [applyAutoThresholds]() { applyAutoThresholds(260, 420, true); });
                       QObject::connect(compactAutoButton, &Button::clicked,
                                        navView, [applyAutoThresholds]() { applyAutoThresholds(340, 620, false); });
                       QObject::connect(minimalAutoButton, &Button::clicked,
                                        navView, [applyAutoThresholds]() { applyAutoThresholds(540, 680, false); });

                       surface->layout()->addWidget(controls);
                       surface->layout()->addWidget(navView);
                       surface->layout()->addWidget(modeStatus);
                       surface->layout()->addWidget(routeStatus);
                       group->layout()->addWidget(surface);
                       return group;
                   }),

        makeSample(QStringLiteral("navigation-view-content-host"),
                   QStringLiteral("StackContentHost page routing"),
                   QStringLiteral("Navigation rows are app-owned; item activation selects pages inserted into NavigationView::contentHost()."),
                   QStringLiteral("StackContentHost* host = navView->contentHost();\n"
                                  "navView->setExpandedPaneWidth(180);\n"
                                  "populateNavigationPages(host);\n"
                                  "host->setTransitionEffect(StackContentHost::TransitionEffect::SlideFromLeft);\n"
                                  "mainSection->onActivated = [host](int pageIndex) {\n"
                                  "    const int direction = pageIndex >= host->currentIndex() ? 1 : -1;\n"
                                  "    host->setCurrentIndex(pageIndex, direction, true);\n"
                                  "};"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 8);
                       auto* surface = makeNavigationPreviewSurface(group);

                       auto* navView = new NavigationView(surface);
                       navView->setFixedSize(620, 320);
                       navView->setAnimationEnabled(true);
                       navView->setDisplayMode(NavigationView::DisplayMode::Left);
                       navView->setExpandedPaneWidth(180);

                       auto* mainSection = new NavigationDemoSection({
                           {Typography::Icons::Home, QStringLiteral("Home"), 0},
                           {Typography::Icons::Document, QStringLiteral("Documents"), 2},
                           {Typography::Icons::Settings, QStringLiteral("Settings"), 5},
                       }, navView);
                       mainSection->setSelectedIndex(0);
                       navView->setMainChromeWidget(mainSection);

                       StackContentHost* host = navView->contentHost();
                       populateNavigationPages(host);
                       host->setTransitionEffect(StackContentHost::TransitionEffect::SlideFromLeft);

                       auto* status = makeStatusLabel(surface, QStringLiteral("Current page: Home"));
                       status->setFluentTypography(Typography::FontRole::Caption);
                       connectNavigationRoutes(nullptr, mainSection, nullptr, host, status);

                       surface->layout()->addWidget(navView);
                       surface->layout()->addWidget(status);
                       group->layout()->addWidget(surface);
                       return group;
                   })
    };
}

QVector<GallerySample> pivotSamples()
{
    return {
        makeSample(QStringLiteral("pivot-basic"),
                   QStringLiteral("Pivot with hosted pages"),
                   QStringLiteral("Selecting a pivot item changes the external StackContentHost page; disabled items stay visible but cannot be selected."),
                   QStringLiteral("auto* pivot = new Pivot(this);\n"
                                  "pivot->addItem({\"All\", Typography::Icons::Mail});\n"
                                  "pivot->addItem({\"Unread\", Typography::Icons::Filter});\n"
                                  "pivot->addItem({\"Flagged\", Typography::Icons::Flag});\n"
                                  "pivot->addItem({\"Locked\", Typography::Icons::Lock, false});\n"
                                  "pivot->addItem({\"Mentions\", Typography::Icons::Contact});\n\n"
                                  "auto* host = new StackContentHost(this);\n"
                                  "for (int i = 0; i < pivot->itemCount(); ++i)\n"
                                  "    host->insertPage(i, createPage(pivot->itemAt(i).header));\n\n"
                                  "connect(pivot, &Pivot::currentChanged,\n"
                                  "        host, [host](int index) { host->setCurrentIndex(index, 0, true); });"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 8);
                       auto* pivot = new Pivot(group);
                       pivot->setFixedSize(540, 44);
                       const QVector<PivotItem> items{
                           PivotItem(QStringLiteral("All"), Typography::Icons::Mail),
                           PivotItem(QStringLiteral("Unread"), Typography::Icons::Filter),
                           PivotItem(QStringLiteral("Flagged"), Typography::Icons::Flag),
                           PivotItem(QStringLiteral("Locked"), Typography::Icons::Lock, false),
                           PivotItem(QStringLiteral("Mentions"), Typography::Icons::Contact)
                       };
                       for (const PivotItem& item : items)
                           pivot->addItem(item);
                       pivot->setSelectedIndex(0);

                       auto* host = new StackContentHost(group);
                       host->setFixedSize(540, 128);
                       for (int i = 0; i < items.size(); ++i) {
                           host->insertPage(i, makeHostPage(
                               host,
                               items.at(i).header,
                               i == 3
                                   ? QStringLiteral("This disabled pivot item remains visible but cannot be selected.")
                                   : QStringLiteral("CurrentChanged drives this StackContentHost page.")));
                       }
                       host->setCurrentIndex(0, 0, false);

                       Label* status = makeStatusLabel(group, QStringLiteral("Showing: All"));
                       QObject::connect(pivot, &Pivot::currentChanged,
                                        status, [status, pivot, host](int index) {
                                            if (index >= 0 && index < host->count())
                                                host->setCurrentIndex(index, index >= host->currentIndex() ? 1 : -1, true);
                                            status->setText(QStringLiteral("Showing: %1")
                                                                .arg(pivot->itemAt(index).header));
                                        });
                       group->layout()->addWidget(pivot);
                       group->layout()->addWidget(host);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("pivot-overflow-behavior"),
                   QStringLiteral("OverflowBehavior"),
                   QStringLiteral("ScrollButtons pages through hidden headers with arrows; MoreButton collapses hidden headers behind the ... button and emits their indexes."),
                   QStringLiteral("const QVector<PivotItem> items{\n"
                                  "    {\"All\", Typography::Icons::Mail},\n"
                                  "    {\"Unread\", Typography::Icons::Filter},\n"
                                  "    {\"Flagged\", Typography::Icons::Flag},\n"
                                  "    {\"Mentions\", Typography::Icons::Contact},\n"
                                  "    {\"Archive\", Typography::Icons::Storage},\n"
                                  "    {\"Long category\", Typography::Icons::Folder}\n"
                                  "};\n\n"
                                  "auto* scrollButtons = new Pivot(this);\n"
                                  "for (const PivotItem& item : items)\n"
                                  "    scrollButtons->addItem(item);\n"
                                  "scrollButtons->setOverflowBehavior(Pivot::OverflowBehavior::ScrollButtons);\n"
                                  "scrollButtons->setFixedWidth(420);\n"
                                  "// Hidden headers are reached with the left and right overflow arrows.\n\n"
                                  "auto* moreButton = new Pivot(this);\n"
                                  "for (const PivotItem& item : items)\n"
                                  "    moreButton->addItem(item);\n"
                                  "moreButton->setOverflowBehavior(Pivot::OverflowBehavior::MoreButton);\n"
                                  "moreButton->setFixedWidth(420);\n"
                                  "// Hidden headers are grouped behind the ... overflow button.\n"
                                  "connect(moreButton, &Pivot::overflowActivated,\n"
                                  "        this, [](const QVector<int>& hiddenIndexes) {\n"
                                  "            // hiddenIndexes are the headers behind the ... button.\n"
                                  "        });"),
                   [](QWidget* parent) {
                       auto* group = verticalGroup(parent, 10);
                       const QVector<PivotItem> items{
                           PivotItem(QStringLiteral("All"), Typography::Icons::Mail),
                           PivotItem(QStringLiteral("Unread"), Typography::Icons::Filter),
                           PivotItem(QStringLiteral("Flagged"), Typography::Icons::Flag),
                           PivotItem(QStringLiteral("Mentions"), Typography::Icons::Contact),
                           PivotItem(QStringLiteral("Archive"), Typography::Icons::Storage),
                           PivotItem(QStringLiteral("Long category"), Typography::Icons::Folder),
                           PivotItem(QStringLiteral("Settings"), Typography::Icons::Settings),
                           PivotItem(QStringLiteral("History"), Typography::Icons::History)
                       };

                       auto* status = makeStatusLabel(
                           group,
                           QStringLiteral("MoreButton groups hidden headers behind the ... button."));
                       status->setFluentTypography(Typography::FontRole::Caption);

                       auto addOverflowRow = [group, items, status](const QString& label,
                                                                    Pivot::OverflowBehavior behavior) {
                           auto* row = horizontalGroup(group, 10);
                           auto* rowLabel = new Label(label, row);
                           rowLabel->setFluentTypography(Typography::FontRole::Caption);
                           rowLabel->setFixedWidth(96);

                           auto* pivot = new Pivot(row);
                           pivot->setFixedSize(420, 44);
                           pivot->setOverflowBehavior(behavior);
                           for (const PivotItem& item : items)
                               pivot->addItem(item);
                           pivot->setSelectedIndex(0);

                           if (behavior == Pivot::OverflowBehavior::MoreButton) {
                               QObject::connect(pivot, &Pivot::overflowActivated,
                                                status, [status, pivot](const QVector<int>& hiddenIndexes) {
                                                    QStringList headers;
                                                    for (int index : hiddenIndexes)
                                                        headers.append(pivot->itemAt(index).header);
                                                    status->setText(QStringLiteral("MoreButton contains: %1")
                                                                        .arg(headers.join(QStringLiteral(", "))));
                                                });
                           }

                           row->layout()->addWidget(rowLabel);
                           row->layout()->addWidget(pivot);
                           group->layout()->addWidget(row);
                       };

                       addOverflowRow(QStringLiteral("ScrollButtons"), Pivot::OverflowBehavior::ScrollButtons);
                       addOverflowRow(QStringLiteral("MoreButton"), Pivot::OverflowBehavior::MoreButton);
                       group->layout()->addWidget(status);
                       return group;
                   })
    };
}

QVector<GallerySample> selectorBarSamples()
{
    return {
        makeSample(QStringLiteral("selector-bar-basic"),
                   QStringLiteral("SelectorBar with hosted pages"),
                   QStringLiteral("currentChanged swaps the external page; selectionChanged exposes the selected item's data payload."),
                   QStringLiteral("auto* selector = new SelectorBar(this);\n"
                                  "selector->addItem(SelectorBarItem(\"Inbox\", Typography::Icons::Mail,\n"
                                  "                                 true, true, \"inbox\"));\n"
                                  "selector->addItem(SelectorBarItem(\"Calendar\", Typography::Icons::Calendar,\n"
                                  "                                 true, true, \"calendar\"));\n"
                                  "selector->addItem(SelectorBarItem(\"Settings\", Typography::Icons::Settings,\n"
                                  "                                 true, true, \"settings\"));\n\n"
                                  "auto* host = new StackContentHost(this);\n"
                                  "for (int i = 0; i < selector->itemCount(); ++i)\n"
                                  "    host->insertPage(i, createPage(selector->itemAt(i).text));\n\n"
                                  "connect(selector, &SelectorBar::currentChanged,\n"
                                  "        host, [host](int index) { host->setCurrentIndex(index, 0, true); });\n"
                                  "connect(selector, &SelectorBar::selectionChanged,\n"
                                  "        this, [](int, const SelectorBarItem& item) {\n"
                                  "            const auto key = item.data.toString();\n"
                                  "        });"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 8);
                       auto* selector = new SelectorBar(group);
                       selector->setFixedSize(540, 44);
                       selector->addItem(SelectorBarItem(QStringLiteral("Inbox"), Typography::Icons::Mail,
                                                         true, true, QStringLiteral("inbox")));
                       selector->addItem(SelectorBarItem(QStringLiteral("Calendar"), Typography::Icons::Calendar,
                                                         true, true, QStringLiteral("calendar")));
                       selector->addItem(SelectorBarItem(QStringLiteral("Settings"), Typography::Icons::Settings,
                                                         true, true, QStringLiteral("settings")));
                       selector->setSelectedIndex(0);

                       auto* host = new StackContentHost(group);
                       host->setFixedSize(540, 128);
                       for (int i = 0; i < selector->itemCount(); ++i) {
                           const SelectorBarItem item = selector->itemAt(i);
                           host->insertPage(i, makeHostPage(
                               host,
                               item.text,
                               QStringLiteral("selectionChanged carries data key \"%1\".")
                                   .arg(item.data.toString())));
                       }
                       host->setCurrentIndex(0, 0, false);

                       Label* status = makeStatusLabel(group, QStringLiteral("View: Inbox, data: inbox"));
                       QObject::connect(selector, &SelectorBar::currentChanged,
                                        host, [host](int index) {
                                            if (index >= 0 && index < host->count())
                                                host->setCurrentIndex(index, index >= host->currentIndex() ? 1 : -1, true);
                                        });
                       QObject::connect(selector, &SelectorBar::selectionChanged,
                                        status, [status](int, const SelectorBarItem& item) {
                                            status->setText(QStringLiteral("View: %1, data: %2")
                                                                .arg(item.text, item.data.toString()));
                                        });

                       group->layout()->addWidget(selector);
                       group->layout()->addWidget(host);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("selector-bar-item-state"),
                   QStringLiteral("SelectorBar item state"),
                   QStringLiteral("Items can be disabled, hidden, selected, and carry data without owning the page content."),
                   QStringLiteral("auto* selector = new SelectorBar(this);\n"
                                  "selector->addItem(SelectorBarItem(\"Overview\", Typography::Icons::Home,\n"
                                  "                                 true, true, \"overview\"));\n"
                                  "selector->addItem(SelectorBarItem(\"Sample code\", Typography::Icons::Document,\n"
                                  "                                 true, false, \"code\"));\n"
                                  "selector->addItem(SelectorBarItem(\"Disabled\", Typography::Icons::Lock,\n"
                                  "                                 false, true, \"disabled\"));\n"
                                  "selector->addItem(SelectorBarItem(\"Settings\", Typography::Icons::Settings,\n"
                                  "                                 true, true, \"settings\"));\n\n"
                                  "selector->setItemVisible(1, false);  // hidden until the button reveals it\n"
                                  "selector->setItemEnabled(2, false);  // visible but not selectable\n"
                                  "selector->setItemSelected(0, true);\n\n"
                                  "connect(showCodeButton, &Button::clicked,\n"
                                  "        selector, [selector] { selector->setItemVisible(1, true); });"),
                   [](QWidget* parent) {
                       auto* group = verticalGroup(parent, 8);
                       auto* selector = new SelectorBar(group);
                       selector->setFixedSize(540, 44);
                       selector->addItem(SelectorBarItem(QStringLiteral("Overview"), Typography::Icons::Home,
                                                         true, true, QStringLiteral("overview")));
                       selector->addItem(SelectorBarItem(QStringLiteral("Sample code"), Typography::Icons::Document,
                                                         true, false, QStringLiteral("code")));
                       selector->addItem(SelectorBarItem(QStringLiteral("Disabled"), Typography::Icons::Lock,
                                                         false, true, QStringLiteral("disabled")));
                       selector->addItem(SelectorBarItem(QStringLiteral("Settings"), Typography::Icons::Settings,
                                                         true, true, QStringLiteral("settings")));
                       selector->setItemSelected(0, true);

                       auto* controls = horizontalGroup(group, 8);
                       auto* showCodeButton = makeControlButton(controls, QStringLiteral("Show code"));
                       controls->layout()->addWidget(showCodeButton);
                       auto* status = makeStatusLabel(group, QStringLiteral("Sample code is hidden; Disabled is visible but not selectable."));
                       status->setFluentTypography(Typography::FontRole::Caption);

                       QObject::connect(showCodeButton, &Button::clicked,
                                        selector, [selector, showCodeButton, status, codeVisible = false]() mutable {
                                            codeVisible = !codeVisible;
                                            selector->setItemVisible(1, codeVisible);
                                            showCodeButton->setText(codeVisible ? QStringLiteral("Hide code")
                                                                                : QStringLiteral("Show code"));
                                            status->setText(codeVisible
                                                                ? QStringLiteral("Sample code is visible and selectable.")
                                                                : QStringLiteral("Sample code is hidden; Disabled is visible but not selectable."));
                                        });
                       QObject::connect(selector, &SelectorBar::selectionChanged,
                                        status, [status](int, const SelectorBarItem& item) {
                                            status->setText(QStringLiteral("Selected %1, data: %2")
                                                                .arg(item.text, item.data.toString()));
                                        });

                       group->layout()->addWidget(selector);
                       group->layout()->addWidget(controls);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("selector-bar-overflow-behavior"),
                   QStringLiteral("SelectorBar OverflowBehavior"),
                   QStringLiteral("ScrollButtons pages through hidden items with arrows; MoreButton groups hidden items behind the ... button."),
                   QStringLiteral("const QVector<SelectorBarItem> items{\n"
                                  "    {\"Category 1\", Typography::Icons::Folder},\n"
                                  "    {\"Category 2\", Typography::Icons::Document},\n"
                                  "    {\"Category 3\", Typography::Icons::Folder},\n"
                                  "    {\"Category 4\", Typography::Icons::Document},\n"
                                  "    {\"Category 5\", Typography::Icons::Folder},\n"
                                  "    {\"Category 6\", Typography::Icons::Document}\n"
                                  "};\n\n"
                                  "auto* scrollButtons = new SelectorBar(this);\n"
                                  "for (const SelectorBarItem& item : items)\n"
                                  "    scrollButtons->addItem(item);\n"
                                  "scrollButtons->setOverflowBehavior(SelectorBar::OverflowBehavior::ScrollButtons);\n"
                                  "scrollButtons->setFixedWidth(360);\n\n"
                                  "auto* moreButton = new SelectorBar(this);\n"
                                  "for (const SelectorBarItem& item : items)\n"
                                  "    moreButton->addItem(item);\n"
                                  "moreButton->setOverflowBehavior(SelectorBar::OverflowBehavior::MoreButton);\n"
                                  "moreButton->setFixedWidth(360);\n"
                                  "connect(moreButton, &SelectorBar::overflowActivated,\n"
                                  "        this, [](const QVector<int>& hiddenIndexes) {\n"
                                  "            // hiddenIndexes are the items behind the ... button.\n"
                                  "        });"),
                   [](QWidget* parent) {
                       auto* group = verticalGroup(parent, 10);
                       QVector<SelectorBarItem> items;
                       for (int i = 0; i < 6; ++i) {
                           items.append(SelectorBarItem(
                               QStringLiteral("Category %1").arg(i + 1),
                               i % 2 == 0 ? Typography::Icons::Folder : Typography::Icons::Document));
                       }

                       auto* status = makeStatusLabel(group, QStringLiteral("Click the MoreButton row's ... to list hidden items."));
                       status->setFluentTypography(Typography::FontRole::Caption);

                       auto addOverflowRow = [group, items, status](const QString& label,
                                                                    SelectorBar::OverflowBehavior behavior) {
                           auto* row = horizontalGroup(group, 10);
                           auto* rowLabel = new Label(label, row);
                           rowLabel->setFluentTypography(Typography::FontRole::Caption);
                           rowLabel->setFixedWidth(96);

                           auto* selector = new SelectorBar(row);
                           selector->setFixedSize(360, 44);
                           selector->setOverflowBehavior(behavior);
                           for (const SelectorBarItem& item : items)
                               selector->addItem(item);
                           selector->setSelectedIndex(0);

                           if (behavior == SelectorBar::OverflowBehavior::MoreButton) {
                               QObject::connect(selector, &SelectorBar::overflowActivated,
                                                status, [status, selector](const QVector<int>& hiddenIndexes) {
                                                    QStringList itemTexts;
                                                    for (int index : hiddenIndexes)
                                                        itemTexts.append(selector->itemAt(index).text);
                                                    status->setText(QStringLiteral("MoreButton contains: %1")
                                                                        .arg(itemTexts.join(QStringLiteral(", "))));
                                                });
                           }

                           row->layout()->addWidget(rowLabel);
                           row->layout()->addWidget(selector);
                           group->layout()->addWidget(row);
                       };

                       addOverflowRow(QStringLiteral("ScrollButtons"), SelectorBar::OverflowBehavior::ScrollButtons);
                       addOverflowRow(QStringLiteral("MoreButton"), SelectorBar::OverflowBehavior::MoreButton);
                       group->layout()->addWidget(status);
                       return group;
                   })
    };
}

QVector<GallerySample> tabViewSamples()
{
    return {
        makeSample(QStringLiteral("tab-view-hosted-pages"),
                   QStringLiteral("TabView with hosted pages"),
                   QStringLiteral("currentChanged selects the external StackContentHost page; tabMoved keeps tab order and page order aligned."),
                   QStringLiteral("auto* tabs = new TabView(this);\n"
                                  "tabs->setTabWidthMode(TabView::TabWidthMode::SizeToContent);\n"
                                  "tabs->setTabReorderEnabled(true);\n"
                                  "tabs->setTabsClosable(false);\n"
                                  "tabs->setAddTabButtonVisible(false);\n"
                                  "tabs->addTab(TabViewItem(\"Home\", Typography::Icons::Home));\n"
                                  "tabs->addTab(TabViewItem(\"Details\", Typography::Icons::Document));\n"
                                  "tabs->addTab(TabViewItem(\"Activity\", Typography::Icons::Calendar));\n\n"
                                  "auto* host = new StackContentHost(this);\n"
                                  "for (int i = 0; i < tabs->tabCount(); ++i)\n"
                                  "    host->insertPage(i, createPage(tabs->tabAt(i).text));\n\n"
                                  "connect(tabs, &TabView::currentChanged,\n"
                                  "        host, [host](int index) { host->setCurrentIndex(index, 0, true); });\n"
                                  "connect(tabs, &TabView::tabMoved,\n"
                                  "        host, [host](int from, int to) { host->movePage(from, to); });"),
                   [](QWidget* parent) {
                       auto* container = verticalGroup(parent, 8);
                       auto* surface = new NavigationSampleSurface(container);
                       surface->setFixedSize(560, 186);
                       auto* surfaceLayout = new QVBoxLayout(surface);
                       surfaceLayout->setContentsMargins(0, 0, 0, 0);
                       surfaceLayout->setSpacing(0);

                       auto* tabView = new TabView(surface);
                       tabView->setFixedSize(560, 40);
                       tabView->setTabWidthMode(TabView::TabWidthMode::SizeToContent);
                       tabView->setTabReorderEnabled(true);
                       tabView->setCloseButtonOverlayMode(TabView::CloseButtonOverlayMode::OnHover);
                       tabView->setAddTabButtonVisible(false);
                       tabView->setTabsClosable(false);

                       auto* contentHost = new StackContentHost(surface);
                       contentHost->setFixedSize(560, 146);
                       auto makePage = [contentHost](const QString& title) {
                           return makeHostPage(
                               contentHost,
                               title,
                               QStringLiteral("%1 content hosted by the selected tab.").arg(title));
                       };
                       const QVector<TabViewItem> initialTabs{
                           TabViewItem(QStringLiteral("Home"), Typography::Icons::Home),
                           TabViewItem(QStringLiteral("Details"), Typography::Icons::Document),
                           TabViewItem(QStringLiteral("Activity"), Typography::Icons::Calendar)
                       };
                       int pageIndex = 0;
                       for (const TabViewItem& tab : initialTabs) {
                           tabView->addTab(tab);
                           contentHost->insertPage(pageIndex, makePage(tab.text));
                           ++pageIndex;
                       }
                       tabView->setSelectedIndex(0);
                       contentHost->setCurrentIndex(0, 0, false);
                       auto* status = makeStatusLabel(container, QStringLiteral("Selected tab: Home"));
                       QObject::connect(tabView, &TabView::currentChanged,
                                        contentHost, [contentHost, status, tabView](int index) {
                                            if (index >= 0 && index < contentHost->count())
                                                contentHost->setCurrentIndex(index);
                                            if (index >= 0)
                                                status->setText(QStringLiteral("Selected tab: %1")
                                                                    .arg(tabView->tabAt(index).text));
                                        });
                       QObject::connect(tabView, &TabView::tabMoved,
                                        contentHost, [contentHost, status, tabView](int from, int to) {
                                            contentHost->movePage(from, to);
                                            const int current = tabView->selectedIndex();
                                            if (current >= 0 && current < contentHost->count())
                                                contentHost->setCurrentIndex(current, 0, false);
                                            status->setText(QStringLiteral("Moved tab %1 to %2")
                                                                .arg(from + 1)
                                                                .arg(to + 1));
                                        });

                       surfaceLayout->addWidget(tabView);
                       surfaceLayout->addWidget(contentHost);
                       container->layout()->addWidget(surface);
                       container->layout()->addWidget(status);
                       return container;
                   }),
        makeSample(QStringLiteral("tab-view-add-close"),
                   QStringLiteral("Add, close, and tab state"),
                   QStringLiteral("The add button raises addTabRequested; close buttons raise tabCloseRequested; per-tab closable and enabled state still apply."),
                   QStringLiteral("auto* tabs = new TabView(this);\n"
                                  "tabs->setCloseButtonOverlayMode(TabView::CloseButtonOverlayMode::Always);\n"
                                  "tabs->setAddTabButtonVisible(true);\n"
                                  "tabs->setTabsClosable(true);\n"
                                  "tabs->addTab(TabViewItem(\"Home\", Typography::Icons::Home, false));\n"
                                  "tabs->addTab(TabViewItem(\"Draft\", Typography::Icons::Document));\n"
                                  "tabs->addTab(TabViewItem(\"Review\", Typography::Icons::Edit));\n"
                                  "tabs->addTab(TabViewItem(\"Disabled\", Typography::Icons::Lock, true, false));\n\n"
                                  "connect(tabs, &TabView::addTabRequested,\n"
                                  "        tabs, [tabs] {\n"
                                  "            const int index = tabs->addTab(TabViewItem(\"Document\", Typography::Icons::Document));\n"
                                  "            tabs->setSelectedIndex(index);\n"
                                  "        });\n"
                                  "connect(tabs, &TabView::tabCloseRequested,\n"
                                  "        tabs, [tabs](int index) { tabs->closeTab(index); });"),
                   [](QWidget* parent) {
                       auto* container = verticalGroup(parent, 8);
                       auto* tabView = new TabView(container);
                       tabView->setFixedSize(560, 40);
                       tabView->setTabWidthMode(TabView::TabWidthMode::SizeToContent);
                       tabView->setCloseButtonOverlayMode(TabView::CloseButtonOverlayMode::Always);
                       tabView->setAddTabButtonVisible(true);
                       tabView->setTabsClosable(true);
                       tabView->addTab(TabViewItem(QStringLiteral("Home"), Typography::Icons::Home, false));
                       tabView->addTab(TabViewItem(QStringLiteral("Draft"), Typography::Icons::Document));
                       tabView->addTab(TabViewItem(QStringLiteral("Review"), Typography::Icons::Edit));
                       tabView->addTab(TabViewItem(QStringLiteral("Disabled"), Typography::Icons::Lock, true, false));
                       tabView->setSelectedIndex(0);

                       auto* status = makeStatusLabel(container, QStringLiteral("Home is pinned; Disabled is not selectable."));
                       status->setFluentTypography(Typography::FontRole::Caption);
                       QObject::connect(tabView, &TabView::addTabRequested,
                                        tabView, [tabView, status]() {
                                            static int documentNumber = 0;
                                            const QString title = QStringLiteral("Document %1").arg(++documentNumber);
                                            const int index = tabView->addTab(TabViewItem(title, Typography::Icons::Document));
                                            tabView->setSelectedIndex(index);
                                            status->setText(QStringLiteral("Added %1").arg(title));
                                        });
                       QObject::connect(tabView, &TabView::tabCloseRequested,
                                        tabView, [tabView, status](int index) {
                                            const QString title = tabView->tabAt(index).text;
                                            if (tabView->closeTab(index))
                                                status->setText(QStringLiteral("Closed %1").arg(title));
                                            else
                                                status->setText(QStringLiteral("%1 cannot be closed").arg(title));
                                        });
                       QObject::connect(tabView, &TabView::currentChanged,
                                        status, [status, tabView](int index) {
                                            if (index >= 0)
                                                status->setText(QStringLiteral("Selected %1").arg(tabView->tabAt(index).text));
                                        });

                       container->layout()->addWidget(tabView);
                       container->layout()->addWidget(status);
                       return container;
                   }),
        makeSample(QStringLiteral("tab-view-width-modes"),
                   QStringLiteral("TabWidthMode"),
                   QStringLiteral("Equal gives tabs the same width; SizeToContent follows header text; Compact collapses inactive tabs."),
                   QStringLiteral("auto addTabs = [](TabView* tabs) {\n"
                                  "    tabs->addTab(TabViewItem(\"Home\", Typography::Icons::Home));\n"
                                  "    tabs->addTab(TabViewItem(\"Long document\", Typography::Icons::Document));\n"
                                  "    tabs->addTab(TabViewItem(\"Activity\", Typography::Icons::Calendar));\n"
                                  "    tabs->setTabsClosable(false);\n"
                                  "    tabs->setAddTabButtonVisible(false);\n"
                                  "};\n\n"
                                  "auto* equal = new TabView(this);\n"
                                  "addTabs(equal);\n"
                                  "equal->setTabWidthMode(TabView::TabWidthMode::Equal);\n"
                                  "auto* sizeToContent = new TabView(this);\n"
                                  "addTabs(sizeToContent);\n"
                                  "sizeToContent->setTabWidthMode(TabView::TabWidthMode::SizeToContent);\n"
                                  "auto* compact = new TabView(this);\n"
                                  "addTabs(compact);\n"
                                  "compact->setTabWidthMode(TabView::TabWidthMode::Compact);\n"
                                  "compact->setSelectedIndex(1);"),
                   [](QWidget* parent) {
                       auto* group = verticalGroup(parent, 10);
                       auto addModeRow = [group](const QString& label,
                                                 TabView::TabWidthMode mode,
                                                 int selectedIndex = 0) {
                           auto* row = horizontalGroup(group, 10);
                           auto* rowLabel = new Label(label, row);
                           rowLabel->setFluentTypography(Typography::FontRole::Caption);
                           rowLabel->setFixedWidth(104);

                           auto* tabs = new TabView(row);
                           tabs->setFixedSize(430, 40);
                           tabs->setTabWidthMode(mode);
                           tabs->setTabsClosable(false);
                           tabs->setAddTabButtonVisible(false);
                           tabs->addTab(TabViewItem(QStringLiteral("Home"), Typography::Icons::Home));
                           tabs->addTab(TabViewItem(QStringLiteral("Long document"), Typography::Icons::Document));
                           tabs->addTab(TabViewItem(QStringLiteral("Activity"), Typography::Icons::Calendar));
                           tabs->setSelectedIndex(selectedIndex);

                           row->layout()->addWidget(rowLabel);
                           row->layout()->addWidget(tabs);
                           group->layout()->addWidget(row);
                       };

                       addModeRow(QStringLiteral("Equal"), TabView::TabWidthMode::Equal);
                       addModeRow(QStringLiteral("SizeToContent"), TabView::TabWidthMode::SizeToContent);
                       addModeRow(QStringLiteral("Compact"), TabView::TabWidthMode::Compact, 1);
                       return group;
                   }),
        makeSample(QStringLiteral("tab-view-close-button-modes"),
                   QStringLiteral("CloseButtonOverlayMode"),
                   QStringLiteral("Auto follows the selected tab, OnHover reveals close buttons on hover, and Always reserves close affordances."),
                   QStringLiteral("auto addTabs = [](TabView* tabs) {\n"
                                  "    tabs->setAddTabButtonVisible(false);\n"
                                  "    tabs->addTab(TabViewItem(\"Primary\", Typography::Icons::AppIconDefault));\n"
                                  "    tabs->addTab(TabViewItem(\"Reference\", Typography::Icons::Document));\n"
                                  "    tabs->addTab(TabViewItem(\"Pinned\", Typography::Icons::Pin, false));\n"
                                  "};\n\n"
                                  "auto* autoClose = new TabView(this);\n"
                                  "addTabs(autoClose);\n"
                                  "autoClose->setCloseButtonOverlayMode(TabView::CloseButtonOverlayMode::Auto);\n\n"
                                  "auto* hoverClose = new TabView(this);\n"
                                  "addTabs(hoverClose);\n"
                                  "hoverClose->setCloseButtonOverlayMode(TabView::CloseButtonOverlayMode::OnHover);\n\n"
                                  "auto* alwaysClose = new TabView(this);\n"
                                  "addTabs(alwaysClose);\n"
                                  "alwaysClose->setCloseButtonOverlayMode(TabView::CloseButtonOverlayMode::Always);"),
                   [](QWidget* parent) {
                       auto* group = verticalGroup(parent, 10);
                       auto addCloseModeRow = [group](const QString& label,
                                                      TabView::CloseButtonOverlayMode mode) {
                           auto* row = horizontalGroup(group, 10);
                           auto* rowLabel = new Label(label, row);
                           rowLabel->setFluentTypography(Typography::FontRole::Caption);
                           rowLabel->setFixedWidth(104);

                           auto* tabs = new TabView(row);
                           tabs->setFixedSize(430, 40);
                           tabs->setTabWidthMode(TabView::TabWidthMode::SizeToContent);
                           tabs->setCloseButtonOverlayMode(mode);
                           tabs->setAddTabButtonVisible(false);
                           tabs->addTab(TabViewItem(QStringLiteral("Primary"), Typography::Icons::AppIconDefault));
                           tabs->addTab(TabViewItem(QStringLiteral("Reference"), Typography::Icons::Document));
                           tabs->addTab(TabViewItem(QStringLiteral("Pinned"), Typography::Icons::Pin, false));

                           row->layout()->addWidget(rowLabel);
                           row->layout()->addWidget(tabs);
                           group->layout()->addWidget(row);
                       };

                       addCloseModeRow(QStringLiteral("Auto"), TabView::CloseButtonOverlayMode::Auto);
                       addCloseModeRow(QStringLiteral("OnHover"), TabView::CloseButtonOverlayMode::OnHover);
                       addCloseModeRow(QStringLiteral("Always"), TabView::CloseButtonOverlayMode::Always);
                       return group;
                   }),
        makeSample(QStringLiteral("tab-view-overflow-reorder"),
                   QStringLiteral("Overflow and reordering"),
                   QStringLiteral("SizeToContent tabs overflow into arrow buttons; enabling reordering emits tabMoved when tabs are dragged."),
                   QStringLiteral("auto* tabs = new TabView(this);\n"
                                  "tabs->setTabWidthMode(TabView::TabWidthMode::SizeToContent);\n"
                                  "tabs->setTabReorderEnabled(true);\n"
                                  "tabs->setTabsClosable(false);\n"
                                  "tabs->setAddTabButtonVisible(false);\n"
                                  "tabs->setFixedWidth(360);\n"
                                  "for (int i = 1; i <= 8; ++i)\n"
                                  "    tabs->addTab(TabViewItem(QString(\"Document %1 with longer title\").arg(i),\n"
                                  "                             Typography::Icons::Document));\n"
                                  "connect(tabs, &TabView::tabMoved,\n"
                                  "        this, [](int from, int to) { /* keep external page order aligned */ });"),
                   [](QWidget* parent) {
                       auto* group = verticalGroup(parent, 8);
                       auto* tabs = new TabView(group);
                       tabs->setFixedSize(360, 40);
                       tabs->setTabWidthMode(TabView::TabWidthMode::SizeToContent);
                       tabs->setTabReorderEnabled(true);
                       tabs->setTabsClosable(false);
                       tabs->setAddTabButtonVisible(false);
                       for (int i = 1; i <= 8; ++i) {
                           tabs->addTab(TabViewItem(QStringLiteral("Document %1 with longer title").arg(i),
                                                    Typography::Icons::Document));
                       }
                       tabs->setSelectedIndex(5);
                       auto* status = makeStatusLabel(group, QStringLiteral("Selected: Document 6 with longer title"));
                       status->setFluentTypography(Typography::FontRole::Caption);
                       QObject::connect(tabs, &TabView::currentChanged,
                                        status, [status, tabs](int index) {
                                            if (index >= 0)
                                                status->setText(QStringLiteral("Selected: %1").arg(tabs->tabAt(index).text));
                                        });
                       QObject::connect(tabs, &TabView::tabMoved,
                                        status, [status](int from, int to) {
                                            status->setText(QStringLiteral("Moved tab %1 to %2")
                                                                .arg(from + 1)
                                                                .arg(to + 1));
                                        });

                       group->layout()->addWidget(tabs);
                       group->layout()->addWidget(status);
                       return group;
                   })
    };
}

} // namespace

QVector<GallerySample> navigationSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("breadcrumb"))
        return breadcrumbSamples();
    if (routeId == QStringLiteral("navigation-view"))
        return navigationViewSamples();
    if (routeId == QStringLiteral("pivot"))
        return pivotSamples();
    if (routeId == QStringLiteral("selector-bar"))
        return selectorBarSamples();
    if (routeId == QStringLiteral("tab-view"))
        return tabViewSamples();
    return {};
}

} // namespace fluent::gallery
