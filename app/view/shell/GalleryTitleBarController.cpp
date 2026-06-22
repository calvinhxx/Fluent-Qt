#include "GalleryTitleBarController.h"

#include <utility>

#include <QAbstractAnimation>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPoint>
#include <QPointer>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QVariantAnimation>

#include "components/basicinput/Button.h"
#include "components/foundation/QMLPlus.h"
#include "components/status_info/ToolTip.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/Label.h"
#include "components/windowing/TitleBar.h"
#include "design/Animation.h"
#include "design/Typography.h"
#include "AppIcon.h"
#include "GallerySearchRanking.h"
#include "GalleryWindowMetrics.h"

namespace fluent::gallery {
namespace {

using Edge = fluent::AnchorLayout::Edge;
using TitleBarMetrics = metrics::TitleBar;

constexpr char kButtonPressAnimationName[] = "galleryTitleBarButtonPressAnimation";
constexpr qreal kButtonPressScale = 0.86;  // WinUI-like press depth for icon buttons. zh_CN: 仿 WinUI 的图标按钮按下缩放深度。

int titleBarLeadingOffset(const fluent::windowing::TitleBar* bar)
{
    if (!bar)
        return TitleBarMetrics::HorizontalMargin;
    return TitleBarMetrics::leadingOffset(bar->systemReservedLeadingWidth());
}

// WinUI-style click feedback: the glyph quickly dips to ~0.86 scale and springs back, reading as
// a press without moving the button or disturbing the layout. zh_CN: 仿 WinUI 点击反馈：字形快速缩小再弹回，
// 呈现按下感，不移动按钮、不影响布局。
void startButtonPress(fluent::basicinput::Button* button)
{
    if (!button || !button->isEnabled())
        return;

    if (auto* current = button->findChild<QPropertyAnimation*>(QString::fromLatin1(kButtonPressAnimationName))) {
        current->stop();
        current->deleteLater();
    }

    button->setIconScale(1.0);
    auto* animation = new QPropertyAnimation(button, "iconScale", button);
    animation->setObjectName(QString::fromLatin1(kButtonPressAnimationName));
    const auto motion = button->themeAnimation();
    animation->setDuration(motion.fast);
    animation->setEasingCurve(motion.decelerate);
    animation->setStartValue(1.0);
    animation->setKeyValueAt(0.4, kButtonPressScale);
    animation->setEndValue(1.0);
    QObject::connect(animation, &QPropertyAnimation::finished, button,
                     [button]() { button->setIconScale(1.0); });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

} // namespace

GalleryTitleBarController::GalleryTitleBarController(fluent::windowing::TitleBar* bar,
                                                    const QStringList& searchTitles,
                                                    Callbacks callbacks,
                                                    QObject* parent)
    : QObject(parent)
    , m_bar(bar)
    , m_callbacks(std::move(callbacks))
{
    build(searchTitles);
}

void GalleryTitleBarController::build(const QStringList& searchTitles)
{
    auto* bar = m_bar;
    auto* layout = qobject_cast<fluent::AnchorLayout*>(bar->layout());
    if (!layout)
        return;

    bar->setTitleBarHeight(TitleBarMetrics::Height);

    m_backButton = new fluent::basicinput::Button(bar);
    auto* backButton = m_backButton;
    backButton->setObjectName(QStringLiteral("GalleryTitleBar.BackButton"));
    backButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    backButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    backButton->setFluentSize(fluent::basicinput::Button::Small);
    backButton->setFont(backButton->themeFont(Typography::FontRole::Caption).toQFont());
    backButton->setIconGlyph(Typography::Icons::TitleBarBack, TitleBarMetrics::ButtonIconSize);
    // Height is fixed; the width is driven by the reveal animation (0 when there is no history,
    // ButtonSize once back navigation is available). zh_CN: 高度固定，宽度由展开动画驱动。
    backButton->setFixedHeight(TitleBarMetrics::ButtonSize);
    backButton->setFocusPolicy(Qt::NoFocus);
    fluent::status_info::ToolTip::attach(backButton, QStringLiteral("Back"));
    backButton->setEnabled(false);
    backButton->installEventFilter(this);
    // Start faded out; the reveal animation drives contentOpacity alongside the width collapse so
    // showing/hiding reads as a smooth slide-in rather than a hard pop. zh_CN: 初始淡出，展开动画同时驱动透明度与宽度。
    backButton->setContentOpacity(0.0);
    connect(backButton, &fluent::basicinput::Button::clicked, this, [this]() {
        if (m_callbacks.onBack)
            m_callbacks.onBack();
    });

    m_menuButton = new fluent::basicinput::Button(bar);
    auto* menuButton = m_menuButton;
    menuButton->setObjectName(QStringLiteral("GalleryTitleBar.MenuButton"));
    menuButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    menuButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    menuButton->setFluentSize(fluent::basicinput::Button::Small);
    menuButton->setIconGlyph(Typography::Icons::GlobalNav, TitleBarMetrics::ButtonIconSize);
    menuButton->setFixedSize(TitleBarMetrics::ButtonSize, TitleBarMetrics::ButtonSize);
    menuButton->setFocusPolicy(Qt::NoFocus);
    fluent::status_info::ToolTip::attach(menuButton,
                                         QStringLiteral("Toggle navigation pane"));
    menuButton->setEnabled(false);
    menuButton->installEventFilter(this);
    connect(menuButton, &fluent::basicinput::Button::clicked, this, [this]() {
        if (m_callbacks.onToggleNav)
            m_callbacks.onToggleNav();
    });

    auto* appIcon = new QLabel(bar);
    m_appIcon = appIcon;
    appIcon->setObjectName(QStringLiteral("GalleryTitleBar.AppIcon"));
    appIcon->setAlignment(Qt::AlignCenter);
    appIcon->setFixedSize(TitleBarMetrics::AppIconSize, TitleBarMetrics::AppIconSize);
    appIcon->setPixmap(appicon::pixmap(TitleBarMetrics::AppIconSize, bar->devicePixelRatioF()));

    auto* title = new fluent::textfields::Label(QStringLiteral("WinUI 3 Gallery"), bar);
    m_title = title;
    title->setObjectName(QStringLiteral("GalleryTitleBar.Title"));
    title->setFluentTypography(Typography::FontRole::Caption);
    title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    title->setFixedSize(TitleBarMetrics::TitleWidth, TitleBarMetrics::TitleHeight);

    fluent::AnchorLayout::Anchors backAnchors;
    backAnchors.left = {bar, Edge::Left, titleBarLeadingOffset(bar)};
    backAnchors.verticalCenter = {bar, Edge::VCenter, 0};
    layout->addAnchoredWidget(backButton, backAnchors);

    connect(bar, &fluent::windowing::TitleBar::systemReservedLeadingWidthChanged, backButton,
            [backButton, layout](int newWidth) {
                backButton->anchors()->left.offset = newWidth > 0
                    ? TitleBarMetrics::leadingOffset(newWidth)
                    : TitleBarMetrics::HorizontalMargin;
                layout->invalidate();
            });

    fluent::AnchorLayout::Anchors menuAnchors;
    menuAnchors.left = {backButton, Edge::Right, TitleBarMetrics::ItemGap};
    menuAnchors.verticalCenter = {bar, Edge::VCenter, 0};
    layout->addAnchoredWidget(menuButton, menuAnchors);

    fluent::AnchorLayout::Anchors appIconAnchors;
    appIconAnchors.left = {menuButton, Edge::Right, TitleBarMetrics::ItemGap};
    appIconAnchors.verticalCenter = {bar, Edge::VCenter, 0};
    layout->addAnchoredWidget(appIcon, appIconAnchors);

    fluent::AnchorLayout::Anchors titleAnchors;
    titleAnchors.left = {appIcon, Edge::Right, TitleBarMetrics::ItemGap};
    titleAnchors.verticalCenter = {bar, Edge::VCenter, 0};
    layout->addAnchoredWidget(title, titleAnchors);

    auto* searchBox = new fluent::textfields::AutoSuggestBox(bar);
    m_searchBox = searchBox;
    searchBox->setObjectName(QStringLiteral("GalleryTitleBar.SearchBox"));
    searchBox->setPlaceholderText(QStringLiteral("Search controls and samples..."));
    searchBox->setSuggestions(searchTitles);
    searchBox->setInputHeight(TitleBarMetrics::SearchHeight);
    searchBox->setQueryButtonSize(TitleBarMetrics::ButtonSize);
    searchBox->setClearButtonSize(TitleBarMetrics::ButtonSize);
    // Height is fixed; width is driven by updateLayout() so the box can shrink to fit the free
    // span between the leading group and the native caption controls. zh_CN: 高度固定，宽度由 updateLayout() 驱动。
    searchBox->setFixedHeight(TitleBarMetrics::SearchHeight);
    // AutoSuggestBox leaves filtering to the owner (WinUI semantics): token-AND, prefix-first.
    // zh_CN: AutoSuggestBox 把过滤交给使用方：词元 AND 匹配、前缀优先。
    connect(searchBox, &fluent::textfields::AutoSuggestBox::textChangedWithReason, searchBox,
            [searchBox, searchTitles](const QString& text,
                                      fluent::textfields::AutoSuggestBox::TextChangeReason reason) {
                if (reason != fluent::textfields::AutoSuggestBox::TextChangeReason::UserInput)
                    return;
                searchBox->setSuggestions(search::rankedTitles(searchTitles, text));
            });
    connect(searchBox, &fluent::textfields::AutoSuggestBox::querySubmitted, this,
            [this](const QString& queryText, const QVariant& chosenSuggestion) {
                const QString chosen = chosenSuggestion.toString();
                if (m_callbacks.onSearch)
                    m_callbacks.onSearch(chosen.isEmpty() ? queryText : chosen);
            });
    connect(searchBox, &fluent::textfields::AutoSuggestBox::suggestionChosen, this,
            [this](const QVariant& item) {
                if (m_callbacks.onSearch)
                    m_callbacks.onSearch(item.toString());
            });

    // The search box is intentionally left out of the AnchorLayout: it needs a max width,
    // centering-with-clamp, and collapse behaviour the anchor model can't express, so updateLayout()
    // positions it manually against the live bar width. zh_CN: 搜索框刻意不入 AnchorLayout，由 updateLayout() 手动定位。
    bar->installEventFilter(this);
    // Start fully collapsed: no history at launch. setBackAvailable() animates it open later.
    // zh_CN: 启动完全收起；之后由 setBackAvailable() 动画展开。
    applyBackButtonReveal(0.0);
    updateLayout();
}

bool GalleryTitleBarController::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_backButton || watched == m_menuButton) {
        auto* button = qobject_cast<fluent::basicinput::Button*>(watched);
        switch (event->type()) {
        case QEvent::ToolTip:
            // Replace Qt's native tooltip with the Fluent ToolTip (reusing Qt's hover delay);
            // returning true suppresses the platform bubble. zh_CN: 用 Fluent ToolTip 替换原生提示，返回 true 抑制平台气泡。
            showToolTip(button);
            return true;
        case QEvent::Leave:
        case QEvent::Hide:
            hideToolTip();
            break;
        case QEvent::MouseButtonPress:
            startButtonPress(button);
            hideToolTip();
            break;
        default:
            break;
        }
    } else if (watched == m_bar && event->type() == QEvent::Resize) {
        hideToolTip();  // a width change can orphan a bubble that got no Leave event
        updateLayout();
    }

