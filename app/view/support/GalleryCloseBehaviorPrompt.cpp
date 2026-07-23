#include "GalleryCloseBehaviorPrompt.h"

#include <functional>

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "components/foundation/QMLPlus.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "view/support/GalleryCloseBehaviorUi.h"

namespace fluent::gallery {
namespace {

constexpr int kCloseBehaviorContentWidth = 300;
constexpr QSize kCloseBehaviorDialogSize(380, 288);
constexpr int kChoiceRowHeight = 42;
constexpr int kChoiceRowPadding = 8;
constexpr int kChoiceTextGap = 8;
constexpr int kChoiceIconSize = 16;
constexpr int kChoiceTextTop = 3;

class CloseBehaviorChoiceRow final : public QWidget, public FluentElement {
public:
    CloseBehaviorChoiceRow(GallerySettings::CloseBehavior behavior,
                           const QString& glyph,
                           const QString& title,
                           const QString& description,
                           QWidget* parent)
        : QWidget(parent)
        , m_behavior(behavior)
        , m_glyph(glyph)
    {
        setObjectName(QStringLiteral("galleryCloseBehaviorRow%1")
                          .arg(static_cast<int>(behavior)));
        setAccessibleName(title);
        setAccessibleDescription(description);
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(kChoiceRowHeight);

        auto* layout = new AnchorLayout(this);
        using Edge = AnchorLayout::Edge;

        m_iconSlot = new QWidget(this);
        m_iconSlot->setFixedSize(kChoiceIconSize, kChoiceIconSize);
        m_iconSlot->setAttribute(Qt::WA_TransparentForMouseEvents);

        m_title = new fluent::textfields::Label(title, this);
        m_title->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_title->setFluentTypography(Typography::FontRole::BodyStrong);

        m_description = new fluent::textfields::Label(description, this);
        m_description->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_description->setFluentTypography(Typography::FontRole::Caption);
        m_description->setWordWrap(false);

        AnchorLayout::Anchors iconAnchors;
        iconAnchors.left = {this, Edge::Left, kChoiceRowPadding};
        iconAnchors.verticalCenter = {this, Edge::VCenter, 0};
        layout->addAnchoredWidget(m_iconSlot, iconAnchors);

        // The row itself carries selection (accent fill + accent icon), so no
        // separate radio control is needed; text spans to the row's right edge.
        // zh_CN: 选中态由整行承载（强调色填充 + 强调色图标），无需独立单选按钮，文本延伸到行右缘。
        AnchorLayout::Anchors titleAnchors;
        titleAnchors.left = {m_iconSlot, Edge::Right, kChoiceTextGap};
        titleAnchors.right = {this, Edge::Right, -kChoiceRowPadding};
        titleAnchors.top = {this, Edge::Top, kChoiceTextTop};
        layout->addAnchoredWidget(m_title, titleAnchors);

        AnchorLayout::Anchors descriptionAnchors;
        descriptionAnchors.left = {m_title, Edge::Left, 0};
        descriptionAnchors.right = {m_title, Edge::Right, 0};
        descriptionAnchors.top = {m_title, Edge::Bottom, 0};
        layout->addAnchoredWidget(m_description, descriptionAnchors);

        onThemeUpdated();
    }

    GallerySettings::CloseBehavior behavior() const { return m_behavior; }

    bool isSelected() const { return m_selected; }

    void setSelected(bool selected)
    {
        if (m_selected == selected)
            return;
        m_selected = selected;
        update();
    }

    void setOnActivate(std::function<void()> callback) { m_onActivate = std::move(callback); }