    return QObject::eventFilter(watched, event);
}

void GalleryTitleBarController::updateLayout()
{
    auto* bar = m_bar;
    if (!bar || !m_searchBox)
        return;

    const bool minimalNav = m_callbacks.isMinimalNavLayout && m_callbacks.isMinimalNavLayout();

    // In the minimal (hidden-pane) layout the app title+icon give way to the search box; otherwise
    // they show. Nothing shows while the splash owns the title bar. zh_CN: 最小布局下标题+图标让位给搜索框。
    const bool showAppIcon = m_chromeVisible;
    const bool showTitle = m_chromeVisible && !minimalNav;
    if (m_title)
        m_title->setVisible(showTitle);
    if (m_appIcon)
        m_appIcon->setVisible(showAppIcon);

    // Settle the left-anchored chain (back/menu/icon/title) before publishing hit-test rects.
    // zh_CN: 发布命中测试区域前，先让左锚链落到最终几何。
    if (auto* barLayout = bar->layout())
        barLayout->activate();

    const int leftBound = TitleBarMetrics::searchLeftBound(bar->systemReservedLeadingWidth(),
                                                           showAppIcon, showTitle, m_backReveal);
    const int rightBound = TitleBarMetrics::searchRightBound(bar->width(),
                                                             bar->systemReservedTrailingWidth());
    const int avail = TitleBarMetrics::searchAvailableWidth(leftBound, rightBound);

    const bool showSearch = m_chromeVisible && TitleBarMetrics::canShowSearch(avail);
    m_searchBox->setVisible(showSearch);
    if (showSearch) {
        const int searchW = TitleBarMetrics::searchWidth(avail);
        const int x = TitleBarMetrics::searchX(bar->width(), searchW, leftBound, rightBound);
        const int y = (bar->height() - TitleBarMetrics::SearchHeight) / 2;
        m_searchBox->setGeometry(x, y, searchW, TitleBarMetrics::SearchHeight);
    }

    // The search box lives outside the AnchorLayout and title/icon visibility just changed —
    // republish the native hit-test regions so every control stays click-through.
    // zh_CN: 搜索框在 AnchorLayout 之外、可见性刚变化，重新发布命中测试区域，保证可点击。
    bar->refreshChromeExclusions();
}