    void onThemeUpdated() override
    {
        if (m_title)
            m_title->onThemeUpdated();
        if (m_description) {
            m_description->onThemeUpdated();
            QPalette palette = m_description->palette();
            palette.setColor(QPalette::WindowText, themeColors().textSecondary);
            m_description->setPalette(palette);
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const auto colors = themeColors();
        const bool selected = m_selected;
        const QRectF rowRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

        QColor fill = Qt::transparent;
        if (selected) {
            fill = colors.accentDefault;
            fill.setAlpha(currentTheme() == Dark ? 28 : 16);
        } else if (m_pressed) {
            fill = colors.subtleTertiary;
        } else if (m_hovered) {
            fill = colors.subtleSecondary;
        }

        painter.setBrush(fill);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(rowRect, 6, 6);

        const QRectF iconRect = QRectF(m_iconSlot->geometry()).adjusted(0.5, 0.5, -0.5, -0.5);
        QFont iconFont = Typography::Icons::font(Typography::IconSize::Compact);
        painter.setFont(iconFont);
        painter.setPen(selected ? colors.textAccentPrimary : colors.textSecondary);
        painter.drawText(iconRect, Qt::AlignCenter, m_glyph);
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
        const bool activate = event->button() == Qt::LeftButton
            && m_pressed && rect().contains(event->pos());
        m_pressed = false;
        update();
        if (activate) {
            if (m_onActivate)
                m_onActivate();
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

private:
    GallerySettings::CloseBehavior m_behavior;
    QString m_glyph;
    QWidget* m_iconSlot = nullptr;
    fluent::textfields::Label* m_title = nullptr;
    fluent::textfields::Label* m_description = nullptr;
    std::function<void()> m_onActivate;
    bool m_selected = false;
    bool m_hovered = false;
    bool m_pressed = false;
};

} // namespace

CloseBehaviorPromptContent::CloseBehaviorPromptContent(
    GallerySettings::CloseBehavior current,
    QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("galleryCloseBehaviorPromptContent"));
    setFixedWidth(kCloseBehaviorContentWidth);

    m_selected = current;

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    addChoice(root,
              GallerySettings::CloseBehavior::Minimize,
              Typography::Icons::ChromeMinimize,
              QStringLiteral("Minimize window"),
              closebehaviorui::minimizeDescription());
    addChoice(root,
              GallerySettings::CloseBehavior::Tray,
              Typography::Icons::Hide,
              closebehaviorui::keepRunningChoice(),
              closebehaviorui::keepRunningDescription());
    addChoice(root,
              GallerySettings::CloseBehavior::Quit,
              Typography::Icons::Power,
              QStringLiteral("Quit the app"),
              QStringLiteral("Stop the Gallery completely."));

    root->activate();
    const int contentHeight = root->hasHeightForWidth()
        ? root->heightForWidth(kCloseBehaviorContentWidth)
        : root->sizeHint().height();
    setFixedHeight(contentHeight);

    syncSelection();
    onThemeUpdated();
}

GallerySettings::CloseBehavior CloseBehaviorPromptContent::selectedBehavior() const
{
    return m_selected;
}

void CloseBehaviorPromptContent::onThemeUpdated()
{
    for (auto* row : m_rows) {
        if (auto* themed = dynamic_cast<FluentElement*>(row))
            themed->onThemeUpdated();
    }
    update();
}

void CloseBehaviorPromptContent::addChoice(QVBoxLayout* root,
                                           GallerySettings::CloseBehavior behavior,
                                           const QString& glyph,
                                           const QString& title,
                                           const QString& description)
{
    auto* row = new CloseBehaviorChoiceRow(behavior, glyph, title, description, this);
    row->setOnActivate([this, behavior]() {
        m_selected = behavior;
        syncSelection();
    });
    m_rows.append(row);
    root->addWidget(row);
}

void CloseBehaviorPromptContent::syncSelection()
{
    for (auto* widget : m_rows) {
        auto* row = static_cast<CloseBehaviorChoiceRow*>(widget);
        row->setSelected(row->behavior() == m_selected);
    }
}

QSize closeBehaviorDialogSize()
{
    return kCloseBehaviorDialogSize;
}

} // namespace fluent::gallery