void GalleryTitleBarController::setChromeVisible(bool visible, bool animated)
{
    // The custom title-bar widgets all share the "GalleryTitleBar." object-name prefix; the native
    // min/max/close buttons do not, so toggling by prefix leaves them alone. zh_CN: 自定义控件均以
    // "GalleryTitleBar." 前缀命名，按前缀切换不影响原生窗口按钮。
    if (!m_bar)
        return;

    m_chromeVisible = visible;
    const QList<QWidget*> chrome = m_bar->findChildren<QWidget*>();
    for (QWidget* widget : chrome) {
        if (!widget->objectName().startsWith(QStringLiteral("GalleryTitleBar.")))
            continue;

        if (!visible) {
            widget->setGraphicsEffect(nullptr);
            widget->setVisible(false);
            continue;
        }

        widget->setVisible(true);
        if (!animated) {
            widget->setGraphicsEffect(nullptr);
            continue;
        }

        // Subtle fade-in, timed to the splash's dissolve. The effect is dropped once the fade
        // completes so it adds no steady-state paint cost. zh_CN: 简约淡入，与 splash 同步；结束即移除效果。
        auto* effect = new QGraphicsOpacityEffect(widget);
        widget->setGraphicsEffect(effect);
        auto* fade = new QPropertyAnimation(effect, "opacity", widget);
        fade->setStartValue(0.0);
        fade->setEndValue(1.0);
        fade->setDuration(::Animation::Duration::Normal);
        fade->setEasingCurve(::Animation::getEasing(::Animation::EasingType::Decelerate));
        QPointer<QWidget> guard = widget;
        connect(fade, &QPropertyAnimation::finished, widget, [guard]() {
            if (guard)
                guard->setGraphicsEffect(nullptr);
        });
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    }

    // setVisible(true) above un-hides chrome uniformly; re-apply the adaptive rules so the
    // title/icon and search box land in their width-appropriate state. zh_CN: 重新套用自适应规则。
    updateLayout();
}

void GalleryTitleBarController::setMenuEnabled(bool enabled)
{
    if (m_menuButton)
        m_menuButton->setEnabled(enabled);
}

QWidget* GalleryTitleBarController::searchBox() const
{
    return m_searchBox;
}

void GalleryTitleBarController::applyBackButtonReveal(qreal reveal)
{
    m_backReveal = reveal;
    if (m_backButton) {
        m_backButton->setFixedWidth(qRound(reveal * TitleBarMetrics::ButtonSize));
        m_backButton->setContentOpacity(reveal);
    }
    if (m_menuButton && m_menuButton->anchors())
        m_menuButton->anchors()->left.offset = qRound(reveal * TitleBarMetrics::ItemGap);
    if (auto* barLayout = m_bar ? m_bar->layout() : nullptr)
        barLayout->invalidate();
    updateLayout();
}

void GalleryTitleBarController::setBackAvailable(bool available)
{
    if (!m_backButton || m_backRevealed == available)
        return;
    m_backRevealed = available;
    m_backButton->setEnabled(available);

    const auto motion = m_backButton->themeAnimation();
    if (!m_backRevealAnimation) {
        m_backRevealAnimation = new QVariantAnimation(this);
        m_backRevealAnimation->setDuration(motion.normal);
        connect(m_backRevealAnimation, &QVariantAnimation::valueChanged, this,
                [this](const QVariant& value) { applyBackButtonReveal(value.toReal()); });
    }
    m_backRevealAnimation->stop();
    // Ease out on the way in (decisive arrival), the standard curve on the way out.
    // zh_CN: 入场用缓出（落点干脆），退场用标准曲线。
    m_backRevealAnimation->setEasingCurve(available ? motion.decelerate : motion.standard);
    m_backRevealAnimation->setStartValue(m_backReveal);
    m_backRevealAnimation->setEndValue(available ? 1.0 : 0.0);
    m_backRevealAnimation->start();
}

void GalleryTitleBarController::showToolTip(fluent::basicinput::Button* button)
{
    if (!button)
        return;
    const QString text = button->toolTip();
    if (text.isEmpty())
        return;

    if (!m_toolTip) {
        m_toolTip = new fluent::status_info::ToolTip(nullptr);
        m_toolTip->setAnimationEnabled(true);
    }
    m_toolTip->setText(text);  // adjustSize() runs inside

    // Center the bubble under the button. The widget carries a transparent shadow band, so offset
    // by shadowMargin to land the visible bubble (not the band) at the gap below. zh_CN: 气泡居中于按钮下方。
    const int shadow = m_toolTip->shadowMargin();
    const QPoint anchor = button->mapToGlobal(QPoint(button->width() / 2, button->height()));
    const int x = anchor.x() - m_toolTip->width() / 2;
    const int y = anchor.y() + TitleBarMetrics::ToolTipGap - shadow;
    m_toolTip->move(x, y);
    m_toolTip->show();
    m_toolTip->raise();
}

void GalleryTitleBarController::hideToolTip()
{
    if (m_toolTip)
        m_toolTip->hide();
}

} // namespace fluent::gallery
